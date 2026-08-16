/*
Created: 14:08:2026 - 16:50:36
Last updated: 16:08:2026 - 21:50:43
Module: engine/app
File: engine/app/sources/MapCatalog.h

Responsibility:
- The editor/play map browser's data model: the fixed category list (folders)
  and the .map manifests found in each (docs/MAP_LAYOUT.md, lead-owned
  contract). Reads the transit `key = value` manifest; it does NOT extend the
  format (that is the contract owner's, Rule 25/26).

Key items:
- MapManifest: one parsed .map (name / zone / source / description / size).
- MapCategory: one folder (slug + its maps; may be empty).
- MapCatalog: all categories in contract order + find() by "category/stem".
- parse_map_manifest(): text -> MapManifest.
- scan_map_catalog(): walk assets/maps for the fixed categories.
- split_map_source(): "stand:Testbed" -> ("stand", "Testbed"). The app maps the
  scheme to a StandId / .dfw itself, so this header pulls in no world types.

Dependencies:
- Uses: std only (filesystem, string). No world/render/platform headers.
- Used by: engine/app (App builds it, Menu browses it).

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly. LEAD-owned file (Rule 25).
- The .map manifest format is docs/MAP_LAYOUT.md's contract: read the keys it
  documents, ignore unknown keys (forward-compatible), never invent new ones
  here.
*/
/*
UPD:
- 14:08:2026 - 16:50:36: Created -- the map browser's catalog (categories ->
                         .map manifests), per docs/MAP_LAYOUT.md.
- 15:08:2026 - 01:04:30: MapManifest::objects — карта выбирает свою полку реестра (галерея
  колосса живёт на своей карте, не тесня общую).
- 16:08:2026 - 21:08:52: MapManifest::scene — карта называет свой файл композиции.
- 16:08:2026 - 21:50:43: objects — список полок через ';', по порядку, побеждает первая.
*/

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace dfn::app {

// One parsed .map manifest. The fields are the contract's documented keys;
// name and description are CONTENT (authored in the data file, Rule 5) and are
// shown verbatim. Unknown keys are ignored on parse.
struct MapManifest {
    std::string category;    // owning folder slug (landscape, trees, ...)
    std::string file_stem;   // filename without ".map" -- the DFN_OPEN_MAP address
    std::string name;        // 'name' -- shown in the list
    std::string zone;        // 'zone' -- the owning zone
    std::string source;      // 'source' -- "stand:Testbed" | "dfw:<file>"
    std::string description; // 'description' -- shown under the name
    /// 'objects' -- registry directory the map's exhibits load from (Gallery
    /// stands). Empty = the stand's default. A MAP chooses its shelf, so a
    /// colossus can live on its own map without crowding the tree gallery.
    /// SEVERAL SHELVES, separated by ';' — "assets/objects/parts;assets/objects/trees".
    /// Searched in order, first hit wins. A town scene needs building parts,
    /// street props and flora's trees at once, and one shelf per map would
    /// force whoever composes it to copy .dfo files between directories — which
    /// is exactly how two copies of an object drift apart under one name.
    std::string objects;
    /// 'scene' -- a .scene composition file (engine/world/sources/Scene.h).
    /// When set, the map's objects stand WHERE THE FILE SAYS instead of on an
    /// auto-generated grid: the composition becomes data a human and an agent
    /// edit and dfn_scene_check judges, which is the whole point of the tool.
    /// Empty = the stand's own arrangement, as before.
    std::string scene;
    int size_chunks = 0;     // 'size_chunks'
};

// One category folder. Present even when empty: the browser shows empty
// categories rather than hiding them (contract: "Пустые категории показывай").
struct MapCategory {
    std::string slug;              // folder name, matches map_categories()
    std::vector<MapManifest> maps; // sorted by file_stem; may be empty
};

// The whole catalog, categories in the contract's fixed order.
struct MapCatalog {
    std::vector<MapCategory> categories;

    // Look up "category/stem" (the DFN_OPEN_MAP door). nullptr if not found.
    [[nodiscard]] const MapManifest* find(std::string_view category,
                                          std::string_view stem) const;
};

// The nine categories in docs/MAP_LAYOUT.md order (в40). New categories are the
// lead's to add here and on disk.
[[nodiscard]] const std::vector<std::string>& map_categories();

// Parse a manifest's `key = value` text. Lines starting with '#' and blank
// lines are ignored; unknown keys are ignored; `category` and `file_stem` are
// NOT set here (the scanner knows them from the path).
[[nodiscard]] MapManifest parse_map_manifest(std::string_view text);

// Split "scheme:value" into its two halves. Returns false if there is no ':'.
[[nodiscard]] bool split_map_source(std::string_view source, std::string& scheme,
                                    std::string& value);

// Walk `root` (e.g. "assets/maps") for every category in map_categories(),
// reading each ".map". Categories always appear, in contract order, empty or
// not. A missing root yields all-empty categories rather than an error: the
// browser must still draw.
[[nodiscard]] MapCatalog scan_map_catalog(const std::string& root);

} // namespace dfn::app
