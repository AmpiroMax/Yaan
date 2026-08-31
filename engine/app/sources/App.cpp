/*
Module: engine/app
File: engine/app/sources/App.cpp

Responsibility:
- Composition root implementation: subsystem wiring, the fixed-step/interpolated
  main loop, and the chunk-event ferry (world -> render meshes + physics bodies).

Key items:
- App::init/run/shutdown; AppConfig::from_env (DFN_INTERNAL_RES, DFN_NULL_*).
- Chunk ferry: on ChunkLoaded converts uint16 heightfield to the float buffer
  TerrainDesc expects (kept alive per chunk until ChunkUnloaded).

Dependencies:
- Uses: platform factories (glfw/bgfx/jolt + null), core, world, render, gameplay.
- Used by: main.cpp.

Notes:
- Sim-zone seam: PlayerMovementSystem API per sim's stage-2 report; the three
  call sites are marked SIM-SEAM and adjusted at integration.
- Tour finished (post_frame == true) requests window close (render's contract).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/

#include "engine/app/sources/App.h"

#include "engine/app/sources/AppInternal.h"
#include "engine/app/sources/AppSettings.h"

#include "engine/app/sources/AppDoors.h"
#include "engine/app/sources/AppStand.h"
#include "engine/app/sources/AppHud.h"

#include "engine/app/sources/AssetBake.h"

#include "engine/app/sources/Controls.h"
#include "engine/app/sources/EditorHud.h"
#include "engine/app/sources/HudScreen.h"
#include "engine/app/sources/IntroVideo.h"
#include "engine/app/sources/Localization.h"
// The object menu's pictures. Included HERE and not in App.h on purpose: only
// wire_editor_panels() names it, and App.h is already the widest header in the
// tree.
#include "engine/editor/sources/EditorPaletteThumb.h"
#include "engine/editor/sources/EditorToolPath.h"
// Generated at BUILD time by tools/stamp_build_commit.cmake; carries
// DFN_BUILD_COMMIT into every state capture. See that script for why the
// configure-time version was a defect rather than a simplification.
#include "BuildInfo.h"

#include "engine/core/components/sources/Components.h"
#include "engine/world/sources/CoarseTerrain.h"
#include "engine/world/sources/HouseMesh.h"
#include "engine/world/sources/WorldgenForest.h"
#include "engine/world/sources/LayoutLoad.h"
#include "engine/world/sources/Scene.h"
#include "engine/world/sources/Worldgen.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/physics/sources/CollisionLayers.h"
#include "engine/physics/sources/TerrainCollision.h"
#include "engine/core/serialization/sources/ContentHash.h"
#include "engine/anim/sources/Body.h"
#include "engine/anim/sources/BodyMesh.h"
#include "engine/gameplay/sources/HeldItem.h"
#include "engine/gameplay/sources/InteractableSpawn.h"
#include "engine/gameplay/sources/InteractionSystem.h"
#include "engine/gameplay/sources/InventoryScreen.h"
#include "engine/gameplay/sources/Item.h"
#include "engine/gameplay/sources/PlayerActions.h"
#include "engine/gameplay/sources/PlayerMovement.h" // sim's confirmed stage-2 API
#include "engine/gameplay/sources/PropCollision.h"
#include "engine/gameplay/sources/ViewModel.h"
#include "engine/render/sources/BitmapFont.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/SkyModel.h"
#include "engine/render/sources/TerrainLod.h"
#include "engine/gameplay/sources/StepEvents.h"
#include "engine/gameplay/sources/StepFeel.h"
#include "engine/platform/audio/sources/miniaudio/CreateMiniaudioAudio.h"
#include "engine/platform/audio/sources/null/CreateNullAudio.h"
#include "engine/platform/input/interfaces/IInput.h"
#include "engine/platform/input/sources/glfw/CreateGlfwInput.h"
#include "engine/platform/physics/interfaces/IPhysics.h"
#include "engine/platform/physics/sources/jolt/CreateJoltPhysics.h"
#include "engine/platform/physics/sources/null/CreateNullPhysics.h"
#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/platform/render/sources/bgfx/CreateBgfxRenderer.h"
#include "engine/platform/render/sources/null/CreateNullRenderer.h"
#include "engine/platform/window/interfaces/IWindow.h"
#include "engine/platform/window/sources/glfw/CreateGlfwWindow.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <limits>
#include <string_view>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace dfn::app {

namespace {

// ЧТО ОТКРЕПЛЕНО ОБРАТНО НА СТЕННЫЕ ЧАСЫ (дверь DFN_UNPIN — контрольные руки
// детерминизма). `what` — одно из "clock", "steps", "fade", "gate"; "all"
// открепляет всё, то есть возвращает прогон в то состояние, в каком он был до
// волны детерминизма тура.
//
// ЧИТАЕТСЯ ЗДЕСЬ, А НЕ РЯДОМ С ТАБЛИЦЕЙ, и это не вкус: рукав app_doors
// требует, чтобы у каждой описанной двери был читатель, и файл САМОЙ ТАБЛИЦЫ
// из переписи читателей исключён — иначе строка засчитывалась бы читателем
// сама себе, и правило «нет двери, которую никто не читает» ничего бы не
// значило. Все пять точек, которые эту дверь спрашивают, живут в этом файле.
//
// Латчится один раз: значение не может смениться посреди прогона, а getenv на
// каждом кадре в пяти местах кадрового цикла — это пять чтений ради строки,
// которая не меняется.
[[nodiscard]] bool unpinned(std::string_view what) {
    static const std::string value = [] {
        const char* v = door_value("DFN_UNPIN");
        return std::string(v != nullptr ? v : "");
    }();
    if (value.empty()) {
        return false;
    }
    if (value == "all" || value == "1") {
        return true;
    }
    // Токены через запятую; сравнение по ЦЕЛОМУ токену, а не по вхождению
    // подстроки: "fade" не имеет права зажечься от "no-fade".
    std::size_t at = 0;
    while (at <= value.size()) {
        const std::size_t end = value.find(',', at);
        const std::size_t stop = (end == std::string::npos) ? value.size() : end;
        if (std::string_view(value).substr(at, stop - at) == what) {
            return true;
        }
        if (end == std::string::npos) {
            break;
        }
        at = end + 1;
    }
    return false;
}

// СЕТКА ТАЙЛОВ, ПЕРЕПРАВА ЧАНКОВ И pack_coord УЕХАЛИ В AppInternal.h: после
// выноса enter_world в AppWorld.cpp они понадобились ДВУМ файлам, а
// безымянное пространство видно только одному.

// ЗАГЛАВНАЯ ТЕМА: ОДНА СТРОКА НА ВЕСЬ ДВИЖОК, И ОНА УКАЗЫВАЕТ НА ПЕТЛЮ.
// main_theme_loop.ogg — нарезка БЕЗ затухания, со стыком, подогнанным
// материалом: точка петли это конец файла, поэтому проигрывание не знает ни
// про кроссфейд, ни про метки — просто loop. Соседний main_theme.ogg длиннее и
// заканчивается затуханием: он сведён для ТИТРОВ, и на репите его хвост
// слышен как провал между проходами. Перепутать их — единственный способ
// сломать этот заказ, поэтому обе строки стоят рядом и подписаны.
constexpr const char* MENU_THEME_PATH =
    "games/daggerfall_n/assets/audio/music/main_theme_loop.ogg";
constexpr const char* MENU_THEME_FALLBACK_PATH =
    "games/daggerfall_n/assets/audio/music/main_theme.ogg";

// СКОЛЬКО ЗАТУХАЕТ ТЕМА НА ВХОДЕ В МИР. Секунда — не «примерно ноль» и не
// «слышно, как выключили»: обрыв в тишину читается как сбой звука, а два-три
// секундных ухода превращают каждое нажатие «Играть» в ожидание. Заказ
// владельца назвал ~1 с, и это же число меряется в приёмке.
constexpr float MENU_MUSIC_FADE_OUT_S = 1.0f;

// РОСЧЕРК ЗАСТАВКИ. Отдельный актив, а не первые секунды темы: он написан под
// КАДРЫ интро (гул на 0.30, УДАР на 1.65 — в тот момент, когда картинка выходит
// на полную яркость, хвост зала с 3.50), и его длина 5.6 с ДЛИННЕЕ видео на два
// с лишним секунды НАМЕРЕННО. Хвост обязан звучать, когда меню уже открылось и
// тема пошла: росчерк кончается пустой квинтой ре-ля, с которой тема
// начинается, и стоит на децибел тише её. Обрезать его по концу видео значило
// бы выбросить единственное, ради чего он такой длины.
constexpr const char* INTRO_STING_PATH =
    "games/daggerfall_n/assets/audio/ui/intro_sting.ogg";

// ПРОПУСК ЗАСТАВКИ ГАСИТ РОСЧЕРК ЗА ПЯТУЮ ДОЛЮ СЕКУНДЫ. Не мгновенно: обрыв
// оркестрового удара в тишину щёлкает, и щелчок в первую секунду игры читается
// как сломанный звук. И не по-музыкальному долго: игрок, нажавший «пропустить»,
// просил тишины СЕЙЧАС, а росчерк, доигрывающий секунду поверх меню, — это
// кнопка, которая не сработала.
constexpr float INTRO_STING_SKIP_FADE_S = 0.2f;

// КАК БЫСТРО МИР ЗАМОЛКАЕТ, КОГДА ПЕРЕСТАЁТ ИДТИ. Треть секунды: обрыв ветра
// за один кадр слышится как отвалившийся звук (жалоба на «пропал звук» и
// жалоба на «звук остался» — это одна и та же жалоба с разных сторон), а
// уход длиннее полусекунды превращает Esc в ожидание. Тем же временем мир
// и возвращается — пандус один на оба направления.
constexpr float WORLD_AUDIO_FADE_S = 0.35f;

} // namespace

// SETTINGS_PATH уехал в AppSettings.cpp вместе с чтением и записью: имя файла
// принадлежит тому, кто с файлом работает.

// ДВЕ ФУНКЦИИ НИЖЕ ВИДНЫ СОСЕДНЕМУ ФАЙЛУ.
// unattended_run() и write_settings() зовут обработчики клавиш, уехавшие в
// AppInput.cpp (слой 1 разбора). Внутренняя связка сделала бы вторую копию
// каждой — а «за этим никто не играет» и «настройки сохранены» обязаны
// значить ОДНО И ТО ЖЕ в обоих файлах (правило 32).

// unattended_run() УЕХАЛ В AppDoors.cpp (слой 2 разбора). Здесь он был
// выражением из тринадцати слагаемых «дверь X открыта ИЛИ дверь Y открыта», и
// его собственный комментарий признавался, что одну дверь в него заметали
// дважды. Теперь это КОЛОНКА ТАБЛИЦЫ, и она проверяется рукавом app_doors по
// каждой двери порознь: открыта одна — ответ обязан совпасть с её строкой.

// НАСТРОЙКИ УЕХАЛИ В AppSettings.{h,cpp} (заказ 18.08). Разбор отделён от
// файла: пока он сидел здесь, проверить его было нечем — этот файл владеет
// окном.

AppConfig AppConfig::from_env() {
    AppConfig cfg;
    cfg.internal_width = static_cast<uint32_t>(config::INTERNAL_RES_W);
    cfg.internal_height = static_cast<uint32_t>(config::INTERNAL_RES_H);
    load_or_create_settings(cfg); // file first; env below overrides (tooling)
    if (const char* res = door_value("DFN_INTERNAL_RES")) {
        unsigned w = 0, h = 0;
        if (std::sscanf(res, "%ux%u", &w, &h) == 2 && w > 0 && h > 0) {
            cfg.internal_width = w;
            cfg.internal_height = h;
        }
    }
    if (const char* nr = door_value("DFN_NULL_RENDER"); nr && nr[0] == '1') {
        cfg.use_null_renderer = true;
    }
    if (const char* mn = door_value("DFN_MENU")) {
        cfg.show_menu = (mn[0] == '1');
    }
    // DFN_STAND, not DFN_MAP: DFN_MAP was already render's MAP-SCREEN probe,
    // and Tour::stand_steps treats any probe variable as "this run is a single
    // evidence frame" -- so selecting the stand with it silently collapsed the
    // stand's own tour to one testbed frame. A name collision, found by the
    // frame it produced rather than by reading either file.
    if (const char* mp = door_value("DFN_STAND")) {
        const std::string m(mp);
        if (m == "forest") {
            cfg.start_stand = 1;
        } else if (m == "testbed" || m == "valley") {
            cfg.start_stand = 0;
        } else {
            cfg.start_stand = static_cast<uint32_t>(std::strtoul(mp, nullptr, 10));
        }
    }
    // TOOLING NEVER STOPS AT A MENU: nobody is there to press Enter, and a tour
    // that screenshots a menu is a tour that verified nothing.
    //
    // THE LIST MUST NAME EVERY AUTOMATED DOOR, and it did not. `DFN_TOUR` and
    // `DFN_PLAYTEST` were here from the start; `DFN_CAPTURE_AFTER`,
    // `DFN_BODY_PROBE` and `DFN_MENU_SHOT` were added later and each inherited
    // the trap -- they run unattended, so they sat on the start screen until the
    // timer fired and photographed the menu. Every agent shooting frames hit it
    // at once, which is the tell that this is a LIST that grows rather than a
    // property of the two names originally on it. The user reported it as
    // "they all hang in the menu".
    //
    // The rule for whoever adds the next door: if it runs without a human, it
    // belongs in this condition, and the condition is the place to look BEFORE
    // debugging why a frame is wrong.
    //
    // `DFN_FRAME_LOG` is deliberately NOT here, and the distinction is the rule
    // itself: it DRIVES nothing. It observes, so it is used both unattended
    // (alongside DFN_PLAYTEST, which is already on the list) and by a human
    // playing with the log running -- and that human wants his menu. Menu
    // frames simply log speed 0, which is the standing-still control anyway.
    if (unattended_run()) {
        cfg.show_menu = false;
    }
    // SILENCE IS THE DEFAULT, AND THIS IS A TEMPORARY INVERSION. The user asked
    // for it in as many words on 14.08.2026: "выключи звук в игре на время / и
    // пусть все кто запускает игру запускались без звука" -- everyone, humans
    // and agents alike, not just the automated runs.
    //
    // TO GIVE THE SOUND BACK, DELETE THIS INVERSION. Do not go looking for who
    // muted the engine: it was muted here, on purpose, on that date, and the
    // whole reason this paragraph is longer than the code is that a default
    // nobody remembers choosing costs somebody an hour a week from now.
    //
    // DFN_NULL_AUDIO stays and still means what it always meant. It is written
    // into recipes already on disk, and a door that quietly stops existing
    // makes every recipe naming it a lie.
    // ЗВУК СНОВА ВКЛЮЧЁН ПО УМОЛЧАНИЮ (28.08.2026). Переворот 14.08 был
    // ВРЕМЕННЫМ по просьбе владельца («выключи звук на время»); время вышло
    // его же заказом — заглавная тема в главном меню на репите. Дверь
    // DFN_AUDIO осталась и работает в обе стороны: =0 глушит, =1 включает
    // поверх любого умолчания; DFN_NULL_AUDIO по-прежнему значит то же, что
    // всегда (рецепты с её именем не становятся ложью).
    cfg.use_null_audio = false;
    if (const char* na = door_value("DFN_NULL_AUDIO"); na && na[0] == '1') {
        cfg.use_null_audio = true;
    }
    if (const char* a = door_value("DFN_AUDIO"); a && a[0] != 0) {
        cfg.use_null_audio = (a[0] == '0');
    }
    // SAID OUT LOUD, because an engine that is silent AND silent about being
    // silent is the mute zero this whole harness exists to refuse: the next
    // person to notice would file "the audio is broken" against something
    // nobody broke, and would be right to.
    if (cfg.use_null_audio) {
        std::fprintf(stderr,
                     "[audio] SILENT (DFN_NULL_AUDIO/DFN_AUDIO=0) -- "
                     "set DFN_AUDIO=1 for sound\n");
    }
    if (const char* np = door_value("DFN_NULL_PHYSICS"); np && np[0] == '1') {
        cfg.use_null_physics = true;
    }
    // The harness needs its own door: settings.cfg is shared by every zone and
    // a run must not edit it to take a frame.
    if (const char* bf = door_value("DFN_BLACK_FLOOR"); bf != nullptr && *bf != '\0') {
        float v = 0.0f;
        if (std::sscanf(bf, "%f", &v) == 1 && v >= 0.0f && v <= 0.25f) {
            cfg.black_floor = v;
        } else {
            std::fprintf(stderr, "[config] DFN_BLACK_FLOOR=\"%s\" REJECTED (want 0..0.25)\n", bf);
        }
    }
    if (const char* pal = door_value("DFN_PALETTE"); pal && pal[0] == '1') {
        cfg.palette_post = true;
    }
    // Same tooling pattern as DFN_PALETTE: the settings row is the user's, the
    // env var is the harness's. head_bob 0 is the ready-made MOTION control
    // (Rule 30) -- bob/dip/settle stop, events and sound keep firing -- so a
    // judder can be attributed to camera motion or exonerated of it in one run.
    if (const char* hb = door_value("DFN_HEAD_BOB"); hb != nullptr && *hb != '\0') {
        float v = 1.0f;
        if (std::sscanf(hb, "%f", &v) == 1 && v >= 0.0f && v <= 2.0f) {
            cfg.head_bob = v;
        } else {
            // LOUD, not silent. A rejected value here would leave bob at its
            // default while the harness believed the control was applied -- so
            // the counterfactual arm would be a duplicate of the other arm, and
            // "the judder survives bob at zero" would be concluded from a run
            // where bob was never zero. A control that can silently fail to
            // apply is worse than no control (Rule 30).
            std::fprintf(stderr,
                         "[config] DFN_HEAD_BOB=\"%s\" REJECTED (want 0..2); "
                         "head_bob stays %.2f -- the motion control was NOT "
                         "applied\n",
                         hb, static_cast<double>(cfg.head_bob));
        }
    }
    return cfg;
}

App::App()
    // Declaration order: telemetry_ is declared before timestep_ in App.h, so it
    // is initialised first (keeps -Wreorder-ctor quiet).
    : telemetry_(static_cast<size_t>(config::TELEMETRY_RING_SAMPLES)),
      timestep_(config::SIM_DT, static_cast<uint32_t>(config::SIM_MAX_CATCHUP_STEPS)) {
    if (const char* v = door_value("DFN_CAM_TRACE"); v != nullptr && *v != '0') {
        cam_trace_ = true;
    }
}

App::~App() = default;

// THE PREPARATION SCREEN. Deliberately the plainest thing in the engine: one
// line of what is happening, one bar, one count. A first launch that shows a
// black window for a minute is indistinguishable from one that hung.
// ЭКРАН, КОТОРОГО НИКТО НЕ ВИДЕЛ. Полоса запекания рисовалась в hud() с
// 18.08 и НИ РАЗУ не показалась: render гейтит блит на hud_visible_, а его
// выставляет только run(), который во время запекания ещё не начался.
// Найдено при заведении экрана загрузки (И15) — та же ошибка на том же пути.
void App::draw_bake_progress(std::size_t done, std::size_t total,
                             const std::string& what) {
    if (renderer_ == nullptr || total == 0) {
        return;
    }
    render::PixelCanvas& c = render_system_.hud();
    if (c.width() == 0 || c.height() == 0) {
        return;
    }
    const int w = static_cast<int>(c.width());
    const int h = static_cast<int>(c.height());
    c.clear(render::Color{18, 20, 24});

    const int bar_w = w / 2;
    const int bar_h = 10;
    const int bar_x = (w - bar_w) / 2;
    const int bar_y = h / 2;
    char line[192];
    std::snprintf(line, sizeof(line), "Готовлю ресурсы: %s", what.c_str());
    render::draw_text(c, bar_x, bar_y - 24, line, render::Color{225, 225, 215}, true);
    std::snprintf(line, sizeof(line), "%zu из %zu", done, total);
    render::draw_text(c, bar_x, bar_y + bar_h + 8, line,
                      render::Color{170, 175, 185}, true);
    render::draw_text(c, bar_x, bar_y + bar_h + 22,
                      "Это делается один раз: объекты не хранятся в репозитории,"
                      " они пекутся из кода.",
                      render::Color{120, 126, 136}, true);

    c.fill_rect(bar_x - 1, bar_y - 1, bar_w + 2, bar_h + 2,
                render::Color{70, 74, 82});
    const int filled = static_cast<int>(static_cast<double>(bar_w)
                                        * static_cast<double>(done)
                                        / static_cast<double>(total));
    c.fill_rect(bar_x, bar_y, filled, bar_h, render::Color{150, 190, 120});

    // The window must keep answering the OS while this runs, or the desktop
    // marks the app as not responding halfway through its own progress bar.
    if (window_ != nullptr) {
        window_->poll_events();
    }
    render_system_.set_hud_visible(true); // без этого холст рисуется в никуда
    render_system_.render(world_, *renderer_, camera_, 0.0f);
}

bool App::init(const AppConfig& config) {
    // THE HOUR OF THE DAY, ON DEMAND (DFN_TIME_OF_DAY=0..1; 0.25 sunrise, 0.5
    // noon, 0.75 sunset, 0 midnight). Without it NIGHT COULD NOT BE
    // PHOTOGRAPHED at all: the clock starts at START_TIME_OF_DAY and only a
    // human pressing T could move it, so every lamp, every firefly and every
    // shadow-at-dusk claim was unverifiable by any automated run — the same
    // shape of hole that let "в третьем лице тела нет" live until a human
    // looked. Read once, and the value is REJECTED OUT LOUD rather than
    // clamped: a typo that silently becomes noon would send someone hunting a
    // light that was working.
    if (const char* tod = door_value("DFN_TIME_OF_DAY");
        tod != nullptr && *tod != '\0') {
        char* end = nullptr;
        const double v = std::strtod(tod, &end);
        if (end == tod || v < 0.0 || v > 1.0) {
            std::fprintf(stderr, "[app] DFN_TIME_OF_DAY=\"%s\" is not a fraction "
                                 "of a day in 0..1 -- REFUSED, clock untouched\n",
                         tod);
        } else {
            game_seconds_ = v * static_cast<double>(config::DAY_LENGTH_SECONDS);
            std::fprintf(stderr, "[app] DFN_TIME_OF_DAY=%.3f -> game clock at "
                                 "%.1f s\n", v, game_seconds_);
        }
    }
    config_ = config;

    window_ = platform::create_glfw_window();
    platform::WindowInitParams wp;
    wp.width = config.window_width;
    wp.height = config.window_height;
    wp.title = "Daggerfall N"; // bootstrap exception: replaced by loc lookup (sync #2 note)
    wp.fullscreen = config.fullscreen;
    if (!window_ || !window_->init(wp)) {
        return false;
    }
    input_ = platform::create_glfw_input(*window_);

    renderer_ = config.use_null_renderer ? platform::create_null_renderer()
                                         : platform::create_bgfx_renderer();
    platform::RendererInitParams rp;
    rp.native_window_handle = window_->native_handle();
    rp.framebuffer_width = window_->framebuffer_size().x;
    rp.framebuffer_height = window_->framebuffer_size().y;
    rp.internal_width = config.internal_width;
    rp.internal_height = config.internal_height;
    rp.palette_post = config.palette_post;
    // ДВЕРЬ VSYNC (DFN_VSYNC=0). Заведена, когда профиль кадра города упёрся
    // в монитор: медиана dt_ms по Вайтрану (434 постройки) и по пустой
    // demo-карте совпала до 0.0001 мс — обе равны 1/120 с, и любой замер
    // «до/после» по dt_ms возвращал ноль, что бы кадр ни стоил на самом
    // деле. С открытой дверью dt_ms снова мерит рендер. Игровой путь без
    // переменной — прежний vsync, до последнего бита.
    if (const char* vs = door_value("DFN_VSYNC"); vs != nullptr && *vs == '0') {
        rp.vsync = false;
        std::fprintf(stderr, "[render] DFN_VSYNC=0 — vsync выключен, dt_ms "
                             "меряет кадр, а не монитор\n");
    }
    if (!renderer_ || !renderer_->init(rp)) {
        return false;
    }

    // THE EDITOR'S INTERFACE (Dear ImGui, EditorUi.cpp). NOT an error if it
    // fails to come up: the editor then behaves exactly as it did before this
    // module existed, which is the right answer for a tool layer on a backend
    // that ships no shaders (Rule 3's spirit). The game's own menu, HUD and
    // controls screen are untouched by it — they stay on PixelCanvas and keep
    // going through the palette and the post chain.
    // THE EDITOR'S TEXT COMES FROM THE GAME'S TABLE, handed over as a function:
    // engine/editor sits BELOW engine/app in the DAG, so it cannot include the
    // localization header, and a panel that hard-codes Russian would break
    // Rule 5 anyway.
    EditorUi::set_text_source([](const char* key) -> std::string_view {
        return localized(serialization::fnv1a64(key));
    });
    // NOT ON THE NULL BACKEND, and this guard is Rule 3 itself rather than
    // caution. The ImGui bridge lives beside the bgfx backend and calls bgfx
    // directly; with the null renderer bgfx was never initialised, so bringing
    // the interface up would crash the ONE configuration that exists to run
    // without a graphics device — headless tests and DFN_NULL_RENDER=1. A
    // feature that dies under a null backend is a bug, so it simply does not
    // start there, and the editor behaves exactly as it did before this module.
    if (!config.use_null_renderer && !editor_ui_.init(*renderer_)) {
        std::fprintf(stderr, "[editor-ui] интерфейс редактора не поднялся — "
                             "редактор работает как раньше\n");
    }

    physics_ = config.use_null_physics ? platform::create_null_physics()
                                       : platform::create_jolt_physics();
    if (!physics_ || !physics_->init()) {
        return false;
    }

    // Audio: no device is a MODE, not an error (Rule 3) -- the game runs
    // silent-but-correct on the null backend.
    audio_ = config.use_null_audio ? platform::create_null_audio()
                                   : platform::create_miniaudio_audio();
    if (!audio_ || !audio_->init()) {
        audio_ = platform::create_null_audio();
        (void)audio_->init();
    }
    // ТРИ ШИНЫ ОТ МАСТЕРА, И ЭТО ВСЯ АРХИТЕКТУРА МИКШЕРА НА СЕГОДНЯ. Эффекты
    // (шаги, прыжки, всплески, ветер), музыка, речь — три ветки, три ручки.
    //
    // РЕЧЕВАЯ ШИНА ЗАВЕДЕНА ВПЕРЁД ГОЛОСОВ, ПО ЗАКАЗУ ВЛАДЕЛЬЦА, и это прямо
    // отменяет довод, который стоял здесь час назад («шина без единого звука —
    // это ползунок, который ничего не делает»). Довод был не пустой, и ответ на
    // него — доктрина корневого экрана (26.08): спрятанный пункт неотличим от
    // несуществующего. Ползунок настоящий — он крутит настоящую шину и
    // переживает перезапуск; на шине пока просто тихо, и первая же реплика
    // попадёт в громкость, которую игрок выбрал заранее, а не в умолчание.
    // Цена нулевая: ma_sound_group без источников не считает ничего.
    //
    // Шина ОКРУЖЕНИЯ из контракта IAudio по-прежнему не заводится: её никто не
    // просил, и в отличие от речи у неё нет ни ползунка, ни обещания.
    sfx_bus_ = audio_->create_bus({});
    music_bus_ = audio_->create_bus({});
    voice_bus_ = audio_->create_bus({});
    audio_->set_bus_volume(sfx_bus_, config.sfx_volume);
    audio_->set_bus_volume(music_bus_, config.music_volume);
    audio_->set_bus_volume(voice_bus_, config.voice_volume);
    // И ПОД ШИНОЙ ЭФФЕКТОВ — ШИНА МИРА, У КОТОРОЙ ЕСТЬ ХОЗЯИН (заказ владельца
    // 28.08, дословно: «зашёл на Вайтран, поиграл, вышел в главное меню —
    // продолжили играть шумы фоновые/ветер/вода… звук должен быть привязан к
    // чему-то конкретному»).
    //
    // ЧТО БЫЛО НЕ ТАК, И ЭТО НЕ ЗАБЫТЫЙ ВЫЗОВ. Ветер заводился ОДИН РАЗ при
    // старте приложения и жил до конца прогона — то есть его хозяином было
    // ПРИЛОЖЕНИЕ, а звучал он про МИР. Выход в меню мир не выгружает (так
    // задумано: «Продолжить» обязано возвращать туда же), поэтому не было ни
    // одного события, на котором ветру полагалось бы замолчать. Чинить это
    // строкой «а ещё выключить ветер вот здесь» значило бы завести седьмое
    // место, где кто-то однажды забудет седьмой звук (правило 32).
    //
    // ПРАВИЛО ВМЕСТО СЛУЧАЯ: у каждого излучателя есть ХОЗЯИН, и звук замолкает
    // вместе с ним. Хозяев двое — МИР и МЕНЮ, — и у каждого своя шина. Всё, что
    // издаёт мир, играет на world_bus_, и приглушение мира это ОДНО число на
    // ней. Новый звук наследует правило тем, что выбирает шину: другого способа
    // его завести нет. Свод — в engine/platform/audio/docs/README.md.
    world_bus_ = audio_->create_bus(sfx_bus_);
    sound_bank_ = gameplay::load_step_sound_bank(
        *audio_, "games/daggerfall_n/assets/audio", world_bus_);
    gameplay::wire_step_audio(bus_, *audio_, sound_bank_);
    // ЗВУК МИРА ОТ ИСТОЧНИКА. Здесь стояла одна строка start_wind_loop — и
    // она была ДЕФЕКТОМ, названным владельцем дословно 28.08: «не должно быть
    // просто так фонового шума — а он даже в домах есть; у звука всегда должен
    // быть источник». Ветер игрался ОДНИМ непространственным голосом: одинаково
    // громким в чистом поле, в роще и в запертой комнате, потому что у него не
    // было места в мире — только громкость.
    //
    // Заменяющий предмет ничего не играет сам: он ждёт КРОН И РУСЕЛ карты
    // (enter_world кладёт их set_sources). Мир без деревьев остаётся ТИХИМ, и
    // это не побочный эффект, а проверяемое утверждение приёмки.
    ambience_bank_ = gameplay::WorldAmbience::load_bank(
        *audio_, "games/daggerfall_n/assets/audio", world_bus_);
    ambience_.set_bank(ambience_bank_);
    ambience_log_ = [] {
        const char* e = door_value("DFN_AMBIENCE_LOG");
        return e != nullptr && *e != '\0' && *e != '0';
    }();
    // ЗАГЛАВНАЯ ТЕМА, ОДИН РАЗ ЗА ЗАПУСК. Отказ громкий и НЕ фатальный: игра
    // без музыки — это игра, а игра, не запустившаяся из-за отсутствующего
    // .ogg, — это поломка. Загрузка идёт и на пустом бэкенде: там load_sound
    // не трогает диск, а ветка «если звук настоящий» была бы вторым местом,
    // где решается, играет ли музыка (её решает update_menu_music, и только он).
    menu_theme_ = audio_->load_sound(MENU_THEME_PATH);
    if (!menu_theme_.valid()) {
        // ПАДЕНИЕ НА ВЕРСИЮ С ЗАТУХАНИЕМ — на случай, когда петлевой нарезки
        // ещё нет на диске. Она СВЕДЕНА ДЛЯ ТИТРОВ и на репите слышна как
        // провал в конце каждого прохода, поэтому это запасной ход, а не
        // равноправный: строка стоит здесь именно для того, чтобы жалоба
        // «музыка проваливается» находила эту причину за один grep.
        menu_theme_ = audio_->load_sound(MENU_THEME_FALLBACK_PATH);
        if (menu_theme_.valid()) {
            std::fprintf(stderr,
                         "[музыка] петлевой нарезки нет (%s) — играю %s, "
                         "у неё в конце ЗАТУХАНИЕ и на репите будет провал\n",
                         MENU_THEME_PATH, MENU_THEME_FALLBACK_PATH);
        } else {
            std::fprintf(stderr,
                         "[музыка] тема не загрузилась (%s): меню будет тихим\n",
                         MENU_THEME_PATH);
        }
    }
    // РОСЧЕРК ЗАСТАВКИ — там же и на тех же условиях: отказ громкий и не
    // фатальный, заставка без звука это заставка.
    intro_sting_ = audio_->load_sound(INTRO_STING_PATH);
    if (!intro_sting_.valid()) {
        std::fprintf(stderr,
                     "[музыка] росчерк заставки не загрузился (%s): интро будет "
                     "немым\n",
                     INTRO_STING_PATH);
    }

    if (!render_system_.init(*renderer_)) {
        return false;
    }
    // The map canvas rasterizes in internal-resolution pixels, so it must know
    // the settings.cfg-driven resolution to stay pixel-exact (render's note).
    render_system_.set_internal_resolution(config.internal_width, config.internal_height);
    // THE BLACK FLOOR REACHES THE FRAME AT STARTUP, not only when the
    // calibration page closes. Without this line min_brightness is a value the
    // game stores, re-reads and writes back and NEVER DRAWS -- measured on six
    // acceptance frames: day and tunnel moved 0.0002 and 0.025 of a shade step
    // between a floor of zero and a floor of one and a half steps, which is the
    // run's own noise and nothing else. The control-against-control noise floor
    // was six to twenty times larger than either arm.
    render_system_.environment().black_floor = config.black_floor;
    // And the dial opens where the player left it, for the same reason.
    menu_.set_black_floor(config.black_floor);

    // Страница настроек открывается на том, с чем игра ЗАПУЩЕНА, и это же
    // значение отвечает на вопрос «какая строка применится лишь после
    // перезапуска»: модель хранит вторую копию и сравнивает с ней.
    //
    // И ЭТА КОПИЯ ОДНА НА ВСЮ ИГРУ (заказ владельца 27.08: «оно в игре и меню
    // должно синхронизироваться»). Синхронизация здесь не механизм, а
    // ОТСУТСТВИЕ второго состояния: страница настроек — одна MenuModel, и с
    // корня, и с паузы открывается ОНА ЖЕ. set_settings зовётся ровно один раз,
    // при старте; всё, что игрок повернул, живёт дальше в этой модели, а
    // settings_return_ помнит только, куда возвращаться. Две копии — по одной
    // на точку входа — разошлись бы в первый же вечер, и симптом («в паузе
    // поменял, в меню старое») выглядел бы как несохранение.
    MenuSettings ms;
    ms.window_w = config.window_width;
    ms.window_h = config.window_height;
    ms.fullscreen = config.fullscreen;
    ms.internal_w = config.internal_width;
    ms.internal_h = config.internal_height;
    ms.msaa = config.msaa_samples;
    ms.palette = config.palette_post;
    ms.head_bob = config.head_bob;
    ms.music_volume = config.music_volume;
    ms.sfx_volume = config.sfx_volume;
    ms.voice_volume = config.voice_volume;
    menu_.set_settings(ms);

    // Rule 5: every user-facing string comes from here and nowhere else.
    // A missing file is loud and the game still runs, with every string drawn
    // as a visible placeholder rather than as nothing.
    (void)load_localization("games/daggerfall_n/assets/localization/ru.txt");

    // THE MAP BROWSER'S CATALOG. Scanned from disk (assets/maps/<category>/
    // <map>.map) instead of a code table: adding a map is a data file, not a
    // recompile (Rule 6), and the browser two-levels it category -> map
    // (docs/MAP_LAYOUT.md). The menu only reads the catalog; the app owns it.
    // FIRST RUN: MAKE WHAT THE REPOSITORY DELIBERATELY DOES NOT CARRY.
    //
    // The registry used to be 210 MB of .dfo committed to git, every byte of it
    // a deterministic output of a forge that lives in this same repository.
    // The user's decision (17.08): «в гит результаты работы кода не сохраняем /
    // я же если друзьям буду код давать запуска, они не будут со всеми ассетами
    // его получать... а шейдеры компилить и ассеты запекать они при первом
    // запуске будут / и должны при загрузке игры видеть соответствующее
    // сообщение и полоску загрузки что m из n... готовы».
    //
    // So a clone carries code and compositions; the objects are made on
    // arrival, once, with a bar — because a game that sits black for a minute
    // on first launch is a game its first player thinks is broken.
    if (const BakePlan plan = plan_asset_bake(); !plan.empty()) {
        std::fprintf(stderr, "[bake] первый запуск: готовлю %zu объект(ов)\n",
                     plan.total);
        std::size_t last_drawn = 0;
        const bool ok = run_asset_bake(plan, [&](std::size_t done, std::size_t total,
                                                 const std::string& what) {
            // A frame per PERCENT, not per object: 2387 present() calls would
            // make the bake slower than the work it is reporting, and the bar
            // would be the bottleneck rather than the forge.
            const std::size_t step = std::max<std::size_t>(1, total / 100);
            if (done != total && done - last_drawn < step) {
                return;
            }
            last_drawn = done;
            draw_bake_progress(done, total, what);
        });
        if (!ok) {
            std::fprintf(stderr, "[bake] ЧАСТЬ ОБЪЕКТОВ НЕ ЗАПИСАНА — карты, "
                                 "которые их читают, откроются неполными\n");
        }
    }

    // ПРИБОР ТРЕТЬЕГО ЛИЦА. Читается ЗДЕСЬ, а не в кадре: значение, читаемое
    // каждый кадр, разрешает ленте поменять смысл на середине.
    if (const char* v = door_value("DFN_CAM_PROBE"); v != nullptr && v[0] != '\0'
                                                     && v[0] != '0') {
        cam_probe_ = true;
    }
    if (const char* v = door_value("DFN_CAM_ORBIT"); v != nullptr && v[0] != '\0') {
        cam_probe_spin_ = static_cast<float>(std::atof(v));
    }
    // СТЕНД ПЕРСОНАЖА. Обе двери читаются ЗДЕСЬ по той же причине, что и обвод
    // выше: доза, читаемая каждый кадр, разрешает ленте поменять смысл на
    // середине, и две руки сравнения перестают отличаться только дозой.
    if (const char* v = door_value("DFN_STAND_CAM"); v != nullptr && v[0] != '\0') {
        const int n = std::atoi(v);
        if (n >= 1 && n <= static_cast<int>(STAND_CAMERA_COUNT)) {
            stand_cam_ = static_cast<uint32_t>(n);
            third_person_ = true; // фигуру снимают снаружи, иначе снимать нечего
            const StandCamera cam = stand_camera(stand_cam_);
            cam_boom_desc_.back = cam.back_m;
            cam_boom_desc_.lift = cam.lift_m;
            std::fprintf(stderr, "[stand] камера %u «%s»: азимут %+.0f, тангаж "
                                 "%+.0f, стрела %.2f м\n",
                         stand_cam_, cam.label,
                         static_cast<double>(cam.orbit_yaw_deg),
                         static_cast<double>(cam.orbit_pitch_deg),
                         static_cast<double>(cam.back_m));
        } else {
            std::fprintf(stderr, "[stand] DFN_STAND_CAM=%s — не поза стенда, их "
                                 "%u. Дверь ОТКАЗАНА вслух.\n", v,
                         STAND_CAMERA_COUNT);
        }
    }
    if (const char* v = door_value("DFN_STAND_SEQ"); v != nullptr && v[0] == '1') {
        stand_seq_ = true;
        std::fprintf(stderr, "[stand] очередь клипов: %.0f с от Idle до Sit\n",
                     static_cast<double>(STAND_SEQUENCE_S));
    }

    catalog_ = scan_map_catalog("assets/maps");
    menu_.set_catalog(&catalog_);
    // DFN_MENU_PAGE=root|maps|pause|calibrate -- which page an unattended run
    // opens on.
    // Without it only the root page is photographable, because the map picker
    // and the pause page can be reached ONLY by a hand on the keyboard, so two
    // of the three screens the player actually sees have never been evidence.
    // Refused out loud on an unknown value, like every other tooling door here:
    // falling back to root would archive a root frame under a pause filename.
    if (const char* mp = door_value("DFN_MENU_PAGE"); mp != nullptr && *mp != '\0') {
        const std::string page(mp);
        if (page == "root") {
            menu_.open(MenuPage::Root);
        } else if (page == "maps" || page == "categories") {
            // The browser's first level. Play target by default; DFN_EDITOR
            // below re-opens it as the editor browser if set.
            menu_.open_browser(BrowseTarget::Play);
        } else if (page == "category_maps") {
            // The browser's SECOND level, so the map list is photographable too
            // (Rule 27). Lands on the first category that actually has maps --
            // an empty list would prove nothing.
            menu_.open_browser(BrowseTarget::Play);
            size_t first_with_maps = 0;
            for (size_t i = 0; i < catalog_.categories.size(); ++i) {
                if (!catalog_.categories[i].maps.empty()) {
                    first_with_maps = i;
                    break;
                }
            }
            menu_.open_category(first_with_maps);
        } else if (page == "pause") {
            menu_.open(MenuPage::Pause);
        } else if (page == "calibrate") {
            // The calibration page is reachable only through the root by hand,
            // exactly as the map picker and the pause page were. Same argument
            // as the branches above: a screen the player sees and a run cannot
            // photograph is not evidence -- and this is the one screen the whole
            // brightness dial exists for.
            menu_.open(MenuPage::Calibrate);
        } else if (page == "settings") {
            menu_.open(MenuPage::Settings);
        } else if (page == "settings_video") {
            // ГРУППЫ НАСТРОЕК — КАЖДАЯ СВОЕЙ ДВЕРЬЮ (заказ владельца 28.08). До
            // разбивки на группы «settings» снимала единственную страницу
            // значений; теперь та же строка снимает ОГЛАВЛЕНИЕ, а сами значения
            // стали недостижимы для прогона — то есть две страницы из трёх
            // перестали быть доказательством молча. Правило 27 требует
            // обратного, и цена — две строки.
            menu_.open(MenuPage::SettingsVideo);
        } else if (page == "settings_audio") {
            menu_.open(MenuPage::SettingsAudio);
        } else if (page == "controls") {
            // The key list, and it is reachable only two levels in by hand
            // (settings -> controls) -- the same argument as every branch
            // above. It is also the page most likely to be quietly WRONG, so
            // being able to photograph it is worth more here than elsewhere.
            menu_.open(MenuPage::Controls);
        } else if (page == "credits") {
            // ТИТРЫ, и они снимаются не ради красоты: там стоит строка
            // атрибуции силуэта дуба, без которой герб использовать нельзя
            // (CC BY 2.0, assets/branding/README.txt). Требование лицензии,
            // которое нельзя предъявить кадром, — это требование, о котором
            // через месяц никто не вспомнит.
            menu_.open(MenuPage::Credits);
        } else if (page == "stub") {
            // Страница «этого ещё нет», на которую ведут пункты без систем.
            menu_.open_stub("menu.stub.no_saves");
        } else if (page == "splash") {
            // Кадр студии. Он показывается ДО меню и по таймеру, то есть в
            // обычном прогоне его нельзя ни поймать, ни снять.
            menu_.open(MenuPage::Splash);
        } else {
            std::fprintf(stderr,
                         "[menu] DFN_MENU_PAGE=\"%s\" is not "
                         "root|categories|category_maps|pause|calibrate|settings|controls|"
                         "credits|stub|splash -- "
                         "REFUSING to run, because a root frame filed under "
                         "\"%s\" is worse than no frame\n",
                         mp, mp);
            return false;
        }
    }
    {
        const auto fb = window_->framebuffer_size();
        camera_.set_projection(static_cast<float>(config::CAMERA_FOV_Y),
                               static_cast<float>(fb.x) / static_cast<float>(fb.y),
                               static_cast<float>(config::CAMERA_NEAR),
                               static_cast<float>(config::CAMERA_FAR));
    }

    // The readout is a KEY (F3), but a key cannot be pressed by a tour, and
    // Rule 27 wants a frame of it. So it also has an env door -- the same
    // shape as every other verification hook here, and the reason the readout
    // can be shown in evidence at all.
    if (const char* dbg = door_value("DFN_DEBUG_OVERLAY");
        dbg != nullptr && *dbg == '1') {
        debug_overlay_ = true;
    }
    // WIREFRAME DOOR (В28), the key-4 toggle's Rule 27 twin. The bgfx backend
    // already honours DFN_WIREFRAME=1 itself (render's acceptance recipe); this
    // mirrors the flag app-side so the editor overlay's [каркас] tag agrees with
    // what is on screen, and so a frame of wireframe is reachable without a key.
    if (const char* wf = door_value("DFN_WIREFRAME"); wf != nullptr && *wf == '1') {
        wireframe_ = true;
        renderer_->set_wireframe(true);
    }

    // STATE CAPTURE destination and STATE RESTORE source.
    capture_dir_ = [] {
        const char* d = door_value("DFN_CAPTURE_DIR");
        return std::string(d != nullptr ? d : "captures");
    }();
    // THE ERROR_CODE OVERLOAD, NOT THE THROWING ONE. The throwing form killed
    // the process before the world loaded whenever DFN_CAPTURE_DIR named an
    // existing path, and render lost three probe runs to it: no PNG, no
    // sidecar, and a run that measured NOTHING looked exactly like a run that
    // measured zero. That is the second defect in this capture path whose
    // failure mode is a legitimate-looking zero, which is the failure mode
    // worth being paranoid about here.
    std::error_code cap_dir_ec;
    std::filesystem::create_directories(capture_dir_, cap_dir_ec);
    if (cap_dir_ec && !std::filesystem::is_directory(capture_dir_)) {
        std::fprintf(stderr, "[capture] cannot use directory \"%s\": %s\n",
                     capture_dir_.c_str(), cap_dir_ec.message().c_str());
    }
    if (const char* ca = door_value("DFN_CAPTURE_AFTER"); ca != nullptr) {
        capture_after_s_ = std::strtod(ca, nullptr);
    }
    // The same door counted in FRAMES. A run that fires on a wall-clock second
    // reaches a different frame number on a loaded machine than on an idle one,
    // so two arms of one recipe cannot be compared bit for bit -- which is the
    // whole method every zone's acceptance rests on. Requested by ui after it
    // measured the residue: 412 pixels still differed between identical runs
    // once the sky's own clocks were pinned, and this was all of it.
    if (const char* cf = door_value("DFN_CAPTURE_AFTER_FRAMES"); cf != nullptr) {
        capture_after_frames_ = std::strtoull(cf, nullptr, 10);
        if (capture_after_frames_ == 0) {
            std::fprintf(stderr,
                         "[capture] DFN_CAPTURE_AFTER_FRAMES=\"%s\" is not a positive "
                         "frame count -- REFUSING to run, because a door that "
                         "silently does nothing is worse than no door\n",
                         cf);
            return false;
        }
    }

    // ЗАПИСЬ ПРОХОДА (DFN_RECORD_EVERY=<кадров>): покадровая лента + строки
    // состояния для субтитров. Ноль отвергается вслух, как у соседей.
    if (const char* re = door_value("DFN_RECORD_EVERY"); re != nullptr) {
        record_every_ = std::strtoull(re, nullptr, 10);
        if (record_every_ == 0) {
            std::fprintf(stderr,
                         "[record] DFN_RECORD_EVERY=\"%s\" is not a positive "
                         "frame count -- REFUSING to run\n",
                         re);
            return false;
        }
    }

    // THE DOSE DOOR FOR THE SCREENSHOT KEY (DFN_SHOT_AFTER=<frames>). Key 5 is
    // reachable only by a human hand, and a feature only a hand can reach is a
    // feature nobody can prove works (Rule 27) -- so the same act is available
    // without one, and the acceptance run checks all three artifacts it is
    // supposed to leave: the .png, the chat line carrying "capture", and the
    // capture column in the flushed trace.
    //
    // REFUSES A ZERO OUT LOUD, like its neighbour above and for the reason that
    // neighbour records: a door that silently does nothing produces a run that
    // is indistinguishable from a run where the feature is broken.
    if (const char* xe = door_value("DFN_INTERIOR_EXIT"); xe != nullptr) {
        interior_exit_frames_ = std::strtoull(xe, nullptr, 10);
    }
    if (const char* sf = door_value("DFN_SHOT_AFTER"); sf != nullptr) {
        shot_after_frames_ = std::strtoull(sf, nullptr, 10);
        if (shot_after_frames_ == 0) {
            std::fprintf(stderr,
                         "[shot] DFN_SHOT_AFTER=\"%s\" is not a positive frame "
                         "count -- REFUSING to run, because a door that silently "
                         "does nothing is worse than no door\n",
                         sf);
            return false;
        }
    }

    // THE FRAME LOG (DFN_FRAME_LOG=<path>). See App.h for why this exists and
    // is not another screenshot door. It opens LOUDLY: a run that logged
    // nothing must not be mistakable for a run that logged zeros -- that exact
    // confusion already cost three probe runs on the line above.
    if (const char* fl = door_value("DFN_FRAME_LOG"); fl != nullptr && *fl != '\0') {
        frame_log_ = std::fopen(fl, "wb");
        if (frame_log_ == nullptr) {
            std::fprintf(stderr, "[frame_log] cannot open \"%s\" for writing\n", fl);
        } else {
            std::fprintf(frame_log_,
                         "# Daggerfall N per-frame log -- one line per PRESENTED frame.\n"
                         "# No readback, no settle, no cooldown: this instrument cannot\n"
                         "# quiet the thing it is pointed at. Between-frames motion is\n"
                         "# arithmetic on adjacent lines.\n"
                         "# frame dt_ms game_s speed fov_y eye_x eye_y eye_z yaw pitch"
                         " prev_draws prev_tris prev_backend_draws\n"
                         "# prev_* are the renderer's counters for the LAST COMPLETED\n"
                         "# frame (frame_stats contract) -- one line behind the pose\n"
                         "# columns by construction, and NAMED so: a route profile\n"
                         "# reads medians, where the shift is nothing; a per-frame\n"
                         "# splice must shift them itself and now cannot do it silently.\n");
        }
    }

    // THE CHAT VERIFICATION DOOR (DFN_CHAT_MSG="text"). Writes one entry into
    // the active map's chat, with the frame's capture attached, and closes --
    // so the chat path is provable without a hand on the keyboard, exactly as
    // DFN_CAPTURE_AFTER proves the capture path (Rule 27). DFN_CHAT_WHO sets the
    // role: default "human" (a player remark), or a ZONE NAME to write a demo
    // self-doc line (O1, e.g. DFN_CHAT_WHO=flora). Serviced after render().
    if (const char* msg = door_value("DFN_CHAT_MSG"); msg != nullptr && *msg != '\0') {
        ChatEntry e;
        const char* who = door_value("DFN_CHAT_WHO");
        e.who = (who != nullptr && *who != '\0') ? who : "human";
        e.text = msg;
        chat_pending_entry_ = std::move(e);
        chat_pending_ = true;
        chat_then_close_ = true;
    }

    // TRAJECTORY REPLAY DOOR (DFN_TRAJ_PLAY=<file>). Loaded BEFORE the world is
    // built, because the file names WHICH stand it was recorded in -- replaying
    // into a different world would be a coincidence, not a reproduction. The
    // replay then drives the camera and the counted clock in run(), so two
    // playbacks render bit-for-bit (Rule 53). Closes when the file is spent.
    if (const char* tp = door_value("DFN_TRAJ_PLAY"); tp != nullptr && *tp != '\0') {
        TrajectoryPlayer pl;
        if (!pl.load(tp)) {
            std::fprintf(stderr, "[traj] cannot play %s\n", tp);
        } else {
            config_.start_stand = pl.stand();
            config_.show_menu = false;
            traj_play_then_close_ = true;
            traj_play_ = std::move(pl);
        }
    }
    // TRAJECTORY RECORD DOOR (DFN_TRAJ_REC=<file>). Arms recording for the whole
    // run and writes on stop -- pair it with DFN_EDITOR/DFN_OPEN_MAP (free
    // camera) or DFN_PLAYTEST_ROUTE (a scripted walk) to record hands-free.
    // Interactive recording is the R key in the editor.
    if (const char* tr = door_value("DFN_TRAJ_REC"); tr != nullptr && *tr != '\0') {
        traj_rec_out_ = tr;
        traj_rec_arm_ = true;
    }

    // DFN_RESTORE names a sidecar written by F2. Read BEFORE the world is
    // built, because the capture says WHICH stand to build -- restoring a pose
    // into the default map and then noticing the mismatch would be a worse
    // version of the same feature.
    if (const char* rp = door_value("DFN_RESTORE"); rp != nullptr && *rp != '\0') {
        std::ifstream in(rp, std::ios::binary);
        if (!in) {
            std::fprintf(stderr, "[restore] cannot open %s\n", rp);
        } else {
            const std::string text((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
            restore_ = parse_snapshot(text);
            if (!restore_) {
                std::fprintf(stderr, "[restore] %s is not a state capture\n", rp);
            } else {
                // The capture decides the map and the menu is skipped: a
                // restore that stopped at a start screen would need the player
                // to pick the right map by hand, which is the mistake the file
                // exists to prevent.
                config_.start_stand = restore_->stand;
                config_.show_menu = false;
            }
        }
    }

    bool editor_door = door_value("DFN_EDITOR") != nullptr;

    // (A) THE CONCRETE-MAP DOOR (DFN_OPEN_MAP=<category>/<map>). Automated: load
    // exactly this .map, bypassing the browser, and enter Editor if DFN_EDITOR
    // is set or Playing otherwise. A miss is loud and fatal -- an automated run
    // that silently loaded the wrong world is worse than one that stops.
    if (const char* om = door_value("DFN_OPEN_MAP"); om != nullptr && *om != '\0') {
        const std::string addr(om);
        const size_t slash = addr.find('/');
        const MapManifest* m =
            slash == std::string::npos
                ? nullptr
                : catalog_.find(addr.substr(0, slash), addr.substr(slash + 1));
        if (m == nullptr) {
            std::fprintf(stderr,
                         "[maps] DFN_OPEN_MAP=\"%s\" not found under assets/maps "
                         "-- REFUSING to run (want <category>/<map>)\n",
                         om);
            return false;
        }
        if (!open_map(*m)) {
            return false; // open_map reported the reason
        }
        // A CAMERA DOOR SET WITHOUT DFN_EDITOR=1 USED TO DO NOTHING, SILENTLY,
        // and the frame that came out was taken from the player's spawn — which
        // on a house map is INSIDE the house. The houses zone lost a capture to
        // exactly that and had to work out why from the picture. A door that is
        // set and ignored is worse than one that does not exist: the recipe
        // looks right, the frame looks wrong, and nothing connects them. So the
        // editor mode is entered FOR the author here, and the substitution is
        // announced — silently entering it would be the same lie in the other
        // direction.
        if (!editor_door) {
            const char* cam = door_value("DFN_EDITOR_CAM");
            const char* rel = door_value("DFN_EDITOR_CAM_REL");
            if ((cam != nullptr && *cam != '\0') || (rel != nullptr && *rel != '\0')) {
                std::fprintf(stderr,
                             "[maps] DFN_EDITOR_CAM%s is set but DFN_EDITOR is not "
                             "-- entering the editor anyway, or the frame would be "
                             "taken from the PLAYER SPAWN and the door would look "
                             "broken\n", rel != nullptr && *rel != '\0' ? "_REL" : "");
                editor_door = true;
            }
        }
        if (editor_door) {
            // ДВЕРЬ ОТКРЫВАЕТ РЕДАКТОРСКУЮ СЕССИЮ, а не только режим камеры:
            // рецепты, снимающие панели и инструменты, приходят сюда, и гейт
            // обязан пропускать ровно тот путь, которым ходит человек из меню.
            set_editor_session(true, "дверь DFN_EDITOR");
            enter_editor_mode();
            // THE RELATIVE DOOR (DFN_EDITOR_CAM_REL=x,height_above_ground,z,
            // yaw,pitch), asked for by the flora zone after it lost two days
            // to a camera 11.8 m under the world. Same five fields, but the
            // second is measured FROM THE GROUND, so a recipe stays right on
            // any stand: the gallery's terrain sits at ~25 m, the one-tree
            // stand's elsewhere, and no author should have to know either to
            // frame a picture. Read BEFORE the absolute door so a recipe
            // carrying both gets the absolute one it spelled out.
            if (const char* rel = door_value("DFN_EDITOR_CAM_REL");
                rel != nullptr && *rel != '\0') {
                float x = 0, above = 0, z = 0, yaw = 0, pitch = 0;
                if (std::sscanf(rel, "%f,%f,%f,%f,%f", &x, &above, &z, &yaw, &pitch)
                    == 5) {
                    const float ground = chunks_.height_at({x, z}).value_or(0.0f);
                    editor_cam_.set_pose({x, ground + above, z}, yaw, pitch);
                    std::fprintf(stderr,
                                 "[editor] DFN_EDITOR_CAM_REL: ground %.2f m + %.2f = "
                                 "eye at %.2f m\n",
                                 static_cast<double>(ground), static_cast<double>(above),
                                 static_cast<double>(ground + above));
                } else {
                    std::fprintf(stderr,
                                 "[editor] DFN_EDITOR_CAM_REL=\"%s\" is not "
                                 "x,height_above_ground,z,yaw,pitch -- REFUSED, "
                                 "keeping the player-eye seed\n",
                                 rel);
                }
            }
            if (const char* cam = door_value("DFN_EDITOR_CAM");
                cam != nullptr && *cam != '\0') {
                float x = 0, y = 0, z = 0, yaw = 0, pitch = 0;
                if (std::sscanf(cam, "%f,%f,%f,%f,%f", &x, &y, &z, &yaw, &pitch) == 5) {
                    editor_cam_.set_pose({x, y, z}, yaw, pitch);
                    // SAY IT WHEN THE EYE IS UNDERGROUND. The y here is an
                    // ABSOLUTE world height, and a stand's ground is wherever
                    // its terrain put it — the gallery sits at ~25 m, not at
                    // zero. A camera below that renders the UNDERSIDE of the
                    // world: green above, sky below, and a perfectly upright
                    // HUD over it. That frame reads as "the capture is flipped
                    // vertically", and it cost the flora zone two days and six
                    // reproductions of a renderer bug that does not exist.
                    //
                    // Warned, NOT clamped: looking at the world from beneath is
                    // a legitimate thing to want, and a door that silently
                    // moved the camera would be lying about the pose its own
                    // sidecar records.
                    if (const auto ground = chunks_.height_at({x, z})) {
                        if (y < *ground) {
                            std::fprintf(stderr,
                                         "[editor] DFN_EDITOR_CAM y=%.2f is BELOW the "
                                         "ground here (%.2f m): the frame will show the "
                                         "world from underneath, which looks like an "
                                         "upside-down capture. Raise y above %.2f to "
                                         "stand on it.\n",
                                         static_cast<double>(y),
                                         static_cast<double>(*ground),
                                         static_cast<double>(*ground));
                        }
                    }
                } else {
                    std::fprintf(stderr,
                                 "[editor] DFN_EDITOR_CAM=\"%s\" is not "
                                 "x,y,z,yaw,pitch -- keeping the player-eye seed\n",
                                 cam);
                }
            }
        } else {
            set_editor_session(false, "дверь DFN_OPEN_MAP без DFN_EDITOR");
            mode_ = AppMode::Playing;
        }
        // A PAUSE PAGE OVER NOTHING IS NOT EVIDENCE OF THE PAUSE PAGE. Until
        // today DFN_MENU_PAGE=pause could only be photographed with no world
        // loaded, so the frame showed the rows over a flat ground -- and the
        // whole design of that page (a HALF veil, so the player still sees
        // where he left off, with a plate under the text because the veil is
        // not what the words stand on) is a claim about what happens over a
        // LIVE frame. DFN_OPEN_MAP=<map> DFN_MENU_PAGE=pause now loads the
        // world and then pauses it, which is the state a player is actually in.
        if (const char* mp = door_value("DFN_MENU_PAGE");
            mp != nullptr && std::string(mp) == "pause") {
            paused_from_ = mode_;
            mode_ = AppMode::Menu;
            input_->set_cursor_captured(false);
            menu_.open(MenuPage::Pause);
            return true;
        }
        input_->set_cursor_captured(!unattended_run());
        return true;
    }

    // (B) THE EDITOR BROWSER BOOT (DFN_EDITOR=1, no concrete map). Opens the
    // editor's map browser rather than a world -- the whole point of the browser
    // is that entering the editor does NOT jump into a map. A menu page picked
    // by DFN_MENU_PAGE above is respected.
    if (editor_door) {
        mode_ = AppMode::Menu;
        input_->set_cursor_captured(false);
        if (door_value("DFN_MENU_PAGE") == nullptr) {
            menu_.open_browser(BrowseTarget::Editor);
        }
        return true;
    }

    // (C) NORMAL LAUNCH. Menu-first: the engine is up but no world exists until
    // a map is chosen in the browser. The legacy DFN_MENU=0 path still builds a
    // stand directly for the older tooling doors (tour, capture, restore).
    //
    // THE MENU-SHOT DOORS WANT THE MENU SHOWN, NOT SKIPPED -- the exact opposite
    // of the world-target doors. `unattended_run()` forced show_menu off for
    // them (they gate the cursor and the counted clock like any evidence door),
    // so re-assert the menu here: DFN_MENU_PAGE names a screen to photograph,
    // DFN_MENU_SHOT shoots one. Without this the browser -- the new screen this
    // whole cut exists for -- was unphotographable by a door (Rule 27), and it
    // only worked earlier because DFN_EDITOR forced the menu in branch B. This
    // is the third time a refactor swept a menu-shot door into the menu-SKIP: it
    // gates the cursor, it does NOT gate the menu (see the note at
    // unattended_run()).
    const bool wants_menu_screen = door_value("DFN_MENU_PAGE") != nullptr
                                   || door_value("DFN_MENU_SHOT") != nullptr;
    if (config_.show_menu || wants_menu_screen) {
        mode_ = AppMode::Menu;
        input_->set_cursor_captured(false);
        // THE STUDIO'S FRAME AT LAUNCH (owner, 26.08: «рисуй его отдельным
        // сплэш-кадром при запуске приложения»). Two guards, and both matter:
        //
        // NOT ON AN UNATTENDED RUN, EVER. Every tour, playtest, capture and
        // menu-shot recipe in this tree starts by measuring frames, and a
        // splash prepended to all of them would shift every counted frame in
        // the project by two seconds. Nobody would file that as this change's
        // fault -- it would look like the world got slower.
        //
        // NOT WHEN A DOOR ALREADY NAMED A PAGE: DFN_MENU_PAGE says which screen
        // to photograph, and covering it with a title card is the one thing
        // that door exists to prevent.
        //
        // ДЛИТЕЛЬНОСТЬ БЕРЁТСЯ У АКТИВА, А НЕ НАЗНАЧАЕТСЯ ЗДЕСЬ (заказ 27.08:
        // интро — предзаписанное видео). Число в коде и число в файле — это два
        // определения одной длительности: разойдясь, они дают либо обрезанное
        // интро, либо чёрный экран в хвосте, и ни то ни другое не сообщает о
        // себе. Актива нет — остаётся прежняя нарисованная заставка и её
        // прежние 2.2 с.
        constexpr float SPLASH_DEFAULT_S = 2.2f;
        const float intro_s = intro_video().duration_s();
        const float splash_len = intro_s > 0.0f ? intro_s : SPLASH_DEFAULT_S;
        const bool named_page = door_value("DFN_MENU_PAGE") != nullptr;
        float splash = (unattended_run() || named_page) ? 0.0f : splash_len;
        if (const char* v = door_value("DFN_SPLASH"); v != nullptr && *v != '\0') {
            splash = static_cast<float>(std::atof(v));
        }
        if (menu_.page() == MenuPage::Splash) {
            // DFN_MENU_PAGE=splash asked for THIS page on purpose. Give it a
            // duration whatever the guards above decided, and start the clock
            // HALF WAY IN: the frame fades, so t=0 is a black rectangle, and a
            // door that photographs the page it was asked for must photograph
            // it at the opacity a player sees, not at the one frame where it
            // is not there yet.
            splash = splash > 0.0f ? splash : splash_len;
            // НА ПИКЕ, А НЕ НА ПОЛОВИНЕ. У нарисованной заставки середина и
            // была пиком; у интро пик — это доля, названная в tools/gen_intro.py
            // (пауза 0.30 + восход 1.35 из 3.50 = 0.47 длительности). Число
            // повторено здесь ОДНОЙ строкой сознательно: снимок двери — это
            // приборный кадр, и требовать ради него общий заголовок с кривой
            // яркости значило бы завести зависимость шире, чем предмет.
            menu_.tick(splash * (intro_s > 0.0f ? 0.52f : 0.5f));
        }
        menu_.set_splash_seconds(splash);
        if (splash > 0.0f && menu_.page() == MenuPage::Root) {
            menu_.open(MenuPage::Splash);
        }
    } else {
        if (!enter_world(config_.start_stand)) {
            return false;
        }
    }
    return true;
}

// Builds (or rebuilds) the world for one demo map. Everything that depends on
// terrain existing lives here: streaming, edge walls, the chunk ferry, the
// player, the testbed content, the body, the mirror puppet and the playtest.
// --- THE COMPOSITION'S DETAIL LADDER ----------------------------------------
//
// A composition is uploaded in 32 m tiles (see enter_world), and each tile is
// baked either from the objects' near forms or from their `-far` forms. This is
// NOT the FloraLod ladder: that one thins a GENERATED scatter lattice, while
// this one swaps one baked object for another baked object. Two different
// machines with one purpose, and conflating them would put registry objects
// through a re-bake that knows nothing about them.
//
// THE SWITCH IS PER TILE, NOT PER TREE, and that is what makes it cheap AND
// quiet: within a tile nothing pops relative to its neighbours, and the seam
// between two tiles is at least SCENE_FAR_ON_M away from the eye.
namespace {
/// Distance (metres, to the tile's NEAREST point) at which a tile drops to the
/// cheaper form, and the distance at which it comes back. The band is
/// hysteresis: it is WIDER THAN A TILE on purpose, so walking one tile's width
/// cannot make a tile flap between forms — a flap would be a visible pulse in
/// the middle distance, which is worse than either form.
///
/// The numbers come from what the far form actually drops. Flora's `-far`
/// removes detail it measured as invisible past ~32 m (side feather fans, the
/// dead-branch skirt, acorns). 44 m is that distance with ~35 % margin, so the
/// swap happens where the dropped detail was already gone; 56 m gives a 12 m
/// band, more than the 32 m tile's half-width of travel needed to cross back.
constexpr float SCENE_FAR_ON_M = 56.0f;
constexpr float SCENE_FAR_OFF_M = 44.0f;
} // namespace

void App::bake_scene_tile(SceneTile& tile, bool far_form) {
    render::MeshData wood;
    render::MeshData cards;

    for (const world::Placement& p : tile.parts) {
        // The far form when there IS one; the near form otherwise. Falling back
        // rather than skipping is the whole reason an absent `-far` is legal.
        auto it = far_form ? scene_objects_.find(p.object + "-far")
                           : scene_objects_.end();
        if (it == scene_objects_.end()) {
            it = scene_objects_.find(p.object);
        }
        if (it == scene_objects_.end()) {
            continue;
        }
        const render::RegistryObject& obj = it->second;
        render::append_transformed(wood, obj.wood, p.position, p.yaw, p.scale);
        render::append_transformed(cards, obj.cards, p.position, p.yaw, p.scale);
        render::append_transformed(cards, obj.bark, p.position, p.yaw, p.scale);
        render::append_transformed(cards, obj.ground, p.position, p.yaw, p.scale);
    }
    render_system_.upload_prebuilt_scatter(*renderer_, tile.key, wood, cards);
    tile.far_form = far_form;
}

void App::refresh_scene_lod(glm::vec3 eye) {
    SceneTile* worst = nullptr;
    float worst_distance = 0.0f;
    bool worst_far = false;
    for (SceneTile& tile : scene_tiles_) {
        // Distance to the tile's NEAREST point, not to its centre: a 32 m tile
        // whose near edge is under the player's feet has its centre 22 m away,
        // and a ladder measured from the centre would coarsen the ground he is
        // standing on.
        const glm::vec2 e{eye.x, eye.z};
        const glm::vec2 d = glm::max(glm::max(tile.min_xz - e, e - tile.max_xz),
                                     glm::vec2{0.0f});
        const float dist = glm::length(d);
        const bool want = tile.far_form ? dist > SCENE_FAR_OFF_M : dist > SCENE_FAR_ON_M;
        if (want == tile.far_form) {
            continue;
        }
        if (worst == nullptr || dist < worst_distance) {
            worst = &tile;
            worst_distance = dist;
            worst_far = want;
        }
    }
    if (worst != nullptr) {
        bake_scene_tile(*worst, worst_far);
    }
}

// enter_world УЕХАЛА В AppWorld.cpp (заказ 18.08: «enter_world точно можно в
// другом месте расписать, ф-ция на 1300 строк»). Реализация класса разложена по
// нескольким .cpp — заголовок остаётся один, и это ровно то, что он просил.

namespace {

// How far `value` still has to travel to reach `target`. Negative means it is
// already there or past. `direction` is the probe's DECLARED travel direction,
// not the measured one: several render frames can share a simulation tick, and
// a zero measured step read as "ascending" fired every shot at once.
[[nodiscard]] float distance_to(float value, float target, int direction) {
    if (direction == 0) { // a cycle in [0,1) that only ever advances forward
        return std::fmod(target - value + 1.0f, 1.0f);
    }
    return direction > 0 ? target - value : value - target;
}

} // namespace

void App::body_probe_drive() {
    if (!body_probe_) {
        return;
    }
    BodyProbe& p = *body_probe_;
    p.elapsed_s += static_cast<float>(config::SIM_DT);
    auto* ps = world_.get<gameplay::PlayerState>(player_);
    if (ps == nullptr) {
        return;
    }
    if (p.mode == "stride") {
        // The bot walks; the probe only aims the eye at its own feet.
        ps->pitch = p.pitch;
        return;
    }
    if (p.mode == "gait") {
        // STRAFE, facing the double across the mirror plane. The stride clock
        // advances from real horizontal displacement at the real walk speed, so
        // the phase, the step length and therefore the leg cycle are exactly
        // the walking ones; only the facing is turned out of the travel
        // direction, which this rig's v1 does not model anyway (no strafe
        // clip). The double reflects x=x and z=-z, so it tracks alongside at a
        // FIXED distance while both of us walk — the framing cannot drift.
        const auto* self = world_.get<components::Transform>(player_);
        const auto* other = world_.get<components::Transform>(mirror_puppet_);
        if (self != nullptr && other != nullptr) {
            const glm::vec2 d{other->position.x - self->position.x,
                              other->position.z - self->position.z};
            if (glm::dot(d, d) > 1.0e-6f) {
                p.aim_yaw = std::atan2(d.x, -d.y);
            }
        }
        ps->yaw = p.aim_yaw;
        ps->pitch = p.pitch;
        ps->pending_look = {0.0f, 0.0f};
        ps->move_axes = {1.0f, 0.0f};
        ps->run = false;
        return;
    }
    if (p.mode == "profile" || p.mode == "plant") {
        // Walk around to the double's side during the warm-up, then stand and
        // watch it from there. The showcase double faces a fixed direction, so
        // a vantage off its shoulder is a true side profile.
        const auto* self = world_.get<components::Transform>(player_);
        const auto* other = world_.get<components::Transform>(mirror_puppet_);
        if (self != nullptr && other != nullptr) {
            const glm::vec2 me{self->position.x, self->position.z};
            const glm::vec2 it{other->position.x, other->position.z};
            const glm::vec2 stand = it + glm::vec2{4.5f, 1.5f};
            const glm::vec2 leg = stand - me;
            const bool travelling = glm::length(leg) > 0.6f
                                    && p.elapsed_s < p.warmup_s - 0.5f;
            const glm::vec2 aim = travelling ? leg : (it - me);
            if (glm::dot(aim, aim) > 1.0e-6f) {
                ps->yaw = std::atan2(aim.x, -aim.y);
            }
            ps->move_axes = travelling ? glm::vec2{0.0f, 1.0f} : glm::vec2{0.0f, 0.0f};
        }
        ps->run = false;
        ps->pending_look = {0.0f, 0.0f};
        ps->pitch = p.pitch;
        return;
    }
    // Showcase and mirror: stand still, face the double. The mirror run then
    // turns LEFT at a fixed rate, and the double must answer to ITS left.
    ps->move_axes = {0.0f, 0.0f};
    ps->run = false;
    ps->pending_look = {0.0f, 0.0f};
    if (!p.aimed) {
        const auto* self = world_.get<components::Transform>(player_);
        const auto* other = world_.get<components::Transform>(mirror_puppet_);
        if (self != nullptr && other != nullptr) {
            const glm::vec2 d{other->position.x - self->position.x,
                              other->position.z - self->position.z};
            if (glm::dot(d, d) > 1.0e-6f) {
                p.aim_yaw = std::atan2(d.x, -d.y);
            }
        }
        p.aimed = p.elapsed_s >= p.warmup_s;
    }
    float offset = 0.0f;
    if (p.mode == "mirror" && p.elapsed_s > p.warmup_s) {
        offset = std::max(-0.5f, -0.25f * (p.elapsed_s - p.warmup_s));
    }
    ps->yaw = p.aim_yaw + offset;
    ps->pitch = p.pitch;
}

void App::body_probe_frame(float alpha, float frame_dt) {
    if (!body_probe_ || renderer_ == nullptr) {
        return;
    }
    (void)alpha;
    (void)frame_dt;
    BodyProbe& p = *body_probe_;
    if (p.next >= p.targets.size() || p.elapsed_s > p.warmup_s + 60.0f) {
        if (!p.log.empty()) {
            if (std::FILE* f = std::fopen((p.dir + "/probe_log.txt").c_str(), "w")) {
                std::fwrite(p.log.data(), 1, p.log.size(), f);
                std::fclose(f);
            }
            p.log.clear();
        }
        window_->request_close();
        return;
    }
    // NOTE the cooldown is spent further down, AFTER the tracked value has been
    // refreshed: skipping the read as well let the per-frame step be measured
    // across five frames, and the tolerance derived from it fired the next shot
    // a tenth of a cycle early.

    // What this probe is watching, and the line the shot must land on.
    float now = 0.0f;
    bool wrapping = false;
    float speed = 0.0f;
    float step_len = 0.0f;
    if (p.mode == "stride" || p.mode == "gait") {
        const auto* drive = world_.get<anim::BodyDrive>(player_);
        if (drive == nullptr) {
            return;
        }
        now = drive->stride_phase;
        speed = drive->speed_mps;
        step_len = drive->step_length_m;
        wrapping = true;
        if (speed < 0.5f || !drive->grounded) {
            p.prev_value = now; // a standing frame proves nothing about a stride
            return;
        }
    } else if (p.mode == "showcase" || p.mode == "profile" || p.mode == "plant") {
        const auto* drive = world_.get<anim::BodyDrive>(mirror_puppet_);
        if (drive == nullptr) {
            return;
        }
        now = drive->showcase_time_s;
    } else {
        const auto* ps = world_.get<gameplay::PlayerState>(player_);
        if (ps == nullptr) {
            return;
        }
        now = ps->yaw - p.aim_yaw;
    }
    if (p.elapsed_s < p.warmup_s) {
        p.prev_value = now;
        return;
    }

    // The backend captures into the NEXT rendered frame, so the target is
    // tested against where the subject will BE, not where it is.
    const float delta = wrapping ? std::fmod(now - p.prev_value + 1.0f, 1.0f)
                                 : now - p.prev_value;
    const float predicted = wrapping ? std::fmod(now + delta, 1.0f) : now + delta;
    p.prev_value = now;
    if (!p.primed) {
        p.value = predicted;
        p.primed = true;
        return;
    }
    p.value = predicted;
    if (p.cooldown > 0) {
        --p.cooldown; // the backend is still flushing the previous shot
        return;
    }
    // Shoot the frame that lands NEAREST the target: when the remaining travel
    // is under one frame of it, the frame after this one would overshoot. The
    // achieved value is logged rather than assumed — at this frame rate the
    // landing error is one frame of stride, and the log says how much.
    const float tolerance = std::max(std::fabs(delta), 0.005f);
    if (distance_to(predicted, p.targets[p.next], wrapping ? 0 : p.direction)
        > tolerance) {
        return;
    }

    char name[96];
    std::snprintf(name, sizeof(name), "%02zu_%s_%.3f.png", p.next, p.mode.c_str(),
                  static_cast<double>(p.targets[p.next]));
    (void)renderer_->save_screenshot(p.dir + "/" + name);
    // The double's own facing goes in the log: "it turns the other way" is a
    // claim about a number, and the frame should not be the only witness.
    float puppet_yaw = 0.0f;
    if (const auto* pd = world_.get<anim::BodyDrive>(mirror_puppet_)) {
        puppet_yaw = pd->facing_yaw;
    }
    char line[256];
    std::snprintf(line, sizeof(line),
                  "%s target=%.3f captured=%.3f speed=%.2f step=%.2f t=%.1f "
                  "double_yaw=%.3f\n",
                  name,
                  static_cast<double>(p.targets[p.next]),
                  static_cast<double>(predicted), static_cast<double>(speed),
                  static_cast<double>(step_len), static_cast<double>(p.elapsed_s),
                  static_cast<double>(puppet_yaw));
    p.log += line;
    std::fprintf(stderr, "[body_probe] %s", line);
    ++p.next;
    p.cooldown = 4; // let the backend flush before another shot is scheduled
}

// ---------------------------------------------------------------------------
// DEBUG READOUT + STATE CAPTURE / RESTORE
//
// The user asked for one thing that is really two: a readout he can look at
// while playing, and a screenshot that carries enough state for someone else to
// stand where he was standing. They share a struct on purpose -- see
// DebugOverlay.h -- so the number he is looking at when he decides something is
// wrong is the number in the file he sends.
// ---------------------------------------------------------------------------

DebugSnapshot App::collect_snapshot(float alpha) {
    DebugSnapshot s{};
    s.stand = active_stand_;
    s.seed = 1u; // the fixed worldgen seed (Rule 13.1); see enter_world()
    s.build_commit = DFN_BUILD_COMMIT;
    {
        // Wall clock, so a folder of captures can be put back in the order the
        // player took them. Local time on purpose: its reader is the person who
        // pressed the key, and "which of these two did I take first" is the
        // only question it answers.
        const std::time_t t = std::time(nullptr);
        std::tm tm{};
        localtime_r(&t, &tm);
        char stamp[32];
        std::strftime(stamp, sizeof(stamp), "%d:%m:%Y - %H:%M:%S", &tm);
        s.captured_at = stamp;
    }

    s.game_seconds = game_seconds_;
    const double day_len = static_cast<double>(config::DAY_LENGTH_SECONDS);
    const double days = game_seconds_ / day_len;
    s.day_fraction = static_cast<float>(days - std::floor(days));
    const double lunar = days / static_cast<double>(config::LUNAR_MONTH_DAYS);
    s.lunar_phase = static_cast<float>(lunar - std::floor(lunar));

    // THE EYE, NOT THE FEET. The camera pose is what the frame was rendered
    // from, so it is what a restore must reproduce; the Transform is half a
    // body lower and would put the restored player's head where his knees
    // were. Interpolated at the same alpha render() used, for the same reason
    // the capture waits for render(): the file must describe the image.
    const auto eye = camera_.interpolated_pose(alpha);
    s.position = eye.position;
    s.yaw = eye.yaw;
    s.pitch = eye.pitch;
    const float cp = std::cos(eye.pitch);
    s.look_dir = {std::sin(eye.yaw) * cp, std::sin(eye.pitch), -std::cos(eye.yaw) * cp};

    if (const auto* ps = world_.get<gameplay::PlayerState>(player_)) {
        s.speed_mps = ps->stride_speed;
        s.vertical_velocity = ps->vertical_velocity;
        s.stride_phase = ps->stride_phase;
        s.gait = static_cast<uint8_t>(ps->gait);
        s.locomotion = static_cast<uint8_t>(ps->locomotion);
        s.grounded = !ps->airborne;
        s.crouched = ps->crouched;
        s.water_depth = ps->water_depth;
    }

    s.internal_w = config_.internal_width;
    s.internal_h = config_.internal_height;
    s.fov_y_rad = camera_.fov_y();
    s.head_bob = config_.head_bob;
    s.palette_post = config_.palette_post;

    const auto& env = render_system_.environment();
    s.wind_strength = env.wind_strength;
    s.cloud_cover = env.cloud_cover;
    s.ambient_darkness = env.ambient_darkness;

    s.fps = frame_clock_.fps();
    s.frame_ms = frame_clock_.mean_ms();
    s.frame_ms_worst = frame_clock_.worst_ms();
    s.chunks_resident = static_cast<uint32_t>(chunks_.loaded_chunks().size());
    s.lod_nodes = static_cast<uint32_t>(render_system_.lod_pending().size());
    return s;
}

// A KEY EDGE, ASKED FOR BY ACTION RATHER THAN BY KEY. This is what keeps the
// controls screen honest (Controls.h): the handlers below name what they DO,
// the table says which key does it, and the screen draws the same table -- so
// a key cannot be dispatched without a row, and a row cannot exist without a
// description. Writing `Key::NUM_4` in a handler would restore the old split,
// where the screen was a copy of the bindings instead of the bindings.
bool App::action_pressed(Action action) const {
    // THE SCOPE IS NOW OBEYED, AND UNTIL TODAY IT WAS NOT. Every row of the
    // binding table has carried a Scope since the table was written, the
    // controls screen has been SHOWING it to the user, and this function
    // ignored it — the guard lived at each call site as `mode_ == Editor &&`,
    // written by hand, sometimes forgotten. That was survivable while no two
    // rows shared a key. It stopped being survivable the moment the digits
    // 1..5 became the editor's five modes: 2 must mean the surface brush in
    // the editor and the readout in the body, and one keypress cannot ask two
    // handlers to sort that out between them.
    //
    // И ОБЛАСТЬ «ТОЛЬКО В РЕДАКТОРЕ» СПРАШИВАЕТ ПРО СЕССИЮ, А НЕ ПРО КАМЕРУ
    // (заказ владельца 27.08). mode_ == Editor отвечал на вопрос «где сейчас
    // камера», и этого было достаточно ровно до тех пор, пока в редакторский
    // режим нельзя было попасть из игры. Попасть было можно — клавишей ` — и
    // тогда все инструменты становились доступны в обычной игре. Условие здесь
    // И-овое, а не заменяющее: право строить (editor_session_) и текущий взгляд
    // (mode_) — разные вопросы, и клавиша инструмента требует обоих.
    const auto listening = [this](Scope scope) {
        switch (scope) {
        case Scope::Anywhere:    return true;
        case Scope::EditorOnly:  return editor_session_ && mode_ == AppMode::Editor;
        case Scope::PlayingOnly: return mode_ == AppMode::Playing;
        }
        return false;
    };
    const Binding& b = binding_for(action);
    if (listening(b.scope) && input_->was_pressed(b.key)) {
        return true;
    }
    // The alias answers to its OWN scope: F3 has meant the readout everywhere
    // since it existed, and archived recipes say so on disk.
    return b.alias != platform::Key::UNKNOWN && listening(b.alias_scope)
           && input_->was_pressed(b.alias);
}

void App::write_capture(const DebugSnapshot& snap) {
    char stem[64];
    std::snprintf(stem, sizeof(stem), "capture_%03d", captures_written_);
    const std::string base = capture_dir_ + "/" + stem;

    // The PNG first: if the backend refuses it, the sidecar must not claim a
    // frame that does not exist. A state file pointing at a missing image is
    // worse than no capture, because it reads as evidence.
    if (!renderer_->save_screenshot(base + ".png")) {
        std::fprintf(stderr, "[capture] screenshot FAILED, no state written: %s.png\n",
                     base.c_str());
        return;
    }
    const std::string text = format_snapshot(snap);
    if (std::FILE* f = std::fopen((base + ".txt").c_str(), "wb"); f != nullptr) {
        std::fwrite(text.data(), 1, text.size(), f);
        std::fclose(f);
    } else {
        std::fprintf(stderr, "[capture] state file FAILED: %s.txt\n", base.c_str());
        return;
    }
    // Echoed to the terminal as well as the file: the fastest path from "I saw
    // something wrong" to a repro is a copy-paste, and that needs no file
    // manager.
    std::fprintf(stderr, "[capture] %s.png + .txt\n%s", base.c_str(), text.c_str());
    ++captures_written_;

    // A LANDMARK IN THE TRACE, and it is written HERE rather than beside the
    // key that asked for it (Rule 32). Four things take captures -- key 3, key
    // 5, a chat remark, and the DFN_* doors -- and a trace that only knew about
    // one of them would answer "was this moment shot?" correctly for that one
    // and wrongly for the other three. The ring is the editor's; in Playing it
    // is empty and this costs a branch.
    //
    // The sample carries THIS moment's pose, not an interpolated one: it is
    // built from the same snapshot the .txt sidecar was written from, so the
    // trace line, the image and the state file all name one frame.
    if (mode_ == AppMode::Editor) {
        TelemetrySample t;
        t.game_seconds = snap.game_seconds;
        t.position = snap.position;
        t.yaw = snap.yaw;
        t.pitch = snap.pitch;
        t.fps = snap.fps;
        t.frame_ms = snap.frame_ms;
        t.chunks_resident = snap.chunks_resident;
        t.lod_nodes = snap.lod_nodes;
        t.capture = base + ".png";
        telemetry_.push(t);
    }
}

std::string App::chat_path_for_current_map() const {
    const MapManifest* m = current_manifest();
    if (m == nullptr) {
        return {};
    }
    // docs/MAP_LAYOUT.md: the chat is a JSONL log beside the map's own manifest,
    // so a remark "knows its map" and therefore its owner zone. The catalog was
    // scanned from "assets/maps" (init), so the path is category/file_stem there.
    return "assets/maps/" + m->category + "/" + m->file_stem + ".chat.jsonl";
}

void App::write_pending_chat(float alpha) {
    const std::string chat_path = chat_path_for_current_map();
    if (chat_path.empty()) {
        std::fprintf(stderr, "[chat] no map open; entry dropped\n");
        return;
    }
    const DebugSnapshot snap = collect_snapshot(alpha);
    ChatEntry entry = chat_pending_entry_;
    if (entry.who.empty()) {
        entry.who = "human";
    }
    // ATTACH A CAPTURE VIA THE EXISTING DFN_CAPTURE PATH -- reuse, not a second
    // screenshot pipeline (Rule 32/35, the lead's "переиспользуй снимок").
    // write_capture writes capture_NNN.{png,txt} into capture_dir_ and bumps
    // captures_written_ only on success, so the pre-call index names the files.
    const int idx = captures_written_;
    write_capture(snap);
    if (captures_written_ > idx) {
        char stem[64];
        std::snprintf(stem, sizeof(stem), "capture_%03d.png", idx);
        entry.capture = capture_dir_ + "/" + stem;
    }
    // Best-effort wall date: the same local-time stamp collect_snapshot already
    // reads (a tooling path, not the deterministic sim), an EXTRA field never the
    // order key (MAP_LAYOUT.md).
    const std::optional<std::string> date =
        snap.captured_at.empty() ? std::nullopt
                                 : std::optional<std::string>(snap.captured_at);
    (void)append_chat_entry(chat_path, snap.game_seconds, entry, date);
}

void App::apply_restore(const DebugSnapshot& snap) {
    // A restore into a different world is a coincidence, not a reproduction.
    // Said loudly rather than silently tolerated (Rule 27) -- and NOT refused,
    // because a capture from an older stand list is still the best guess
    // available and refusing would throw away the only evidence there is.
    if (snap.stand != active_stand_) {
        std::fprintf(stderr,
                     "[restore] STAND MISMATCH: capture says %u, world is %u. "
                     "The pose is being applied anyway, but this is NOT the "
                     "world the capture was taken in.\n",
                     snap.stand, active_stand_);
    }
    game_seconds_ = snap.game_seconds;

    auto* ps = world_.get<gameplay::PlayerState>(player_);
    auto* tr = world_.get<components::Transform>(player_);
    if (ps == nullptr || tr == nullptr) {
        std::fprintf(stderr, "[restore] no player to restore onto\n");
        return;
    }
    // The capture holds the EYE; the character controller is placed by its
    // FEET. Subtracting the eye height here is the inverse of the transform
    // sim applies when it writes CameraPose -- if that offset ever changes,
    // this is a second consumer of it and belongs in NUMBERS (Rule 35). It
    // already is one: PLAYER_EYE_HEIGHT.
    // THE EYE IS NOT ABOVE THE FEET, IT IS ABOVE AND FORWARD OF THEM. Undoing
    // only the height left a systematic PLAYER_EYE_FORWARD error along the
    // facing direction, so capture -> restore -> capture WALKED THE PLAYER
    // 0.10 m FORWARD EVERY TIME. Measured, not reasoned: a round trip at yaw
    // 1.93936 moved the eye by (+0.0974, +0.0394), against the predicted
    // (+0.0934, +0.0358) for a 0.10 m forward offset.
    //
    // The residual check could not have caught this, and that is the lesson:
    // it compares the achieved position against a target computed with the
    // SAME wrong formula, so it reported 0.000 m of error while the player
    // drifted. A check derived from the thing it checks is not a check. The
    // property that catches it is ROUND-TRIP IDEMPOTENCE -- restore a capture,
    // capture again, and the two files must agree.
    const float eye_h = static_cast<float>(config::PLAYER_EYE_HEIGHT);
    const float fwd = static_cast<float>(config::PLAYER_EYE_FORWARD);
    const glm::vec3 facing{std::sin(snap.yaw), 0.0f, -std::cos(snap.yaw)};
    // A CROUCHED CAPTURE RESTORES CROUCHED, and it did not before: the snapshot
    // has carried `crouched` since it was written, nothing ever applied it, so
    // every crouched frame restored standing -- and the feet were then derived
    // by subtracting the STANDING eye height from a CROUCHED eye, which buried
    // the player by the depth of the squat. Both halves are fixed here, and the
    // offset comes from the one producer that knows it rather than from a third
    // idea of where the eye is (the second one is what this commit removes).
    const glm::vec2 crouch_eye =
        snap.crouched ? anim::crouch_eye_offset(body_rig_.proportions, 1.0f)
                      : glm::vec2{0.0f, 0.0f};
    const float ahead = fwd + crouch_eye.x;
    const glm::vec3 feet{snap.position.x - facing.x * ahead,
                         snap.position.y - (eye_h - crouch_eye.y),
                         snap.position.z - facing.z * ahead};

    // A RESTORE IS A PLACEMENT, NOT A WALK. `teleport_character` is documented
    // as "instant placement without collision resolution (spawn, chunk
    // streaming)", which is exactly this operation.
    //
    // THIS CODE PREVIOUSLY CLAIMED IPhysics HAD NO TELEPORT, and built a
    // 30-frame convergence loop to work around a function that was already
    // there. The claim was mine and it was never checked -- I grepped for
    // `set_character_position`, did not find it, and wrote the conclusion into
    // a comment as fact (Rule 34). Sim caught it, and by then the false
    // premise had already been quoted into core's brief, one hop from being
    // three agents deep. It also explains a number that shipped: the 0.53 m
    // restore drift was not the capsule settling, it was the capsule WALKING
    // and running out of frames. A placement does not drift.
    physics_->teleport_character(ps->character, feet);
    tr->position = feet;
    restore_target_ = feet;
    // ONE post-hoc check, not a convergence loop. Teleport bypasses collision
    // by contract -- correct here, because the capture was taken from a legal
    // pose, so reproducing it is legal. But that also means a capture taken
    // inside geometry would now restore SILENTLY, so the next frame still
    // reports where the capsule actually ended up (sim's caveat, kept).
    restore_attempts_ = 0;

    ps->yaw = snap.yaw;
    ps->pitch = snap.pitch;
    ps->vertical_velocity = 0.0f; // a restored player is not mid-fall
    // THE KEY, NOT THE STATE. Setting `crouched` here would flag a capsule that
    // is still standing height; holding the key lets sim's own crouch state
    // machine resize the capsule and ease the blend on the next tick, which is
    // the only code allowed to decide whether there is headroom to stand again.
    ps->crouch_held = snap.crouched;
    hold_crouch_ = snap.crouched; // ...and KEPT held, see App.h

    std::fprintf(stderr,
                 "[restore] stand %u  pos %.2f %.2f %.2f  yaw %.4f  pitch %.4f  "
                 "clock %.1f  (from build %s)\n",
                 snap.stand, static_cast<double>(snap.position.x),
                 static_cast<double>(snap.position.y),
                 static_cast<double>(snap.position.z), static_cast<double>(snap.yaw),
                 static_cast<double>(snap.pitch), snap.game_seconds,
                 snap.build_commit.c_str());
}

// Resolve a browser-chosen .map to a world. `source` is the transit bridge
// until the baker lands (docs/MAP_LAYOUT.md): "stand:<id>" builds the generator
// stand, "dfw:<file>" will load a baked map and today reports honestly that no
// baker has produced one. Every failure path leaves a status the browser draws
// -- never a silent nothing (Rule 27).
bool App::open_map(const MapManifest& manifest) {
    const auto status = [&](std::string_view key, std::string_view detail) {
        std::string s(localized(serialization::fnv1a64(key)));
        if (!detail.empty()) {
            s += " ";
            s += detail;
        }
        menu_.set_browser_status(s);
    };

    std::string scheme;
    std::string value;
    if (!split_map_source(manifest.source, scheme, value)) {
        status("map.err.source", manifest.source);
        std::fprintf(stderr, "[maps] %s: source \"%s\" is not scheme:value\n",
                     manifest.file_stem.c_str(), manifest.source.c_str());
        return false;
    }
    if (scheme == "stand") {
        // The two stands core ships today. New stands add a row here AND in
        // core's StandId; a name the manifest carries but core does not know is
        // reported rather than silently mapped to the default.
        std::optional<uint32_t> stand;
        if (value == "Testbed") {
            stand = static_cast<uint32_t>(world::StandId::Testbed);
        } else if (value == "Forest") {
            stand = static_cast<uint32_t>(world::StandId::Forest);
        } else if (value == "OneTree") {
            stand = static_cast<uint32_t>(world::StandId::OneTree);
        } else if (value == "Gallery") {
            stand = static_cast<uint32_t>(world::StandId::Gallery);
        }
        if (!stand) {
            status("map.err.stand", value);
            std::fprintf(stderr, "[maps] %s: unknown stand \"%s\"\n",
                         manifest.file_stem.c_str(), value.c_str());
            return false;
        }
        // The Gallery stand reads its shelf during enter_world, so the choice
        // must land BEFORE the call; every other stand ignores it.
        gallery_objects_dir_ = manifest.objects.empty() ? "assets/objects/trees"
                                                        : manifest.objects;
        gallery_scene_ = manifest.scene;
        // The shelf list is split ONCE, here, not at every lookup: a scene of
        // two thousand parts would otherwise re-split the same string two
        // thousand times.
        gallery_shelves_.clear();
        for (std::size_t at = 0; at <= gallery_objects_dir_.size();) {
            const std::size_t sep = gallery_objects_dir_.find(';', at);
            const std::size_t end = sep == std::string::npos
                                      ? gallery_objects_dir_.size() : sep;
            std::string one = gallery_objects_dir_.substr(at, end - at);
            while (!one.empty() && (one.front() == ' ' || one.front() == '\t')) {
                one.erase(one.begin());
            }
            while (!one.empty() && (one.back() == ' ' || one.back() == '\t')) {
                one.pop_back();
            }
            if (!one.empty()) {
                gallery_shelves_.push_back(std::move(one));
            }
            if (sep == std::string::npos) {
                break;
            }
            at = sep + 1;
        }
        gallery_size_chunks_ = std::max(1, manifest.size_chunks);
        // ИМЯ ДЛЯ ЗАГОЛОВКА ЭКРАНА ЗАГРУЗКИ — ДО постройки мира: экран
        // показывается с первого её кадра, а current_map_ присваивается
        // только после удачи (см. её комментарий в App.h).
        loading_map_name_ = manifest.name;
        if (!enter_world(*stand)) {
            status("map.err.build", {});
            return false;
        }
        current_map_ = manifest; // for current_manifest() (chat path derivation)
        return true;
    }
    if (scheme == "dfw") {
        // The baker is a later cut; a .dfw source is a legitimate future map
        // with no file yet, so this is a stated "not ready", not a crash.
        status("map.err.nobake", value);
        std::fprintf(stderr,
                     "[maps] %s: source dfw:%s -- the baker has not produced this "
                     "map yet\n",
                     manifest.file_stem.c_str(), value.c_str());
        return false;
    }
    status("map.err.source", manifest.source);
    std::fprintf(stderr, "[maps] %s: unknown source scheme \"%s\"\n",
                 manifest.file_stem.c_str(), scheme.c_str());
    return false;
}

// THE FREE CAMERA IS SEEDED FROM THE EYE, NOT THE FEET, so entering the editor
// does not jump the view: the player was looking from CameraPose, and that is
// exactly where the fly begins. Falls back to the Transform + eye height only
// before the first step has published a pose.
void App::enter_editor_mode() {
    glm::vec3 eye{0.0f};
    float yaw = 0.0f;
    float pitch = 0.0f;
    if (const auto* pose = world_.get<components::CameraPose>(player_)) {
        eye = pose->position;
        yaw = pose->yaw;
        pitch = pose->pitch;
    } else if (const auto* tr = world_.get<components::Transform>(player_)) {
        eye = tr->position
              + glm::vec3{0.0f, static_cast<float>(config::PLAYER_EYE_HEIGHT), 0.0f};
        if (const auto* ps = world_.get<gameplay::PlayerState>(player_)) {
            yaw = ps->yaw;
            pitch = ps->pitch;
        }
    }
    editor_cam_.reset(eye, yaw, pitch);
    mode_ = AppMode::Editor;
}

void App::set_editor_session(bool on, const char* why) {
    editor_session_ = on;
    // ПАНЕЛИ СЛЕДУЮТ ЗА ПРАВОМ, А НЕ ЗА РЕЖИМОМ КАМЕРЫ. Владелец назвал два
    // разных отказа одним днём — «интерфейс редактора есть даже в обычной игре»
    // и «выхожу в главное меню, интерфейс панели редактора остаётся», — и оба
    // случаются, когда видимость панелей решается не там, где решается право.
    editor_ui_.set_visible(on);
    std::fprintf(stderr, "[режим] сессия %s (%s)\n",
                 on ? "РЕДАКТОРА: инструменты и панели доступны"
                    : "ИГРЫ: инструментов и панелей нет",
                 why != nullptr ? why : "?");
}

// POSSESS THE PLAYER at the free camera. The inverse of apply_restore's eye ->
// feet transform, and it reuses the SAME two offsets (PLAYER_EYE_HEIGHT and
// PLAYER_EYE_FORWARD) rather than a third idea of where the eye sits (Rule 35):
// the eye is above AND forward of the feet, so undoing only the height would
// place the body a fixed step behind where the camera looked. teleport_character
// is a placement, not a walk (the same call apply_restore uses); if the camera
// was high, the body simply falls to the ground under it, which is what "teleport
// the body under the camera" means.
// ============================ THE BUILD HAND ================================
// Everything below serves ONE promise: the colour the builder sees is the
// scene judge's answer about HIS ghost. So the hooks here hand the judge the
// same facts the tool hands it -- ground from the streamed chunks, sizes from
// render::measure_object -- and nothing decides anything on its own.

// BuildJudgeCtx И build_extent ОБЪЯВЛЕНЫ В App.h. Они стояли здесь, в безымянном
// пространстве, ровно пока их читал один файл; wire_editor_panels() уехала в
// AppEditorWiring.cpp и читает ту же мерку, а «та же мерка» — это ОДНА функция,
// а не одинаковый текст в двух местах (правило 39).
const render::ObjectExtent* build_extent(void* ctx, const std::string& name) {
    auto* c = static_cast<BuildJudgeCtx*>(ctx);
    if (const auto it = c->extents->find(name); it != c->extents->end()) {
        return &it->second;
    }
    auto obj = c->objects->find(name);
    if (obj == c->objects->end() && c->shelves != nullptr) {
        for (const std::string& shelf : *c->shelves) {
            if (auto loaded = render::read_object(std::filesystem::path(shelf)
                                                  / (name + ".dfo"))) {
                obj = c->objects->emplace(name, std::move(*loaded)).first;
                break;
            }
        }
    }
    if (obj == c->objects->end()) {
        return nullptr; // not on any shelf: the judge's KnownObject says so
    }
    return &c->extents->emplace(name, render::measure_object(obj->second)).first->second;
}

namespace {

float build_ground_at(void* ctx, glm::vec2 p) {
    auto* c = static_cast<BuildJudgeCtx*>(ctx);
    // THE GROUND THE PLAYER STANDS ON, not the generator's ideal: the map may
    // carry authored pads and river beds, and a ghost judged against the
    // untouched height field would be called "hovering" on every terrace.
    return c->chunks->height_at(p).value_or(0.0f);
}

bool build_object_extent(void* ctx, const std::string& name, float& radius,
                         float& bottom) {
    const render::ObjectExtent* e = build_extent(ctx, name);
    if (e == nullptr) {
        return false;
    }
    radius = e->radius;
    bottom = e->bottom;
    return true;
}

bool build_object_top(void* ctx, const std::string& name, float& top) {
    const render::ObjectExtent* e = build_extent(ctx, name);
    if (e == nullptr) {
        return false;
    }
    top = e->top;
    return true;
}

bool build_object_box(void* ctx, const std::string& name, glm::vec2& lo,
                      glm::vec2& hi) {
    const render::ObjectExtent* e = build_extent(ctx, name);
    if (e == nullptr) {
        return false;
    }
    lo = e->lo;
    hi = e->hi;
    return true;
}

bool build_object_box_solid(void* ctx, const std::string& name, glm::vec2& lo,
                            glm::vec2& hi) {
    const render::ObjectExtent* e = build_extent(ctx, name);
    if (e == nullptr) {
        return false;
    }
    lo = e->slo;
    hi = e->shi;
    return true;
}

bool build_object_solid(void* ctx, const std::string& name) {
    const render::ObjectExtent* e = build_extent(ctx, name);
    // UNKNOWN COUNTS AS SOLID. An object the map does not carry is judged by
    // KnownObject anyway; answering "not an obstacle" here would additionally
    // let it overlap anything, turning one honest finding into a silent pass.
    return e == nullptr || e->solid;
}

} // namespace

const std::string& App::build_selected() const {
    static const std::string none;
    if (build_group_ >= build_groups_.size()) {
        return none;
    }
    const BuildGroup& g = build_groups_[build_group_];
    return build_item_ < g.names.size() ? g.names[build_item_] : none;
}

glm::vec3 App::editor_aim_point() {
    return editor_aim().point;
}

ToolAim App::editor_aim() {
    // WHERE THE EYE MEETS THE GROUND. The renderer's centre pick is NOT the
    // answer: it reports whatever surface the picker sampled — on the frames
    // this was first tried it read 1.2 m while the camera looked across open
    // grass, which put the ghost inside the near plane and made it invisible.
    // A build tool places things ON THE GROUND, so it asks the ground.
    //
    // March, then bisect: the height field is not analytic, and a closed-form
    // intersection would have to assume a plane the terrain is not.
    //
    // ONE AIM FOR FIVE TOOLS, and that is why it is a function rather than the
    // top of update_build_tool(). The brush must bite exactly where the ghost
    // would stand and where the selection picks — three copies of this march
    // would agree today and drift the first time one of them was tuned, and
    // the symptom would be a brush that digs a metre from the crosshair.
    const float yaw = editor_cam_.yaw();
    const float pitch = editor_cam_.pitch();
    const glm::vec3 fwd{std::sin(yaw) * std::cos(pitch), std::sin(pitch),
                        -std::cos(yaw) * std::cos(pitch)};
    const glm::vec3 origin = editor_cam_.position();
    constexpr float MAX_REACH_M = 80.0f; // further than a builder can judge anyway
    constexpr float STEP_M = 0.5f;
    float hit_t = MAX_REACH_M;
    float prev_t = 0.0f;
    float prev_gap = origin.y - chunks_.height_at({origin.x, origin.z}).value_or(origin.y);
    bool found = false;
    BuildJudgeCtx ctx{&chunks_, &scene_objects_, &build_extents_, &gallery_shelves_};
    // WHAT THE RAY MEETS FIRST — ground OR something already standing. Marching
    // terrain alone sends the ghost THROUGH a wall and out the far side, where
    // the builder cannot see it and would swear the tool is broken. A house is
    // as much a surface to build against as the hill it stands on.
    const auto blocked_by_placement = [&](const glm::vec3& at) {
        for (const world::Placement& p : scene_doc_.placements) {
            const render::ObjectExtent* e = build_extent(&ctx, p.object);
            if (e == nullptr || !e->solid) {
                continue;
            }
            if (at.y < p.position.y + e->bottom || at.y > p.position.y + e->top) {
                continue;
            }
            const float dx = at.x - p.position.x;
            const float dz = at.z - p.position.z;
            if (dx * dx + dz * dz <= e->radius * e->radius) {
                return true;
            }
        }
        return false;
    };
    for (float t = STEP_M; t <= MAX_REACH_M; t += STEP_M) {
        const glm::vec3 at = origin + fwd * t;
        if (blocked_by_placement(at)) {
            hit_t = t;
            found = true;
            break;
        }
        const float gap = at.y - chunks_.height_at({at.x, at.z}).value_or(at.y);
        if (gap <= 0.0f && prev_gap > 0.0f) {
            // Crossed the surface between prev_t and t: close in. Eight halvings
            // put the answer inside a millimetre, well under the 25 cm grid the
            // result is snapped to anyway.
            float lo = prev_t;
            float hi = t;
            for (int i = 0; i < 8; ++i) {
                const float mid = 0.5f * (lo + hi);
                const glm::vec3 m = origin + fwd * mid;
                const float g = m.y - chunks_.height_at({m.x, m.z}).value_or(m.y);
                (g <= 0.0f ? hi : lo) = mid;
            }
            hit_t = hi;
            found = true;
            break;
        }
        prev_t = t;
        prev_gap = gap;
    }
    // LOOKING AT THE SKY is not an error, it is a builder turning around. The
    // ghost goes to arm's length and the judge will call it hovering, which is
    // the truth and is visible.
    //
    // AND THE DISTANCE COMES BACK WITH THE POINT, which is what makes the reach
    // ceiling expressible at all (user, 18.08: «я не должен уметь за 1000 км
    // что-то строить»). It used to return a bare vec3, so no consumer could ask
    // how far the march had gone — the ceiling had nowhere to be checked.
    // И ПОСТРОЙКА — ТОЖЕ ПОВЕРХНОСТЬ, В КОТОРУЮ УПИРАЕТСЯ ВЗГЛЯД.
    //
    // Пока её здесь не было, луч проходил СКВОЗЬ стену и останавливался на
    // земле за ней: точка прицела оказывалась в тридцати метрах, проверка
    // дальности отказывала щелчку — и человек трижды писал «не могу выбрать
    // стену», глядя прямо на неё. Дело было не в поиске цели, а в том, что
    // прицел не считал дом препятствием.
    float house_t = 0.0f;
    if (house_.pick_element_ray(origin, fwd, HOUSE_EDGE_GRAB_M, &house_t) != world::NO_ELEMENT
        && house_t > 0.0f && (!found || house_t < hit_t)) {
        hit_t = house_t;
        found = true;
    }
    ToolAim aim;
    aim.origin = origin;
    aim.point = origin + fwd * (found ? hit_t : 8.0f);
    aim.distance_m = found ? hit_t : MAX_REACH_M;
    aim.hit = found;
    aim.pointer_over_ui = editor_ui_.wants_mouse();
    return aim;
}

// ПРИЦЕЛ КАДРА — ОДИН МАРШ ВМЕСТО ЧЕТЫРЁХ.
//
// editor_aim() выше это марш в 160 шагов по высотному полю плюс линейный
// перебор ВСЕХ расстановок карты на каждом шаге, и за кадр его звали четыре
// раза: призрак (update_build_tool), тик инструмента (update_editor_tools),
// строка состояния в оверлее и кольцо кисти в отладочных линиях. Все четыре
// ответа в неизменившемся мире побитово одинаковы.
//
// НЕ «ОДИН РАЗ В НАЧАЛЕ КАДРА», И ЭТО ГЛАВНОЕ ЗДЕСЬ РЕШЕНИЕ. Между вызовами мир
// МЕНЯЕТСЯ, дважды и в известных местах:
//   1. editor_cam_.update() — между призраком и тиком инструмента. Прицел это
//      луч ИЗ камеры, поэтому кэш через движение камеры дал бы кисть, кусающую
//      там, где крестик был кадр назад, — ровно та жалоба, из-за которой у
//      editor_aim() вообще одно тело на пятерых.
//   2. тик инструмента — мазок кисти двигает землю, постановка добавляет
//      расстановку, постройка меняет граф: всё это входы марша.
// Поэтому кэш ЛЕНИВЫЙ и с явным сбросом ровно в этих точках, а не в одной
// точке начала кадра. В спокойном кадре (камера стоит, кнопка не нажата) марш
// один; в рабочем — не больше трёх, и каждый из трёх отвечает про тот мир,
// который в этот момент есть.
ToolAim App::aim_this_frame() {
    if (!frame_aim_valid_) {
        frame_aim_ = editor_aim();
        frame_aim_valid_ = true;
    }
    return frame_aim_;
}

void App::clear_build_ghost() {
    // ДЕТАЛЬ ВЫПАДАЕТ ИЗ РУК ВМЕСТЕ С ИНСТРУМЕНТОМ (заказ 18.08: «после
    // выключения инструмента редактуры последний выбранный объект остаётся в
    // руках и рисуется, а должен выключаться с выключением инструмента»).
    //
    // РАЗБОР. set_ghost_mesh стоял в самом КОНЦЕ update_build_tool, а выходов
    // из неё три ранних: не тот инструмент, режим выбора, пустое имя. На каждом
    // из них загрузка не вызывалась вовсе — и меш, загруженный прошлым кадром,
    // оставался в рендерере и продолжал рисоваться. Снаружи это выглядело как
    // деталь, прилипшая к руке. Вне режима редактора хуже: там функция не
    // зовётся ни разу, поэтому призрак переживал и выход из редактуры.
    //
    // Флаг, а не безусловная загрузка пустого меша каждый кадр: очистка это
    // СОБЫТИЕ, происходящее раз на смену инструмента, и шестьдесят пустых
    // загрузок в секунду были бы платой за одну.
    if (!ghost_uploaded_) {
        return;
    }
    ghost_uploaded_ = false;
    build_ghost_ = {};
    build_verdict_ = {};
    if (renderer_ != nullptr) {
        render_system_.set_ghost_mesh(*renderer_, render::MeshData{});
    }
}

void App::update_build_tool() {
    build_ghost_ = {};
    build_verdict_ = {};
    build_target_ = static_cast<std::size_t>(-1);
    // THE GHOST BELONGS TO THE PLACING MODE, and the SELECTION mode needs the
    // same "what is under the crosshair" answer — so the pass runs for both.
    // Before the modes existed this hung off "is the palette open", which meant
    // the tool the user chose and the tool that was armed were different
    // questions with one answer between them.
    // ЧТО НУЖНО ИНСТРУМЕНТУ — СПРАШИВАЕМ У ИНСТРУМЕНТА. Раньше здесь стояло
    // выражение build_hand_wants_aim(tool), то есть «спроси ярлык и реши за
    // него»; теперь тот, кто рисует призрак, отвечает сам, и добавление шестого
    // инструмента ничего здесь не меняет.
    const ToolAim aim_probe = aim_this_frame();
    const ToolPreview want = editor_ui_.toolbox().preview(aim_probe);
    if ((!want.ghost && !want.target_probe) || gallery_scene_.empty()) {
        clear_build_ghost();
        return;
    }
    BuildJudgeCtx ctx{&chunks_, &scene_objects_, &build_extents_, &gallery_shelves_};
    const glm::vec3 aim = aim_probe.point;

    // WHAT IS UNDER THE CROSSHAIR, for deleting. Nearest placement within its
    // own measured reach, so a small prop wins over the big house it stands
    // in front of instead of the other way round.
    float best = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < scene_doc_.placements.size(); ++i) {
        const world::Placement& p = scene_doc_.placements[i];
        const render::ObjectExtent* e = build_extent(&ctx, p.object);
        if (e == nullptr) {
            continue;
        }
        const glm::vec2 d{aim.x - p.position.x, aim.z - p.position.z};
        const float dist = std::sqrt(d.x * d.x + d.y * d.y);
        if (dist <= std::max(e->radius, 0.35f) && dist < best) {
            best = dist;
            build_target_ = i;
        }
    }

    // NO GHOST UNLESS THE TOOL ASKED FOR ONE. The selecting hand wants the
    // target probe above and nothing else: a green outline standing in front of
    // the thing being picked is a second answer to "what am I about to touch".
    if (!want.ghost) {
        clear_build_ghost();
        return;
    }
    const std::string& name = build_selected();
    if (name.empty()) {
        clear_build_ghost();
        return;
    }
    build_ghost_.object = name;
    build_ghost_.yaw = build_yaw_;
    glm::vec3 at = snap_to_grid(aim);
    // НА ЧТО САДИТСЯ ДЕТАЛЬ: земля ИЛИ верх того, во что упёрся прицел (заказ
    // 18.08: «не могу ставить объекты друг на друга»). Здесь стояло безусловное
    // `at.y = height_at(...)`, то есть рука НИКОГДА не предлагала штабельного
    // положения: наведясь на настил, человек получал деталь на уровне грунта —
    // внутри того, на что целится, — и судья честно отвечал «buried in». Запрет
    // выглядел запретом правил, а был отказом инструмента.
    {
        world::SceneWorld sw;
        sw.ground_at = &build_ground_at;
        sw.object_extent = &build_object_extent;
        sw.object_top = &build_object_top;
        sw.object_box = &build_object_box;
        sw.object_solid = &build_object_solid;
        sw.object_box_solid = &build_object_box_solid;
        sw.ctx = &ctx;
        const float ground = chunks_.height_at({at.x, at.z}).value_or(at.y);
        glm::vec3 probe_at = at;
        probe_at.y = aim.y;
        at.y = place_support_y(scene_doc_, probe_at, ground, sw);
    }
    const render::ObjectExtent* ge = build_extent(&ctx, name);
    if (ge != nullptr) {
        at.y -= ge->bottom;
        // TURN AROUND THE PART'S CENTRE, NOT ITS ORIGIN (user, 17.08:
        // «стрелками я должен крутить их вокруг их центра»). A part's origin
        // is its footing CORNER, so rotating about it swings a three-metre
        // beam out of the crosshair and the builder chases it around the
        // screen. Keeping the centre under the crosshair means the position
        // has to move as the angle changes — which is exactly what "rotate
        // about the centre" means.
        const glm::vec2 c = 0.5f * (ge->lo + ge->hi);
        const float cs = std::cos(build_yaw_);
        const float sn = std::sin(build_yaw_);
        at.x -= cs * c.x + sn * c.y;
        at.z -= -sn * c.x + cs * c.y;
    }
    build_ghost_.position = at;

    // THE JUDGE ITSELF, on a copy of the composition with the ghost appended.
    // Costs one pass per frame and buys the only property worth having: the
    // editor cannot allow what the judge forbids.
    world::SceneDoc probe = scene_doc_;
    world::Placement cand;
    cand.object = name;
    cand.position = build_ghost_.position;
    cand.yaw = build_yaw_;
    // THE GHOST IS JUDGED AS THE PART THAT WILL ACTUALLY BE PLACED, group and
    // all. Judging a lone candidate and then placing a member of a house is two
    // different questions with one colour between them, and the rules that
    // differ are exactly the ones a house is made of: a group member may rest
    // on another member (OnGround steps aside) and must then answer to the
    // joints instead. Green on a ghost that turns red the moment it lands is
    // the fastest way to teach a builder that the colour means nothing.
    cand.group = build_group_name_;
    probe.placements.push_back(cand);
    world::SceneWorld jw;
    jw.ground_at = &build_ground_at;
    jw.object_extent = &build_object_extent;
    jw.object_top = &build_object_top;
    jw.object_box = &build_object_box;
    jw.object_solid = &build_object_solid;
    jw.object_box_solid = &build_object_box_solid;
    jw.ctx = &ctx;
    // СУДИМ ПО РАЗНОСТИ, А НЕ ПО ИНДЕКСУ КАНДИДАТА. Спрашивать «есть ли находка
    // с моим номером» — слепое правило: NoOverlap судья вешает на БОЛЕЕ РАННЮЮ
    // из пары (Scene.cpp, цикл i < j), а призрак всегда дописан в КОНЕЦ, то есть
    // всегда оказывается j и своей находки не получает никогда. Практический
    // итог был именно тот, которого правило и должно не допускать: деталь,
    // поставленная внутрь уже стоящего дерева или дома, светилась ЗЕЛЁНЫМ.
    // Нашла зона кистей замером — два дуба радиуса 2 м в 20 см друг от друга
    // принимались оба.
    //
    // Разность слепых пятен не имеет и не требует списка «какое правило кого
    // винит»: всё, что судья начал говорить в момент добавления кандидата, —
    // вина кандидата, где бы судья это ни повесил. Цена — второй проход судьи.
    const std::vector<world::SceneFinding> before = world::check_scene(scene_doc_, jw);
    const std::vector<world::SceneFinding> after = world::check_scene(probe, jw);
    build_verdict_ = {true, {}};
    if (after.size() > before.size()) {
        // Первая находка, которой раньше не было. Переадресуем её на кандидата,
        // чтобы фраза для человека бралась ТОЙ ЖЕ таблицей (правило 32).
        std::vector<world::SceneFinding> blame;
        blame.push_back(after[before.size() < after.size() ? before.size() : 0]);
        blame.back().placement_index = 0;
        build_verdict_ = verdict_from_findings(blame, 0);
    }

    // THE PREVIEW IS THE PART ITSELF, not a box around it (user, 17.08:
    // «нужно чтобы был предпросмотр объектов, не просто рамка»). A wireframe
    // says where something will stand; it does not say WHAT will stand there,
    // and with two thousand parts on the shelf that is the question.
    render::MeshData ghost;
    if (const auto obj = scene_objects_.find(name); obj != scene_objects_.end()) {
        for (const render::MeshData* m : {&obj->second.wood, &obj->second.bark,
                                          &obj->second.cards, &obj->second.ground}) {
            render::append_transformed(ghost, *m, build_ghost_.position,
                                       build_yaw_, 1.0f);
        }
        // ЦВЕТ ДЕТАЛИ ОСТАЁТСЯ ЕЁ СОБСТВЕННЫМ, а приговор говорят РЁБРА.
        // Заливка целиком в зелёный была моей ошибкой и пользователь назвал её
        // сразу: «объект рисуется зелёным, а я должен объект видеть, рисуйте
        // зелёным рёбра, но не грани». Строитель выбирает из двух тысяч
        // деталей — ему нужно узнать ту, что в руке, а не только услышать
        // «можно». Ответ и предмет — разные вещи, и красить одно другим значит
        // отнимать предмет ради ответа, который и так виден по контуру.
    }
    render_system_.set_ghost_mesh(*renderer_, ghost);
    ghost_uploaded_ = true;
}

void App::rebake_tile_at(glm::vec2 world_xz) {
    const glm::ivec2 key{
        SCENE_TILE_KEY_BASE + static_cast<int>(std::floor(world_xz.x / SCENE_TILE_M)),
        SCENE_TILE_KEY_BASE + static_cast<int>(std::floor(world_xz.y / SCENE_TILE_M))};
    for (SceneTile& st : scene_tiles_) {
        if (st.key == key) {
            // THE TILE'S PARTS ARE RE-READ FROM THE COMPOSITION, not patched.
            // Placing appends, deleting erases, and EDITING MOVES — and a move
            // is what the patched version could not express: the properties
            // column changes a position, and a tile carrying its own copy would
            // keep baking the part where it used to be. One truth about what
            // stands where (Rule 32); the copy exists only to be baked from.
            st.parts.clear();
            for (const world::Placement& p : scene_doc_.placements) {
                if (p.position.x >= st.min_xz.x && p.position.x < st.max_xz.x
                    && p.position.z >= st.min_xz.y && p.position.z < st.max_xz.y) {
                    st.parts.push_back(p);
                }
            }
            bake_scene_tile(st, st.far_form);
            return;
        }
    }
    // NO TILE THERE YET is not an error: the builder may place the first thing
    // in an empty corner of the map. It appears on the next map load; saying
    // so out loud beats a part that silently does not draw.
    std::fprintf(stderr, "[build] плитки в (%.1f, %.1f) ещё нет — деталь появится "
                         "после перезагрузки карты\n",
                 static_cast<double>(world_xz.x), static_cast<double>(world_xz.y));
}

bool App::build_place() {
    if (!build_ghost_.valid() || !build_verdict_.allowed) {
        return false;
    }
    world::Placement p;
    p.object = build_ghost_.object;
    p.position = build_ghost_.position;
    p.yaw = build_ghost_.yaw;
    // WHAT IT IS PART OF, and this line is the difference between "a tool that
    // places props" and "a tool that builds a house". Without a group every
    // hand-placed part is a lone object, and a lone object must stand ON THE
    // GROUND (Scene.cpp: a group is what excuses a member from the earth). A
    // wall panel seated on posts at 0.5 m, a deck at 3.25 m and every rafter
    // above them are then refused by OnGround before a single joint rule ever
    // gets a word — MEASURED, and it is the finding of 17.08: replaying the
    // demo's own log house through this very hand gave 21 refusals out of 46
    // parts with no group against 2 with one (tests/app/HouseScenarioTests.cpp,
    // app_house_scenario, both arms one function with one flag changed). The
    // rules were not missing; the hand was not telling them what it was
    // building.
    p.group = build_group_name_;
    scene_doc_.placements.push_back(p);
    scene_dirty_ = true;
    // The tile re-reads the composition inside rebake_tile_at, so there is no
    // second list to keep in step here.
    rebake_tile_at({p.position.x, p.position.z});
    return true;
}

bool App::build_delete() {
    if (build_target_ >= scene_doc_.placements.size()) {
        return false;
    }
    const world::Placement gone = scene_doc_.placements[build_target_];
    scene_doc_.placements.erase(scene_doc_.placements.begin()
                                + static_cast<std::ptrdiff_t>(build_target_));
    scene_dirty_ = true;
    build_target_ = static_cast<std::size_t>(-1);
    rebake_tile_at({gone.position.x, gone.position.z});
    return true;
}

// ========================= THE OTHER FOUR TOOLS =============================
// The mode is EditorUi's; what a mode DOES is here, and every decision inside
// belongs to a module a test can instantiate. This block is wiring, and it is
// deliberately thin for the reason this file has already paid for twice:
// App.cpp owns a window, so nothing decided here can ever be measured.

bool App::apply_selection_edit() {
    props_.refusal.clear();
    if (selected_ >= scene_doc_.placements.size()) {
        return false;
    }
    // THE POSITION IS MOVED HERE AND THE REST THROUGH edit_placement, because
    // that function's whole value is that it RE-JUDGES and puts the placement
    // back on a refusal. Moving a part by typing a coordinate has to obey the
    // same rules as moving it by pointing at the ground, or the panel becomes
    // the way around the judge.
    const world::Placement before = scene_doc_.placements[selected_];
    scene_doc_.placements[selected_].position = {props_.x, props_.y, props_.z};

    BuildJudgeCtx ctx{&chunks_, &scene_objects_, &build_extents_, &gallery_shelves_};
    world::SceneWorld jw;
    jw.ground_at = &build_ground_at;
    jw.object_extent = &build_object_extent;
    jw.object_top = &build_object_top;
    jw.object_box = &build_object_box;
    jw.object_solid = &build_object_solid;
    jw.object_box_solid = &build_object_box_solid;
    jw.ctx = &ctx;

    PlantParams to;
    to.yaw = props_.yaw_deg * glm::pi<float>() / 180.0f;
    to.scale = props_.scale;
    to.set_yaw = true;
    to.set_scale = true;
    const BuildVerdict v = edit_placement(scene_doc_, selected_, to, jw);
    if (!v.allowed) {
        // PUT IT ALL BACK, including the position edit_placement never saw.
        scene_doc_.placements[selected_] = before;
        props_.refusal = localized(serialization::fnv1a64(v.reason));
        rebake_tile_at({before.position.x, before.position.z});
        return false;
    }
    scene_dirty_ = true;
    // THE SAME ONE TILE, twice: the part left one and arrived in another.
    rebake_tile_at({before.position.x, before.position.z});
    rebake_tile_at({props_.x, props_.z});
    return true;
}

bool App::save_map_with_relief() {
    // ДВА ФАЙЛА, ОДНА КНОПКА. Сцена и сиделка .relief — разные записи об одной
    // карте, и человек, нажавший «сохранить», не обязан знать, что их две.
    // Порядок важен: имя сиделки уходит В СЦЕНУ, поэтому оно проставляется до
    // записи сцены, иначе карта откроется без собственных правок.
    if (!relief_.empty() || !relief_.paths().empty()) {
        if (scene_doc_.relief.empty()) {
            scene_doc_.relief =
                std::filesystem::path(gallery_scene_).stem().string() + ".relief";
        }
    }
    if (!world::write_scene(scene_doc_, gallery_scene_)) {
        return false;
    }
    if (scene_doc_.relief.empty()) {
        return true;
    }
    const std::filesystem::path side =
        std::filesystem::path(gallery_scene_).parent_path() / scene_doc_.relief;
    if (!world::write_relief(relief_, side)) {
        std::fprintf(stderr, "[relief] не записал %s\n", side.string().c_str());
        return false;
    }
    std::fprintf(stderr, "[relief] %s: %zu отсчётов, %zu троп\n",
                 side.string().c_str(), relief_.size(), relief_.paths().size());
    return true;
}

std::size_t App::commit_relief_path(std::size_t index, const world::ReliefPath* path) {
    // ОДНА ДВЕРЬ НА ТРИ ДЕЙСТВИЯ, и потому ровно одно место, которое помнит про
    // перепечку канала и про пометку чанков. Три отдельных крючка означали бы
    // три места, где об этом можно забыть, а забывший даёт тропу, которой нет
    // на земле, — то есть инструмент, который «не работает».
    glm::vec2 lo{0.0f};
    glm::vec2 hi{0.0f};
    bool had_box = false;
    if (index < relief_.paths().size()) {
        // ГДЕ ТРОПА БЫЛА — тоже грязно: сдвинутый узел освобождает землю,
        // которую надо перестроить, иначе на карте остаётся её призрак.
        had_box = world::relief_path_bounds(relief_.paths()[index], lo, hi);
    }

    std::size_t result = static_cast<std::size_t>(-1);
    if (path == nullptr) {
        relief_.erase_path(index);
    } else if (index < relief_.paths().size()) {
        relief_.set_path(index, *path);
        result = index;
    } else {
        result = relief_.add_path(*path);
    }

    glm::vec2 lo2{0.0f};
    glm::vec2 hi2{0.0f};
    if (path != nullptr && world::relief_path_bounds(*path, lo2, hi2)) {
        lo = had_box ? glm::min(lo, lo2) : lo2;
        hi = had_box ? glm::max(hi, hi2) : hi2;
        had_box = true;
    }

    chunks_.set_composed_relief(relief_);
    // КАРТА СТАЛА ГРЯЗНОЙ. Без этого «сохранить» отвечало бы «сохранять
    // нечего» человеку, который только что провёл тропу.
    scene_dirty_ = true;
    if (had_box) {
        (void)chunks_.invalidate_area(lo, hi);
        // ТОТ ЖЕ ФЛАГ, ЧТО У КИСТИ: земля показывается, пока ведёшь, с той же
        // частотой, выведенной из той же измеренной цены.
        ground_moved_since_push_ = true;
    }
    return result;
}

bool App::apply_terrain_dab(const TerrainBrush& brush, glm::vec2 centre, float dt_s) {
    // THE GROUND WITHOUT THE HAND EDITS, for the smoothing brush. The chunks
    // hold the composed ground, so the layer's own delta comes back off it —
    // and during a stroke the chunks have not been rebuilt yet, so this is
    // exact for every sample the stroke has not touched and conservative for
    // the ones it has. Only Smooth reads it; Raise, Lower and Paint never ask.
    struct GroundCtx {
        world::ChunkManager* chunks;
        world::ReliefLayer* relief;
    } gc{&chunks_, &relief_};
    BrushGround ground;
    ground.ctx = &gc;
    ground.base_at = [](void* c, glm::vec2 xz) {
        auto* g = static_cast<GroundCtx*>(c);
        const float composed = g->chunks->height_at(xz).value_or(0.0f);
        return composed - g->relief->height_delta_at(xz);
    };

    const BrushDabReport r = apply_brush(relief_, brush, centre, dt_s, ground);
    if (!r.any) {
        return false;
    }
    last_dab_samples_ = r.samples_touched;
    last_dab_worst_m_ = r.max_abs_delta_m;
    // ЭТОТ ФЛАГ И ЕСТЬ «ПОКАЗЫВАТЬ ЕСТЬ ЧТО». Ставится там, где земля
    // ДЕЙСТВИТЕЛЬНО сдвинулась, а не там, где нажата кнопка.
    ground_moved_since_push_ = true;
    // PAINT WHAT IS NOW TRUE, MARK WHAT STOPPED BEING TRUE, REBUILD LATER.
    // Three calls because there are three decisions (ChunkManager.h says so);
    // folding them would hide the middle one, and rebuilding per dab would pay
    // the cost of a whole stroke for every millimetre of mouse travel.
    chunks_.set_composed_relief(relief_);
    (void)chunks_.invalidate_area(r.min_xz, r.max_xz);
    return true;
}

void App::finish_stroke() {
    // THE STROKE'S BUDGET IS SPENT HERE, once. Draining rather than one chunk
    // per frame because the builder let the button go and is now looking at
    // ground that has not moved yet — a picture that lags the hand by seconds
    // reads as a tool that did nothing, which is the complaint this whole day
    // is about.
    std::size_t left = chunks_.rebuild_dirty(world_, bus_, 4);
    for (int guard = 0; guard < 16 && left > 0; ++guard) {
        left = chunks_.rebuild_dirty(world_, bus_, 4);
    }
    scene_dirty_ = true;
    std::fprintf(stderr, "[кисть] мазок: %d образцов, худший %.3f м; чанков в "
                         "очереди осталось %zu\n",
                 last_dab_samples_, static_cast<double>(last_dab_worst_m_), left);
}

int App::plant_dab_here(const PlantBrush& brush, glm::vec2 centre) {
    // WHAT TO PLANT COMES WITH THE CALL. It used to read a PlantBrush that
    // lived on App and was edited from the BRUSH panel, while this function was
    // called from the BUILD tool's handler — the split the user found with «что
    // за порода выбирается, когда я открываю меню кисти? она ни на что не
    // влияет».
    if (brush.species.empty()) {
        // NOTHING ARMED IS NOT AN ERROR, it is an empty click that says so —
        // and the badge already said it before the click (PlantTool::status).
        std::fprintf(stderr, "[посадка] ни одна порода не выбрана — сажать нечего\n");
        return 0;
    }
    struct GroundCtx {
        world::ChunkManager* chunks;
    } gc{&chunks_};
    BrushGround ground;
    ground.ctx = &gc;
    // THE FINISHED GROUND, hand edits included: a tree planted on a hill the
    // composer just raised has to stand on the hill and not inside it.
    ground.base_at = [](void* c, glm::vec2 xz) {
        return static_cast<GroundCtx*>(c)->chunks->height_at(xz).value_or(0.0f);
    };
    // THE SEED IS THE PLACE AND THE COUNT, so two dabs at one spot differ and
    // one dab is reproducible. A clock would make the tool untestable; a fixed
    // seed would stamp the same five trees at every click.
    const std::uint64_t seed =
        static_cast<std::uint64_t>(std::llround(centre.x * 64.0f)) * 73856093ULL
        ^ static_cast<std::uint64_t>(std::llround(centre.y * 64.0f)) * 19349663ULL
        ^ (scene_doc_.placements.size() * 83492791ULL);
    const std::vector<PlantCandidate> cands =
        plant_candidates(brush, centre, seed, ground);
    if (cands.empty()) {
        return 0;
    }
    BuildJudgeCtx ctx{&chunks_, &scene_objects_, &build_extents_, &gallery_shelves_};
    world::SceneWorld jw;
    jw.ground_at = &build_ground_at;
    jw.object_extent = &build_object_extent;
    jw.object_top = &build_object_top;
    jw.object_box = &build_object_box;
    jw.object_solid = &build_object_solid;
    jw.object_box_solid = &build_object_box_solid;
    jw.ctx = &ctx;
    const PlantDabReport rep = plant_dab(scene_doc_, cands, jw);
    if (rep.planted > 0) {
        scene_dirty_ = true;
        rebake_tile_at(centre);
    }
    // WHY NOTHING APPEARED, when nothing appeared. Silence here is the failure
    // mode that makes a correct tool feel broken.
    std::fprintf(stderr, "[посадка] посажено %d, отказано %d%s%s\n", rep.planted,
                 rep.refused,
                 rep.refused > 0 ? "; первая причина: " : "",
                 rep.refused > 0 ? [&rep] {
                     for (const BuildVerdict& v : rep.verdicts) {
                         if (!v.allowed) {
                             return v.reason.c_str();
                         }
                     }
                     return "";
                 }() : "");
    return rep.planted;
}

void App::update_editor_tools(float dt_s) {
    if (mode_ != AppMode::Editor) {
        return;
    }
    // ОДИН ВЫЗОВ ВМЕСТО SWITCH ИЗ ПЯТИ ВЕТОК. Здесь стояла развилка по ярлыку
    // инструмента, и каждая ветка была наполовину этим файлом, наполовину
    // где-то ещё в run(). Теперь щелчок уходит АКТИВНОМУ инструменту и только
    // ему: ящик выводит нажатие, протяжку и отпускание из одного состояния
    // кнопки, поэтому второго соглашения о фронтах в программе нет.
    const ToolAim aim = aim_this_frame();
    const bool down = input_->is_down(platform::MouseButton::LEFT);
    const ToolTickReport tick = editor_ui_.toolbox().update(
        aim, dt_s, down, editor_ui_.tool_world());
    (void)tick;
    // ИНСТРУМЕНТ ТОЛЬКО ЧТО МОГ ДВИНУТЬ МИР — значит запомненный прицел про него
    // больше не отвечает: мазок опустил землю под крестиком, постановка добавила
    // расстановку, которая для луча препятствие. Сброс по СОБЫТИЮ, а не по факту
    // «мы в редакторе»: без нажатия ничего не менялось и пересчитывать нечего.
    if (tick.pressed || tick.dragged || tick.released) {
        invalidate_frame_aim();
    }
    // ЗЕМЛЯ ДВИГАЕТСЯ, ПОКА ВЕДЁШЬ, А НЕ ОДНИМ СКАЧКОМ НА ОТПУСКАНИИ (заказ
    // 18.08). Мазок и раньше считался покадрово; не считался ПОКАЗ —
    // rebuild_dirty звался только из finish_stroke, поэтому человек вёл кисть
    // по земле, которая стояла на месте до конца штриха.
    //
    // ЧАСТОТА ВЫВЕДЕНА ИЗ ИЗМЕРЕННОЙ ЦЕНЫ, А НЕ НАЗНАЧЕНА: перестройка одного
    // чанка стоит 196 мс (12 прогонов, живое кольцо 3x3), то есть двенадцать
    // кадров при 60 к/с. StrokeRefresh спрашивает у самой перестройки, сколько
    // она заняла, и ставит следующую не раньше чем через REFRESH_COST_RATIO
    // таких цен — на показ уходит около пятой части времени штриха на ЛЮБОЙ
    // машине, а не столько, сколько вышло на моей.
    if (stroke_refresh_.step(ground_moved_since_push_, dt_s)) {
        ground_moved_since_push_ = false;
        const auto t0 = std::chrono::steady_clock::now();
        // ОДИН ЧАНК ЗА ПОКАЗ. Бюджет здесь не тот, что в finish_stroke: там
        // штрих кончился и человек ждёт правду целиком, здесь он ведёт кисть и
        // ждёт ДВИЖЕНИЯ. Чанк под прицелом перестроится первым, потому что он
        // и помечен первым.
        (void)chunks_.rebuild_dirty(world_, bus_, 1);
        const auto t1 = std::chrono::steady_clock::now();
        stroke_refresh_.note_cost(
            std::chrono::duration<float>(t1 - t0).count());
    }
    if (tick.released) {
        stroke_refresh_.end();
    }
    // ДОМ ТЕЛОМ, А НЕ ПРОВОЛОКОЙ — И ТОЛЬКО КОГДА ОН ИЗМЕНИЛСЯ.
    //
    // Проволока отвечает на вопрос «где якоря и куда идут оси» и остаётся: без
    // неё не во что целиться. Тело отвечает на другой вопрос — «что я построил»
    // — и его до сегодня не было вовсе, хотя геометрия считалась (build_house_
    // mesh) и проверялась рукавом.
    //
    // Сравнивается ВЕРСИЯ ГЕОМЕТРИИ, а не сам граф: она растёт в единственной
    // двери ко всем мутациям и ровно для этого заведена. Не revision(): тот
    // растёт только на отмене и отвечает на вопрос про ИМЕНА, а не про форму. Перезаливать тысячи треугольников на
    // каждый кадр значило бы платить за неподвижное.
    // ...и ещё по смене ВЫБОРА: качание петли включено только у выбранной
    // двери, а выбор версию графа не растит (он не правка).
    static world::ElementId last_selected_for_upload = world::NO_ELEMENT;
    if (house_.version() != house_mesh_version_
        || house_.selected_element() != last_selected_for_upload) {
        house_mesh_version_ = house_.version();
        last_selected_for_upload = house_.selected_element();
        upload_house_mesh();
    }
    // КОЛЬЦО КИСТИ И ПРИЗРАК — ЭТО ЛИНИИ, поэтому инструмент, которому они
    // нужны, открывает эту дверь сам. Просить строителя выставить переменную
    // окружения, чтобы увидеть, куда укусит кисть, значит отдать ему
    // неработающий инструмент.
    const ToolPreview want = editor_ui_.toolbox().preview(aim);
    if ((want.ring_brush != nullptr || want.ghost || want.polyline != nullptr
         || want.handles != nullptr || want.accent != nullptr)
        && renderer_ != nullptr) {
        renderer_->set_debug_lines(true);
    }
    // ВЫБРАННАЯ РАССТАНОВКА ОТКРЫВАЕТ СВОИ НАСТРОЙКИ. The properties are the
    // select tool's own settings now, so "show me what I picked" is one call
    // and not a panel id somebody else has to remember.
    // РЕШЕНИЕ «ПОКАЗАТЬ СВОЙСТВА» УЕХАЛО В САМ ИНСТРУМЕНТ ВЫБОРА
    // (ToolWorld::open_own_settings, зовётся из SelectTool::on_press). Здесь оно
    // жило условием, которое не спрашивало, чей сейчас ход, и потому открывало
    // настройки ЛЮБОГО инструмента — попытка посадить дерево распахивала меню
    // посадки. App.cpp окна не тестирует, поэтому и поймать это мог только
    // человек за игрой; в инструменте у того же решения есть прибор.
}

void App::become_player_from_editor() {
    auto* ps = world_.get<gameplay::PlayerState>(player_);
    if (ps == nullptr) {
        mode_ = AppMode::Playing;
        return;
    }
    const float yaw = editor_cam_.yaw();
    const glm::vec3 eye = editor_cam_.position();
    const float eye_h = static_cast<float>(config::PLAYER_EYE_HEIGHT);
    const float fwd = static_cast<float>(config::PLAYER_EYE_FORWARD);
    const glm::vec3 facing{std::sin(yaw), 0.0f, -std::cos(yaw)};
    const glm::vec3 feet{eye.x - facing.x * fwd, eye.y - eye_h, eye.z - facing.z * fwd};
    physics_->teleport_character(ps->character, feet);
    if (auto* tr = world_.get<components::Transform>(player_)) {
        tr->position = feet;
    }
    const float limit = static_cast<float>(config::CAMERA_PITCH_LIMIT);
    ps->yaw = yaw;
    ps->pitch = std::clamp(editor_cam_.pitch(), -limit, limit);
    ps->vertical_velocity = 0.0f; // a possessed player is not mid-fall
    mode_ = AppMode::Playing;
    std::fprintf(stderr,
                 "[editor] possessed player: feet %.2f %.2f %.2f  yaw %.4f\n",
                 static_cast<double>(feet.x), static_cast<double>(feet.y),
                 static_cast<double>(feet.z), static_cast<double>(yaw));
}

// --- КАМЕРА ТРЕТЬЕГО ЛИЦА: ДОЗА И ПРИБОР --------------------------------------
//
// ДОЗА. DFN_CAM_COLLIDE=0 снимает щуп и оставляет всё прочее нетронутым, то
// есть даёт контрольную руку, которая ОБЯЗАНА провалить приёмку (Rule 48:
// критерий, проходящий при нулевой дозе, меряет другую систему). Читается один
// раз и защёлкивается: доза, меняющаяся посреди прогона, делает две половины
// ленты несравнимыми.
// ДОЗА НАСЕСТА. DFN_POSTURE_CAM=0 возвращает стрелу в глаз позы и снимает
// потолок тангажа — то есть даёт РОВНО ту камеру, на которой волна поз нашла
// схлопывание. Читается один раз и защёлкивается: доза, меняющаяся посреди
// прогона, делает две половины ленты несравнимыми.
bool App::posture_cam_enabled() {
    static const bool on = [] {
        const char* v = door_value("DFN_POSTURE_CAM");
        return !(v != nullptr && v[0] == '0');
    }();
    return on;
}

bool App::cam_collide_enabled() const {
    static const bool on = [] {
        const char* v = door_value("DFN_CAM_COLLIDE");
        return !(v != nullptr && v[0] == '0');
    }();
    return on;
}

// ПРИБОР. Мерит НЕ ту величину, которой управляет стрела. Стрела считает длину
// по СФЕРКАСТУ; прибор пускает ЛУЧ от головы к получившейся точке камеры и
// спрашивает, встретил ли он что-нибудь по дороге. Голова заведомо внутри
// оболочки (в ней стоит персонаж), поэтому «луч упёрся раньше камеры» и есть
// определение «камера снаружи». Совпадение двух разных запросов — измерение;
// пересказ сферкаста самим себе был бы арифметикой (Rule 46).
void App::cam_probe_step(const glm::vec3& head, const glm::vec3& cam, float length,
                         const gameplay::CameraBoomAim& aim) {
    if (!cam_probe_ || physics_ == nullptr) {
        return;
    }
    ++cam_probe_frames_;
    float depth = 0.0f;
    bool outside = false;
    if (length > 1e-4f) {
        const platform::RayHit seg =
            physics_->raycast(head, aim.direction, length, physics::LAYER_STATIC);
        if (seg.hit && seg.distance < length) {
            outside = true;
            depth = length - seg.distance;
            ++cam_probe_outside_;
            cam_probe_worst_ = std::max(cam_probe_worst_, depth);
        }
    }
    std::fprintf(stderr,
                 "[cam] f=%llu head %.3f %.3f %.3f  cam %.3f %.3f %.3f  "
                 "want %.3f got %.3f  outside %d  depth %.3f\n",
                 static_cast<unsigned long long>(cam_probe_frames_),
                 static_cast<double>(head.x), static_cast<double>(head.y),
                 static_cast<double>(head.z), static_cast<double>(cam.x),
                 static_cast<double>(cam.y), static_cast<double>(cam.z),
                 static_cast<double>(aim.reach), static_cast<double>(length),
                 outside ? 1 : 0, static_cast<double>(depth));
}

void App::cam_probe_report() const {
    if (!cam_probe_) {
        return;
    }
    std::fprintf(stderr,
                 "[cam] ИТОГ: кадров %llu, камера за оболочкой на %llu, "
                 "худший заход %.3f м, щуп %s\n",
                 static_cast<unsigned long long>(cam_probe_frames_),
                 static_cast<unsigned long long>(cam_probe_outside_),
                 static_cast<double>(cam_probe_worst_),
                 cam_collide_enabled() ? "ВКЛ" : "ВЫКЛ");
}

void App::sync_audio_volumes() {
    // ЧЕРНОВИК СТРАНИЦЫ — ЕДИНСТВЕННЫЙ ИСТОЧНИК ПРАВДЫ О ГРОМКОСТИ, пока
    // страница открыта, и здесь он просто доезжает до шин. Сравнение с config_
    // не оптимизация: set_bus_volume каждый кадр — это ещё и запись громкости
    // поверх любого будущего дакинга, то есть тихая отмена чужого фейда.
    const MenuSettings& s = menu_.settings();
    if (s.music_volume != config_.music_volume) {
        config_.music_volume = s.music_volume;
        audio_->set_bus_volume(music_bus_, config_.music_volume);
    }
    if (s.sfx_volume != config_.sfx_volume) {
        config_.sfx_volume = s.sfx_volume;
        audio_->set_bus_volume(sfx_bus_, config_.sfx_volume);
    }
    if (s.voice_volume != config_.voice_volume) {
        config_.voice_volume = s.voice_volume;
        audio_->set_bus_volume(voice_bus_, config_.voice_volume);
    }
}

bool App::menu_audio_allowed() const {
    // ОДИН ОТВЕТ НА ВСЕ ЗВУКИ МЕНЮ. Дверь НАЗЫВАЕТ ответ, а не сдвигает его:
    // 1 — звучать и в счётном прогоне (так петлю и стык можно послушать
    // прогоном), 0 — молчать всегда. Без двери правило то же, что у кадра
    // студии: звук есть ровно тогда, когда за игрой сидит человек.
    if (const char* m = door_value("DFN_MUSIC"); m != nullptr && *m != '\0') {
        return m[0] == '1';
    }
    return !unattended_run();
}

void App::fade_intro_sting(float seconds) {
    if (!intro_sting_voice_.valid() || intro_sting_fade_len_s_ > 0.0f) {
        return; // нечего гасить, либо уже гасим: два пандуса — это скачок
    }
    intro_sting_fade_begin_ = std::chrono::steady_clock::now();
    intro_sting_fade_len_s_ = seconds > 0.0f ? seconds : 0.0f;
    if (intro_sting_fade_len_s_ <= 0.0f) {
        audio_->stop(intro_sting_voice_);
        intro_sting_voice_ = {};
    }
}

void App::update_intro_sting() {
    if (!intro_sting_.valid()) {
        return; // актива нет — сказано вслух при загрузке
    }
    // СТАРТ — НА ПЕРВОМ КАДРЕ, ГДЕ СТОИТ СТРАНИЦА ЗАСТАВКИ, и это и есть
    // синхронность с видео: тот же кадр рисует нулевой кадр интро, и обе
    // дорожки дальше идут по одним стенным часам. Заводить звук в init()
    // нельзя — между концом init и первым кадром лежит вся заливка окна, и
    // удар на 1.65 с пришёлся бы не на ту картинку.
    if (!intro_sting_started_ && mode_ == AppMode::Menu
        && menu_.page() == MenuPage::Splash && menu_audio_allowed()) {
        intro_sting_started_ = true; // засов: заставка бывает раз за запуск
        platform::PlayParams params;
        // НА МУЗЫКАЛЬНОЙ ШИНЕ, А НЕ НА ШИНЕ ЭФФЕКТОВ. Довод не «это красиво
        // звучит», а проверяемый: хвост росчерка ЗВУЧИТ ОДНОВРЕМЕННО с темой и
        // сведён относительно неё (на децибел тише, кончается той же пустой
        // квинтой). Разведи их по разным шинам — и игрок получит два органа
        // управления одной музыкальной фразой: убрав музыку в ноль, он всё
        // равно поймает полный оркестровый удар, а убрав эффекты, потеряет
        // half того, что задумано как одно целое.
        params.bus = music_bus_;
        params.spatial = false;
        intro_sting_voice_ = audio_->play(intro_sting_, params);
        std::fprintf(stderr,
                     "[музыка] росчерк заставки пошёл, громкость шины %.2f\n",
                     static_cast<double>(config_.music_volume));
    }
    // ЖИВ ЛИ ОН — СПРАШИВАЕТСЯ У БЭКЕНДА, а не считается по своим часам.
    // Доигравший одноразовый голос снимается подметанием, и его ручка
    // становится безопасным no-op'ом (контракт IAudio) — но продолжать по ней
    // пандус значило бы гасить тишину целую секунду каждый кадр.
    if (!intro_sting_voice_.valid() || !audio_->is_playing(intro_sting_voice_)) {
        if (intro_sting_fade_len_s_ <= 0.0f) {
            intro_sting_voice_ = {}; // доиграл сам — это НЕ событие, это конец
        }
    }
    if (!intro_sting_voice_.valid()) {
        return;
    }
    // УХОД В МИР ГАСИТ РОСЧЕРК ВМЕСТЕ С ТЕМОЙ, и той же секундой: они звучат
    // как одно, и разъехавшись на входе в мир прозвучали бы как два.
    if (mode_ != AppMode::Menu) {
        fade_intro_sting(MENU_MUSIC_FADE_OUT_S);
    }
    if (intro_sting_fade_len_s_ <= 0.0f) {
        return; // играет своим чередом: на естественном конце делать НЕЧЕГО
    }
    const float gone = std::chrono::duration<float>(
                           std::chrono::steady_clock::now() - intro_sting_fade_begin_)
                           .count();
    const float left = 1.0f - gone / intro_sting_fade_len_s_;
    if (left <= 0.0f) {
        audio_->stop(intro_sting_voice_);
        intro_sting_voice_ = {};
        intro_sting_fade_len_s_ = 0.0f;
        std::fprintf(stderr, "[музыка] росчерк погашен за %.2f с\n",
                     static_cast<double>(gone));
    } else {
        audio_->set_voice_volume(intro_sting_voice_, left);
    }
}

void App::update_world_audio() {
    // СВОИ СТЕННЫЕ ЧАСЫ, и это не мелочь: часы меню на выходе в мир не идут, а
    // шага симуляции в меню нет — то есть ни одни из двух существующих часов не
    // тикают на ОБЕИХ сторонах перехода, ради которого пандус и заведён.
    // Зажато сверху: свёрнутое окно не должно догонять пандус одним прыжком.
    const auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - audio_tick_prev_).count();
    audio_tick_prev_ = now;
    dt = std::clamp(dt, 0.0f, 0.1f);

    // МИР СЛЫШЕН РОВНО ПОКА ОН ИДЁТ. Пауза — тоже остановленный мир, и это не
    // упущение: игрок, поставивший игру на паузу, остановил её, а ветер,
    // продолжающий дуть над замершим кадром, — это ровно та жалоба, с которой
    // заказ и пришёл, только на две секунды раньше по времени игрока.
    const float target = (mode_ == AppMode::Menu) ? 0.0f : 1.0f;
    const float step = dt / WORLD_AUDIO_FADE_S;
    if (world_gain_ < target) {
        world_gain_ = std::min(target, world_gain_ + step);
    } else if (world_gain_ > target) {
        world_gain_ = std::max(target, world_gain_ - step);
    } else {
        return; // приехали: не тревожим шину каждым кадром
    }
    world_ramp_s_ += dt;
    audio_->set_bus_volume(world_bus_, world_gain_);
    // СКАЗАНО ВСЛУХ, И ТОЛЬКО НА КОНЦАХ ПАНДУСА. «Почему в меню тихо» и «почему
    // в меню шумит» — одна и та же жалоба с двух сторон, и обе проверяются
    // ушами, которых у прогона нет. Две строки за переход делают её проверяемой
    // прогоном; строка на каждый кадр пандуса залила бы лог.
    if (world_gain_ <= 0.0f || world_gain_ >= 1.0f) {
        std::fprintf(stderr, "[звук] мир %s (пандус %.2f с)\n",
                     world_gain_ <= 0.0f ? "замолчал: он не идёт" : "снова звучит",
                     static_cast<double>(world_ramp_s_));
        world_ramp_s_ = 0.0f;
    }
}

void App::update_menu_music() {
    if (!menu_theme_.valid()) {
        return; // темы нет на диске — сказано вслух при загрузке, и хватит
    }
    // ЧЕГО ХОЧЕТ ЭТОТ КАДР — ОДНО ВЫРАЖЕНИЕ, И ЭТО ВСЁ ПОВЕДЕНИЕ ЗАКАЗА.
    //
    // * mode_ == Menu: тема принадлежит МЕНЮ. Вход в мир (Играть, Редактор,
    //   Продолжить) выводит нас отсюда, и музыка уходит сама.
    // * не заставка: кадр студии — это ещё не главное меню, и тема должна
    //   начинаться ПОСЛЕ него, а не под ним.
    // * не над живым миром: пауза — это мир, поставленный на паузу, а не
    //   второй стартовый экран. Настройки, открытые из паузы, — тоже пауза
    //   (menu_.over_world() считает это по цепочке возвратов).
    // * не беспилотный прогон и не запрет двери — menu_audio_allowed() отвечает
    //   на это ОДИН раз за оба звука меню: тема и росчерк заставки молчат в
    //   счётных прогонах по одному и тому же правилу, и две его копии разошлись
    //   бы в первый же день (правило 32).
    const bool want = menu_audio_allowed() && mode_ == AppMode::Menu
                      && menu_.page() != MenuPage::Splash && !menu_.over_world();

    const bool playing = menu_music_.valid();
    if (want == playing) {
        return;
    }
    if (want) {
        // ОДИН СЛОЙ, loop=1, БЕЗ ТОЧЕК ПЕТЛИ. play_music зациклит слой целиком,
        // а стык подогнан МАТЕРИАЛОМ (нарезка без затухания, хвост реверберации
        // завёрнут на начало) — поэтому здесь нечего настраивать, и это ровно
        // то, ради чего петлевую версию просили: код, который не знает, что
        // трек когда-то заканчивался.
        const platform::SoundHandle layers[] = {menu_theme_};
        menu_music_ = audio_->play_music(layers, music_bus_);
        // СКАЗАНО ВСЛУХ, ОБА РАЗА. Музыка — единственная часть кадра, которую
        // нельзя увидеть на снимке: «играет ли она» проверяется ушами, а уши
        // есть не у всех, кто запускает игру. Две строки в логе делают заказ
        // проверяемым прогоном (правило 27 в его звуковом изводе).
        // Какой ИМЕННО файл загрузился, сказано один раз при загрузке — здесь
        // повторять его нельзя: строка врала бы про запасной ход.
        std::fprintf(stderr, "[музыка] тема пошла на репите, громкость шины %.2f\n",
                     static_cast<double>(config_.music_volume));
    } else {
        audio_->stop_music(menu_music_, MENU_MUSIC_FADE_OUT_S);
        std::fprintf(stderr, "[музыка] тема затухает за %.1f с\n",
                     static_cast<double>(MENU_MUSIC_FADE_OUT_S));
        // Ручка ОТПУСКАЕТСЯ СРАЗУ, хотя звук ещё секунду затухает: она значит
        // «меню поёт», а меню уже не поёт. Бэкенд домётывает фейд и подметает
        // голоса сам (music sweep в MiniaudioAudio::update).
        menu_music_ = {};
    }
}

int App::run() {
    auto last = std::chrono::steady_clock::now();
    // Стенные часы заставки, отдельно от часов меню: их РАСХОЖДЕНИЕ и было
    // дефектом, а величину, которую меришь, нельзя мерить ею же самой.
    splash_started_ = last;
    // Часы звуковых пандусов заводятся ЗДЕСЬ, а не остаются в эпохе: первый
    // dt иначе был бы «сколько машина работает», и зажим сверху прятал бы это
    // вместо того, чтобы этого не было.
    audio_tick_prev_ = last;
    // И ШИНА МИРА НАЧИНАЕТ ТАМ, ГДЕ МЫ СТОИМ, а не в единице. Запуск с меню —
    // это мир, которого ещё нет: пандус из единицы в ноль на первых кадрах не
    // слышен (звучать нечему), зато пишет в лог «мир замолчал» про мир,
    // который никто не заводил, и следующий человек идёт искать причину.
    // Запуск сразу в мир (DFN_MENU=0) по той же строке получает ветер с
    // первого кадра, а не через треть секунды нарастания.
    world_gain_ = (mode_ == AppMode::Menu) ? 0.0f : 1.0f;
    audio_->set_bus_volume(world_bus_, world_gain_);
    while (!window_->should_close()) {
        window_->poll_events();
        input_->update();

        // ЗАГЛАВНАЯ ТЕМА — ОДИН ВЫЗОВ НА КАДР, И ОН СТОИТ ЗДЕСЬ, ВЫШЕ РАЗВИЛКИ
        // «меню или мир». Ветка меню его бы не увидела на том кадре, где игрок
        // ушёл в мир (мы уже в другой ветке), и музыка уезжала бы на один кадр
        // позже нажатия — а на выходе из мира не остановилась бы вовсе. Это не
        // экономия строки: развилка и есть то место, где такие вызовы теряются.
        update_menu_music();
        // И РОСЧЕРК ЗАСТАВКИ — ТУТ ЖЕ, СОСЕДНЕЙ СТРОКОЙ. Он не часть темы (у
        // него своя длина, он переживает конец интро и звучит поверх меню), но
        // он ровно так же обязан замолчать на уходе в мир — а уход в мир
        // случается на кадре, который ветка меню уже не увидит.
        update_intro_sting();
        // И ЗВУК МИРА — ТАМ ЖЕ. Три вызова подряд и есть весь ответ на «что
        // сейчас звучит»: мир слышен, пока идёт; меню поёт, пока открыто.
        update_world_audio();

        // MENU MODE: the engine is up, the world may not exist yet. Nothing
        // simulates here -- the menu is drawn over whatever the last frame was
        // (a dimmed world when paused, a plain ground before any world).
        if (mode_ == AppMode::Menu) {
            // THE MENU'S OWN CLOCK. The dust field and the splash fade are
            // functions of it and hold no state of their own, so the same
            // second draws the same frame (Rule 13). Clamped, because a menu
            // left open while the machine swapped must not teleport the motes.
            const auto menu_now = std::chrono::steady_clock::now();
            menu_.tick(std::min(0.1f, std::chrono::duration<float>(menu_now - last).count()));
            // ЧАСЫ МЕНЮ ШЛИ ПОЧТИ ОСТАНОВИВШИСЬ, И ЭТО БЫЛА ПРИЧИНА «ЗАСТАВКА
            // ВИСИТ И ЖДЁТ КЛИКА» (владелец, 27.08). Метка `last` двигалась в
            // САМОМ КОНЦЕ ветки меню — последней строкой перед continue, — а
            // разность бралась в начале СЛЕДУЮЩЕГО кадра. Между этими двумя
            // точками нет ничего: отрисовка кадра лежит МЕЖДУ ними, снаружи
            // измеряемого отрезка. То есть tick() каждый кадр получал не
            // длительность кадра, а длительность перехода к следующей итерации
            // цикла — микросекунды. Часы меню шли примерно в тысячу раз
            // медленнее настоящих, заставка не доживала до своих секунд НИКОГДА,
            // и единственным выходом оставалась клавиша.
            //
            // Метка двигается ЗДЕСЬ, у замера: «сколько прошло от начала
            // прошлого кадра до начала этого» — единственное определение, при
            // котором сумма тиков равна настоящему времени. Выход в мир от
            // этого не получает скачка dt, ради которого метку и сдвигали в
            // конец: кадр меню короткий, и разность на первом игровом кадре —
            // это он и есть.
            last = menu_now;

            // ЗВУК В МЕНЮ ТОЖЕ ЖИВЁТ. Бэкенду нужен свой кадр: им доводятся
            // фейды и подметаются доигравшие голоса, и без этого вызова
            // затухание темы на входе в мир было бы единственным фейдом,
            // который никто не досчитывает. Слушатель — в начале координат:
            // на этом экране мира нет, а музыка не пространственная.
            audio_->update(platform::ListenerPose{
                .position = {0.0f, 0.0f, 0.0f},
                .forward = {0.0f, 0.0f, -1.0f},
                .up = {0.0f, 1.0f, 0.0f}});
            // И ГРОМКОСТЬ — ЖИВЬЁМ, пока ползунок крутится. Единственная
            // настройка, которую нельзя выбрать глазами: страница, отдающая
            // её только на выходе, заставила бы игрока входить и выходить,
            // пока не угадает.
            sync_audio_volumes();

            // THE POINTER, IN CANVAS PIXELS (owner, 26.08: «выбор должен быть
            // доступен как мышкой, так и стрелочками»). The menu is drawn into
            // the HUD canvas at the INTERNAL resolution and stretched over the
            // window, while the mouse arrives in the OS's LOGICAL units -- on a
            // Retina display the framebuffer is twice the content size, so a
            // hit test written against either number alone misses by a factor
            // of two. content_size() exists for exactly this pair.
            const glm::vec2 cursor = input_->mouse_position();
            const glm::uvec2 content = window_->content_size();
            const int hud_w = static_cast<int>(render_system_.hud().width());
            const int hud_h = static_cast<int>(render_system_.hud().height());
            int mx = -1;
            int my = -1;
            if (content.x > 0 && content.y > 0) {
                mx = static_cast<int>(cursor.x / static_cast<float>(content.x)
                                      * static_cast<float>(hud_w));
                my = static_cast<int>(cursor.y / static_cast<float>(content.y)
                                      * static_cast<float>(hud_h));
            }
            const size_t hovered = menu_row_at(hud_w, hud_h, menu_, mx, my);
            // HOVER MOVES THE SELECTION ONLY WHEN THE POINTER MOVED. A hand
            // resting on the mouse while the other drives the arrows would
            // otherwise drag the selection back under the cursor every frame,
            // and the keyboard would look broken.
            const bool pointer_moved = std::abs(cursor.x - menu_cursor_.x) > 0.5f
                                       || std::abs(cursor.y - menu_cursor_.y) > 0.5f;
            if (pointer_moved) {
                menu_cursor_ = cursor;
                menu_.set_selection(hovered);
            }

            // THE STUDIO'S FRAME. It answers to every key and to the mouse,
            // because a title card the player cannot dismiss is the first thing
            // he will hate about the game; and it leaves on its own when its
            // time is up.
            const bool on_splash = menu_.page() == MenuPage::Splash;
            if (on_splash) {
                const bool skip = input_->was_pressed(platform::Key::ENTER)
                                  || input_->was_pressed(platform::Key::ESCAPE)
                                  || input_->was_pressed(platform::Key::SPACE)
                                  || input_->was_pressed(platform::MouseButton::LEFT);
                if (skip) {
                    // ПРОПУСК ГАСИТ РОСЧЕРК, А ТАЙМЕР — НЕТ, и это вся разница
                    // между двумя выходами из заставки. Досмотревший игрок
                    // ДОЛЖЕН услышать хвост поверх открывшегося меню (он для
                    // того и длиннее видео); нажавший «пропустить» просил
                    // тишины, и оркестр, доигрывающий над корневым экраном,
                    // прочитался бы как незасчитанное нажатие.
                    fade_intro_sting(INTRO_STING_SKIP_FADE_S);
                }
                if (skip || menu_.time() >= menu_.splash_seconds()) {
                    // ОДНА СТРОКА, КОТОРОЙ НЕ ХВАТАЛО. Заставка «висела и ждала
                    // клика» месяц, и ни один снимок не мог этого показать: на
                    // КАДРЕ она выглядела правильно, неправильным было ВРЕМЯ
                    // (метка `last` двигалась снаружи измеряемого отрезка, см.
                    // запись выше). Дефект жил в разнице между часами меню и
                    // стенными часами, а разницу видно только числом — поэтому
                    // здесь печатаются ОБА, один раз за запуск. Расхождение
                    // вдесятеро — это тот же дефект, вернувшийся, и его увидит
                    // первый же, кто запустит игру из терминала.
                    const float wall = std::chrono::duration<float>(
                                           menu_now - splash_started_)
                                           .count();
                    std::fprintf(stderr,
                                 "[интро] заставка закончилась: часы меню %.2f с, "
                                 "стенные %.2f с, обещано %.2f с%s\n",
                                 static_cast<double>(menu_.time()),
                                 static_cast<double>(wall),
                                 static_cast<double>(menu_.splash_seconds()),
                                 skip ? " (пропущена игроком)" : "");
                    menu_.open(MenuPage::Root);
                }
            }
            // THE KEY THAT DISMISSED THE SPLASH DOES NOT ALSO PRESS A ROW. The
            // page changed to the root above, and the same Enter edge is still
            // in this frame's snapshot -- without this guard, launching the
            // game and tapping Enter twice would run the first menu item.
            if (!on_splash) {
            if (input_->was_pressed(platform::Key::UP)) {
                menu_.move(-1);
            }
            if (input_->was_pressed(platform::Key::DOWN)) {
                menu_.move(1);
            }
            // Value rows turn sideways. adjust() is a no-op by construction on
            // every page that has no values, so this needs no page test.
            if (input_->was_pressed(platform::Key::LEFT)) {
                menu_.adjust(-1);
            }
            if (input_->was_pressed(platform::Key::RIGHT)) {
                menu_.adjust(1);
            }
            MenuAction action = MenuAction::None;
            if (input_->was_pressed(platform::Key::ENTER)) {
                action = menu_.activate();
            } else if (input_->was_pressed(platform::Key::ESCAPE)) {
                action = menu_.back();
            } else if (input_->was_pressed(platform::MouseButton::LEFT)) {
                // A CLICK IS ENTER ON THE ROW UNDER THE POINTER, and it points
                // first: clicking a row the keyboard had not selected must act
                // on the row that was CLICKED. On a page with no rows (credits,
                // the stub, the splash) the click is still Enter, which is what
                // "click anywhere to go back" means there.
                if (hovered < menu_.item_count()) {
                    menu_.set_selection(hovered);
                    action = menu_.activate();
                } else if (menu_.item_count() == 0 && mx >= 0) {
                    action = menu_.activate();
                }
            }
            switch (action) {
            case MenuAction::OpenMap: {
                // The browser chose a map. Resolve its source and build the
                // world; on a source that cannot open yet (a .dfw with no baker)
                // open_map leaves a status on the browser and we stay in it,
                // rather than jumping into nothing (docs/MAP_LAYOUT.md).
                const MapManifest* m = menu_.chosen_map();
                if (m != nullptr && open_map(*m)) {
                    // BOTH buttons run this browser; the target decides the mode.
                    // И ЭТО ЖЕ РЕШЕНИЕ — ЕДИНСТВЕННОЕ, ГДЕ ДАЁТСЯ ПРАВО СТРОИТЬ
                    // (заказ владельца 27.08). Кнопка меню и есть ответ на «это
                    // игра или редактор»; всё, что дальше, только читает его.
                    if (menu_.browse_target() == BrowseTarget::Editor) {
                        set_editor_session(true, "пункт меню «Редактор»");
                        enter_editor_mode();
                    } else {
                        set_editor_session(false, "вход в мир как в игру");
                        mode_ = AppMode::Playing;
                    }
                    input_->set_cursor_captured(!unattended_run());
                }
                break;
            }
            case MenuAction::Resume:
                // Back to whichever mode paused: pausing the editor and resuming
                // must not silently possess the body.
                mode_ = paused_from_;
                // И ПАНЕЛИ ВОЗВРАЩАЮТСЯ РОВНО ТЕМ, КОМУ ОНИ ПОЛОЖЕНЫ. Ветка
                // меню гасит интерфейс каждым кадром (см. пустой кадр ниже),
                // поэтому возврат в мир обязан назвать право заново — и он
                // называет ЕГО, а не «как было»: иначе пауза стала бы вторым
                // местом, где решается, редактор это или игра.
                editor_ui_.set_visible(editor_session_);
                input_->set_cursor_captured(!unattended_run());
                break;
            case MenuAction::Quit:
                window_->request_close();
                break;
            case MenuAction::CalibrationDone:
                // The page navigates itself (ui's model owns that); this arm
                // only persists the value the player just dialled in.
                config_.black_floor = menu_.black_floor();
                render_system_.environment().black_floor = config_.black_floor;
                write_settings(config_);
                break;
            case MenuAction::SettingsDone: {
                // Живьём применяется ТОЛЬКО то, что живьём применимо:
                // покачивание — это множитель, который шаговый контекст читает
                // каждый кадр. Разрешение, сглаживание и палитра
                // проглатываются рендером при инициализации, поэтому они
                // пишутся в файл и вступают со следующим запуском — страница
                // говорит об этом игроку сама (needs_restart()).
                const MenuSettings& s = menu_.settings();
                config_.internal_width = s.internal_w;
                config_.internal_height = s.internal_h;
                config_.msaa_samples = s.msaa;
                config_.palette_post = s.palette;
                config_.head_bob = s.head_bob;
                step_ctx_.bob_scale = config_.head_bob;
                // ГРОМКОСТЬ УЖЕ ПРИМЕНЕНА — она едет на шины каждым кадром
                // меню (sync_audio_volumes). ЗДЕСЬ ОНА ТОЛЬКО ЗАКРЕПЛЯЕТСЯ В
                // config_, чтобы write_settings ниже записал именно её: без
                // этой строки «выключил музыку» пережило бы страницу и не
                // пережило бы перезапуск, а такое читается как несохранение
                // всех настроек сразу.
                sync_audio_volumes();
                // ОКНО И ПОЛНЫЙ ЭКРАН — ЖИВЬЁМ (заказ владельца 27.08). Обе
                // строки идут через окно платформы и проверяются ПО ФАКТУ:
                // бэкенд имеет право отказать, и настройка, записанная в файл
                // как принятая, когда окно её не приняло, — это файл, который
                // врёт про экран. Отказ громкий, и в config_ уезжает то, что
                // окно действительно показывает.
                if (window_ != nullptr) {
                    if (s.fullscreen != window_->is_fullscreen()) {
                        window_->set_fullscreen(s.fullscreen);
                    }
                    if (!window_->is_fullscreen()
                        && (s.window_w != config_.window_width
                            || s.window_h != config_.window_height)) {
                        window_->set_size(s.window_w, s.window_h);
                    }
                    const glm::uvec2 got = window_->content_size();
                    config_.fullscreen = window_->is_fullscreen();
                    if (!config_.fullscreen && got.x > 0 && got.y > 0) {
                        config_.window_width = got.x;
                        config_.window_height = got.y;
                    }
                    if (config_.fullscreen != s.fullscreen) {
                        std::fprintf(stderr,
                                     "[window] полный экран НЕ переключился: "
                                     "оставляю %s\n",
                                     config_.fullscreen ? "полный экран" : "окно");
                    }
                }
                write_settings(config_);
                break;
            }
            case MenuAction::SaveMap: {
                // WRITES AND STAYS on the page, so a second save is possible
                // and the player sees the answer where he pressed the button.
                if (gallery_scene_.empty()) {
                    menu_.set_browser_status(std::string(localized(serialization::fnv1a64("menu.save.no_scene"))));
                } else if (!scene_dirty_) {
                    menu_.set_browser_status(std::string(localized(serialization::fnv1a64("menu.save.nothing"))));
                } else if (save_map_with_relief()) {
                    scene_dirty_ = false;
                    menu_.set_browser_status(std::string(localized(serialization::fnv1a64("menu.save.done"))));
                } else {
                    menu_.set_browser_status(std::string(localized(serialization::fnv1a64("menu.save.failed"))));
                }
                break;
            }
            case MenuAction::DiscardToRoot:
                // THE ONE IRREVERSIBLE ROW. It does exactly what it says and
                // nothing else: no write, no "are you sure" that would train
                // the player to answer without reading.
                scene_dirty_ = false;
                mode_ = AppMode::Menu;
                input_->set_cursor_captured(false);
                menu_.open(MenuPage::Root);
                break;
            case MenuAction::ToRoot:
                // LEAVING WITHOUT CLOSING THE GAME — the half the user found
                // missing: the pause page's only exit used to be Quit. Unsaved
                // work is kept in memory, not thrown away, because "back to the
                // menu" promises nothing about saving in either direction.
                mode_ = AppMode::Menu;
                input_->set_cursor_captured(false);
                menu_.open(MenuPage::Root);
                break;
            case MenuAction::None:
                break;
            }
            } // !on_splash
            if (window_->should_close()) {
                break;
            }
            // Drawn through the PUBLIC hud layer rather than a new render API:
            // clear() writes alpha 255, so a fully cleared canvas covers the
            // frame exactly like an opaque screen, and the pause page clears
            // transparent to keep the world visible underneath.
            // LIVE PREVIEW: the calibration page draws its squares through the
            // INVERSE curve, on the assumption that the lift is applied to the
            // glass. If the floor does not live in the environment while the
            // player turns the dial, the page shows squares understated by
            // exactly the lift that is missing -- so it lies harder the further
            // the dial is turned, which is the worst possible direction for a
            // control that exists to be believed.
            if (menu_.page() == MenuPage::Calibrate) {
                render_system_.environment().black_floor = menu_.black_floor();
            }
            // ГЛАВНОЕ МЕНЮ ВСЕГДА ЧИСТОЕ (заказ владельца 27.08: «когда я выхожу
            // в главное меню, интерфейс панели редактора остаётся, а должен
            // убираться»).
            //
            // ПОЧЕМУ ОДНОГО set_visible(false) МАЛО, и почему это была не
            // забытая строка, а устройство. Панели рисует НЕ этот цикл:
            // ImGui::Render() складывает списки, а бэкенд выкладывает их внутри
            // IRenderer::end_frame(). Ветка меню уходит на continue ДО пары
            // begin_frame/end_frame — то есть новых списков не появляется, а
            // бэкенд продолжает выкладывать ПОСЛЕДНИЕ. Панели, которые «не
            // убираются», — это буквально последний кадр редактора, застывший
            // под меню. Поэтому здесь делается не «спрятать», а ПУСТОЙ КАДР:
            // ImGui объявляет ноль окон и Render() кладёт пустые списки, после
            // чего выкладывать бэкенду нечего.
            editor_ui_.set_visible(false);
            editor_ui_.begin_frame(*input_, *window_, 0.0f);
            editor_ui_.end_frame();
            // Если в ЭТОМ же кадре меню отдало право строить (кнопка
            // «Редактор» или «Продолжить» в сессии), пустой кадр выше не
            // должен унести видимость с собой: следующий кадр уже не меню,
            // и включить панели больше некому. Видимость = право строить.
            if (mode_ != AppMode::Menu) editor_ui_.set_visible(editor_session_);
            draw_menu(render_system_.hud(), menu_);
            render_system_.set_hud_visible(true);

            // --- ОБЪЁМНЫЙ ГЕРБ ГЛАВНОГО МЕНЮ (заказ владельца 27.08) --------
            // ТОЛЬКО НА КОРНЕ. Пауза остаётся без герба, и это решение по
            // образцу: в Skyrim сигил живёт на лице игры, а экран паузы —
            // это столбец строк поверх мира, куда игрок вернётся. Герб там
            // спорил бы с миром за глубину и за внимание.
            //
            // ОКРУЖЕНИЕ СОХРАНЯЕТСЯ И ВОЗВРАЩАЕТСЯ. Свет экрана меню — не
            // свет мира: игровой кадр каждый раз переписывает солнце из
            // своих часов, а вот интерьер (AppInterior) держит своё
            // окружение МЕЖДУ кадрами, и меню, открытое из комнаты, унесло
            // бы её свет с собой.
            // СВЕТ ЭКРАНА СТАВИТСЯ НА ВСЯКОМ КАДРЕ КОРНЯ, А ГЕРБ — ЗА ДВЕРЬЮ,
            // и это разделение сделано ради ЗАМЕРА. Первая версия гасила
            // свет вместе с гербом, и рука DFN_MENU_OAK=0 меряла тогда не
            // цену меша, а разницу двух РАЗНЫХ кадров: без света экрана
            // солнце окружения остаётся поднятым, бэкенд строит два каскада
            // теней для мира, которого никто не видит, и «контрольный» кадр
            // выходил на 4.4 мс ДОРОЖЕ подопытного. Теперь у обеих рук
            // одинаково всё, кроме одного submit'а.
            //
            // ДВЕРЬ ТРЁХЗНАЧНА, И ТРЕТЬЕ ЗНАЧЕНИЕ — ЭТО ТРЕТЬЯ РУКА ЗАМЕРА:
            //   1 (дефолт) — свет экрана и герб;
            //   0          — свет экрана без герба (цена ОДНОГО меша);
            //   none       — ни того, ни другого, то есть кадр меню такой,
            //                каким он был до этой волны (цена самого света).
            // Все три выходят из ОДНОЙ сборки (правило 47): пара, снятая с
            // двух разных двоичных файлов, доказывает только то, что они
            // разные.
            const bool oak_page = menu_.page() == MenuPage::Root;
            const char* oak_door = door_value("DFN_MENU_OAK");
            const bool oak_screen = oak_page
                                    && (oak_door == nullptr
                                        || std::strcmp(oak_door, "none") != 0);
            const bool oak_on = oak_screen
                                && (oak_door == nullptr || *oak_door != '0');
            platform::RenderEnvironment saved_env{};
            const float saved_fov = camera_.fov_y();
            bool env_saved = false;
            if (oak_screen) {
                saved_env = render_system_.environment();
                env_saved = true;
                // ДЛИННЫЙ ОБЪЕКТИВ НА ОДИН КАДР (см. OAK_MENU_FOV_DEG).
                // Холст меню от этого не двигается ни на пиксель: он вписан
                // в пирамиду по построению (draw_overlay считает половину
                // высоты из того же поля зрения).
                camera_.set_projection(glm::radians(OAK_MENU_FOV_DEG),
                                       camera_.aspect_ratio(),
                                       camera_.near_plane(), camera_.far_plane());
                light_menu_screen(render_system_.environment(), camera_,
                                  menu_lights_);
                render_system_.set_transient_lights(menu_lights_);
                if (oak_on && menu_emblem_.ensure_loaded(*renderer_)) {
                    render_system_.set_screen_prop(
                        menu_emblem_.screen_prop(camera_, menu_.time()));
                }
            }
            const auto render_started = std::chrono::steady_clock::now();
            render_system_.render(world_, *renderer_, camera_, 0.0f);
            if (env_saved) {
                render_system_.environment() = saved_env;
                render_system_.set_transient_lights({});
                camera_.set_projection(saved_fov, camera_.aspect_ratio(),
                                       camera_.near_plane(), camera_.far_plane());
            }
            // ЦЕНА КАДРА МЕНЮ ЧИСЛОМ (DFN_MENU_COST=<кадров>). Две руки —
            // с гербом и с DFN_MENU_OAK=0 — и есть весь замер: одна
            // длительность сама по себе не говорит, сколько стоит герб.
            if (const char* cost = door_value("DFN_MENU_COST");
                cost != nullptr && *cost != '\0') {
                const int want = std::max(1, std::atoi(cost));
                menu_cost_ms_.push_back(
                    std::chrono::duration<float, std::milli>(
                        std::chrono::steady_clock::now() - render_started)
                        .count());
                // Первые кадры выброшены: в них живут заливка меша, компиляция
                // конвейеров и первый снимок холста, то есть не цена кадра.
                if (static_cast<int>(menu_cost_ms_.size()) >= want + 20) {
                    std::vector<float> tail(menu_cost_ms_.begin() + 20,
                                            menu_cost_ms_.end());
                    double sum = 0.0;
                    for (const float v : tail) {
                        sum += static_cast<double>(v);
                    }
                    std::sort(tail.begin(), tail.end());
                    std::fprintf(stderr,
                                 "[меню] цена кадра: %zu кадров, среднее %.3f мс, "
                                 "медиана %.3f мс, минимум %.3f мс (герб %s, "
                                 "%u треугольников)\n",
                                 tail.size(), sum / static_cast<double>(tail.size()),
                                 static_cast<double>(tail[tail.size() / 2]),
                                 static_cast<double>(tail.front()),
                                 oak_on ? "есть" : "снят",
                                 menu_emblem_.triangles());
                    window_->request_close();
                }
            }
            // VERIFICATION HOOK (Rule 27): a menu nobody can photograph is a
            // menu nobody can verify. DFN_MENU_SHOT=<path> captures one frame
            // of whichever page is showing and closes.
            if (const char* shot = door_value("DFN_MENU_SHOT");
                shot != nullptr && *shot != '\0') {
                // The backend captures AFTER the current end_frame and needs a
                // few frames to flush (the tour learned this the hard way), so
                // shoot once and keep drawing until the flush lands.
                if (menu_shot_frames_ == 0) {
                    (void)renderer_->save_screenshot(shot);
                }
                if (++menu_shot_frames_ > 4) {
                    window_->request_close();
                }
            }
            // МЕТКА ВРЕМЕНИ ДВИНУТА В НАЧАЛО ВЕТКИ, к самому замеру (см. запись
            // там же): здесь она стояла ПОСЛЕ отрисовки и потому исключала её из
            // измеряемого отрезка — часы меню шли микросекундами в кадр.
            continue;
        }
        if (window_->consume_resize()) {
            const auto fb = window_->framebuffer_size();
            renderer_->resize(fb.x, fb.y);
            camera_.set_projection(camera_.fov_y(),
                                   static_cast<float>(fb.x) / static_cast<float>(fb.y),
                                   camera_.near_plane(), camera_.far_plane());
        }
        // ВЕСЬ КЛАВИАТУРНЫЙ ВВОД КАДРА — ОДНОЙ СТРОКОЙ (слой 1 разбора,
        // docs/audits/PLAN_APP_DECOMPOSITION.md). Здесь лежало 350 строк: чат, ESC,
        // полный экран, Tab, третье лицо, вывод, снимок, каркас, пять
        // инструментов, курсор, меню объектов, поворот детали, снимок экрана,
        // замечание, запись и повтор траектории, карта — и перед каждым от руки
        // написанное `!chat_typing &&`. Обработчики уехали в AppInput.cpp, а
        // сама привязка «действие → метод» стала таблицей в AppActions.cpp,
        // которую читает рукав app_controls. Этот файл держит окно и потому не
        // проверяется ничем; таблица рядом — проверяется.
        const bool chat_typing = chat_overlay_.is_open();
        if (!dispatch_actions(chat_typing)) {
            continue; // ESC увёл в меню паузы: этого кадра больше нет
        }
        // ГРАНИЦА КАДРА ДЛЯ ПРИЦЕЛА. Ставится ПОСЛЕ прогулки по клавишам, потому
        // что обработчик может и сам поставить деталь (Enter в редакторе), — то
        // есть кадр начинается для прицела здесь, а не на poll_events().
        invalidate_frame_aim();
        // THE DOOR TAKES THE SAME PATH AS THE KEY (DFN_THIRD_PERSON=1), fired
        // once on the first playing frame. Third person could only ever be
        // reached by a human pressing 1, so no automated run could photograph
        // it — which is exactly how "в третьем лице вообще тела нет" survived
        // until a human looked. A door that reproduced the toggle in its own
        // code would be a second definition of third person and would drift;
        // this one calls the very method the key calls (Rule 32).
        if (!third_person_door_fired_ && mode_ == AppMode::Playing) {
            third_person_door_fired_ = true;
            if (const char* d = door_value("DFN_THIRD_PERSON");
                d != nullptr && *d != '\0' && *d != '0') {
                std::fprintf(stderr, "[editor] DFN_THIRD_PERSON: третье лицо\n");
                on_third_person();
            }
        }
        if (mode_ == AppMode::Editor) {
            EditorToolbox& box = editor_ui_.toolbox();
            // DOOR: DFN_EDITOR_TOOL=1..5 picks a tool without a keypress, so a
            // feature that exists only on screen can be photographed by an
            // unattended run (Rule 27). IT GOES THROUGH THE SAME click_icon the
            // hand does — a door that set the pointer itself would photograph a
            // path no user takes.
            static const int tool_door = [] {
                const char* v = door_value("DFN_EDITOR_TOOL");
                return v != nullptr ? std::atoi(v) : 0;
            }();
            static bool tool_door_used = false;
            if (!tool_door_used && tool_door != 0 && box.count() > 0) {
                tool_door_used = true;
                if (tool_door >= 1 && tool_door <= static_cast<int>(box.count())) {
                    on_tool_pick(tool_door - 1);
                    std::fprintf(stderr, "[editor] дверь DFN_EDITOR_TOOL=%d\n", tool_door);
                } else {
                    // A DOOR THAT SILENTLY DOES NOTHING is worse than no door:
                    // the run photographs an empty hand and the frame looks
                    // like the feature is missing.
                    std::fprintf(stderr, "[editor] DFN_EDITOR_TOOL=%d вне 1..%zu — "
                                         "инструмент НЕ выбран\n",
                                 tool_door, box.count());
                }
            }
            // DOOR: DFN_EDITOR_SETTINGS=1..5 opens THAT tool's settings without
            // touching the hand — the very property the user asked for, and one
            // no screenshot can show on its own.
            static const int settings_door = [] {
                const char* v = door_value("DFN_EDITOR_SETTINGS");
                return v != nullptr ? std::atoi(v) : 0;
            }();
            static bool settings_door_used = false;
            if (!settings_door_used && settings_door >= 1
                && settings_door <= static_cast<int>(box.count())) {
                settings_door_used = true;
                box.click_settings(static_cast<std::size_t>(settings_door - 1));
                std::fprintf(stderr, "[editor] дверь DFN_EDITOR_SETTINGS=%d: настройки "
                                     "открыты, в руке по-прежнему %s\n",
                             settings_door,
                             box.active() != nullptr ? box.active()->identity().id
                                                     : "ничего");
            }

            // ДВЕРЬ: DFN_HOUSE_PULL=<метры> — подтягивание шарика якоря к
            // себе без колеса мыши. Отвес рисуется ТОЛЬКО когда шарик поднят
            // над травой, а колесо беспилотному прогону недоступно: без этой
            // двери приёмочного кадра с отвесом не существует (правило 27).
            static const float pull_door = [] {
                const char* v = door_value("DFN_HOUSE_PULL");
                return v != nullptr ? static_cast<float>(std::atof(v)) : 0.0f;
            }();
            static bool pull_door_used = false;
            // ДВЕРЬ: DFN_HOUSE_DEMO=1 — маленький сруб в графе, без единого
            // щелчка. Тело постройки иначе не попадает ни на один беспилотный
            // кадр: пустой граф рисует пустоту, а построить дом мышью прогон не
            // умеет (правило 27).
            // ДВЕРЬ: DFN_HOUSE_GRID=<шаг> — сетка на беспилотном прогоне.
            // Без неё ни один кадр не может показать ни отсечек, ни того, как
            // якорь садится в узел.
            static const float grid_door = [] {
                const char* v = door_value("DFN_HOUSE_GRID");
                return v != nullptr ? static_cast<float>(std::atof(v)) : 0.0f;
            }();
            static bool grid_door_used = false;
            if (grid_door > 0.0f && !grid_door_used) {
                grid_door_used = true;
                house_.set_grid_step_m(grid_door);
                house_.set_grid_on(true);
                std::fprintf(stderr, "[постройка] дверь DFN_HOUSE_GRID: шаг %.2f м\n",
                             static_cast<double>(house_.grid_step_m()));
            }
            static const bool demo_door = door_value("DFN_HOUSE_DEMO") != nullptr;
            static bool demo_door_used = false;
            if (demo_door && !demo_door_used) {
                demo_door_used = true;
                seed_demo_house();
            }
            // ЖДЁМ, ПОКА ЯЩИК НАПОЛНЕН. Первый кадр редактора приходит раньше
            // инструментов, и дверь, потратившая себя на пустой ящик, молча не
            // сделала бы ничего — тот самый худший исход, о котором говорит
            // соседняя дверь.
            if (!pull_door_used && pull_door > 0.0f && box.count() > 0) {
                if (const std::size_t i = box.index_of("house.vertex"); i != NO_TOOL) {
                    pull_door_used = true;
                    if (auto* vt = dynamic_cast<HouseVertexTool*>(box.at(i))) {
                        vt->set_pull_m(pull_door);
                        std::fprintf(stderr, "[editor] дверь DFN_HOUSE_PULL=%.2f м\n",
                                     static_cast<double>(vt->pull_m()));
                    }
                }
            }
        }
        // ДВЕРЬ: DFN_EDITOR_PARTS=1 / DFN_EDITOR_BRUSH=1 — ОДНО нажатие на
        // беспилотном прогоне. Обе открывают НАСТРОЙКИ соответствующего
        // инструмента (список объектов у постройки, кисть у высоты): дверь
        // подаёт ровно то, что подаёт рука, — тот же click_settings, что и
        // треугольник, и тот же on_build_menu(), что и клавиша B.
        static const bool brush_door = door_value("DFN_EDITOR_BRUSH") != nullptr;
        static bool brush_door_used = false;
        if (brush_door && !brush_door_used && mode_ == AppMode::Editor) {
            brush_door_used = true;
            wire_editor_panels();
            if (const std::size_t i = editor_ui_.toolbox().index_of("height"); i != NO_TOOL) {
                editor_ui_.toolbox().click_settings(i);
            }
            std::fprintf(stderr, "[editor] дверь DFN_EDITOR_BRUSH: настройки кисти\n");
        }
        static const bool parts_door = door_value("DFN_EDITOR_PARTS") != nullptr;
        static bool parts_door_used = false;
        if (parts_door && !parts_door_used && mode_ == AppMode::Editor) {
            parts_door_used = true;
            std::fprintf(stderr, "[editor] дверь DFN_EDITOR_PARTS: нажатие «меню объектов»\n");
            on_build_menu();
        }
        // The ghost is recomputed every frame from THIS tick's aim: a ghost
        // remembered across a frame would lag the crosshair, and a lagging
        // ghost placed on click puts the part where the builder was looking a
        // moment ago.
        if (mode_ == AppMode::Editor) {
            // DOOR: DFN_BUILD=1 opens the palette without a keypress, so the
            // ghost can be photographed by an unattended run. Same binary, same
            // world — the frame shows what a builder sees, not a mock-up.
            static const bool build_door = [] {
                const char* v = door_value("DFN_BUILD");
                return v != nullptr && *v != '\0' && *v != '0';
            }();
            static bool build_door_used = false;
            // The tools exist from the first editor frame, so their buttons are
            // on the bar before anybody presses anything.
            wire_editor_panels();
            if (build_door && !build_door_used) {
                build_door_used = true;
                renderer_->set_debug_lines(true);
                if (const std::size_t i = editor_ui_.toolbox().index_of("place");
                    i != NO_TOOL) {
                    // ТА ЖЕ РУКА: дверь берёт инструмент постройки тем же
                    // click_icon, каким его берёт человек.
                    editor_ui_.toolbox().click_icon(i, editor_ui_.tool_world());
                }
                if (build_groups_.empty()) {
                    build_groups_ = build_palette(gallery_objects_dir_);
                }
            }
            update_build_tool();
        } else {
            // ВЫШЕЛ ИЗ РЕДАКТУРЫ — РУКА ПУСТА. Здесь update_build_tool не
            // зовётся вовсе, поэтому без этой строки деталь оставалась висеть
            // в мире и в игровом режиме.
            clear_build_ghost();
        }
        // TOOLING DOOR for the same capture (DFN_CAPTURE_AFTER=<seconds>):
        // fires one capture and closes. This is how the capture path itself is
        // verified -- an F2 that only a human can press is a feature nobody can
        // prove works, and the restore it feeds would be untested by
        // construction (Rule 27).
        if (capture_after_s_ > 0.0) {
            capture_after_elapsed_ += std::chrono::duration<double>(
                                          std::chrono::steady_clock::now() - last)
                                          .count();
            if (capture_after_elapsed_ >= capture_after_s_) {
                capture_pending_ = true;
                capture_after_s_ = 0.0;
                capture_then_close_ = true;
            }
        }
        // THE DOSE DOOR FOR KEY 5 (DFN_SHOT_AFTER=<frames>), and it exists for
        // the same reason DFN_CAPTURE_AFTER does: a feature only a human hand
        // can reach is a feature nobody can prove works. Counted in FRAMES
        // rather than seconds so two runs of one recipe are comparable bit for
        // bit -- a wall second fits a different number of frames on a loaded
        // machine, which is the defect DFN_CAPTURE_AFTER_FRAMES was added for.
        // Fires once, then closes the app, so the run's artifacts are complete
        // when it exits: the .png, the chat line and the flushed trace.
        // ВЫХОД ИЗ ЛОКАЦИИ ПО СЧЁТУ КАДРОВ (И15). Кадры, а не секунды, по той
        // же причине, что у соседей: два прогона одного рецепта обязаны выйти
        // на одном и том же кадре независимо от загрузки машины.
        //
        // И ДВЕРЬ ЖМЁТ КЛАВИШУ, А НЕ ЗОВЁТ ВЫХОД (починка 27.08). До сегодня
        // она звала leave_interior() напрямую — и потому мерила ПОЛОВИНУ
        // выхода: прицел, подсказку и нажатие она не проходила вовсе. Живая
        // жалоба владельца («двери на вход работают, на выход — нет») жила
        // ровно в пропущенной половине, и зелёный беспилотный замер
        // «выход 0.01 мс» стоял рядом с ней, ничего не зная. Теперь дверь
        // взводит тот же interact_pressed, что и клавиша E, и если под
        // прицелом ничего нет — она НЕ выходит и говорит об этом вслух:
        // молчаливое падение назад на прямой вызов вернуло бы ровно ту
        // слепоту, из-за которой дефект дожил до игрока.
        if (interior_exit_frames_ > 0) {
            ++interior_exit_seen_;
            if (interior_exit_seen_ >= interior_exit_frames_) {
                interior_exit_frames_ = 0;
                const bool aimed =
                    world_.has_resource<components::HoverTarget>()
                    && world_.alive(
                           world_.resource<components::HoverTarget>().entity);
                if (auto* ps = world_.get<gameplay::PlayerState>(player_)) {
                    std::fprintf(stderr,
                                 "[интерьер] дверь DFN_INTERIOR_EXIT: жму E; под "
                                 "прицелом %s\n",
                                 aimed ? "ЕСТЬ цель"
                                       : "ПУСТО — выхода не будет");
                    ps->interact_pressed = true;
                } else {
                    std::fprintf(stderr,
                                 "[интерьер] дверь DFN_INTERIOR_EXIT: игрока нет\n");
                }
            }
        }
        // РУКА НА КЛАВИШЕ ПЕРЕД ЛАВКОЙ (DFN_SEAT_TAKE). Тот же приём и тот же
        // довод, что у соседки строкой выше: она ЖМЁТ E, а не зовёт take_seat,
        // — иначе прицел, подсказка и нажатие остались бы непроверенными, а
        // именно там 27.08 и жил дефект, который беспилотный замер не увидел.
        drive_seat_take();
        if (shot_after_frames_ > 0) {
            ++shot_after_frames_seen_;
            if (shot_after_frames_seen_ >= shot_after_frames_) {
                chat_pending_entry_ = ChatEntry{};
                chat_pending_entry_.who = "human";
                chat_pending_ = true;
                shot_after_frames_ = 0;
                chat_then_close_ = true; // the existing flag: the shot IS a chat entry
            }
        }
        // ...and the same door counted in frames, which IS comparable bit for
        // bit. Counted here rather than in the render block so it advances once
        // per loop iteration, exactly like the frame the log names.
        //
        // И ДОЗА НАЧИНАЕТ СЧЁТ НЕ ОТ ЗАПУСКА, А ОТ ГОТОВНОСТИ МИРА. «Снять
        // через 120 кадров» отвечает на вопрос «сколько ждать», ЧИСЛОМ, которое
        // кто-то однажды угадал; настоящий вопрос — «всё ли приехало». Пока
        // очередь чанков не пуста, крупная сетка строится, дальняя земля не
        // доехала или капсула ещё оседает, эта доза НЕ ТИКАЕТ, а часы мира
        // стоят (ниже). Тогда кадр съёмки — чистая функция рецепта: доза
        // кадров после готовности, и ни одного кадра загрузки внутри.
        //
        // Заодно это снимает вопрос «а 120 кадров хватит?»: не хватит — доза
        // просто начнётся позже, а потолок затвора скажет вслух, если мир не
        // сошёлся вовсе. Дверь контроля DFN_UNPIN=gate возвращает счёт от
        // запуска.
        if (capture_after_frames_ > 0) {
            const bool wait_for_world = counted_run() && !unpinned("gate")
                                        && !world_settled_;
            if (!wait_for_world) {
                ++capture_after_frames_seen_;
                if (capture_after_frames_seen_ >= capture_after_frames_) {
                    capture_pending_ = true;
                    capture_after_frames_ = 0;
                    capture_then_close_ = true;
                }
            }
        }

        const auto now = std::chrono::steady_clock::now();
        const double frame_dt = std::chrono::duration<double>(now - last).count();
        last = now;
        frame_clock_.push(static_cast<float>(frame_dt));
        // ДЕЛЬТА ДЛЯ ВСЕГО, ЧТО ПЛАВНО ЕДЕТ В КАДРЕ, — ОДНА, И В БЕСПИЛОТНОМ
        // ПРОГОНЕ ОНА СЧЁТНАЯ. Растворение дальней земли, рой светляков и
        // сглаживание стрелы камеры — фильтры по ВРЕМЕНИ, и на стенной дельте
        // каждый из них ловится в разной фазе двумя прогонами одного рецепта.
        // Раньше счётную дельту получал ОДИН тур («tour_.active() ? SIM_DT :
        // frame_dt» в трёх местах); дверь дозы, которой снимают города, не
        // получала ничего — это тот же промах «проверка написана как „это
        // тур?“ вместо свойства», что и у часов суток 13.08.
        // Дверь контроля: DFN_UNPIN=fade.
        const double visual_dt = (counted_run() && !unpinned("fade"))
                                     ? static_cast<double>(config::SIM_DT)
                                     : frame_dt;
        // Cleared before the tick that may set it again (the chunk ferry does).
        world_changed_this_frame_ = false;

        // FREE CAMERA of the editor. Advanced from live input every render
        // frame. The sim still ticks below -- streaming, sky and the body keep
        // living so the flown world is the real one -- but the player's input
        // is withheld (the !editor guard on the look block) and the frame is
        // drawn from this pose instead of the player's CameraPose.
        const bool editor = mode_ == AppMode::Editor;
        if (editor && !chat_typing) { // typing must not fly the free camera
            // КАМЕРА ЗАМИРАЕТ, ПОКА УКАЗАТЕЛЬ НАД ПАНЕЛЬЮ. Пользователь просил
            // ровно это: «когда я открываю меню, я стою на месте, а мышкой
            // кликаю по меню». Без такой проверки движение мыши по списку
            // одновременно крутило бы мир, и выбрать деталь было бы нельзя.
            // wants_keyboard отдельно: пока каретка в поиске, W/A/S/D — это
            // ТЕКСТ, иначе набрать «wall» значит уехать вперёд и влево.
            // ЗАМОРАЖИВАЕМ ТОЛЬКО ПРИ ОТКРЫТОЙ ПАНЕЛИ, и это не осторожность, а
            // разбор настоящего отказа: признак «интерфейс забрал мышь» сам по
            // себе оказался истинным и БЕЗ единой открытой панели, поэтому
            // камера замирала навсегда, а курсор пропадал — редактор переставал
            // управляться совсем. Открытая панель — условие, которое видно
            // глазами и которое не может залипнуть незаметно.
            // РЕШАЕТ ЧЕЛОВЕК, А НЕ ДОГАДКА. Раньше курсор освобождался сам,
            // когда указатель оказывался над панелью, — и вместе с курсором у
            // камеры пропадала мышь: в полёте взгляд застревал в одну сторону.
            // Пользователь назвал это точнее меня: «то за камеру держится, то в
            // UI, и непонятно». Теперь состояние переключается клавишей R и не
            // зависит от того, открыто что-нибудь или нет.
            const bool pointer_mode = editor_ui_.toolbox().pointer_mode();
            if (pointer_mode) {
                // Курсор отдаётся интерфейсу, иначе он невидим и заперт в
                // центре окна — кликать было бы нечем.
                input_->set_cursor_captured(false);
                // Камера НЕ обновляется вовсе: и поворот, и полёт читают один
                // и тот же ввод, который сейчас принадлежит интерфейсу.
            } else if (editor_camera_takes_mouse(editor, chat_typing, pointer_mode)) {
                if (!unattended_run()) {
                    input_->set_cursor_captured(true);
                }
                // КОЛЕСО МЕНЯЕТ ДАЛЬНОСТЬ ВЗАИМОДЕЙСТВИЯ (решение 18.08:
                // «приближение и отдаление сделаем на скролл мыши, тот слайдер
                // в общих настройках оставим, но управление допом на скроле
                // будет»). Ползунок под шестерёнкой остаётся и показывает то же
                // число — это ОДНО состояние с двумя органами управления, а не
                // два состояния: колесо зовёт тот же set_reach_ceiling_m, что и
                // ползунок, и зажим пределов живёт внутри него (правило 32).
                //
                // Шаг геометрический, как у скорости полёта: от 2 до 80 м
                // одинаковое число щелчков в любой части диапазона.
                //
                // ЕСЛИ ИНСТРУМЕНТ ПРОСИТ КОЛЕСО — ОНО ЕГО. У якоря колесо
                // подтягивает шарик к себе вдоль луча, и другого способа
                // поставить вершину в воздух у руки нет («не могу приближать
                // сферу к себе или отдалять», 18.08). Условие, а не вторая
                // клавиша: орган один, а кто им распоряжается — решает то, что
                // сейчас в руке.
                if (const float wheel = input_->scroll_delta().y; wheel != 0.0f) {
                    auto& box = editor_ui_.toolbox();
                    if (IEditorTool* tool = box.active();
                        tool != nullptr && tool->wants_wheel()) {
                        tool->on_wheel(wheel);
                    } else {
                        box.set_reach_ceiling_m(box.reach_ceiling_m()
                                                * std::pow(1.15f, wheel));
                    }
                }
                const float yaw_before = editor_cam_.yaw();
                const glm::vec3 pos_before = editor_cam_.position();
                editor_cam_.update(*input_, static_cast<float>(frame_dt));
                // КАМЕРА ТРОНУЛАСЬ — ЗНАЧИТ РУКА ВЕРНУЛАСЬ К ПРИЦЕЛУ. Спрашивается
                // РЕЗУЛЬТАТ (сдвинулась ли камера), а не список клавиш: список
                // пришлось бы дописывать при каждой новой привязке и однажды не
                // дописать. Порог — не ноль: мышь отдаёт микродрожь даже
                // неподвижная, и от неё метка гасла бы сама собой.
                const bool cam_moved =
                    glm::length(editor_cam_.position() - pos_before) > 1e-4f
                    || std::fabs(editor_cam_.yaw() - yaw_before) > 1e-4f;
                if (house_.nudging() && cam_moved) {
                    house_.set_nudging(false);
                }
                // ПРИЦЕЛ — ЛУЧ ИЗ КАМЕРЫ, и камера только что тронулась. Тот же
                // порог и то же измерение, что у метки подталкивания выше: два
                // ответа на вопрос «сдвинулась ли она» разъехались бы, а здесь
                // он один.
                if (cam_moved) {
                    invalidate_frame_aim();
                }
                // ПРИБОР ВМЕСТО ГЛАЗ. Три захода подряд «камера не крутится»
                // разбирал человек за игрой, а не измерение: у нас не было ни
                // одной проверки на то, что смещение мыши ДОХОДИТ до камеры.
                // Дверь печатает сырую пару чисел — что пришло на вход и что
                // изменилось на выходе, — поэтому «мышь молчит» и «камера
                // глуха» перестают выглядеть одинаково.
                if (cam_trace_) {
                    const glm::vec2 md = input_->mouse_delta();
                    std::fprintf(stderr,
                                 "[cam] free=%d captured=%d md=(%+.2f,%+.2f) "
                                 "yaw %.4f -> %.4f\n",
                                 pointer_mode ? 1 : 0,
                                 input_->is_cursor_captured() ? 1 : 0,
                                 static_cast<double>(md.x), static_cast<double>(md.y),
                                 static_cast<double>(yaw_before),
                                 static_cast<double>(editor_cam_.yaw()));
                }
            }
        }
        // WHAT THE CHOSEN TOOL DOES, and it runs here rather than beside the
        // key handlers because it needs THIS frame's dt: a brush measured per
        // dab digs twice as fast on a machine running twice the frame rate,
        // and the sculptor blames his own hand.
        if (editor && !chat_typing) {
            update_editor_tools(static_cast<float>(frame_dt));
        }

        // TRAJECTORY REPLAY (O3): consume one recorded frame per PRESENTED
        // frame. The eye pose and the counted clock come from the FILE, so two
        // playbacks render bit-for-bit (Rule 53). Set here, before the clock and
        // the streaming focus below read it; the camera is overridden from it in
        // the pose block. When the file is spent the replay ends (and the
        // DFN_TRAJ_PLAY door closes).
        replaying_ = false;
        if (traj_play_ && traj_play_->active()) {
            if (const TrajectoryFrame* f = traj_play_->advance()) {
                replay_frame_ = *f;
                replaying_ = true;
            }
        }
        if (traj_play_ && !traj_play_->active()) {
            if (traj_play_then_close_ && !flush_countdown_.armed()) {
                flush_countdown_.arm(); // let the last frame's capture flush
            }
            traj_play_.reset();
        }

        // In-game clock (в67): DAY_LENGTH_SECONDS per day, with a debug key that
        // runs it DEBUG_TIME_SCALE faster so shadows can be watched sweeping.
        const double time_scale = (input_->is_down(platform::Key::T) && !chat_typing)
                                      ? static_cast<double>(config::DEBUG_TIME_SCALE)
                                      : 1.0;
        // THE TOUR RUNS ON A COUNTED CLOCK, NOT A WALL CLOCK, and this is the
        // rest of the Rule 42 defect in the acceptance instrument.
        //
        // Gating the settle on streaming quiescence was necessary and NOT
        // SUFFICIENT: measured after that fix, two runs of the same binary still
        // differed by 27.67% of pixels. The reason is here. Everything animated
        // in the frame -- sun elevation and colour, the moon, cloud drift, the
        // wind field the foliage bends to -- is a function of `game_seconds_`,
        // which advances by the WALL-CLOCK frame delta. So a machine that runs
        // the tour faster photographs a world at a different hour and a
        // different gust phase, and the diff is dominated by sky and foliage
        // rather than by anything the change under test touched.
        //
        // A fixed increment per rendered frame makes the world a pure function
        // of the frame INDEX, which is what an acceptance instrument has to be.
        // Play still runs on the wall clock, because play is not evidence.
        //
        // AND THE TEST FOR "IS THIS EVIDENCE" IS NOT "IS THIS A TOUR". That was
        // the flaw here, and it survived because the tour was the only door that
        // had been measured. Every other unattended door -- DFN_CAPTURE_AFTER,
        // DFN_RESTORE, DFN_PLAYTEST -- photographs frames that zones then put in
        // acceptance, and every one of them was advancing this clock by the WALL
        // delta. Measured tonight, and measured only because a door I had just
        // shipped failed to deliver what it promised: two runs pinned to the
        // same 600 rendered frames still reached game_seconds 893.719 and
        // 890.615, three seconds of sun and wind apart, because 600 frames of a
        // wall clock is not a duration.
        //
        // Rule 35, third consumer: `unattended_run()` already answers "nobody is
        // playing this" for the menu skip and the cursor grab. It answers this
        // question too, and answering it in one place is the point.
        // И ЧАСЫ СТОЯТ, ПОКА ЗАТВОР ЖДЁТ МИР. Приколотить прибавку к номеру
        // кадра было необходимо и НЕ ДОСТАТОЧНО: номер кадра, на котором
        // открывается затвор, сам зависел от того, сколько кадров ушло на
        // загрузку, — то есть час съёмки был функцией загрузки машины через
        // вторую дверь. Со стоящими часами кадр снимается в час
        // «начало + доза × SIM_DT» независимо от того, сколько заняла
        // загрузка; и пара «до/после», у которой карта потяжелела, не
        // получает вместе с правкой ещё и сдвинутое солнце.
        const bool clock_counted = (tour_.active() || counted_run())
                                   && !unpinned("clock");
        const bool shutter_waits = counted_run() && !unpinned("gate")
                                   && !world_settled_
                                   && (tour_.active() || capture_after_frames_ > 0);
        game_seconds_ += clock_counted
                             ? (shutter_waits ? 0.0
                                              : static_cast<double>(config::SIM_DT))
                             : frame_dt * time_scale;
        // REPLAY OVERRIDES THE CLOCK with the recorded value, so the sky, sun,
        // wind and everything else derived below is the recorded moment --
        // identical on every playback (Rule 53). Set after the increment so the
        // overwrite wins.
        if (replaying_) {
            game_seconds_ = replay_frame_.game_seconds;
        }
        const double day_len = static_cast<double>(config::DAY_LENGTH_SECONDS);
        const double days = game_seconds_ / day_len;
        const float day_fraction = static_cast<float>(days - std::floor(days));
        // The lunar phase is a PURE function of the date — no accumulated state,
        // so the moon is knowable for any past or future day (в69: werewolves,
        // vampires and lunar magic will depend on it).
        const double lunar = days / static_cast<double>(config::LUNAR_MONTH_DAYS);
        const float lunar_phase = static_cast<float>(lunar - std::floor(lunar));
        // У ЛОКАЦИИ НЕТ НЕБА. Часы суток идут (время в игре не стоит), но
        // солнце и ambient интерьера выставлены входом, и apply_sky_time
        // возвращала бы их к уличным КАЖДЫЙ КАДР — то есть свет комнаты
        // существовал бы ровно один кадр после входа (И15).
        if (!render_system_.world_suspended()) {
            render::apply_sky_time(render_system_.environment(), day_fraction,
                                   lunar_phase);
        }
        // THE SKY HAD TWO CLOCKS. The sun and the moon have run off the frame
        // counter for days -- that is what the `game_seconds_ += SIM_DT` above
        // is for -- but the cloud drift and the wind envelope kept reading the
        // wall clock every frame, so a restore restored the sky and not the
        // weather in it. Measured by ui and localised by render: two runs of ONE
        // recipe differed on 1.79% of pixels, all of it sky and treetops, and
        // pinning the visual clock made the same pair bit-identical. One clock
        // now, from the same seconds everything else here is derived from.
        render_system_.set_visual_time(game_seconds_);

        // Authored darkness (LANDSCAPE §6.3). The rule has two halves —
        // ENCLOSED (rock actually overhead, so a shaft open to the sky is not
        // dark) and EARNED (>= DARKNESS_DEPTH_MIN walked ALONG the corridor
        // from the nearest mouth, not straight-line through rock). Both live in
        // worldgen, which is why the app asks rather than computes: an app-side
        // approximation redefined "cave" as "low ground" and would have kept
        // the whole switchback tunnel lit, since its portal is 15 m away
        // through stone but 60 m away on foot.
        if (const auto* t = world_.get<components::Transform>(player_);
            t != nullptr && !render_system_.world_suspended()) {
            render_system_.environment().ambient_darkness = chunks_.darkness_at(t->position);
        }

        if (!playtest_ && !editor && !chat_typing) { // typing must not walk/turn
            // THIRD-PERSON ORBIT (his request, and the Skyrim rule he named):
            // standing still, the mouse swings the camera AROUND the character
            // and the character does not turn; moving, the camera locks behind
            // him. Implemented by withholding the look from sim for those
            // frames rather than by reaching into their look code -- the app
            // owns which input reaches the simulation, and this keeps the
            // character's own yaw the single authority on where he faces.
            bool orbiting = false;
            if (third_person_) {
                if (const auto* ps = world_.get<gameplay::PlayerState>(player_)) {
                    orbiting = ps->stride_speed < 0.15f; // still, not "not running"
                }
            }
            if (orbiting) {
                const glm::vec2 d = input_->mouse_delta();
                const float sens = static_cast<float>(config::MOUSE_SENSITIVITY);
                orbit_yaw_ += d.x * sens;
                orbit_pitch_ = std::clamp(orbit_pitch_ - d.y * sens, -1.2f, 1.2f);
                // Sim's accumulate runs normally so movement keys still reach
                // it -- he can walk out of an orbit without letting go of the
                // mouse -- and then the LOOK it banked is dropped. Clearing the
                // latch rather than adding a sim entry point keeps this the
                // app's decision about which input reaches the simulation,
                // which is the composition root's job (Rule 22), and leaves
                // sim's look code with exactly one caller and one meaning.
                gameplay::player_accumulate_input(world_, *input_);
                if (auto* ps = world_.get<gameplay::PlayerState>(player_)) {
                    ps->pending_look = glm::vec2{0.0f};
                }
            } else {
                // Locked behind: the offset decays rather than snapping, so
                // starting to walk does not whip the camera.
                orbit_yaw_ *= 0.85f;
                orbit_pitch_ *= 0.85f;
                gameplay::player_accumulate_input(world_, *input_); // per render frame (sim's contract)
            }
            // ДВЕРЬ DFN_CAM_ORBIT: обвод камеры по кругу без руки на мыши.
            // Стрела упирается в РАЗНЫЕ стены на разных азимутах, и прогон,
            // снятый на одном, не является измерением комнаты (Rule 27: точка
            // съёмки, неспособная провалиться, не доказательство). Крутится
            // ПОСЛЕ ветки затухания — иначе «locked behind» гасил бы обвод.
            if (third_person_ && cam_probe_spin_ != 0.0f) {
                orbit_yaw_ += cam_probe_spin_ * (3.14159265f / 180.0f)
                              * static_cast<float>(frame_dt);
            }
            // ЗАДАННАЯ КАМЕРА СТЕНДА — ПОСЛЕДНЕЙ, и это не порядок ради
            // порядка: и затухание «locked behind», и обвод выше пишут в те же
            // два поля, а заданная поза обязана быть заданной — иначе два
            // прогона одной дозой снимут два разных кадра.
            if (stand_cam_ != 0) {
                const StandCamera cam = stand_camera(stand_cam_);
                orbit_yaw_ = cam.orbit_yaw_deg * (3.14159265f / 180.0f);
                orbit_pitch_ = cam.orbit_pitch_deg * (3.14159265f / 180.0f);
            }
        }

        // THE WORLD PAUSES WHILE THE INVENTORY IS OPEN (в70: "инвентарь как в
        // скайриме" -- Skyrim, Oblivion and Morrowind all pause, and the pause
        // is what makes a menu with a 3D preview usable at all: you study an
        // object without being hit while you do it).
        //
        // Three of these systems MUST keep running, and sim restructured their
        // zone so that they can. A pause that skipped the whole block would
        // have locked the player in the menu forever, because the key that
        // CLOSES the screen is consumed by player_actions_step -- and the
        // preview turntable lived in the movement path, so "skip movement"
        // would have frozen the one thing on screen. The guard compiles either
        // way; only the restructure makes it correct.
        const bool paused =
            world_.has_resource<gameplay::InventoryScreen>()
            && world_.resource<gameplay::InventoryScreen>().open;

        if (paused) {
            // reset() rather than simply skipping the loop: accumulate() would
            // bank a step per frame while paused and spend the backlog in one
            // burst on unpause, teleporting the player. SIM_MAX_CATCHUP_STEPS
            // bounds that but does not make it right.
            timestep_.reset();
            gameplay::player_actions_step(world_, bus_, *physics_);
            gameplay::update_carried_lights(world_);
            gameplay::update_view_model(world_);
            bus_.pump();
        }

        // СЧЁТНЫЕ ЧАСЫ БЫЛИ ВЫДАНЫ ТОЛЬКО КАРТИНКЕ, А НЕ СИМУЛЯЦИИ, и это
        // недостающая половина починки 13.08. Тогда game_seconds_ приколотили
        // к номеру кадра — солнце, ветер и облака перестали зависеть от
        // загрузки машины. Но ЧИСЛО ШАГОВ симуляции за кадр так и осталось
        // функцией СТЕННОЙ дельты: на быстром кадре ноль шагов, на медленном
        // до пяти. То есть в беспилотном прогоне часы мира и его физика шли с
        // РАЗНОЙ скоростью, и расхождение было разным в каждом прогоне.
        //
        // Что от этого зависело, всё сразу: сколько раз позван стриминг (он
        // стоит под `steps > 0`, а бюджет — один чанк на вызов, то есть на
        // кадре без шага не приезжает НИЧЕГО), сколько шагов осела капсула
        // игрока, докуда доехала дальняя земля, и alpha — остаток
        // аккумулятора, чистая стенная дробь, которой интерполируется поза
        // камеры и всё, что рисуется от неё.
        //
        // Замер: стенд (карта с коротким стримингом) давал два побитово равных
        // кадра и ДО этой правки; город — 0.175 % пикселей врозь. Разделяет
        // руки не «город сложнее», а объём стриминга: там, где очередь пуста
        // почти сразу, стенной счёт шагов не успевает разойтись.
        //
        // Игровой режим не трогаем: игра не является доказательством, и
        // фиксированный шаг на кадр сделал бы скорость мира функцией частоты
        // кадров. Дверь контроля DFN_UNPIN=steps возвращает стенную дельту.
        const bool counted_steps = counted_run() && !unpinned("steps");
        const uint32_t steps =
            paused ? 0u
                   : timestep_.accumulate(counted_steps
                                              ? static_cast<double>(config::SIM_DT)
                                              : frame_dt);

        // STREAMING RUNS ONCE PER FRAME, NOT ONCE PER STEP -- and moving this
        // line out of the catch-up loop is the fix for the project's largest
        // measured stall (730 ms mean, reproduced 3/3 at a fixed world
        // position, plus 1.3-1.6 s at startup).
        //
        // `CHUNK_LOAD_BUDGET` admits ONE chunk per update at ~83 ms, and its
        // own NUMBERS row says two in a frame already reads as a freeze. Inside
        // the loop, `update()` ran once per SIM STEP -- so after any slow frame
        // the accumulator asked for 24 steps, `SIM_MAX_CATCHUP_STEPS` clamped
        // it to 5, and the frame admitted FIVE chunks: ~415 ms against measured
        // second frames of 320-400 ms. Sim's trace shows every stall episode
        // sitting at exactly 5.00 ticks per frame with zero variance against a
        // normal 0.503 -- a number that repeats exactly is a clamp, not a
        // coincidence.
        //
        // The protection inverted precisely when it was needed: THE SLOWER THE
        // FRAME, THE MORE STREAMING WORK THE NEXT FRAME WAS ALLOWED TO DO.
        // Positive feedback, bounded only by the clamp.
        //
        // Two zones reasoned about this correctly and still got it wrong,
        // because both assumed a per-FRAME call while reading a per-STEP one --
        // the budget is denominated in one clock's units while the freeze it
        // prevents is measured in another's.
        //
        // THE INVARIANT BELOW IS PRESERVED: streaming still runs before any
        // step, so collision bodies exist before the first step executes. What
        // is given up is that steps 2..5 of a catch-up burst stream against a
        // focus up to 5 x 16.67 ms x 6 m/s = 0.5 m stale, against a streaming
        // radius measured in hundreds of metres.
        // ФОКУС СТРИМИНГА СЧИТАЕТСЯ ОДИН РАЗ ЗА КАДР И СНАРУЖИ `steps > 0`.
        // Раньше он жил внутри условия, а дальняя земля ниже по кадру считала
        // его ВТОРОЙ РАЗ той же лесенкой — две копии одного решения, которые
        // обязаны согласиться (правило 39). И снаружи он нужен затвору: «всё
        // ли приехало в радиус» спрашивается КАЖДЫЙ кадр, в том числе на
        // кадре, где шага не случилось.
        {
            glm::vec3 focus{0.0f};
            if (replaying_) {
                focus = replay_frame_.position; // stream around the replayed eye
            } else if (tour_.active()) {
                focus = tour_.focus_position();
            } else if (editor) {
                focus = editor_cam_.position(); // stream around the flying eye
            } else if (const auto* t = world_.get<components::Transform>(player_)) {
                focus = t->position;
            }
            stream_focus_ = focus;
        }
        chunks_pending_ = render_system_.world_suspended()
                              ? 0u
                              : chunks_.pending_chunk_count(stream_focus_);
        if (steps > 0) {
            // A step must never execute against a world whose collision bodies
            // are one tick stale, or the player falls through terrain that has
            // not been created yet.
            const glm::vec3 focus = stream_focus_;
            // ПОДВЕС ГОРОДА (И15): поток чанков ЗАМОРОЖЕН, пока игрок в
            // локации. Фокус в кармане на километр ниже выгрузил бы весь
            // город и загрузил бы пустоту вокруг кармана — то есть выход
            // наружу стоил бы полной загрузки карты вместо переключения
            // флага. Город стоит там, где стоял.
            if (!render_system_.world_suspended()) {
                chunks_.update(focus, world_, bus_);
            }
            bus_.pump();
        }

        for (uint32_t i = 0; i < steps; ++i) {
            // Prop collision goes AFTER streaming and BEFORE the step, for the
            // same reason the terrain ferry does: chunk residency and the site
            // entities ChunkManager spawns must both exist, and their bodies
            // must be in the world before step() runs. One tick late means the
            // player walks through a house once.
            gameplay::update_prop_collision(world_, *physics_, chunks_);

            if (!tour_.active()) { // frozen player during the tour: deterministic frames
                // The bot steers by writing the same input intents human keys
                // write -- BEFORE pre_step, per sim's playtest contract.
                if (playtest_ && !playtest_->finished) {
                    gameplay::playtest_drive(*playtest_, world_);
                }
                // ОЧЕРЕДЬ СТЕНДА — ТУДА ЖЕ, КУДА И БОТ, и по той же причине:
                // она пишет ТЕ ЖЕ намерения, что пишут клавиши (move_axes,
                // передачи, прыжок, присяд, E), и дальше работает настоящий
                // код движения. Очередь, которая ставила бы клип напрямую,
                // фотографировала бы AppStand.cpp вместо движка.
                if (stand_seq_) {
                    if (auto* ps = world_.get<gameplay::PlayerState>(player_)) {
                        const float dt = static_cast<float>(timestep_.step_dt());
                        const float prev = stand_seq_t_;
                        stand_seq_t_ += dt;
                        const StandStep step = stand_sequence_at(prev, stand_seq_t_);
                        ps->move_axes = step.move;
                        ps->jog = step.jog;
                        ps->run = step.run;
                        ps->crouch_held = step.crouch;
                        ps->jump_pressed = ps->jump_pressed || step.jump;
                        ps->interact_pressed = ps->interact_pressed || step.interact;
                        ps->yaw = step.face_yaw;
                        // ОРУЖИЕ — ОБЪЯВЛЕНИЕ, А НЕ НАЖАТИЕ (AppStand.h): обе
                        // строки ставятся из одной, как и от клавиши T, чтобы
                        // «руки заняты» не значило разное для картинки и для
                        // скорости.
                        ps->weapon_drawn = step.weapon;
                        if (auto* bd = world_.get<anim::BodyDrive>(player_)) {
                            bd->weapon_drawn = step.weapon;
                        }
                        // «СТОИМ» — ЗАЯВКА, А НЕ НАЖАТИЕ E (AppStand.h).
                        // leave_posture() ничего не делает со стоящим, и
                        // именно поэтому очередь может её объявлять каждый
                        // тик: у второго нажатия E такой роскоши нет.
                        if (step.stand) {
                            leave_posture();
                            ps->yaw = step.face_yaw;
                        }
                    }
                }
                // AFTER the bot (it owns yaw; the probe owns the rest) and
                // BEFORE pre_step, which is where a look intent is consumed.
                body_probe_drive();
                // The restored crouch, re-asserted after accumulate_input has
                // overwritten it from a keyboard nobody is sitting at.
                if (hold_crouch_) {
                    if (auto* ps = world_.get<gameplay::PlayerState>(player_)) {
                        ps->crouch_held = true;
                    }
                }
                // The water callback is the authoritative source. Sampling the
                // terrain and subtracting, or reading the drawn water, would
                // let a primitive that extends past real water be swum in.
                // ПОЗА ПАРКУЕТ КАПСУЛУ И ГАСИТ НАМЕРЕНИЯ — СТРОГО ДО pre_step,
                // потому что именно он превращает намерение в перемещение.
                // Гасить после значило бы дать сидящему полшага в тик.
                // ФИЗИКА ПРЕДМЕТОВ — ДО park_posture И ДО pre_step, и это не
                // вкус: здесь распознаётся КОРОТКОЕ и ДОЛГОЕ нажатие E, а
                // короткое возвращается защёлкой interact_pressed, которую
                // park_posture и actions_step читают ниже по тику. Стой этот
                // вызов после них — короткое нажатие приезжало бы на тик
                // позже долгого, то есть дверь открывалась бы с задержкой,
                // видимой глазом.
                grab_input(static_cast<float>(timestep_.step_dt()));
                park_posture();
                gameplay::player_pre_step(world_, *physics_,
                    [this](glm::vec2 xz) { return chunks_.water_surface_at(xz); },
                    step_ctx_);
                physics_->step(static_cast<float>(timestep_.step_dt()));
                gameplay::player_post_step(world_, *physics_, step_ctx_);
                // ТЕЛА ПРЕДМЕТОВ -> МАТРИЦЫ ОТРИСОВКИ, сразу за шагом: кадр
                // обязан показать ТУ позу, которую только что посчитала
                // физика. Спящие тела не спрашиваются (их поза не менялась).
                sync_loose_props();

                // BODY FERRY (character's zone reads, the app writes): sim's
                // stride clock drives the leg clips, so the visual foot-plant
                // and the footstep sound land on the same tick by construction.
                if (auto* drive = world_.get<anim::BodyDrive>(player_)) {
                    if (const auto* ps = world_.get<gameplay::PlayerState>(player_)) {
                        drive->stride_phase = ps->stride_phase;
                        drive->step_length_m = gameplay::step_length(ps->stride_speed);
                        drive->speed_mps = ps->stride_speed;
                        drive->facing_yaw = ps->yaw;
                        drive->grounded = !ps->airborne;
                        drive->vertical_velocity = ps->vertical_velocity;
                        drive->crouch_blend = ps->crouch_blend;
                        // THE GAIT ITSELF, not the speed it was derived from.
                        // While this line was missing, character re-derived the
                        // gear by comparing speed against WALK_SPEED and
                        // RUN_SPEED, and the three-speed ruling turned that into
                        // a defect: JOG 3.0 rendered as a walk clip leaning
                        // (3.0-1.8)/(6.0-1.8) = 0.286 toward run -- a gait
                        // nobody chose (Rule 37).
                        //
                        // AN EXPLICIT SWITCH, NEVER A CAST. anim sits below
                        // gameplay in the DAG, so anim::Gait cannot BE
                        // gameplay::Gait and the two declarations exist by
                        // construction (Rule 35 with no remedy available -- the
                        // rule's usual fix, move it to NUMBERS, does not apply
                        // to a type). A static_cast would keep compiling if
                        // either enum gained or reordered a member; the switch
                        // goes red HERE, at the one place that can see both.
                        switch (ps->gait) {
                        case gameplay::Gait::Walk: drive->gait = anim::Gait::Walk; break;
                        case gameplay::Gait::Jog:  drive->gait = anim::Gait::Jog;  break;
                        case gameplay::Gait::Run:  drive->gait = anim::Gait::Run;  break;
                        }
                        // THE RETURN FERRY: the lean travels back the other way.
                        // The rig leans a body that has no eye and the camera
                        // holds an eye that has no body, so the offset between
                        // them belonged to nobody and the chest-to-eye gap grew
                        // 5x at full run. character owns the geometry, so
                        // character computes it; sim only applies it to
                        // CameraPose. Deriving it here from the gait would put a
                        // second copy of the AUTHORED gait_run_weight table on
                        // the consumer's side -- the very defect the ferry above
                        // exists to prevent, one direction over.
                        //
                        // ONE TICK LATE, knowingly: post_step already ran this
                        // iteration, so this lands on the next one. 16.7 ms on a
                        // POSTURAL offset that only changes when the player
                        // shifts gear is not perceptible, and the alternative --
                        // computing it before post_step -- needs a second copy
                        // of the gait switch above, which is a worse trade than
                        // one tick. If that ever stops being true, hoist the
                        // switch into a helper rather than duplicating it.
                        // THE EASED WEIGHT, NOT THE GAIT. Both the trunk and the
                        // eye must lean by the SAME float or they desync during
                        // a gear change -- and the desync is one-sided:
                        // accelerating, the eye leads a body still straightening
                        // up, which is safer than steady state; DECELERATING, the
                        // body is still leaning while the eye is already back on
                        // the axis, and the chest returns for the length of every
                        // run->walk. An intermittent chest nobody can reproduce
                        // is worse than the pop it would replace, which is why
                        // easing either side alone was rejected.
                        //
                        // `run_weight` is character's internal state, advanced in
                        // update_bodies each fixed tick, so body and eye read one
                        // number and cannot drift by construction.
                        step_ctx_.eye_lean =
                            anim::eye_lean_offset(body_rig_.proportions,
                                                  drive->run_weight);
                        // THE CROUCH TRAVELS THE SAME WAY, and it had to: the
                        // camera used to drop to sim's own CROUCH_EYE_HEIGHT
                        // 0.85 while character folded the body by half the leg,
                        // which left the eye 0.36 m below the drawn skull and
                        // 0.25 m below its neck -- inside the chest, reported
                        // twice by the user. `drive->crouch_blend` is the same
                        // float this block just ferried the other way, so the
                        // posed body and the camera cannot disagree about how
                        // deep the squat is.
                        step_ctx_.crouch_eye =
                            anim::crouch_eye_offset(body_rig_.proportions,
                                                    drive->crouch_blend);
                    }
                }
                anim::update_bodies(world_, body_rig_);
                // ТЕЛО МОДЕЛИ — ТЕМ ЖЕ ТИКОМ. Проигрыватель клипов ведёт своё
                // состояние (какой клип, где мы в нём, что гаснет) РОВНО ЗДЕСЬ,
                // сразу за update_bodies и по той же ферме BodyDrive: иначе он
                // читал бы привод прошлого тика, и модель отставала бы от
                // коробок ровно на кадр — ту самую разницу, которую доза
                // DFN_BODY_BOXES обязана НЕ показывать.
                if (skinned_character_.ready()) {
                    if (const auto* cdrive = world_.get<anim::BodyDrive>(player_)) {
                        const auto* ctr = world_.get<components::Transform>(player_);
                        skinned_character_.advance(
                            body_rig_, *cdrive,
                            ctr != nullptr ? ctr->position : glm::vec3{0.0f},
                            static_cast<float>(timestep_.step_dt()));
                    }
                }
                // КАМЕРА ПОЗЫ — СРАЗУ ЗА ТЕЛОМ: update_bodies только что
                // опубликовал глаз НАРИСОВАННОЙ позы, и ставить камеру раньше
                // значило бы читать позу прошлого тика. Стоит ДО update_hover,
                // потому что луч перекрестья обязан идти из того же глаза.
                posture_camera();
                posture_trace_step(static_cast<float>(timestep_.step_dt()));

                // Invariant checks AFTER post_step; an incident screenshots.
                if (playtest_ && !playtest_->finished) {
                    if (const size_t n = gameplay::playtest_check(*playtest_, world_, pt_env_);
                        n > 0 && pt_shots_ < 20) {
                        (void)renderer_->save_screenshot(
                            pt_dir_ + "/incident_" + std::to_string(pt_shots_++) + ".png");
                    }
                }

                // Hover AFTER post_step: the crosshair ray must use THIS tick's
                // eye pose. Hovering from last tick's pose acts on what you were
                // looking at a frame ago -- invisible standing still, wrong
                // while turning.
                gameplay::update_hover(world_, *physics_);
                // ПРИЦЕЛ ДВЕРИ = РАДИУС + ВЗГЛЯД, и вторая половина живёт
                // здесь. Луч взаимодействия отвечает «в теле двери» — а тело,
                // начатое вокруг головы, отвечает так под любым углом (Jolt
                // считает луч из нутра выпуклого тела попаданием на нулевой
                // доле). Крит владельца 28.08: «захожу в дом — сразу горит
                // надпись „выйти"». СТРОГО МЕЖДУ hover и actions: подсказка и
                // клавиша E читают ОДИН HoverTarget, и погасить его здесь —
                // единственный способ не дать им разойтись.
                filter_door_hover();
                // ТОТ ЖЕ ПРИЁМ ДЛЯ МЕБЕЛИ, И ЭТО НЕ КОПИЯ, А ВТОРОЙ СПИСОК:
                // геометрия у двери и у лавки разная (прямоугольник створки
                // против габарита предмета), а правило одно — подсказка горит
                // только когда СМОТРИМ.
                filter_seat_hover();
                // Actions AFTER the hover they act on: E interact, F light, I bag.
                gameplay::player_actions_step(world_, bus_, *physics_);
                // Carriers without a view model (NPCs with lanterns).
                gameplay::update_carried_lights(world_);
                // LAST: reads the CameraPose post_step wrote and the HeldItem
                // the actions may have just changed, so a torch picked up this
                // tick is in hand this tick rather than next.
                gameplay::update_view_model(world_);
                bus_.pump(); // deliver the interaction events published above
                // ПЕРЕХОД В ЛОКАЦИЮ — ПОСЛЕ РАЗДАЧИ СОБЫТИЙ, НЕ ВНУТРИ НЕЁ.
                // Вход сносит сущности переходов и телепортирует игрока;
                // сделать это из обработчика значило бы править контейнеры,
                // по которым шина сейчас идёт (И15).
                take_portal();
                // ПОЗА — ПОСЛЕ РАЗДАЧИ СОБЫТИЙ, по той же причине, что и
                // переход: вход в позу правит PlayerState и телепортирует
                // капсулу, а из обработчика шины это правка контейнеров, по
                // которым шина сейчас идёт.
                take_seat();
                // ENTITIES QUEUED FOR DESTRUCTION ACTUALLY DIE HERE. World.h
                // says this belongs to the app loop, "once per simulation tick,
                // after all systems have run" -- and nothing called it, so every
                // destroy_deferred() in the project was a no-op. The one
                // production caller is TAKE: the item went into the bag and the
                // prop stayed standing, takeable again, for ever, which the user
                // reads as "I pressed it and nothing happened" because the torch
                // is still there.
                //
                // The suite was green throughout because three tests call this
                // themselves. A test that performs a step the application does
                // not perform is testing a game that does not exist.
                world_.flush_destroyed();
            }
        }

        const float alpha = static_cast<float>(timestep_.alpha());
        const auto* pose = world_.get<components::CameraPose>(player_);
        const auto* prev_pose = world_.get<components::PreviousCameraPose>(player_);
        if (replaying_) {
            // TRAJECTORY REPLAY drives the eye from the file. prev == curr so any
            // alpha reproduces the recorded pose exactly, and the recorded fov is
            // applied (the run fov-kick changes the image, so it is reproduced,
            // not derived). This is what makes two playbacks bit-identical.
            camera_.set_poses({replay_frame_.position, replay_frame_.yaw, replay_frame_.pitch},
                              {replay_frame_.position, replay_frame_.yaw, replay_frame_.pitch});
            camera_.set_projection(replay_frame_.fov_y, camera_.aspect_ratio(),
                                   camera_.near_plane(), camera_.far_plane());
        } else if (editor) {
            // The app owns the free pose outright: prev == curr, so any alpha
            // the loop computes below reproduces it exactly and the frame log,
            // audio and culling all read the flown eye.
            const auto ep = editor_cam_.pose();
            camera_.set_poses(ep, ep);
            camera_.set_projection(static_cast<float>(config::CAMERA_FOV_Y),
                                   camera_.aspect_ratio(), camera_.near_plane(),
                                   camera_.far_plane());
        } else if (pose != nullptr && prev_pose != nullptr) {
            // THIRD PERSON: the eye pulls back along the orbit direction. The
            // character's own yaw is untouched -- the camera moves, he does not
            // turn -- which is what makes standing-still orbiting read right.
            if (third_person_) {
                const float y0 = prev_pose->yaw + orbit_yaw_;
                const float y1 = pose->yaw + orbit_yaw_;
                const float p0 = prev_pose->pitch + orbit_pitch_;
                const float p1 = pose->pitch + orbit_pitch_;
                // СТРЕЛА, А НЕ ФИКСИРОВАННОЕ СМЕЩЕНИЕ. Раньше здесь стояло
                // eye - fwd*3.2 + up*0.55 без единого вопроса к миру, и камера
                // в доме уезжала СКВОЗЬ стену — «могу за границы посмотреть»
                // (владелец, 27.08). Кадрирование (3.2 назад, 0.55 вверх) не
                // изменилось ни на сантиметр: оно теперь умолчание оснастки в
                // CameraBoom.h, а здесь у него появился ограничитель.
                //
                // ОДНА ДЛИНА НА ОБА КОНЦА интерполяции. Щуп пускается от
                // ТЕКУЩЕЙ головы, и полученная длина применяется и к prev, и к
                // curr: два конца, посчитанные независимо, дали бы отрезок, чья
                // середина при alpha=0.5 лежит там, где не мерил никто.
                // НАЧАЛО СТРЕЛЫ У ТЕЛА НА МЕБЕЛИ — НЕ ГЛАЗ ПОЗЫ. Глаз
                // лежащего стоит в 0.25 м над матрасом, и сфера щупа радиусом
                // 0.25 начинала ВНУТРИ кровати: коллизия честно прижимала
                // стрелу к нулю, и третье лицо схлопывалось в первое (находка
                // волны поз). Точка над предметом считается при посадке
                // (posture_perch_), смешивается долей позы и здесь только
                // читается; заодно она же — центр обвода, оттого лежащего
                // камера обходит сверху-сбоку, а не ныряет к нему в матрас.
                gameplay::CameraBoomPerch perch1{pose->position, p1};
                gameplay::CameraBoomPerch perch0{prev_pose->position, p0};
                if (posture_perch_valid_ && posture_cam_enabled()) {
                    const auto* drv = world_.get<anim::BodyDrive>(player_);
                    const float pw = drv != nullptr ? drv->posture_blend : 0.0f;
                    perch1 = gameplay::camera_boom_perch(pose->position, p1,
                                                         posture_perch_,
                                                         posture_pitch_cap_, pw);
                    perch0 = gameplay::camera_boom_perch(prev_pose->position, p0,
                                                         posture_perch_,
                                                         posture_pitch_cap_, pw);
                }
                const auto aim1 = gameplay::camera_boom_aim(y1, perch1.pitch,
                                                            cam_boom_desc_);
                const auto aim0 = gameplay::camera_boom_aim(y0, perch0.pitch,
                                                            cam_boom_desc_);
                float length = aim1.reach;
                if (physics_ != nullptr && cam_collide_enabled()) {
                    // ТОЛЬКО СТАТИКА. Оболочка дома и земля — LAYER_STATIC (у
                    // интерьера это его собственное тело, AppInterior); утварь
                    // и створки лежат на LAYER_INTERACTABLE, и брать их в щуп
                    // значит дёргать камеру о каждый оброненный факел.
                    const platform::RayHit sweep = gameplay::camera_boom_sweep(
                        *physics_, perch1.origin, aim1, cam_boom_desc_,
                        physics::LAYER_STATIC);
                    length = gameplay::camera_boom_free_length(sweep, aim1.reach,
                                                               cam_boom_desc_);
                }
                length = gameplay::camera_boom_step(cam_boom_, length,
                                                    static_cast<float>(visual_dt),
                                                    cam_boom_desc_);
                const glm::vec3 cam1 = perch1.origin + aim1.direction * length;
                const glm::vec3 cam0 = perch0.origin + aim0.direction * length;
                // И КАДР СНИМАЕТСЯ ТЕМ ЖЕ ТАНГАЖОМ, КОТОРЫМ СЧИТАНА СТРЕЛА.
                // Иначе камера, поднятая над лежащим, смотрела бы туда же,
                // куда он, — в потолок, мимо собственного тела.
                camera_.set_poses({cam0, y0, perch0.pitch}, {cam1, y1, perch1.pitch});
                cam_probe_step(perch1.origin, cam1, length, aim1);
            } else {
                cam_boom_.length = -1.0f; // вид выключен: стрела заводится заново
                camera_.set_poses({prev_pose->position, prev_pose->yaw, prev_pose->pitch},
                                  {pose->position, pose->yaw, pose->pitch});
            }
            // Speed-coupled FOV (sim writes fov_scale at fixed tick; the app
            // interpolates and applies -- default 1.0 changes nothing).
            const float fs = prev_pose->fov_scale
                           + (pose->fov_scale - prev_pose->fov_scale) * alpha;
            camera_.set_projection(static_cast<float>(config::CAMERA_FOV_Y) * fs,
                                   camera_.aspect_ratio(), camera_.near_plane(),
                                   camera_.far_plane());
        }

        // THE FRAME LOG, written HERE and not earlier: every quantity below is
        // the one this frame is actually about to be drawn with, so a line and
        // its frame cannot disagree. Logged unconditionally when the door is
        // open -- filtering by speed here would hide the standing-still control
        // that tells us whether the instrument itself is steady (Rule 30).
        if (frame_log_ != nullptr) {
            const auto eye = camera_.interpolated_pose(alpha);
            float spd = 0.0f;
            if (const auto* ps = world_.get<gameplay::PlayerState>(player_)) {
                spd = ps->stride_speed;
            }
            // Счётчики рендера — ПОСЛЕДНЕГО ЗАВЕРШЁННОГО кадра (контракт
            // frame_stats): честно на строку позади колонок позы, и заголовок
            // лога говорит это вслух. Профиль LOD читает медианы вызовов и
            // треугольников по маршруту — сдвиг на кадр там ничто, а тихий
            // сплайс «счётчики N-1 против dt N» без предупреждения уже был
            // назван капканом (замер кузнеца 22.08).
            const platform::RenderFrameStats& fst = renderer_->frame_stats();
            std::fprintf(frame_log_,
                         "%llu %.4f %.6f %.6f %.8f %.6f %.6f %.6f %.6f %.6f"
                         " %u %u %u\n",
                         static_cast<unsigned long long>(frame_log_index_++),
                         frame_dt * 1000.0, game_seconds_,
                         static_cast<double>(spd),
                         static_cast<double>(camera_.fov_y()),
                         static_cast<double>(eye.position.x),
                         static_cast<double>(eye.position.y),
                         static_cast<double>(eye.position.z),
                         static_cast<double>(eye.yaw),
                         static_cast<double>(eye.pitch),
                         fst.scene_draws, fst.scene_triangles,
                         fst.backend_draws);
        }

        // Audio follows the eye; the wind bed follows the ONE wind model the
        // foliage bends to (Rule 35 -- same gust envelope for ear and eye).
        {
            const auto eye = camera_.interpolated_pose(alpha);
            const float cp = std::cos(eye.pitch);
            const platform::ListenerPose lp{
                eye.position,
                {std::sin(eye.yaw) * cp, std::sin(eye.pitch), -std::cos(eye.yaw) * cp},
                {0.0f, 1.0f, 0.0f}};
            audio_->update(lp);

            // ОТКУДА СЛУШАЮТ МИР. Снаружи — глаз. В ЛОКАЦИИ — точка возврата
            // у двери, а не глаз: карман интерьера лежит на километр ниже
            // мира, и слушать город оттуда значило бы получить тишину по
            // расстоянию — правильный ответ по неправильной причине (а на
            // карте, где деревья растут в километре по горизонтали, тот же
            // счёт дал бы уже неправильный ответ).
            gameplay::WorldAmbience::Listener al;
            al.position = eye.position;
            if (world_.has_resource<gameplay::InteriorState>()) {
                const auto& ist = world_.resource<gameplay::InteriorState>();
                if (ist.inside() && !ist.stack.empty()) {
                    al.indoors = true;
                    al.position = ist.stack.front().position;
                    // ОТКРЫТОСТЬ ДВЕРИ — расстояние до САМОГО ПОЛОТНА, а не
                    // до точки входа. Точка входа стояла здесь час назад и
                    // оказалась неправдой на первом же замере: беспилотный
                    // вход в дом Вайтрана дал срез 420 Гц («вглубь комнаты»),
                    // стоя у двери, — потому что точка входа локации не
                    // обязана лежать у порога (её ставит композиция, и в
                    // безымянном случае это вообще начало кармана). Полотно
                    // же знает своё место само: DoorAim::at посчитан из его
                    // треугольников.
                    float d = 1e9f;
                    for (const PortalLink& pl : portals_) {
                        if (!pl.interior) {
                            continue; // дверь ГОРОДА к делу не относится
                        }
                        d = std::min(d, glm::length(eye.position - pl.aim.at));
                    }
                    if (d > 1e8f) {
                        // Полотна нет вовсе (локация без перехода — так бывает
                        // у пробных карт): глухо, и это честнее выдуманного
                        // порога.
                        d = 1e9f;
                    }
                    const float near_door = std::clamp(1.0f - (d - 1.5f) / 4.0f, 0.0f, 1.0f);
                    al.door_openness = near_door * (ist.door_open ? 1.0f : 0.55f);
                }
            }
            // ЛУЧИ К ИСТОЧНИКУ — ЗДЕСЬ, потому что физика есть только у app:
            // gameplay получает замыкание и не знает ни про Jolt, ни про слои.
            // ДВА ЛУЧА, разнесённые в стороны: ствол дерева стоит тем же слоем,
            // что и стена дома, и одиночный луч щёлкал бы «перекрыто/свободно»
            // на каждом шаге по роще. Стена ловит оба, ствол — один.
            const auto probe = [this](const glm::vec3& from,
                                      const glm::vec3& to) -> float {
                if (physics_ == nullptr) {
                    return 0.0f;
                }
                const glm::vec3 delta = to - from;
                const float len = glm::length(delta);
                if (len < 2.0f) {
                    return 0.0f; // источник в двух шагах: заслонять нечему
                }
                const glm::vec3 dir = delta / len;
                // Луч НЕ ДОВОДИТСЯ до самой кроны: под ней стоит ствол этого
                // же дерева, и «дерево заслоняет само себя» — верный ответ на
                // неверный вопрос.
                const float reach = len - 2.0f;
                // БОКОВОЙ ОТНОС, И ОН НЕ ИМЕЕТ ПРАВА БЫТЬ НОРМАЛИЗАЦИЕЙ НУЛЯ.
                // Крона стоит НАД слушателем чаще, чем кажется: 12 м вверх и
                // три в сторону — это почти вертикальный луч, у которого
                // (-dir.z, 0, dir.x) вырождается, а normalize вырожденного
                // вектора даёт NaN и уводит луч в бесконечность.
                glm::vec3 side{-dir.z, 0.0f, dir.x};
                const float side_len = glm::length(side);
                side = side_len > 1e-3f ? side / side_len * 0.9f
                                        : glm::vec3{0.9f, 0.0f, 0.0f};
                int blocked = 0;
                for (const glm::vec3& off : {side, -side}) {
                    if (physics_->raycast(from + off, dir, reach,
                                          physics::LAYER_STATIC)
                            .hit) {
                        ++blocked;
                    }
                }
                return static_cast<float>(blocked) * 0.5f;
            };
            ambience_.update(*audio_, al,
                             render_system_.environment().wind_strength,
                             static_cast<float>(frame_dt), probe);
            if (ambience_log_ && game_seconds_ >= ambience_log_at_) {
                ambience_log_at_ = game_seconds_ + 1.0;
                std::fprintf(stderr,
                             "[звук] излучателей %zu, голосов %zu/%zu, ветер "
                             "%.2f (ступень %d), слушатель %s\n",
                             ambience_.emitters().size(), ambience_.live_voices(),
                             gameplay::AMBIENCE_MAX_VOICES,
                             static_cast<double>(
                                 render_system_.environment().wind_strength),
                             ambience_.current_wind_step() + 1,
                             al.indoors ? "в локации" : "снаружи");
                for (const auto& e : ambience_.emitters()) {
                    std::fprintf(stderr,
                                 "[звук]   %s x%.1f z%.1f d=%.1f м g=%.4f "
                                 "срез=%.0f Гц перекрытие=%.2f крон=%u\n",
                                 e.water ? "вода " : (e.conifer ? "хвоя " : "листва"),
                                 static_cast<double>(e.at.x),
                                 static_cast<double>(e.at.z),
                                 static_cast<double>(e.distance_m),
                                 static_cast<double>(e.gain),
                                 static_cast<double>(e.cutoff_hz),
                                 static_cast<double>(e.occlusion), e.crowns);
                }
            }
        }
        if (playtest_ && !playtest_->finished) {
            gameplay::playtest_note_frame(*playtest_, static_cast<float>(frame_dt));
        }
        if (tour_.active()) {
            tour_.apply(camera_);
        }

        // FAR-DETAIL FERRY. Four things here are load-bearing and were paid for
        // in measurements rather than opinion:
        //  - the streamed rect is CHUNK-ALIGNED, not eye +/- radius in metres.
        //    Render's descent tests inside/outside against it, and core measured
        //    an unaligned rect costing 71 nodes where the aligned one costs 46.
        //    It is also a correctness matter, not a saving: a level-0 node is
        //    1 m where a chunk heightfield is 2 m, so without the rect the two
        //    systems draw the same ground twice at slightly different heights.
        //  - update_lod takes the RENDER delta, never SIM_DT. The cross-fade is
        //    a visual effect; at the fixed rate a 0.6 s dissolve steps in 16
        //    chunks and reads as a flicker rather than a fade.
        //  - the ferry collects against lod_pending(), NOT lod_to_load().
        //    to_load names a node exactly once, while core answers several
        //    frames later under its row budget, so a ferry built on to_load
        //    requests nodes it never collects and the ground never appears.
        //  - drop the mesh BEFORE releasing the field it was built from, the
        //    same lifetime rule as ChunkUnloaded.
        if (render_system_.lod_enabled()) {
            const glm::vec3 eye = camera_.interpolated_pose(alpha).position;
            const float cs = static_cast<float>(config::CHUNK_SIZE);
            const float r = static_cast<float>(config::CHUNK_LOAD_RADIUS);
            // Same focus the streaming loop used this frame: the tour drives it
            // during a tour, the player otherwise.
            // ТОТ ЖЕ ФОКУС, ЧТО У ПОТОКА ЧАНКОВ, и теперь буквально тот же:
            // лесенка «повтор / тур / редактор / игрок» стояла здесь вторым
            // экземпляром и была обязана совпадать с первым.
            const glm::vec3 lod_focus = stream_focus_;
            const glm::vec2 fc{std::floor(lod_focus.x / cs), std::floor(lod_focus.z / cs)};
            render_system_.set_streamed_rect({(fc.x - r) * cs, (fc.y - r) * cs},
                                             {(fc.x + r + 1.0f) * cs,
                                              (fc.y + r + 1.0f) * cs});
            // THE CROSS-FADE ALSO RUNS ON A COUNTED CLOCK DURING A TOUR, for
            // the same reason the game clock does: `LOD_FADE_SECONDS` is a
            // dissolve measured in REAL seconds, so a machine that reaches the
            // shot faster catches the fade at a different point and two runs
            // disagree over whichever patches of ground are mid-dissolve.
            //
            // Measured: gating the settle on streaming quiescence took the
            // tour's self-control from 27.67% to nothing on its own; adding the
            // counted game clock took it to 14.73%. Neither was sufficient
            // alone, and each was a different quantity riding the same wall
            // clock. This is the third.
            render_system_.update_lod(eye, static_cast<float>(visual_dt));
            refresh_scene_lod(eye);

            // THE SWARM: counted dt, never the wall clock — two acceptance runs
            // pinned to the same frame count must put every mote in the same
            // place, and a wall second holds a different number of frames on a
            // loaded machine (the same trap the game clock fell into once).
            //
            // NIGHT AS A NUMBER, not a switch: the sun's height fades the swarm
            // in across dusk instead of turning it on between two frames.
            {
                const float sun_y = render_system_.environment().sun_direction.y;
                const float night01 = std::clamp((0.06f - sun_y) / 0.18f, 0.0f, 1.0f);
                const float dt = static_cast<float>(visual_dt);
                fireflies_.update(dt, night01, [this](float x, float z) {
                    // The streamer knows the ground; a chunk that is not
                    // resident yet answers nothing, and a mote there simply
                    // keeps the height it had rather than dropping to zero.
                    return chunks_.height_at({x, z}).value_or(0.0f);
                });
                render::MeshData swarm;
                // The camera gives right and forward; up is their cross —
                // billboards must face the EYE, and a world-up would tilt every
                // mote the moment the player looks down.
                const glm::vec3 cam_right = camera_.right(alpha);
                const glm::vec3 cam_up = glm::normalize(
                    glm::cross(cam_right, camera_.forward(alpha)));
                fireflies_.build_mesh(swarm, cam_right, cam_up);
                // SAY IT ONCE, when the swarm first exists: "the fireflies are
                // in" is a claim, "1120 vertices at night01 0.87" is a fact,
                // and a swarm that silently built nothing would look exactly
                // like a swarm that is simply hard to see.
                static bool swarm_announced = false;
                if (!swarm_announced && !swarm.vertices.empty()) {
                    swarm_announced = true;
                    std::fprintf(stderr, "[fireflies] %zu mote(s) alight, %zu "
                                         "vertices, night %.2f\n",
                                 static_cast<std::size_t>(fireflies_.count()),
                                 swarm.vertices.size(), static_cast<double>(night01));
                }
                render_system_.set_firefly_mesh(*renderer_, swarm);

                std::vector<render::RenderSystem::ExtraLight> glow;
                for (const auto& L : fireflies_.lights_ranked(eye, 3)) {
                    // Warm green, flora's colour; the radius rides the pulse so
                    // a breathing swarm breathes on the ground too.
                    glow.push_back({L.pos, glm::vec3{0.62f, 0.95f, 0.45f} * L.intensity,
                                    2.0f + L.intensity, false});
                }
                render_system_.set_transient_lights(std::move(glow));
            }

            const auto to_world = [](const render::LodNode& n) {
                return world::CoarseNode{n.level, n.x, n.z};
            };
            std::vector<world::CoarseNode> wanted;
            wanted.reserve(render_system_.lod_to_load().size());
            for (const auto& n : render_system_.lod_to_load()) {
                wanted.push_back(to_world(n));
            }
            if (!wanted.empty()) {
                chunks_.request_coarse_nodes(wanted);
            }
            for (const auto& n : render_system_.lod_to_release()) {
                render_system_.drop_lod_node(*renderer_, n);
                chunks_.release_coarse_node(to_world(n));
            }
            for (const auto& n : render_system_.lod_pending()) {
                const auto wn = to_world(n);
                if (auto hf = chunks_.coarse_heightfield(wn)) {
                    auto sf = chunks_.coarse_surfacefield(wn);
                    render_system_.upload_lod_node(*renderer_, n, *hf,
                                                   sf ? &*sf : nullptr);
                }
            }
        }

        // СБОРКА КАДРА ОВЕРЛЕЕВ — ФАКТЫ ЗДЕСЬ, СБОРКА В AppHud.cpp (слой 3
        // разбора, docs/audits/PLAN_APP_DECOMPOSITION.md). Здесь лежало 238 строк, из
        // которых половина уже была вынесена в модули, а СБОРКА — порядок
        // слоёв, кто под кем стоит, и есть ли вообще что показывать — жила в
        // файле, который держит окно. Ровно поэтому наложение отладочного
        // вывода и редакторского блока три дня ловил человек, а не прибор.
        //
        // ГРАНИЦА ТАКАЯ: собирание ФАКТОВ остаётся здесь (оно требует мира, а
        // мир требует окна), а РЕШЕНИЯ — что говорит подпись, что рисуется
        // раньше, кто под кем — уехали туда, где их читает рукав.
        {
            render::PixelCanvas& hud = render_system_.hud();
            hud.clear_transparent();
            // ЧТО ОСТАЛОСЬ МИРУ ПОСЛЕ ПОЛОС ИНТЕРФЕЙСА, на ЭТОМ холсте.
            // Жалоба пользователя 17.08: «кнопки сверху пересекаются с дебаг
            // текстом». Число не подбирается и не живёт здесь — его считает
            // EditorUi, который эти полосы и поставил, и отдаёт ДОЛЯМИ, а не
            // пикселями: HUD компонуется во ВНУТРЕННЮЮ цель, а не в кадровый
            // буфер, и число в пикселях было бы верным для одного и тихо
            // неверным для другого (EditorUi.h говорит это дословно).
            // МИР САДИТСЯ ПОД ПОЛОСУ, А НЕ ПОД НЕЁ ЖЕ ПОВЕРХ (заказ 18.08:
            // «пусть инструмент рисуется не поверх игрового экрана... пусть игра
            // ниже рисуется, тогда проблем с наложением не будет»).
            //
            // Тот же прямоугольник, что раньше служил ТОЛЬКО оверлеям, теперь
            // отдаётся РЕНДЕРЕРУ: картинка мира физически не заходит под
            // интерфейс. Поэтому HUD ниже получает ПОЛНЫЙ прямоугольник —
            // отступать ему больше не от чего, а отступи он ещё раз, вышел бы
            // двойной отступ, и компас уехал бы от края на две полосы.
            const EditorRect world_norm = editor_ui_.world_rect_norm();
            renderer_->set_present_rect_norm(world_norm.x, world_norm.y,
                                             world_norm.w, world_norm.h);
            HudFrame frame;
            frame.facts.third_person = third_person_;
            frame.facts.map_open = render_system_.map_open();
            frame.facts.debug_readout = debug_overlay_ || capture_pending_;
            frame.facts.editor_mode = editor;
            // Полный холст: отступ уже взят рендерером выше (см. довод там).
            frame.facts.world_x = 0;
            frame.facts.world_y = 0;
            frame.facts.world_w = static_cast<int>(hud.width());
            frame.facts.world_h = static_cast<int>(hud.height());
            // КУДА СМОТРИТ ГЛАЗ, а не куда стоит тело: лента обязана совпасть
            // с картинкой, а картинка нарисована из позы КАМЕРЫ — той же, из
            // которой снимок состояния берёт свой yaw.
            frame.facts.yaw_rad = camera_.interpolated_pose(alpha).yaw;
            frame.facts.fov_y_rad = camera_.fov_y();

            // ЧТО В РУКЕ И ЧТО БУДЕТ ПО ЩЕЛЧКУ — спрашивается У ИНСТРУМЕНТА,
            // а СКЛАДЫВАЕТСЯ во фразу уже в AppHud (там же это и проверяется).
            ToolStatus st;
            std::string tool_title;
            frame.tool.editor = editor;
            if (editor) {
                if (const IEditorTool* tool = editor_ui_.toolbox().active();
                    tool != nullptr) {
                    frame.tool.have_tool = true;
                    tool_title = EditorUi::tr(tool->identity().title_key);
                    frame.tool.title = tool_title;
                    frame.tool.wants_rotation = tool->wants_part_rotation();
                    st = editor_ui_.toolbox().status(aim_this_frame());
                    frame.tool.status_key = st.key;
                    frame.tool.status_text = st.text;
                    frame.tool.ready = st.ready;
                    frame.tool.group = build_group_name_;
                }
                frame.tool.ui_wants_mouse = editor_ui_.wants_mouse();
            }

            // NOT IN THE EDITOR: the free camera has no reach and does not
            // interact, so the player's last hover ("Открыть") would hang under
            // the crosshair as a verb the flying eye cannot perform.
            if (!editor && world_.has_resource<components::HoverTarget>()) {
                if (const auto& hover = world_.resource<components::HoverTarget>();
                    hover.prompt_key != 0) {
                    frame.prompt = localized(hover.prompt_key);
                }
            }
            // В ПОЗЕ ПОДСКАЗКА ОДНА И ОНА ПЕРЕБИВАЕТ НАВЕДЕНИЕ: сидящий
            // паркует капсулу в стороне от мебели, перекрестье смотрит куда
            // угодно, а единственное, что клавиша E сейчас делает, — поднимает
            // его. «Сесть» на уже сидящем звало бы делать сделанное.
            if (!editor && in_posture()) {
                frame.prompt = localized(serialization::fnv1a64("prompt.stand"));
            }
            // ПОДСКАЗКА ПРЕДМЕТА ПЕРЕБИВАЕТ НАВЕДЕНИЕ — и только она может это
            // делать, кроме позы. Довод тот же: когда предмет УЖЕ в руках,
            // единственное, что клавиша сейчас делает, — отпускает его, а
            // «Сесть» под перекрестьем звало бы к действию, которое зона
            // предметов запрещает (руки заняты). Когда предмет только под
            // прицелом, «Взять» стоит ближе к правде, чем «Сесть» на лавке за
            // ним: целятся в то, что ближе, и хват берёт ЛУЧОМ, а не коробкой.
            if (!editor && !in_posture()) {
                if (const std::uint64_t key = grab_prompt_key(); key != 0) {
                    frame.prompt = localized(key);
                }
            }
            if (const char* probe = door_value("DFN_HUD_PROBE");
                probe != nullptr && *probe == '1') {
                frame.probe = true;
            }
            // Дверь читается ОДИН РАЗ: дверь, опрашиваемая каждый кадр, — это
            // выключатель, и два кадра одного прогона могут разойтись в том,
            // что именно проверялось. Обе руки приёмки (с панелью и без) выходят
            // из ОДНОГО бинарника.
            static const bool hud_off = [] {
                const char* v = door_value("DFN_HUD");
                return v != nullptr && *v == '0';
            }();
            frame.hud_off = hud_off;

            DebugSnapshot snap;
            if (debug_overlay_ || capture_pending_) {
                snap = collect_snapshot(alpha);
                frame.readout = &snap;
            }
            // В28 INTROSPECTION, laid out by EditorHud. frame_stats() and
            // center_pick() describe the LAST completed frame (read before this
            // frame's render), which is one frame of lag on a readout --
            // imperceptible, and the only honest option, since the numbers do
            // not exist until end_frame.
            //
            // БЛОК ЦИФР ПОКАЗЫВАЕТСЯ ТОЛЬКО ВМЕСТЕ С ОТЛАДОЧНЫМ ВЫВОДОМ (заказ
            // 18.08: «убери текст про число треугольников и режим редактора»).
            // Он не удалён: числа кадра, прицела и мыши — приборы, которыми за
            // этот вечер поймано три дефекта, и выбрасывать их значило бы
            // остаться без глаз. Он просто перестал быть постоянным: F3
            // поднимает и вывод, и его. На чистом экране режим называет
            // маленький жёлтый значок внизу, и этого человеку достаточно.
            EditorHudSnapshot ed;
            if (editor && (debug_overlay_ || capture_pending_)) {
                const platform::RenderFrameStats& fs = renderer_->frame_stats();
                const platform::RenderPick& pk = renderer_->center_pick();
                ed.fly_speed_mps = editor_cam_.speed();
                ed.frame_triangles = fs.scene_triangles;
                ed.frame_draws = fs.backend_draws;
                ed.wireframe = wireframe_;
                ed.aim_hit = pk.hit;
                ed.aim_triangles = pk.triangles;
                ed.aim_distance_m = pk.distance_m;
                ed.aim_pick_id = pk.pick_id;
                // МЫШЬ И УГОЛ НА ЭКРАН (заказ 18.08). Смещение берётся ЗДЕСЬ и
                // сейчас, из того же самого input_, из которого его берёт
                // камера, — не из запомненной копии: копия рядом с настоящим
                // значением и есть способ показать одно, пока работает другое.
                const glm::vec2 md = input_->mouse_delta();
                ed.mouse_dx = md.x;
                ed.mouse_dy = md.y;
                ed.cursor_captured = input_->is_cursor_captured();
                ed.cursor_free = editor_ui_.toolbox().pointer_mode();
                ed.yaw_deg = glm::degrees(editor_cam_.yaw());
                ed.pitch_deg = glm::degrees(editor_cam_.pitch());
                frame.editor_block = &ed;
            }
            frame.chat = &chat_overlay_;
            render_system_.set_hud_visible(compose_hud(hud, frame));
        }

        // RECORDED BEFORE render(), which is the only window there is: both
        // begin_frame and end_frame live INSIDE RenderSystem::render, and the
        // backend clears the line list when it submits. Pushed after render()
        // the lines would sit until the NEXT frame's submit — a whole frame of
        // lag on a view whose job is to answer "what am I standing against".
        // THE COLLIDERS, DRAWN (DFN_DRAW_COLLIDERS=1). Asked for by the user
        // after three separate "I cannot walk here" reports that were all one
        // wrong shape: a collider nobody can see is a collider nobody can
        // argue with, and every such report costs a round trip to diagnose.
        //
        // Drawn as the EDGES OF THE ACTUAL TRIANGLES handed to physics — not a
        // box around them and not a re-derivation. A debug view that draws its
        // own idea of the shape would agree with the code that built it and
        // disagree with the body that is actually there, which is worse than
        // no view at all.
        // THE GHOST. Drawn as the part's MEASURED box (render::measure_object,
        // the same ruler the judge uses), green when the judge allows it and
        // red when it does not — and the red one is still drawn, because
        // hiding it would leave the builder aiming at nothing and guessing.
        // The part it would delete is outlined too, in the same red: what is
        // about to be destroyed must be visible before the key is pressed.
        if (mode_ == AppMode::Editor && editor_ui_.toolbox().preview(ToolAim{}).ghost) {
            BuildJudgeCtx gctx{&chunks_, &scene_objects_, &build_extents_, &gallery_shelves_};
            const auto outline = [&](const world::Placement& p, uint32_t colour) {
                const render::ObjectExtent* e = build_extent(&gctx, p.object);
                if (e == nullptr) {
                    return;
                }
                // Turned with the part: an axis-aligned box around a rotated
                // wall would claim a footprint the wall does not have, and the
                // builder would trust the box over the rules.
                const float c = std::cos(p.yaw);
                const float sn = std::sin(p.yaw);
                const glm::vec2 corner[4] = {{e->lo.x, e->lo.y}, {e->hi.x, e->lo.y},
                                             {e->hi.x, e->hi.y}, {e->lo.x, e->hi.y}};
                glm::vec3 low[4];
                glm::vec3 high[4];
                for (int i = 0; i < 4; ++i) {
                    const glm::vec2 q = corner[i];
                    const glm::vec3 w{p.position.x + c * q.x + sn * q.y, 0.0f,
                                      p.position.z - sn * q.x + c * q.y};
                    low[i] = {w.x, p.position.y + e->bottom, w.z};
                    high[i] = {w.x, p.position.y + e->top, w.z};
                }
                for (int i = 0; i < 4; ++i) {
                    const int j = (i + 1) % 4;
                    renderer_->debug_line(low[i], low[j], colour);
                    renderer_->debug_line(high[i], high[j], colour);
                    renderer_->debug_line(low[i], high[i], colour);
                }
            };
            constexpr uint32_t OK_GREEN = 0xFF44FF44u;  // 0xAABBGGRR
            constexpr uint32_t NO_RED = 0xFF4444FFu;
            constexpr uint32_t DOOMED = 0xFF44AAFFu;    // orange: about to go
            if (build_ghost_.valid()) {
                world::Placement g;
                g.object = build_ghost_.object;
                g.position = build_ghost_.position;
                g.yaw = build_ghost_.yaw;
                outline(g, build_verdict_.allowed ? OK_GREEN : NO_RED);
            }
            if (build_target_ < scene_doc_.placements.size()) {
                outline(scene_doc_.placements[build_target_], DOOMED);
            }
        }
        // THE ZONE THE BRUSH IS ABOUT TO CHANGE (user, 18.08: «для кисти
        // объектов нужно добавить отрисовку той зоны, которую я буду изменять,
        // зелёным вверх строю, красным вниз»). Two rings: the RIM, where the
        // brush stops biting, and the flat top, where the falloff begins — so
        // the picture carries the same two numbers the stroke does.
        //
        // ITS GEOMETRY IS NOT COMPUTED HERE. brush_outline() bisects the rim
        // out of brush_weight(), the very function apply_brush() calls on every
        // sample; a circle drawn from radius_m here would ignore the minimum
        // radius clamp and promise a 40 cm brush while two metres of ground
        // moved. A ring that lies is worse than no ring at all.
        // КОЛЬЦО РИСУЕТСЯ ТОМУ, КТО ЕГО ПОПРОСИЛ. Здесь стоял вопрос «активен ли
        // один из двух режимов кисти» — то есть седьмое место, знавшее
        // перечисление. Инструмент отдаёт СВОЮ кисть, и кольцо считается по ней.
        const ToolAim ring_aim = mode_ == AppMode::Editor ? aim_this_frame() : ToolAim{};
        const TerrainBrush* ring_brush =
            mode_ == AppMode::Editor ? editor_ui_.toolbox().preview(ring_aim).ring_brush
                                     : nullptr;
        if (ring_brush != nullptr) {
            const glm::vec3 aim = ring_aim.point;
            // WHERE THE RING SITS WHEN THE GROUND IS NOT KNOWN, and it must not
            // be zero. A sample outside every resident chunk has no height, and
            // falling back to sea level drops that part of the ring tens of
            // metres below the crosshair — caught in the acceptance frame, where
            // the whole outline hung near the bottom of the screen while the aim
            // was in the middle of it. The honest fallback is the height of the
            // point the builder is actually aiming at: "the ground here is not
            // loaded, so the ring stays level with your aim".
            struct RingGround {
                world::ChunkManager* chunks;
                float fallback_y;
            } rg{&chunks_, aim.y};
            BrushGround ground;
            ground.ctx = &rg;
            // THE FINISHED GROUND, hand edits included: the ring has to hug the
            // surface the builder is looking at, not the one under his edits.
            ground.base_at = [](void* c, glm::vec2 xz) {
                auto* g = static_cast<RingGround*>(c);
                return g->chunks->height_at(xz).value_or(g->fallback_y);
            };
            const BrushOutline zone = brush_outline(*ring_brush, {aim.x, aim.z}, ground);
            const auto stroke_ring = [&](const std::vector<glm::vec3>& pts) {
                for (std::size_t i = 0; i + 1 < pts.size(); ++i) {
                    renderer_->debug_line(pts[i], pts[i + 1], zone.color_rgba);
                }
            };
            stroke_ring(zone.rim);
            stroke_ring(zone.core);
        }
        // ЛИНИЯ ТРОПЫ И ЕЁ УЗЛЫ. Спрашивается ОТВЕТ («что нарисовать»), а не
        // ярлык («не тропа ли в руке»): инструмент отдаёт ломаную, уже
        // положенную на землю, и код отрисовки о тропах ничего не знает.
        if (mode_ == AppMode::Editor) {
            const ToolPreview want = editor_ui_.toolbox().preview(ring_aim);
            const std::uint32_t col = want.line_color != 0 ? want.line_color : 0xFFFFFFFFu;
            if (want.polyline != nullptr) {
                for (std::size_t i = 0; i + 1 < want.polyline->size(); ++i) {
                    renderer_->debug_line((*want.polyline)[i], (*want.polyline)[i + 1],
                                          col);
                }
            }
            // ПРИЗРАК — СВОИМ ЦВЕТОМ И ПАРАМИ. Отдельная стопка появилась
            // после кадра пользователя, на котором жёлтых шариков оказалось
            // два: выбранный якорь и будущий, оба в стопке подсветки.
            if (want.ghost_pairs != nullptr) {
                const std::uint32_t gc = want.ghost_color != 0 ? want.ghost_color
                                                               : 0xFF60E080u;
                for (std::size_t i = 0; i + 1 < want.ghost_pairs->size(); i += 2) {
                    renderer_->debug_line((*want.ghost_pairs)[i],
                                          (*want.ghost_pairs)[i + 1], gc);
                }
            }
            if (want.handles != nullptr) {
                // ПАРАМИ: узлы приходят отрезками, а не точками, потому что
                // рисовальщик умеет линии и только их.
                for (std::size_t i = 0; i + 1 < want.handles->size(); i += 2) {
                    renderer_->debug_line((*want.handles)[i], (*want.handles)[i + 1], col);
                }
            }
            if (want.accent != nullptr) {
                // ВТОРАЯ СТОПКА, ВТОРОЙ ЦВЕТ — «что выбрано». Тот же формат пар и
                // тот же вызов; разделены они не по форме, а по смыслу, и одним
                // цветом на всё подсветка перестала бы быть ответом.
                const std::uint32_t acc =
                    want.accent_color != 0 ? want.accent_color : col;
                for (std::size_t i = 0; i + 1 < want.accent->size(); i += 2) {
                    renderer_->debug_line((*want.accent)[i], (*want.accent)[i + 1], acc);
                }
            }
        }
        if (collider_debug_) {
            constexpr uint32_t WIRE = 0xFF44FF44u; // 0xAABBGGRR: green
            // NEAR THE EYE ONLY, and with a budget. A town's colliders are
            // hundreds of thousands of lines; the transient buffer holds a
            // fraction of that, and asking for all of them drops ALL of them.
            // What a person debugging a collider needs is the shape he is
            // standing against, so the view shows that and says how much it
            // left out.
            constexpr float SHOW_RADIUS_M = 24.0f;
            constexpr std::size_t LINE_BUDGET = 12000;
            const glm::vec3 eye = camera_.interpolated_pose(alpha).position;
            std::size_t shown = 0;
            std::size_t skipped = 0;
            for (const DebugCollision& c : scene_collision_debug_) {
                for (std::size_t i = 0; i + 2 < c.indices.size(); i += 3) {
                    const glm::vec3& a = c.positions[c.indices[i]];
                    const glm::vec3& b = c.positions[c.indices[i + 1]];
                    const glm::vec3& d = c.positions[c.indices[i + 2]];
                    if (glm::distance(a, eye) > SHOW_RADIUS_M) {
                        ++skipped;
                        continue;
                    }
                    if (shown >= LINE_BUDGET) {
                        ++skipped;
                        continue;
                    }
                    // NUDGED TOWARD THE EYE by two centimetres. The collider
                    // is COINCIDENT with the surface it belongs to, so drawn
                    // exactly it loses the depth test to its own geometry and
                    // shows nothing — which is what the first two attempts at
                    // this view looked like, and which is indistinguishable
                    // from "the feature was never written".
                    const auto lift = [&eye](const glm::vec3& q) {
                        const glm::vec3 to = eye - q;
                        const float d2 = glm::length(to);
                        return d2 > 1e-4f ? q + to * (0.02f / d2) : q;
                    };
                    const glm::vec3 la = lift(a);
                    const glm::vec3 lb = lift(b);
                    const glm::vec3 ld = lift(d);
                    renderer_->debug_line(la, lb, WIRE);
                    renderer_->debug_line(lb, ld, WIRE);
                    renderer_->debug_line(ld, la, WIRE);
                    shown += 3;
                }
            }
            static bool told = false;
            if (!told) {
                told = true;
                std::fprintf(stderr, "[colliders] %zu triangle edge(s) drawn within "
                                     "%.0f m, %zu out of range or over budget\n",
                             shown / 3, static_cast<double>(SHOW_RADIUS_M), skipped);
            }
        }
        // THE EDITOR'S INTERFACE — THE ONE HOOK. Everything ImGui does happens
        // between these two calls: begin_frame pumps our platform input into it
        // and runs every open panel's draw callback; end_frame hands the lists
        // to the bgfx backend, which draws them inside the renderer's own
        // end_frame (below, in render_system_.render) — after the upscale, at
        // native resolution, and a second time into the capture target so the
        // panels appear on screenshots.
        //
        // A PANEL IS NEVER NAMED HERE. Panels register themselves through
        // EditorUi::add_panel, which is what lets three agents add tools to
        // this editor without three of them editing this file.
        editor_ui_.begin_frame(*input_, *window_, static_cast<float>(frame_dt));
        editor_ui_.end_frame();
        // ПАЛИТРА КОСТЕЙ ЭТОГО КАДРА (волна импорта и скиннинга, 30.08;
        // клипы и интерполяция — волна «клипы и текстуры», 31.08).
        //
        // ПОЗА ТЕПЕРЬ ИНТЕРПОЛИРУЕТСЯ МЕЖДУ ТИКАМИ, тем же alpha, которым
        // render интерполирует Transform (правило 12). До этой волны палитра
        // строилась по позе ТЕКУЩЕГО тика, и это было названо хвостом:
        // при 60 Гц не видно, при 30 видно. Всё, что знает про клип, про
        // прошлый тик и про дозу DFN_PROC_GAIT, живёт в SkinnedCharacter —
        // здесь остаётся ровно ферма alpha.
        // ДВА МЕСТА В СПИСКЕ, А НЕ ОДНО: второе — КЛИНОК В РУКЕ. Он едет на
        // ТОЙ ЖЕ палитре и ТОЙ ЖЕ матрице (SkinnedCharacter::blade_draw), то
        // есть на костях тела, поэтому меч не может отстать от кулака на кадр
        // — как и хитбоксы ниже, и по той же причине.
        std::array<render::RenderSystem::SkinnedDraw, 2> skinned_draws{};
        if (skinned_character_.ready()) {
            skinned_draws[0] = skinned_character_.build_draw(
                body_rig_, /*hide_head=*/!third_person_, alpha);
            const std::size_t count = skinned_character_.blade_drawn() ? 2u : 1u;
            if (count == 2) {
                skinned_draws[1] = skinned_character_.blade_draw(skinned_draws[0]);
            }
            render_system_.set_skinned_bodies(
                std::span<const render::RenderSystem::SkinnedDraw>{
                    skinned_draws.data(), count});
            // ХИТБОКСЫ ЧАСТЕЙ ТЕЛА ЕДУТ ЗА НАРИСОВАННОЙ ПОЗОЙ, и именно
            // здесь, а не в тике: тело рисуется по ИНТЕРПОЛИРОВАННОЙ позе, и
            // коробки, поставленные по позе тика, отставали бы от него ровно
            // на кадр — то есть на 12 см руки при беге. Матрица берётся ТА ЖЕ,
            // что и у меша (draw.transform), поэтому «во что целишься» и «во
            // что попадаешь» — одно место, а не два похожих.
            if (physics_ != nullptr) {
                if (!body_hitboxes_.live()) {
                    body_hitboxes_.create(*physics_, player_,
                                          skinned_character_.hitboxes(),
                                          skinned_character_.hitbox_pose(),
                                          skinned_draws[0].transform);
                } else {
                    body_hitboxes_.update(*physics_, skinned_character_.hitbox_pose(),
                                          skinned_draws[0].transform);
                }
            }
        }
        render_system_.render(world_, *renderer_, camera_, alpha);

        // ВЕСЬ ХВОСТ КАДРА — ОДНОЙ СТРОКОЙ (слой 4 разбора,
        // docs/audits/PLAN_APP_DECOMPOSITION.md). Здесь лежало 165 строк: запись
        // траектории, телеметрия, снимок, запись чата, отсчёт до закрытия,
        // отчёт восстановления, проба тела и затвор тура. Общее у них ровно
        // одно — они обязаны идти ПОСЛЕ render(), — и это единственная причина,
        // по которой они лежали вперемешку именно здесь. Довод записан в шапке
        // AppAfterFrame.cpp, а решения, которые они принимают, — в
        // AppAfterFrame.h, где их читает рукав app_after_frame.
        after_frame(alpha, static_cast<float>(frame_dt));
    }
    // STOP -> flush the telemetry ring beside the map (item 3). Empty in any run
    // that never entered the editor, which is not an error. The file sits next
    // to the map's chat, named `<map>.telemetry.log`, so a chat line can point
    // at it via its `trajectory` field.
    if (!telemetry_.empty()) {
        const std::string chat_path = chat_path_for_current_map();
        if (!chat_path.empty()) {
            const std::string suf = ".chat.jsonl";
            std::string tp = chat_path;
            if (tp.size() >= suf.size()
                && tp.compare(tp.size() - suf.size(), suf.size(), suf) == 0) {
                tp = tp.substr(0, tp.size() - suf.size()) + ".telemetry.log";
            } else {
                tp += ".telemetry.log";
            }
            (void)telemetry_.flush(tp);
        }
    }
    // Gate: a playtest run with incidents exits nonzero (Main passes it through).
    if (playtest_) {
        return playtest_->incidents.empty() ? 0 : 1;
    }
    return 0;
}

