/*
Module: engine/app
File: engine/app/sources/ModelViewer.h

Responsibility:
- THE VIEWING STAND'S MODEL: what can be shown (the three sources of models on
  this tree), which one is showing, and where the orbiting eye stands to frame
  it. Data in / data out, so the whole mode is testable without a window.

Key items:
- ViewerSource / ViewerItem: one showable model and where it came from.
- scan_viewer_items(): the shelves (.dfo), the character glTF, and everything
  downloaded under artifacts/3D.
- ViewerView / viewer_reset() / viewer_orbit() / viewer_zoom(): the orbiting
  eye, and the one key that puts it back.
- viewer_fit_distance(): how far the eye must stand for the whole bound to fit.
- viewer_display_scale(): the factor a DOWNLOADED model is drawn at; our own
  shelves and characters are always 1.0, because they are already in metres.
- viewer_step_index(): next / previous, and the same by CATEGORY.

Dependencies:
- Uses: glm, std (filesystem, string). NOT engine/render: the measurements
  arrive as plain lo/hi corners, so a test can pose a bound in two lines.
- Used by: engine/app (AppViewer.cpp), tests/app/ModelViewerTests.cpp.

Notes:
- WHY THE SIZE IS AN ARGUMENT AND NOT A READ. The extent of a model is measured
  by render::measure_object, which is the ONE ruler of this repo (Rule 32).
  Taking it as a parameter here is what keeps a second ruler from growing in
  the viewer, and it is also what lets the framing be checked against a bound
  nobody had to bake first.
- THE DISPLAY SCALE IS A LAST RESORT, NOT A LOOK, and WHO wrote the file
  decides it — not how big the file is. Everything our own pipeline bakes is in
  metres and is drawn at 1.0, so the caption states a real size the owner can
  argue with. Only a DOWNLOAD, whose units are unknown by definition, is scaled,
  and the caption prints the raw measurement beside the factor.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Pure functions, no clock and no window (Rule 8): the time and the frame's
  aspect arrive as parameters. Two runs of one door must frame identically.
- User-facing labels are localization keys resolved by the caller (Rule 5).
  Nothing here returns a sentence; it returns names, numbers and key hashes.
*/

#pragma once

#include <glm/vec3.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace dfn::app {

/// WHICH MAP IS THE VIEWING STAND. Named once, read by the app when a map
/// opens and by the door that opens it, so "am I in the viewer" has exactly one
/// answer and moving the file is a one-line change rather than a hunt.
inline constexpr const char* VIEWER_MAP_CATEGORY = "stands";
inline constexpr const char* VIEWER_MAP_STEM = "viewer";

/// WHERE A SHOWABLE MODEL CAME FROM. Three, because the three answer three
/// different questions the owner asked in one sentence — "что накачал и что
/// генерируешь": Shelf is what our forges bake, Character is the imported rig
/// as it arrived, Downloaded is the raw file somebody fetched.
enum class ViewerSource : std::uint8_t {
    Shelf = 0,   ///< assets/objects/**.dfo — baked by our own forges
    Character,   ///< assets/objects/characters/*.glb|gltf — the imported rig
    Downloaded,  ///< artifacts/3D/** — .stl / .glb somebody downloaded
    Count,
};

/// ONE LINE OF THE VIEWER'S LIST.
struct ViewerItem {
    std::string name;      ///< the file stem, shown on screen
    std::string category;  ///< the folder it lives in ("trees", "gallery/kaykit-knight")
    /// WHAT THE FILE SAYS ABOUT ITSELF: the first meaningful line of a
    /// SOURCE.txt beside it, or the path when there is none. Provenance is
    /// CONTENT (Rule 5) — it is read from the tree, never written here.
    std::string origin;
    std::string path;      ///< the file to open
    ViewerSource source = ViewerSource::Shelf;

    [[nodiscard]] bool needs_conversion() const {
        return source != ViewerSource::Shelf;
    }
};

/// WHERE THE THREE SOURCES LIVE. Parameters rather than constants so a test
/// can point them at a two-file fixture and still exercise the real scan.
struct ViewerRoots {
    std::string shelves = "assets/objects";
    std::string characters = "assets/objects/characters";
    std::string downloads = "artifacts/3D";
};

