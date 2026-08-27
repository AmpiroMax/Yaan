/*
Created: 09:08:2026 - 00:45:00
Last updated: 27:08:2026 - 21:40:30
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
/*
UPD:
- 09:08:2026 - 00:45:00: Created for stage 2 integration.
- 09:08:2026 - 00:48:00: Adopted sim's free-function movement API (Rule 9) —
                         player entity id held here, no system object.
- 09:08:2026 - 10:48:00: palette_post config flag (Q9b, stage-3 render batch).
- 09:08:2026 - 12:49:12: Graphics settings file (user decision, sync #3): settings.cfg
                         read at startup, auto-generated on first run; env
                         overrides file for tooling.
- 09:08:2026 - 17:16:27: world_edge_: static walls at the generated extent.
- 09:08:2026 - 19:12:24: game clock (day/night cycle) held here.
- 09:08:2026 - 22:24:44: Игровые часы стартуют с START_TIME_OF_DAY, а не с нуля — ноль это полночь, и свежий запуск открывался в темноте.
- 10:08:2026 - 02:39:07: Audio, step context, first-person body rig and the autonomous playtest join the composition root (landscape stage wiring).
- 10:08:2026 - 10:52:00: BodyProbe (DFN_BODY_PROBE) — the acceptance-frame path
                         for anything ANIMATED. The Tour freezes the tick, so it
                         can photograph only still life; this probe runs the
                         world and triggers the shot off simulation state.
- 10:08:2026 - 10:28:59: Menu-first launch: init() raises the engine, enter_world() builds a chosen demo map (user request: check different maps, with and without the menu).
- 10:08:2026 - 19:26:40: Отладочный экран (F3) и снимок состояния (F2) с восстановлением по DFN_RESTORE — запрос пользователя: видеть куда смотрю, fps, скорость, координаты, и уметь передать состояние так, чтобы его подняли обратно.
- 10:08:2026 - 19:57:06: Поле счётчика попыток доводки восстановления.
- 10:08:2026 - 20:03:30: Счётчик попыток доводки более не используется — восстановление стало размещением.
- 10:08:2026 - 21:26:54: Поля признака тишины мира для затвора тура.
- 10:08:2026 - 22:37:21: hold_crouch_ -- a restored crouch survives the live keyboard, which is what makes an automated capture at full crouch possible at all (character's carve).
- 10:08:2026 - 23:32:21: Поле msaa_samples в настройках.
- 10:08:2026 - 23:51:30: Поля третьего лица и орбиты камеры.
- 11:08:2026 - 13:48:13: DFN_FRAME_LOG — по строке на каждый ПРЕДЪЯВЛЕННЫЙ кадр, без обратного чтения, без отстоя, без заморозки тика. Пользователь нашёл изъян нашего метода раньше нас: «при прогоне бега тряска есть, а в момент, когда делается скрин, тряски нет». Все наши двери съёмки гасят ровно то, на что наведены, поэтому дефект МЕЖДУ кадрами два дня приходил чистым. Первый же прогон дал размах fov_y 5.951° при беге против 0.0000° на ходьбе и стоя.
- 13:08:2026 - 17:21:38: Переправа мешей демо-предметов (геометрия sim, переправа здесь). Без неё три предмета появлялись с идентификатором меша, который никто не загрузил, и рисовались НИЧЕМ: дверь 1.8 × 2.0 м стояла невидимой в 2.5 м перед точкой старта, при том что луч попадал в её физическую коробку, наведение заполнялось честно и «Открыть» рисовалось поверх пустой травы.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
- 13:08:2026 - 22:14:05: capture_after_frames_ — вторая единица счёта для той же двери снимка. Секунды несравнимы побитово: две руки одного рецепта на разной загрузке машины успевают разное число кадров.
- 14:08:2026 - 16:11:00: AppMode::Editor + свободная камера (EditorCamera). Новый режим летающей камеры (запрос пользователя В39/Л1): облёт мира не игроком; Tab вселяет камеру в игрока и обратно. Дверь DFN_EDITOR=1 (+DFN_EDITOR_CAM=x,y,z,yaw,pitch) — авто-прогон через дверь, не забирающий мышь.
- 14:08:2026 - 16:50:36: Браузер карт (контракт docs/MAP_LAYOUT.md): MapCatalog + current_manifest() (сим для зоны chat — путь чата из category/file_stem). Вход в Играть/Редактор открывает браузер; open_map() разрешает source (stand:/dfw:). Двери: DFN_OPEN_MAP=<кат>/<карта> грузит карту минуя браузер (взамен прежней DFN_EDITOR-в-мир; DFN_MAP занят render'ом), DFN_EDITOR=1 без карты открывает браузер редактора.
- 14:08:2026 - 17:36:02: Поля/методы чата, телеметрии и записи/повтора траектории (В28/O-серия): chat_pending_/chat_pending_entry_, write_pending_chat()+chat_path_for_current_map() (путь из current_manifest()), TelemetryRing telemetry_, TrajectoryRecorder/Player (O3). Включены ChatLog.h и TrajectoryRecord.h.
- 14:08:2026 - 17:51:15: Поле wireframe_ (клавиша 4/F4, каркас В28). Оверлеи редактора читают renderer_->frame_stats()/center_pick() напрямую.
- 14:08:2026 - 18:03:08: ChatOverlay chat_overlay_ (живое окно чата, В28) + include ChatOverlay.h. Открытие '/', ввод text_input(), Enter — отправка через write_pending_chat.
- 14:08:2026 - 19:14:02: Поля двери снимка (shot_after_frames_/_seen_) рядом с capture_after_*: та же единица счёта и тот же довод — кадры сравнимы побитово, стенные секунды нет. Отдельного флага закрытия не заведено, переиспользован chat_then_close_: снимок клавиши 5 И ЕСТЬ запись чата, значит и выключение то же.
- 14:08:2026 - 19:41:18: action_pressed() + include Controls.h — обработчики клавиш спрашивают привязку ПО ДЕЙСТВИЮ, а не называют Key здесь. Это и есть то, что не даёт экрану управления разъехаться с кодом. Половина от 83ef021: уехала в рабочем дереве, доезжает отдельно.
- 15:08:2026 - 01:04:30: gallery_objects_dir_ + gallery_bodies_ (полка реестра и твёрдые стволы).
- 15:08:2026 - 02:14:41: gallery_size_chunks_ — пролёт галереи из манифеста (колоссу нужен 2×2).
- 16:08:2026 - 21:08:52: gallery_scene_ — файл композиции следующей карты (пусто = старая
  автосетка).
- 16:08:2026 - 21:50:43: gallery_shelves_ — полки карты, уже разобранные из objects.
- 17:08:2026 - 07:05:56: scene_spawn_ / третье лицо по двери (см. App.cpp).
- 17:08:2026 - 10:00:40: SceneTile + scene_objects_ + refresh_scene_lod/bake_scene_tile.
- 17:08:2026 - 11:13:47: FireflyField живёт на КАРТУ, а не на чанк — рой у края стриминга не
  должен мигать (пользователь: «повсюду, а не только в какой-то зоне»).
- 17:08:2026 - 11:35:28: scene_doc_ — композиция текущей карты, прочитанная до земли.
- 17:08:2026 - 13:52:37: scene_collision_debug_ / collider_debug_ — DFN_DRAW_COLLIDERS.
- 17:08:2026 - 14:48:55: draw_bake_progress — экран первого запуска.
- 17:08:2026 - 16:27:55: AppConfig::fullscreen — режим, в котором рождается окно (settings.cfg).
- 17:08:2026 - 16:35:20: scene_dirty_ — правил ли кто-нибудь композицию в этой сессии.
- 17:08:2026 - 18:32:56: состояние руки строителя: палитра, призрак, приговор, цель удаления,
  запомненные мерки деталей. Решения — в BuildTool.{h,cpp}, здесь только провода.
- 17:08:2026 - 19:17:13: Поле editor_ui_ — каркас интерфейса редактора (EditorUi). Панелей этот файл не называет ни одной: они регистрируются сами через EditorUi::add_panel, и это то, что позволяет трём агентам добавлять инструменты в редактор, не правя втроём один файл.
- 17:08:2026 - 22:01:29: ЧЕТЫРЕ ОСТАЛЬНЫХ РЕЖИМА ПЕРЕСТАЛИ БЫТЬ ПУСТЫМИ (заказ 17.08 п.2:
  «состояние на R меняется, но инструменты не рисуются, не понятно что сейчас я
  делаю и что»). Поля: слой правок земли relief_, кисть рельефа и посадки,
  мазок, выбранная расстановка selected_ (это НЕ build_target_ — тот меняется
  от дрожания камеры, а править числами надо то, по чему ЩЁЛКНУЛИ), и имя
  текущей постройки build_group_name_. Решения — в EditorBrush/EditorPlant/
  BuildTool; здесь только провода, потому что этот файл держит окно и ничего
  собранного в нём измерить нельзя.
- 17:08:2026 - 22:32:14: flatten_written_ — один [pad] на МАЗОК, а не на кадр: pad это
  утверждение, которое композитор перечитывает, и шестьдесят штук в секунду
  похоронили бы файл, в котором ему жить.
- 18:08:2026 - 00:07:07: cam_trace_ — дверь DFN_CAM_TRACE=1: печатать в stderr
  пару «пришло смещение мыши / стал рыск» на каждом кадре редактора. Заведена
  не для отладки одного вечера: три захода подряд «камера не двигается»
  разбирал человек за игрой, потому что различить «мышь не дошла» и «камера
  проигнорировала» было нечем. Читается один раз при рождении App.
- 18:08:2026 - 01:54:26: ghost_uploaded_ — висит ли призрак В РЕНДЕРЕРЕ. Отдельно от build_ghost_:
  «что я держу» и «что загружено» — разные вопросы, и путать их значит оставлять
  деталь нарисованной после того, как её выпустили.
- 18:08:2026 - 12:07:50: ИНСТРУМЕНТЫ ПЕРЕЕХАЛИ В КЛАССЫ (docs/AUDIT_EDITOR_TOOLS.md).
  Отсюда ушли поля, которые были ЧУЖИМ состоянием, лежавшим у App: cursor_free_
  (теперь EditorToolbox::pointer_mode — клавиша R это часть контракта
  инструментов), terrain_brush_/plant_brush_/brush_stroke_/flatten_written_
  (у каждой кисти теперь своя, внутри своего инструмента) и build_open_
  (список объектов стал настройками инструмента постройки, а не отдельной
  панелью со своей клавишей). apply_terrain_dab принимает КИСТЬ аргументом:
  App больше не знает, какая кисть сейчас в руке, и не должен.
- 18:08:2026 - 13:08:07: ЗЕМЛЯ ДВИГАЕТСЯ, ПОКА ВЕДЁШЬ КИСТЬ, и ТРОПА КРИВОЙ — два заказа 18.08.
  Первый: перестройка чанка звалась только из finish_stroke, поэтому кисть вела по
  неподвижной земле («мне так непонятно что происходит»). Теперь показ идёт во время
  штриха с паузой, выведенной из ИЗМЕРЕННОЙ цены (196 мс на чанк) — StrokeRefresh.
  Второй: шестой инструмент (PathTool) и крючки под него — ground_height, relief_paths,
  commit_path, last_dab; линия и узлы рисуются из ToolPreview, без вопроса «что в руке».
  И ПОПУТНО ЗАКРЫТА СТАРАЯ ДЫРА: приложение НИ РАЗУ не звало read_relief/write_relief —
  ключ `relief` в .scene был, формат был, круговой прогон был, а правки земли жили до
  выхода из игры. Теперь сиделка читается со сценой и пишется кнопкой «сохранить».
- 18:08:2026 - 16:59:18: СЛОЙ 1 РАЗБОРА: dispatch_actions() и семнадцать методов on_*,
  определённых в AppInput.cpp. Поле force_third_person_ удалено — оно
  существовало только затем, чтобы дверь DFN_THIRD_PERSON попала в ту же ветку,
  что и клавиша; теперь дверь зовёт тот же метод, и промежуточный флаг стал
  лишним звеном (правило 32). unattended_run()/write_settings() объявлены здесь:
  обработчики клавиш уехали в соседний файл и зовут их оттуда.
- 18:08:2026 - 17:32:10: СЛОЙ 2: объявление unattended_run() отсюда убрано — оно
  живёт в AppDoors.h рядом с таблицей, из которой выводится.
- 18:08:2026 - 17:36:58: СЛОЙ 4: after_frame(alpha, dt) и два затвора вместо трёх
  голых счётчиков — FlushCountdown вместо close_after_flush_, SettleGate вместо
  quiet_frames_/tour_settle_frames_. Оба определены в AppAfterFrame.h заголовком
  и потому прогоняются рукавом без окна; здесь остаётся только их состояние.
- 18:08:2026 - 18:02:11: Поле house_ — постройка, которую правят три инструмента графа, и она же
  та модель, которой не хватало отмене. ОДНА на троих: копия у каждого
  инструмента — это три дома, расходящиеся на первом сдвинутом якоре.
- 18:08:2026 - 18:58:40: Объявлен on_axis_lock.
- 18:08:2026 - 20:26:30: Объявлен on_delete_selected.
- 18:08:2026 - 21:12:40: upload_house_mesh, seed_demo_house, версия залитого тела.
- 18:08:2026 - 21:38:05: Тело коллайдера постройки и буфер его вершин.
- 18:08:2026 - 23:20:00: nudge_selected_anchor, draw_editor_grid; сетка живёт в сессии, а не вторым полем здесь.
- 18:08:2026 - 23:52:10: Объявлен on_grid_toggle.
- 20:08:2026 - 00:02:30: house_material_swatch / house_wall_example и их кэш.
- 20:08:2026 - 01:58:54: unload_world() и три подписки моста мира (снос предыдущего мира при
  повторном enter_world); прицел кадра frame_aim_ + aim_this_frame()/invalidate_frame_aim()
  — марш в 160 шагов считался по четыре раза за кадр.
- 20:08:2026 - 15:30:00: PlacedHouse + load_scene_houses (готовые постройки карты); дверь DFN_RECORD_EVERY (лента прохода).
- 20:08:2026 - 22:40:00: PlacedHouse.scene_index — распаковка брала соседний дом при нечитаемом файле (аудит #3, находка 1).
- 20:08:2026 - 23:59:00: Указатели инструментов постройки для «стиль в заготовку».
- 24:08:2026 - 01:30:00: И15 волна А — состав интерьера-локации: карман, второй
  слот с телом и страховочной плитой, снятые на время локации лампы и небо
  города, переходы как вещи мира, экран загрузки, замеры входа и выхода.
  upload_house_mesh получил interior_only: вход в дом не имеет права
  перестраивать город (1087 построек Вайтрана — 18.9 с против отпущенных 0.5).
- 24:08:2026 - 03:20:00: hold_loading_screen() и loading_shot_ — настоящая
- 27:08:2026 - 01:20:00: И15 ВОЛНА Б, БОЛВАНКИ. PortalLink дорос полями house/to/
  to_spawn: переход у СТВОРКИ ПОСТРОЙКИ не адресует запись композиции — его
  цель приходит из ключа interior= самого дома, а место берётся у геометрии.
  Отсюда HouseDoorway/house_doorways_ и spawn_house_portals(): мировые
  координаты дверного полотна знает ТОЛЬКО заливка построек, и вторая их
  запись (строкой [portal] в сцене города) была бы вторым ответом на один
  вопрос — правило 39. Пустой `to` значит ЗАПЕРТО, и это не отсутствие
  данных, а состояние: дом-болванка без внутренности обязан отвечать
  надписью, а не молчанием.
  длительность экрана загрузки и его единственный снимок за прогон.
- 27:08:2026 - 14:00:00: editor_session_ / set_editor_session() — право строить как
  свойство ЗАПУСКА мира, а не переключаемое состояние (заказ владельца 27.08).
  Довод — у самого поля.
- 27:08:2026 - 14:30:00: DoorwayKind (Portal/Locked/Decorative) и обратный адрес у
  PortalLink/HouseDoorway — «куда переносит игрока» стало свойством ДВЕРИ, а не
  позы игрока в момент нажатия (заказ владельца 27.08). Плюс loading_map_name_
  (заголовок экрана загрузки нужен ДО постройки мира, а current_map_
  присваивается после удачи) и пара load_step/load_tick: «отметить этап» и
  «показать кадр» ходят вместе всегда — этап, отмеченный без кадра, виден
  прибору и невидим человеку, и ровно этим экран загрузки города был до
  сегодня. Доводы — у самих полей.
- 27:08:2026 - 12:02:02: КАМЕРА ТРЕТЬЕГО ЛИЦА ПОЛУЧИЛА ОБОЛОЧКУ (заказ владельца
  27.08: «находясь в помещении, могу за границы посмотреть»). Поля стрелы
  (CameraBoomState/Desc) и её прибора живут здесь, потому что доза
  DFN_CAM_COLLIDE обязана сниматься с ЖИВОГО вида, из того же бинарника, что и
  рабочая рука. Три метода: cam_collide_enabled (доза), cam_probe_step и
  cam_probe_report (прибор, меряющий ЛУЧОМ то, чем стрела управляет через
  сферкаст).
- 27:08:2026 - 12:58:00: menu_emblem_ / menu_lights_ / menu_cost_ms_ — поля
  объёмного герба главного меню (заказ владельца 27.08). Герб заливается
  один раз при первом показе корня и живёт до конца прогона: 8.5 МБ на
  полке и одна пара буферов дешевле, чем перезаливать их на каждом входе в
  меню.
- 27:08:2026 - 20:10:06: МУЗЫКАЛЬНАЯ ШИНА И ЗАГЛАВНАЯ ТЕМА В МЕНЮ (заказ
  владельца через музыкальную сессию: «заглавная тема играет в главном меню
  на репите»). Поля music_bus_ / menu_theme_ / menu_music_, настройки
  music_volume / sfx_volume, методы update_menu_music() и
  sync_audio_volumes(). ОДИН СЧИТАТЕЛЬ СОСТОЯНИЯ ВМЕСТО ШЕСТИ ПЕРЕХОДОВ —
  довод у объявления метода.
- 27:08:2026 - 20:28:48: voice_bus_ и voice_volume — ТРЕТЬЯ шина (дополнение
  заказа того же дня: «три ползунка — музыка, эффекты, РЕЧЬ»). Голосов ещё
  нет; шина и её настройка заведены заранее, чтобы диалоговая волна подключала
  голос к готовой ручке, а не заводила ручку заодно с голосом.
- 27:08:2026 - 20:49:56: ЗВУК ЗАСТАВКИ (intro_sting_*, update_intro_sting,
  fade_intro_sting, menu_audio_allowed). Росчерк — ОДИН ВЫСТРЕЛ со своей
  длиной, а не состояние экрана: он на два с лишним секунды длиннее интро-видео
  нарочно, и его хвост звучит поверх уже открывшегося меню. Довод против
  сведения его в реконсилятор темы — у полей.
- 27:08:2026 - 21:40:30: ПРИЦЕЛ ДВЕРИ = РАДИУС + ВЗГЛЯД (крит владельца 28.08:
  «захожу в дом — сразу горит „выйти"; дверь ловит нажатие по радиусу от неё, а
  не по радиусу + взгляд»). У HouseDoorway появился ГАБАРИТ ПОЛОТНА (half_w,
  half_h), у PortalLink — DoorAim, у зоны — interior_doorways_ (створки
  локации: обратный [portal] это точка без нормали, а прицелиться изнутри можно
  только по полотну). Плюс doorway_aim/door_aim_now/filter_door_hover/
  probe_door_aim и доза door_aim_enabled.
*/

