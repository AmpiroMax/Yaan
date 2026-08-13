/*
Created: 09:08:2026 - 00:45:00
Last updated: 13:08:2026 - 22:14:05
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
/*
UPD:
- 09:08:2026 - 00:45:00: Created for stage 2 integration.
- 09:08:2026 - 00:48:00: Adopted sim's confirmed free-function movement API
                         (spawn_player / accumulate_input / pre_step / post_step);
                         camera poses read from the player entity's components.
- 09:08:2026 - 10:16:00: ChunkCoord uses x/z (not x/y) — ferry fixed.
- 09:08:2026 - 10:20:00: Switched to core's open_generated() (open(file) is
                         stage 3) — the actual init failure after reboot.
- 09:08:2026 - 10:32:00: Tour vantages offset by spawn ground height (render's
                         underground-camera diagnosis; Rule 26 ack recorded).
- 09:08:2026 - 10:48:00: DFN_PALETTE=1 wired to RendererInitParams.palette_post
                         (stage-3 render batch, Rule 26).
- 09:08:2026 - 12:05:00: Stage-3b ferry: surface-aware terrain upload, scatter
                         upload/drop, per-body water; Tour v3 (testbed steps,
                         per-frame ground resolution, tour-driven streaming
                         focus, frozen player during tour).
- 09:08:2026 - 12:49:12: settings.cfg (graphics settings — user decision, sync #3):
                         internal_resolution + palette read from file,
                         auto-generated on first run; env overrides intact.
- 09:08:2026 - 15:07:13: CRITICAL fix (user report: fell through the world on
                         launch): terrain bodies were built here with
                         TerrainDesc::layer left at 0 — colliding with nothing.
                         Ferry now uses physics::create_terrain_body (owns the
                         decode and LAYER_STATIC). Also: pump the chunk events
                         before spawning, and stream before stepping.
- 09:08:2026 - 16:59:02: Voxel world: terrain collision ferried from
                         ChunkManager::voxel_mesh via create_terrain_mesh_body
                         (heightfield bodies cannot carry the tunnel ceiling).
- 09:08:2026 - 17:16:27: World-edge walls: past the generated extent there is
                         no terrain, and sprint speed made falling out of the
                         world a 20-second accident (sim's finding).
- 09:08:2026 - 17:32:38: Map screen wired: M toggles it, cursor released while
                         open, canvas told the internal resolution.
- 09:08:2026 - 19:12:24: Day/night clock wired: 48-minute day, T holds for a
                         50x debug run, lunar phase as a pure function of date.
- 09:08:2026 - 19:21:01: Terrain DRAWN from the voxel mesh (render's finding:
- 09:08:2026 - 20:27:13: ambient_darkness written per frame — a stand-in for
- 09:08:2026 - 20:38:09: ambient_darkness now asks core's enclosure query;
                         the app-side stand-in is deleted.
                         core's enclosure query so interiors are dark in play.
                         there was no voxel render path at all, so carves were
                         never submitted — the reported "saw the map from
                         inside the barrow" was missing geometry, not light).
- 09:08:2026 - 22:34:17: Взаимодействие подключено к игре: предметы, три пробных объекта (взять/открыть/использовать), столкновения с реквизитом, наведение, действия, переносимый свет, модель рук. Всё это существовало и не вызывалось ни разу.
- 09:08:2026 - 22:38:29: Настоящие номера моделей руки (32) и факела (33) вместо заглушек — render их завёл.
- 09:08:2026 - 22:47:13: Карта снова записывает разведанное: высотное поле едет вместе с воксельной выгрузкой (пометка кусков висела на старом пути и молча отвалилась). Плюс новая сигнатура действий игрока — выбрасывание предметов требует физики.
- 09:08:2026 - 22:49:12: Мир встаёт на паузу с открытым инвентарём (как в TES). Три системы продолжают работать — иначе из меню не выйти. Накопитель шагов сбрасывается, чтобы на выходе не выстрелить пачкой догоняющих тиков.
- 09:08:2026 - 23:30:34: Мир стал 2×2 км (WORLD_EXTENT_CHUNKS 8) — прямая просьба пользователя. Размер мира перестал быть голым числом в исходнике.
- 09:08:2026 - 23:50:20: Ферри дальней детализации: границы мира от core, прямоугольник по сетке чанков, обновление по КАДРОВОМУ времени, сбор по ожидающим узлам, меш уничтожается раньше поля.
- 10:08:2026 - 00:04:04: Подсказки взаимодействия рисуются на экране. Первый настоящий текст в игре: таблица строк грузится из данных, промах даёт заметную заглушку, а не пустоту.
- 10:08:2026 - 02:44:09: Большая проводка ландшафтного этапа: аудио (слушатель, ветер, шаги), контекст шага, тело от первого лица (ферри BodyDrive от часов шага sim), зеркальный двойник (DFN_MIRROR/DFN_SHOWCASE), автономный плейтест (DFN_PLAYTEST), связь угла обзора со скоростью, строка head_bob в настройках.
- 10:08:2026 - 10:36:22: Запуск через МЕНЮ: init() поднимает движок, enter_world() строит выбранную демо-карту. Стартовый экран, выбор карты, пауза по Esc. DFN_MENU=0/DFN_MAP для инструментов; тур и плейтест выключают меню сами.
- 10:08:2026 - 11:37:37: Ферри поверхностей дорожек и маршрут съёмки по точкам стенда — лесок стал фотографируемым.
- 10:08:2026 - 11:40:12: Выбор стенда переехал с DFN_MAP на DFN_STAND — DFN_MAP уже был щупом экрана карты у render, и маршрут стенда молча схлопывался в один кадр.
- 10:08:2026 - 19:26:40: Отладочный экран, снимок состояния и восстановление. Заодно убран ВТОРОЙ обработчик Esc: он звал request_close(), поэтому Esc открывал паузу И закрывал игру — экран паузы существовал, но увидеть его было нельзя.
- 10:08:2026 - 19:44:12: DFN_HEAD_BOB и DFN_PLAYTEST_GAIT — двери к контролю движения и к передаче бота (запрос sim: без них автоматический прогон умел мерить ТОЛЬКО шаг). Неверное значение отвергается ГРОМКО: молчаливый откат к умолчанию воспроизвёл бы ровно тот дефект, ради которого дверь и открыта.
- 10:08:2026 - 19:57:06: Три починки снимка состояния, все три найдены sim при проверке инструмента, а не при его использовании: хэш сборки штампуется во время СБОРКИ (был — при конфигурации, и называл сборку на два коммита старше), восстановление доводится итеративно (промах был 0.53 м), и вычитается ВЫНОС глаза вперёд, а не только высота — иначе круговой прогон уводил игрока на 0.10 м вперёд каждый раз.
- 10:08:2026 - 20:03:30: Восстановление через teleport_character вместо самодельной доводки. Прежний комментарий утверждал, что телепорта в IPhysics нет — предпосылка была моя, непроверенная (grep по неверному имени), и успела уйти в бриф core. Промах упал с 0.53 м до 0.001 м; вся машинерия доводки удалена.
- 10:08:2026 - 20:05:06: DFN_CAPTURE_DIR на существующий каталог убивал процесс до загрузки мира (бросающая форма create_directories) — render потерял на этом три прогона, и прогон, не измеривший НИЧЕГО, выглядел как измеривший ноль.
- 10:08:2026 - 20:08:54: Передача gait в BodyDrive включена: походку выбирает передача, а не сравнение скорости с числами. Закрывает и трусцу-с-наклоном-0.286, и окно регрессии, в котором ВСЕ передачи рисовались шагом.
- 10:08:2026 - 20:20:17: Рука вида от первого лица объявляла меш 32, которого никто никогда не строил, — она рисовалась как НИЧТО с самого дня проводки. Отсутствие теперь объявлено, а не получается случайно; тело и так рисует настоящую правую кисть.
- 10:08:2026 - 20:25:36: chunks_.update() вынесен ИЗ цикла догоняющих шагов — он вызывался раз на ШАГ, поэтому после медленного кадра догон впускал пять кусков подряд по 83 мс. Задержка на пересечении границы 730 мс → 39.8 мс.
- 10:08:2026 - 20:43:18: Наклон глаза берётся из СГЛАЖЕННОГО веса походки, а не из передачи: корпус и глаз обязаны наклоняться одним числом, иначе на торможении с бега грудь возвращается.
- 10:08:2026 - 21:14:51: Меню выключают ВСЕ автоматические двери, а не только тур и плейтест: снимок, зонд тела и восстановление зависали на стартовом экране и фотографировали меню (жалоба пользователя: «они в меню зависают все»).
- 10:08:2026 - 21:26:54: Тур снимается на СЧЁТНЫХ часах, а не настенных: затвор ждёт тишины подгрузки, часы игры и растворение детализации идут фиксированным шагом. Собственный контроль тура 27.67% → 14.73%; остаток НЕ объяснён.
- 10:08:2026 - 21:32:18: Зонд тела считается желающим двойника: без этого он снимал пустую поляну, и кадр читался как «тело не рисуется». Третий немой ноль за день и один и тот же баг — ПРЕДУСЛОВИЕ, записанное списком тех, кому оно тогда понадобилось.
- 10:08:2026 - 21:41:45: Карта грузится из данных, а не вкомпилирована: 441 строка обзора ОДНОЙ игры уезжает из движка. Отказ загрузки — фатален, потому что откат к вкомпилированным значениям дал бы почти правильный мир, которого никто не искал бы.
- 10:08:2026 - 22:37:21: THE CROUCH FERRY (character's carve): crouch_eye travels back the same way the lean does, so the camera and the posed body agree on how deep a squat is. Plus the two halves of a crouched RESTORE that never worked -- the snapshot's `crouched` is applied and HELD (accumulate_input rewrites it from a keyboard nobody is at), and the feet are derived with the crouch offset instead of the standing eye height.
- 10:08:2026 - 23:32:21: Настройка msaa в settings.cfg рядом с разрешением и палитрой: это то, что остановило рябь на линии леса, и понижать её — зрительная регрессия, а не только производительность. Неверное значение отвергается ГРОМКО.
- 10:08:2026 - 23:51:30: Клавиши по запросу пользователя: 1 — вид от третьего лица (стоя мышь вращает камеру ВОКРУГ персонажа и он не поворачивается, в движении камера встаёт за спину), 2 — отладочный экран, 3 — снимок состояния. F2/F3 оставлены псевдонимами, иначе все записанные рецепты съёмки стали бы неверными. В третьем лице возвращается голова — в первом она скрыта намеренно.
- 11:08:2026 - 13:48:13: DFN_FRAME_LOG — по строке на каждый ПРЕДЪЯВЛЕННЫЙ кадр, без обратного чтения, без отстоя, без заморозки тика. Пользователь нашёл изъян нашего метода раньше нас: «при прогоне бега тряска есть, а в момент, когда делается скрин, тряски нет». Все наши двери съёмки гасят ровно то, на что наведены, поэтому дефект МЕЖДУ кадрами два дня приходил чистым. Первый же прогон дал размах fov_y 5.951° при беге против 0.0000° на ходьбе и стоя.
- 13:08:2026 - 15:56:20: DFN_PLAYTEST_ROUTE — маршрут бота абсолютными мировыми координатами. Запрошен зоной dungeon: маршрут patrol был зашит на четыре точки в двух метрах от спавна, притом что PLAYTEST.md сам называет его назначением «scripted acceptance walks (the crag tunnel, the castle ford)». То есть ни один автоматический прогон никогда не был ВНУТРИ чего-либо, а подземелья стоят в 500 м. Кривое значение отвергается ВСЛУХ: молчаливый откат на кольцо у спавна дал бы прогон, отчитывающийся «ходил по тоннелю» и померивший лужайку.
- 13:08:2026 - 16:14:09: DFN_PLAYTEST_ROUTE теперь САМ включает patrol. Разбор маршрута лежал внутри ветки DFN_PLAYTEST, поэтому маршрут без режима тихо не делал ничего — зона dungeon потеряла на этом прогон в 150 секунд, простояв на месте. Это ровно тот молчаливый ноль, против которого построена вся эта оснастка; у «вот маршрут, иди по нему» второго прочтения нет, значит значение несёт намерение, а режим следует за ним. Сообщение печатается вслух.
- 13:08:2026 - 16:26:16: Меню чинится двумя правками по находке зоны ui. DFN_MENU_SHOT УБРАНА из списка пропуска меню: это единственная дверь, которой меню НУЖНО, а присвоение show_menu=false стоит ПОСЛЕ разбора DFN_MENU, поэтому дверь, существующая ради снимка стартового экрана, каждый раз уходила в мир и не сняла его ни разу. И новая DFN_MENU_PAGE=root|maps|pause: без неё выбор карты и пауза достижимы только рукой на клавиатуре, то есть два из трёх экранов, которые видит игрок, никогда не были доказательством. Неизвестное значение отвергается вслух — кадр корня, подшитый под именем паузы, хуже отсутствия кадра.
- 13:08:2026 - 16:43:43: DFN_PLAYTEST_ROUTE добавлена в список пропуска меню — в тот же день, что и заведена. Собственная проверка нашла дыру: «маршрут включает patrol» оказалось мало, потому что блок плейтеста живёт внутри enter_world(), и автоматический прогон с одним маршрутом вечно стоял на СТАРТОВОМ ЭКРАНЕ. Починка молчаливого нуля породила второй молчаливый ноль этажом выше, ровно то, о чём предупреждает комментарий у самого списка, — и увидеть это удалось только прогоном.
- 13:08:2026 - 17:00:50: Подсказка взаимодействия получила плашку (кусок от зоны ui, применён здесь). Она была последним текстом без подложки, при том что рисуется поверх ЧЕГО УГОДНО, на что смотрит игрок: те же чернила, тот же шрифт, тот же замер — 56.1% чернил не проходят правило двух шагов над светлым фоном. Плашка вынесена ui в ОДНУ функцию до применения: она была уже трижды копией, и эта была бы четвёртой.
- 13:08:2026 - 17:17:04: НИ ОДИН АВТОМАТИЧЕСКИЙ ПРОГОН БОЛЬШЕ НЕ ЗАБИРАЕТ МЫШЬ. Жалоба пользователя, работавшего за машиной, пока агенты снимали кадры: «когда запускаются визуальные тесты у меня управление компом перехватывается, меня в игру перекидывает, мышью управлять не могу». Освобождение было написано для ОДНОЙ двери (пробы тела), а не для СВОЙСТВА, которое у дверей общее: у автоматического прогона некому целиться, значит ему незачем владеть указателем. Заведён `unattended_run()` — одно определение на двух потребителей, пропуск меню и захват курсора (правило 35). Все четыре места захвата теперь зовут его.
- 13:08:2026 - 17:21:38: Переправа мешей демо-предметов (геометрия sim, переправа здесь). Без неё три предмета появлялись с идентификатором меша, который никто не загрузил, и рисовались НИЧЕМ: дверь 1.8 × 2.0 м стояла невидимой в 2.5 м перед точкой старта, при том что луч попадал в её физическую коробку, наведение заполнялось честно и «Открыть» рисовалось поверх пустой травы.
- 13:08:2026 - 18:13:27: Факел и рычаг подняты с 0.5 м на 1.3 м. Замер sim: глаз на 1.7 м, предмет на 0.5 м, расстояние 2.3 м — прицел проходит на 31° ВЫШЕ обоих, поэтому игрок, идущий и смотрящий вперёд, не получает даже подсказки; их бот за 90 секунд ни разу не навёл ни один из двух по той же причине. Дверь на 15.6° вниз ловилась всегда — отсюда «дверь работает, остальные два нет», два разных отказа в одной фразе пользователя. Высота — часть расстановки, и 0.5 м были ниже игры.
- 13:08:2026 - 18:30:23: Факел в стартовый инвентарь — ПОМЕЧЕННЫЙ КОСТЫЛЬ СТЕНДА. sim замерила, что вся цепочка факела работает от начала до конца, а в мире ровно ОДИН факел — подбираемый в двух метрах от спавна, то есть примерно в 600 м от устья тоннеля, при пустом стартовом инвентаре. Пользователь пошёл в гору с пустыми руками, и другого исхода у него не было. В настоящей игре «найди чем светить» — это содержание и место ему на подходе к подземелью; здесь это разница между местом, в которое можно играть, и местом, в которое нельзя.
- 13:08:2026 - 18:59:13: Состояние на момент, когда все восемь зон были остановлены случайным прерыванием. Дерево СОБИРАЕТСЯ; красными остаются пять тестов, каждый назван в сообщении коммита. Сохранено, чтобы работа зон не потерялась, а не потому, что она закончена.
- 13:08:2026 - 19:13:36: Пол яркости доходит до кадра ПРИ СТАРТЕ, а не только при закрытии страницы калибровки — мой пропуск, найденный зоной ui приёмочным прогоном: настройка сохранялась, перечитывалась, писалась обратно и НИКОГДА НЕ РИСОВАЛАСЬ. Замерено на шести кадрах: день и тоннель сдвинулись на 0.0002 и 0.025 шага между полом 0 и полом в полтора шага, то есть на собственный шум прогона, а контроль против контроля давал шум в 6–20 раз больше обеих рук. Плюс живой предпросмотр на самой странице: без него она показывает квадраты, занижённые ровно на отсутствующий подъём, то есть врёт тем сильнее, чем выше повёрнута ручка.
- 13:08:2026 - 19:14:43: DFN_MENU_PAGE принимает calibrate. Единственный экран, ради которого заведена вся ручка яркости, не снимался ни разу и снят быть не мог. И ui нашла, почему в settings.cfg оказался min_brightness=0: ручка всегда открывалась на нуле, потому что меню не засевалось сохранённым значением, а любой выход со страницы сохранял то, что на ней стояло.
- 13:08:2026 - 20:41:07: Экран настроек и прицел (четвёртый кусок зоны ui, применён здесь). set_settings() при старте: страница открывается на том, с чем игра ЗАПУЩЕНА, и эта вторая копия — то, против чего отвечает needs_restart(). SettingsDone применяет живьём ТОЛЬКО живьём применимое: покачивание — множитель, который шаговый контекст читает каждый кадр, а разрешение, сглаживание и палитра проглатываются рендером ПРИ ИНИЦИАЛИЗАЦИИ, поэтому пишутся в файл и вступают со следующим запуском. LEFT/RIGHT зовут menu_.adjust() без проверки страницы — на страницах без строк-значений adjust() пуст по построению. DFN_MENU_PAGE принимает settings, довод тот же, что у calibrate. И ПРИЦЕЛ: подсказка взаимодействия рисуется по центру экрана, у которого центр ничем не отмечен; дверь дозы DFN_CROSSHAIR живёт внутри функции, поэтому обе руки приёмки выходят из ОДНОГО бинарника.
- 13:08:2026 - 21:05:12: Прицел спрашивает у приложения ФАКТЫ, а решает сам (HudFacts, зона ui). `any = true` убрано намеренно: слой, числящийся видимым, будучи пустым, делает лживым любой позднейший вопрос «есть ли что-нибудь на экране» — а он у нас задаётся приборами. Правило («метка называет, куда смотрит ЛУЧ КАМЕРЫ»: в третьем лице луч не выходит из глаза, которым целятся; карта — плита, у которой центр уже занят) живёт в draw_crosshair, а не здесь. Приложение сообщает, что ЗНАЕТ, и не решает, что из этого следует.
- 13:08:2026 - 21:48:30: У НЕБА БЫЛО ДВОЕ ЧАСОВ, и починены были только одни. Солнце и луна идут от номера кадра (game_seconds_ += SIM_DT выше — ровно про это), а дрейф облачного поля и огибающая ветра читали СТЕННЫЕ часы каждый кадр, поэтому DFN_RESTORE восстанавливал небо, но не погоду в нём. Найдено зоной ui приёмкой прицела: два прогона ОДНОГО рецепта разошлись на 1.79 % пикселей, все — небо и верхушки деревьев (строки 0–186, ниже 190-й пусто), при том что мерить надо было 72 пикселя метки. Локализовано зоной render: с приколотыми часами та же пара выходит ПОБИТОВО равной. И причина, почему вылезло сегодня, — не поломка, а то, что небо стало содержательнее: кучевые выросли с 2.8 % кадра до 26.8 %, и тот же дрейф двигает на порядок больше пикселей. Теперь часы одни, из тех же секунд, из которых выведено всё остальное здесь.
- 13:08:2026 - 22:14:05: Лента-компас и три полосы (зона ui, применено здесь) — состав выбран пользователем лично. Лента берёт yaw из позы КАМЕРЫ на том же alpha, что и картинка: лента, идущая от позы тела, разошлась бы с тем, что нарисовано, и врала бы тем сильнее, чем быстрее поворот. Полосы стоят полными и убыль НЕ изображают — тратить их пока нечем, а полоса, ползущая для вида, учит читать пустое число. И новая дверь DFN_CAPTURE_AFTER_FRAMES=<N> рядом с секундной: прогон, снимающий по стенной секунде, на загруженной машине успевает другое число кадров, поэтому две руки одного рецепта НЕЛЬЗЯ сравнить побитово — а на этом стоит приёмка всех зон. Запрошена зоной ui после того, как она померила остаток: 4125 расходящихся пикселей упали до 412, когда приколотили часы неба, и вот этим 412 и были. Кривое значение отвергается ВСЛУХ.
*/

