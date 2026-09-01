/*
Module: tools
File: tools/check_human_scale.cpp

Responsibility:
- dfn_human_scale: measures an imported character's PROPORTIONS off its own
  skeleton and judges them against the canon in docs/design/HUMAN_SCALE.md.
  Prints every number it judged by; exits non-zero when any of them is outside
  the band.

Key items:
- main(): <file.dfo> [--tolerance FRAC] [--silhouette-tolerance FRAC]
          [--baseline FILE] [--write-baseline FILE] [--quiet].
- The JOINT landmarks: total height, head height (and heads-per-figure),
  shoulder / hip / knee / ankle heights, shoulder width, upper arm, forearm.
- The SILHOUETTE landmarks, measured on the rest-posed SKIN: hand length,
  fingertip height, arm span, bideltoid shoulders, hip breadth, chest width
  and depth, waist breadth and depth at the navel, upper arm and thigh
  diameters -- plus the trunk's profile in metres at five heights.
- TWO MODES, ONE TABLE. Without --baseline the verdict is the CANON's, which
  is what a CANDIDATE model is judged by. With --baseline the verdict is the
  recorded measurement of the SHIPPED body, and the canon is still measured
  and still printed -- with its deviations -- as reference.

Dependencies:
- Uses: engine/render (.dfo reader), engine/anim (bone map, RigProportions),
  engine/core (skeleton FK).
- Used by: ctest (human_scale_* rows), the character wave's report.

Notes:
- WHOSE BAND IS ON THE SHIPPED BODY (owner's decision, 01.09). The canon in
  docs/design/HUMAN_SCALE.md is what a CANDIDATE is measured against: it
  answers "could this model be our man". It is NOT what the man we already
  ship is judged by, because the owner looked at the raw HumanBase and at the
  same asset run through --fit-canon and --reshape, and kept the first. A
  judge that keeps failing the body the owner chose is not measuring the body,
  it is arguing with him -- and it goes red every day, which is the same as
  going red never. So the visible character gets a BASELINE: its own
  measurement, recorded once beside the asset, with the canon printed next to
  it.
- WHY THE BASELINE IS A FILE BESIDE THE ASSET AND NOT A SECTION INSIDE THE
  .dfo. A baseline stored in the bake would be REWRITTEN BY EVERY BAKE, so it
  could never catch the thing it exists to catch: it would move with the body
  and report agreement forever. The file is written only by an explicit
  --write-baseline, lives in git next to the .glb, and a body that drifts
  fails against the version a person last approved.
- THE BASELINE DOES NOT LOOSEN ANYTHING. It is judged with the SAME two
  tolerance knobs, so the caller says in one place how tight the band is:
  ctest's regression row asks for 2 % (a re-bake of the same asset must
  reproduce itself), and the morph wave's slider ends ask for the canon's own
  5 % / 15 % around the neutral, so a slider cannot sculpt a monster.
- THE CANON IS NOT RETYPED HERE. Every expected fraction is read from
  anim::RigProportions::from_config(), i.e. from the BODY_*_FRAC rows of
  NUMBERS.md, which docs/design/HUMAN_SCALE.md §"Наши строки против канона"
  certifies as EXACTLY the canonical values. A second copy of the table in
  this file would be the two-copies defect (Rule 35) sitting inside the very
  instrument that exists to catch proportion drift.
- IT MEASURES THE BODY, NOT THE PICTURE (Rule 47). Nothing here reads a
  frame, a camera or a light. The silhouette half measures the SKIN because a
  joint has no radius and the rig's last arm joint is the wrist: a table of
  joints alone passed this very model while it was, in the owner's words,
  "чрезмерно перекачен, слишком длинные руки, живота нет". Judging a
  silhouette off a RENDER is what the rule forbids; measuring the body that
  casts it is the opposite.
- THE SKIN HAS ITS OWN BAND (15 % against the joints' 5 %) and the reason is
  written where it is applied: the canon gives the same shoulders as 0.259
  (bone) and 0.29 (skin), so three digits of agreement is precision the source
  does not have.
- THE SKELETON IS STILL NOT THE PICTURE. Joint positions in the
  bind pose are geometry; a silhouette in a frame is a function of the camera,
  the light and the armour. The one thing taken from the MESH is the total
  height, because the crown of the head is not a joint.
- WHY A TOOL AND NOT ONLY A TEST. A test answers "did it break"; the owner's
  question on 30.08 was "what ARE the proportions" -- and that wants the table
  printed, on any file, without a rebuild of the suite.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- Loud on every refusal; the numbers are printed even when the verdict is
  green, because a judge that prints only its verdict cannot be checked.
*/

#include "engine/anim/sources/BoneMap.h"
#include "engine/anim/sources/Rig.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/serialization/sources/Json.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Landmark {
    const char* name;
    float measured;  ///< fraction of the figure's total height
    float canon;
};

[[nodiscard]] glm::vec3 joint_pos(const std::vector<glm::mat4>& model, int32_t j) {
    return j >= 0 ? glm::vec3{model[static_cast<std::size_t>(j)][3]} : glm::vec3{0.0f};
}

/// THE RECORDED MEASUREMENT OF THE SHIPPED BODY. Every row this judge prints,
/// stored under the row's own name, so a landmark that is renamed or added
/// makes the baseline STALE OUT LOUD instead of being silently skipped.
struct Baseline {
    bool loaded = false;
    std::string body;    ///< the .dfo's own name field, as recorded
    float height = 0.0f;
    float heads = 0.0f;
    std::vector<std::pair<std::string, float>> joints;
    std::vector<std::pair<std::string, float>> silhouette;

