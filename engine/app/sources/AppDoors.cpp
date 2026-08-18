/*
Created: 18:08:2026 - 17:32:10
Last updated: 18:08:2026 - 17:32:10
Module: engine/app
File: engine/app/sources/AppDoors.cpp

Responsibility:
- The door table itself, its lookup, the one read, and unattended_run() derived
  from the table's own column. See AppDoors.h for why any of it is a table.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Zone editor owns this file.
*/
/*
UPD:
- 18:08:2026 - 17:32:10: Создан вместе с заголовком — 58 дверей, собранных из шести
  файлов зоны app.
*/

#include "engine/app/sources/AppDoors.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace dfn::app {
namespace {

// THE TABLE. Alphabetical order was rejected: the doors group by WHAT THEY ARE
// FOR -- unattended evidence, then the editor, then the picture, then the
// backends -- and a reader arriving with "is there a door for X" finds X
// faster among its neighbours than among names that merely start alike.
constexpr std::array<Door, 58> TABLE{{
    {"DFN_TOUR",
     "маршрут облёта: камера ведётся по точкам, каждая снимается, приложение закрывается после последней. Счётные часы (кадр — единица времени), иначе два прогона снимут разный час и разный порыв ветра. ЗНАЧЕНИЕ читает render::Tour (engine/render/sources/Tour.cpp); зона app спрашивает только, открыта ли она.",
     DoorRead::Once, true},
    {"DFN_TOUR_DIR",
     "куда класть кадры тура (по умолчанию рядом с рабочим каталогом).",
     DoorRead::Once},
    {"DFN_PLAYTEST",
     "автономный ходок: бот идёт сам, проверки инварианта на каждом тике, кадр на каждое происшествие. Ненулевой выход при происшествиях.",
     DoorRead::Once, true},
    {"DFN_PLAYTEST_ROUTE",
     "файл маршрута для бота вместо случайной прогулки.",
     DoorRead::Once, true},
    {"DFN_PLAYTEST_SEED",
     "зерно генератора бота: тот же прогон повторяется побитово.",
     DoorRead::Once},
    {"DFN_PLAYTEST_SECONDS",
     "сколько секунд игрового времени ходить.",
     DoorRead::Once},
    {"DFN_PLAYTEST_GAIT",
     "передача бота (walk/jog/run). Без неё автоматика ни разу не бывала на двух передачах из трёх.",
     DoorRead::Once},
    {"DFN_PLAYTEST_DIR",
     "куда класть отчёт и кадры происшествий.",
     DoorRead::Once},
    {"DFN_CAPTURE_AFTER",
     "снять состояние через N СЕКУНД и закрыться. Секунды несравнимы побитово между машинами — для сравнения есть счёт в кадрах ниже.",
     DoorRead::Once, true},
    {"DFN_CAPTURE_AFTER_FRAMES",
     "то же, но через N ОТРИСОВАННЫХ КАДРОВ. Единственная из двух единиц, дающая две одинаковые руки на разной загрузке машины.",
     DoorRead::Once, true},
    {"DFN_SHOT_AFTER",
     "снимок экрана (то же, что клавиша 5) через N кадров, потом закрыться. Кадры, а не секунды, по той же причине.",
     DoorRead::Once, true},
    {"DFN_CAPTURE_DIR",
     "каталог снимков состояния и источник восстановления.",
     DoorRead::Once},
    {"DFN_RESTORE",
     "поднять состояние из сайдкара снимка: позиция, углы, приседание, время суток.",
     DoorRead::Once, true},
    {"DFN_BODY_PROBE",
     "проба тела: мир живёт, кадр снимается по состоянию симуляции. Тур замораживает тик и потому умеет снимать только натюрморт.",
     DoorRead::Once, true},
    {"DFN_BODY_PROBE_DIR",
     "куда класть кадры пробы тела.",
     DoorRead::Once},
    {"DFN_BODY_PITCH",
     "наклон взгляда пробы: снять собственные ноги из своего же черепа нельзя, поэтому угол задаётся снаружи.",
     DoorRead::Once},
    {"DFN_MIRROR",
     "зеркальный двойник на стенде: повторяет позу игрока.",
     DoorRead::Once},
    {"DFN_SHOWCASE",
     "двойник не повторяет, а крутит катушку клипов — витрина анимаций.",
     DoorRead::Once},
    {"DFN_MENU",
     "показать (1) или пропустить (0) стартовое меню.",
     DoorRead::Once},
    {"DFN_MENU_PAGE",
     "открыть меню сразу на названной странице (categories/category_maps/pause/calibrate/settings).",
     DoorRead::Once},
    {"DFN_MENU_SHOT",
     "снять один кадр показанной страницы меню и закрыться. МЕНЮ ПРИ ЭТОМ ПОКАЗЫВАЕТСЯ, хотя прогон и беспилотный, — см. предупреждение у unattended_run().",
     DoorRead::Once, true},
    {"DFN_OPEN_MAP",
     "открыть карту <категория>/<карта> минуя браузер. Названа не DFN_MAP: то занято щупом экрана карты у render.",
     DoorRead::Once, true},
    {"DFN_STAND",
     "какой стенд поднимать, когда меню выключено. Раньше это была DFN_MAP, и маршрут стенда молча схлопывался в один кадр.",
     DoorRead::Once},
    {"DFN_EDITOR",
     "войти в режим редактора; без карты — открыть браузер редактора.",
     DoorRead::Once},
    {"DFN_EDITOR_CAM",
     "поставить свободную камеру редактора в x,y,z,yaw,pitch (абсолютные мировые).",
     DoorRead::Once},
    {"DFN_EDITOR_CAM_REL",
     "то же, но от точки спавна карты. Читается ПОСЛЕ абсолютной, чтобы рецепт с обеими получил ту, что выписал явно.",
     DoorRead::Once},
    {"DFN_EDITOR_TOOL",
     "взять инструмент 1..5 без нажатия клавиши — через тот же click_icon, каким его берёт человек.",
     DoorRead::Once},
    {"DFN_EDITOR_SETTINGS",
     "открыть настройки инструмента 1..5, НЕ трогая руку.",
     DoorRead::Once},
    {"DFN_EDITOR_BRUSH",
     "открыть настройки кисти рельефа (то же, что треугольник под её фишкой).",
     DoorRead::Once},
    {"DFN_EDITOR_PARTS",
     "нажать «меню объектов» — тот же метод, что и клавиша B.",
     DoorRead::Once},
    {"DFN_BUILD",
     "взять инструмент постройки и включить отладочные линии, чтобы призрак попал на кадр беспилотного прогона.",
     DoorRead::Once},
    {"DFN_EDITOR_HUD_PINNED",
     "закрепить редакторский блок оверлея на месте: кадры приёмки сравнимы между прогонами.",
     DoorRead::Once},
    {"DFN_THIRD_PERSON",
     "включить третье лицо на первом же игровом кадре. До этой двери вид сзади мог увидеть только человек, нажавший 1.",
     DoorRead::Once},
    {"DFN_WIREFRAME",
     "каркас всей сцены (то же, что клавиша 4/F4).",
     DoorRead::Once},
    {"DFN_DEBUG_OVERLAY",
     "отладочный вывод включён с первого кадра (то же, что клавиша 2/F3).",
     DoorRead::Once},
    {"DFN_DRAW_COLLIDERS",
     "рисовать РЁБРА ТРЕУГОЛЬНИКОВ, отданных физике, — не коробку вокруг них и не пересчёт.",
     DoorRead::Once},
    {"DFN_CAM_TRACE",
     "печатать на каждом кадре редактора пару «пришло смещение мыши / стал рыск». Заведена потому, что «мышь не дошла» и «камера проигнорировала» выглядели одинаково три захода подряд.",
     DoorRead::Once},
    {"DFN_FRAME_LOG",
     "строка на каждый ПРЕДЪЯВЛЕННЫЙ кадр: dt, игровые секунды, скорость, fov, поза глаза. Без обратного чтения и без заморозки тика — тем и ловит дефекты МЕЖДУ кадрами.",
     DoorRead::Once},
    {"DFN_CHAT_MSG",
     "записать строку в чат карты со снимком кадра и закрыться. Так проверяется сам путь записи.",
     DoorRead::Once, true},
    {"DFN_CHAT_WHO",
     "от чьего имени пишет строка выше: human по умолчанию или имя зоны для самодокументации демки.",
     DoorRead::Once},
    {"DFN_TRAJ_REC",
     "писать траекторию с первого кадра (интерактивная запись — клавиша K в редакторе).",
     DoorRead::Once},
    {"DFN_TRAJ_PLAY",
     "воспроизвести .dftraj: глаз ведётся из файла, часы берутся из файла, две прокрутки дают побитово одинаковую картинку.",
     DoorRead::Once, true},
    {"DFN_INTERNAL_RES",
     "внутренняя сетка отрисовки WxH (окно масштабируется целым).",
     DoorRead::Once},
    {"DFN_PALETTE",
     "палитровый пост: 64 цвета и дизеринг.",
     DoorRead::Once},
    {"DFN_BLACK_FLOOR",
     "нижний предел яркости картинки. Отдельная дверь потому, что settings.cfg общий для всех зон, и прогон не должен его править ради кадра.",
     DoorRead::Once},
    {"DFN_HEAD_BOB",
     "множитель покачивания камеры; 0 гасит движение, оставляя звук и анимацию — так тряска либо доказывается движением камеры, либо снимается с него.",
     DoorRead::Once},
    {"DFN_TIME_OF_DAY",
     "час суток на старте. Разбор значения строгий: опечатка, тихо ставшая полднем, отправила бы искать работающий свет.",
     DoorRead::Once},
    {"DFN_NO_LOD",
     "выключить дальний рельеф (LOD) целиком.",
     DoorRead::Once},
    {"DFN_AUDIO",
     "включить настоящий звуковой бэкенд.",
     DoorRead::Once},
    {"DFN_NULL_AUDIO",
     "нулевой звук (правило 3).",
     DoorRead::Once},
    {"DFN_NULL_RENDER",
     "нулевой рендер: кадр считается, ничего не рисуется.",
     DoorRead::Once},
    {"DFN_NULL_PHYSICS",
     "нулевая физика.",
     DoorRead::Once},
    {"DFN_HUD",
     "0 — ЧИСТЫЙ КАДР: ни компаса, ни полос, ни прицела, ни отладочного блока. Для кадров, которые смотрит человек.",
     DoorRead::Once},
    {"DFN_HUD_PROBE",
     "нарисовать настоящую подсказку и нарочный ПРОМАХ рядом: заглушка доказывается неразличимостью, а не предполагается.",
     DoorRead::Once, true},
    {"DFN_CROSSHAIR",
     "0 — убрать прицел.",
     DoorRead::Once},
    {"DFN_HUD_RIBBON",
     "0 — убрать ленту компаса.",
     DoorRead::Once},
    {"DFN_HUD_BARS",
     "0 — убрать полосы состояния.",
     DoorRead::Once},
    {"DFN_UI_PLATE",
     "0 — убрать подложку под текстом оверлея (мерка читаемости на светлом фоне).",
     DoorRead::Once},
}};

} // namespace

