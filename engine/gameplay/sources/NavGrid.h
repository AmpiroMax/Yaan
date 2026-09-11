/*
Module: engine/gameplay
File: engine/gameplay/sources/NavGrid.h

Responsibility:
- СЕТКА ПРОХОДИМОСТИ ДЛЯ НПС (docs/design/NPC_NAVIGATION.md §2–3): столбцы по
  ячейкам, в столбце — этажи (верх твёрдого с просветом под рост и наклоном
  под капсулу), связь соседей по высоте шага, эрозия на радиус капсулы;
  A* по этажам и натяжение пути. Чистые функции над plain data (правила
  8/9), без физики: сетка строится из ТЕХ ЖЕ треугольников, что уходят в
  коллайдер, и потому идёт в тестах без окна и без Jolt.

Key items:
- NavInput / NavMeshSoup / NavBox / NavAgent: вход постройки — супы
  треугольников (дома, детали, воксели), ящики (стволы, предметы), функция
  высоты рельефа, охват, агент (радиус, рост, шаг, наклон — по именам
  реестра).
- NavGrid / NavColumn / NavFloor: результат — столбцы и этажи; флаги этажа:
  clear (просвет под рост), walkable (после эрозии).
- nav_build(): растеризация (треугольник режется по ячейке — стена нулевой
  площади в плане тоже даёт пролёт), слияние пролётов, этажи, эрозия
  двухпроходной фаской по связному графу (как erodeWalkableArea в Recast).
- nav_locate(): ближайший проходимый этаж к точке в пределах NAV_SNAP_M.
- nav_find_path(): A* (8 соседей, без среза углов) + натяжение по линии
  в той же сетке; NavSearch — скрэтч одного поиска, переиспользуется.
- nav_line_walkable(): отрезок проходим — каждая ячейка по нему связана
  с прежней по шагу и проходима.
- nav_geometry_hash() / NavStats: число треугольников и хэш входа — прибор
  «сетка и коллайдер собраны из одного» (условие координатора 1).

Dependencies:
- Uses: glm, std, engine/core/config (NAV_*, PLAYER_* по именам).
- Used by: NpcAction (MoveTo по пути), NpcBehaviour, engine/app (сборка
  NavInput там, где собирается коллизия), tests/sim (sim_nav_grid,
  sim_nav_path).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Сетка НЕ спрашивает физику и НЕ строится второй сборкой геометрии рядом с
  коллайдером: NavInput — выход того же сборщика (условие 1 синка 11.09).
- Правило 12: никаких часов здесь; время постройки меряет тест.
*/
#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <glm/glm.hpp>

namespace dfn::gameplay {

/// Суп треугольников в МИРОВЫХ координатах (индексы тройками).
struct NavMeshSoup {
    std::span<const glm::vec3> positions;
    std::span<const uint32_t> indices;
};

/// Ящик с рыском вокруг вертикали (конвенция сцены: местный +X при yaw
/// уходит в (cos, −sin)).
struct NavBox {
    glm::vec3 center{0.0f};
    glm::vec3 half_extents{0.0f};
    float yaw = 0.0f;
};

/// Агент: капсула. Умолчания — из реестра (PLAYER_*), прибор подставляет
/// свои в контрольной руке («шаг 0,1 марш не берёт»).
struct NavAgent {
    float radius = 0.35f;
    float height = 1.8f;
    float step = 0.35f;
    float max_slope_rad = 0.87f;
};
[[nodiscard]] NavAgent nav_agent_from_registry();

struct NavInput {
    std::span<const NavMeshSoup> meshes;
    std::span<const NavBox> boxes;
    /// Высота рельефа; nullptr — рельефа нет (только геометрия).
    float (*ground)(void* ctx, glm::vec2 world_xz) = nullptr;
    void* ground_ctx = nullptr;
    glm::vec2 min_xz{0.0f};
    glm::vec2 max_xz{0.0f};
    NavAgent agent;
    /// Шаг ячейки и квант высоты; 0 — из реестра (NAV_CELL_M, NAV_HEIGHT_Q_M).
    float cell = 0.0f;
    float height_q = 0.0f;
};

struct NavFloor {
    int32_t top = 0;      ///< верх этажа, кванты от NavGrid::y0
    int32_t ceiling = 0;  ///< низ следующего твёрдого над ним (INT32_MAX — небо)
    bool clear = false;   ///< просвет ≥ рост агента
    bool walkable = false; ///< clear и не съеден эрозией
};

struct NavColumn {
    uint32_t first = 0; ///< индекс в NavGrid::floors
    uint16_t count = 0; ///< этажей в столбце, снизу вверх
};

struct NavStats {
    uint32_t columns = 0;
    uint32_t floors = 0;
    uint32_t walkable = 0;
    uint32_t triangles = 0; ///< треугольников на входе (супы + ящики × 12)
    uint32_t spans = 0;     ///< пролётов твёрдого в постройке (пиковая структура)
    std::size_t build_peak_bytes = 0; ///< память постройки: пролёты + узлы рельефа + результат
    uint64_t geometry_hash = 0;
};

struct NavGrid {
    glm::vec2 origin{0.0f}; ///< угол (min x, min z)
    float cell = 0.25f;
    float height_q = 0.05f;
    float y0 = 0.0f;         ///< кванты высоты считаются от него
    uint32_t nx = 0, nz = 0;
    NavAgent agent;
    std::vector<NavColumn> columns; ///< nx × nz, индекс iz·nx + ix
    std::vector<NavFloor> floors;
    NavStats stats;

