
# engine/physics

## Responsibility

Engine-level physics on top of the `IPhysics` interface: the meaning of the
opaque `CollisionMask` bits, and the bridge from world heightmap data
(`math::HeightFieldView`, raw uint16 + scale/offset) to physics terrain bodies.
Never includes backend headers (Rule 1).

## Key types

- `sources/CollisionLayers.h` — `LAYER_STATIC` (terrain, buildings, prefabs),
  `LAYER_CHARACTER` (player + NPCs), `LAYER_INTERACTABLE`, `LAYER_LOOSE`,
  `LAYER_HITBOX`, `LAYER_FOOT` (physical feet), `LAYER_RAGDOLL`. Backends
  store & AND these bits blindly; extending the set is an engine-side change
  only.
- `sources/TerrainCollision.h`:
  - `create_terrain_mesh_body(physics, voxel_mesh_view, user_data)` — THE
    terrain call for the voxel world: one static mesh body per chunk on
    `LAYER_STATIC`, built from core's extracted surface triangles. Represents
    tunnels/caves/overhangs. An invalid handle means "chunk has no triangles",
    not an error. Collision uses the render-resolution extraction (VOXEL_SIZE
    1 m); see the measurement note below.
  - `create_terrain_body(physics, height_view, user_data, scratch)` — legacy
    heightmap path: decodes the frozen formula (`height = offset + raw*scale`)
    into reused scratch and creates a heightfield-derived body. CANNOT
    represent overhangs; kept for heightfield worlds and tests.

## Usage example

```cpp
// Voxel world (current): on ChunkLoaded
if (const auto mesh = chunks.voxel_mesh(coord)) {
    auto body = dfn::physics::create_terrain_mesh_body(physics, *mesh,
                                                       terrain_entity.packed());
    if (body.valid()) { /* store per coord */ }  // invalid = empty chunk, skip
}
// on ChunkUnloaded: physics.destroy_body(stored_body);
```

Measured collision cost (seed 1 testbed, load radius 2, 12 resident chunks):
~143k triangles per chunk, ~68 ms of Jolt `MeshShape` build per chunk (813 ms
for the initial 12). Correctness first — a ~2 m coarse extraction would cut
that ~4x but risks the tunnel's 3.24 m minimum headroom against a 1.8 m capsule
and 0.35 m step, and no coarse extraction API exists yet. Revisit next stage
with core (coarse extraction and/or off-thread shape building).

## Dependencies

- Uses: `engine/core/math` (HeightFieldView), `engine/platform/physics`
  interface, `dfn_headers`.
- Used by: `engine/gameplay` (layer constants for characters/raycasts),
  chunk-load handlers in `engine/app`, jolt-backed tests.
- Planned here (stage 2+): full `CharacterController` for NPCs, layer bits for
  interactables/projectiles (via sync).

## Стопы (контракт платформы, docs/design/LOCOMOTION_GROUNDED.md §11–§12)

Заказ владельца 04.09: замок стопы уходит; стопа — физическое тело на
физической земле с трением. Контракт в `IPhysics.h`, реализации — Jolt и
null; приборы — `tests/sim/FootPhysicsTests.cpp` (`sim_foot_physics`).

**Тело стопы.** `create_foot_body(FootBodyDesc)`: коробка по полуразмерам
хитбокса стопы (x поперёк, y полтолщины подошвы, z вдоль), масса
`FOOT_BODY_MASS_KG` — нагрузка, которую несёт поставленная стопа, а не масса
стопы (закон трения массу сокращает, прибор это проверяет: 1 кг и 40 кг
ползут одинаково), вещество подошвы ПО ИМЕНИ (`core::find_substance("leather")`),
слой `LAYER_FOOT`, `collides_with` — мир (`LAYER_STATIC | LAYER_LOOSE |
LAYER_INTERACTABLE`). Стопа НЕ трогает капсулу владельца и хитбоксы: капсула
отвечает «куда можно пройти», и стопа внутри неё была бы стеной, которую
ходок носит с собой; хитбоксы остаются на слое Jolt STATIC и отсекаются
маской (`MaskContactListener` в бэкенде). Хэндл — обычное тело:
`body_pose()`, `body_velocity()`, `destroy_body()`.