    [[nodiscard]] const float* find(bool sil, const char* name) const {
        for (const auto& r : sil ? silhouette : joints) {
            if (r.first == name) {
                return &r.second;
            }
        }
        return nullptr;
    }
};

/// Written by hand and not by json_write() on purpose: this file is READ by a
/// machine and REVIEWED by a person in a diff, so it is laid out one row per
/// line. The strict side -- the parser -- is the shared one (json_parse).
[[nodiscard]] bool write_baseline_file(const std::string& out,
                                       const std::string& source,
                                       const std::string& body, float height,
                                       float heads, const std::vector<Landmark>& joints,
                                       const std::vector<Landmark>& silhouette) {
    std::ostringstream o;
    o.setf(std::ios::fixed);
    o.precision(6);
    o << "{\n";
    o << "  \"schema\": \"dfn.body-scale-baseline\",\n";
    o << "  \"version\": 1,\n";
    o << "  \"body\": \"" << body << "\",\n";
    o << "  \"source\": \"" << source << "\",\n";
    o << "  \"decision\": \"owner 01.09: the visible HumanBase ships RAW; "
         "docs/design/HUMAN_SCALE.md stays the band for CANDIDATES\",\n";
    o << "  \"height_m\": " << height << ",\n";
    o << "  \"heads\": " << heads << ",\n";
    const auto table = [&o](const char* key, const std::vector<Landmark>& rows,
                            bool last) {
        o << "  \"" << key << "\": {\n";
        for (std::size_t i = 0; i < rows.size(); ++i) {
            o << "    \"" << rows[i].name << "\": " << rows[i].measured
              << (i + 1 == rows.size() ? "\n" : ",\n");
        }
        o << (last ? "  }\n" : "  },\n");
    };
    table("joints", joints, false);
    table("silhouette", silhouette, true);
    o << "}\n";
    std::ofstream f(out, std::ios::binary);
    if (!f) {
        return false;
    }
    const std::string text = o.str();
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
    return static_cast<bool>(f);
}

[[nodiscard]] bool read_baseline_file(const std::string& in, Baseline& out,
                                      std::string& err) {
    std::ifstream f(in, std::ios::binary);
    if (!f) {
        err = "не открывается";
        return false;
    }
    std::ostringstream buf;
    buf << f.rdbuf();
    const dfn::serialization::JsonParseResult r =
        dfn::serialization::json_parse(buf.str());
    if (!r.ok) {
        err = "строка " + std::to_string(r.error.line) + ": " + r.error.message;
        return false;
    }
    if (r.root.get("schema").as_string() != "dfn.body-scale-baseline") {
        err = "не тот файл: schema != dfn.body-scale-baseline";
        return false;
    }
    out.body = std::string{r.root.get("body").as_string()};
    out.height = static_cast<float>(r.root.get("height_m").as_number());
    out.heads = static_cast<float>(r.root.get("heads").as_number());
    const auto take = [&r](const char* key,
                           std::vector<std::pair<std::string, float>>& into) {
        const dfn::serialization::JsonValue* t = r.root.find(key);
        if (t == nullptr) {
            return;
        }
        for (const dfn::serialization::JsonMember& m : t->members()) {
            into.emplace_back(m.key, static_cast<float>(m.value.as_number()));
        }
    };
    take("joints", out.joints);
    take("silhouette", out.silhouette);
    if (out.joints.empty() || out.silhouette.empty() || out.heads < 1e-3f) {
        err = "пустой: ни одной строки замера";
        return false;
    }
    out.loaded = true;
    return true;
}

} // namespace

