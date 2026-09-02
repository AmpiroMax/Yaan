/*
Module: engine/app
File: engine/app/sources/CharacterTextures.h

Responsibility:
- ЛИСТЫ ПЕРСОНАЖА С ДИСКА НА GPU: секция TEX объекта (ссылка + SHA-256) →
  PNG прочитан (PngImage), sha сверен, пиксели зарегистрированы у render с
  ЦЕПОЧКОЙ МИПОВ, и один и тот же файл поднимается ОДИН РАЗ на процесс —
  тело игрока, тело экрана создания и экспонат смотровой делят один лист.

Key items:
- body_albedo_asset(): номер ассета альбедо для SkinnedDraw::texture_asset,
  0 — «листа нет» (палитра вершин), и каждая причина нуля сказана вслух.
- body_palette_door(): DFN_BODY_PALETTE=1 — запасная рука: лист не
  поднимается вовсе, тело красится палитрой частей, кадр бит-в-бит прежний
  (правило 47: обе руки из одного бинарника).
- DFN_BODY_MIPS=0 — лист поднимается ОДНОЙ ступенью: контрольная рука замера
  мерцания и выбора ступени по дистанции.

Dependencies:
- Uses: engine/render ObjectRegistry (TextureRef), RenderSystem
  (register_texture_asset), PngImage, core Sha256, AppDoors.
- Used by: SkinnedCharacter (загрузка), tests/app.

Notes:
- КЭШ ПО SHA, А НЕ ПО ПУТИ, потому что личность листа — его байты: два тела,
  сославшиеся на один PNG под разными относительными путями (выпечка экрана
  лежит в другом каталоге), обязаны получить один и тот же GPU-ресурс, а
  перерисованный PNG под старым путём — другой.
- ЛИСТЫ ЖИВУТ ДО КОНЦА ПРОЦЕССА. Их единицы (2K RGBA с мипами ≈ 21 МБ на
  лист), а хозяев у одного листа несколько с разной длиной жизни; счётчик
  ссылок ради экономии одного листа между входами в меню — механизм дороже
  того, что он бережёт.
- ПУТЬ ИЗ СЕКЦИИ — ОТ КОРНЯ ДЕРЕВА. Игра запускается из корня, поэтому первым
  пробуется он (текущий каталог); затем — вверх от каталога самого .dfo, на
  случай запуска прибора из другого места. Абсолютный путь берётся как есть
  (фикстура вне дерева).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. Зона app (lead).
- Несовпадение sha — ОТКАЗ листа вслух, не «загрузить что есть»: файл,
  которого выпечка не видела, — не та кожа, и тело честно рисуется палитрой.
*/

#pragma once

#include "engine/platform/render/interfaces/IRenderer.h"
#include "engine/render/sources/ObjectRegistry.h"
#include "engine/render/sources/RenderSystem.h"

#include <cstdint>
#include <filesystem>

namespace dfn::app {

/// Запасная рука: DFN_BODY_PALETTE=1 — листы не поднимаются, тело красится
/// палитрой частей (цвет вершин), как до волны.
[[nodiscard]] bool body_palette_door();

/// Альбедо тела объекта как номер ассета render, или 0. `dfo_path` — откуда
/// объект прочитан (для поиска относительного пути листа); пустой — только
/// от текущего каталога. Повторный вызов с тем же sha — тот же номер.
[[nodiscard]] uint32_t body_albedo_asset(render::RenderSystem& render_system,
                                         platform::IRenderer& renderer,
                                         const render::RegistryObject& object,
                                         const std::filesystem::path& dfo_path);

/// Сколько листов поднято на GPU за процесс (прибор: «экран и мир делят
/// один лист» — число не растёт со вторым телом).
[[nodiscard]] std::size_t body_textures_loaded();

} // namespace dfn::app
