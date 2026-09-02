/*
Module: engine/app
File: engine/app/sources/CharGenBody.h

Responsibility:
- ТЕЛО ЭКРАНА СОЗДАНИЯ ПЕРСОНАЖА: НАСТОЯЩИЙ ИГРОВОЙ ПЕРСОНАЖ (SkinnedCharacter
  через ту же фабрику, что зовёт мир, — рест-поза по коже, клип покоя со
  слоем стойки и обходом рук, хитбоксы по коже, тела Jolt), поверх которого
  живут ползунки: бленд морф-целей, равномерный масштаб роста, пресет и
  выпечка. Ни окна, ни холста, ни ввода: всё это в CharGen.h.

Key items:
- CharGenBody: load / apply / settle / tick / draw / release, веса ползунков,
  рост, пресет, выпечка, прибор зазоров.
- baked_object(): ОДНО тело для экрана и для выпечки — бленд ползунков и
  масштаб роста, применённые к исходному объекту. Экран показывает его,
  «Готово» пишет его: в мир уходит ровно то, что было на экране, по
  построению, а не по совпадению.
- CHARGEN_* : полоса роста и пути двух файлов.

Dependencies:
- Uses: engine/app SkinnedCharacter / CharacterFactory / BodyHitboxes,
  engine/render (ObjectRegistry, MorphBlend, RenderSystem), engine/platform
  IRenderer / IPhysics, engine/anim (Rig, BodyGaps).
- Used by: engine/app (AppCharGen.cpp), tests/app/CharGenTests.cpp,
  tests/app/CharacterPathTests.cpp — и тесты гоняют ЕГО ЖЕ с нулевым
  бэкендом.

Notes:
- ПОЧЕМУ ТЕПЕРЬ SkinnedCharacter (решение владельца 02.09, вариант В).
  Прежний экран показывал слепок: вершины, скиннованные один раз в нулевую
  позу рига через ретаргет, — другой путь позы, чем у мира, и на нём ноги
  слиплись там, где стенд отчитывался нулём пересечений. Один путь
  построения персонажа (CharacterFactory) на мир, экран и смотровую — и
  прибор, стоящий на экране, меряет то, чем игрок пойдёт.
- ДВЕ СКОРОСТИ У ПОЛЗУНКА. apply() — только меш: бленд, масштаб, замена
  буфера и коробки по коже — на каждый кадр перетаскивания. settle() — ПОЛНАЯ
  пересборка тела фабрикой из baked_object(): рест-поза решается заново по
  уже вылепленной коже (широкий таз просит больше отведения рук), библиотека
  клипов калибруется заново. 200 мс, и потому на ОТПУСКАНИИ ручки, на шаге
  стрелкой, на пресете и на «Готово», а не на каждом кадре.
- РОСТ ЖИВЁТ В ГЕОМЕТРИИ, А НЕ В МАТРИЦЕ КАДРА, потому что в мир уезжает
  геометрия: тело экрана = тело выпечки, и масштаб в матрице был бы вторым
  ростом, который выпечке пришлось бы повторять. chargen_in_camera получает
  множитель 1 и уже масштабированный габарит.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead) владеет этим файлом.
- Выпечка ДЕТЕРМИНИРОВАНА: одни и те же числа обязаны давать побайтово один
  файл, иначе приёмка «пресет воспроизводим» меряет погоду. Ничего от часов,
  ничего от порядка, в котором крутили ручки.
*/

#pragma once

