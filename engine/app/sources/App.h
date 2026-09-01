/*
Module: engine/app
File: engine/app/sources/App.h

Responsibility:
- The composition root: owns every subsystem, runs the main loop (fixed-step
  simulation + interpolated render, Rule 12), ferries chunk events from world
  to render/physics (they are DAG siblings and cannot include each other).

Key items:
- AppConfig: backend selection + window/internal resolution (env-overridable).
- App: init() wires backends; run() is the loop; shutdown() tears down.

Dependencies:
- Uses: all platform interfaces, core (ecs/time/events), world, render, gameplay.
- Used by: main.cpp only (Rule 22).

Notes:
- The movement-system seam (sim zone) is integrated via gameplay's fixed-tick
  API; see App.cpp integration notes.
- Tour (Q51): when DFN_TOUR=1 the tour overrides the camera and the app closes
  after the last screenshot.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
*/

#pragma once

#include <chrono>

#include "engine/anim/sources/Rig.h"
#include "engine/app/sources/CharGen.h"
#include "engine/app/sources/ChatLog.h"
#include "engine/app/sources/ChatOverlay.h"
#include "engine/app/sources/BuildTool.h"
#include "engine/editor/sources/EditorPaletteView.h"
#include "engine/app/sources/AppAfterFrame.h"
#include "engine/app/sources/Controls.h"
#include "engine/editor/sources/EditorHistory.h"
#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/DoorAim.h"
#include "engine/app/sources/GrabDrive.h"
#include "engine/app/sources/PropPhysics.h"
#include "engine/app/sources/BodyHitboxes.h"
#include "engine/app/sources/SkinnedCharacter.h"
#include "engine/app/sources/FurnitureSeats.h"
#include "engine/app/sources/EditorCamera.h"
#include "engine/app/sources/EditorPlant.h"
#include "engine/editor/sources/EditorBrushView.h"
#include "engine/editor/sources/EditorPropsView.h"
#include "engine/editor/sources/EditorToolHouse.h"
#include "engine/editor/sources/EditorToolsBuiltin.h"
#include "engine/editor/sources/EditorUi.h"
#include "engine/app/sources/TrajectoryRecord.h"
#include "engine/app/sources/Menu.h"
#include "engine/app/sources/MenuEmblem.h"
#include "engine/app/sources/ModelViewer.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/core/events/sources/EventBus.h"
#include "engine/core/time/sources/FixedTimestep.h"
#include "engine/gameplay/sources/CameraBoom.h"
#include "engine/gameplay/sources/PlayerMovement.h"
#include "engine/gameplay/sources/InteractableMesh.h"
#include "engine/gameplay/sources/Interior.h"
#include "engine/gameplay/sources/PlaytestBot.h"
#include "engine/gameplay/sources/StepAudio.h"
#include "engine/gameplay/sources/WorldAmbience.h"
#include "engine/platform/audio/interfaces/IAudio.h"
#include "engine/platform/physics/interfaces/IPhysics.h"
#include "engine/render/sources/FirstPersonCamera.h"
#include "engine/render/sources/FloraFireflies.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/LoadingScreen.h"
#include "engine/render/sources/RenderSystem.h"
#include "engine/render/sources/Tour.h"
#include "engine/world/sources/ChunkManager.h"
#include "engine/world/sources/Scene.h"

#include <array>
#include <map>
#include <optional>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace dfn::platform {
class IWindow;
class IInput;
class IRenderer;
class IPhysics;
} // namespace dfn::platform

namespace dfn::app {

struct AppConfig {
    uint32_t window_width = 1280;
    uint32_t window_height = 720;
    uint32_t internal_width = 0;  // 0 = take dfn::config INTERNAL_RES_W
    uint32_t internal_height = 0; // 0 = take dfn::config INTERNAL_RES_H
    bool use_null_renderer = false;
    bool use_null_physics = false;
    bool use_null_audio = false;   // DFN_NULL_AUDIO=1
    bool show_menu = true;         // settings.cfg + DFN_MENU=0 for tooling:
                                   // the tour and the playtest bot must not
                                   // stop at a menu nobody can press Enter on
    uint32_t start_stand = 0;      // DFN_STAND: which demo map when the menu is off
    float head_bob = 1.0f;         // settings.cfg: 0 disables bob/dip/settle
                                   // MOTION (events and sound still fire) --
                                   // the research's motion-sickness mandate
    uint32_t msaa_samples = 4; // settings.cfg: coverage samples on the internal
                               // grid (0/1 off, 2, 4, 8). What stopped the
                               // treeline shimmer; DFN_MSAA overrides for tooling
    bool palette_post = false; // Q9b palette quantization (DFN_PALETTE=1)
    // settings.cfg fullscreen: the mode the window is BORN in. F11 toggles it
    // live and writes the answer back, so the choice survives the session that
    // made it — a fullscreen key that forgets is a key you press every launch.
    bool fullscreen = false;
    // settings.cfg min_brightness: the floor the final image never goes below
    // (0 = honest black). Turned live by the calibration screen. Default is one
    // palette shade step, because a default of zero would show the player, on
    // his first launch, exactly the darkness this setting exists to answer.
    float black_floor = static_cast<float>(config::BLACK_FLOOR_LEVEL);
    // settings.cfg music_volume / sfx_volume: the two mixer buses, as linear
    // multipliers (Rule 14). MUSIC IS NOT BORN AT 1.0 and effects are: the
    // theme is a mastered track that would sit on top of a footstep, and a
    // first launch where the menu drowns the game teaches the player to turn
    // sound off rather than down. Both are turned live on the settings page.
    float music_volume = 0.7f;
    float sfx_volume = 1.0f;
    // РЕЧЬ: шина заведена ВПЕРЁД голосов (заказ владельца). Единица, как у
    // эффектов — реплика это то, что игрок слушает, а не фон.
    float voice_volume = 1.0f;
    std::string title_key = "app.title"; // localization key (Rule 5)

    // Populates the fields above from settings.cfg (auto-generated with
    // comments on first run; user-editable graphics settings per sync #3),
    // then applies DFN_* environment overrides on top (tour/tooling):
    // DFN_INTERNAL_RES=WxH, DFN_PALETTE=1, DFN_NULL_RENDER=1, DFN_NULL_PHYSICS=1.
    static AppConfig from_env();
};

// unattended_run() ЖИВЁТ В AppDoors.h, вместе с таблицей дверей, из которой он
// и выводится. Здесь его объявления нет намеренно: второе объявление рядом с
// чужой таблицей — первый шаг ко второму ответу на тот же вопрос.
// Writes settings.cfg. Called from first run, from the calibration and
// settings pages, and from the fullscreen key -- which lives in AppInput.cpp
// since layer 1 of the App.cpp decomposition.
void write_settings(const AppConfig& cfg);

/// СУДЕЙСКИЙ КОНТЕКСТ РУКИ СТРОИТЕЛЯ. Жил в безымянном пространстве App.cpp,
/// пока его читал один файл; с 20.08 его читают ДВА файла реализации того же
/// класса (App.cpp и AppEditorWiring.cpp), и вторая копия разъехалась бы с
/// первой на первой же новой полке (правило 39). Объявление здесь, определение
/// build_extent — в App.cpp, рядом с остальными крючками судьи.
struct BuildJudgeCtx {
    const world::ChunkManager* chunks = nullptr;
    /// MUTABLE ON PURPOSE. The map keeps resident only what its composition
    /// already uses; the builder picks from the whole shelf, so the part he is
    /// holding has to be brought in on demand.
    std::map<std::string, render::RegistryObject>* objects = nullptr;
    std::map<std::string, render::ObjectExtent>* extents = nullptr;
    const std::vector<std::string>* shelves = nullptr;
};

/// Мерка детали: из кэша, иначе с полки (грузит .dfo по требованию). nullptr =
/// такой детали нет ни на одной полке.
const render::ObjectExtent* build_extent(void* ctx, const std::string& name);

class App {
public:
    App();
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    [[nodiscard]] bool init(const AppConfig& config);
    // Builds the world for one demo map. Called from init() when the menu is
    // off, or from the menu when the player picks a map.
    [[nodiscard]] bool enter_world(uint32_t stand);
    int run();
    void shutdown();

    // The map currently loaded (whichever .map the browser opened, or the door
    // resolved). Carries category + file_stem + zone, from which a consumer
    // derives sibling paths -- the chat log lives at
    // assets/maps/<category>/<file_stem>.chat.jsonl (Rule 26 seam for the chat
    // zone). nullptr before any map is opened.
    [[nodiscard]] const MapManifest* current_manifest() const {
        return current_map_ ? &*current_map_ : nullptr;
    }

private:
    void pump_chunk_events(); // ferry ChunkLoaded/Unloaded -> render + physics

    // Menu-first launch: the engine is up but no world exists until a map is
    // chosen. Playing ticks the sim and drives the camera from the player's
    // CameraPose. Editor still ticks the sim (so streaming, sky and the body
    // keep living) but withholds the player's input and drives the camera from
    // a free EditorCamera instead -- a flying eye detached from the body.
    enum class AppMode : uint8_t { Menu, Playing, Editor };
    AppMode mode_ = AppMode::Playing;
    // Where Escape returns to when it opens the pause page: Playing or Editor.
    // Without it Resume always dropped back into Playing, so pausing the editor
    // and resuming would silently possess the body.
    AppMode paused_from_ = AppMode::Playing;
    /// СЕССИЯ РЕДАКТОРА — СВОЙСТВО ЗАПУСКА МИРА, А НЕ СОСТОЯНИЕ (заказ владельца
    /// 27.08: «в обычной игре могу войти в режим редактуры и строить, хотя
    /// должен это уметь только в игровом режиме редактора»).
    ///
    /// ПОЧЕМУ НЕ mode_. mode_ отвечает на вопрос «где сейчас камера» и умеет
    /// меняться клавишей; вопрос владельца — другой: «имеет ли этот мир вообще
    /// право редактироваться». Пока ответ жил в mode_, «обычная игра» и
    /// «редактор» отличались одним нажатием ` — то есть не отличались. Значение
    /// ставится ровно в одном месте (вход в мир) и дальше только читается:
    /// правó строить приезжает из меню вместе с картой и не переключается.
    ///
    /// Ложь по умолчанию: мир, про который никто не сказал «редактор», —
    /// игровой. Обратное умолчание отдаёт инструменты каждому пути, который
    /// забыли назвать, а это ровно то, что чиним.
    bool editor_session_ = false;
    /// Момент, с которого пошли СТЕННЫЕ часы заставки. Пара к menu_.time():
    /// расхождение этих двух чисел и было дефектом «заставка ждёт клика», а
    /// величину, которую меришь, нельзя мерить ею же самой.
    std::chrono::steady_clock::time_point splash_started_{};
    /// Единственная точка, где право строить назначается. Держит вместе три
    /// вещи, которые обязаны совпадать всегда: сам флаг, видимость панелей
    /// ImGui и жалобу в stderr — беззвучный вход не отличить от отказа.
    void set_editor_session(bool on, const char* why);
    // FREE CAMERA of the editor mode. Driven directly by the app each render
    // frame; never interpolated (the app owns the pose outright). Seeded from
    // the player eye on entry so the toggle in and out of the body is seamless.
    EditorCamera editor_cam_;
    /// The editor's ImGui frame. Panels (object menu, terrain brushes, the
    /// properties column) register themselves with it; this file names none of
    /// them on purpose — see the hook in run().
    EditorUi editor_ui_;
    // Enters the editor: seeds the free camera from the player's current eye
    // and switches mode. become_player_from_editor() does the reverse -- it
    // teleports the body's feet under the free camera and hands control back to
    // the Playing controller (the user's В39/Л1: "and the fly-over, and out of
    // the eyes, in the same field").
    void enter_editor_mode();
    void become_player_from_editor();
    // Resolves a browser-chosen .map to a world and builds it (source
    // stand:<id> -> the generator stand; source dfw:<file> -> the baked map,
    // which does not exist until the baker lands -- an honest on-screen status,
    // never a silent nothing, per docs/MAP_LAYOUT.md). Returns true when a
    // world was built; false leaves a browser_status for the player and stays
    // in the menu.
    [[nodiscard]] bool open_map(const MapManifest& manifest);
    MenuModel menu_;
    // The map browser's catalog, scanned from assets/maps at startup and
    // handed to the menu (which only reads it). App owns it; the menu borrows.
    MapCatalog catalog_;
    // The map that was actually opened (a copy of the chosen manifest), exposed
    // through current_manifest() for the chat zone's path derivation.
    std::optional<MapManifest> current_map_;
    /// ЧЕЛОВЕЧЕСКОЕ ИМЯ КАРТЫ, КОТОРАЯ ГРУЗИТСЯ СЕЙЧАС — заголовок экрана
    /// загрузки. Отдельное поле, а не current_map_: та присваивается ПОСЛЕ
    /// удачной постройки мира (она отвечает на «какая карта открыта», и
    /// присвоить её раньше значило бы называть открытой карту, которая может
    /// не собраться), а заголовок нужен ДО первого кадра экрана.
    std::string loading_map_name_;
    uint32_t active_stand_ = 0;
    int menu_shot_frames_ = 0; // DFN_MENU_SHOT flush counter
    /// ОБЪЁМНЫЙ ГЕРБ ГЛАВНОГО МЕНЮ (заказ владельца 27.08). Меш заливается
    /// один раз при первом показе корня и живёт до конца прогона: 214 тыс.
    /// треугольников дешевле держать, чем перезаливать на каждом входе.
    MenuEmblem menu_emblem_;
    /// Два источника кадра меню — свои, не мировые (см. light_menu_screen).
    std::vector<render::RenderSystem::ExtraLight> menu_lights_;
    /// Замер цены кадра меню (DFN_MENU_COST): длительности render() в мс.
    std::vector<float> menu_cost_ms_;
    // WHERE THE POINTER WAS ON THE PREVIOUS MENU FRAME. The menu takes both the
    // arrows and the mouse (owner, 26.08), and hover may only move the
    // selection when the pointer ACTUALLY MOVED -- otherwise a hand resting on
    // the mouse pulls the selection back under the cursor every frame and the
    // arrow keys read as broken. Starts off-screen so the first real position
    // counts as a move.
    glm::vec2 menu_cursor_{-1.0f, -1.0f};
    void body_probe_drive();  // fixed tick: pose the camera for the probe
    void body_probe_frame(float alpha, float frame_dt); // after render: shoot