    [[nodiscard]] bool valid() const { return nx > 0 && nz > 0; }
    [[nodiscard]] float top_y(const NavFloor& f) const {
        return y0 + static_cast<float>(f.top) * height_q;
    }
    [[nodiscard]] glm::vec2 cell_center(uint32_t ix, uint32_t iz) const {
        return origin + glm::vec2{(static_cast<float>(ix) + 0.5f) * cell,
                                  (static_cast<float>(iz) + 0.5f) * cell};
    }
    [[nodiscard]] std::size_t memory_bytes() const {
        return columns.capacity() * sizeof(NavColumn) + floors.capacity() * sizeof(NavFloor);
    }
};

/// Этаж в сетке: столбец и индекс этажа (в NavGrid::floors).
struct NavRef {
    uint32_t ix = 0, iz = 0;
    uint32_t floor = 0;
};

/// Хэш и число треугольников входа — до постройки, чтобы сверять с
/// коллайдером той же геометрии. FNV-1a по позициям, индексам и ящикам.
[[nodiscard]] uint64_t nav_geometry_hash(const NavInput& in);
[[nodiscard]] uint32_t nav_geometry_triangles(const NavInput& in);

/// Постройка. Ложь с текстом — охват пуст, ячейка нечисловая, сетка больше
/// NAV_COLUMNS_MAX столбцов (отказ вслух, не молчаливый обрез).
bool nav_build(const NavInput& in, NavGrid& out, std::string* err);

/// Ближайший ПРОХОДИМЫЙ этаж к точке: по высоте в пределах snap, по
/// горизонтали — кольцами до snap. nullopt — рядом ничего проходимого.
[[nodiscard]] std::optional<NavRef> nav_locate(const NavGrid& g, const glm::vec3& p, float snap_m);

/// Мировая точка этажа (центр ячейки, верх этажа).
[[nodiscard]] glm::vec3 nav_point(const NavGrid& g, const NavRef& r);

/// Соседний этаж в направлении (dx, dz) ∈ {−1,0,1}, связанный по шагу и
/// проходимый; nullopt — связи нет.
[[nodiscard]] std::optional<uint32_t> nav_neighbour(const NavGrid& g, const NavRef& from, int dx, int dz);

/// Отрезок между двумя точками проходим (для натяжения и слежения за путём).
[[nodiscard]] bool nav_line_walkable(const NavGrid& g, const glm::vec3& a, const glm::vec3& b);

/// Путь: путевые точки от старта к цели (обе точки — как даны, промежуточные —
/// углы после натяжения), `next` — какую точку ходок берёт сейчас.
struct NavPath {
    std::vector<glm::vec3> points;
    uint32_t next = 0;
    float cells_length_m = 0.0f; ///< длина пути по ячейкам ДО натяжения (прибор)
    uint32_t cells = 0;          ///< этажей в пути до натяжения (прибор)

    [[nodiscard]] float length_m() const;
};

/// Скрэтч одного поиска: массивы на все этажи, переиспользуются между
/// вызовами (стемп поколения вместо очистки).
struct NavSearch {
    std::vector<float> g;
    std::vector<uint32_t> parent;
    std::vector<uint32_t> stamp;
    std::vector<uint8_t> closed;
    uint32_t generation = 0;
    uint32_t expanded = 0; ///< раскрытых узлов в последнем поиске (прибор)
};

/// A* + натяжение. Ложь — старт или цель не у проходимого этажа (NAV_SNAP_M)
/// или пути нет. `pull` = false — путь по центрам ячеек без натяжения
/// (контрольная рука прибора).
bool nav_find_path(const NavGrid& g, NavSearch& search, const glm::vec3& from, const glm::vec3& to,
                   NavPath& out, bool pull = true);

} // namespace dfn::gameplay