/// EVERYTHING SHOWABLE ON THIS TREE, ordered source → category → name, so the
/// arrow keys walk a stable list and a category is contiguous inside it.
///
/// A `-far` twin is the SAME model made cheaper and is never a second line
/// (the gallery learned this the hard way). A missing root is not an error: a
/// checkout with no artifacts/3D still opens the stand, with fewer lines.
[[nodiscard]] std::vector<ViewerItem> scan_viewer_items(const ViewerRoots& roots);

/// THE ORBITING EYE. Angles are the frame's own convention (docs/RIG.md):
/// yaw 0 looks along -Z, +yaw turns right, +pitch looks up.
struct ViewerView {
    float orbit_yaw = 0.0f;     ///< where the eye stands around the model, rad
    float orbit_pitch = 0.0f;   ///< how far above it, rad (negative = above)
    float dist_m = 3.0f;        ///< eye distance from the look-at point
    float fit_dist_m = 3.0f;    ///< what R returns the distance to
    float target_y = 1.0f;      ///< look-at height above the pad, m
    float model_yaw = 0.0f;     ///< the model's own turn (Q/E), rad
};

/// THE PORTRAIT POSE, and it is a pose rather than a preference: every frame
/// of this stand is shot from it, so two runs of one door are comparable.
/// Three quarters from the front-left, a little above the middle — the pose a
/// figure is read from, and the same one the character stand calls its own.
inline constexpr float VIEWER_START_YAW = 0.6f;
inline constexpr float VIEWER_START_PITCH = -0.18f;

/// HOW FAR THE EYE MAY GO. Both ends are the model's own size times a factor,
/// not metres: a 3 cm cup and a 40 m colossus need the same freedom, and a
/// fixed metre range would let one be unreachable and the other unleaveable.
inline constexpr float VIEWER_ZOOM_MIN_FACTOR = 0.25f;
inline constexpr float VIEWER_ZOOM_MAX_FACTOR = 4.0f;

/// ONE NOTCH OF THE WHEEL, as a fraction of the current distance. Multiplicative
/// and not additive: a step of 20 cm is nothing at 40 m and a jump at 30 cm.
inline constexpr float VIEWER_ZOOM_STEP = 0.12f;

/// AIR AROUND THE MODEL. Above one on purpose — a bound that exactly touches the
/// frustum touches it, and a crown clipped by one pixel reads as a defect in the
/// model rather than in the framing. A tenth is enough BECAUSE the fit below is
/// exact; it was 1.25 while the fit used a bounding sphere, and had to be,
/// because the sphere of a 17 m tree is 29 m across.
inline constexpr float VIEWER_FIT_MARGIN = 1.10f;

/// HOW TALL A MODEL OF UNKNOWN UNITS IS DRAWN, metres. A person's height,
/// because most of what gets downloaded is a figure and because it is the one
/// size a viewer already has a sense of.
inline constexpr float VIEWER_SCALE_TARGET_M = 1.8f;

/// THE FACTOR THE MODEL IS DRAWN AT, and it is decided by WHERE THE FILE CAME
/// FROM rather than by how big it is.
///
/// EXACTLY 1.0 FOR OUR OWN PIPELINE — every shelf .dfo and every imported
/// character. Both are in metres by construction (the forges build in metres;
/// tools/import_gltf.cpp BAKES metres and facing, and says so in its header),
/// so a size band would have been a rule that could only ever fire wrongly: it
/// fired on the 17.6 m oak of the tree shelf the first time this ran, and drew
/// a full-grown oak at the height of a man.
///
/// A QUOTIENT FOR A DOWNLOAD, because a downloaded file's units are UNKNOWN by
/// definition — STL has no unit field at all, and this tree's own downloads are
/// in millimetres (a figure measures 31..46 across its tallest side). Not a
/// guessed millimetre factor either: the tallest side is put at
/// VIEWER_SCALE_TARGET_M, which is a quotient of two measurements and needs no
/// assumption about what the file thought it was saying. The caption prints the
/// RAW measurement beside the factor, so nothing about the file is hidden.
[[nodiscard]] float viewer_display_scale(ViewerSource source, const glm::vec3& lo,
                                         const glm::vec3& hi);

