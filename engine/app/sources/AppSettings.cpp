/*
Created: 18:08:2026 - 18:07:24
Last updated: 18:08:2026 - 18:07:24
Module: engine/app
File: engine/app/sources/AppSettings.cpp

Responsibility:
- Разбор и печать настроек графики. Устройство — в заголовке.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
*/
/*
UPD:
- 18:08:2026 - 18:07:24: Вынесено из App.cpp. Разбор отделён от файла: пока он сидел внутри
  App.cpp вперемешку с открытием файла, проверить его было нечем.
*/

#include "engine/app/sources/AppSettings.h"

#include "engine/app/sources/App.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace dfn::app {
namespace {
constexpr const char* SETTINGS_PATH = "settings.cfg";
} // namespace

std::string settings_to_text(const AppConfig& cfg) {
    std::ostringstream out;
    out << "# Daggerfall N graphics settings (auto-generated; edit freely)\n"
        << "# internal_resolution: rendering pixel grid, integer-upscaled to the\n"
        << "#   window. Default 1920x1080 (full detail). Retro presets:\n"
        << "#   640x360 (fine retro), 320x180 (chunky Daggerfall).\n"
        << "internal_resolution=" << cfg.internal_width << 'x' << cfg.internal_height
        << "\n"
        << "# msaa: coverage samples on the internal grid (0 = off, 2, 4, 8).\n"
        << "#   This is what stopped the treeline shimmering when you run\n"
        << "#   (0.094% -> 0.004% of the screen flipping per frame); lowering\n"
        << "#   it brings that back. It does NOT change the pixel grid.\n"
        << "msaa=" << cfg.msaa_samples << "\n"
        << "# fullscreen: 1 = the window opens on the whole screen. F11 toggles\n"
        << "#   it at any time and writes the answer back here.\n"
        << "fullscreen=" << (cfg.fullscreen ? 1 : 0) << "\n"
        << "# palette: 1 = 64-color quantization + dithering (DOS look), 0 = off.\n"
        << "palette=" << (cfg.palette_post ? 1 : 0) << "\n"
        << "# head_bob: bob/dip/settle motion scale; 0 disables the motion\n"
        << "# entirely (footstep sound and animation still fire).\n"
        << "head_bob=" << cfg.head_bob << "\n"
        << "# min_brightness: the darkest the picture ever gets, in the\n"
        << "#   palette's own shade steps (0.0784 = one step). 0 = honest\n"
        << "#   black: in an unlit cave you see NOTHING, which is correct\n"
        << "#   and unplayable on most monitors. The start menu has a\n"
        << "#   calibration page that sets this by eye.\n"
        << "min_brightness=" << cfg.black_floor << "\n"
        << "# show_menu: 1 = start in the menu and pick a demo map,\n"
        << "#            0 = drop straight into the world.\n"
        << "show_menu=" << (cfg.show_menu ? 1 : 0) << '\n';
    return out.str();
}

void settings_from_text(const std::string& text, AppConfig& cfg) {
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);
        if (key == "internal_resolution") {
            unsigned w = 0;
            unsigned h = 0;
            if (std::sscanf(value.c_str(), "%ux%u", &w, &h) == 2 && w > 0 && h > 0) {
                cfg.internal_width = w;
                cfg.internal_height = h;
            }
        } else if (key == "msaa") {
            const unsigned v =
                static_cast<unsigned>(std::strtoul(value.c_str(), nullptr, 10));
            if (v == 0 || v == 1 || v == 2 || v == 4 || v == 8) {
                cfg.msaa_samples = v;
            } else {
                // ГРОМКО, И НИКОГДА К БЛИЖАЙШЕМУ. Молча подогнанное число
                // выглядит как работающая настройка и рисует другой мир.
                std::fprintf(stderr,
                             "[settings] msaa=%s REJECTED (want 0, 2, 4 or 8); "
                             "keeping %u\n",
                             value.c_str(), cfg.msaa_samples);
            }
        } else if (key == "fullscreen") {
            cfg.fullscreen = !value.empty() && value[0] == '1';
        } else if (key == "palette") {
            cfg.palette_post = !value.empty() && value[0] == '1';
        } else if (key == "show_menu") {
            cfg.show_menu = !value.empty() && value[0] == '1';
        } else if (key == "head_bob") {
            float v = 1.0f;
            if (std::sscanf(value.c_str(), "%f", &v) == 1 && v >= 0.0f && v <= 2.0f) {
                cfg.head_bob = v;
            }
        } else if (key == "min_brightness") {
            float v = 0.0f;
            if (std::sscanf(value.c_str(), "%f", &v) == 1 && v >= 0.0f && v <= 0.25f) {
                cfg.black_floor = v;
            }
        }
    }
}

void write_settings(const AppConfig& cfg) {
    std::ofstream out(SETTINGS_PATH);
    out << settings_to_text(cfg);
}

void load_or_create_settings(AppConfig& cfg) {
    std::ifstream in(SETTINGS_PATH);
    if (!in.is_open()) {
        write_settings(cfg);
        return;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    settings_from_text(buf.str(), cfg);
}

} // namespace dfn::app