#include "engine/app/sources/App.h"

#include "engine/app/sources/HudScreen.h"
#include "engine/app/sources/Localization.h"
// Generated at BUILD time by tools/stamp_build_commit.cmake; carries
// DFN_BUILD_COMMIT into every state capture. See that script for why the
// configure-time version was a defect rather than a simplification.
#include "BuildInfo.h"

#include "engine/core/components/sources/Components.h"
#include "engine/world/sources/CoarseTerrain.h"
#include "engine/world/sources/WorldgenForest.h"
#include "engine/world/sources/LayoutLoad.h"
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
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace dfn::app {

namespace {

// Per-chunk physics state owned by the ferry: TerrainDesc does not promise the
// backend copies the height data, so the float conversion buffer stays alive
// for the lifetime of the body.
struct ChunkPhysics {
    std::vector<float> heights;
    platform::PhysicsBodyHandle body{};
};

uint64_t pack_coord(glm::ivec2 c) {
    return (static_cast<uint64_t>(static_cast<uint32_t>(c.x)) << 32)
         | static_cast<uint64_t>(static_cast<uint32_t>(c.y));
}

} // namespace

// Ferry state lives here rather than in the header to keep App.h light.
static std::unordered_map<uint64_t, ChunkPhysics> g_chunk_physics;

namespace {

constexpr const char* SETTINGS_PATH = "settings.cfg";

// IS THIS RUN UNATTENDED? One definition, two consumers -- the menu skip and the
// cursor grab (Rule 35: a value two places must agree on stops belonging to
// either of them).
//
// The second consumer is why this function exists. The user, working at his
// machine while agents shot frames, reported: "когда запускаются визуальные
// тесты у меня управление компом перехватывается, меня в игру перекидывает,
// мышью управлять не могу". Every automated door except the body probe grabbed
// the desktop pointer, because the exemption had been written for ONE door
// instead of for the PROPERTY the doors share. An unattended run has nobody to
// aim, so it has no business owning the mouse.
[[nodiscard]] bool unattended_run() {
    return std::getenv("DFN_TOUR") != nullptr || std::getenv("DFN_PLAYTEST") != nullptr
           || std::getenv("DFN_PLAYTEST_ROUTE") != nullptr
           || std::getenv("DFN_CAPTURE_AFTER") != nullptr
           || std::getenv("DFN_CAPTURE_AFTER_FRAMES") != nullptr
           || std::getenv("DFN_BODY_PROBE") != nullptr
           || std::getenv("DFN_MENU_SHOT") != nullptr
           || std::getenv("DFN_HUD_PROBE") != nullptr
           || std::getenv("DFN_RESTORE") != nullptr;
}

// Reads key=value graphics settings; writes a commented default file on first
// run so the user always has something to edit (sync #3 decision: resolution
// and palette are user settings, not constants).
// The settings file is written from TWO places -- first run, and the moment
// the player leaves the calibration page. A setting that cannot be saved is
// not a setting, and two copies of this text would be two files that drift.
void write_settings(const AppConfig& cfg) {
    std::ofstream out(SETTINGS_PATH);
        out << "# Daggerfall N graphics settings (auto-generated; edit freely)\n"
            << "# internal_resolution: rendering pixel grid, integer-upscaled to the\n"
            << "#   window. Presets: 640x360 (fine retro), 320x180 (chunky Daggerfall).\n"
            << "internal_resolution=" << cfg.internal_width << 'x' << cfg.internal_height
            << "\n"
            << "# msaa: coverage samples on the internal grid (0 = off, 2, 4, 8).\n"
            << "#   This is what stopped the treeline shimmering when you run\n"
            << "#   (0.094% -> 0.004% of the screen flipping per frame); lowering\n"
            << "#   it brings that back. It does NOT change the pixel grid.\n"
            << "msaa=" << cfg.msaa_samples << "\n"
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
}

void load_or_create_settings(AppConfig& cfg) {
    std::ifstream in(SETTINGS_PATH);
    if (!in.is_open()) {
        write_settings(cfg);
        return;
    }
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
            unsigned w = 0, h = 0;
            if (std::sscanf(value.c_str(), "%ux%u", &w, &h) == 2 && w > 0 && h > 0) {
                cfg.internal_width = w;
                cfg.internal_height = h;
            }
        } else if (key == "msaa") {
            const unsigned v = static_cast<unsigned>(std::strtoul(value.c_str(), nullptr, 10));
            if (v == 0 || v == 1 || v == 2 || v == 4 || v == 8) {
                cfg.msaa_samples = v;
            } else {
                // Loud, never a nearest match: a silently-clamped sample count
                // would look like a working setting and draw a different world.
                std::fprintf(stderr,
                             "[settings] msaa=%s REJECTED (want 0, 2, 4 or 8); "
                             "keeping %u\n", value.c_str(), cfg.msaa_samples);
            }
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

} // namespace

AppConfig AppConfig::from_env() {
    AppConfig cfg;
    cfg.internal_width = static_cast<uint32_t>(config::INTERNAL_RES_W);
    cfg.internal_height = static_cast<uint32_t>(config::INTERNAL_RES_H);
    load_or_create_settings(cfg); // file first; env below overrides (tooling)
    if (const char* res = std::getenv("DFN_INTERNAL_RES")) {
        unsigned w = 0, h = 0;
        if (std::sscanf(res, "%ux%u", &w, &h) == 2 && w > 0 && h > 0) {
            cfg.internal_width = w;
            cfg.internal_height = h;
        }
    }
    if (const char* nr = std::getenv("DFN_NULL_RENDER"); nr && nr[0] == '1') {
        cfg.use_null_renderer = true;
    }
    if (const char* mn = std::getenv("DFN_MENU")) {
        cfg.show_menu = (mn[0] == '1');
    }
    // DFN_STAND, not DFN_MAP: DFN_MAP was already render's MAP-SCREEN probe,
    // and Tour::stand_steps treats any probe variable as "this run is a single
    // evidence frame" -- so selecting the stand with it silently collapsed the
    // stand's own tour to one testbed frame. A name collision, found by the
    // frame it produced rather than by reading either file.
    if (const char* mp = std::getenv("DFN_STAND")) {
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
    if (const char* na = std::getenv("DFN_NULL_AUDIO"); na && na[0] == '1') {
        cfg.use_null_audio = true;
    }
    if (const char* np = std::getenv("DFN_NULL_PHYSICS"); np && np[0] == '1') {
        cfg.use_null_physics = true;
    }
    // The harness needs its own door: settings.cfg is shared by every zone and
    // a run must not edit it to take a frame.
    if (const char* bf = std::getenv("DFN_BLACK_FLOOR"); bf != nullptr && *bf != '\0') {
        float v = 0.0f;
        if (std::sscanf(bf, "%f", &v) == 1 && v >= 0.0f && v <= 0.25f) {
            cfg.black_floor = v;
        } else {
            std::fprintf(stderr, "[config] DFN_BLACK_FLOOR=\"%s\" REJECTED (want 0..0.25)\n", bf);
        }
    }
    if (const char* pal = std::getenv("DFN_PALETTE"); pal && pal[0] == '1') {
        cfg.palette_post = true;
    }
    // Same tooling pattern as DFN_PALETTE: the settings row is the user's, the
    // env var is the harness's. head_bob 0 is the ready-made MOTION control
    // (Rule 30) -- bob/dip/settle stop, events and sound keep firing -- so a
    // judder can be attributed to camera motion or exonerated of it in one run.
    if (const char* hb = std::getenv("DFN_HEAD_BOB"); hb != nullptr && *hb != '\0') {
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

App::App() : timestep_(config::SIM_DT, static_cast<uint32_t>(config::SIM_MAX_CATCHUP_STEPS)) {}

App::~App() = default;

bool App::init(const AppConfig& config) {
    config_ = config;

    window_ = platform::create_glfw_window();
    platform::WindowInitParams wp;
    wp.width = config.window_width;
    wp.height = config.window_height;
    wp.title = "Daggerfall N"; // bootstrap exception: replaced by loc lookup (sync #2 note)
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
    if (!renderer_ || !renderer_->init(rp)) {
        return false;
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
    sfx_bus_ = audio_->create_bus({});
    sound_bank_ = gameplay::load_step_sound_bank(
        *audio_, "games/daggerfall_n/assets/audio", sfx_bus_);
    gameplay::wire_step_audio(bus_, *audio_, sound_bank_);
    wind_loop_ = gameplay::start_wind_loop(*audio_, sound_bank_);

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
    MenuSettings ms;
    ms.internal_w = config.internal_width;
    ms.internal_h = config.internal_height;
    ms.msaa = config.msaa_samples;
    ms.palette = config.palette_post;
    ms.head_bob = config.head_bob;
    menu_.set_settings(ms);

    // Rule 5: every user-facing string comes from here and nowhere else.
    // A missing file is loud and the game still runs, with every string drawn
    // as a visible placeholder rather than as nothing.
    (void)load_localization("games/daggerfall_n/assets/localization/ru.txt");

    // The demo-map table. Adding a stand is one row here plus two localization
    // lines -- the menu itself never changes, which is the point of a table.
    // Stand ids belong to core's WorldGenParams; 0 is today's valley.
    menu_.set_maps({{static_cast<uint32_t>(world::StandId::Testbed),
                     "map.valley.name", "map.valley.blurb"},
                    {static_cast<uint32_t>(world::StandId::Forest),
                     "map.forest.name", "map.forest.blurb"}});
    // DFN_MENU_PAGE=root|maps|pause|calibrate -- which page an unattended run
    // opens on.
    // Without it only the root page is photographable, because the map picker
    // and the pause page can be reached ONLY by a hand on the keyboard, so two
    // of the three screens the player actually sees have never been evidence.
    // Refused out loud on an unknown value, like every other tooling door here:
    // falling back to root would archive a root frame under a pause filename.
    if (const char* mp = std::getenv("DFN_MENU_PAGE"); mp != nullptr && *mp != '\0') {
        const std::string page(mp);
        if (page == "root") {
            menu_.open(MenuPage::Root);
        } else if (page == "maps") {
            menu_.open(MenuPage::Maps);
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
        } else {
            std::fprintf(stderr,
                         "[menu] DFN_MENU_PAGE=\"%s\" is not root|maps|pause|calibrate|settings -- "
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
    if (const char* dbg = std::getenv("DFN_DEBUG_OVERLAY");
        dbg != nullptr && *dbg == '1') {
        debug_overlay_ = true;
    }

    // STATE CAPTURE destination and STATE RESTORE source.
    capture_dir_ = [] {
        const char* d = std::getenv("DFN_CAPTURE_DIR");
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
    if (const char* ca = std::getenv("DFN_CAPTURE_AFTER"); ca != nullptr) {
        capture_after_s_ = std::strtod(ca, nullptr);
    }
    // The same door counted in FRAMES. A run that fires on a wall-clock second
    // reaches a different frame number on a loaded machine than on an idle one,
    // so two arms of one recipe cannot be compared bit for bit -- which is the
    // whole method every zone's acceptance rests on. Requested by ui after it
    // measured the residue: 412 pixels still differed between identical runs
    // once the sky's own clocks were pinned, and this was all of it.
    if (const char* cf = std::getenv("DFN_CAPTURE_AFTER_FRAMES"); cf != nullptr) {
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

    // THE FRAME LOG (DFN_FRAME_LOG=<path>). See App.h for why this exists and
    // is not another screenshot door. It opens LOUDLY: a run that logged
    // nothing must not be mistakable for a run that logged zeros -- that exact
    // confusion already cost three probe runs on the line above.
    if (const char* fl = std::getenv("DFN_FRAME_LOG"); fl != nullptr && *fl != '\0') {
        frame_log_ = std::fopen(fl, "wb");
        if (frame_log_ == nullptr) {
            std::fprintf(stderr, "[frame_log] cannot open \"%s\" for writing\n", fl);
        } else {
            std::fprintf(frame_log_,
                         "# Daggerfall N per-frame log -- one line per PRESENTED frame.\n"
                         "# No readback, no settle, no cooldown: this instrument cannot\n"
                         "# quiet the thing it is pointed at. Between-frames motion is\n"
                         "# arithmetic on adjacent lines.\n"
                         "# frame dt_ms game_s speed fov_y eye_x eye_y eye_z yaw pitch\n");
        }
    }

    // DFN_RESTORE names a sidecar written by F2. Read BEFORE the world is
    // built, because the capture says WHICH stand to build -- restoring a pose
    // into the default map and then noticing the mismatch would be a worse
    // version of the same feature.
    if (const char* rp = std::getenv("DFN_RESTORE"); rp != nullptr && *rp != '\0') {
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

    // The world itself is NOT built here. Menu-first launch means the player
    // picks a demo map before any terrain exists, so world construction lives
    // in enter_world() and init() only raises the engine.
    if (config_.show_menu) {
        mode_ = AppMode::Menu;
        input_->set_cursor_captured(false);
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
bool App::enter_world(uint32_t stand) {
    active_stand_ = stand;
    // Chunk streaming: stage 2 serves the in-memory generated world (core's
    // open_generated path; .dfw file IO lands in stage 3). Testbed extent 4x4
    // chunks (Q45), fixed seed for reproducible screenshots (Rule 13.1).
    world::ChunkStreamingParams sp;
    sp.load_radius = static_cast<uint32_t>(config::CHUNK_LOAD_RADIUS);
    sp.unload_radius = static_cast<uint32_t>(config::CHUNK_UNLOAD_RADIUS);
    world::WorldGenParams gp;
    // THE MAP IS CONTENT AND IT IS LOADED, NOT COMPILED IN (Rule 5). Core moved
    // 441 lines of ONE GAME'S survey -- Vaelmere, Ravenscar, Harrowward -- out
    // of `engine/world` and proved the asset reproduces the compiled defaults
    // exactly; this is the call that retires them. It was the largest Rule 5
    // violation in the repo and the single edit that turns "architecturally
    // reusable" into reusable, because until now the reusable engine knew the
    // name of this game's mountain.
    //
    // A FAILURE IS FATAL, DELIBERATELY. Falling back to the compiled defaults
    // would mean a missing or malformed asset produces a world that looks
    // almost right -- the fourth silent-zero of the day, and the most expensive
    // kind, because nobody would be looking for it. The file carries survey
    // coordinates and FRACTIONS only; the registry anchors and the scaling
    // transforms stay in the engine, so that moving a constant still moves the
    // world it is supposed to move (Rule 37).
    {
        const auto lr = world::load_layout_file(
            "games/daggerfall_n/assets/world/testbed_layout.json", gp.layout);
        if (!lr.ok) {
            std::fprintf(stderr, "[app] FATAL: layout asset: %s\n", lr.error.c_str());
            return false;
        }
    }
    gp.seed = 1u;
    gp.min_chunk = {0, 0};
    // 2x2 km (WORLD_EXTENT_CHUNKS 8 x CHUNK_SIZE 256), the user's direct and
    // twice-repeated request. Was a bare {3,3} here, which made the size of the
    // world unchangeable without editing source. The far-detail ladder is
    // already sized for the 10x10 km target and node ids sit on a fixed world
    // grid, so growing the world renumbers nothing already cached.
    gp.max_chunk = {static_cast<int>(config::WORLD_EXTENT_CHUNKS) - 1,
                    static_cast<int>(config::WORLD_EXTENT_CHUNKS) - 1};
    // The chosen demo map. Stand ids are core's; the app only selects.
    if (stand == static_cast<uint32_t>(world::StandId::Forest)) {
        gp.layout = world::forest_stand_layout();
    }
    chunks_.open_generated(gp, sp);

    // World edge (sim's finding): past the generated extent there is no terrain
    // and the player simply falls out of the world. At walking pace that took
    // minutes of deliberate effort; at sprint speed it is 20 seconds and looks
    // like a crash. Four static walls close the box until the world is bigger.
    {
        const float span = static_cast<float>(config::CHUNK_SIZE)
                         * static_cast<float>(gp.max_chunk.x - gp.min_chunk.x + 1);
        const float mid = span * 0.5f;
        const float h = 200.0f;   // tall enough that no terrain reaches over it
        const float t = 2.0f;     // wall thickness
        const glm::vec3 sides[4] = {{-t, 0.0f, mid}, {span + t, 0.0f, mid},
                                    {mid, 0.0f, -t}, {mid, 0.0f, span + t}};
        const glm::vec3 halves[4] = {{t, h, mid + t}, {t, h, mid + t},
                                     {mid + t, h, t}, {mid + t, h, t}};
        for (int i = 0; i < 4; ++i) {
            platform::StaticBoxDesc wall;
            wall.center = {sides[i].x, h * 0.5f, sides[i].z};
            wall.half_extents = halves[i];
            wall.layer = physics::LAYER_STATIC;
            world_edge_[static_cast<size_t>(i)] = physics_->create_static_box(wall);
        }
    }

    const auto wb = chunks_.water_bodies();
    render_system_.set_water_bodies(*renderer_, wb.lakes, wb.river_stations,
                                    wb.river_segment_offsets);

    // Path surfaces: whole-world, built at open, valid until re-open -- the
    // same lifetime as the water bodies above, so the same one-shot call site
    // is the right one. Empty on a stand with no paths, which is a valid
    // answer and needs no stand check.
    const auto ps = chunks_.path_surface();
    render_system_.set_path_surface(*renderer_, ps.stations, ps.route_offsets);

    // Subscribe the ferry BEFORE the first update so initial loads are seen.
    bus_.subscribe<world::ChunkLoaded>([this](const world::ChunkLoaded& e) {
        world_changed_this_frame_ = true; // streaming quiescence, see run()
        auto view = chunks_.heightfield(e.coord);
        if (!view) {
            return;
        }
        // Terrain is DRAWN from the voxel mesh, not the heightfield. A
        // heightfield stores one height per column, so it is mathematically
        // incapable of a ceiling: inside a carve there was nothing to submit
        // at all, and a live player who walked into the barrow saw the world
        // from the inside. The heightfield upload remains as the fallback for
        // chunks that have no voxel mesh.
        // The heightfield still travels with the voxel upload, for the MAP and
        // only for the map. `note_chunk` used to hang off the heightfield path,
        // so the day terrain moved to the voxel mesh the map silently stopped
        // recording anything that HAD voxel geometry -- i.e. nearly everything.
        // An unexplored map is pixel-identical to a broken one, which is why it
        // went unnoticed for hours. Render will not re-derive one height per
        // column from a surface mesh; the app already holds the field, so the
        // app passes it.
        const auto voxel = chunks_.voxel_mesh(e.coord);
        auto sf = chunks_.surfacefield(e.coord);
        if (voxel) {
            render_system_.upload_terrain_voxel(*renderer_, *voxel, &*view,
                                                sf ? &*sf : nullptr);
        } else {
            render_system_.upload_terrain(*renderer_, *view, sf ? &*sf : nullptr);
        }
        render_system_.upload_scatter(*renderer_, {e.coord.x, e.coord.z},
                                      chunks_.scatter(e.coord));

        // Terrain collision comes from the VOXEL surface, not the heightfield:
        // a heightfield body cannot represent the crag tunnel's ceiling, so the
        // player would walk over the mountain instead of through it. An invalid
        // handle means "empty chunk, no body needed" and is not an error.
        // sim's helper sets LAYER_STATIC (hand-rolling that once left `layer`
        // at 0 — a body colliding with nothing, and the player fell through).
        if (voxel) {
            ChunkPhysics cp;
            cp.body = physics::create_terrain_mesh_body(*physics_, *voxel, 0);
            if (cp.body.valid()) {
                g_chunk_physics[pack_coord({e.coord.x, e.coord.z})] = std::move(cp);
            }
        }
    });
    bus_.subscribe<world::ChunkUnloaded>([this](const world::ChunkUnloaded& e) {
        world_changed_this_frame_ = true; // streaming quiescence, see run()
        render_system_.drop_terrain(*renderer_, {e.coord.x, e.coord.z});
        render_system_.drop_scatter(*renderer_, {e.coord.x, e.coord.z});
        auto it = g_chunk_physics.find(pack_coord({e.coord.x, e.coord.z}));
        if (it != g_chunk_physics.end()) {
            physics_->destroy_body(it->second.body);
            g_chunk_physics.erase(it);
        }
    });

    // Spawn at the center of chunk (0,0), on the ground. The chunk events are
    // QUEUED (post/pump), so the pump here is load-bearing: without it the
    // terrain collision bodies would not exist yet and the player would spawn
    // into empty space and fall through the world.
    const float mid = static_cast<float>(config::CHUNK_SIZE) * 0.5f;
    chunks_.update({mid, 0.0f, mid}, world_, bus_);
    bus_.pump();
    const float ground = chunks_.height_at({mid, mid}).value_or(0.0f);
    const glm::vec3 spawn{mid, ground + 0.2f, mid};

    player_ = gameplay::spawn_player(world_, *physics_, spawn);
    if (!world_.alive(player_)) {
        return false;
    }

    // TESTBED CONTENT (Rule 5 exception, same standing as the fixed seed and
    // the extent walls above): items and placements are data and move to the
    // content loader the day core's JSON reader lands. Ids follow story's
    // convention and are hashed, never spelled in C++ logic.
    //
    // This block exists because the interaction, inventory and held-item
    // systems were written, tested and NEVER CALLED by the running game --
    // which is why "рук нет и трогать нечего" was a bug report rather than a
    // feature request. Same class as the terrain ferry and the unpumped chunk
    // events: the subsystem was correct and the composition root ignored it.
    {
        gameplay::ItemDatabase items;
        gameplay::ItemDef torch;
        torch.id = {serialization::fnv1a64("item.tool.torch")};
        torch.display_name_key = "item.tool.torch.name";
        torch.light_source = true;
        torch.mesh_id = 33; // render's registry: 32 hand, 33 torch
        items.add(torch);
        world_.add_resource(std::move(items));

        world_.add(player_, gameplay::Inventory{});
        world_.add(player_, gameplay::HeldItem{});
        // A TORCH IN HAND AT SPAWN -- and this is a TESTBED CROSSBAR, marked as
        // one so nobody mistakes it for design.
        //
        // The user walked into the mountain tunnel and reported "абсолютная
        // тьма, ничего совершенно не видно". sim measured that the whole torch
        // chain works end to end -- pick up, hold, light, carried light, point
        // light in the frame -- and that the world contains exactly ONE torch,
        // a pickup two metres from the spawn, roughly 600 m from the tunnel
        // mouth, with the inventory starting empty. He went in empty-handed
        // because there was no other outcome available to him.
        //
        // In a shipped game "find something to burn" is CONTENT and belongs in
        // the dungeon's approach. Here it is the difference between a place
        // that can be played and one that cannot, so the stand hands him one.
        if (auto* inv = world_.get<gameplay::Inventory>(player_)) {
            const auto& db = world_.resource<gameplay::ItemDatabase>();
            (void)gameplay::add_item(*inv, db, torch.id, 1);
        }
        // THE VIEW MODEL'S HAND IS DECLARED ABSENT, ON PURPOSE, and this line
        // is a fix rather than a disabling.
        //
        // It used to say 32 (VIEWMODEL_MESH_ID_HAND). Nothing has ever built a
        // mesh for that id: render reserves 32..33 for a view-model mechanism
        // that does not exist yet, and `register_mesh` REFUSES the id for that
        // very reason -- so the app named an asset nobody supplies, and the
        // first-person hand has drawn as NOTHING since the day it was wired.
        // Render's loud unregistered-asset report is what surfaced it; it was
        // firing every launch, next to the `mesh_asset = 0` sentinel warning
        // that made it easy to dismiss as noise.
        //
        // 0 is the documented "none", so this states the absence instead of
        // producing it by accident, and the warning goes quiet because there is
        // nothing missing to warn about.
        //
        // WHY NOT POINT IT AT THE RIG'S HAND INSTEAD: the first-person BODY
        // already draws a real HandR, placed by the rig. A second hand placed
        // by the view model's own sway would be the same hand in two places.
        // Whether a view-model hand should exist at all now that a full body
        // does is a design question, not a wiring one -- raised, not decided
        // here. The item slot is untouched and a held torch still draws.
        world_.add_resource(gameplay::ViewModelAssets{.hand_mesh = 0});
        gameplay::spawn_view_model(world_, player_);

        // Three props, not one: take, open and use are three different verb
        // paths, and a lone pickup would leave two of them as untested in the
        // real game as they were before this block existed.
        // THE PROPS' PLACEHOLDER MESHES (sim's geometry, app ferry -- the same
        // shape as the body-segment ferry below). Without this the three demo
        // props spawn with a RenderMesh id nothing has uploaded and draw as
        // NOTHING, which is how a 1.8 x 2.0 m door stood invisible 2.5 m in
        // front of the spawn with the whole hover chain working correctly
        // around it: the ray hit its physics box, HoverTarget filled honestly,
        // and "Открыть" was drawn over empty grass.
        for (const gameplay::InteractableMesh& m : gameplay::interactable_meshes()) {
            if (!render_system_.register_mesh(*renderer_, m.mesh_asset, m.vertices,
                                              m.indices)) {
                std::fprintf(stderr,
                             "[app] interactable mesh %u refused by the registry -- "
                             "that prop will be INVISIBLE\n", m.mesh_asset);
            }
        }

        gameplay::InteractableDesc take;
        take.kind = gameplay::InteractableKind::Pickup;
        // HEIGHT IS PART OF PLACEMENT, and 0.5 m was below the game. sim
        // measured it: eye at 1.7 m, prop at 0.5 m, 2.3 m away -- the crosshair
        // sits 31 degrees ABOVE both, so a player walking and looking ahead
        // never even gets the prompt. Its bot never once hovered them in 90
        // seconds for the same reason. The door, at 15.6 degrees down, was
        // always caught, which is why the complaint read as "the door works,
        // the other two do nothing" -- two different failures wearing one
        // sentence. 1.3 m is where a wall sconce and a wall lever live anyway.
        take.position = spawn + glm::vec3{2.0f, 1.45f, 0.0f};
        take.prompt_key = "prompt.take";
        take.item = torch.id;
        (void)gameplay::spawn_interactable(world_, *physics_, take);

        gameplay::InteractableDesc lever;
        lever.kind = gameplay::InteractableKind::Usable;
        lever.position = spawn + glm::vec3{-2.0f, 1.45f, 0.0f};
        lever.prompt_key = "prompt.use";
        lever.action = serialization::fnv1a64("use.testbed.lever");
        (void)gameplay::spawn_interactable(world_, *physics_, lever);

        gameplay::InteractableDesc door;
        door.kind = gameplay::InteractableKind::Openable;
        door.position = spawn + glm::vec3{0.0f, 1.0f, -2.5f};
        door.half_extents = {0.9f, 1.0f, 0.1f};
        door.prompt_key = "prompt.open";
        (void)gameplay::spawn_interactable(world_, *physics_, door);
    }

    // FIRST-PERSON BODY (character's zone, wired here). Rigid segments through
    // the ordinary render path; the head MESH is hidden because the camera
    // sits inside the skull.
    body_rig_ = anim::Rig::build(anim::RigProportions::from_config());
    for (uint32_t b = 0; b < anim::BONE_COUNT; ++b) {
        const auto bone = static_cast<anim::Bone>(b);
        const auto seg = anim::build_body_segment_mesh(bone, body_rig_.proportions);
        if (!render_system_.register_mesh(*renderer_, anim::body_segment_mesh_id(bone),
                                          seg.vertices, seg.indices)) {
            std::fprintf(stderr, "[app] body segment mesh %u refused by the registry\n",
                         anim::body_segment_mesh_id(bone));
        }
    }
    anim::spawn_body(world_, player_, body_rig_, /*hide_head=*/true);

    // Landing dip rides sim's measured impact, not a guess (their event).
    bus_.subscribe<gameplay::Landed>([this](const gameplay::Landed& e) {
        anim::note_landed(world_, e.walker, e.impact_speed);
    });

    // MIRROR PUPPET (grill v11). DFN_MIRROR=1: the double stands 3 m ahead and
    // mirrors you. DFN_SHOWCASE=1: it floats and cycles the clip reel instead.
    // Placement literals live here under the testbed block's Rule 5 exception.
    {
        const char* mirror_env = std::getenv("DFN_MIRROR");
        const char* showcase_env = std::getenv("DFN_SHOWCASE");
        // THE PROBE COUNTS AS WANTING A DOUBLE, and leaving it out cost
        // character a whole shoot. `DFN_BODY_PROBE=mirror|showcase|profile|
        // plant|gait` selects the camera BEHAVIOUR and every one of those modes
        // aims at `mirror_puppet_` -- so without the puppet the probe framed an
        // empty clearing, and the resulting frame reads as "the body is not
        // drawing" rather than as "the subject was never spawned".
        //
        // THIRD SILENT ZERO OF THE DAY AND THE SAME BUG ALL THREE TIMES: a
        // PRECONDITION written as a list of the callers who happened to need it
        // when it was written. The menu skip named two env vars and four more
        // arrived; the unregistered-mesh warning named ids and the sentinel
        // arrived; this names two and the probe arrived. A list of names cannot
        // notice that a new caller has the same requirement -- only the
        // requirement can, and the requirement here is "this run aims a camera
        // at the double".
        const char* probe_env = std::getenv("DFN_BODY_PROBE");
        const bool want_mirror = (mirror_env && *mirror_env == '1')
                              || (showcase_env && *showcase_env == '1')
                              || (probe_env != nullptr && *probe_env != '\0');
        if (want_mirror) {
            const glm::vec3 mirror_pt = spawn + glm::vec3{0.0f, 0.0f, -3.0f};
            const auto puppet = anim::spawn_mirror_puppet(world_, body_rig_, player_,
                                                          mirror_pt, {0.0f, 1.0f});
            if (showcase_env && *showcase_env == '1') {
                if (auto* mp = world_.get<anim::MirrorPuppet>(puppet)) {
                    mp->showcase = true;
                    mp->hover_height_m = 1.2f;
                    mp->clip_seconds = 4.0f;
                }
            }
            mirror_puppet_ = puppet;
        }
    }

    // BODY PROBE (Rule 27 evidence; see App.h). The Tour freezes the tick, so
    // an animated subject cannot be photographed by it at all. Here the world
    // RUNS and the shot is triggered off simulation state.
    if (const char* bp = std::getenv("DFN_BODY_PROBE"); bp != nullptr && *bp != '\0') {
        BodyProbe probe;
        probe.mode = bp;
        const char* d = std::getenv("DFN_BODY_PROBE_DIR");
        probe.dir = d ? d : ("screenshots/body_" + probe.mode);
        std::filesystem::create_directories(probe.dir);
        probe.warmup_s = 4.0f;
        if (probe.mode == "stride" || probe.mode == "gait") {
            // The four quarters of ONE stride, in crossing order. This is the
            // Rule 27 range clause: FOOTFALL_PHASE_LEFT/RIGHT are where a foot
            // MUST be planted, and 0.0/0.5 are where one MUST be in the air —
            // a set that can only pass if the plant timing is actually right.
            probe.targets = {static_cast<float>(config::FOOTFALL_PHASE_LEFT), 0.5f,
                             static_cast<float>(config::FOOTFALL_PHASE_RIGHT), 0.0f};
            // "stride" looks down at its own feet; "gait" watches the MIRROR
            // double instead, because a walker cannot photograph its own legs
            // from inside its own skull. Same trigger, outside vantage.
            probe.pitch = probe.mode == "gait" ? -0.10f : -1.15f;
            if (const char* p = std::getenv("DFN_BODY_PITCH")) {
                probe.pitch = std::strtof(p, nullptr);
            }
        } else if (probe.mode == "showcase") {
            // Mid-clip of each of the six reel entries (4 s per clip); the run
            // starts one whole clip in so the warm-up cannot eat a shot.
            probe.targets = {6.0f, 10.0f, 14.0f, 18.0f, 22.0f, 26.0f};
            probe.pitch = 0.15f; // the double floats at 1.2 m
        } else if (probe.mode == "profile") {
            // THE SIDE BEARING, and it exists because the front one cannot
            // fail: a fore-aft leg scissor projects to nothing when the subject
            // faces the lens, so a frontal walk frame looks identical whether
            // the legs swing or not. The mirror double can never supply this —
            // it reflects the camera's OWN facing, so it turns to face you
            // whichever way you turn. Only the showcase double, whose facing is
            // independent, can be walked around and seen in profile.
            probe.targets = {6.0f, 10.0f, 26.0f}; // walk, run, idle
            probe.warmup_s = 5.0f;
            probe.pitch = 0.10f;
        } else if (probe.mode == "plant") {
            // THE FOOTFALL FRAME. Same side vantage, but the shots are the four
            // quarters of one walk cycle: the phase rows FOOTFALL_PHASE_LEFT
            // and _RIGHT, where a foot MUST be down, and 0.5 / 1.0, where the
            // legs MUST be passing. The clip's own clock is a pure function of
            // WALK_SPEED and the step-length pair, so the times are derived
            // here from those rows rather than typed in.
            const auto v = static_cast<float>(config::WALK_SPEED);
            const float step = static_cast<float>(config::STEP_LENGTH_BASE)
                             + static_cast<float>(config::STEP_LENGTH_PER_MPS) * v;
            const float period = 2.0f * step / v;   // seconds per full stride
            const float clip = 4.0f;                // the walk clip starts here
            probe.targets = {clip + (2.0f + static_cast<float>(config::FOOTFALL_PHASE_LEFT))
                                        * period,
                             clip + 2.5f * period,
                             clip + (2.0f + static_cast<float>(config::FOOTFALL_PHASE_RIGHT))
                                        * period,
                             clip + 3.0f * period};
            probe.warmup_s = 4.0f;
            probe.pitch = 0.10f;
        } else { // mirror
            // Turn LEFT by these offsets; the double must turn the other way.
            probe.targets = {0.0f, -0.25f, -0.5f};
            probe.direction = -1;
            probe.pitch = 0.0f;
        }
        body_probe_ = std::move(probe);
    }

    // STEP CONTEXT: who publishes, whose ground, the user's bob setting.
    step_ctx_.events = &bus_;
    step_ctx_.surface_class_at = [this](glm::vec2 xz) {
        return chunks_.surface_class_at(xz);
    };
    step_ctx_.bob_scale = config_.head_bob;

    // AUTONOMOUS PLAYTEST (sim's spec, engine/gameplay/docs/PLAYTEST.md).
    // DFN_PLAYTEST=patrol|explore|soak. The bot writes the same input intents
    // human keys write; incidents screenshot and gate the exit code.
    // A ROUTE IMPLIES A PLAYTEST. Naming waypoints and getting a still player is
    // the silent-no-op failure this harness exists to prevent: the dungeon zone
    // lost a 150-second run to exactly that, standing on the spawn while the
    // route sat parsed and unused inside a branch it never entered. There is no
    // second reading of "here is the route to walk", so the value carries the
    // intent and the mode follows it.
    const char* route_env = std::getenv("DFN_PLAYTEST_ROUTE");
    const bool route_given = route_env != nullptr && *route_env != '\0';
    const char* pt_env = std::getenv("DFN_PLAYTEST");
    if (route_given && (pt_env == nullptr || *pt_env == '\0')) {
        std::fprintf(stderr, "[playtest] DFN_PLAYTEST_ROUTE given without "
                             "DFN_PLAYTEST -- running patrol\n");
    }
    if (const char* pt = (pt_env != nullptr && *pt_env != '\0') ? pt_env
                                                                : (route_given ? "patrol" : nullptr);
        pt != nullptr && *pt != '\0') {
        gameplay::PlaytestConfig ptc;
        const std::string mode(pt);
        if (mode == "patrol") {
            ptc.mode = gameplay::BotMode::WaypointPatrol;
            // v1 route: the three testbed props and home.
            ptc.waypoints = {{spawn.x + 2.0f, spawn.z}, {spawn.x - 2.0f, spawn.z},
                             {spawn.x, spawn.z - 2.5f}, {spawn.x, spawn.z}};
            ptc.loop_waypoints = true;
        } else if (mode == "explore") {
            ptc.mode = gameplay::BotMode::RandomExplorer;
        } else {
            ptc.mode = gameplay::BotMode::Soak;
        }
        if (const char* sd = std::getenv("DFN_PLAYTEST_SEED")) {
            ptc.seed = std::strtoull(sd, nullptr, 10);
        }
        if (const char* sec = std::getenv("DFN_PLAYTEST_SECONDS")) {
            ptc.duration_seconds = std::strtof(sec, nullptr);
        }
        // DFN_PLAYTEST_GAIT=walk|jog|run. The bot already carries the gear
        // (PlaytestConfig::gait); without a door to it the harness can only
        // ever measure WALK, and every step-feel quantity is a function of
        // speed -- so the gears that are not the default are exactly the ones
        // no automated run has ever visited.
        if (const char* g = std::getenv("DFN_PLAYTEST_GAIT"); g != nullptr && *g != '\0') {
            //
            // AN UNKNOWN VALUE IS REFUSED OUT LOUD, not folded into walk. A
            // typo ("jgo") falling through to Walk would silently reproduce the
            // exact defect this door was opened to fix: a run that reports
            // itself as a jog measurement while measuring a walk. The default
            // is the dangerous branch here precisely because it is also the
            // correct spelling of a real gear.
            const std::string gait(g);
            if (gait == "run") {
                ptc.gait = gameplay::Gait::Run;
            } else if (gait == "jog") {
                ptc.gait = gameplay::Gait::Jog;
            } else if (gait == "walk") {
                ptc.gait = gameplay::Gait::Walk;
            } else {
                std::fprintf(stderr,
                             "[playtest] DFN_PLAYTEST_GAIT=\"%s\" is not "
                             "walk|jog|run -- REFUSING to run, because a run "
                             "that quietly measured walk would be reported as "
                             "measuring \"%s\"\n",
                             g, g);
                return false;
            }
        }
        // DFN_PLAYTEST_ROUTE="x,z;x,z;..." -- ABSOLUTE world coordinates.
        //
        // Why this door exists: patrol's route was hardwired to four points two
        // metres around the spawn, and PLAYTEST.md names patrol's own purpose as
        // "scripted acceptance walks (the crag tunnel, the castle ford)". The
        // route was always meant to be given; it simply was never exposed, so no
        // automated run has ever been INSIDE anything -- explorer picks random
        // goals, soak circles the spawn, and the dungeons sit 500 m away.
        //
        // REFUSED OUT LOUD on a malformed value, for the same reason as
        // DFN_PLAYTEST_GAIT above and one degree worse: folding a typo back to
        // the spawn ring would produce a run that reports "walked the tunnel"
        // having measured a lawn. A wrong measurement that looks like a right
        // one is the failure mode this whole harness is built against.
        if (const char* rt = std::getenv("DFN_PLAYTEST_ROUTE");
            rt != nullptr && *rt != '\0') {
            std::vector<glm::vec2> route;
            const std::string spec(rt);
            size_t pos = 0;
            bool ok = true;
            while (pos < spec.size() && ok) {
                const size_t end = std::min(spec.find(';', pos), spec.size());
                const std::string pair = spec.substr(pos, end - pos);
                pos = end + 1;
                if (pair.empty()) {
                    continue; // a trailing ';' is not an error
                }
                const size_t comma = pair.find(',');
                if (comma == std::string::npos) {
                    ok = false;
                    break;
                }
                char* xe = nullptr;
                char* ze = nullptr;
                const std::string xs = pair.substr(0, comma);
                const std::string zs = pair.substr(comma + 1);
                const float x = std::strtof(xs.c_str(), &xe);
                const float z = std::strtof(zs.c_str(), &ze);
                // strtof reports "no conversion" by leaving the end pointer at
                // the start -- checking that is what separates "0" from "oops".
                if (xe == xs.c_str() || ze == zs.c_str()) {
                    ok = false;
                    break;
                }
                route.push_back({x, z});
            }
            if (!ok || route.empty()) {
                std::fprintf(stderr,
                             "[playtest] DFN_PLAYTEST_ROUTE=\"%s\" is not "
                             "\"x,z;x,z;...\" -- REFUSING to run, because a run "
                             "that quietly walked the spawn ring would be "
                             "reported as walking that route\n",
                             rt);
                return false;
            }
            ptc.mode = gameplay::BotMode::WaypointPatrol;
            ptc.waypoints = std::move(route);
            ptc.loop_waypoints = true;
            std::fprintf(stderr, "[playtest] route: %zu waypoints, first (%.1f, %.1f)\n",
                         ptc.waypoints.size(),
                         static_cast<double>(ptc.waypoints.front().x),
                         static_cast<double>(ptc.waypoints.front().y));
        }
        const glm::vec4 wbz = chunks_.world_bounds_xz();
        ptc.world_min = {wbz.x + 16.0f, wbz.y + 16.0f};
        ptc.world_max = {wbz.z - 16.0f, wbz.w - 16.0f};
        playtest_ = gameplay::make_playtest(ptc);
        pt_env_.terrain_height = [this](glm::vec2 xz) { return chunks_.height_at(xz); };
        pt_env_.water_analytic = [this](glm::vec2 xz) { return chunks_.water_surface_at(xz); };
        pt_env_.water_drawn = [this](glm::vec2 xz) -> std::optional<float> {
            const auto bodies = chunks_.water_bodies();
            for (const auto& l : bodies.lakes) {
                const glm::vec2 dd = (xz - l.center) / l.half_extent;
                if (glm::dot(dd, dd) <= 1.0f) {
                    return l.surface_height;
                }
            }
            std::optional<float> best;
            float best_d = 1e9f;
            for (const auto& st : bodies.river_stations) {
                const float dist = glm::length(xz - st.position);
                if (dist <= st.half_width && dist < best_d) {
                    best_d = dist;
                    best = st.surface_height;
                }
            }
            return best;
        };
        pt_env_.world_floor_y = -60.0f; // below every legitimate carve
        const char* dir = std::getenv("DFN_PLAYTEST_DIR");
        pt_dir_ = dir ? dir : ("screenshots/playtest_" + mode);
        std::filesystem::create_directories(pt_dir_);
        // The bot needs the world, not the cursor; the player is NOT frozen.
    }

    {
        const auto fb = window_->framebuffer_size();
        camera_.set_projection(static_cast<float>(config::CAMERA_FOV_Y),
                               static_cast<float>(fb.x) / static_cast<float>(fb.y),
                               static_cast<float>(config::CAMERA_NEAR),
                               static_cast<float>(config::CAMERA_FAR));
    }

    // FAR DETAIL. Chunk streaming reaches CHUNK_LOAD_RADIUS chunks from wherever
    // the player stands while CAMERA_FAR is 8 km, so without this the world ends
    // a few hundred metres away in every direction. Bounds come from CORE rather
    // than from generated config, because the configured extent and the
    // generated extent have already disagreed once this stage.
    //
    // Unconditional, with DFN_NO_LOD=1 as a tooling escape rather than a user
    // setting: with far detail off the world simply stops, which is a broken
    // game and not a quality preference, and a graphics option nobody sets is
    // an untested code path.
    {
        const glm::vec4 wb = chunks_.world_bounds_xz();
        render_system_.set_world_bounds({wb.x, wb.y}, {wb.z, wb.w});
        const char* no_lod = std::getenv("DFN_NO_LOD");
        render_system_.set_lod_enabled(!(no_lod != nullptr && *no_lod == '1'));
    }

    if (render::Tour::enabled_by_env()) {
        const char* dir = std::getenv("DFN_TOUR_DIR");
        // The stand publishes its own vantages, so a tour on the forest stand
        // photographs the forest instead of shooting one frame at a testbed
        // coordinate and stopping. An empty vantage list falls through to the
        // testbed route, so nothing about the old stand changes.
        tour_.begin(render::Tour::stand_steps(chunks_.stand_vantages()),
                    dir ? dir : "screenshots",
                    [this](glm::vec2 p) { return chunks_.height_at(p).value_or(0.0f); });
    } else {
        // NO UNATTENDED RUN OWNS THE MOUSE. This exemption was written for the
        // body probe alone, so every other automated door -- tour, playtest,
        // capture, restore -- still grabbed the desktop pointer and threw the
        // user into the game while he was working. The exemption belonged to
        // the PROPERTY (nobody is aiming), not to one door that happened to
        // have it.
        input_->set_cursor_captured(!unattended_run());
    }

    // A pending restore is applied LAST, once the world it describes exists and
    // the player is in it. Consumed rather than kept: re-entering a map from
    // the menu later should start fresh, not silently teleport to a pose from
    // a command line the player has long forgotten typing.
    if (restore_) {
        apply_restore(*restore_);
        restore_.reset();
    }
    return true;
}

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

int App::run() {
    auto last = std::chrono::steady_clock::now();
    while (!window_->should_close()) {
        window_->poll_events();
        input_->update();

        // MENU MODE: the engine is up, the world may not exist yet. Nothing
        // simulates here -- the menu is drawn over whatever the last frame was
        // (a dimmed world when paused, a plain ground before any world).
        if (mode_ == AppMode::Menu) {
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
            }
            switch (action) {
            case MenuAction::EnterWorld:
                if (!enter_world(menu_.chosen_stand())) {
                    return 1;
                }
                mode_ = AppMode::Playing;
                input_->set_cursor_captured(!unattended_run());
                break;
            case MenuAction::Resume:
                mode_ = AppMode::Playing;
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
                write_settings(config_);
                break;
            }
            case MenuAction::ToRoot:
            case MenuAction::None:
                break;
            }
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
            draw_menu(render_system_.hud(), menu_);
            render_system_.set_hud_visible(true);
            render_system_.render(world_, *renderer_, camera_, 0.0f);
            // VERIFICATION HOOK (Rule 27): a menu nobody can photograph is a
            // menu nobody can verify. DFN_MENU_SHOT=<path> captures one frame
            // of whichever page is showing and closes.
            if (const char* shot = std::getenv("DFN_MENU_SHOT");
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
            last = std::chrono::steady_clock::now(); // no frame_dt spike on resume
            continue;
        }
        if (window_->consume_resize()) {
            const auto fb = window_->framebuffer_size();
            renderer_->resize(fb.x, fb.y);
            camera_.set_projection(camera_.fov_y(),
                                   static_cast<float>(fb.x) / static_cast<float>(fb.y),
                                   camera_.near_plane(), camera_.far_plane());
        }
        // ESC pauses. Cursor is released so the pointer is usable, and the
        // world stops ticking because Menu mode skips the whole simulation.
        //
        // THERE WERE TWO ESCAPE HANDLERS HERE and the first one called
        // request_close(). Both ran on the same edge, so ESC opened the pause
        // menu AND asked the window to close, and the app quit on the next
        // iteration -- the pause screen existed but could never be seen. It
        // survived review because each half is correct on its own; only the
        // pair is wrong, which is why the fix is deleting a handler rather
        // than reordering them (Rule 32).
        if (input_->was_pressed(platform::Key::ESCAPE)) {
            if (render_system_.map_open()) {
                render_system_.set_map_open(false);
            } else {
                menu_.open(MenuPage::Pause);
                mode_ = AppMode::Menu;
                input_->set_cursor_captured(false);
                continue;
            }
        }
        // DEBUG READOUT (F3) and STATE CAPTURE (F2). User request: "нужна
        // кнопка с дебаг выводом, куда я смотрю, fps, скорость координата... надо
        // чтобы я мог скриншот сделать игры, скрина и состояния персонажа... чтобы
        // ты потом восстановил состояние игры, углы мои наклонов, позиций".
        // The capture is deferred to AFTER render() so the .png and the sidecar
        // describe the same frame; capturing here would save the state of frame
        // N next to the image of frame N-1.
        // USER-CHOSEN KEYS (his request): 1 third person, 2 the debug readout,
        // 3 the state capture. F3/F2 stay as aliases -- they are in the frames
        // and recipes already archived, and silently moving a key would make
        // every recipe on disk wrong.
        if (input_->was_pressed(platform::Key::NUM_1)) {
            third_person_ = !third_person_;
            orbit_yaw_ = 0.0f;
            orbit_pitch_ = 0.0f;
            // THE HEAD COMES BACK IN THIRD PERSON. It is hidden in first person
            // because the camera sits inside the skull; from behind, a headless
            // body is the first thing he would report, and it would read as a
            // missing mesh rather than as a deliberate first-person choice.
            if (auto* rig = world_.get<anim::BodyRig>(player_)) {
                rig->hide_head = !third_person_;
                const auto head = rig->segments[anim::bone_index(anim::Bone::Head)];
                if (auto* rm = world_.get<components::RenderMesh>(head)) {
                    rm->mesh_asset = third_person_
                        ? anim::body_segment_mesh_id(anim::Bone::Head) : 0u;
                }
            }
        }
        if (input_->was_pressed(platform::Key::NUM_2)
            || input_->was_pressed(platform::Key::F3)) {
            debug_overlay_ = !debug_overlay_;
        }
        if (input_->was_pressed(platform::Key::NUM_3)
            || input_->was_pressed(platform::Key::F2)) {
            capture_pending_ = true;
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
        // ...and the same door counted in frames, which IS comparable bit for
        // bit. Counted here rather than in the render block so it advances once
        // per loop iteration, exactly like the frame the log names.
        if (capture_after_frames_ > 0) {
            ++capture_after_frames_seen_;
            if (capture_after_frames_seen_ >= capture_after_frames_) {
                capture_pending_ = true;
                capture_after_frames_ = 0;
                capture_then_close_ = true;
            }
        }
        if (input_->was_pressed(platform::Key::M)) {
            render_system_.toggle_map();
            // Free the cursor while the map is up: mouse-look under a fullscreen
            // plate spins the world behind it for no reason.
            input_->set_cursor_captured(!render_system_.map_open() && !unattended_run());
        }

        const auto now = std::chrono::steady_clock::now();
        const double frame_dt = std::chrono::duration<double>(now - last).count();
        last = now;
        frame_clock_.push(static_cast<float>(frame_dt));
        // Cleared before the tick that may set it again (the chunk ferry does).
        world_changed_this_frame_ = false;

        // In-game clock (в67): DAY_LENGTH_SECONDS per day, with a debug key that
        // runs it DEBUG_TIME_SCALE faster so shadows can be watched sweeping.
        const double time_scale = input_->is_down(platform::Key::T)
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
        // A fixed increment per rendered frame makes the tour's world a pure
        // function of the frame INDEX, which is what an acceptance instrument
        // has to be. It changes nothing outside a tour: play still runs on the
        // wall clock, because play is not evidence.
        game_seconds_ += tour_.active() ? static_cast<double>(config::SIM_DT)
                                        : frame_dt * time_scale;
        const double day_len = static_cast<double>(config::DAY_LENGTH_SECONDS);
        const double days = game_seconds_ / day_len;
        const float day_fraction = static_cast<float>(days - std::floor(days));
        // The lunar phase is a PURE function of the date — no accumulated state,
        // so the moon is knowable for any past or future day (в69: werewolves,
        // vampires and lunar magic will depend on it).
        const double lunar = days / static_cast<double>(config::LUNAR_MONTH_DAYS);
        const float lunar_phase = static_cast<float>(lunar - std::floor(lunar));
        render::apply_sky_time(render_system_.environment(), day_fraction, lunar_phase);
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
        if (const auto* t = world_.get<components::Transform>(player_)) {
            render_system_.environment().ambient_darkness = chunks_.darkness_at(t->position);
        }

        if (!playtest_) {
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

        const uint32_t steps = paused ? 0u : timestep_.accumulate(frame_dt);

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
        if (steps > 0) {
            // A step must never execute against a world whose collision bodies
            // are one tick stale, or the player falls through terrain that has
            // not been created yet.
            glm::vec3 focus{0.0f};
            if (tour_.active()) {
                focus = tour_.focus_position();
            } else if (const auto* t = world_.get<components::Transform>(player_)) {
                focus = t->position;
            }
            chunks_.update(focus, world_, bus_);
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
                gameplay::player_pre_step(world_, *physics_,
                    [this](glm::vec2 xz) { return chunks_.water_surface_at(xz); },
                    step_ctx_);
                physics_->step(static_cast<float>(timestep_.step_dt()));
                gameplay::player_post_step(world_, *physics_, step_ctx_);

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
                // Actions AFTER the hover they act on: E interact, F light, I bag.
                gameplay::player_actions_step(world_, bus_, *physics_);
                // Carriers without a view model (NPCs with lanterns).
                gameplay::update_carried_lights(world_);
                // LAST: reads the CameraPose post_step wrote and the HeldItem
                // the actions may have just changed, so a torch picked up this
                // tick is in hand this tick rather than next.
                gameplay::update_view_model(world_);
                bus_.pump(); // deliver the interaction events published above
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
        if (pose != nullptr && prev_pose != nullptr) {
            // THIRD PERSON: the eye pulls back along the orbit direction. The
            // character's own yaw is untouched -- the camera moves, he does not
            // turn -- which is what makes standing-still orbiting read right.
            if (third_person_) {
                const float y0 = prev_pose->yaw + orbit_yaw_;
                const float y1 = pose->yaw + orbit_yaw_;
                const float p0 = prev_pose->pitch + orbit_pitch_;
                const float p1 = pose->pitch + orbit_pitch_;
                const auto back = [](float yaw, float pitch, glm::vec3 eye) {
                    const float cp = std::cos(pitch);
                    const glm::vec3 fwd{std::sin(yaw) * cp, std::sin(pitch),
                                        -std::cos(yaw) * cp};
                    // Distance and lift are debug-view framing, not world
                    // constants: this view exists so a human can watch the body
                    // (his stated reason -- «полезно для дебага»), so it is
                    // sized to hold the whole figure with headroom rather than
                    // derived from anything.
                    return eye - fwd * 3.2f + glm::vec3{0.0f, 0.55f, 0.0f};
                };
                camera_.set_poses({back(y0, p0, prev_pose->position), y0, p0},
                                  {back(y1, p1, pose->position), y1, p1});
            } else {
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
            std::fprintf(frame_log_,
                         "%llu %.4f %.6f %.6f %.8f %.6f %.6f %.6f %.6f %.6f\n",
                         static_cast<unsigned long long>(frame_log_index_++),
                         frame_dt * 1000.0, game_seconds_,
                         static_cast<double>(spd),
                         static_cast<double>(camera_.fov_y()),
                         static_cast<double>(eye.position.x),
                         static_cast<double>(eye.position.y),
                         static_cast<double>(eye.position.z),
                         static_cast<double>(eye.yaw),
                         static_cast<double>(eye.pitch));
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
            gameplay::update_wind_loop(*audio_, wind_loop_,
                                       render_system_.environment().wind_strength);
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
            glm::vec3 lod_focus{0.0f};
            if (tour_.active()) {
                lod_focus = tour_.focus_position();
            } else if (const auto* t = world_.get<components::Transform>(player_)) {
                lod_focus = t->position;
            }
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
            render_system_.update_lod(eye, tour_.active()
                                               ? static_cast<float>(config::SIM_DT)
                                               : static_cast<float>(frame_dt));

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

        // INTERACTION PROMPT. The cheapest visible thing in the project: the
        // hover path, the verbs and the keys have all existed for hours and
        // could not draw a pixel without glyphs. Shadow is not decoration --
        // at five pixels tall, unshadowed text vanishes over grass.
        {
            render::PixelCanvas& hud = render_system_.hud();
            hud.clear_transparent();
            bool any = false;
            // ПРИЦЕЛ. Подсказка взаимодействия рисуется по центру экрана, у
            // которого центр ничем не отмечен, — это и была жалоба на кадре
            // ui-ingame. Дверь дозы DFN_CROSSHAIR=0 живёт внутри функции: обе
            // руки приёмки из одного бинарника.
            HudFacts facts;
            facts.third_person = third_person_;
            facts.map_open = render_system_.map_open();
            facts.debug_readout = debug_overlay_ || capture_pending_;
            // КУДА СМОТРИТ ГЛАЗ, а не куда стоит тело: лента обязана совпасть
            // с картинкой, а картинка нарисована из позы КАМЕРЫ — той же, из
            // которой снимок состояния берёт свой yaw.
            facts.yaw_rad = camera_.interpolated_pose(alpha).yaw;
            facts.fov_y_rad = camera_.fov_y();
            // Здоровье/силы/магия остаются единицами: тратить их пока нечем, и
            // полоса, которая ползёт для вида, учит читать пустое число.
            any = draw_compass_ribbon(hud, facts) || any;
            any = draw_condition_bars(hud, facts) || any;
            any = draw_crosshair(hud, facts) || any;
            if (world_.has_resource<components::HoverTarget>()) {
                const auto& hover = world_.resource<components::HoverTarget>();
                if (hover.prompt_key != 0) {
                    const std::string_view text = localized(hover.prompt_key);
                    const int w = static_cast<int>(hud.width());
                    const int h = static_cast<int>(hud.height());
                    // The prompt stands on the same ground as the readout: same
                    // ink, same font, same 5 px letters, so ui's measurement
                    // applies to it word for word -- 56.1% of that ink fails the
                    // two-step separation rule wherever the background is bright,
                    // and this line is drawn over whatever the player happens to
                    // be facing. It was the only text left without a plate.
                    const int tw = render::text_width_px(text);
                    const int tx = (w - tw) / 2;
                    draw_text_plate(hud, tx, h - 40, tw, render::FONT_INK_H);
                    render::draw_text(hud, tx, h - 40, text,
                                      render::Color{232, 228, 214}, /*shadow=*/true);
                    any = true;
                }
            }
            // VERIFICATION HOOK (Rule 27, gated): draws a real prompt and a
            // deliberate MISS side by side, so the placeholder is proved to be
            // unmistakable rather than assumed to be.
            if (const char* probe = std::getenv("DFN_HUD_PROBE");
                probe != nullptr && *probe == '1') {
                const int w = static_cast<int>(hud.width());
                const int h = static_cast<int>(hud.height());
                const std::string_view hit = localized(serialization::fnv1a64("prompt.take"));
                const std::string_view miss = localized(serialization::fnv1a64("prompt.nonexistent"));
                render::draw_text(hud, (w - render::text_width_px(hit)) / 2, h - 40,
                                  hit, render::Color{232, 228, 214}, true);
                render::draw_text(hud, (w - render::text_width_px(miss)) / 2, h - 24,
                                  miss, render::Color{232, 228, 214}, true);
                any = true;
            }
            // The readout draws LAST inside the HUD block so it is never
            // occluded by a prompt, and it forces the layer visible: a debug
            // view that can be hidden by whatever else is on screen is not a
            // debug view.
            if (debug_overlay_ || capture_pending_) {
                draw_debug_overlay(hud, collect_snapshot(alpha));
                any = true;
            }
            render_system_.set_hud_visible(any);
        }

        render_system_.render(world_, *renderer_, camera_, alpha);

        // CAPTURE AFTER RENDER, so the .png and the sidecar are the same frame.
        // The snapshot is collected a second time here rather than reused from
        // the overlay above -- one frame of drift between the image and its
        // state file is exactly the kind of small lie that makes a repro fail
        // for reasons nobody can find.
        if (capture_pending_) {
            capture_pending_ = false;
            write_capture(collect_snapshot(alpha));
            // CLOSING HERE WOULD LOSE THE PNG. save_screenshot() returns true
            // when the capture has been REQUESTED, not when the file exists --
            // the bgfx backend reads the framebuffer back over the following
            // frames. Closing on the same frame produced a .txt with no .png
            // beside it, and, worse, a "[capture] ok" line above the pair. So
            // the tooling door waits for the flush; the same reason the body
            // probe holds a 4-frame cooldown between shots.
            if (capture_then_close_) {
                close_after_flush_ = 8;
            }
        }
        // How far the restore actually got. IPhysics has no teleport (see
        // apply_restore), so this is the check that keeps a half-completed
        // restore from passing as a completed one.
        if (close_after_flush_ > 0 && --close_after_flush_ == 0) {
            window_->request_close();
        }
        if (restore_target_) {
            if (const auto* ps = world_.get<gameplay::PlayerState>(player_)) {
                const glm::vec3 got = physics_->character_position(ps->character);
                // HORIZONTAL AND VERTICAL ERROR ARE DIFFERENT QUANTITIES and
                // only one of them is a failure. A straight 3D distance called
                // the first working restore BLOCKED at 1.138 m -- of which
                // 0.07 m was horizontal and the rest was the capsule settling
                // onto the ground, which is the controller doing its job. The
                // captured eye height is a float that lands a few centimetres
                // off the terrain; the player then falls those centimetres,
                // every time, correctly. Which quantity the threshold sits on
                // is itself a measurement (Rule 30), and this one was on the
                // wrong quantity -- it would have cried wolf on every restore
                // ever taken, which is precisely how a check gets ignored.
                const float dx = got.x - restore_target_->x;
                const float dz = got.z - restore_target_->z;
                const float horiz = std::sqrt(dx * dx + dz * dz);
                const float vert = got.y - restore_target_->y;
                std::fprintf(stderr,
                             "[restore] landed %.2f %.2f %.2f  horiz %.3f m  "
                             "settle %.3f m%s\n",
                             static_cast<double>(got.x), static_cast<double>(got.y),
                             static_cast<double>(got.z), static_cast<double>(horiz),
                             static_cast<double>(vert),
                             horiz > 1.0f
                                 ? "  -- BLOCKED, this is NOT the captured spot"
                                 : "");
            }
            restore_target_.reset();
        }
        body_probe_frame(alpha, static_cast<float>(frame_dt));
        // THE TOUR'S SETTLE IS GATED ON THE WORLD HAVING STOPPED CHANGING,
        // not on frames elapsing. `Tour.cpp` waits a fixed 45 RENDERED FRAMES
        // for streaming that is driven in SIM STEPS off a wall clock -- Rule 42,
        // a budget denominated in one clock's units enforcing a limit that only
        // matters in another's. The cost was measured, not guessed: two runs of
        // the SAME binary at the SAME commit differ by 17.4% of pixels (34.7%
        // re-measured later), so no full-tour pixel claim below ~20% has ever
        // certified anything -- in the instrument this project uses for Rule 27.
        //
        // The gate lives HERE rather than in Tour.cpp because the app is the
        // only place that can see all three queues at once, and because it needs
        // no change to render's contract: withholding the call simply pauses the
        // countdown, so the 45 frames now run on a SETTLED world instead of
        // starting at the refocus.
        //
        // HYSTERESIS IS NOT OPTIONAL: a queue legitimately reads empty for one
        // frame mid-refocus, so quiescence must HOLD. And the cap is a backstop
        // that REPORTS -- an unreachable vantage must say so rather than hang,
        // because a tour that quietly never finishes is the same silent-zero
        // failure as a capture that wrote nothing.
        //
        // HONEST GAP, disclosed rather than papered over: there is no
        // chunk-pending accessor, so "chunks still arriving" is inferred from
        // ChunkLoaded/ChunkUnloaded events seen this frame. That is sound for
        // "something arrived" and blind to "something is queued and has not
        // arrived yet" -- a request to core is out for the real counter, and
        // until it lands the cap is doing more work than it should.
        if (tour_.active()) {
            const bool quiet = !world_changed_this_frame_
                               && chunks_.coarse_pending_count() == 0
                               && render_system_.lod_pending().empty();
            quiet_frames_ = quiet ? quiet_frames_ + 1 : 0;
            const bool settled = quiet_frames_ >= 4;
            // The cap counts frames since the world was last SETTLED, so it
            // measures "this vantage is not converging" rather than "the tour
            // has been running a while" -- the second would fire on a long but
            // healthy route.
            tour_settle_frames_ = settled ? 0 : tour_settle_frames_ + 1;
            const bool capped = tour_settle_frames_ >= 600;
            if (capped && !settled) {
                std::fprintf(stderr,
                             "[tour] vantage never settled after %d frames "
                             "(quiet=%d coarse=%zu lod=%zu) -- shooting anyway, "
                             "this frame is NOT evidence\n",
                             tour_settle_frames_, quiet_frames_,
                             chunks_.coarse_pending_count(),
                             render_system_.lod_pending().size());
            }
            if ((settled || capped) && tour_.post_frame(*renderer_)) {
                window_->request_close(); // tour finished (render's contract)
            }
        }
        if (playtest_ && playtest_->finished && pt_artifacts_pending_) {
            gameplay::playtest_write_artifacts(*playtest_, pt_dir_);
            pt_artifacts_pending_ = false;
            window_->request_close();
        }
    }
    // Gate: a playtest run with incidents exits nonzero (Main passes it through).
    if (playtest_) {
        return playtest_->incidents.empty() ? 0 : 1;
    }
    return 0;
}

void App::shutdown() {
    if (frame_log_ != nullptr) {
        std::fprintf(stderr, "[frame_log] %llu frames written\n",
                     static_cast<unsigned long long>(frame_log_index_));
        std::fclose(frame_log_);
        frame_log_ = nullptr;
    }
    if (physics_) {
        for (auto& [key, cp] : g_chunk_physics) {
            physics_->destroy_body(cp.body);
        }
        g_chunk_physics.clear();
    }
    if (renderer_) {
        chunks_.unload_all(world_, bus_);
        bus_.pump();
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
