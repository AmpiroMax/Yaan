/*
Created: 18:08:2026 - 18:11:12
Last updated: 18:08:2026 - 18:11:12
Module: engine/app
File: engine/app/sources/AppInternal.h

Responsibility:
- ОБЩЕЕ МЕЖДУ ФАЙЛАМИ РЕАЛИЗАЦИИ App. Ничего наружу зоны: заголовок включают
  только .cpp самого App.

Key items:
- SCENE_TILE_M / SCENE_TILE_KEY_BASE: сетка тайлов композиции.
- ChunkPhysics / g_chunk_physics / pack_coord: состояние переправы чанков.

Dependencies:
- Uses: platform/physics (дескриптор тела), glm.
- Used by: App.cpp, AppWorld.cpp.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- ЭТО НЕ ПУБЛИЧНЫЙ ЗАГОЛОВОК. Он появился ровно потому, что реализацию класса
  разложили по нескольким .cpp (заказ 18.08: «app.h нормальный по размерам,
  может по разным cpp файлам реализацию класс раскидаем?») — и то, что раньше
  пряталось в безымянном пространстве одного файла, стало нужно двоим.
  Класть это в App.h нельзя: заголовок класса остаётся лёгким, а сюда попадает
  только то, чего требует РАЗРЕЗ, и ничего больше.
- ЕСЛИ СЮДА ХОЧЕТСЯ ДОБАВИТЬ ТРЕТЬЕ — сначала спроси, не значит ли это, что
  разрез прошёл не по шву. Общий заголовок, растущий сам собой, — это тот же
  один большой файл, только с лишним шагом.
*/
/*
UPD:
- 18:08:2026 - 18:11:12: Создан при выносе enter_world в AppWorld.cpp. Пять имён, все были в
  безымянном пространстве App.cpp и понадобились обоим файлам.
*/

#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include <glm/vec2.hpp>

#include "engine/platform/physics/interfaces/IPhysics.h"

namespace dfn::app {

/// СЕТКА ТАЙЛОВ КОМПОЗИЦИИ. Названа один раз и используется загрузчиком и рукой
/// строителя: правка перепекает ОДИН тайл, и найти его она может только если
/// делит мир так же, как делил загрузчик.
inline constexpr float SCENE_TILE_M = 32.0f;
inline constexpr int SCENE_TILE_KEY_BASE = 1000;

/// Состояние физики чанка, которым владеет переправа: TerrainDesc не обещает,
/// что бэкенд скопирует высоты, поэтому буфер живёт столько же, сколько тело.
struct ChunkPhysics {
    std::vector<float> heights;
    platform::PhysicsBodyHandle body{};
};

inline std::uint64_t pack_coord(glm::ivec2 c) {
    return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.x)) << 32)
         | static_cast<std::uint64_t>(static_cast<std::uint32_t>(c.y));
}

/// Состояние переправы. Живёт здесь, а не в заголовке класса, чтобы App.h
/// оставался лёгким.
inline std::unordered_map<std::uint64_t, ChunkPhysics> g_chunk_physics;

} // namespace dfn::app