    // A KEY EDGE BY ACTION, resolved through the binding table (Controls.h)
    // instead of by naming a key here. That indirection is what keeps the
    // controls screen from drifting: a handler names what it DOES, the table
    // says which key does it, and the screen draws the same table.
    [[nodiscard]] bool action_pressed(Action action) const;

    // ЕДИНСТВЕННОЕ МЕСТО, ГДЕ КЛАВИША ВООБЩЕ ДОХОДИТ ДО ПРИЛОЖЕНИЯ (слой 1
    // разбора App.cpp, docs/audits/PLAN_APP_DECOMPOSITION.md). Обходит таблицу
    // AppActions.h и зовёт метод, названный в её строке. Возвращает false,
    // когда кадр надо бросить (ESC увёл в меню паузы) — раньше на этом месте
    // стоял `continue` посреди тысячи строк.
    //
    // ЗАЧЕМ ВООБЩЕ ОДНО МЕСТО. До сегодня обработчиков было восемнадцать, и
    // каждый сам писал перед собой `!chat_typing &&`. Такой уговор соблюдают
    // все, пока не появится девятнадцатый; а цена ошибки — набранное в чате
    // слово, которое роняет снимки и вертит камеру. Теперь запрет — колонка
    // таблицы, и её читает рукав app_controls.
    [[nodiscard]] bool dispatch_actions(bool chat_typing);
    // ВЕСЬ ХВОСТ КАДРА, и у него ровно одна общая причина существовать там,
    // где он существует: всё это обязано идти ПОСЛЕ render(). Определён в
    // AppAfterFrame.cpp, довод — в шапке того файла.
    void after_frame(float alpha, float frame_dt);
    // Обработчики, названные строками таблицы. Определены в AppInput.cpp; имя
    // метода И ЕСТЬ поле `handler` в строке, и рукав держит их вместе.
    // --- СМОТРОВАЯ. Определены в AppViewer.cpp -----------------------------
    /// Открыта ли смотровая. Один вопрос, много читателей (камера, ввод, HUD,
    /// подавление фигуры игрока) — и ровно поэтому он метод, а не сравнение
    /// строк карты, переписанное в четырёх местах.
    [[nodiscard]] bool viewer_active() const { return viewer_mode_; }
    /// Собрать список моделей и показать первую (или названную дверью).
    /// Зовётся из enter_world, когда манифест карты — смотровая.
    void viewer_enter();
    /// Снять модель с постамента и погасить режим.
    void viewer_leave();
    /// Показать модель №index: преобразовать при надобности, прочитать .dfo,
    /// собрать потоки и залить их ОДНИМ ключом россыпи (старый меш при этом
    /// уничтожается — пара на пару).
    void viewer_show(int index);
    /// Мышь и колесо в облёт; зовётся из кадрового цикла.
    void viewer_mouse();
    /// Поза камеры этого кадра. Возвращает false, если смотровая закрыта.
    [[nodiscard]] bool viewer_camera_pose(components::CameraPose& out) const;
    /// Подписи смотровой на холст HUD. Возвращает, легло ли на него что-нибудь.
    [[nodiscard]] bool viewer_draw(render::PixelCanvas& hud);
    void on_viewer_cycle();
    void on_viewer_turn();
    void on_viewer_reset();

    // --- ЭКРАН СОЗДАНИЯ ПЕРСОНАЖА (AppCharGen.cpp) ------------------------
    /// Открыт ли экран. Один вопрос, три читателя (ввод меню, отрисовка меню,
    /// кадр меню) — и потому метод, а не сравнение страницы в трёх местах.
    [[nodiscard]] bool chargen_active() const { return chargen_open_; }
    /// Загрузить тело, собрать ручки из его секции MORF, поднять пресет, если
    /// он уже есть. Тихо ничего не делает, если тело не читается: экран без
    /// фигуры показывать нечестно, и вместо него остаётся главное меню.
    void chargen_enter();
    /// Построить риг тела, если он ещё не построен. Довод — у определения
    /// (AppWorld.cpp): экран создания открывается там, где мира нет.
    void ensure_body_rig();
    /// Снять тело с видеокарты и погасить экран. Пара к enter по построению.
    void chargen_leave();
    /// Весь кадр экрана: ввод (клавиши, мышь, колесо, буквы), перекладка меша
    /// на движение ручки, отрисовка холста. Возвращает false, когда экран
    /// закрылся в этом же кадре.
    bool chargen_frame(int hud_w, int hud_h, int mx, int my, bool pointer_moved);
    /// Довести ОДНУ сдвинутую строку до тела: вес — в бленд, рост — в
    /// множитель кадра. Возвращает true, если меш надо перекладывать.
    bool chargen_apply_row(std::size_t row_index);
    /// То же ПО ИМЕНИ строки: так его зовут доза и подъём пресета.
    bool chargen_push_to_body(const std::string& name);
    /// «Готово»: пресет на диск, тело выпечено, статус на экран.
    void chargen_commit();
    /// Свет и фигура этого кадра — то же устройство, что у герба меню.
    void chargen_screen_prop();

    void on_third_person();
    // ДОЗА щупа камеры: DFN_CAM_COLLIDE=0 снимает коллизию, ничего больше не
    // меняя, — контрольная рука приёмки из ТОГО ЖЕ бинарника (Rule 47).
    [[nodiscard]] bool cam_collide_enabled() const;
    // Прибор третьего лица (DFN_CAM_PROBE). Мерит НЕ ту величину, которой
    // управляет стрела: луч от головы к получившейся точке камеры. Попал во
    // что-то раньше, чем дошёл, — камера за оболочкой, и это засчитано.
    void cam_probe_step(const glm::vec3& head, const glm::vec3& cam, float length,
                        const gameplay::CameraBoomAim& aim);
    void cam_probe_report() const;
    void on_debug_readout();
    void on_state_capture();
    void on_wireframe();
    void on_screenshot();
    void on_toggle_body();
    void on_weapon_toggle();
    /// ПЕРЕБОР ПОЗ РЕЕСТРА (AppPoses.cpp). Направление берётся у НАЖАТОЙ
    /// СКОБКИ: строка таблицы одна на обе клавиши, и обработчик — единственное
    /// место, где известно, какая из них под пальцем.
    void on_pose_cycle();
    void step_pose(int delta);
    /// ПОЗА С НАЧАЛА ПРОГОНА (DFN_POSE=<имя записи реестра>). Читается один
    /// раз при появлении тела: беспилотный прогон обязан снять кадр позы, не
    /// нажав ни одной клавиши.
    void apply_pose_dose();
    /// ЛЕНТА ПОЗ (DFN_POSE_TAPE=1): перебор всех записей реестра по своим
    /// часам, по одному слоту на POSE_TAPE_DWELL_S. Ставится ТОЙ ЖЕ функцией,
    /// что и клавиша, — лента обязана показывать переходы, а не свою копию их.
    void tick_pose_tape(float dt);
    void set_pose_slot(uint32_t slot);
    void on_trajectory_record();
    void on_trajectory_replay();
    void on_chat_window();
    void on_quick_remark();
    void on_map();
    void on_menu_pause();
    /// ЗАГЛАВНАЯ ТЕМА: ОДНО МЕСТО, ГДЕ РЕШАЕТСЯ, ИГРАЕТ ЛИ ОНА. Зовётся каждый
    /// кадр и приводит звук к тому, чего требует нынешний экран, — а не
    /// расставляется вызовами start/stop по шести переходам (пункт меню, пауза,
    /// возврат в корень, сброс сессии, выход из редактора, закрытие карты).
    /// Шесть точек — это шесть шансов забыть одну; забытая даёт музыку поверх
    /// игры, и найдут её ушами, а не сборкой (правило 32).
    void update_menu_music();
    /// ЗВУК МИРА СЛЫШЕН, ПОКА МИР ИДЁТ. Считает и ведёт приглушение шины мира;
    /// это же место — единственный ответ на вопрос «почему в меню тихо».
    void update_world_audio();
    /// РОСЧЕРК ЗАСТАВКИ: запуск на первом кадре интро, гашение на пропуске и на
    /// уходе в мир, и НИЧЕГО на естественном конце — он длиннее видео нарочно.
    void update_intro_sting();
    /// Начать гашение росчерка за `seconds`. Повторный вызов, пока идёт
    /// гашение, игнорируется: два наложенных пандуса — это скачок громкости.
    void fade_intro_sting(float seconds);
    /// ЗВУЧИТ ЛИ МЕНЮ ВООБЩЕ — один ответ на тему и на росчерк. Две копии
    /// правила «в счётных прогонах тихо» разошлись бы в первый же день, когда
    /// кто-нибудь заведёт третий звук меню (правило 32).
    [[nodiscard]] bool menu_audio_allowed() const;
    /// Громкость двух шин из черновика страницы настроек — ЖИВЬЁМ. Отдельно от
    /// SettingsDone нарочно: SettingsDone случается на ВЫХОДЕ со страницы, а
    /// громкость — единственная настройка, которую нельзя выбрать глазами.
    void sync_audio_volumes();
    void on_fullscreen();
    void on_cursor_toggle();
    void on_build_menu();
    void on_build_rotate();
    void on_undo_redo();
    void on_axis_lock();
    void on_delete_selected();
    void on_grid_toggle();
    /// Шаг стрелками по выбранному якорю; false — стрелки не нажаты.
    bool nudge_selected_anchor();
    /// Отсечки сетки вокруг прицела (только когда сетка включена).
    void draw_editor_grid(const ToolAim& aim);
    /// Пересчитать тело постройки и отдать его в отрисовку.
    /// `interior_only` — перезалить ТОЛЬКО интерьерный слот (И15): вход в
    /// дом не имеет права перестраивать город, а 1087 построек Вайтрана —
    /// это секунды против полусекунды, отпущенной своду на вход.
    void upload_house_mesh(bool interior_only = false);
    /// ГОТОВЫЕ ПОСТРОЙКИ КАРТЫ: секция [house] сцены — граф из .dfh + место.
    /// Читаются на входе в мир, вливаются в те же потоки и коллайдер, что и
    /// строящийся дом (одна история для картинки и физики).
    void load_scene_houses();
    struct PlacedHouse {
        world::HouseGraph graph;
        glm::vec3 pos{0.0f};
        float yaw = 0.0f;
        /// Индекс СВОЕЙ записи в scene_doc_.houses. Списки не параллельны:
        /// непрочитанный .dfh пропускается загрузкой, и поиск по индексу
        /// сцены распаковывал СОСЕДНИЙ дом (аудит #3, находка 1).
        std::size_t scene_index = 0;
    };
    std::vector<PlacedHouse> placed_houses_;

    // ---- И15: ИНТЕРЬЕРЫ-ЛОКАЦИИ (docs/plans/INTERIORS_I15.md, волна А) ----
    // Город НЕ выгружается на входе в дом: он подвешивается (render не рисует
    // экстерьер, chunks_.update заморожен, его тела не трогаются), а интерьер
    // строится в КАРМАНЕ на километр ниже. Карман по Y, а не по XZ, потому
    // что ключ ячейки пакетирования различает XZ лишь в полосе -256..1792 м.