#include "engine/anim/sources/BodyGaps.h"
#include "engine/anim/sources/Rig.h"
#include "engine/app/sources/CharacterFactory.h"
#include "engine/app/sources/SkinnedCharacter.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/platform/physics/interfaces/IPhysics.h"
#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/render/sources/MorphBlend.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/RenderSystem.h"

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace dfn::app {

// --- РОСТ -------------------------------------------------------------------

/// ИМЯ РУЧКИ РОСТА. Совпадает с ключом локализации `morph.slider.stature`,
/// заведённым шагом 1 под ползунок, которого тогда не могло быть, и с ключом
/// пресета: одно слово на подпись, на файл и на дозу (правило 32).
///
/// И ЭТО НЕ ЦЕЛЬ MORF. Шаг 1 доказал, что рост чистым морфом невозможен:
/// судья пропускает ±1 см, потому что морф двигает МЕШ, а не суставы, и
/// голова отрывается от черепа (6.16 голов в фигуре при +1 и 10.09 при −1
/// против канона 7.5-8.0). Рост едет РАВНОМЕРНЫМ МАСШТАБОМ рест-скелета,
/// меша и переносов клипов — то есть тем единственным преобразованием,
/// которое пропорций не трогает вовсе.
inline constexpr const char* CHARGEN_HEIGHT_KEY = "stature";

/// РОСТ ФИГУРЫ, КАК ОНА ПРИШЛА. Замер, а не допущение: `dfn_human_scale
/// assets/objects/characters/HumanBase.dfo` печатает «figure height 1.750 m».
inline constexpr float CHARGEN_BODY_HEIGHT_M = 1.750f;
/// ПОЛОСА — ±5 % ОТ ЭТОГО РОСТА. Судья пропорций масштабу безразличен по
/// построению (все его ориентиры, кроме числа голов, — доли роста), поэтому
/// полосу держит не он, а канон человеческого роста, названный заказом.
inline constexpr float CHARGEN_HEIGHT_MIN_M = 1.66f;
inline constexpr float CHARGEN_HEIGHT_MAX_M = 1.84f;

/// Множитель рест-скелета, меша и переносов клипов, дающий этот рост.
[[nodiscard]] float chargen_height_scale(float height_m);

// --- ГДЕ ЛЕЖИТ ПРЕСЕТ И ВЫПЕЧЕННОЕ ТЕЛО -------------------------------------
//
// РЯДОМ С ТЕМ, ЧТО УЖЕ ПИШЕТ ШАГ 1, И ЭТО ВЫБОР ИЗ ДВУХ, А НЕ УМОЛЧАНИЕ.
// Заказ предлагал второе место — сейв-профиль, — и его в дереве НЕТ: строка
// «Загрузить» главного меню ведёт на заглушку `menu.stub.no_saves`, каталога
// пользователя не существует вовсе (ни одного обращения к HOME или APPDATA во
// всём engine и tools), а settings.cfg лежит относительным путём в рабочем
// каталоге. То есть «честнее с зоной сейвов» сегодня означает «нигде».
// Пресет кладётся туда, куда его уже кладёт панель редактора шага 1
// (assets/characters/presets/), и ровно двумя именованными строками — чтобы
// переезд в сейв-профиль, когда та зона появится, был правкой этих двух
// строк, а не поиском по дереву.
inline constexpr const char* CHARGEN_PRESET_PATH =
    "assets/characters/presets/player.json";
inline constexpr const char* CHARGEN_BAKED_PATH =
    "assets/characters/presets/player.dfo";

/// ИСХОДНОЕ ТЕЛО — ОДНО ИМЯ НА ЭКРАН И НА МИР, и это правило 32, а не
/// аккуратность.
///
/// ПОЧЕМУ ЭТА СТРОКА ВЫНЕСЕНА СЮДА. Она стояла ДВАЖДЫ: своей константой в
/// AppCharGen.cpp и склеенной из каталога и имени в AppWorld.cpp. Пока обе
/// копии совпадали, «экран показывает то тело, которым игрок пойдёт в мир»
/// было ПРАВДОЙ ПО СОВПАДЕНИЮ — а совпадение расходится в день, когда одну из
/// двух правят. И расходится оно МОЛЧА: обе половины продолжают показывать
/// человека, просто разного, и заметить это можно только поставив два кадра
/// рядом. Владелец 01.09 и поставил — и спросил, почему на экране создания не
/// тот, кого он видел в смотровой.
inline constexpr const char* CHARGEN_SOURCE_BODY =
    "assets/objects/characters/HumanBase.dfo";
/// ТА ЖЕ МОДЕЛЬ БЕЗ `--reshape` — рука «до» двери DFN_BODY_V1 (правило 47).
inline constexpr const char* CHARGEN_SOURCE_BODY_V1 =
    "assets/objects/characters/HumanBaseV1.dfo";

/// СОДЕРЖИМОЕ ФАЙЛА ТЕЛА ОДНИМ ЧИСЛОМ, или 0, если файла нет.
///
/// НЕ УКРАШЕНИЕ ЖУРНАЛА. «Экран показывает то же тело, что мир» — утверждение
/// о БАЙТАХ, и проверяется оно байтами: два потребителя, назвавшие одну
/// строку, всё ещё могли бы читать разные файлы, если бы кто-то переписал один
/// из них между запусками. Хэш печатается в журнал экрана и сверяется набором.
[[nodiscard]] std::uint64_t chargen_body_hash(const std::filesystem::path& path);

/// ХЭШ ПОЗЫ/МЕША: положения вершин тела в рест-позе рига, квантованные до
/// десятой миллиметра, свёрнутые fnv1a64. Им написана приёмка «после «Готово»
/// в мир уходит ровно то, что было на экране»: экран считает его со своего
/// тела, мир — с загруженной выпечки, и два числа обязаны совпасть.
[[nodiscard]] std::uint64_t chargen_pose_hash(const SkinnedCharacter& body);

/// ЧИСЛА ПРЕСЕТА, БЕЗ ТЕЛА. Ровно то, что игрок накрутил: имена целей и веса,
/// рост в метрах и имя персонажа. Тело из этого выводится, а не хранится.
struct CharGenPreset {
    std::string name;
    /// ОТКУДА ЭТОТ ЧЕЛОВЕК: id народа и id типажа из .people, или пусто.
    ///
    /// ОДИН ФОРМАТ НА ТРИ ПРИМЕНЕНИЯ (CHARGEN_UI.md, раздел 4): пресет
    /// игрока, типаж народа и запись НПС — это ОДИН файл, и поэтому генератор
    /// населения ест выход экрана создания без переходника. Народ, не
    /// записанный здесь, пришлось бы восстанавливать по числам ползунков —
    /// то есть угадывать.
    ///
    /// ID, А НЕ КЛЮЧ ЛОКАЛИЗАЦИИ И НЕ НОМЕР. Номер в списке меняется от
    /// первого нового файла в каталоге; ключ локализации — это подпись, и
    /// перевод её на другой язык не имеет права поменять народ персонажа.
    std::string people;
    std::string archetype;
    float height_m = CHARGEN_BODY_HEIGHT_M;
    std::vector<std::pair<std::string, float>> sliders;
};

class CharGenBody {
public:
    /// Читает .dfo, берёт его цели MORF и строит ИГРОВОГО персонажа фабрикой
    /// (CharacterFactory: рест-поза по коже, клипы, хитбоксы, тела Jolt —
    /// `physics` может быть null в наборе). False (и жалоба в поток ошибок)
    /// оставляет объект пустым и безвредным. `rig` даёт ПРОПОРЦИИ;
    /// `legacy_rest` — прежняя коробочная рест-поза, рука «до» (правило 47).
    [[nodiscard]] bool load(render::RenderSystem& render_system,
                            platform::IRenderer& renderer, platform::IPhysics* physics,
                            const anim::Rig& rig, const std::filesystem::path& path,
                            bool legacy_rest = false);
    void release(render::RenderSystem& render_system, platform::IRenderer& renderer,
                 platform::IPhysics* physics);
    [[nodiscard]] bool ready() const { return character_.ready(); }

    [[nodiscard]] const std::vector<render::MorphTarget>& morphs() const {
        return source_.morphs;
    }
    [[nodiscard]] const render::MorphState& weights() const { return weights_; }
    /// Ставит вес, ЗАЖИМАЯ его в полосу цели (полоса лежит в файле и измерена
    /// приёмкой шага 1). true — значение изменилось.
    bool set_weight(std::size_t index, float value);
    /// То же ПО ИМЕНИ ЦЕЛИ. Экран знает строки по именам (номер строки и
    /// номер цели совпадают ровно до первой категории, вставленной перед
    /// телосложением), и связывать их номером значило бы завести
    /// зависимость между раскладкой экрана и порядком секции MORF.
    bool set_weight(std::string_view name, float value);
    void reset();

    [[nodiscard]] float height_m() const { return height_m_; }
    /// true — рост изменился. Зажимается в канон.
    bool set_height_m(float metres);
    [[nodiscard]] float height_scale() const { return chargen_height_scale(height_m_); }

    /// БЫСТРАЯ ПОЛОВИНА: бленд + масштаб + замена меша + коробки по коже.
    /// На движение ручки, не покадрово (MorphBlend.h: 0.146 мс на 8546
    /// вершин и 11 целей).
    bool apply(render::RenderSystem& render_system, platform::IRenderer& renderer);
    /// МЕДЛЕННАЯ ПОЛОВИНА: тело пересобрано фабрикой из baked_object() —
    /// рест-поза решена заново по вылепленной коже, клипы откалиброваны,
    /// тела Jolt поставлены. На отпускании ручки, шаге стрелкой, пресете.
    bool settle(render::RenderSystem& render_system, platform::IRenderer& renderer,
                platform::IPhysics* physics);

    /// ОДИН ТИК ТЕЛА: клип покоя со слоями, как в мире. `dt` — секунды.
    void tick(float dt);
    /// КАДР: палитра позы тела и коробки, сдвинутые в мир матрицей `to_world`
    /// (та же, которой рисуют). Возвращает draw с палитрой.
    [[nodiscard]] render::RenderSystem::SkinnedDraw draw(float alpha,
                                                         platform::IPhysics* physics,
                                                         const glm::mat4& to_world);

    /// ИГРОВОЙ ПЕРСОНАЖ ЭКРАНА — тот же класс, что у игрока в мире.
    [[nodiscard]] const SkinnedCharacter& character() const { return character_; }
    [[nodiscard]] SkinnedCharacter& character() { return character_; }
    [[nodiscard]] const CharacterBodies& bodies() const { return bodies_; }
    [[nodiscard]] std::size_t triangles() const { return character_.triangle_count(); }
    /// Габарит РЕСТ-ПОЗЫ с текущими ползунками и ростом, в осях модели
    /// (уже в метрах роста: масштаб сидит в геометрии).
    [[nodiscard]] const glm::vec3& lo() const { return lo_; }
    [[nodiscard]] const glm::vec3& hi() const { return hi_; }

    /// ПРИБОР НА ПУТИ ИГРОКА: зазоры нога↔нога, кисть↔бедро, предплечье↔корпус
    /// в рест-позе ТОГО тела, что на экране, — те же вершины, та же привязка,
    /// та же нулевая поза, из которых собран портрет.
    [[nodiscard]] anim::BodyGaps screen_gaps() const;

    /// ТЕЛО КАК ОНО УЕДЕТ В МИР: исходный объект с блендом ползунков, снятой
    /// секцией MORF и масштабом роста. Экран строится из НЕГО, «Готово» пишет
    /// ЕГО. Детерминирован.
    [[nodiscard]] render::RegistryObject baked_object() const;

    /// Пресет из текущего состояния (плюс переданное имя).
    [[nodiscard]] CharGenPreset preset(std::string name) const;
    /// Ставит состояние по пресету. Неизвестные имена целей ПРОПУСКАЮТСЯ и
    /// называются вслух: пресет старше тела — не повод отказать в экране.
    void apply_preset(const CharGenPreset& preset);

    /// ВЫПЕЧКА: baked_object() на диск. Возвращает false и говорит вслух.
    [[nodiscard]] bool bake(const std::filesystem::path& out) const;

private:
    render::RegistryObject source_{};             ///< как в файле, с MORF
    std::vector<platform::SkinnedVertex> blended_; ///< рабочий буфер бленда
    render::MorphState weights_{};
    float height_m_ = CHARGEN_BODY_HEIGHT_M;
    glm::vec3 lo_{0.0f};
    glm::vec3 hi_{0.0f};
    std::filesystem::path source_path_;
    anim::Rig proportions_{};
    bool legacy_rest_ = false;
    /// Масштаб роста, с которым тело собрано последним settle(): apply()
    /// масштабирует вершины им же, чтобы меш не ушёл от костей до отпускания.
    float settled_scale_ = 1.0f;

    SkinnedCharacter character_{};
    CharacterBodies bodies_{};

    /// Габарит рест-позы — с тела, как оно есть сейчас.
    void measure_bounds();
};

/// ЗАПИСЬ И ЧТЕНИЕ ПРЕСЕТА. Отдельно от класса, потому что читать пресет
/// умеет и тот, у кого тела нет: приложение спрашивает «а есть ли уже
/// персонаж» до всякой видеокарты.
[[nodiscard]] bool write_chargen_preset(const std::filesystem::path& out,
                                        const CharGenPreset& preset);
[[nodiscard]] bool read_chargen_preset(const std::filesystem::path& in,
                                       CharGenPreset& out);

} // namespace dfn::app
