/*
Module: engine/app
File: engine/app/sources/ModelConvert.h

Responsibility:
- TURNS A DOWNLOADED FILE INTO SOMETHING THE GAME CAN ALREADY DRAW: a .stl is
  read straight into the registry's own object format, a .glb/.gltf is handed
  to the offline importer we already own. Both land in a CACHE beside the
  downloads, so the second look at a model costs a file open.

Key items:
- read_stl(): binary and ASCII STL -> render::MeshData, flat-shaded.
- ConvertResult / convert_model(): path in, a .dfo path (or a loud reason) out.
- model_cache_dir(): artifacts/3D/.cache — our OUTPUT, never a source.
- cache_name_for(): the cache file's name, derived from path + size + mtime.

Dependencies:
- Uses: engine/render (MeshData, ObjectRegistry), std. No window.
- Used by: engine/app (AppViewer.cpp), tests/app/ModelConvertTests.cpp.

Notes:
- WHY STL IS READ HERE AND glTF IS NOT. STL is 84 bytes of header and 50 bytes
  per triangle — no parser, no library, no third-party header, and therefore no
  breach of Rule 1 in a runtime layer. glTF is a real format with a real parser,
  and this repo already decided where that parser lives: OFFLINE, in
  tools/import_gltf.cpp, so that "the engine's only input is the .dfo". This
  file honours that decision instead of quietly making a second glTF reader —
  it runs the tool and caches what the tool wrote.
- THE ORDER ASKED FOR BLENDER AND BLENDER IS NOT INSTALLED (`which blender` is
  empty on this machine, as docs/research/CHARACTER_PIPELINE.md §4.2 already
  recorded). Reading STL directly is strictly better than shelling out anyway:
  it is deterministic, it needs no external install, and it cannot half-succeed
  in a way that leaves a plausible wrong model on the stand. The glTF half is
  the one that genuinely needs a converter, and it uses OUR converter.
- THE CACHE KEY CARRIES SIZE AND MTIME, not just the name. A wave is filling
  artifacts/3D while this one runs; a cache keyed by name alone would show
  yesterday's figure under today's file for ever, and nothing would say so.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- FAIL SOFT AND LOUD. A refused file returns an empty path and a reason; it
  never returns half a model. A viewer that shows a plausible wrong mesh is
  worse than one that says it could not read the file.
*/

#pragma once

#include "engine/render/sources/ProcMesh.h"

#include <filesystem>
#include <optional>
#include <string>

namespace dfn::app {

/// Reads an STL (binary or ASCII) into flat-shaded triangles in the file's own
/// units AND ITS OWN AXES. nullopt on a truncated or unreadable file — never a
/// partial mesh.
///
/// THE AXES ARE THE FILE'S, NOT OURS, and that is deliberate: this function is
/// a FORMAT READER, and a reader that also re-oriented could not be used to
/// find out what a file actually contains. The turn into our frame happens once,
/// at the bake (stl_z_up_to_y_up, called by convert_model), the same place
/// tools/import_gltf.cpp bakes metres and facing.
///
/// NORMALS ARE RECOMPUTED FROM THE WINDING and the facet normal in the file is
/// ignored: printable models are full of zero and denormalised facet normals,
/// and a lit frame made of them reads as holes in the model rather than as a
/// bad file.
[[nodiscard]] std::optional<render::MeshData> read_stl(const std::filesystem::path& path);

/// Where converted models are kept. Under the downloads root, hidden by name,
/// because it is OUTPUT: the scan skips it explicitly (ModelViewer.cpp) so a
/// converted figure is never listed twice.
[[nodiscard]] std::filesystem::path model_cache_dir(const std::filesystem::path& downloads_root);

/// TURNS A PRINTABLE MODEL UPRIGHT: (x, y, z) -> (x, z, -y), a -90 degree turn
/// about X, normals with it. In place.
///
/// WHY THIS IS A CONVENTION AND NOT A GUESS. STL has no axis field at all, and
/// every printing pipeline that writes one — the slicer, the printer, the model
/// sites these files come from — is Z-UP, because the build plate is the XY
/// plane. Our frame is Y-up (docs/REFERENCE_FRAMES.md). Measured on this tree's
/// own downloads: every figure in artifacts/3D/gallery is tallest along Z
/// (31..46 units against 16..45 across), i.e. standing in ITS frame and lying
/// on its back in ours. Without this turn the whole stand shows corpses.
void stl_z_up_to_y_up(render::MeshData& mesh);

/// The cache file name for a source file: stem plus a hash of its absolute
/// path, byte size and write time. Stable across runs, different the moment
/// the file changes.
[[nodiscard]] std::string cache_name_for(const std::filesystem::path& path);

struct ConvertResult {
    std::filesystem::path dfo;   ///< empty on failure
    std::string reason;          ///< empty on success; a sentence otherwise
    bool from_cache = false;     ///< true when nothing had to be converted
    std::size_t triangles = 0;
};

/// Makes a .dfo out of `path` if there is not one already. `.dfo` in, `.dfo`
/// out unchanged (no copy, no cache entry): the shelves need no conversion and
/// pretending otherwise would double every read.
///
/// `gltf_tool` is the path to dfn_import_gltf; empty (or missing) means glTF
/// cannot be converted on this machine, which is reported as a reason rather
/// than as a crash.
[[nodiscard]] ConvertResult convert_model(const std::filesystem::path& path,
                                          const std::filesystem::path& downloads_root,
                                          const std::filesystem::path& gltf_tool);

/// WHERE dfn_import_gltf IS, if it is anywhere. Looked for beside the running
/// binary first (that is where a normal build puts it), then in the build
/// directories this repo conventionally uses, then in DFN_GLTF_TOOL. Empty
/// when there is none — the caller says so on screen instead of guessing.
[[nodiscard]] std::filesystem::path find_gltf_tool(const std::filesystem::path& exe_dir);

} // namespace dfn::app