    /// Постройки открытой локации (обычно одна: тот же .dfh, что снаружи).
    std::vector<PlacedHouse> interior_houses_;
    /// Вершины интерьерного коллайдера. Поле по той же причине, что и у
    /// house_positions_: дескриптор физики берёт их СПАНОМ.
    std::vector<glm::vec3> interior_positions_;
    platform::PhysicsBodyHandle interior_body_{};
    /// СТРАХОВОЧНАЯ ПЛИТА под карманом: пол интерьера — обычная геометрия, а
    /// дырка в ней при -1000 м означала бы падение без дна и без диагноза.
    platform::PhysicsBodyHandle interior_plate_{};
    /// Композиция открытой локации (её [air], [light], [spawn], [portal]).
    world::SceneDoc interior_doc_;
    /// Какая локация СЕЙЧАС ЗАЛИТА в слот. Не то же самое, что «где игрок»:
    /// на выходе геометрия остаётся резидентной, и повторный вход в тот же
    /// дом стоит переключения флага. Ради этого выход укладывается в 0.05 с.
    std::string interior_resident_;
    /// Начало кармана (centre_x, -1000, centre_z).
    glm::vec3 interior_pocket_{0.0f, -1000.0f, 0.0f};
    /// Лампы ГОРОДА, снятые на время интерьера и возвращаемые на выходе.
    std::vector<render::RenderSystem::ExtraLight> city_lights_;
    bool city_lights_saved_ = false;
    /// Солнце и ambient города — та же пара «снять и вернуть».
    glm::vec3 city_sun_color_{0.0f};
    glm::vec3 city_ambient_{0.0f};
    bool city_sky_saved_ = false;
    /// ЧТО ЭТА ДВЕРЬ ЗА ДВЕРЬ (заказ владельца 27.08: «куда переносит игрока —
    /// свойство дверей должно быть чёткое: это порталы. Также должны быть и
    /// декоративные двери, что будут просто открываться, межкомнатные»).
    /// Тип — СВОЙСТВО СТВОРКИ, выведенное при постройке, а не догадка места
    /// нажатия: одна и та же клавиша E на трёх типах обязана делать три
    /// РАЗНЫЕ вещи, и решать это по наличию строки `interior` значило бы
    /// хранить тип в его же последствии.
    enum class DoorwayKind : std::uint8_t {
        Portal,     ///< ведёт в локацию: экран загрузки, телепорт
        Locked,     ///< запечатана, внутренности нет: honest «Заперто»
        Decorative, ///< просто открывается: поворот полотна, без загрузки
    };
    /// Переход как ВЕЩЬ МИРА: сущность-взаимодействие на каждый [portal].
    struct PortalLink {
        std::uint64_t action = 0;   ///< хеш действия Usable
        std::size_t index = 0;      ///< номер в portals текущей композиции
        bool interior = false;      ///< портал принадлежит локации, не городу
        /// ПЕРЕХОД У СТВОРКИ ПОСТРОЙКИ (И15 волна Б), а не из [portal] сцены.
        /// Он не адресует запись композиции: цель лежит прямо здесь, потому
        /// что её ИСТОЧНИК — геометрия дверного полотна, которую знает только
        /// заливка построек. Пустой `to` — ЗАПЕРТО: дверь есть, внутренности
        /// у неё нет, и игрок обязан услышать это, а не жать в пустоту.
        bool house = false;
        std::string to;
        std::string to_spawn;
        ecs::EntityId entity{};
        DoorwayKind kind = DoorwayKind::Portal;
        /// НОМЕР ПОЛОТНА В ГОРОДСКОМ СПИСКЕ ДВЕРЕЙ render'а. Нужен только
        /// декоративной двери: открыть её — значит повернуть ЕЁ меш, а не
        /// «какую-нибудь дверь». Список заполняет та же заливка, что и
        /// створки, одним проходом — второй счёт разошёлся бы с первым.
        std::size_t door_index = 0;
        /// ОБРАТНЫЙ АДРЕС ПЕРЕХОДА (владелец 27.08: «выход из двери должен
        /// ставить игрока СПИНОЙ К ДВЕРИ, из которой он вышел, рядом с тем
        /// домом, у которого был — не в какую-то рандомную точку»). Это
        /// ДАННЫЕ ДВЕРИ, а не поза игрока: поза — то, где человек стоял в
        /// момент нажатия, и она законно бывает любой (подошёл боком, прыгал,
        /// открыл дверь с крыльца соседа). Точка выведена из полотна:
        /// середина створки плюс шаг по её наружной нормали.
        glm::vec3 back_at{0.0f};
        float back_yaw = 0.0f;
        bool back_set = false;
        /// ОТКРЫТА ЛИ ДЕКОРАТИВНАЯ СТВОРКА. Состояние живёт здесь, а не у
        /// меша: меш перезаливается на каждой правке дома, а «эта дверь
        /// открыта» — свойство мира.
        bool open = false;
        /// ПРИЦЕЛ ЭТОЙ ДВЕРИ: прямоугольник полотна и радиус руки. Им же
        /// ставится коробка взаимодействия, и им же решается, целится ли
        /// игрок в створку прямо сейчас, — один ответ на оба вопроса
        /// (см. DoorAim.h). Крит владельца 28.08: «дверь ловит нажатие по
        /// радиусу от неё, а не по радиусу + взгляд на дверь».
        DoorAim aim;
    };
    /// Куда портал возвращает — пара «точка и взгляд», собранная у двери.
    struct PortalReturn {
        glm::vec3 at{0.0f};
        float yaw = 0.0f;
        bool set = false;
    };
    std::vector<PortalLink> portals_;
    /// СТВОРКА ПОСТРОЙКИ КАРТЫ КАК ВХОД (И15 волна Б, «дома болванками»).
    /// Собирается заливкой построек: только она считает мировые координаты
    /// дверного полотна, и второй счёт (в генераторе, строкой [portal]) был бы
    /// вторым ответом на вопрос «где дверь этого дома» — правило 39. Поэтому в
    /// сцене города НЕТ ни одной строки перехода: там стоит только `interior=`
    /// у постройки, а дверь находит движок.
    struct HouseDoorway {
        glm::vec3 at{0.0f};        ///< середина габарита створки, мировые
        /// НАРУЖНАЯ НОРМАЛЬ ПОЛОТНА (единичная, по XZ). Считается из тех же
        /// треугольников створки, что и `at`: сторона выбирается по вектору
        /// «центр дома → дверь», и потому не зависит ни от порядка обхода
        /// рецепта, ни от поворота размещения. Ею определяется, где игрок
        /// окажется, ВЫЙДЯ обратно, — а это, по слову владельца, обязано быть
        /// свойством двери, а не случайностью позы.
        glm::vec3 out_normal{0.0f, 0.0f, 1.0f};
        /// ГАБАРИТ ПОЛОТНА, половинами: поперёк нормали и по высоте. Считается
        /// из тех же треугольников створки, что `at` и нормаль, и существует
        /// затем, что прицел двери — это ПРЯМОУГОЛЬНИК СТВОРКИ, а не шар
        /// вокруг её середины (крит владельца 28.08). Без габарита прицел
        /// нечем очертить, и он вырождается в радиус.
        float half_w = 0.5f;
        float half_h = 1.0f;
        float reach_m = 1.0f;      ///< с какого расстояния берётся рукой
        std::size_t scene_index = 0;
        std::size_t door_index = 0; ///< номер полотна в городском списке дверей
        DoorwayKind kind = DoorwayKind::Locked;
        std::string interior;      ///< пусто — заперто либо декоративная
    };
    std::vector<HouseDoorway> house_doorways_;
    /// СТВОРКИ ОТКРЫТОЙ ЛОКАЦИИ. Тот же сбор, что у города, только приёмник
    /// другой: обратный [portal] композиции — точка без нормали и без
    /// габарита, а прицелиться в дверь изнутри можно только по её ПОЛОТНУ.
    /// Оболочка локации — тот же .dfh, что стоит в городе, поэтому полотно
    /// здесь настоящее, а не выведенное из точки перехода.
    std::vector<HouseDoorway> interior_doorways_;
    /// Заводит сущности переходов по створкам построек карты.
    void spawn_house_portals();
    /// ПРИЦЕЛ ПО СТВОРКЕ: прямоугольник полотна плюс радиус руки.
    [[nodiscard]] static DoorAim doorway_aim(const HouseDoorway& d);
    /// ДОЗА ПРИЦЕЛА (DFN_DOOR_AIM): 1 — радиус И взгляд; 0 — прежний прицел
    /// бит-в-бит, куб вокруг середины створки без проверки взгляда.
    [[nodiscard]] static bool door_aim_enabled();
    /// ДОЗА СВЕТА ИНТЕРЬЕРА (DFN_INTERIOR_LIGHT), 0..1. 0 — прежний кадр
    /// бит-в-бит; 1 (умолчание) — общий свет комнаты по часу суток и
    /// поправка огней по их роли (крит владельца 28.08 «в домах слишком
    /// темно»). Разбор чисел — в шапке блока в AppInterior.cpp.
    [[nodiscard]] static float interior_light_dose();
    /// ДОЗА СВЕТА ИЗ ОКОН (DFN_INTERIOR_WINDOW), 0..1. 0 — прежний кадр
    /// бит-в-бит: вставки глухие, запечённая видимость окон не знает.
    /// 1 (умолчание) — оконный лист светится дневным небом и поднимает
    /// видимость у проёма (крит отчёта interior-light-28-08 §7 п.1-2).
    ///
    /// ОТДЕЛЬНАЯ ОТ DFN_INTERIOR_LIGHT, И ЭТО НЕ УДОБСТВО. Та доза живёт
    /// целиком в enter_interior (ambient и огни ставятся на входе), а эта
    /// достаёт до ПЕЧКИ ВИДИМОСТИ и до отпечатка её кэша — то есть до
    /// величины, которая переживает вход и ложится на диск. Одна доза на две
    /// такие разные вещи означала бы, что «контрольная рука света» молча
    /// перепекает геометрию.
    [[nodiscard]] static float interior_window_dose();
    /// ДОЛЯ ДНЯ ДЛЯ СВЕТА ЛОКАЦИИ, 0 (ночь) .. 1 (день). ОДИН ХОЗЯИН
    /// (правило 35): её читают и общий свет комнаты (AppInterior), и сила
    /// свечения оконных вставок (AppHouse), и разойтись им нельзя — окно,
    /// горящее полднем в комнате с ночным ambient, это не полутон, а брак.
    /// Считается из game_seconds_ той же render::sun_direction_at, что и
    /// петля кадра; окружение подвешенного мира спрашивать нельзя (у
    /// локации нет неба, apply_sky_time при подвесе не зовётся).
    [[nodiscard]] float interior_day_factor() const;
    /// СВЕЧЕНИЕ ОКОННОЙ ВСТАВКИ на текущий час суток: тон (вершинным цветом
    /// листа) и сила (величиной эмиссии). Числа и их вывод — в блоке «СВЕТ
    /// ИНТЕРЬЕРА» в AppInterior.cpp, рядом с тоном общего света, потому что
    /// они об одном и том же небе за стеной и обязаны спорить друг с другом
    /// в одном месте, а не в двух файлах.
    struct PaneGlow {
        glm::vec3 tint{1.0f};
        float strength = 0.0f; ///< 0 — вставка не светится (доза 0).
    };
    [[nodiscard]] PaneGlow interior_pane_glow() const;
    /// ЦЕЛИТСЯ ЛИ ИГРОК В ЭТУ ДВЕРЬ ПРЯМО СЕЙЧАС. Радиус И взгляд, один ответ
    /// на подсказку и на клавишу.
    [[nodiscard]] DoorAimHit door_aim_now(const PortalLink& link) const;
    /// СНИМАЕТ ПОДСКАЗКУ ДВЕРИ, В КОТОРУЮ НЕ ЦЕЛЯТСЯ. Зовётся сразу после
    /// gameplay::update_hover и до gameplay::player_actions_step: обе стороны
    /// (надпись на экране и приём E) читают ОДИН HoverTarget, поэтому гасить
    /// его — единственный способ не дать им разойтись.
    void filter_door_hover();
    /// ПРИБОР ПРИЦЕЛА (DFN_DOOR_AIM_PROBE): по каждой створке карты четыре
    /// руки — смотрю в полотно, смотрю на стену рядом, стою спиной, стою
    /// далеко, — и ответ настоящего луча по настоящим телам.
    void probe_door_aim();
    /// Открывает/закрывает ДЕКОРАТИВНУЮ створку: поворот полотна на петле,
    /// без загрузки и без телепорта.
    void toggle_decorative_door(const PortalLink& link);
    /// Открывает ВСЕ декоративные створки карты (дверь DFN_DOOR_OPEN): иначе
    /// поворот полотна не попадает ни на один беспилотный кадр.
    void open_decorative_doors();
    events::SubscriptionId used_sub_{};

    // ---- СИДЕТЬ И ЛЕЖАТЬ (обязательство эпохи, заказ владельца 28.08) ------
    // «На стулья необходимо добавить возможность садиться, на кровати
    // ложиться». Механика живёт ЗДЕСЬ, а не в gameplay, ровно по той же
    // причине, что и переходы: точки позы выводятся из ГЕОМЕТРИИ залитой
    // локации, и знает её только этот слой (AppSeats.cpp).