**Два режима.** `Swing` — кинематическое: `set_foot_kinematic_pose(foot,
pose)` говорит, где стопа будет к КОНЦУ следующего `step()`; тело едет туда
скоростью, то есть кружку на пути толкает, а не пропускает. Без новой позы —
стоит. `Plant` — динамическое: вращение заперто (ориентация — дело
анимации, стопа на склоне ползёт блоком, а не кувыркается), тяжесть прижимает
к земле, трение пары `sqrt(μ_подошвы·μ_земли)` (правило названо один раз в
`PhysicsSubstance.h`) держит или отпускает — решает решатель, а не особый
случай. При переходе в `Plant` остаточная скорость маха сбрасывается:
анимация сказала «вниз», стопа, которая приземлилась, не брошена. В `Plant`
`set_foot_kinematic_pose` игнорируется: физика владеет стопой; поднять —
сначала `Swing`.

**Контакт.** `foot_contact(foot)` после `step()`: форма стопы против всего,
что она может трогать, плюс `FOOT_BODY_SKIN_M` воздуха (решатель держит
покоящееся тело на нуле проникновения ± остаток, и без зазора «стоит»
мигало бы). Самый глубокий контакт — земля. Поля: `touching`, `point`,
`normal` (наружу от земли), `depth`, `ground` (вещество), `ground_user_data`
(чьё тело), `slope_tan`, `friction_pair`, `holds` (критерий Кулона
`tan θ ≤ friction_pair` — предсказание, чтобы anim знал до постановки),
`slip_velocity` / `slip_speed_mps` (факт: скорость стопы относительно земли
вдоль неё).

**Числа** (Jolt, кожа по граниту пара 0.648 / порог 32.9°; по стеклу 0.387 /
21.2°): плоскость — 0 скольжения; 5° гранит — 0.000 мм за 2 с; 45° гранит —
2.44 м/с; 30° стекло — 1.642 м/с² по пути и 1.615 по скорости при законе
1.615; развёртка по 0.5° — держит до 32.9°, ползёт с 33.4°; стопа на ящике —
вещество и хозяин ящика; цена двух стоп за тик — 0.0003 мс разностью против
руки с ходящей капсулой, два запроса контакта 0.0026 мс.

**Null.** Стопа стоит на плоскости, по которой скользит капсула: касание,
нормаль +Y, держит, скольжение 0; мах приходит за шаг; планта держит место.

**Что делает anim/app (сессия 62).** Каждый тик маха —
`set_foot_kinematic_pose` от клипа; на постановке — `set_foot_mode(Plant)`;
IK ноги — к `body_pose(foot)`; корень — от поставленной стопы; подъём —
`Swing`. Замок как код уходит.

## Толчки капсулы и регдолл (HIT_REACTIONS_PHYSICS.md §3)

- `CharacterDesc.mass_kg` (0 → `CHARACTER_MASS_KG`); `character_mass()`.
- `character_add_impulse(c, J)`: Δv = J/m в скорость капсулы; гаснет на земле
  за `CHARACTER_PUSH_DECAY_S`, в полёте нет; `character_velocity()` — скорость
  после шага. Иммунитет — масса и ничего больше (800 кг → десятая доля Δv).
- `character_contacts(c)` — контакты последнего шага: тело (`user_data`),
  точка, нормаль (в капсулу), относительная скорость, масса (+inf у статики),
  `pushed_character` / `pushed_body` по массе против `CHARACTER_PUSH_MASS_KG`:
  легче — капсула его двигает, оно её нет; тяжелее — наоборот. Прибор: ящик
  1 кг на 3 м/с не двигает капсулу (0.000 м) и отлетает; 200 кг двигает на
  0.50 м.
- Регдолл: `create_ragdoll(RagdollDesc)` — части (коробка/сфера, масса,
  вещество, поза мира) и сустав к родителю (точка, ось скрутки, ось
  плоскости, конус, пределы скрутки), родитель с ребёнком не сталкиваются;
  `set_ragdoll_pose` / `ragdoll_pose` (поза ↔ скелет), `ragdoll_add_impulse`
  (часть, J, точка), `ragdoll_drive_to_pose(target, strength)` — моторы
  суставов, `strength·RAGDOLL_MOTOR_TORQUE_NM`, пружина `RAGDOLL_MOTOR_HZ`;
  `ragdoll_asleep`. Прибор: цепочка таз–бедро–голень пассивная падает и
  засыпает за 2.2 с; с моторами держит позу (в падении ≤ 3.8°, после 5 Н·с
  ≤ 3.4°), без моторов лежит сложенной (60° / 29°).
- Хвосты: обмен импульсом при ударе оружием и стойкость — gameplay;
  «болванчик» на стенде — регдолл, подвешенный за макушку, — конструкции
  подвеса в контракте ещё нет (следующая порция); чтение позы регдолла в
  скелет — anim (кости ← `ragdoll_pose`).