void App::shutdown() {
    // ИТОГ ПРИБОРА КАМЕРЫ — В shutdown(), А НЕ В КОНЦЕ run(): из run() есть
    // выход по кадру-снимку и по гейту прогулки, и строка, стоящая на одном из
    // них, у остальных не печатается вовсе.
    cam_probe_report();
    if (frame_log_ != nullptr) {
        std::fprintf(stderr, "[frame_log] %llu frames written\n",
                     static_cast<unsigned long long>(frame_log_index_));
        std::fclose(frame_log_);
        frame_log_ = nullptr;
    }
    // ВЕСЬ СНОС МИРА — ОДНОЙ СТРОКОЙ. Список жил здесь и НЕ жил в enter_world,
    // из-за чего второе открытие карты строило поверх первого (третий аудит,
    // пункт 1). Теперь он один на два вызова: AppWorld.cpp::unload_world().
    unload_world();
    if (renderer_) {
        // Before the renderer: the interface owns bgfx resources (its program,
        // its font atlas) that must be destroyed while bgfx is still up.
        editor_ui_.shutdown();
        // Герб меню — буфер вне мира: unload_world() его не видит, и без этой
        // строки прогон закрывался с «1 mesh handle still live».
        menu_emblem_.release(*renderer_);
        render_system_.shutdown(*renderer_);
        renderer_->shutdown();
    }
    if (audio_) {
        audio_->shutdown();
    }
    if (physics_) {
        physics_->shutdown();
    }
    if (window_) {
        window_->shutdown();
    }
}

} // namespace dfn::app