    /// ТОЧКА ПОЗЫ КАК ВЕЩЬ МИРА: сущность с подсказкой и телом прицела плюс
    /// сама точка. Пара «действие -> точка», как у PortalLink, и по той же
    /// причине: событие Used несёт хеш действия, а не сущность.
    struct SeatLink {
        std::uint64_t action = 0;
        ecs::EntityId entity{};
        FurnitureSpot spot;
    };
    std::vector<SeatLink> seats_;
    /// ЗАЯВКА НА ПОЗУ, поданная обработчиком Used и исполняемая ПОСЛЕ раздачи
    /// событий — тот же довод, что у pending_portal_. 0 — заявки нет.
    std::uint64_t pending_seat_ = 0;
    /// НОМЕР ЗАНЯТОЙ ТОЧКИ в seats_, либо -1. Номер, а не указатель: список
    /// перетряхивается при входе в другую локацию.
    int active_seat_ = -1;
    /// К КАКОЙ ТОЧКЕ СЕЙЧАС ИДЁМ, либо -1 (заказ владельца 28.08, пункт 4:
    /// «сесть/лечь начинается с ходьбы к точке старта»). Между нажатием E и
    /// позой встал ПОДХОД: пока это поле не -1, приложение пишет игроку
    /// ходьбу и доворот ТЕМИ ЖЕ намерениями, какими их пишет клавиатура
    /// (move_axes/yaw), и только дойдя — ставит позу.
    int approach_seat_ = -1;
    /// Куда идём и каким рыском встаём (posture_start от меты точки).
    PostureStart approach_start_{};
    /// Сколько уже идём, с: предохранитель от застрявшего (см.
    /// SEAT_APPROACH_TIMEOUT_S).
    float approach_s_ = 0.0f;
    /// Обратный отсчёт до следующей строки следа подхода (DFN_POSTURE_TRACE):
    /// строка на тик утопила бы след, а строка в секунду показывает, ИДЁТ ли
    /// человек, — а именно это и нужно, когда он не идёт.
    int approach_trace_left_ = 0;
    /// СКОЛЬКО ВРЕМЕНИ ЧЕЛОВЕК НЕ ПРИБЛИЖАЕТСЯ К ТОЧКЕ, с, и КАК БЛИЗКО он к
    /// ней подходил. Пара, которой распознаётся «упёрся»: у лавки за столом
    /// точка старта лежит в столе, и место, дальше которого не пускают, и есть
    /// место, с которого на эту лавку садятся (см. SEAT_STALL_S).
    float approach_stall_s_ = 0.0f;
    float approach_best_m_ = 0.0f;
    /// Сказано ли уже вслух, что упёрся: строка на тик утопила бы лог.
    bool approach_stall_said_ = false;
    /// ЧЬЁ СИДЕНЬЕ ДЕРЖИТ ПЕРЕКРЕСТЬЕ, ПОКА КЛАВИША ЛЕЖИТ, либо -1. Короткое
    /// нажатие E приложение возвращает НЕ В ТОТ ТИК, В КОТОРОМ НАЖАЛИ, а в
    /// тот, в котором ОТПУСТИЛИ (AppProps::grab_input делит E на короткое и
    /// долгое), — и всё это время рука на мыши продолжает уводить прицел.
    /// Прицел, замороженный на время лежания клавиши, и есть починка «садится
    /// не каждый раз»: см. filter_seat_hover.
    int aim_held_seat_ = -1;
    /// Лежала ли клавиша В ПРОШЛОМ ТИКЕ. Нужна затем, что защёлка короткого
    /// нажатия возвращается в тик ОТПУСКАНИЯ: прицел обязан дожить ровно до
    /// него, иначе заморозка гаснет на один тик раньше, чем её спросят.
    bool aim_key_was_down_ = false;
    /// КУДА ВСТАЁТ ЧЕЛОВЕК, ВЫЙДЯ ИЗ ПОЗЫ, и это НЕ вычисленная точка рядом с
    /// мебелью, а ТА, ГДЕ ОН СТОЯЛ, КОГДА СЕЛ. Вычисленная точка требует
    /// доказательства, что она не в теле мебели и не в стене; эта не требует
    /// — человек только что стоял на ней ногами. Она же и есть «рядом»: до
    /// сиденья от неё не больше радиуса руки, иначе сесть было бы нельзя.
    glm::vec3 posture_exit_{0.0f};
    float posture_exit_yaw_ = 0.0f;
    /// НАЧАЛО СТРЕЛЫ ТРЕТЬЕГО ЛИЦА, ПОКА ЧЕЛОВЕК НА МЕБЕЛИ: точка над
    /// ПРЕДМЕТОМ (его собственный верх плюс радиус щупа с отступом), а не
    /// глаз позы. Живёт здесь, а не в CameraBoom.h, потому что габарит
    /// предмета знает только тот, кто собрал точку позы; и переживает выход
    /// из позы намеренно — стрела обязана вернуться к глазу ПЛАВНО, вместе с
    /// блендером, а тот доезжает до нуля уже после того, как active_seat_
    /// погас. См. camera_boom_perch().
    glm::vec3 posture_perch_{0.0f};
    /// Есть ли смысл в posture_perch_ (ложь до первой посадки в этой сессии).
    bool posture_perch_valid_ = false;
    /// ПОТОЛОК ТАНГАЖА ЭТОЙ ПОЗЫ: лежащему — gameplay::POSTURE_BOOM_PITCH_MAX
    /// (у него взгляд в потолок, и стрела без потолка ныряет в матрас),
    /// сидящему — предел камеры, то есть ничего.
    float posture_pitch_cap_ = 0.0f;
    /// СЛЕД ПЕРЕХОДА (DFN_POSTURE_TRACE=1): строка на тик, пока блендер в
    /// пути, — время, доля, высота таза и глаза. Прибор, которым меряется
    /// «монотонно и без рывка»: по кадру этого не увидеть.
    bool posture_trace_ = false;
    double posture_trace_t_ = 0.0;
    /// Собирает точки позы по УЖЕ ЗАЛИТОЙ композиции (нужна геометрия).
    /// `interior` — какую из двух спрашивать: карман локации (вход в дом) или
    /// карту под открытым небом (вход в мир, правка сцены редактором). Правило
    /// у них одно, и живёт оно в collect_furniture_spots.
    void spawn_furniture_seats(bool interior);
    /// Сносит сущности и тела точек позы; поднимает игрока, если он сидел.
    void clear_furniture_seats();
    /// СНИМАЕТ ПОДСКАЗКУ МЕБЕЛИ, В КОТОРУЮ НЕ ЦЕЛЯТСЯ. Тот же приём и то же
    /// место, что у filter_door_hover: между update_hover и actions_step.
    void filter_seat_hover();
    /// Исполняет заявку: начинает ПОДХОД к точке старта либо поднимает.
    void take_seat();
    /// РАЗ В ТИК, ДО pre_step (зовётся из park_posture): ведёт человека к
    /// точке старта позы и, дойдя и довернувшись, ставит позу.
    void approach_step(float dt);
    /// Начать подход к точке `index` списка seats_; вслух отказывает, если
    /// точка дальше SEAT_APPROACH_MAX_M.
    void begin_approach(std::size_t index);
    /// Бросить подход, назвав причину (правило 30: у отказа есть причина).
    void cancel_approach(const char* why);
    /// САМА ПОСТАНОВКА ПОЗЫ. Единственный вход в позу во всём приложении:
    /// и живая клавиша, и беспилотная рука приходят сюда через подход.
    void enter_posture(std::size_t index);
    /// Поднимает из позы (E, прыжок, шаг) и возвращает капсуле управление.
    void leave_posture();
    /// РАЗ В ТИК, ДО pre_step: пока человек в позе, движение выключено, а
    /// капсула стоит запаркованной на точке выхода.
    void park_posture();
    /// ДОЗА НАСЕСТА СТРЕЛЫ В ПОЗЕ (DFN_POSTURE_CAM): 0 — стрела снова
    /// начинается в глазу позы, как до этой волны. Отрицательное плечо.
    [[nodiscard]] static bool posture_cam_enabled();
    /// РАЗ В ТИК, ПОСЛЕ update_bodies: печатает след перехода, если открыта
    /// дверь DFN_POSTURE_TRACE. Молчит, когда блендер стоит на месте.
    void posture_trace_step(float dt);
    /// РАЗ В ТИК, ПОСЛЕ update_bodies: ставит камеру в глаз НАРИСОВАННОЙ позы.
    void posture_camera();
    /// СИДИТ ЛИ ЧЕЛОВЕК ПРЯМО СЕЙЧАС (включая переход туда и обратно).
    [[nodiscard]] bool in_posture() const { return active_seat_ >= 0; }
    [[nodiscard]] SeatAimHit seat_aim_now(const SeatLink& link) const;
    /// ДОЗА МЕХАНИКИ ПОЗ (DFN_SEAT): 0 — ни одной точки, ни одной подсказки,
    /// игра бит-в-бит доволновая. Отрицательное плечо правила 30.
    [[nodiscard]] static bool seats_enabled();
    /// ДОЗА ПОДХОДА (DFN_SEAT_APPROACH): 0 — поза ставится в тик нажатия из
    /// того места, где человек стоял, и прицел живёт один кадр, то есть игра
    /// бит-в-бит доволновая. Отрицательное плечо правила 30, и оно же —
    /// вторая рука замера «садится не каждый раз».
    [[nodiscard]] static bool seat_approach_enabled();
    /// ДОЗА СБОРА ПОД ОТКРЫТЫМ НЕБОМ (DFN_SEAT_MAP): 0 — на карте точек нет
    /// ни одной, как было до волны «посадка под открытым небом», а в комнате
    /// всё по-прежнему. Отрицательное плечо правила 47: обе руки сравнения
    /// выходят из одного бинарника.
    [[nodiscard]] static bool seats_map_enabled();
    /// ПРИБОР ПОЗ (DFN_SEAT_PROBE): по каждой точке локации четыре руки —
    /// смотрю в предмет, смотрю мимо, стою спиной, стою далеко.
    void probe_seats();
    /// БЕСПИЛОТНАЯ РУКА НА КЛАВИШЕ (DFN_SEAT_TAKE=sit|lie): подводит игрока к
    /// ближайшей подходящей точке, разворачивает на неё и ЖМЁТ E — тем же
    /// interact_pressed, что и человек. Через столько же кадров жмёт ещё раз,
    /// чтобы встать. Прямой вызов take_seat() мерил бы половину механики
    /// (мимо прицела, подсказки и нажатия) — ровно ту ошибку, за которую
    /// заплатил беспилотный замер выхода из дома 27.08.
    void drive_seat_take();
    /// Через сколько кадров жать; 0 — рука не заведена.
    std::uint64_t seat_take_frames_ = 0;
    std::uint64_t seat_take_seen_ = 0;
    /// 0 — ещё не садились, 1 — сели (ждём кадры до вставания), 2 — всё.
    int seat_take_stage_ = 0;
    /// Чего просили: лежак (иначе сиденье).
    bool seat_take_lie_ = false;
    /// Доворот взгляда после посадки, градусы (DFN_SEAT_TAKE=lie:-35). 0 — не
    /// доворачивать: в позе взгляд свободен, и умолчание позы остаётся.
    float seat_take_pitch_deg_ = 0.0f;
    /// СКОЛЬКО КАДРОВ ДЕРЖАТЬ КЛАВИШУ (DFN_SEAT_TAKE=sit+24@30) и на сколько
    /// градусов увести ВЗГЛЯД за это время. Рука прибора обязана нажимать E
    /// ТАК ЖЕ, КАК ЖИВАЯ: живая клавишу ДЕРЖИТ, и приложение возвращает
    /// короткое нажатие только на отпускании (GrabDrive.h). Прибор, тыкающий
    /// прямо в защёлку, мерил бы половину пути и не увидел бы разрыва в тиках,
    /// из-за которого посадка срабатывала не каждый раз. 0 — прежний тычок.
    std::uint64_t seat_take_hold_frames_ = 0;
    float seat_take_hand_turn_deg_ = 0.0f;
    /// Сколько кадров держать осталось; 0 — клавиша не лежит.
    std::uint64_t seat_take_hold_left_ = 0;
    /// Доворот РЫСКА после посадки, градусы (DFN_SEAT_TAKE=lie:-38,90). У
    /// лежащего взгляд идёт вдоль кровати, и стрела третьего лица уходит
    /// ровно в изголовье; развернуть голову вбок — единственный способ снять
    /// его со стороны, и в живой игре это делает мышь.
    float seat_take_yaw_deg_ = 0.0f;
    // ---- ФИЗИКА ПРЕДМЕТОВ (заказ владельца 28.08: «зажав E, поднимать
    // объекты, держать, складывать друг на друга»). Тело — AppProps.cpp;
    // здесь — состояние и три точки врезки в тик (grab_input до park_posture,
    // sync_loose_props сразу за шагом физики, подсказка рядом с дверной).