std::span<const Door> doors() { return TABLE; }

const Door* find_door(std::string_view name) {
    for (const Door& d : TABLE) {
        if (name == d.name) {
            return &d;
        }
    }
    return nullptr;
}

const char* door_value(std::string_view name) {
    if (find_door(name) == nullptr) {
        // LOUD, AND ONCE. A door read but never listed is exactly the state
        // this table was built to end: it works for whoever wrote it and does
        // not exist for anybody else. Refusing to read it turns "undocumented"
        // into "does not work", which is a complaint somebody makes out loud.
        std::fprintf(stderr,
                     "[doors] %.*s ЧИТАЕТСЯ, но строки в таблице у неё нет — "
                     "дверь НЕ открыта (engine/app/sources/AppDoors.cpp)\n",
                     static_cast<int>(name.size()), name.data());
        return nullptr;
    }
    // std::getenv wants a NUL-terminated name; every caller passes a literal,
    // so the copy is one small string per read of a door that is usually
    // latched anyway.
    const std::string key(name);
    return std::getenv(key.c_str());
}

bool unattended_run() {
    // DERIVED, NEVER LISTED. The predecessor of this loop was a thirteen-term
    // `||` chain written out by hand, and its own comment records that a door
    // had been swept into it twice by edits that looked harmless. A column is
    // not sweepable: adding a door with the flag unset changes nothing here,
    // and setting it is a one-word diff that says what it means.
    for (const Door& d : TABLE) {
        if (d.unattended && door_value(d.name) != nullptr) {
            return true;
        }
    }
    return false;
}

} // namespace dfn::app
