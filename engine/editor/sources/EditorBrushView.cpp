/*
Created: 17:08:2026 - 20:06:53
Last updated: 18:08:2026 - 13:08:07
Module: engine/editor
File: engine/editor/sources/EditorBrushView.cpp

Responsibility:
- The ground swatches declared in EditorBrushView.h — the four surfaces, baked
  once each and kept.

Dependencies:
- Uses: EditorBrushView.h, EditorBrush.h, EditorUi.h, Dear ImGui.
- Used by: engine/app (App).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- СВОТЧ НЕ ВТОРОЙ НАБОР ЦВЕТОВ ЗЕМЛИ. Он собирается из тех же ячеек
  проц-текстур и по тем же весам splat_weights_of, что и фрагмент, — иначе это
  была бы картинка, обещающая не ту землю.
*/
/*
UPD:
- 17:08:2026 - 20:06:53: Создан — панель кисти рельефа и посадки.
- 18:08:2026 - 01:24:18: свотчи поверхностей: печь один раз, рисовать 24 px с уменьшением.
- 18:08:2026 - 01:29:51: три попытки выпечки свотча и голос при сдаче (см. заголовок; правка от
  хрупкости, не по воспроизведённому отказу).
- 18:08:2026 - 13:08:07: ПАНЕЛЬ УДАЛЕНА (её никто не звал — ноль ссылок в дереве), остались
  свотчи. Списки режимов и поверхностей отсюда ушли ещё 18.08 12:06 — они живут
  в EditorToolsGround.cpp, у самих инструментов; здесь они дублировались молча.
*/

#include "engine/editor/sources/EditorBrushView.h"

#include "engine/editor/sources/EditorPaletteThumb.h"

#include <cstdio>
#include <vector>

namespace dfn::app {
namespace {

/// How big a swatch is baked and drawn. 32 texels is the smallest square in
/// which the 4x4 Bayer dither of the blend band still reads as two materials
/// rather than as noise — the band is exactly what a builder cannot picture
/// from the word «смесь», so a swatch too small to show it would be showing
/// him the one thing he already knew.
constexpr int SWATCH_PX = 32;

} // namespace

BrushSwatches::~BrushSwatches() {
    if (ui_ == nullptr) {
        return;
    }
    for (const auto& [surface, slot] : tex_) {
        (void)surface;
        if (slot.texture != 0) {
            ui_->drop_texture(slot.texture);
        }
    }
}

EditorTexture BrushSwatches::surface(math::SurfaceClass surface) {
    // BAKED ONCE PER CLASS AND KEPT. The panel asks every frame for every row;
    // baking here would be four proc-texture composes per frame for a picture
    // that cannot change. A FAILED bake is cached as 0 too, deliberately —
    // retrying a bake that already failed, sixty times a second, turns one
    // missing picture into a stutter.
    Slot& slot = tex_[surface];
    if (slot.texture != 0 || slot.given_up) {
        return slot.texture;
    }
    // ТРИ ПОПЫТКИ, А НЕ ОДНА И НЕ БЕСКОНЕЧНО. Одна попытка — то, что было, и
    // это стоило картинок целиком: неудача запоминалась как готовый ноль, так
    // что панель до конца сессии рисовала одни названия, а код выглядел
    // рабочим. Бесконечные попытки — другая крайность: одна недоступная
    // картинка превратилась бы в композицию текстуры шестьдесят раз в секунду.
    constexpr int MAX_ATTEMPTS = 3;
    ++slot.attempts;
    std::vector<std::uint8_t> rgba;
    if (ui_ != nullptr && bake_surface_swatch(surface, SWATCH_PX, rgba)) {
        slot.texture = ui_->make_texture(static_cast<std::uint32_t>(SWATCH_PX),
                                         static_cast<std::uint32_t>(SWATCH_PX), rgba.data());
    }
    if (slot.texture == 0 && slot.attempts >= MAX_ATTEMPTS) {
        slot.given_up = true;
        // СДАЁМСЯ ВСЛУХ. Молчащий отказ здесь неотличим от «так и задумано»:
        // на экране в обоих случаях просто название без картинки.
        std::fprintf(stderr,
                     "[кисть] свотч поверхности %d не выпекся за %d попытки — "
                     "в панели останется одно название\n",
                     static_cast<int>(surface), MAX_ATTEMPTS);
    }
    return slot.texture;
}

} // namespace dfn::app