/// HOW FAR THE EYE MUST STAND FOR THE WHOLE BOUND TO FIT, metres.
///
/// THE BOUND IS A CYLINDER ABOUT +Y, not a sphere, and that is the difference
/// between a model that fills the frame and one that sits in the middle of it.
/// A sphere is rotation-invariant in every direction, which is more than this
/// mode ever needs: Q/E and the orbit both turn about Y ONLY, so a cylinder is
/// already invariant under everything that can happen — and it is far tighter.
/// Measured on the tree shelf: the 16.7 x 17.6 x 15.8 m oak has a 29 m bounding
/// sphere and a 23 m cylinder, and the sphere framing put the tree at a third of
/// the frame's height with the rest sky.
///
/// A BOX WOULD BE TIGHTER STILL AND IS REFUSED: its projected size changes as it
/// turns, so the distance would breathe every time Q/E was pressed, which reads
/// as the model pumping toward the camera.
///
/// `aspect` is width/height; both fields are checked and the harder one wins.
[[nodiscard]] float viewer_fit_distance(const glm::vec3& lo, const glm::vec3& hi,
                                        float fov_y, float aspect);

/// THE VIEW A FRESHLY SHOWN MODEL (and the R key) gets.
[[nodiscard]] ViewerView viewer_reset(ViewerSource source, const glm::vec3& lo,
                                      const glm::vec3& hi, float fov_y, float aspect);

/// MOUSE MOTION INTO THE ORBIT, pixels in, radians on the view. Pitch is
/// clamped just short of the poles: a boom exactly overhead has no azimuth and
/// the frame rolls.
void viewer_orbit(ViewerView& view, float dx_px, float dy_px, float sensitivity);

/// WHEEL NOTCHES INTO THE DISTANCE, clamped to the model's own band.
void viewer_zoom(ViewerView& view, float notches);

/// WHERE THE EYE IS, given the point it looks at.
[[nodiscard]] glm::vec3 viewer_eye(const glm::vec3& target, const ViewerView& view);

/// WHAT THE EYE LOOKS AT: the middle of the bound, standing on the pad.
[[nodiscard]] glm::vec3 viewer_target(const glm::vec3& pad_origin, const glm::vec3& lo,
                                      const glm::vec3& hi, float scale);

/// NEXT / PREVIOUS. `step` is +1 or -1; `by_category` jumps to the first item
/// of the neighbouring category instead — the shelf of building parts is 2411
/// lines long, and walking it one arrow at a time is not walking it.
///
/// WRAPS, and that is the whole reason it is a function: the first draft
/// clamped, and the last of 2610 models then had no way back to the first
/// except 2609 presses.
[[nodiscard]] int viewer_step_index(const std::vector<ViewerItem>& items, int index,
                                    int step, bool by_category);

/// THE FIRST ITEM WHOSE NAME MATCHES (exactly, then as a suffix of the path):
/// the DFN_VIEWER_ITEM door. -1 when nothing matches, and the caller is
/// expected to say so out loud rather than show item 0 — a frame that is
/// plausible and not the one asked for is the failure a named door exists to
/// avoid.
[[nodiscard]] int viewer_find_item(const std::vector<ViewerItem>& items,
                                   const std::string& name);

/// HOW MANY ITEMS EACH SOURCE CONTRIBUTED. Reported at load so a checkout with
/// no artifacts/3D says so in one line instead of being discovered by a person
/// pressing the arrow key 2600 times.
struct ViewerTally {
    int shelf = 0;
    int character = 0;
    int downloaded = 0;
    [[nodiscard]] int total() const { return shelf + character + downloaded; }
};
[[nodiscard]] ViewerTally viewer_tally(const std::vector<ViewerItem>& items);

/// THE SIZE LINE'S NUMBERS, in metres of the RAW model (before the display
/// scale): width, height, depth. Kept as a struct rather than formatted here
/// because the units word is a localization key and belongs to the caller.
struct ViewerSize {
    float width_m = 0.0f;
    float height_m = 0.0f;
    float depth_m = 0.0f;
    float scale = 1.0f; ///< what it is drawn at; 1.0 = its own size
};
[[nodiscard]] ViewerSize viewer_size(ViewerSource source, const glm::vec3& lo,
                                    const glm::vec3& hi);

} // namespace dfn::app