#pragma once

#include <chrono>

#include "engine/anim/sources/Rig.h"
#include "engine/app/sources/ChatLog.h"
#include "engine/app/sources/ChatOverlay.h"
#include "engine/app/sources/BuildTool.h"
#include "engine/editor/sources/EditorPaletteView.h"
#include "engine/app/sources/AppAfterFrame.h"
#include "engine/app/sources/Controls.h"
#include "engine/editor/sources/EditorHistory.h"
#include "engine/app/sources/DebugOverlay.h"
#include "engine/app/sources/DoorAim.h"
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
    // разбора App.cpp, docs/PLAN_APP_DECOMPOSITION.md). Обходит таблицу
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
    // THIRD PERSON (key 1), his request: a debug view from behind. Standing
    // still the mouse orbits the camera and the body does NOT turn; moving, the
    // camera locks behind him -- the Skyrim behaviour he named.
    bool third_person_ = false;
    float orbit_yaw_ = 0.0f;
    float orbit_pitch_ = 0.0f;
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
    uint64_t cam_probe_frames_ = 0;  // сколько кадров прибор насчитал
    uint64_t cam_probe_outside_ = 0; // на скольких камера оказалась за оболочкой
    float cam_probe_worst_ = 0.0f;   // худший заход за стену, м
    bool debug_overlay_ = false;    // key 2 (F3 alias)
    // Whole-scene wireframe (В28), key 4 / F4. Toggles IRenderer::set_wireframe;
    // the editor overlay reads it back to label the mode. Off by default, zero
    // cost off (render's contract).
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
    // ЗАТВОР ТУРА: гистерезис и потолок, вынесенные в AppAfterFrame.h, где их
    // прогоняет рукав. Здесь было два счётчика и два литерала в кадровом цикле.
    SettleGate settle_gate_;

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
    gameplay::WindLoop wind_loop_{};
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