    /// ОДИН ПОДВИЖНЫЙ ПРЕДМЕТ МИРА: тело Jolt, дро с матрицей и масса.
    struct LoosePropLink {
        std::string object;                 ///< имя предмета реестра
        platform::PhysicsBodyHandle body{}; ///< динамическое тело
        std::size_t render_index = 0;       ///< номер дро в RenderSystem
        float mass_kg = 0.0f;
        bool interior = false;              ///< живёт в кармане локации
    };
    std::vector<LoosePropLink> loose_props_;
    /// РАССТАНОВКИ ПОДВИЖНЫХ ПРЕДМЕТОВ, снятые заливкой построек. Заполняет их
    /// upload_house_mesh: он один читает сцену и знает, где карман локации, — а
    /// собирать тела на середине заливки нельзя, там ещё нет ни коллайдера, ни
    /// пола, на который предмет обязан лечь.
    struct LoosePlacement {
        std::string object;
        glm::vec3 position{0.0f};
        float yaw = 0.0f;
        float scale = 1.0f;
        bool interior = false;
    };
    std::vector<LoosePlacement> loose_placements_;
    /// Реестр физики предметов (assets/objects/furniture/PHYSICS.txt), читан
    /// один раз. Пустой — мир вчерашний: всё неподвижно.
    std::map<std::string, PropRow> prop_table_;
    bool prop_table_read_ = false;
    /// Ручки хвата (GrabDrive.h). Поле, а не константа, чтобы прибор мог
    /// крутить их, не пересобирая игру.
    GrabTuning grab_tuning_;
    GrabHold grab_hold_;
    /// НОМЕР ПРЕДМЕТА В РУКАХ в loose_props_, либо -1. Номер, а не указатель:
    /// список перетряхивается на входе в другую локацию.
    int grabbed_ = -1;
    /// Сколько уже длится отставание предмета от руки (для выпадения).
    float grab_lag_s_ = 0.0f;
    /// Доворот предмета в руках вокруг вертикали, радианы (колесо мыши).
    float grab_spin_ = 0.0f;
    /// ЗАЩЁЛКА КОРОТКОГО НАЖАТИЯ, ВЗЯТАЯ НА ВРЕМЯ. Договор с волной «сидеть и
    /// лежать»: за interact_pressed остаётся смысл «КОРОТКОЕ нажатие
    /// случилось», и распознавание короткого/долгого кладёт эта зона. Пока
    /// клавиша ещё нажата, знать, коротким ли будет нажатие, НЕЛЬЗЯ — поэтому
    /// защёлка снимается на время удержания и возвращается на отпускании, если
    /// удержание не дотянуло до порога. Ни строки в чужом коде.
    bool grab_short_held_ = false;
    /// ЭТА РАССТАНОВКА — ПОДВИЖНЫЙ ПРЕДМЕТ? true — она снята в свой список и
    /// НЕ должна попасть ни в поток построек, ни в общий коллайдер. Зовётся
    /// из заливки (AppHouse), потому что там один читатель сцены и один
    /// владелец кармана локации.
    [[nodiscard]] bool loose_prop_placement(const world::Placement& placement,
                                           const glm::vec3& origin, bool interior);
    /// НАКРЫТЫЙ СТОЛ ДЛЯ ПРИЁМКИ (DFN_GRAB_LAB): доставляет в локацию кувшин,
    /// кубки, книги, миски и бутыль. Нужен потому, что боевые карты сегодня не
    /// ставят в комнаты ни одного подвижного предмета из реестра.
    void add_lab_props();
    /// Собирает тела и дро подвижных предметов по снятым расстановкам.
    void spawn_loose_props();
    /// Сносит тела и дро; роняет предмет из рук, если он был.
    void clear_loose_props();
    /// РАЗ В ТИК, ДО park_posture: короткое/долгое E, прицел, взятие.
    void grab_input(float dt);
    /// РАЗ В ТИК, ПОСЛЕ шага физики: тела -> матрицы отрисовки. `force` —
    /// спросить и СПЯЩИЕ тоже: на подъёме комнаты спят все, и без этого их
    /// матрицы остались бы единичными, то есть все предметы нарисовались бы в
    /// начале координат мира (поймано замером 28.08).
    void sync_loose_props(bool force = false);
    /// Подсказка «Взять»/«Отпустить», если целимся в предмет или несём его.
    [[nodiscard]] std::uint64_t grab_prompt_key() const;
    /// НА КАКОЙ ПРЕДМЕТ СЕЙЧАС СМОТРИМ (номер в loose_props_ либо -1).
    /// Радиус и взгляд — тем же лучом, что у перекрестья, и с проверкой, что
    /// между глазом и предметом нет стены.
    [[nodiscard]] int grab_aimed() const;
    /// Точка перед камерой, к которой пружина ведёт предмет.
    [[nodiscard]] glm::vec3 grab_hand_point() const;
    /// Взять предмет по номеру / отпустить (бросить, если задана скорость).
    void grab_take(int index);
    void grab_release(bool thrown);
    /// ДОЗА ФИЗИКИ ПРЕДМЕТОВ (DFN_PROPS): 0 — ни одного динамического тела, ни
    /// одного дро, клавиша E прежняя. Отрицательное плечо правила 30.
    [[nodiscard]] static bool props_enabled();
    /// ПРИБОР (DFN_GRAB_PROBE): беспилотная рука — подойти, взять, перенести,
    /// поставить, бросить; печатает числа и снимает кадры.
    void probe_grab();
    std::uint64_t grab_probe_seen_ = 0;
    /// РУКА ПРИБОРА НА КЛАВИШЕ И НА МЫШИ. Подмешиваются к СЫРОМУ состоянию
    /// там же, где читается настоящее: прибор обязан мерить ту же механику,
    /// что и человек, а не звать её половину напрямую.
    bool grab_probe_key_ = false;
    bool grab_probe_click_ = false;
    /// СЕРЕДИНА СТОЛЕШНИЦЫ накрытого стола, найденная лучами (DFN_GRAB_LAB).
    /// Её же берёт прибор как точку «поставить на стол»: искать столешницу
    /// второй раз значило бы завести второй ответ на один вопрос (правило 39).
    glm::vec3 grab_lab_table_{0.0f};
    /// Стойка, с которой хват однажды получился (прибор возвращается на неё).
    glm::vec3 grab_probe_stand_{0.0f};
    bool grab_lab_on_table_ = false;

    /// ЗАЯВКА НА ПЕРЕХОД, поданная обработчиком Used и исполняемая ПОСЛЕ
    /// раздачи событий. Переход сносит сущности и телепортирует игрока —
    /// изнутри обработчика это правка контейнеров, по которым шина идёт.
    /// 0 — заявки нет.
    std::uint64_t pending_portal_ = 0;
    /// Исполняет заявку (если она есть) и гасит её.
    void take_portal();
    /// ЧЕРЕЗ СКОЛЬКО КАДРОВ ВЫЙТИ НАРУЖУ (DFN_INTERIOR_EXIT). 0 — не выходить.
    /// Дверь заведена потому, что выход умеет только рука на клавише, и без
    /// неё время выхода — единственное число свода И15, которое не может
    /// назвать ни один автоматический прогон.
    std::uint64_t interior_exit_frames_ = 0;
    std::uint64_t interior_exit_seen_ = 0;
    /// Экран загрузки. Один на приложение: у него нет ресурсов, а очистка
    /// между загрузками — это begin().
    render::LoadingScreen loading_;
    /// Длительность экрана в секундах (DFN_INTERIOR_FADE; 0 — мгновенно,
    /// и тогда кадры двух прогонов сравнимы побитово).
    float interior_fade_s_ = 0.15f;
    /// Замеры последнего перехода, миллисекунды. Отдельные поля, а не одно:
    /// у входа и выхода РАЗНЫЕ цели свода (0.5 с против 0.05 с).
    double interior_enter_ms_ = 0.0;
    double interior_leave_ms_ = 0.0;

    /// Заливает тело интерьерного коллайдера и страховочную плиту.
    void upload_interior_body(const std::vector<std::uint32_t>& indices);
    /// Вход в локацию. `spawn_name` — имя [spawn] целевой сцены; пусто —
    /// заголовочный spawn. Возвращает false и НЕ трогает мир, если сцену
    /// прочитать не удалось: полпути внутрь хуже, чем закрытая дверь.
    /// `back` — ОБРАТНЫЙ АДРЕС ДВЕРИ, через которую входят: где игрок встанет,
    /// выйдя. `back.set == false` — точкой возврата становится поза игрока
    /// (так входят [portal] композиции, у которых полотна нет). Аргумент
    /// ОБЯЗАТЕЛЕН намеренно: умолчание «поза игрока» тихо вернуло бы ровно то
    /// поведение, на которое владелец пожаловался, всякому новому вызову.
    [[nodiscard]] bool enter_interior(const std::string& scene_path,
                                      const std::string& spawn_name,
                                      const PortalReturn& back);
    /// Выход наружу по верхней ступени стека.
    void leave_interior();
    /// Заводит сущности переходов по [portal] текущей композиции.
    /// `entry` — МИРОВАЯ ТОЧКА ВХОДА в локацию (ступни). По ней прицел
    /// обратной двери растягивается так, чтобы из точки входа он ловился
    /// всегда: «вошёл и не могу выйти» — не то, за что игрок должен платить
    /// за вольность генератора. Для города nullptr.
    void spawn_scene_portals(const world::SceneDoc& doc, bool interior,
                             const glm::vec3* entry = nullptr);
    /// Снимает сущности переходов (обе стороны — город и локация).
    void clear_scene_portals(bool interior);
    /// Показывает один кадр экрана загрузки (тот же путь, что у меню:
    /// CPU-холст блитом поверх кадра; ImGui здесь не бывает).
    /// `shot_stem` — имя файла беспилотного снимка ЭТОГО экрана. Стволов два,
    /// и это не украшение: снимок пишется один раз за прогон, а прогон,
    /// открывающий карту и входящий в дом, показывает ДВА разных экрана —
    /// с одним именем город затёр бы комнату, которую рецепт и снимал.
    void present_loading_frame(const char* shot_stem = "loading");
    /// Держит экран на протяжении interior_fade_s_, показывая кадры. Спать
    /// вместо этого нельзя: окно обязано отвечать ОС.
    void hold_loading_screen();
    /// ЗАКРЫВАЕТ ЭТАП И ПОКАЗЫВАЕТ КАДР. Пара «отметить и предъявить» ходит
    /// вместе всегда: этап, отмеченный без кадра, виден прибору и невидим
    /// человеку — а ровно этим экран загрузки города и был до 27.08 (модель
    /// велась с 24.08, кадры не показывались, окно висело 2.3 с).
    void load_step(const char* what);
    /// ДВИГАЕТ ДОЛЮ ВНУТРИ ДЛИННОГО ЭТАПА (0..1 по своему счёту — например,
    /// по числу залитых построек) и показывает кадр, но НЕ ЧАЩЕ, чем раз в
    /// LOAD_FRAME_MIN_MS: кадр города стоит миллисекунды, и полоса, рисуемая
    /// на каждую постройку, сделала бы загрузку заметно длиннее ради движения,
    /// которого глаз всё равно не различает.
    void load_tick(float fraction);
    /// Пороговый шаг кадров экрана внутри этапа, миллисекунды.
    static constexpr int LOAD_FRAME_MIN_MS = 80;
    std::chrono::steady_clock::time_point loading_frame_at_{};
    /// Стволы имён, по которым снимок экрана загрузки уже написан. Латч на
    /// СТВОЛ, а не на прогон: см. present_loading_frame.
    std::vector<std::string> loading_shots_;
    /// Ствол имени снимка ТЕКУЩЕЙ загрузки: «loading» у входа в дом (так его
    /// зовут рецепты И15) и «loading_world» у загрузки карты.
    const char* loading_shot_stem_ = "loading";
    /// Заготовки инструментов постройки — для «стиль в заготовку» Библиотеки.
    /// Сырые указатели: владеет ящик инструментов, живут с ним.
    HouseLineTool* house_line_tool_ = nullptr;
    HouseSurfaceTool* house_surface_tool_ = nullptr;
    /// Картинка материала набора / пример заполнения стены — для панелей.
    std::uint64_t house_material_swatch(int surface, int tone, int px);
    std::uint64_t house_wall_example(int variant, int px);
    /// Маленький сруб в графе — для беспилотного кадра (дверь DFN_HOUSE_DEMO).
    void seed_demo_house();
    void on_tool_pick(int index);
    // НЕ ДЕЙСТВИЕ, А ПОЛЛИНГ: стрелки крутят деталь, Delete её убирает. У них
    // нет строки в таблице привязок, потому что таблица — это КРАЙ клавиши, а
    // стрелки читаются как навигация меню в другом месте. Живут рядом с
    // обработчиками, потому что это тот же ввод и та же рука.
    void update_part_rotation();
    // Окно чата, пока в нём печатают: ввод, забой, отправка, закрытие. Отдельно
    // от таблицы, потому что это ветка, в которой таблица НЕ РАБОТАЕТ.
    void service_chat_typing();

    // DEBUG READOUT + STATE CAPTURE (user request). collect_snapshot() reads
    // the world; write_capture() saves the .png and its sidecar; apply_restore()
    // puts the player back where a sidecar says he was.
    [[nodiscard]] DebugSnapshot collect_snapshot(float alpha);
    void write_capture(const DebugSnapshot& snap);
    void apply_restore(const DebugSnapshot& snap);
    // CHAT BOX (В28/O-серия): writes the pending entry, with the current frame's
    // capture attached, into the map's chat. Serviced after render() for the
    // same reason F2 is -- the image and its record must be the same frame.
    void write_pending_chat(float alpha);
    // The chat file beside the ACTIVE map (docs/MAP_LAYOUT.md), derived from the
    // browser's current_manifest() (category/file_stem). "" when no map is open
    // (chat disabled, said once).
    [[nodiscard]] std::string chat_path_for_current_map() const;
    // ТРЕТЬЕ ЛИЦО (клавиша 1) — СВОБОДНЫЙ ОБЛЁТ (заказ владельца 31.08,
    // пункт 4). Мышь крутит камеру вокруг персонажа ВСЕГДА; стоя тело не
    // поворачивается за ней вовсе; при нажатии движения тело доворачивается к
    // направлению «камера + ввод» с ограниченной скоростью (BODY_TURN_RATE), а
    // идёт при этом строго туда, куда просит ввод, независимо от того,
    // докрутилось оно или нет (ThirdPersonRig.h).
    bool third_person_ = false;
    // АЗИМУТ И ТАНГАЖ СТРЕЛЫ — МИРОВЫЕ УГЛЫ, А НЕ ОТСТУП ОТ РЫСКА ТЕЛА. Раньше
    // кадр считался как `pose->yaw + orbit_yaw_`, то есть камера жила в
    // системе тела; как только тело начинает доворачиваться к камере, оба
    // конца интерполяции кадра читают ОДИН отступ и РАЗНЫЕ рыски, и азимут
    // дёргается назад на долю доворота каждый кадр. См. ThirdPersonRig.h.
    float cam_yaw_ = 0.0f;
    float cam_pitch_ = 0.0f;
    // СТРЕЛА КАМЕРЫ И ЕЁ ЩУП (заказ владельца 27.08: «в помещении могу за
    // границы посмотреть»). Оснастка живёт здесь, а не в CameraBoom.h с
    // умолчаниями, потому что щуп-дозу DFN_CAM_COLLIDE=0 надо уметь снять с
    // ЖИВОГО вида, из того же бинарника, что и рабочую руку (Rule 47).
    gameplay::CameraBoomState cam_boom_{};
    gameplay::CameraBoomDesc cam_boom_desc_{};
    // Приборная часть: DFN_CAM_PROBE печатает строку на кадр, DFN_CAM_ORBIT
    // крутит orbit_yaw_ сам, чтобы стрела обошла все стены комнаты без руки.
    bool cam_probe_ = false;
    float cam_probe_spin_ = 0.0f;    // град/с, 0 — не крутить
    /// DFN_CAM_WALK: направление движения, нажатое дозой. Нулевой вектор —
    /// дверь закрыта, и клавиатура (или очередь стенда) единственный автор
    /// осей, как было. Прибор пункта 4: облёт СТОЯЩЕЙ фигуры не может
    /// показать доворот, потому что доворота у стоящей фигуры нет.
    glm::vec2 cam_walk_{0.0f, 0.0f};
    // СТЕНД ПЕРСОНАЖА (правило 17a, заказ владельца 31.08). Приёмочные кадры
    // фигуры снимаются ТОЛЬКО на assets/maps/stands/character.map, и снимаются
    // одинаково: заданная камера и заданная очередь клипов. 0 — обе двери
    // закрыты, всё как было.
    uint32_t stand_cam_ = 0;         // DFN_STAND_CAM=1..4, 0 — не задана
    bool stand_seq_ = false;         // DFN_STAND_SEQ=1 — играть очередь
    float stand_seq_t_ = 0.0f;       // секунды очереди, ведутся фиксированным тиком