int main(int argc, char** argv) {
    std::string path;
    // 5 % is the owner's own band (order of 30.08: "если доли модели расходятся
    // с каноном > 5 %"). Kept as a flag so a stricter reading can be asked for
    // without editing the judge -- but never defaulted looser.
    // THE CANON'S OWN TWO BANDS, named because they are used TWICE: as the
    // default verdict band, and -- when the verdict has been handed to a
    // baseline -- as the band the canon's reference column is still marked up
    // with. Judging the reference column at the baseline's much tighter band
    // would print "the canon is missed" beside rows that sit well inside it.
    constexpr float CANON_JOINT_BAND = 0.05f;
    constexpr float CANON_SILHOUETTE_BAND = 0.15f;
    float tolerance = CANON_JOINT_BAND;
    // THE SKIN'S OWN BAND. Wider than the skeleton's on purpose and for a
    // stated reason (see the silhouette block below): the canon gives the same
    // shoulders as 0.259 and 0.29 depending on whether bone or skin is meant,
    // so three digits of agreement is precision the source does not have.
    float silhouette_tolerance = CANON_SILHOUETTE_BAND;
    bool quiet = false;
    // THE BASELINE FILE. Given, the verdict comes from it and the canon is
    // printed as reference; asked to be written, the judge records what it
    // measured and says nothing about whether it likes it.
    std::string baseline_path;
    std::string write_baseline;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--tolerance" && i + 1 < argc) {
            tolerance = std::strtof(argv[++i], nullptr);
        } else if (a == "--silhouette-tolerance" && i + 1 < argc) {
            silhouette_tolerance = std::strtof(argv[++i], nullptr);
        } else if (a == "--baseline" && i + 1 < argc) {
            baseline_path = argv[++i];
        } else if (a == "--write-baseline" && i + 1 < argc) {
            write_baseline = argv[++i];
        } else if (a == "--quiet") {
            quiet = true;
        } else if (path.empty()) {
            path = a;
        }
    }
    if (path.empty()) {
        std::fprintf(stderr, "dfn_human_scale <character.dfo> [--tolerance 0.05] "
                             "[--silhouette-tolerance 0.15] [--baseline f.json] "
                             "[--write-baseline f.json] [--quiet]\n");
        return 2;
    }
    if (!baseline_path.empty() && !write_baseline.empty()) {
        std::fprintf(stderr, "[scale] --baseline и --write-baseline вместе не "
                             "имеют смысла: файл нельзя одновременно писать и "
                             "судить по нему\n");
        return 2;
    }
    const auto obj = dfn::render::read_object(path);
    if (!obj.has_value()) {
        std::fprintf(stderr, "[scale] cannot read \"%s\"\n", path.c_str());
        return 1;
    }
    if (obj->skeleton.empty() || obj->skin.vertices.empty()) {
        std::fprintf(stderr, "[scale] \"%s\" is not a character (no SKEL/SKIN)\n",
                     path.c_str());
        return 1;
    }
    const dfn::anim::SkeletonBinding bind = dfn::anim::bind_skeleton(obj->skeleton);
    if (bind.bound_count < dfn::anim::BONE_COUNT) {
        std::fprintf(stderr,
                     "[scale] \"%s\": only %u of %u rig bones bind by name — the "
                     "proportions of an unbound skeleton cannot be judged\n",
                     path.c_str(), bind.bound_count, dfn::anim::BONE_COUNT);
        return 1;
    }

    // TOTAL HEIGHT COMES FROM THE MESH: the crown of the head is not a joint,
    // and every fraction below is a fraction OF IT.
    //
    // MEASURED ON THE REST-POSED MESH, NOT ON THE STORED BIND VERTICES, and
    // that distinction is the whole difference between an instrument and a
    // decoration. A model the importer fitted to the canon has a bind pose
    // its palette no longer maps to the identity -- the mesh is CARRIED to its
    // new proportions by the skinning. Reading the stored vertices there
    // measures the model as it arrived from its author, i.e. the one state the
    // fit was supposed to change (Rule 47: the instrument must not lose its
    // subject exactly when the effect is present). First run of this judge did
    // exactly that and reported a 25.68-head figure.
    const dfn::anim::Rig rig =
        dfn::anim::Rig::build(dfn::anim::RigProportions::from_config());
    const dfn::anim::SkinnedRigBinding skinned =
        dfn::anim::bind_skinned_rig(rig, obj->skeleton);
    std::vector<glm::mat4> palette(obj->skeleton.size());
    dfn::anim::skinning_palette(rig, obj->skeleton, skinned, dfn::anim::LocalPose{},
                                palette);
    // THE REST-POSED MESH IS KEPT, not just its extremes. Every silhouette
    // number below -- the fingertip, the deltoid, the belly -- is a property of
    // the SKIN and cannot be read off a joint; skinning the model twice to get
    // them would be the two-copies defect inside the instrument.
    std::vector<glm::vec3> rest(obj->skin.vertices.size());
    float lo = 0.0f;
    float hi = 0.0f;
    for (std::size_t i = 0; i < obj->skin.vertices.size(); ++i) {
        rest[i] = dfn::anim::cpu_skin_position(obj->skin.vertices[i], palette);
        const float y = rest[i].y;
        lo = i == 0 ? y : std::min(lo, y);
        hi = i == 0 ? y : std::max(hi, y);
    }
    const float height = hi - lo;
    if (height < 1e-3f) {
        std::fprintf(stderr, "[scale] \"%s\" has no height\n", path.c_str());
        return 1;
    }

    // JOINTS IN THE REST FRAME, for the same reason the mesh is: a T-posed
    // model's bind-pose shoulders sit where its arms were bound, not where
    // the figure stands.
    const std::size_t n = obj->skeleton.size();
    std::vector<glm::mat4> model(n);
    dfn::anim::rest_model_matrices(rig, obj->skeleton, skinned, dfn::anim::LocalPose{},
                                   model);

    using dfn::anim::Bone;
    using dfn::anim::bone_index;
    const auto at = [&](Bone b) {
        return joint_pos(model, bind.joint[bone_index(b)]);
    };
    const glm::vec3 neck = at(Bone::Head);
    const glm::vec3 shoulder_l = at(Bone::UpperArmL);
    const glm::vec3 shoulder_r = at(Bone::UpperArmR);
    const glm::vec3 elbow_l = at(Bone::ForearmL);
    const glm::vec3 wrist_l = at(Bone::HandL);
    const glm::vec3 hip = at(Bone::Pelvis);
    const glm::vec3 knee_l = at(Bone::ShinL);
    const glm::vec3 ankle_l = at(Bone::FootL);

    const dfn::anim::RigProportions canon = dfn::anim::RigProportions::from_config();
    const float H = static_cast<float>(dfn::config::PLAYER_CAPSULE_HEIGHT);
    const std::vector<Landmark> rows{
        // Heights from the SOLE (lo), as fractions of the figure's height.
        {"head height (crown - neck)", (hi - neck.y) / height, canon.head_height / H},
        {"neck height", (neck.y - lo) / height, canon.neck_height / H},
        {"shoulder height", ((shoulder_l.y + shoulder_r.y) * 0.5f - lo) / height,
         canon.shoulder_height / H},
        {"hip height", (hip.y - lo) / height, canon.hip_height / H},
        {"knee height", (knee_l.y - lo) / height, canon.knee_height / H},
        {"ankle height", (ankle_l.y - lo) / height, canon.ankle_height / H},
        // Segment lengths, joint to joint.
        {"upper arm", glm::length(elbow_l - shoulder_l) / height,
         canon.upper_arm_length / H},
        {"forearm", glm::length(wrist_l - elbow_l) / height, canon.forearm_length / H},
        {"shoulder width", glm::length(shoulder_l - shoulder_r) / height,
         canon.shoulder_width / H},
    };


    // =====================================================================
    // THE SILHOUETTE, MEASURED ON THE MESH.
    //
    // WHY A SECOND TABLE AT ALL, when Rule 47 says this instrument measures
    // the SKELETON. Because the owner's complaint on 31.08 -- "чрезмерно
    // перекачен, слишком длинные руки, живота нет" -- names three things a
    // joint cannot hold. Every landmark above was green on this very model
    // while all three were true of it: the arm is long past the WRIST, which
    // is the last joint the rig has; muscle is the mesh's radius about a bone,
    // and a bone has no radius; a belly is the depth of the trunk between two
    // joints. A judge whose whole table is joints will pass a bodybuilder and
    // a stick figure with equal enthusiasm.
    //
    // IT IS STILL NOT A PICTURE. Nothing below reads a frame, a camera or a
    // light: these are extents of the REST-POSED SKIN in metres, which is
    // geometry exactly as much as a joint position is. What Rule 47 forbids is
    // judging a silhouette off a rendered image; measuring the body that casts
    // it is the opposite of that.
    //
    // THE BAND IS ITS OWN (--silhouette-tolerance, default 15 %), and that is
    // not a loosening of the 5 % above. A joint height is a length and the
    // canon states it to three digits; a bideltoid width is a body that wears
    // clothes, and HUMAN_SCALE.md itself gives 0.259 and 0.29 for the same
    // shoulders depending on whether bone or skin is meant. Judging skin at the
    // bone's tolerance would be precision the source does not have.
    // =====================================================================
    //
    // WHICH RIG BONE EACH IMPORTED JOINT BELONGS TO -- its nearest bound
    // ancestor, itself included. Joints are stored parent-before-child, so one
    // forward pass fills the tree; a Rigify spine's three unbound links thus
    // count as trunk and a finger as hand, which is what a body part means.
    std::vector<int> bone_of_joint(n, -1);
    for (uint32_t b = 0; b < dfn::anim::BONE_COUNT; ++b) {
        const int32_t j = bind.joint[b];
        if (j >= 0) {
            bone_of_joint[static_cast<std::size_t>(j)] = static_cast<int>(b);
        }
    }
    for (std::size_t j = 0; j < n; ++j) {
        if (bone_of_joint[j] < 0) {
            const int32_t par = obj->skeleton.joints[j].parent;
            if (par >= 0) {
                bone_of_joint[j] = bone_of_joint[static_cast<std::size_t>(par)];
            }
        }
    }
    // A VERTEX BELONGS TO ITS HEAVIEST BONE. Not to all four: a body part is a
    // partition of the surface, and the seam vertices that straddle two parts
    // are a rounding error on an extent, not a fifth limb.
    std::vector<int> vbone(obj->skin.vertices.size(), -1);
    for (std::size_t i = 0; i < obj->skin.vertices.size(); ++i) {
        const auto& v = obj->skin.vertices[i];
        int best = -1;
        float bw = -1.0f;
        for (int k = 0; k < 4; ++k) {
            if (v.weights[k] > bw) {
                bw = v.weights[k];
                best = static_cast<int>(v.joints[k]);
            }
        }
        vbone[i] = best >= 0 && static_cast<std::size_t>(best) < n
                       ? bone_of_joint[static_cast<std::size_t>(best)]
                       : -1;
    }
    const auto bi = [](Bone b) { return static_cast<int>(bone_index(b)); };
    const auto is_one_of = [](int b, std::initializer_list<int> set) {
        return std::find(set.begin(), set.end(), b) != set.end();
    };
    const bool arm_bones_split =
        std::any_of(vbone.begin(), vbone.end(),
                    [&](int b) { return b == bi(Bone::HandL) || b == bi(Bone::HandR); });

    // WHICH WAY THE FIGURE FACES, TAKEN FROM THE TOES rather than assumed.
    // The importer's --yaw is a flag someone passes; the toe is a fact about
    // the body. A belly measured against the wrong sign is a back.
    float facing = -1.0f; // model faces -Z by our convention (docs/RIG.md)
    {
        const int32_t ankle_j = bind.joint[bone_index(Bone::FootL)];
        float far_neg = 0.0f;
        float far_pos = 0.0f;
        for (std::size_t i = 0; i < rest.size(); ++i) {
            if (vbone[i] != bi(Bone::FootL) && vbone[i] != bi(Bone::FootR)) {
                continue;
            }
            const float dz = rest[i].z - joint_pos(model, ankle_j).z;
            far_neg = std::min(far_neg, dz);
            far_pos = std::max(far_pos, dz);
        }
        if (far_pos > -far_neg) {
            facing = 1.0f;
        }
    }

    const float axis_x = hip.x;
    /// Widest x-extent of the chosen vertices inside a horizontal band, as a
    /// fraction of the figure's height. `keep` decides what counts as the body
    /// part being measured.
    const auto width_in_band = [&](float y0_frac, float y1_frac, auto keep) {
        float wmin = 0.0f;
        float wmax = 0.0f;
        bool any = false;
        for (std::size_t i = 0; i < rest.size(); ++i) {
            const float f = (rest[i].y - lo) / height;
            if (f < y0_frac || f > y1_frac || !keep(vbone[i])) {
                continue;
            }
            wmin = any ? std::min(wmin, rest[i].x) : rest[i].x;
            wmax = any ? std::max(wmax, rest[i].x) : rest[i].x;
            any = true;
        }
        return any ? (wmax - wmin) / height : 0.0f;
    };
    /// The trunk's front reach and its full depth at a height, as fractions of
    /// height. TWO NUMBERS AND NOT ONE, because a belly is not a thicker trunk:
    /// it is thickness that went FORWARD. A man's back is nearly flat from the
    /// shoulder blades to the sacrum while his front is not, so the depth alone
    /// cannot tell a gut from a barrel chest, and the front reach alone cannot
    /// tell a gut from a man leaning back.
    const auto trunk_at = [&](float y0_frac, float y1_frac, float& front,
                              float& depth) {
        float fmax = 0.0f;
        float bmax = 0.0f;
        bool any = false;
        for (std::size_t i = 0; i < rest.size(); ++i) {
            const float f = (rest[i].y - lo) / height;
            if (f < y0_frac || f > y1_frac) {
                continue;
            }
            if (arm_bones_split
                && is_one_of(vbone[i], {bi(Bone::UpperArmL), bi(Bone::UpperArmR),
                                        bi(Bone::ForearmL), bi(Bone::ForearmR),
                                        bi(Bone::HandL), bi(Bone::HandR)})) {
                continue;
            }
            const float d = rest[i].z * facing;
            fmax = any ? std::max(fmax, d) : d;
            bmax = any ? std::min(bmax, d) : d;
            any = true;
        }
        front = any ? fmax / height : 0.0f;
        depth = any ? (fmax - bmax) / height : 0.0f;
    };
    const auto depth_in_band = [&](float y0_frac, float y1_frac) {
        float front = 0.0f;
        float depth = 0.0f;
        trunk_at(y0_frac, y1_frac, front, depth);
        return depth;
    };
    /// Diameter of a limb across the MIDDLE of its segment, measured
    /// perpendicular to the bone. Twice the farthest perpendicular reach, which
    /// is the honest reading of "how thick is this arm" for a shape that is not
    /// a circle.
    const auto limb_diameter = [&](Bone bone, Bone child) {
        const glm::vec3 a = at(bone);
        const glm::vec3 b = at(child);
        const glm::vec3 axis = b - a;
        const float len = glm::length(axis);
        if (len < 1e-4f) {
            return 0.0f;
        }
        const glm::vec3 u = axis / len;
        float best = 0.0f;
        for (std::size_t i = 0; i < rest.size(); ++i) {
            if (vbone[i] != bi(bone)) {
                continue;
            }
            const glm::vec3 d = rest[i] - a;
            const float t = glm::dot(d, u) / len;
            if (t < 0.3f || t > 0.7f) {
                continue;
            }
            best = std::max(best, glm::length(d - u * (t * len)));
        }
        return 2.0f * best / height;
    };

    // THE HAND IS MEASURED FROM THE WRIST TO THE FARTHEST VERTEX ON IT, and
    // this single number is the owner's "слишком длинные руки". The rig's last
    // arm joint IS the wrist: everything past it -- palm, fingers -- is mesh
    // the joint table never saw, and the fit that made every segment canonical
    // above could not touch it.
    float hand_len = 0.0f;
    float fingertip_y = hi;
    if (arm_bones_split) {
        for (const Bone h : {Bone::HandL, Bone::HandR}) {
            const glm::vec3 w = at(h);
            for (std::size_t i = 0; i < rest.size(); ++i) {
                if (vbone[i] != bi(h)) {
                    continue;
                }
                hand_len = std::max(hand_len, glm::length(rest[i] - w));
                fingertip_y = std::min(fingertip_y, rest[i].y);
            }
        }
    }
    const float upper_arm_m = glm::length(elbow_l - shoulder_l);
    const float forearm_m = glm::length(wrist_l - elbow_l);
    const float half_span = std::fabs(shoulder_l.x - axis_x) + upper_arm_m + forearm_m
                            + hand_len;
    const float bideltoid = width_in_band(0.760f, 0.830f, [](int) { return true; });
    const float hip_breadth =
        width_in_band(0.495f, 0.545f, [&](int b) {
            return !arm_bones_split
                   || !is_one_of(b, {bi(Bone::UpperArmL), bi(Bone::UpperArmR),
                                     bi(Bone::ForearmL), bi(Bone::ForearmR),
                                     bi(Bone::HandL), bi(Bone::HandR)});
        });
    // THE BANDS ARE THE CANON'S OWN HEIGHTS, not round numbers: the chest at
    // BODY_CHEST_HEIGHT_FRAC and the waist at BODY_NAVEL_HEIGHT_FRAC. A belly
    // measured five centimetres off the navel is a different organ.
    const auto CH = [](double f) { return static_cast<float>(f); };
    const float chest_h = CH(dfn::config::BODY_CHEST_HEIGHT_FRAC);
    const float navel_h = CH(dfn::config::BODY_NAVEL_HEIGHT_FRAC);
    const float shoulder_h = static_cast<float>(dfn::config::BODY_SHOULDER_HEIGHT_FRAC);
    const auto no_arms = [&](int b) {
        return !arm_bones_split
               || !is_one_of(b, {bi(Bone::UpperArmL), bi(Bone::UpperArmR),
                                 bi(Bone::ForearmL), bi(Bone::ForearmR),
                                 bi(Bone::HandL), bi(Bone::HandR)});
    };
    /// How far the trunk's front reaches PAST THE SPINE at a height, in
    /// fractions of the figure's height.
    ///
    /// THE BELLY LIVES HERE AND NOT IN THE DEPTH, and finding that out cost
    /// this wave a whole revision. Depth is front PLUS back, and a man's back
    /// at chest height carries the shoulder blades: a trunk that is 0.27 m
    /// deep at the nipples and 0.26 m at the navel has a perfectly visible
    /// gut, because 0.13 of the first is behind the spine and only 0.06 of the
    /// second is. Judging the belly by depth reported "no belly" on a figure
    /// whose stomach reached 3.6 cm further forward than its chest.
    const auto front_from_spine = [&](float y0_frac, float y1_frac) {
        const glm::vec3 hip_j = hip;
        const glm::vec3 neck_j = neck;
        float best = 0.0f;
        bool any = false;
        for (std::size_t i = 0; i < rest.size(); ++i) {
            const float f = (rest[i].y - lo) / height;
            if (f < y0_frac || f > y1_frac || !no_arms(vbone[i])) {
                continue;
            }
            const float t = (rest[i].y - hip_j.y) / std::max(1e-4f, neck_j.y - hip_j.y);
            const float spine_z = hip_j.z + (neck_j.z - hip_j.z) * t;
            const float d = (rest[i].z - spine_z) * facing;
            best = any ? std::max(best, d) : d;
            any = true;
        }
        return any ? best / height : 0.0f;
    };
    const float chest_depth = depth_in_band(chest_h - 0.02f, chest_h + 0.02f);
    const float waist_depth = depth_in_band(navel_h - 0.02f, navel_h + 0.02f);
    const float belly_depth = depth_in_band(navel_h - 0.08f, navel_h - 0.04f);
    const float chest_w = width_in_band(chest_h - 0.02f, chest_h + 0.02f, no_arms);
    const float waist_w = width_in_band(navel_h - 0.02f, navel_h + 0.02f, no_arms);

    const std::vector<Landmark> mesh_rows{
        {"hand length (wrist-tip)", hand_len / height, canon.hand_length / H},
        {"fingertip height", (fingertip_y - lo) / height,
         static_cast<float>(dfn::config::BODY_FINGERTIP_HEIGHT_FRAC)},
        {"arm span", 2.0f * half_span / height,
         static_cast<float>(dfn::config::BODY_ARM_SPAN_FRAC)},
        {"shoulders (bideltoid)", bideltoid,
         static_cast<float>(dfn::config::BODY_SHOULDER_SILHOUETTE_FRAC)},
        {"hip breadth (mesh)", hip_breadth, canon.hip_width / H},
        {"chest width", chest_w, static_cast<float>(dfn::config::BODY_CHEST_WIDTH_FRAC)},
        {"chest depth", chest_depth, canon.torso_depth / H},
        {"waist breadth (navel)", waist_w,
         static_cast<float>(dfn::config::BODY_WAIST_WIDTH_FRAC)},
        // THE ONE ROW THAT IS THE BELLY. It is judged against a number only
        // 4 % larger than the chest's depth, and that smallness is the point:
        // the owner asked for a man who is not carved, not for a fat one.
        {"waist depth (navel)", waist_depth,
         static_cast<float>(dfn::config::BODY_WAIST_DEPTH_FRAC)},
        {"upper arm diameter", limb_diameter(Bone::UpperArmL, Bone::ForearmL),
         canon.arm_thickness / H},
        {"thigh diameter", limb_diameter(Bone::ThighL, Bone::ShinL),
         canon.leg_thickness / H},
    };

    const float heads = height / std::max(hi - neck.y, 1e-4f);

    // === THE BASELINE ====================================================
    // Recording it is a separate errand and it ENDS HERE: writing a file and
    // then passing judgement in the same run would make "what does this body
    // measure" and "is this body allowed" one command, and the first is
    // exactly what you reach for when the second is refusing.
    if (!write_baseline.empty()) {
        if (!write_baseline_file(write_baseline, path, obj->name, height, heads, rows,
                                 mesh_rows)) {
            std::fprintf(stderr, "[scale] не пишется baseline \"%s\"\n",
                         write_baseline.c_str());
            return 1;
        }
        std::printf("[scale] baseline записан: %s — \"%s\", %.3f м, %.2f головы, "
                    "%zu суставных строк, %zu силуэтных\n",
                    write_baseline.c_str(), obj->name.c_str(),
                    static_cast<double>(height), static_cast<double>(heads),
                    rows.size(), mesh_rows.size());
        return 0;
    }
    Baseline base;
    if (!baseline_path.empty()) {
        std::string err;
        if (!read_baseline_file(baseline_path, base, err)) {
            std::fprintf(stderr,
                         "[scale] baseline \"%s\": %s\n"
                         "        пересоздаётся ЯВНО: dfn_human_scale %s "
                         "--write-baseline %s\n",
                         baseline_path.c_str(), err.c_str(), path.c_str(),
                         baseline_path.c_str());
            return 1;
        }
    }
    const bool by_baseline = base.loaded;

    int failures = 0;
    int canon_out = 0;
    if (!quiet) {
        std::printf("[scale] \"%s\": %s\n", path.c_str(), obj->name.c_str());
        std::printf("[scale] figure height %.3f m (model units), %.2f heads "
                    "(canon 7.5-8.0)\n",
                    static_cast<double>(height), static_cast<double>(heads));
        if (by_baseline) {
            // THE ONE LINE THAT SAYS WHO IS JUDGING. A reader who sees a green
            // verdict under a table full of "<-- CANON" has to be told, in the
            // report itself, which column the exit code came from.
            std::printf("[scale] ВЕРДИКТ ПО BASELINE \"%s\" (тело \"%s\", %.3f м, "
                        "%.2f головы); канон печатается СПРАВОЧНО\n",
                        baseline_path.c_str(), base.body.c_str(),
                        static_cast<double>(base.height),
                        static_cast<double>(base.heads));
            if (base.body != obj->name) {
                std::printf("[scale] ВНИМАНИЕ: baseline снят с тела \"%s\", а мерится "
                            "\"%s\" — судим по числам, но это разные тела\n",
                            base.body.c_str(), obj->name.c_str());
            }
            std::printf("        %-28s %8s %8s %9s %9s %9s\n", "landmark", "model",
                        "canon", "d.canon", "baseline", "d.base");
        } else {
            std::printf("        %-28s %8s %8s %9s\n", "landmark", "model", "canon",
                        "delta");
        }
    }
    /// One table, judged by whichever column is authoritative. `measurable`
    /// keeps the n/a rows of a model with no hand bones out of both counts.
    const auto judge = [&](const std::vector<Landmark>& table, float band,
                           float canon_band, bool silhouette) {
        for (const Landmark& r : table) {
            const bool measurable = !silhouette || r.measured > 1e-6f;
            const float rel_c =
                r.canon > 1e-6f ? (r.measured - r.canon) / r.canon : 0.0f;
            const bool bad_c = measurable && std::fabs(rel_c) > canon_band;
            canon_out += bad_c ? 1 : 0;
            if (!by_baseline) {
                failures += bad_c ? 1 : 0;
                if (!quiet) {
                    std::printf("        %-28s %8.3f %8.3f %+8.1f%% %s\n", r.name,
                                static_cast<double>(r.measured),
                                static_cast<double>(r.canon),
                                static_cast<double>(rel_c * 100.0f),
                                bad_c ? "<-- OUT" : (measurable ? "" : "(n/a)"));
                }
                continue;
            }
            const float* want = base.find(silhouette, r.name);
            if (want == nullptr) {
                // A ROW THE BASELINE HAS NEVER SEEN IS A FAILURE, not a skip:
                // the instrument grew a landmark and nobody re-approved the
                // body against it.
                ++failures;
                if (!quiet) {
                    std::printf("        %-28s %8.3f %8.3f %+8.1f%% %9s %9s <-- НЕТ В "
                                "BASELINE\n", r.name, static_cast<double>(r.measured),
                                static_cast<double>(r.canon),
                                static_cast<double>(rel_c * 100.0f), "-", "-");
                }
                continue;
            }
            const float rel_b = *want > 1e-6f ? (r.measured - *want) / *want : 0.0f;
            const bool bad_b = std::fabs(rel_b) > band;
            failures += bad_b ? 1 : 0;
            if (!quiet) {
                std::printf("        %-28s %8.3f %8.3f %+8.1f%% %9.3f %+8.1f%% %s\n",
                            r.name, static_cast<double>(r.measured),
                            static_cast<double>(r.canon),
                            static_cast<double>(rel_c * 100.0f),
                            static_cast<double>(*want),
                            static_cast<double>(rel_b * 100.0f),
                            bad_b ? "<-- OUT" : (bad_c ? "(канон мимо)" : ""));
            }
        }
    };
    judge(rows, tolerance, CANON_JOINT_BAND, false);
    if (!quiet) {
        std::printf("        %s\n", "-- silhouette (rest-posed MESH, not joints) "
                                     "-------------------");
    }
    if (!arm_bones_split && !quiet) {
        std::printf("        (this model weights no vertex to a HAND bone: the "
                    "hand, fingertip and span rows read 0 and are NOT judged)\n");
    }
    judge(mesh_rows, silhouette_tolerance, CANON_SILHOUETTE_BAND, true);
    // THREE RATIOS, PRINTED AND NOT JUDGED, and the honesty is in the second
    // half of that sentence. Each is a quotient of two rows already judged
    // above, so failing them again would count one defect twice; they are here
    // because they, not the fractions, are what a person SEES -- "V-shaped",
    // "barrel-chested", "has a gut" are all ratios. The bands beside them come
    // from docs/design/HUMAN_SCALE.md's own reading of ANSUR II (bideltoid
    // 0.290H over hip breadth 0.191H = 1.52) and from the reference frames
    // gathered for this wave; they say what the number MEANS, not whether the
    // model passes.
    if (!quiet) {
        std::printf("        %-28s %8.2f  band %s\n", "shoulders / hips",
                    static_cast<double>(hip_breadth > 1e-6f ? bideltoid / hip_breadth
                                                            : 0.0f),
                    "1.35-1.65 (>1.8 = bodybuilder)");
        const float f_che = front_from_spine(chest_h - 0.02f, chest_h + 0.02f);
        const float f_nav = front_from_spine(navel_h - 0.02f, navel_h + 0.02f);
        std::printf("        %-28s %8.2f  band %s\n", "BELLY: navel / chest front",
                    static_cast<double>(f_che > 1e-6f ? f_nav / f_che : 0.0f),
                    "1.00-1.25 (<0.95 = no belly, >1.4 = gut)");
        std::printf("        %-28s %8.2f  band %s\n", "waist / chest (depth)",
                    static_cast<double>(chest_depth > 1e-6f ? waist_depth / chest_depth
                                                            : 0.0f),
                    "0.92-1.05 - front+BACK, blades included");
        std::printf("        %-28s %8.2f  band %s\n", "lower belly / chest (depth)",
                    static_cast<double>(chest_depth > 1e-6f ? belly_depth / chest_depth
                                                            : 0.0f),
                    "0.75-1.00 - pelvis, not stomach");
        // THE ARM'S OWN TAPER, printed and not judged. The reshape scales a
        // whole arm by ONE factor -- the canon has a row for the upper arm's
        // thickness and none for the forearm's -- so this ratio is the model's
        // own and comes out of the pass unchanged. It is here because "the
        // muscle is gone" and "the arms are sticks" are the same measurement
        // read twice, and the second reading needs its own number.
        const float fore_d = limb_diameter(Bone::ForearmL, Bone::HandL);
        const float upper_d = limb_diameter(Bone::UpperArmL, Bone::ForearmL);
        std::printf("        %-28s %8.2f  band %s\n", "forearm / upper arm",
                    static_cast<double>(upper_d > 1e-6f ? fore_d / upper_d : 0.0f),
                    "0.78-0.92 (girth 27 cm over 32 cm)");
        std::printf("        %-28s %8.3f  %8s %s\n", "  forearm diameter",
                    static_cast<double>(fore_d), "", "(fraction of height)");
        std::printf("        %-28s %8.2f  band %s\n", "chest width / depth",
                    static_cast<double>(chest_depth > 1e-6f ? chest_w / chest_depth
                                                            : 0.0f),
                    "1.20-1.50");
        // THE TRUNK'S PROFILE IN METRES, printed because a ratio that has gone
        // wrong never says WHERE. Five heights from the armpit to the hip, each
        // with the width, the depth and how far the front reaches past the
        // body axis -- read down the front column and a belly is visible as a
        // number the way it is visible on a person.
        // THE PROFILE IS MEASURED FROM THE SPINE, not from z = 0. A model
        // whose author left the body off the origin would otherwise report a
        // belly or a hunchback that is only an offset, and the direction the
        // figure faces is taken from its toes (see `facing` above) rather than
        // assumed from the import flag.
        std::printf("        facing %+.0f Z, spine at z = %.3f (hips) / %.3f "
                    "(neck)\n", static_cast<double>(facing),
                    static_cast<double>(hip.z * facing),
                    static_cast<double>(neck.z * facing));
        std::printf("        %-28s %8s %8s %8s\n", "trunk profile (m)", "width",
                    "depth", "front");
        for (const float f : {shoulder_h, chest_h, 0.5f * (chest_h + navel_h),
                              navel_h, navel_h - 0.06f}) {
            float front = 0.0f;
            float depth = 0.0f;
            trunk_at(f - 0.02f, f + 0.02f, front, depth);
            const float w = width_in_band(f - 0.02f, f + 0.02f, [&](int b) {
                return !arm_bones_split
                       || !is_one_of(b, {bi(Bone::UpperArmL), bi(Bone::UpperArmR),
                                         bi(Bone::ForearmL), bi(Bone::ForearmR),
                                         bi(Bone::HandL), bi(Bone::HandR)});
            });
            char label[32];
            std::snprintf(label, sizeof(label), "  at %.2fH", static_cast<double>(f));
            std::printf("        %-28s %8.3f %8.3f %8.3f\n", label,
                        static_cast<double>(w * height),
                        static_cast<double>(depth * height),
                        static_cast<double>(front * height));
        }
    }

    // HEADS-PER-FIGURE IS JUDGED SEPARATELY and its band is the artistic one
    // (HUMAN_SCALE.md: 7.5-8 heads), not a percentage of a fraction: it is the
    // single number a person reads off a silhouette, and the stylised chibi
    // that started this rule fails HERE first and by a mile.
    const bool heads_canon_bad = heads < 7.5f || heads > 8.0f;
    canon_out += heads_canon_bad ? 1 : 0;
    bool heads_bad = heads_canon_bad;
    if (by_baseline) {
        heads_bad = base.heads > 1e-3f
                    && std::fabs(heads - base.heads) / base.heads > tolerance;
    }
    failures += heads_bad ? 1 : 0;
    if (!quiet && (heads_bad || heads_canon_bad)) {
        std::printf("        %-28s %8.2f %8s %9s %s\n", "heads per figure",
                    static_cast<double>(heads), by_baseline ? "baseline" : "7.5-8.0",
                    "", heads_bad ? "<-- OUT" : "(канон мимо: 7.5-8.0)");
    }
    if (!by_baseline) {
        if (failures > 0) {
            std::fprintf(stderr,
                         "[scale] %d landmark(s) outside the canon's band "
                         "(+-%.0f%% on joints, +-%.0f%% on the silhouette) "
                         "(docs/design/HUMAN_SCALE.md) — REFUSED as a VISIBLE "
                         "character. A model may still be a fine test fixture.\n",
                         failures, static_cast<double>(tolerance * 100.0f),
                         static_cast<double>(silhouette_tolerance * 100.0f));
            return 1;
        }
        if (!quiet) {
            std::printf("[scale] every landmark within its band (+-%.0f%% joints, "
                        "+-%.0f%% silhouette)\n",
                        static_cast<double>(tolerance * 100.0f),
                        static_cast<double>(silhouette_tolerance * 100.0f));
        }
        return 0;
    }
    // THE BASELINE'S VERDICT, and the canon's count printed BESIDE it rather
    // than swallowed. "Green here, twelve rows off the canon" is the honest
    // description of the body the owner chose, and hiding the second half
    // would turn this judge back into a rubber stamp.
    if (failures > 0) {
        std::fprintf(stderr,
                     "[scale] %d строк(и) вне BASELINE \"%s\" (+-%.0f%% суставы, "
                     "+-%.0f%% силуэт) — ТЕЛО СДВИНУЛОСЬ. Если сдвиг намеренный, "
                     "baseline пересоздаётся явно: dfn_human_scale %s "
                     "--write-baseline %s\n",
                     failures, baseline_path.c_str(),
                     static_cast<double>(tolerance * 100.0f),
                     static_cast<double>(silhouette_tolerance * 100.0f), path.c_str(),
                     baseline_path.c_str());
        return 1;
    }
    if (!quiet) {
        std::printf("[scale] BASELINE: все строки внутри +-%.0f%% (суставы) / "
                    "+-%.0f%% (силуэт). КАНОН СПРАВОЧНО: %d строк(и) вне полосы "
                    "(±%.0f%% / ±%.0f%%) — решение владельца 01.09, тело "
                    "отгружается сырым\n",
                    static_cast<double>(tolerance * 100.0f),
                    static_cast<double>(silhouette_tolerance * 100.0f), canon_out,
                    static_cast<double>(CANON_JOINT_BAND * 100.0f),
                    static_cast<double>(CANON_SILHOUETTE_BAND * 100.0f));
    }
    return 0;
}