    // --- СМОТРОВАЯ (заказ владельца 01.09) --------------------------------
    // Режим предпросмотра моделей: пустая площадка, портретный свет, одна
    // модель на постаменте и стрелки, которыми её меняют. Состояние живёт
    // здесь, а решения — в ModelViewer.h; в этом файле только то, чему нужен
    // рендерер и открытая карта (AppViewer.cpp).
    //
    // ПРИЗНАК РЕЖИМА — ОТКРЫТАЯ КАРТА, А НЕ ВТОРОЙ ФЛАГ РЯДОМ С НЕЙ. Карта
    // stands/viewer.map И ЕСТЬ смотровая; поле ниже взводится в enter_world по
    // манифесту и гаснет там же. Отдельный флаг, который можно было бы
    // выставить на боевой карте, дал бы режим, в котором стрелки листают
    // модели посреди Вайтрана.
    bool viewer_mode_ = false;
    std::vector<ViewerItem> viewer_items_;
    int viewer_index_ = 0;
    ViewerView viewer_view_{};
    /// Где стоит постамент — середина площадки, взятая у стримера при входе.
    glm::vec3 viewer_pad_{0.0f};
    /// Габарит ПОКАЗАННОЙ модели в её собственных единицах и множитель показа.
    glm::vec3 viewer_lo_{0.0f};
    glm::vec3 viewer_hi_{0.0f};
    float viewer_scale_ = 1.0f;
    std::size_t viewer_triangles_ = 0;
    /// Почему модель не показана, если не показана. Пустая строка — показана.
    std::string viewer_error_;
    /// Куда идти за dfn_import_gltf; пусто — на этой машине его нет, и
    /// смотровая говорит это вслух вместо того, чтобы показать пустой постамент.
    std::string viewer_gltf_tool_;
    /// Сколько раз модель менялась за прогон — счётчик для отчёта приёмки: он
    /// отвечает на «сколько пар меша создано и уничтожено», чего кадр не
    /// показывает.
    std::uint64_t viewer_swaps_ = 0;

    // --- ЭКРАН СОЗДАНИЯ ПЕРСОНАЖА ----------------------------------------
    // ФЛАГ, А НЕ СТРАНИЦА МЕНЮ, и это то же различение, что у смотровой выше:
    // MenuPage — это СПИСОК СТРОК, и вся арифметика меню построена на нём. У
    // этого экрана органов управления четыре (см. CharGen.h), и он живёт
    // рядом с меню, а не внутри него.
    bool chargen_open_ = false;
    CharGenScreen chargen_{};
    CharGenBody chargen_body_{};
    /// Где был указатель на прошлом кадре экрана — облёт считается разностью.
    glm::vec2 chargen_cursor_{-1.0f, -1.0f};
    /// Тянем ли сейчас облёт (левая кнопка зажата НА ФИГУРЕ, не на ручке).
    bool chargen_orbiting_ = false;
    /// Доза DFN_CHARGEN читается ОДИН раз за прогон, и этот флаг — её
    /// защёлка. Дверь опрашивается в ветке меню, а не в init(): экрану
    /// нужны видеокарта и испечённый шрифт, а на месте init() ещё не
    /// готово ни то, ни другое.
    bool chargen_dose_read_ = false;
    /// Построен ли body_rig_ (см. ensure_body_rig).
    bool body_rig_built_ = false;
    uint64_t cam_probe_frames_ = 0;  // сколько кадров прибор насчитал
    uint64_t cam_probe_outside_ = 0; // на скольких камера оказалась за оболочкой
    float cam_probe_worst_ = 0.0f;   // худший заход за стену, м
    bool debug_overlay_ = false;    // key 2 (F3 alias)
    // Whole-scene wireframe (В28), key 4 / F4. Toggles IRenderer::set_wireframe;
    // the editor overlay reads it back to label the mode. Off by default, zero
    // cost off (render's contract).
    /// ГДЕ СЕЙЧАС СТОИТ ПЕРЕБОР ПОЗ. Ноль — ЖИВОЕ ТЕЛО (локомоция), 1..N —
    /// записи реестра по порядку. Ноль не отдан позе «стоя» намеренно:
    /// «стоя» это поза, которую тело ДЕРЖИТ, а живое тело ходит, и на стенде
    /// надо уметь показать оба.
    uint32_t pose_slot_ = 0;
    bool pose_dose_done_ = false;
    bool pose_tape_ = false;
    bool pose_tape_read_ = false;
    float pose_tape_t_ = 0.0f;
    bool wireframe_ = false;
    bool capture_pending_ = false;  // F2, serviced after render()
    FrameClock frame_clock_{};
    int captures_written_ = 0;
    std::string capture_dir_;
    /// ЗАПИСЬ ПРОХОДА (DFN_RECORD_EVERY=<кадров>, 20.08: «сделай запись экрана
    /// прохода... к видео сохраняй и „субтитры" — позиции игрока, направление
    /// взгляда»). Каждый N-й ПОКАЗАННЫЙ кадр — rec_%05d.png в capture_dir_ и
    /// строка rec.log с тем же снимком состояния, что у F2. Видео и .srt
    /// собирает tools/make_walk_video.py из этих двух артефактов.
    std::uint64_t record_every_ = 0;
    std::uint64_t record_seen_ = 0;
    int record_written_ = 0;
    double capture_after_s_ = 0.0;      // DFN_CAPTURE_AFTER, 0 = off
    double capture_after_elapsed_ = 0.0;
    // DFN_CAPTURE_AFTER_FRAMES, 0 = off. The SAME door counted in frames
    // instead of seconds, because the seconds door cannot be compared bit for
    // bit: two runs of one recipe reach different frame numbers under different
    // machine load, and everything derived from the frame counter then diverges.
    // Measured by ui: 4125 differing pixels between two runs on the same keys,
    // down to 412 once the sky's clocks were pinned -- and the remainder was
    // this. Frames are the unit the rest of the loop already runs on.
    uint64_t capture_after_frames_ = 0;
    uint64_t capture_after_frames_seen_ = 0;
    bool capture_then_close_ = false;
    // DFN_SHOT_AFTER=<frames>: the dose door for the key-5 screenshot. Counted
    // in frames for the same reason its neighbour above is -- a wall second
    // holds a different number of frames on a loaded machine, so two runs of
    // one recipe would not be comparable. It reuses chat_then_close_ to exit:
    // the shot IS a chat entry, so it is the same shutdown.
    uint64_t shot_after_frames_ = 0;
    uint64_t shot_after_frames_seen_ = 0;
    // СКОЛЬКО КАДРОВ ЕЩЁ РИСОВАТЬ, ЧТОБЫ .PNG УСПЕЛ ЛЕЧЬ. Было голое число;
    // стало объект с правилом «второй взвод не укорачивает ожидание», потому
    // что снимок и запись чата приходятся на один кадр (клавиша 5 это и то и
    // другое). Правило проверяется в tests/app/AfterFrameTests.cpp.
    FlushCountdown flush_countdown_;
    // FRAME LOG (DFN_FRAME_LOG=<path>) -- one line per PRESENTED frame, written
    // live, with no readback, no settle and no cooldown.
    //
    // Why it is not a screenshot: the user found the reason himself. "при
    // прогоне бега есть тряска, но в момент, когда делается скрин, тряски нет,
    // картинка статичная." Every capture door we own either freezes the tick
    // (the tour) or waits for the backend to flush (F2, the body probe's
    // cooldown of 4). A defect that lives in the DIFFERENCE between consecutive
    // frames cannot survive any of that -- the instrument settles the thing it
    // was pointed at. Two days of clean single frames were the instrument
    // agreeing with itself.
    //
    // So this logs the quantities that MOVE THE WHOLE PICTURE, once per frame
    // actually presented, and the between-frames motion is then arithmetic on
    // adjacent lines rather than something a still has to show.
    std::FILE* frame_log_ = nullptr;
    uint64_t frame_log_index_ = 0;
    // CHAT BOX (В28/O-серия; docs/MAP_LAYOUT.md). The chat is a JSONL append-log
    // beside the active map; the pending entry is written after render() so its
    // attached capture and the entry describe the same frame.
    bool chat_pending_ = false;
    ChatEntry chat_pending_entry_{};
    bool chat_then_close_ = false;       // the DFN_CHAT_MSG verification door closes
    // The typed-chat window (В28): opened with '/', it captures the keyboard for
    // live UTF-8 input; Enter sends (through write_pending_chat), Escape closes.
    ChatOverlay chat_overlay_;
    // TELEMETRY RING (item 3): sampled on the COUNTED clock in the editor and
    // flushed beside the map on stop. In-game stays light (В39: no continuous
    // log). Constructed in App() from config::TELEMETRY_RING_SAMPLES.
    TelemetryRing telemetry_;
    double telemetry_last_s_ = -1.0e18;  // counted-clock time of the last sample

    // TRAJECTORY RECORD + DETERMINISTIC REPLAY (O3, the key item of В28). Record
    // a walk/look per presented frame; replay drives the camera and the counted
    // clock from the file so two replays render bit-for-bit (Rule 53). Recording
    // is an editor action; replay is driven by R/P keys or the
    // DFN_TRAJ_REC / DFN_TRAJ_PLAY doors.
    TrajectoryRecorder traj_rec_;
    std::optional<TrajectoryPlayer> traj_play_;
    TrajectoryFrame replay_frame_{};      // the frame being replayed this iteration
    bool replaying_ = false;              // set per frame while a replay is live
    bool traj_play_then_close_ = false;   // the DFN_TRAJ_PLAY door closes when spent
    std::string traj_last_path_;          // last recording written (P replays it)
    std::string traj_rec_out_;            // DFN_TRAJ_REC target, "" = off
    bool traj_rec_arm_ = false;           // begin recording when the world is entered
    int traj_written_ = 0;                // names trajectory_NNN.dftraj
    // A restore read from DFN_RESTORE, held until enter_world() has built the
    // map it names -- the pose cannot be applied to a world that does not
    // exist yet, and the stand it names decides WHICH world gets built.
    std::optional<DebugSnapshot> restore_;
    // A RESTORED CROUCH IS HELD, not merely set once. accumulate_input rewrites
    // crouch_held from the real keyboard every RENDER frame, so a restored
    // crouch survived exactly until the first frame -- which is why no
    // automated capture had ever been taken at full crouch, and why the defect
    // that put the camera inside the chest was only ever seen by the user.
    // Cleared by any crouch capture that restores standing.
    bool hold_crouch_ = false;
    // Where a restore ASKED the capsule to end up. Checked once, the frame
    // after: IPhysics has no teleport, so a restore is a long collide-and-slide
    // walk and can be stopped by geometry. Reported, never assumed.
    std::optional<glm::vec3> restore_target_;
    // Remaining correction attempts. One collide-and-slide step does not carry
    // a long displacement (sim measured 0.53 m of residual), so the horizontal
    // correction is re-issued until it converges or these run out.
    int restore_attempts_ = 0; // vestigial after the teleport fix; kept at 0
    // STREAMING QUIESCENCE for the tour's settle (Rule 42). The tour waited a
    // fixed count of RENDERED frames for work denominated in SIM steps, and two
    // runs of the same binary differed by 17-35% of pixels because of it. These
    // let the app hold the countdown until the world has actually stopped
    // changing. `world_changed_this_frame_` is set by the chunk ferry and
    // cleared at the top of each frame.
    bool world_changed_this_frame_ = false;
    // ЗАТВОР БЕСПИЛОТНОГО КАДРА: гистерезис и потолок, вынесенные в
    // AppAfterFrame.h, где их прогоняет рукав. Здесь было два счётчика и два
    // литерала в кадровом цикле. Затвор ОДИН на тур и на дверь дозы: раньше он
    // стоял внутри `if (tour_.active())`, и дверь, которой снимают города,
    // отсчитывала свои кадры от запуска — то есть снимала то, что успело
    // приехать.
    SettleGate settle_gate_;
    // ВЕРДИКТ ЗАТВОРА ЗА ПРОШЛЫЙ КАДР. Латч, а не пересчёт по месту: его
    // спрашивают трое в разных точках кадра (тур в хвосте, доза в голове, часы
    // мира в середине), и три ответа обязаны быть одним ответом.
    bool world_settled_ = true;
    bool settle_cap_said_ = false;
    // Высота капсулы игрока на прошлом кадре — по её ПРИРАЩЕНИЮ затвор судит,
    // сошлась ли осадка (PLAYER_SETTLE_EPS_M). Дверная волна намерила 1.3 мм
    // расхождения именно здесь.
    float last_player_y_ = 0.0f;
    // ФОКУС СТРИМИНГА ЭТОГО КАДРА, посчитанный ОДИН раз: его спрашивают поток
    // чанков, дальняя земля и затвор, и три копии одной лесенки «повтор / тур /
    // редактор / игрок» обязаны были согласиться.
    glm::vec3 stream_focus_{0.0f};
    // Сколько клеток радиуса ещё не приехало — очередь, которую до 29.08 было
    // нечем увидеть (ChunkManager::pending_chunk_count).
    std::size_t chunks_pending_ = 0;

    AppConfig config_{};

    std::unique_ptr<platform::IWindow> window_;
    std::unique_ptr<platform::IInput> input_;
    std::unique_ptr<platform::IRenderer> renderer_;
    std::unique_ptr<platform::IPhysics> physics_;
    std::unique_ptr<platform::IAudio> audio_;

    ecs::World world_;
    events::EventBus bus_;
    time::FixedTimestep timestep_;
    world::ChunkManager chunks_;
    render::RenderSystem render_system_;
    render::FirstPersonCamera camera_;
    render::Tour tour_;
    ecs::EntityId player_{};
    // In-game clock; DAY_LENGTH_SECONDS per day. Starts at START_TIME_OF_DAY
    // rather than at zero: zero is MIDNIGHT, so a fresh launch opened in the
    // dark and the frame gave no hint that the hour was the reason.
    double game_seconds_ = static_cast<double>(config::START_TIME_OF_DAY)
                           * static_cast<double>(config::DAY_LENGTH_SECONDS);
    std::array<platform::PhysicsBodyHandle, 4> world_edge_{}; // extent walls
    /// СНОС ПРЕДЫДУЩЕГО МИРА. enter_world() зовётся по КАЖДОМУ открытию карты
    /// из браузера, а не один раз за запуск, и до 20.08 не сносил ничего: чанки
    /// оставались резидентными, тела переправы и края мира — живыми, водяные
    /// бакеты и путевые поверхности — залитыми, а spawn_player заводил ВТОРОГО
    /// игрока поверх первого. Список здесь и в shutdown() — ОДИН (правило 32):
    /// shutdown() зовёт эту же функцию, поэтому третьей копии сноса быть не
    /// может.
    void unload_world();
    /// ПОДПИСКИ МОСТА МИРА, которые надо снимать вместе с миром. Без них второй
    /// вход в мир вешал ВТОРОЙ обработчик на ChunkLoaded, и каждый чанк
    /// заливался дважды — первая заливка при этом навсегда терялась в бэкенде.
    events::SubscriptionId chunk_loaded_sub_{};
    events::SubscriptionId chunk_unloaded_sub_{};
    events::SubscriptionId landed_sub_{};
    /// Registry directory the NEXT Gallery open loads from (set by open_map
    /// from the manifest; default = the tree shelf) and the exhibits' static
    /// trunk bodies (user: «сделать деревья физичными, не давать сквозь них
    /// ходить»), destroyed on the next gallery load.
    std::string gallery_objects_dir_ = "assets/objects/trees";
    /// The map's .scene, if it has one: WHERE things stand, as an edited file
    /// instead of a grid this code invents. Empty = the auto-grid, as before.
    std::string gallery_scene_;
    /// ONE TILE OF A COMPOSITION: its placements, kept so the tile can be
    /// re-baked in a cheaper form when the player walks away from it, and the
    /// form it is currently baked in.
    struct SceneTile {
        glm::ivec2 key{0};
        glm::vec2 min_xz{0.0f};
        glm::vec2 max_xz{0.0f};
        std::vector<world::Placement> parts;
        bool far_form = false;
    };
    /// THE SWARM. Lives for the whole map, not for a chunk: the user asked for
    /// fireflies «повсюду, а не только в какой-то зоне», and a chunk-owned
    /// swarm would blink out at the streaming edge.
    /// The CURRENT map's composition, read once before the ground is built
    /// (its pads shape the height field) and used again to place the objects.
    /// Draws one frame of the first-run preparation screen.
    void draw_bake_progress(std::size_t done, std::size_t total,
                            const std::string& what);

    world::SceneDoc scene_doc_;
    /// HAS THE COMPOSITION BEEN CHANGED IN THIS SESSION? Saving writes the doc
    /// back over its .scene, and those files carry HAND-WRITTEN comments and
    /// hand-chosen ordering (the showcase says so in its own header). Writing
    /// an unchanged doc would silently reformat somebody's file and lose the
    /// comments — so "save" with nothing to save must be a refusal that says
    /// so, not a no-op and not a rewrite.
    bool scene_dirty_ = false;

    // ---- THE BUILD HAND (editor). Decisions live in BuildTool.{h,cpp}; what
    // is here is the state and the wiring to the world.
    std::vector<BuildGroup> build_groups_;
    /// МЕНЮ ОБЪЕКТОВ: модель живёт здесь, панель объявляется в EditorUi один
    /// раз при подъёме карты. App владеет обеими — панель на них ссылается.
    PaletteModel palette_;
    bool palette_wired_ = false;
    /// КУРСОР ЖИВЁТ В ЯЩИКЕ ИНСТРУМЕНТОВ (EditorToolbox::pointer_mode), а не
    /// здесь: клавиша R — часть контракта инструментов («почти как в vim»), и
    /// поле в App было бы второй копией того же состояния. Спрашивать —
    /// editor_ui_.toolbox().pointer_mode().
    /// DFN_CAM_TRACE=1 — печатать в stderr пару «пришло смещение мыши / стал
    /// рыск» на каждом кадре редактора. Читается один раз при старте.
    bool cam_trace_ = false;
    std::size_t build_group_ = 0;
    std::size_t build_item_ = 0;
    float build_yaw_ = 0.0f;
    BuildGhost build_ghost_;
    /// Висит ли сейчас призрак В РЕНДЕРЕРЕ. Отдельно от build_ghost_, потому
    /// что вопрос «что я держу» и вопрос «что загружено» — разные, и путать их
    /// значит оставлять деталь нарисованной после того, как её выпустили.
    bool ghost_uploaded_ = false;
    /// ИСТОРИЯ ПРАВОК: снимки состояния, а не обратные действия. Обратное
    /// действие требует, чтобы КАЖДАЯ операция умела себя обращать, и ломается
    /// на первой, которая не умеет, — а дальше отмена врёт молча.
    EditorHistory history_;
    /// ПОСТРОЙКА, КОТОРУЮ ПРАВЯТ ТРИ ИНСТРУМЕНТА (вершины, прямая, поверхность),
    /// и ОДНА на всех троих: копия графа у каждого — это три дома, которые
    /// разъедутся на первом же сдвинутом якоре. Здесь же она нужна отмене:
    /// история хранит снимки текстом и НЕ ЗНАЕТ про модель нарочно, поэтому
    /// применяет снимок тот, у кого модель есть, — и это единственное место.
    HouseSession house_;
    /// Версия геометрии, при которой тело залито последний раз.
    std::uint32_t house_mesh_version_ = 0;
    /// Сетка и её шаг живут в сессии постройки (HouseSession): по ней прилипают
    /// якоря, ею шагают стрелки, её же рисуют отсечки. Здесь поля НЕТ нарочно —
    /// второе такое поле было бы вторым ответом на один вопрос.
    /// Коллайдер постройки и его вершины. Позиции живут полем, а не временной
    /// переменной: дескриптор физики берёт их СПАНОМ, и буфер обязан пережить
    /// вызов, иначе тело построится по памяти, которой уже нет.
    platform::PhysicsBodyHandle house_body_{};
    /// Кэш испечённых свотчей: (surface,tone,px) и примеры заполнения.
    std::unordered_map<std::uint64_t, std::uint64_t> house_swatches_;
    std::vector<glm::vec3> house_positions_;
    BuildVerdict build_verdict_;
    /// Which placement the crosshair is on, for DELETING. npos = none. Kept as
    /// an index into scene_doc_.placements, resolved fresh every frame: an
    /// index remembered across an edit would delete the wrong thing.
    std::size_t build_target_ = static_cast<std::size_t>(-1);
    /// Measured sizes, memoised. Same ruler as the judge and the tools
    /// (render::measure_object), never a second copy.
    std::map<std::string, render::ObjectExtent> build_extents_;
    void update_build_tool();
    void clear_build_ghost();
    [[nodiscard]] bool build_place();
    [[nodiscard]] bool build_delete();

    // ---- THE OTHER FOUR TOOLS (editor). The mode lives in EditorUi; this is
    // what each mode DOES, and it is wiring only — every decision belongs to a
    // module a test can instantiate (EditorBrush, EditorPlant, BuildTool).
    //
    // WHY THEY ARE HERE AT ALL (user, 17.08.2026: «состояние на R меняется, но
    // инструменты не рисуются, не понятно что сейчас я делаю и что»). Four of
    // the five modes were empty: the chip lit up, the camera obeyed, and the
    // world did not answer. A mode that changes nothing outside the interface
    // is indistinguishable from a broken key.
    /// The hand edit of the ground for THIS map, and the one truth about it:
    /// it goes into the world through ChunkManager::set_composed_relief, which
    /// feeds compose_passes — the ground the player walks and the ground
    /// check_scene judges are then one thing.
    world::ReliefLayer relief_;
    /// Записать карту ЦЕЛИКОМ: сцену и сиделку .relief рядом с ней. Одна кнопка
    /// «сохранить» не обязана знать, что записей две.
    bool save_map_with_relief();
    /// ЕДИНСТВЕННАЯ ДВЕРЬ К ЗАПИСИ ТРОПЫ (ToolWorld::commit_path): добавить,
    /// заменить или убрать, перепечь канал износа и пометить землю.
    std::size_t commit_relief_path(std::size_t index, const world::ReliefPath* path);
    /// КИСТИ ЖИВУТ В СВОИХ ИНСТРУМЕНТАХ (HeightBrushTool / SurfacePaintTool /
    /// PlantTool). Общий TerrainBrush с полем mode, показанный двумя фишками,
    /// и был тем «странным взаимодействием покраски и высоты», которое
    /// пользователь назвал 18.08: полоса задавала режим, панель задавала его же
    /// из второго места. Два объекта не могут разойтись во мнении о своём
    /// режиме.
    /// The last dab's numbers, for the panel's readout. A brush that has
    /// silently stopped biting looks exactly like a brush aimed at nothing.
    int last_dab_samples_ = 0;
    float last_dab_worst_m_ = 0.0f;
    /// КОГДА ПОКАЗАТЬ ЗЕМЛЮ, НЕ ДОЖИДАЯСЬ ОТПУСКАНИЯ (заказ 18.08: «хочу
    /// изменение ландшафта от инструмента высоты в реальном времени... а мне
    /// так непонятно что происходит»). Решение живёт в engine/editor отдельным
    /// предметом, а не парой полей здесь, потому что App держит окно и не
    /// заводится в проверке — а вопрос «сколько раз за штрих изменилась земля»
    /// не задать ни одному кадру.
    StrokeRefresh stroke_refresh_;
    /// Земля действительно сдвинулась с прошлого показа. Не «кнопка зажата»:
    /// кисть, наведённая за край подгруженного кольца, не двигает ничего, и
    /// перестраивать после неё нечего.
    bool ground_moved_since_push_ = false;
    bool brush_wired_ = false;
    /// Species the map's shelves carry, read once — a directory listing per
    /// frame is a directory listing per frame.
    std::vector<std::string> plant_species_;
    /// THE SELECTED PLACEMENT, and it is NOT build_target_: that one is what
    /// the crosshair is over right now and changes as the camera drifts, which
    /// is the wrong thing to be editing numbers of. This one is what the
    /// builder CLICKED, and it survives him looking away from it.
    std::size_t selected_ = static_cast<std::size_t>(-1);
    bool props_wired_ = false;
    /// The properties column's live numbers, edited in place by the panel and
    /// pushed into the world through EditorPlant::edit_placement — which
    /// re-judges, and puts the placement back on a refusal.
    PropsModel props_;
    /// ЧТО СТРОИМ СЕЙЧАС — the group new parts join. Empty means "alone", and
    /// alone is what every hand-placed part used to be: see the note at
    /// build_place() for why that made a house impossible to build by hand.
    std::string build_group_name_;
    /// Declares the editor's panels ONCE, on entering the editor rather than on
    /// a keypress: a menu that does not exist until you press its shortcut is a
    /// menu for whoever wrote it.
    void wire_editor_panels();
    void update_editor_tools(float dt_s);
    /// Pushes the properties column's numbers into the composition, re-judged.
    /// False = the judge refused and nothing changed; props_.refusal says why.
    [[nodiscard]] bool apply_selection_edit();
    /// Where the crosshair meets the ground (or what it meets first). ONE aim
    /// for five tools — three copies of this march would drift the first time
    /// one of them was tuned, and the symptom is a brush biting a metre from
    /// the cross.
    [[nodiscard]] glm::vec3 editor_aim_point();
    /// ТО ЖЕ, НО С ДАЛЬНОСТЬЮ И ПРИЗНАКОМ ПОПАДАНИЯ — то, что читает потолок
    /// дальности (EditorToolbox). Голый vec3 не давал спросить «а как далеко»,
    /// поэтому общий параметр было негде проверить.
    [[nodiscard]] ToolAim editor_aim();
    /// ПРИЦЕЛ КАДРА — СЧИТАННЫЙ ОДИН РАЗ. editor_aim() это марш в 160 шагов по
    /// высотному полю плюс линейный перебор всех расстановок, и его звали ЧЕТЫРЕ
    /// раза за кадр (призрак, тик инструмента, строка состояния, кольцо кисти).
    /// Здесь он считается лениво и запоминается до ближайшего события, которое
    /// меняет ОТВЕТ: сдвиг камеры и мутация мира инструментом. Не «раз в начале
    /// кадра»: между призраком и тиком стоит editor_cam_.update(), а между тиком
    /// и строкой состояния — мазок кисти, и переиспользование через них давало
    /// бы кисть, кусающую там, где прицел был кадр назад.
    [[nodiscard]] ToolAim aim_this_frame();
    void invalidate_frame_aim() { frame_aim_valid_ = false; }
    ToolAim frame_aim_{};
    bool frame_aim_valid_ = false;
    /// КИСТЬ ПРИХОДИТ ОТ ИНСТРУМЕНТА, а не берётся из поля App: настройки,
    /// которые человек двигал, и земля, которую он копает, обязаны быть ОДНОЙ
    /// кистью.
    [[nodiscard]] bool apply_terrain_dab(const TerrainBrush& brush, glm::vec2 centre,
                                         float dt_s);
    void finish_stroke();
    [[nodiscard]] int plant_dab_here(const PlantBrush& brush, glm::vec2 centre);
    /// Re-bakes the ONE tile a placement falls in. An edit must not cost a
    /// whole-map re-bake: the builder places a part every few seconds.
    void rebake_tile_at(glm::vec2 world_xz);
    [[nodiscard]] const std::string& build_selected() const;
    /// DFN_DRAW_COLLIDERS=1: the collision triangles kept so the debug pass can
    /// draw them. Requested by the user after three separate "I cannot walk
    /// here" reports that all turned out to be one wrong collider — a shape
    /// nobody could see was a shape nobody could argue with.
    struct DebugCollision {
        std::vector<glm::vec3> positions;
        std::vector<uint32_t> indices;
    };
    std::vector<DebugCollision> scene_collision_debug_;
    bool collider_debug_ = false;
    render::FireflyField fireflies_;
    std::vector<SceneTile> scene_tiles_;
    /// Every registry object the composition uses, near forms and `-far` forms
    /// alike, keyed by the name that was read. Kept resident because a re-bake
    /// must not go back to disk: it happens while the player is walking.
    std::map<std::string, render::RegistryObject> scene_objects_;
    /// Re-bakes at most ONE composition tile per frame into the form its
    /// distance asks for. One per frame, nearest mismatch first: the same
    /// stance the scatter ladder takes, for the same reason — a re-bake costs
    /// a bake, and the mismatch that matters is the one in front of the eye.
    void refresh_scene_lod(glm::vec3 eye);
    /// Builds and uploads one tile in the given form.
    void bake_scene_tile(SceneTile& tile, bool far_form);

    /// Where the CURRENT map's composition wants the player, if it said so.
    /// Recorded while the scene loads and consumed by the single spawn call at
    /// the end of enter_world — never spawned on the spot, because everything
    /// after that call (the character rig above all) must still run.
    std::optional<glm::vec3> scene_spawn_;
    /// DFN_THIRD_PERSON, fired once through the ordinary toggle branch.
    bool third_person_door_fired_ = false;
    float scene_spawn_yaw_ = 0.0f;
    /// The shelf list from `objects`, already split on ';' and trimmed.
    std::vector<std::string> gallery_shelves_;
    int gallery_size_chunks_ = 1; // Gallery extent, from the manifest
    std::vector<platform::PhysicsBodyHandle> gallery_bodies_;

    // Step feel + audio (sim's zone, wired here).
    platform::BusHandle sfx_bus_{};
    // МУЗЫКАЛЬНАЯ ШИНА — ВТОРАЯ ВЕТКА ОТ МАСТЕРА, а не громкость на голосе.
    // Ползунок обязан работать по всему, что играет как музыка, включая то,
    // чего ещё нет (боевые слои, стингеры); шина — единственное место, где
    // «вся музыка» можно назвать одним словом. И она нужна раньше дакинга:
    // приглушить музыку под реплику можно только тому, у кого есть своя ручка.
    platform::BusHandle music_bus_{};
    /// ШИНА МИРА — И ЭТО НЕ ЕЩЁ ОДНА ГРОМКОСТЬ, А ХОЗЯИН. Всё, что издаёт МИР
    /// (шаги, прыжки, приземления, всплески, ветер), играет здесь, и здесь же
    /// это глушится ОДНИМ движением, когда мир перестаёт идти. Она РЕБЁНОК
    /// sfx_bus_, а не его брат: ползунок «эффекты» обязан по-прежнему править
    /// всем, что издаёт мир. Гашение по хозяину и громкость по вкусу игрока —
    /// два РАЗНЫХ множителя, и сложить их в одну ручку значило бы стирать
    /// выбор игрока каждым выходом в меню.
    ///
    /// ПРАВИЛО, КОТОРОЕ ЭТА ШИНА ВЫРАЖАЕТ (полностью — в
    /// engine/platform/audio/docs/README.md): у каждого излучателя есть ХОЗЯИН,
    /// и звук замолкает вместе с ним. Хозяев сегодня двое: МИР и МЕНЮ.
    platform::BusHandle world_bus_{};
    /// Текущее приглушение шины мира, 0..1, и оно ползёт, а не прыгает: ветер,
    /// обрубленный за кадр, слышен как отвалившийся звук, а не как пауза.
    float world_gain_ = 1.0f;
    /// Сколько уже длится текущий пандус, в секундах. Существует РАДИ ЛОГА:
    /// «мир замолчал» без числа — это утверждение, которое нечем проверить, а
    /// затухание длиной в кадр и затухание длиной в секунду выглядят в логе
    /// одинаково. Обнуляется на каждом конце пандуса.
    float world_ramp_s_ = 0.0f;
    /// ЧАСЫ ЗВУКОВЫХ ПАНДУСОВ — СВОИ, СТЕННЫЕ. Не часы меню (на выходе в мир
    /// они не идут) и не шаг симуляции (в меню его нет): затухание живёт в ушах
    /// игрока, у которых есть только одни часы.
    std::chrono::steady_clock::time_point audio_tick_prev_{};
    /// ШИНА РЕЧИ, У КОТОРОЙ ПОКА НЕТ НИ ОДНОГО ГОЛОСА. Заведена по заказу
    /// владельца заранее, и это дешевле, чем кажется: ma_sound_group без
    /// источников не считает ничего. Зато диалоговая волна не будет заодно
    /// трогать страницу настроек, файл настроек и их рукава — она подключит
    /// голос к готовой ручке.
    platform::BusHandle voice_bus_{};
    gameplay::StepSoundBank sound_bank_{};
    /// ЗВУК МИРА ОТ ИСТОЧНИКА (заказ владельца 28.08). Здесь стоял WindLoop —
    /// один непространственный голос, который начинался при старте приложения
    /// и звучал одинаково везде, включая комнаты. Теперь мир звучит только из
    /// точек: кроны деревьев карты и русла её рек.
    gameplay::WorldAmbience ambience_;
    gameplay::WorldAmbience::Bank ambience_bank_{};
    /// Прибор дозы DFN_AMBIENCE_LOG: печатать таблицу излучателей раз в
    /// секунду. Живёт полем, а не статиком в кадре, потому что читается один
    /// раз при старте (доза Once), а печатает каждый кадр.
    bool ambience_log_ = false;
    double ambience_log_at_ = 0.0;
    gameplay::StepContext step_ctx_{};
    /// ЗАГЛАВНАЯ ТЕМА. Загружается один раз при старте (полный декод в память,
    /// ~37 МБ на 1:36 — решение записано в docs/DECISIONS.md и в шапке
    /// бэкенда) и живёт до конца прогона: перезаливать её на каждом входе в
    /// меню значило бы платить декодом за каждый выход из мира.
    platform::SoundHandle menu_theme_{};
    /// Играющий экземпляр темы, если он есть. Пустая ручка = меню молчит, и
    /// это ЕДИНСТВЕННОЕ состояние музыки, которое приложение хранит: чего
    /// хочет кадр, считает update_menu_music() из страницы меню.
    platform::MusicHandle menu_music_{};
    /// РОСЧЕРК ЗАСТАВКИ — ОДИН ВЫСТРЕЛ, А НЕ СОСТОЯНИЕ, и потому у него своя
    /// горстка полей, а не ветка внутри музыки. Тема — функция экрана (её
    /// считает update_menu_music каждый кадр); росчерк живёт СВОЮ длину, 5.6 с,
    /// и переживает конец интро-видео на два секунды с лишним: его хвост
    /// намеренно втекает в уже открывшееся меню, где тема уже играет. Свести
    /// это в один реконсилятор значило бы сделать «сколько времени прошло»
    /// частью вопроса «какой сейчас экран».
    platform::SoundHandle intro_sting_{};
    platform::AudioVoiceHandle intro_sting_voice_{};
    bool intro_sting_started_ = false; // засов: заставка бывает раз за запуск
    /// ГАШЕНИЕ РОСЧЕРКА СЧИТАЕТСЯ ПО СТЕННЫМ ЧАСАМ. Затухание — это доля
    /// секунды в ушах игрока, а не доля тика симуляции; и часы меню тут не
    /// годятся: на пропуске заставки они уже своё отсчитали. Длина > 0 значит
    /// «сейчас гасится» — отдельного флага для этого не нужно.
    std::chrono::steady_clock::time_point intro_sting_fade_begin_{};
    float intro_sting_fade_len_s_ = 0.0f;

    // First-person body (character's zone, wired here).
    anim::Rig body_rig_{};
    /// ТЕЛО МОДЕЛЬЮ, А НЕ КОРОБКАМИ (волна импорта и скиннинга, 30.08).
    /// Пусто, если .dfo персонажа не нашёлся или отказал: тогда рисуются
    /// прежние пятнадцать коробок, и это НЕ запасной путь, а вторая рука
    /// дозы DFN_BODY_BOXES (правило 47) — обе выходят из одного бинарника.
    SkinnedCharacter skinned_character_{};
    /// ТЕЛА ХИТБОКСОВ ИГРОКА в Jolt (BodyHitboxes.h). Своим слоем, отдельно от
    /// капсулы движения: капсула отвечает «куда пройти», хитбоксы — «во что
    /// попали», и один слой на оба вопроса заставил бы локомоцию цепляться за
    /// собственные локти.
    BodyHitboxes body_hitboxes_{};
    /// Дверь дозы: 1 — рисовать коробки, как до этой волны.
    bool body_boxes_ = false;
    ecs::EntityId mirror_puppet_{}; // DFN_MIRROR/DFN_SHOWCASE double, 0 when absent

    // BODY PROBE (Rule 27 evidence path for the body; DFN_BODY_PROBE=
    // stride|showcase|mirror). The screenshot Tour FREEZES the simulation, so
    // every animated subject in the project is invisible to it by construction:
    // no tick means no update_bodies, no stride clock, no clip reel. This probe
    // is the opposite instrument — the world RUNS and the camera is posed and
    // triggered off simulation state, so a frame can be demanded AT a named
    // stride phase or clip time. Debug tooling: gated, and it closes the app
    // when the shot list is spent.
    struct BodyProbe {
        std::string mode;            // stride | showcase | mirror
        std::string dir;             // output directory
        std::vector<float> targets;  // stride phase | clip time (s) | yaw offset
        size_t next = 0;             // index into targets
        int direction = 1;           // +1 targets ascend, -1 descend, 0 = cycle
        float warmup_s = 0.0f;       // streaming/settle time before the first shot
        float elapsed_s = 0.0f;
        float pitch = 0.0f;          // forced look pitch, radians
        float aim_yaw = 0.0f;        // resolved at warmup end (mirror/showcase)
        bool aimed = false;
        bool primed = false;         // one frame of history before triggering
        float value = 0.0f;          // this frame's tracked quantity
        float prev_value = 0.0f;     // last frame's, for crossing detection
        float tick_value = 0.0f;     // tracked quantity at the newest tick
        float prev_tick_value = 0.0f;
        int cooldown = 0;            // frames before another shot may be scheduled
        std::string log;             // one line per shot, written next to the frames
    };
    std::optional<BodyProbe> body_probe_;

    // Autonomous playtest (sim's zone; DFN_PLAYTEST=patrol|explore|soak).
    std::optional<gameplay::PlaytestState> playtest_;
    gameplay::PlaytestCheckEnv pt_env_{};
    std::string pt_dir_;
    int pt_shots_ = 0;
    bool pt_artifacts_pending_ = true;
};

} // namespace dfn::app
