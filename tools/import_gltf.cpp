/*
Module: tools
File: tools/import_gltf.cpp

Responsibility:
- dfn_import_gltf: reads a glTF 2.0 file (cgltf) with a skin and writes ONE
  .dfo v5 carrying the skinned mesh (SKIN), the skeleton (SKEL) and the clips
  (ANIM). The offline half of the character pipeline: the game never sees a
  .gltf, only the .dfo this tool bakes.

Key items:
- main(): CLI (--out, --height, --yaw, --skin, --name, --fit-hips,
  --fit-canon, --reshape, --skin-palette).
- read_skeleton() / read_skin() / read_clips(): the three sections.
- normalize_and_scale(): metres and facing, baked at import.
- fit_to_canon(): the JOINTS to docs/design/HUMAN_SCALE.md, by moving them.
- reshape_to_commoner(): the SKIN to the same canon -- limb thickness, hand
  length, hip breadth and the waist -- by deforming it in the rest pose and
  writing the result back through the skinning, normals rebuilt.

Dependencies:
- Uses: cgltf (bgfx's vendored copy), engine/render ObjectRegistry, engine/core
  skeleton, engine/anim BoneMap (the coverage report only).
- Used by: the character pipeline; run by hand, and by ctest through the
  fixture it bakes.

Notes:
- WHY A THIRD-PARTY HEADER IS ALLOWED HERE AND NOWHERE ELSE (Rule 1). Rule 1
  isolates third-party libraries from the ENGINE's layers; this is an OFFLINE
  TOOL, outside the DAG entirely, and the isolation it exists to protect is
  kept in the strongest possible form: glTF does not reach the runtime at all.
  The engine's only input is the .dfo, exactly as it is for trees, kits and
  houses (в1: "nothing is generated in the frame" -- here, nothing is PARSED
  in the frame either).
- TWO FITS, AND THEY ANSWER DIFFERENT COMPLAINTS. --fit-canon moves JOINTS;
  --reshape moves FLESH. Keeping them apart is not tidiness: on the reference
  base every one of the nine joint landmarks was inside 2 % of the canon at
  the moment the owner called the figure "чрезмерно перекачен, слишком длинные
  руки, живота нет". A joint has no radius, the rig's last arm joint is the
  wrist, and a belly lies between two joints -- so no amount of moving joints
  could have answered any of the three.
- NEITHER FIT HAS A TASTE KNOB. Every factor either pass applies is a quotient
  of two measurements: what the registry says the body should be, over what
  this model measures. Adding a hand-tuned multiplier anywhere here would put
  the pass beyond the reach of any instrument, which is the one thing the
  whole character wave exists to avoid.
- METRES AND FACING ARE BAKED, NOT CONFIGURED. A model imported at 1.9 m
  facing -Z has no runtime knob to get wrong, and a knob is what would rot: it
  would be read by the app, by the editor and by every test, and the day one
  of them forgets it, a character stands 30 cm tall with its back to you and
  nothing says why.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- LOUD ON EVERY REFUSAL. A half-imported character is worse than none: it
  draws, it looks nearly right, and the defect surfaces a week later as
  "animation feels off".
*/

#define CGLTF_IMPLEMENTATION
#include "cgltf.h"

#include "engine/anim/sources/BodyMesh.h"
#include "engine/anim/sources/BoneMap.h"
#include "engine/anim/sources/SkinnedBody.h"
#include "engine/anim/sources/RestFit.h"
#include "engine/anim/sources/Rig.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <map>
#include <string>
#include <vector>

namespace {

using dfn::render::SkinMesh;
namespace skel = dfn::skel;

struct Options {
    std::string input;
    std::string out;
    std::string name = "character";
    /// Target standing height in metres; 0 = keep the file's own scale.
    float height_m = 0.0f;
    /// Fit by the HIP JOINT instead of the mesh's bounding box.
    bool fit_hips = false;
    /// Rescale the SEGMENTS to the canon of docs/design/HUMAN_SCALE.md.
    bool fit_canon = false;
    /// Deform the SKIN to the canon's body shape (muscle, hand, belly).
    bool reshape = false;
    /// Print the trunk's factor at every sampled height. The one view that
    /// answers "the chest is 10 % over, WHERE did the field lose it".
    bool reshape_trace = false;
    bool skin_palette = false;
    /// Degrees of yaw baked into the model so it ends up facing -Z (our
    /// convention, docs/RIG.md). Most authoring tools export facing +Z.
    float yaw_deg = 180.0f;
    int skin_index = 0;
};

[[nodiscard]] glm::mat4 node_local(const cgltf_node* node) {
    cgltf_float m[16];
    cgltf_node_transform_local(node, m);
    return glm::make_mat4(m);
}

/// TRS of a node, taken from its explicit fields when it has them and
/// decomposed from its matrix when it does not. glTF allows either, and a
/// reader that handles only one silently loses half the files in the world.
void node_trs(const cgltf_node* node, glm::vec3& t, glm::quat& r, glm::vec3& s) {
    if (node->has_matrix != 0) {
        const glm::mat4 m = node_local(node);
        t = glm::vec3{m[3]};
        glm::mat3 basis{m};
        s = glm::vec3{glm::length(basis[0]), glm::length(basis[1]),
                      glm::length(basis[2])};
        for (int c = 0; c < 3; ++c) {
            basis[c] = s[c] > 1e-8f ? basis[c] / s[c] : basis[c];
        }
        r = glm::normalize(glm::quat_cast(basis));
        return;
    }
    t = node->has_translation != 0
            ? glm::vec3{node->translation[0], node->translation[1], node->translation[2]}
            : glm::vec3{0.0f};
    r = node->has_rotation != 0
            ? glm::quat{node->rotation[3], node->rotation[0], node->rotation[1],
                        node->rotation[2]}
            : glm::quat{1.0f, 0.0f, 0.0f, 0.0f};
    s = node->has_scale != 0
            ? glm::vec3{node->scale[0], node->scale[1], node->scale[2]}
            : glm::vec3{1.0f};
}

/// glTF's joint order is whatever the exporter wrote; ours must be
/// parent-before-child (engine/core Skeleton). This reorders once, and every
/// joint index in the file (vertex JOINTS_0, animation targets) is remapped
/// through the same table -- there is exactly ONE map, built here.
struct JointOrder {
    std::vector<const cgltf_node*> nodes;      ///< in OUR order
    std::map<const cgltf_node*, int32_t> index; ///< node -> our index
};

[[nodiscard]] bool build_joint_order(const cgltf_skin* skin, JointOrder& order) {
    std::map<const cgltf_node*, bool> in_skin;
    for (cgltf_size i = 0; i < skin->joints_count; ++i) {
        in_skin[skin->joints[i]] = true;
    }
    // Depth-first from every joint whose parent is not itself a joint.
    std::vector<const cgltf_node*> stack;
    for (cgltf_size i = 0; i < skin->joints_count; ++i) {
        const cgltf_node* j = skin->joints[i];
        if (j->parent == nullptr || in_skin.find(j->parent) == in_skin.end()) {
            stack.push_back(j);
        }
    }
    if (stack.empty()) {
        std::fprintf(stderr, "[import] the skin has no root joint (a cycle?) -- REFUSED\n");
        return false;
    }
    // Reverse so the first root comes out first (stack pops from the back).
    std::reverse(stack.begin(), stack.end());
    while (!stack.empty()) {
        const cgltf_node* n = stack.back();
        stack.pop_back();
        if (order.index.find(n) != order.index.end()) {
            continue;
        }
        order.index[n] = static_cast<int32_t>(order.nodes.size());
        order.nodes.push_back(n);
        for (cgltf_size c = n->children_count; c > 0; --c) {
            const cgltf_node* child = n->children[c - 1];
            if (in_skin.find(child) != in_skin.end()) {
                stack.push_back(child);
            }
        }
    }
    if (order.nodes.size() != skin->joints_count) {
        std::fprintf(stderr,
                     "[import] %zu joints reachable of %zu in the skin -- the "
                     "hierarchy is disconnected, REFUSED\n",
                     order.nodes.size(), static_cast<size_t>(skin->joints_count));
        return false;
    }
    return true;
}

[[nodiscard]] bool read_skeleton(const cgltf_skin* skin, const JointOrder& order,
                                 skel::Skeleton& out) {
    out.joints.resize(order.nodes.size());
    for (std::size_t i = 0; i < order.nodes.size(); ++i) {
        const cgltf_node* n = order.nodes[i];
        skel::SkeletonJoint& j = out.joints[i];
        j.name = n->name != nullptr ? n->name : ("joint" + std::to_string(i));
        const auto pit = n->parent != nullptr ? order.index.find(n->parent)
                                              : order.index.end();
        j.parent = pit != order.index.end() ? pit->second : -1;
        node_trs(n, j.bind_translation, j.bind_rotation, j.bind_scale);
        j.inverse_bind = glm::mat4{1.0f};
    }
    if (skin->inverse_bind_matrices != nullptr) {
        for (cgltf_size g = 0; g < skin->joints_count; ++g) {
            cgltf_float m[16];
            if (cgltf_accessor_read_float(skin->inverse_bind_matrices, g, m, 16) == 0) {
                std::fprintf(stderr, "[import] inverseBindMatrices[%zu] unreadable "
                                     "-- REFUSED\n", static_cast<size_t>(g));
                return false;
            }
            const auto it = order.index.find(skin->joints[g]);
            if (it != order.index.end()) {
                out.joints[static_cast<std::size_t>(it->second)].inverse_bind =
                    glm::make_mat4(m);
            }
        }
    } else {
        // No inverse binds in the file: the spec says they default to identity,
        // which is only correct when the skin is authored at the origin.
        std::fprintf(stderr, "[import] NOTE: no inverseBindMatrices; identity "
                             "assumed (glTF default)\n");
    }
    return true;
}

/// ONE PRIMITIVE'S FLAT ALBEDO, from its glTF material. sRGB is what the rest
/// of the pipe stores (BodyMesh's tunics, every prop's vertex colour), and
/// glTF's baseColorFactor is LINEAR, so the transfer function is applied here
/// rather than left for a shader that does not know where the colour came
/// from. A material with a base colour TEXTURE still contributes only its
/// factor: this asset ships none (no images, no textures in the file), and the
/// case where one exists is a texture section the .dfo does not have yet --
/// named as a tail rather than silently averaged into a colour.
[[nodiscard]] uint32_t base_color_rgba(const cgltf_material* mat, uint32_t fallback) {
    if (mat == nullptr || mat->has_pbr_metallic_roughness == 0) {
        return fallback;
    }
    const cgltf_float* f = mat->pbr_metallic_roughness.base_color_factor;
    const auto to_srgb = [](float c) {
        c = c < 0.0f ? 0.0f : (c > 1.0f ? 1.0f : c);
        const float s = c <= 0.0031308f ? c * 12.92f
                                        : 1.055f * std::pow(c, 1.0f / 2.4f) - 0.055f;
        return static_cast<uint32_t>(s * 255.0f + 0.5f);
    };
    const uint32_t r = to_srgb(static_cast<float>(f[0]));
    const uint32_t g = to_srgb(static_cast<float>(f[1]));
    const uint32_t b = to_srgb(static_cast<float>(f[2]));
    const uint32_t a = to_srgb(static_cast<float>(f[3]));
    return (a << 24) | (b << 16) | (g << 8) | r; // 0xAABBGGRR
}

/// APPENDS one primitive to `out`. Appends, because a character is routinely
/// SIX skinned primitives and not one: the reference knight ships as
/// Knight_Body, Knight_Head, two arms and two legs, all bound to the same
/// skeleton. Reading only the first is not "a simpler importer", it is an
/// importer that silently drops five sixths of the man -- measured on the
/// first run: 594 triangles of an expected 6-7 thousand, and the height came
/// out 0.297 m because it had measured a forearm.
[[nodiscard]] bool read_skin(const cgltf_primitive* prim, const JointOrder& order,
                             const cgltf_skin* skin, SkinMesh& out,
                             uint32_t base_color_rgba) {
    const cgltf_accessor* pos = nullptr;
    const cgltf_accessor* nrm = nullptr;
    const cgltf_accessor* uv = nullptr;
    const cgltf_accessor* joints = nullptr;
    const cgltf_accessor* weights = nullptr;
    for (cgltf_size a = 0; a < prim->attributes_count; ++a) {
        const cgltf_attribute& at = prim->attributes[a];
        switch (at.type) {
        case cgltf_attribute_type_position: pos = at.data; break;
        case cgltf_attribute_type_normal: nrm = at.data; break;
        case cgltf_attribute_type_texcoord: if (uv == nullptr) uv = at.data; break;
        case cgltf_attribute_type_joints: if (joints == nullptr) joints = at.data; break;
        case cgltf_attribute_type_weights: if (weights == nullptr) weights = at.data; break;
        default: break;
        }
    }
    if (pos == nullptr || joints == nullptr || weights == nullptr) {
        std::fprintf(stderr, "[import] primitive lacks POSITION/JOINTS_0/WEIGHTS_0 "
                             "-- REFUSED\n");
        return false;
    }
    const std::size_t count = pos->count;
    const std::size_t base = out.vertices.size();
    out.vertices.resize(base + count);
    for (std::size_t i = 0; i < count; ++i) {
        dfn::platform::SkinnedVertex& v = out.vertices[base + i];
        cgltf_float p[3] = {0, 0, 0};
        (void)cgltf_accessor_read_float(pos, i, p, 3);
        v.position = {p[0], p[1], p[2]};
        cgltf_float n[3] = {0, 1, 0};
        if (nrm != nullptr) {
            (void)cgltf_accessor_read_float(nrm, i, n, 3);
        }
        v.normal = glm::normalize(glm::vec3{n[0], n[1], n[2]});
        cgltf_float t[2] = {0, 0};
        if (uv != nullptr) {
            (void)cgltf_accessor_read_float(uv, i, t, 2);
        }
        v.uv = {t[0], t[1]};
        v.color_rgba = base_color_rgba;
        cgltf_uint ji[4] = {0, 0, 0, 0};
        (void)cgltf_accessor_read_uint(joints, i, ji, 4);
        cgltf_float w[4] = {0, 0, 0, 0};
        (void)cgltf_accessor_read_float(weights, i, w, 4);
        float sum = 0.0f;
        for (int k = 0; k < 4; ++k) {
            sum += w[k];
        }
        // A VERTEX WHOSE WEIGHTS DO NOT SUM TO 1 SHRINKS TOWARD THE ORIGIN,
        // and it reads as a dent in the mesh rather than as a bad file. Fixed
        // here, once, where the file is the thing being read.
        const float inv = sum > 1e-6f ? 1.0f / sum : 0.0f;
        for (int k = 0; k < 4; ++k) {
            const auto g = static_cast<cgltf_size>(ji[k]);
            int32_t ours = 0;
            if (g < skin->joints_count) {
                const auto it = order.index.find(skin->joints[g]);
                ours = it != order.index.end() ? it->second : 0;
            }
            if (ours < 0 || ours > 255) {
                std::fprintf(stderr, "[import] joint index %d out of a byte -- "
                                     "REFUSED\n", ours);
                return false;
            }
            v.joints[k] = static_cast<uint8_t>(ours);
            v.weights[k] = sum > 1e-6f ? w[k] * inv : (k == 0 ? 1.0f : 0.0f);
        }
    }
    if (prim->indices != nullptr) {
        out.indices.reserve(out.indices.size() + prim->indices->count);
        for (cgltf_size i = 0; i < prim->indices->count; ++i) {
            out.indices.push_back(
                static_cast<uint32_t>(base)
                + static_cast<uint32_t>(cgltf_accessor_read_index(prim->indices, i)));
        }
    } else {
        out.indices.reserve(out.indices.size() + count);
        for (std::size_t i = 0; i < count; ++i) {
            out.indices.push_back(static_cast<uint32_t>(base + i));
        }
    }
    return true;
}

void read_clips(const cgltf_data* data, const JointOrder& order,
                std::vector<skel::AnimClip>& out) {
    for (cgltf_size a = 0; a < data->animations_count; ++a) {
        const cgltf_animation& anim = data->animations[a];
        skel::AnimClip clip;
        clip.name = anim.name != nullptr ? anim.name : ("clip" + std::to_string(a));
        for (cgltf_size c = 0; c < anim.channels_count; ++c) {
            const cgltf_animation_channel& ch = anim.channels[c];
            if (ch.target_node == nullptr || ch.sampler == nullptr) {
                continue;
            }
            const auto it = order.index.find(ch.target_node);
            if (it == order.index.end()) {
                continue; // animates something that is not a joint of this skin
            }
            skel::AnimChannel out_ch;
            out_ch.joint = static_cast<uint32_t>(it->second);
            switch (ch.target_path) {
            case cgltf_animation_path_type_translation:
                out_ch.path = skel::AnimPath::Translation;
                break;
            case cgltf_animation_path_type_rotation:
                out_ch.path = skel::AnimPath::Rotation;
                break;
            case cgltf_animation_path_type_scale:
                out_ch.path = skel::AnimPath::Scale;
                break;
            default:
                continue; // morph weights: no consumer yet, dropped LOUDLY below
            }
            const cgltf_animation_sampler& s = *ch.sampler;
            const cgltf_size keys = s.input->count;
            const bool cubic = s.interpolation == cgltf_interpolation_type_cubic_spline;
            if (cubic) {
                // CUBICSPLINE stores (in-tangent, value, out-tangent) per key.
                // Taking the VALUE and dropping the tangents converts it to a
                // linear clip -- lossy, and said out loud rather than silently
                // read as three keys of nonsense.
                std::fprintf(stderr,
                             "[import] clip \"%s\": CUBICSPLINE channel converted "
                             "to LINEAR (tangents dropped)\n", clip.name.c_str());
            }
            const int comps = out_ch.path == skel::AnimPath::Rotation ? 4 : 3;
            out_ch.times.reserve(keys);
            out_ch.values.reserve(keys);
            for (cgltf_size k = 0; k < keys; ++k) {
                cgltf_float t = 0.0f;
                (void)cgltf_accessor_read_float(s.input, k, &t, 1);
                cgltf_float v[4] = {0, 0, 0, 1};
                const cgltf_size vi = cubic ? k * 3 + 1 : k;
                (void)cgltf_accessor_read_float(s.output, vi, v,
                                                static_cast<cgltf_size>(comps));
                out_ch.times.push_back(t);
                out_ch.values.push_back(glm::vec4{v[0], v[1], v[2], v[3]});
                clip.duration_s = std::max(clip.duration_s, t);
            }
            if (!out_ch.times.empty()) {
                clip.channels.push_back(std::move(out_ch));
            }
        }
        if (!clip.channels.empty()) {
            out.push_back(std::move(clip));
        }
    }
}

/// THE MODEL'S OWN AXES, ITS SIZE AND ITS FACING, ALL BAKED HERE.
///
/// `pre` is the transform from the file's mesh-local space to ours. It is NOT
/// a preference: half the sample assets in the world are authored Z-UP and
/// carry the conversion in a node ABOVE the skeleton root (RiggedFigure's
/// "Z_UP" node is literally named that). A reader that starts at the skin's
/// root joint never sees it and imports a character lying on its face -- which
/// is the exact defect this argument exists to prevent, because it is
/// indistinguishable from a bad rest pose until someone works out where the
/// missing rotation went.
///
/// The maths, once. With A = `pre`, root local L, inverse bind B, vertex p:
///   root local' = A * L      -> every joint's model matrix becomes A * J
///   inverse bind' = B * A^-1
///   vertex'      = A * p
/// so palette' * vertex' = (A J B A^-1)(A p) = A (J B p): the same body, in
/// our axes. At bind pose J*B = I, so the bind vertices ARE the model, which
/// is what lets the height below be measured off them.
///
/// The scale then rides on top: with every joint's local translation scaled by
/// s, a joint's model matrix becomes S*J*S^-1, so the inverse bind's
/// translation column is scaled too and the vertices with it.
void apply_pre_transform(skel::Skeleton& skeleton, SkinMesh& skin,
                         std::vector<skel::AnimClip>& clips, const glm::mat4& pre) {
    const glm::mat4 pre_inv = glm::inverse(pre);
    const glm::mat3 pre_rot{pre};
    for (skel::SkeletonJoint& j : skeleton.joints) {
        j.inverse_bind = j.inverse_bind * pre_inv;
        if (j.parent >= 0) {
            continue;
        }
        const glm::mat4 local = glm::translate(glm::mat4{1.0f}, j.bind_translation)
                                * glm::mat4_cast(j.bind_rotation)
                                * glm::scale(glm::mat4{1.0f}, j.bind_scale);
        const glm::mat4 m = pre * local;
        j.bind_translation = glm::vec3{m[3]};
        glm::mat3 basis{m};
        j.bind_scale = glm::vec3{glm::length(basis[0]), glm::length(basis[1]),
                                 glm::length(basis[2])};
        for (int c = 0; c < 3; ++c) {
            basis[c] = j.bind_scale[c] > 1e-8f ? basis[c] / j.bind_scale[c] : basis[c];
        }
        j.bind_rotation = glm::normalize(glm::quat_cast(basis));
    }
    for (dfn::platform::SkinnedVertex& v : skin.vertices) {
        v.position = glm::vec3{pre * glm::vec4{v.position, 1.0f}};
        v.normal = glm::normalize(pre_rot * v.normal);
    }
    // THE CLIPS CARRY THE ROOT'S FRAME TOO, AND THE ROTATION HALF OF THAT WAS
    // MISSING UNTIL 31.08. A clip exported from Blender keys EVERY joint,
    // root included, and a keyed rotation REPLACES the bind rotation this
    // function just rotated (skel::sample_clip: bind first, keys over it). So
    // the yaw baked into the bind pose survived exactly as long as nobody
    // played a clip: the rest pose faced -Z, and the moment Idle_Loop started
    // the figure spun back to the +Z its author had left it facing.
    //
    // Measured on HumanBase: the root's authored rotation is (-90 deg about X)
    // — the file's own Z-UP node — and `pre` is a further 180 deg about Y from
    // `--yaw 180`. The bind pose composed both; the clip re-asserted the first
    // alone, which is precisely a body walking backwards. The owner reported
    // it as "повёрнут задом наперёд" and it is the same defect the header
    // above warns about for translations, one path over.
    const glm::quat pre_q = [&] {
        glm::mat3 basis{pre};
        for (int c = 0; c < 3; ++c) {
            const float len = glm::length(basis[c]);
            basis[c] = len > 1e-8f ? basis[c] / len : basis[c];
        }
        return glm::normalize(glm::quat_cast(basis));
    }();
    for (skel::AnimClip& c : clips) {
        for (skel::AnimChannel& ch : c.channels) {
            if (ch.joint >= skeleton.joints.size()
                || skeleton.joints[ch.joint].parent >= 0) {
                continue;
            }
            if (ch.path == skel::AnimPath::Translation) {
                for (glm::vec4& v : ch.values) {
                    v = glm::vec4{glm::vec3{pre * glm::vec4{glm::vec3{v}, 1.0f}}, v.w};
                }
            } else if (ch.path == skel::AnimPath::Rotation) {
                for (glm::vec4& v : ch.values) {
                    const glm::quat q = glm::normalize(pre_q
                                                       * glm::quat{v.w, v.x, v.y, v.z});
                    v = glm::vec4{q.x, q.y, q.z, q.w};
                }
            }
        }
    }
}

void apply_scale(skel::Skeleton& skeleton, SkinMesh& skin,
                 std::vector<skel::AnimClip>& clips, float scale) {
    if (std::fabs(scale - 1.0f) < 1e-6f) {
        return;
    }
    for (skel::SkeletonJoint& j : skeleton.joints) {
        j.bind_translation *= scale;
        j.inverse_bind[3] = glm::vec4{glm::vec3{j.inverse_bind[3]} * scale,
                                      j.inverse_bind[3].w};
    }
    for (dfn::platform::SkinnedVertex& v : skin.vertices) {
        v.position *= scale;
    }
    for (skel::AnimClip& c : clips) {
        for (skel::AnimChannel& ch : c.channels) {
            if (ch.path != skel::AnimPath::Translation) {
                continue;
            }
            for (glm::vec4& v : ch.values) {
                v = glm::vec4{glm::vec3{v} * scale, v.w};
            }
        }
    }
}

/// CARRIES A CHANGE OF THE BIND POSE INTO THE CLIPS. Everything that moves a
/// joint -- the canon fit, the grounding shift -- moves the SKELETON, and a
/// clip exported from Blender keys every joint's translation and scale
/// anyway: play those keys back and the model quietly returns to the
/// proportions and the ground height it shipped with. Measured on the visible
/// character before this existed: the walk's stance ankle sat at 0.089 m (its
/// standing height, correct) but the jog's at 0.171 m -- the man ran eight
/// centimetres above the grass, and the wave's own prober called it 0.39 m of
/// foot slide because it had no other way to say "he is not touching".
///
/// TRANSLATION IS CARRIED AS AN OFFSET AND SCALE AS A RATIO, deliberately.
/// Every joint but the hips keys a CONSTANT translation (its bone offset), and
/// an offset reproduces the new offset exactly; the hips key a bob about that
/// constant, and an offset preserves the bob's amplitude instead of stretching
/// it by a fit that was about limb lengths. Scale is multiplicative by nature.
void carry_bind_change_into_clips(const skel::Skeleton& before,
                                  const skel::Skeleton& after,
                                  std::vector<skel::AnimClip>& clips) {
    const std::size_t n = std::min(before.joints.size(), after.joints.size());
    for (skel::AnimClip& c : clips) {
        for (skel::AnimChannel& ch : c.channels) {
            if (ch.joint >= n) {
                continue;
            }
            const skel::SkeletonJoint& b = before.joints[ch.joint];
            const skel::SkeletonJoint& a = after.joints[ch.joint];
            if (ch.path == skel::AnimPath::Translation) {
                const glm::vec3 d = a.bind_translation - b.bind_translation;
                for (glm::vec4& v : ch.values) {
                    v = glm::vec4{glm::vec3{v} + d, v.w};
                }
            } else if (ch.path == skel::AnimPath::Scale) {
                const glm::vec3 r{
                    std::fabs(b.bind_scale.x) > 1e-6f ? a.bind_scale.x / b.bind_scale.x : 1.0f,
                    std::fabs(b.bind_scale.y) > 1e-6f ? a.bind_scale.y / b.bind_scale.y : 1.0f,
                    std::fabs(b.bind_scale.z) > 1e-6f ? a.bind_scale.z / b.bind_scale.z : 1.0f};
                for (glm::vec4& v : ch.values) {
                    v = glm::vec4{glm::vec3{v} * r, v.w};
                }
            }
        }
    }
}

/// The mesh's vertical extent IN ITS REST POSE. Not the stored bind vertices:
/// once a segment has been rescaled the palette at rest is no longer the
/// identity, and the stored vertices describe the model as its author shipped
/// it -- the one state the fit exists to leave behind.
void rest_extent(const skel::Skeleton& skeleton, const SkinMesh& skin, float& lo,
                 float& hi) {
    // ОДНА РЕСТ-ПОЗА НА ТЕЛО (RestFit.h): решённая по коже — та, в которой
    // тело стоит на экране и в мире, и ровно её подошвы заземляются здесь.
    const dfn::anim::Rig rig = dfn::anim::rest_rig_for(skeleton, skin.vertices);
    const dfn::anim::SkinnedRigBinding sb = dfn::anim::bind_skinned_rig(rig, skeleton);
    std::vector<glm::mat4> palette(skeleton.size());
    dfn::anim::skinning_palette(rig, skeleton, sb, dfn::anim::LocalPose{}, palette);
    for (std::size_t i = 0; i < skin.vertices.size(); ++i) {
        const float y = dfn::anim::cpu_skin_position(skin.vertices[i], palette).y;
        lo = i == 0 ? y : std::min(lo, y);
        hi = i == 0 ? y : std::max(hi, y);
    }
}

/// SEGMENT-BY-SEGMENT FIT TO THE CANON (owner's order 30.08: "пропорции тела
/// делать сразу"; the canon is docs/design/HUMAN_SCALE.md, whose fractions are
/// exactly the BODY_*_FRAC rows this reads through RigProportions).
///
/// WHAT IT DOES, AND WHY IT WORKS AT ALL. Every segment of the rig -- hip to
/// neck, shoulder to elbow, elbow to wrist, hip to knee, knee to ankle -- is
/// measured on the imported skeleton IN OUR REST POSE and its joint
/// translations are scaled until the segment is the canonical length. The
/// INVERSE BIND matrices are deliberately left alone, so the skinning palette
/// at rest is no longer the identity and linear blend skinning carries the
/// mesh with the moved joints. That is the whole trick, and it is what a
/// proportion slider does in a character creator -- no Blender, no morph
/// targets, no second mesh.
///
/// IN METRES, ABSOLUTELY, AND NOT AS FRACTIONS OF THE MODEL'S OWN HEIGHT.
/// The first version wrote every target as canon_fraction x (this model's
/// height) and iterated, because the height is itself a result of the lengths
/// being fitted. That is a fixed-point iteration whose map is not a
/// contraction: it settled 6.8 % short on five landmarks at best, and once
/// joints were read in the rest frame it DIVERGED to a skeleton 6.6e12 metres
/// tall. The cure was not damping. It was noticing that the target does not
/// depend on the model at all: our rig's canon lengths are metres, the rest
/// pose's DIRECTIONS come from our rig anyway, so each segment can simply be
/// told what to be. One pass, no feedback, nothing to diverge.
///
/// WHAT IT CANNOT DO, said out loud. It moves JOINTS; it cannot reshape a
/// skull. The head's own height and the ankle's height above the sole are
/// fitted by SCALING those joints, which scales every vertex weighted to them
/// -- honest for +-20 %, visibly wrong past that. And it assumes the model is
/// humanoid to begin with: run it on a chibi and you get a chibi with long
/// thin limbs, which is why the visible character is chosen by the judge and
/// not by the fit.
void fit_to_canon(skel::Skeleton& skeleton, const SkinMesh& skin,
                  const dfn::anim::RigProportions& canon) {
    using dfn::anim::Bone;
    using dfn::anim::bone_index;
    const dfn::anim::SkeletonBinding bind = dfn::anim::bind_skeleton(skeleton);
    if (bind.bound_count < dfn::anim::BONE_COUNT) {
        std::fprintf(stderr, "[import] --fit-canon needs all 15 bones bound "
                             "(%u are) -- SKIPPED\n", bind.bound_count);
        return;
    }
    const float shoulder_dx = canon.shoulder_width * 0.5f;
    const float shoulder_dy = canon.shoulder_height - canon.hip_height;
    struct Seg {
        Bone bone;
        Bone parent;
        float canon_len; // METRES
    };
    // THE TRUNK HANGS OFF THE **PELVIS**, NOT THE TORSO, and that is a fact
    // about our rig rather than a shortcut: docs/RIG.md puts the Torso bone's
    // proximal joint at the HIP CENTRE -- the same point as the Pelvis. An
    // imported spine's first joint sits well above the hips, so measuring
    // hip-to-neck from it and calling the result `torso_length` stretched the
    // trunk by 0.16 m and made the whole figure 1.926 m instead of 1.800.
    const Seg segs[] = {
        {Bone::Head, Bone::Pelvis, canon.torso_length()},
        {Bone::UpperArmL, Bone::Pelvis,
         std::sqrt(shoulder_dx * shoulder_dx + shoulder_dy * shoulder_dy)},
        {Bone::UpperArmR, Bone::Pelvis,
         std::sqrt(shoulder_dx * shoulder_dx + shoulder_dy * shoulder_dy)},
        {Bone::ForearmL, Bone::UpperArmL, canon.upper_arm_length},
        {Bone::ForearmR, Bone::UpperArmR, canon.upper_arm_length},
        {Bone::HandL, Bone::ForearmL, canon.forearm_length},
        {Bone::HandR, Bone::ForearmR, canon.forearm_length},
        {Bone::ThighL, Bone::Pelvis, canon.hip_width * 0.5f},
        {Bone::ThighR, Bone::Pelvis, canon.hip_width * 0.5f},
        {Bone::ShinL, Bone::ThighL, canon.thigh_length()},
        {Bone::ShinR, Bone::ThighR, canon.thigh_length()},
        {Bone::FootL, Bone::ShinL, canon.shin_length()},
        {Bone::FootR, Bone::ShinR, canon.shin_length()},
    };
    std::vector<glm::mat4> model(skeleton.size());
    const auto refresh = [&] {
        // JOINTS ARE READ IN THE **REST** FRAME, not the bind frame, and the
        // two differ the moment a model binds in a T-pose: bind-pose shoulders
        // sit where the arms were bound, not where the figure stands.
        const dfn::anim::Rig r = dfn::anim::rest_rig_for(skeleton, skin.vertices);
        const dfn::anim::SkinnedRigBinding sb =
            dfn::anim::bind_skinned_rig(r, skeleton);
        dfn::anim::rest_model_matrices(r, skeleton, sb, dfn::anim::LocalPose{}, model);
    };
    /// Only joints on the chain from `parent` (exclusive) down to `child`
    /// (inclusive) belong to the segment's length: an intermediate the rig has
    /// no bone for -- a three-link spine, a clavicle -- does; a cousin does not.
    const auto on_chain = [&](int32_t j, int32_t child, int32_t parent) {
        for (int32_t a = child; a > parent && a >= 0;
             a = skeleton.joints[static_cast<std::size_t>(a)].parent) {
            if (a == j) {
                return true;
            }
        }
        return false;
    };

    // TWO ROUNDS, and only because the head and foot SCALES change what the
    // sole and the crown are, which the pelvis placement then reads. The
    // segment lengths themselves are exact after the first.
    for (int round = 0; round < 2; ++round) {
        refresh();
        for (const Seg& seg : segs) {
            const int32_t child = bind.joint[bone_index(seg.bone)];
            const int32_t parent = bind.joint[bone_index(seg.parent)];
            if (child < 0 || parent < 0) {
                continue;
            }
            const float measured =
                glm::length(glm::vec3{model[static_cast<std::size_t>(child)][3]}
                            - glm::vec3{model[static_cast<std::size_t>(parent)][3]});
            if (measured < 1e-5f || seg.canon_len < 1e-5f) {
                continue;
            }
            const float ratio = seg.canon_len / measured;
            for (int32_t j = child; j > parent; --j) {
                if (on_chain(j, child, parent)) {
                    skeleton.joints[static_cast<std::size_t>(j)].bind_translation *=
                        ratio;
                }
            }
            refresh();
        }

        // THE FOOT IS SCALED so the ANKLE sits the canon height above the sole,
        // and THE HEAD so the crown sits the canon head-height above the neck.
        // Both are mesh properties, and a joint's only handle on a mesh is its
        // scale. Both read the REST-POSED mesh, because that is the body.
        for (int fix = 0; fix < 2; ++fix) {
            float rlo = 0.0f;
            float rhi = 0.0f;
            rest_extent(skeleton, skin, rlo, rhi);
            refresh();
            for (const Bone foot : {Bone::FootL, Bone::FootR}) {
                const int32_t j = bind.joint[bone_index(foot)];
                if (j < 0) {
                    continue;
                }
                const float have = model[static_cast<std::size_t>(j)][3][1] - rlo;
                if (have > 1e-4f && canon.ankle_height > 1e-4f) {
                    skeleton.joints[static_cast<std::size_t>(j)].bind_scale *=
                        canon.ankle_height / have;
                }
            }
            rest_extent(skeleton, skin, rlo, rhi);
            refresh();
            const int32_t head = bind.joint[bone_index(Bone::Head)];
            if (head >= 0) {
                const float have = rhi - model[static_cast<std::size_t>(head)][3][1];
                if (have > 1e-4f && canon.head_height > 1e-4f) {
                    skeleton.joints[static_cast<std::size_t>(head)].bind_scale *=
                        canon.head_height / have;
                }
            }
        }

        // THE SHOULDERS ARE PLACED SIDEWAYS, NOT SCALED. Their distance from
        // the spine is a WIDTH, and scaling a segment that runs mostly upward
        // cannot change a width: on the reference base the segment fit left
        // the span 19.7 % narrow while every length it touched was exact.
        for (const Bone arm : {Bone::UpperArmL, Bone::UpperArmR}) {
            const int32_t j = bind.joint[bone_index(arm)];
            const int32_t torso = bind.joint[bone_index(Bone::Pelvis)];
            if (j < 0 || torso < 0) {
                continue;
            }
            refresh();
            // THE OFFSET IS WRITTEN IN MODEL SPACE AND BROUGHT BACK INTO THE
            // PARENT'S FRAME. A Rigify clavicle's frame is tilted, so adding
            // the delta straight into `bind_translation.x` moves the shoulder
            // ALONG THE CLAVICLE, not sideways: measured cost of getting it
            // wrong was a span that went from 19.7 % narrow to 42.5 % narrow
            // while the code read as a fix.
            const std::size_t ji = static_cast<std::size_t>(j);
            const int32_t parent = skeleton.joints[ji].parent;
            const float axis_x = model[static_cast<std::size_t>(torso)][3][0];
            const glm::vec3 have_p = glm::vec3{model[ji][3]};
            const float have = have_p.x - axis_x;
            const float want =
                std::copysign(shoulder_dx, have != 0.0f ? have : 1.0f);
            const glm::vec3 target{axis_x + want, have_p.y, have_p.z};
            skeleton.joints[ji].bind_translation =
                parent >= 0
                    ? glm::vec3{glm::inverse(model[static_cast<std::size_t>(parent)])
                                * glm::vec4{target, 1.0f}}
                    : target;
        }
    }
}

/// THE FIGURE ITSELF (owner's verdict 31.08: "чрезмерно перекачен, слишком
/// длинные руки, живота нет, всё кривенько — норм для файтинга, не для RPG").
///
/// WHY --fit-canon WAS NOT ENOUGH, and this is the whole reason the pass
/// exists. The fit moves JOINTS, and on this model every joint landmark was
/// already green inside 2 % while all four complaints were true of it. A
/// joint has no radius, so it cannot be too muscular; the rig's last arm joint
/// is the WRIST, so 23 cm of hand hung past everything the fit could reach;
/// and a belly is the depth of the trunk BETWEEN two joints. Measured before
/// this pass: upper arm 61.6 % thicker than the canon, thigh 35.2 %, hips
/// 29.6 % wider, hand 22.1 % longer, arm span 1.20H against a target of 1.03H.
///
/// IT DEFORMS THE SKIN, IN THE REST POSE, AND WRITES THE RESULT BACK THROUGH
/// THE SKINNING. Each vertex is skinned to its rest position, moved there, and
/// carried back into bind space through the INVERSE of its own blended matrix,
/// so the file stays one skinned mesh with the file's own inverse binds and
/// every clip keeps playing. There is no morph target, no second mesh and no
/// Blender in this path.
///
/// NOTHING HERE IS A TASTE KNOB. Every factor is a RATIO OF TWO MEASUREMENTS:
/// what the canon says the body should be, over what this model measures. Run
/// it on a thin model and it would fatten the arms by the same code. That is
/// deliberate and it is what keeps the pass out of the "artist tweaks numbers
/// until the screenshot looks right" trade, which no instrument can check.
///
/// CONTINUITY IS A PROPERTY OF THE CONSTRUCTION, NOT A CLEANUP AFTERWARDS.
/// The displacement of a vertex is a weighted sum over its own skinning
/// weights (limbs) plus a field that reads only its rest POSITION (trunk).
/// Both are continuous across the mesh because the weights are, so a seam
/// between two primitives cannot open: two vertices at the same place with the
/// same weights get the same displacement. The welding pass below then makes
/// that exact rather than nearly exact, and MEASURES the gap it closed.
void reshape_to_commoner(skel::Skeleton& skeleton, SkinMesh& skin,
                         const dfn::anim::RigProportions& canon, bool trace) {
    using dfn::anim::Bone;
    using dfn::anim::bone_index;
    const dfn::anim::SkeletonBinding bind = dfn::anim::bind_skeleton(skeleton);
    if (bind.bound_count < dfn::anim::BONE_COUNT) {
        std::fprintf(stderr, "[import] --reshape needs all 15 bones bound "
                             "(%u are) -- SKIPPED\n", bind.bound_count);
        return;
    }
    const std::size_t n = skeleton.size();
    const std::size_t vn = skin.vertices.size();
    const dfn::anim::Rig rig = dfn::anim::rest_rig_for(skeleton, skin.vertices);
    const dfn::anim::SkinnedRigBinding sb = dfn::anim::bind_skinned_rig(rig, skeleton);
    std::vector<glm::mat4> palette(n);
    std::vector<glm::mat4> model(n);
    dfn::anim::skinning_palette(rig, skeleton, sb, dfn::anim::LocalPose{}, palette);
    dfn::anim::rest_model_matrices(rig, skeleton, sb, dfn::anim::LocalPose{}, model);

    // THE PER-VERTEX BLENDED MATRIX, kept because it is needed TWICE: once
    // forward to reach the rest pose and once inverted to come back. Rebuilding
    // it the second time from the same weights would be two copies of the
    // blend, and the two would drift the day someone changes the influence
    // count.
    std::vector<glm::mat4> vmat(vn);
    std::vector<glm::vec3> rest(vn);
    for (std::size_t i = 0; i < vn; ++i) {
        const auto& v = skin.vertices[i];
        glm::mat4 m{0.0f};
        for (int k = 0; k < 4; ++k) {
            const std::size_t j = v.joints[k];
            if (v.weights[k] > 0.0f && j < n) {
                m += palette[j] * v.weights[k];
            }
        }
        // A vertex with no weight at all rides the identity rather than a zero
        // matrix, which would collapse it onto the origin -- silently, and only
        // for the handful of vertices an exporter forgot.
        if (std::fabs(m[3][3]) < 1e-6f) {
            m = glm::mat4{1.0f};
        }
        vmat[i] = m;
        rest[i] = glm::vec3{m * glm::vec4{v.position, 1.0f}};
    }
    float lo = rest.empty() ? 0.0f : rest[0].y;
    float hi = lo;
    for (const glm::vec3& r : rest) {
        lo = std::min(lo, r.y);
        hi = std::max(hi, r.y);
    }
    const float height = hi - lo;
    if (height < 1e-3f) {
        std::fprintf(stderr, "[import] --reshape: the model has no height -- SKIPPED\n");
        return;
    }

    // Body part of every joint: its nearest bound ancestor, itself included.
    std::vector<int> bone_of_joint(n, -1);
    for (uint32_t b = 0; b < dfn::anim::BONE_COUNT; ++b) {
        const int32_t j = bind.joint[b];
        if (j >= 0) {
            bone_of_joint[static_cast<std::size_t>(j)] = static_cast<int>(b);
        }
    }
    for (std::size_t j = 0; j < n; ++j) {
        if (bone_of_joint[j] < 0) {
            const int32_t par = skeleton.joints[j].parent;
            if (par >= 0) {
                bone_of_joint[j] = bone_of_joint[static_cast<std::size_t>(par)];
            }
        }
    }
    const auto bi = [](Bone b) { return static_cast<int>(bone_index(b)); };
    const auto jpos = [&](Bone b) {
        const int32_t j = bind.joint[bone_index(b)];
        return j >= 0 ? glm::vec3{model[static_cast<std::size_t>(j)][3]} : glm::vec3{0.0f};
    };
    std::vector<int> vbone(vn, -1);
    for (std::size_t i = 0; i < vn; ++i) {
        const auto& v = skin.vertices[i];
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

    // WHICH WAY IT FACES, TAKEN FROM THE TOES. --yaw is what the operator
    // claimed; the toe is what the body says. A belly put on the wrong side is
    // a hunchback, and no test of a number would catch it.
    float facing = -1.0f;
    {
        const glm::vec3 ank = jpos(Bone::FootL);
        float far_neg = 0.0f;
        float far_pos = 0.0f;
        for (std::size_t i = 0; i < vn; ++i) {
            if (vbone[i] != bi(Bone::FootL) && vbone[i] != bi(Bone::FootR)) {
                continue;
            }
            far_neg = std::min(far_neg, rest[i].z - ank.z);
            far_pos = std::max(far_pos, rest[i].z - ank.z);
        }
        if (far_pos > -far_neg) {
            facing = 1.0f;
        }
    }
    const glm::vec3 fwd{0.0f, 0.0f, facing};

    // --- what this model measures, in metres -----------------------------
    const auto limb_radius = [&](Bone bone, Bone child) {
        const glm::vec3 a = jpos(bone);
        const glm::vec3 u = jpos(child) - a;
        const float len = glm::length(u);
        if (len < 1e-4f) {
            return 0.0f;
        }
        const glm::vec3 e = u / len;
        float best = 0.0f;
        for (std::size_t i = 0; i < vn; ++i) {
            if (vbone[i] != bi(bone)) {
                continue;
            }
            const glm::vec3 d = rest[i] - a;
            const float t = glm::dot(d, e) / len;
            if (t < 0.3f || t > 0.7f) {
                continue;
            }
            best = std::max(best, glm::length(d - e * (t * len)));
        }
        return best;
    };
    const float arm_r = limb_radius(Bone::UpperArmL, Bone::ForearmL);
    const float leg_r = limb_radius(Bone::ThighL, Bone::ShinL);
    float hand_len = 0.0f;
    {
        const glm::vec3 w = jpos(Bone::HandL);
        for (std::size_t i = 0; i < vn; ++i) {
            if (vbone[i] == bi(Bone::HandL)) {
                hand_len = std::max(hand_len, glm::length(rest[i] - w));
            }
        }
    }

    // THE SPINE LINE, interpolated by height between the hip and the neck
    // joints. Everything about the trunk is measured FROM IT and not from
    // x = 0: a model whose author left the body off the origin would otherwise
    // grow a belly that is only an offset.
    const glm::vec3 hip_j = jpos(Bone::Pelvis);
    const glm::vec3 neck_j = jpos(Bone::Head);
    const auto spine_at = [&](float y) {
        const float t = (y - hip_j.y) / std::max(1e-4f, neck_j.y - hip_j.y);
        return hip_j + (neck_j - hip_j) * t;
    };
    const auto is_arm = [&](int b) {
        return b == bi(Bone::UpperArmL) || b == bi(Bone::UpperArmR)
               || b == bi(Bone::ForearmL) || b == bi(Bone::ForearmR)
               || b == bi(Bone::HandL) || b == bi(Bone::HandR);
    };
    /// Half-width, forward reach and backward reach of the BODY in a band
    /// around a height, all from the spine line and with the ARMS LEFT OUT: at
    /// hip height a hanging hand is beside the pelvis, and counting it as hip
    /// breadth is how a model measures 44 cm across the hips.
    ///
    /// THE LEGS ARE **IN**, and that is not sloppiness. `BODY_HIP_WIDTH_FRAC`
    /// is a breadth ACROSS THE TROCHANTERS, and in any rig the flesh over a
    /// trochanter is weighted to the thigh -- measure only the pelvis and the
    /// number comes out 25 cm on a body that is 43 cm across, then the
    /// correction widens the hips it was asked to narrow. The instrument and
    /// the correction have to be reading the SAME body.
    struct Slice {
        float half_w = 0.0f;
        float front = 0.0f;
        float back = 0.0f;
    };
    const auto slice_at = [&](float yfrac) {
        Slice s;
        const float y = lo + yfrac * height;
        // A NARROW BAND, AND THAT IS THE WHOLE CORRECTION AT THE CHEST. The
        // first version sampled +-0.02H, i.e. a 7 cm slab, and took its
        // EXTREME as "the body at this height" -- then applied the resulting
        // factor to every vertex in the slab, including the ones that were
        // extreme only at its edge. Measured cost: the chest band the judge
        // reads came out 8.8 % over the canon while every factor the pass
        // printed looked right, because the deepest millimetre of the ribcage
        // sat in one sample's skirt and got its neighbour's number. Half a
        // centimetre of half-band, sampled densely, makes each factor local.
        const float band = 0.020f * height;
        for (std::size_t i = 0; i < vn; ++i) {
            if (std::fabs(rest[i].y - y) > band || is_arm(vbone[i])) {
                continue;
            }
            const glm::vec3 d = rest[i] - spine_at(rest[i].y);
            const float fz = glm::dot(d, fwd);
            s.half_w = std::max(s.half_w, std::fabs(d.x));
            s.front = std::max(s.front, fz);
            s.back = std::max(s.back, -fz);
        }
        return s;
    };
    const auto HF = [](double frac) { return static_cast<float>(frac); };
    const float hipf = HF(dfn::config::BODY_HIP_HEIGHT_FRAC);
    const float navf = HF(dfn::config::BODY_NAVEL_HEIGHT_FRAC);
    const float chef = HF(dfn::config::BODY_CHEST_HEIGHT_FRAC);
    const float nekf = HF(dfn::config::BODY_NECK_HEIGHT_FRAC);
    // BELOW THE CROTCH THE FIELD LETS GO. It gates on "not an arm" rather than
    // "is trunk" (see the slice above), so without this the hip correction
    // would run all the way down both legs and squeeze the ankles by a factor
    // meant for the pelvis.
    const float crotchf = hipf - 0.09f;

    // --- what the canon asks for, in metres ------------------------------
    const float H = static_cast<float>(dfn::config::PLAYER_CAPSULE_HEIGHT);
    const float want_hip_w = canon.hip_width;
    const float want_nav_w = H * HF(dfn::config::BODY_WAIST_WIDTH_FRAC);
    const float want_nav_d = H * HF(dfn::config::BODY_WAIST_DEPTH_FRAC);
    const float want_che_w = H * HF(dfn::config::BODY_CHEST_WIDTH_FRAC);
    const float want_che_d = canon.torso_depth;

    // A RATIO IS CLAMPED, NOT TRUSTED. Every one of them is a quotient whose
    // denominator this model supplies, and a model with a degenerate part
    // (a hand welded into the forearm, a trunk two vertices wide) would
    // otherwise ask for a factor of forty. The band is generous enough that a
    // real correction is never touched and narrow enough that a broken one
    // cannot destroy the mesh -- and it says so out loud when it bites.
    int clamped = 0;
    const auto ratio = [&](const char* what, float want, float have) {
        if (have < 1e-4f || want < 1e-4f) {
            return 1.0f;
        }
        const float r = want / have;
        const float c = std::clamp(r, 0.55f, 1.80f);
        if (std::fabs(c - r) > 1e-4f) {
            ++clamped;
            std::fprintf(stderr, "[import] --reshape: %s wanted x%.2f, CLAMPED to "
                                 "x%.2f -- this model is not the shape the pass "
                                 "assumes\n",
                         what, static_cast<double>(r), static_cast<double>(c));
        }
        return c;
    };
    // The arm's ratio drives the FOREARM and the HAND too, and the leg's the
    // SHIN. One number per limb, not three: the canon has a row for the upper
    // arm's thickness and none for the forearm's, and inventing two more would
    // ALSO destroy the taper this model draws between them. Scaling a limb by
    // its own arm's correction removes the muscle and keeps the shape.
    const float k_arm = ratio("upper arm", canon.arm_thickness * 0.5f, arm_r);
    const float k_leg = ratio("thigh", canon.leg_thickness * 0.5f, leg_r);
    const float k_hand_long = ratio("hand length", canon.hand_length, hand_len);

    // --- THE TRUNK'S FACTORS, RESOLVED AT EVERY HEIGHT ---------------------
    //
    // ONE FACTOR PER HEIGHT, EACH FROM THAT HEIGHT'S OWN MEASUREMENT. The
    // first version measured the body at three heights, worked out three
    // factors and interpolated BETWEEN THE FACTORS. That is not the same
    // thing, and the difference showed: the chest came out 4.5 % deeper than
    // the canon because vertices just below it carried a factor mixed from the
    // navel's, and the belly -- whose whole point is to be deeper than the
    // chest -- ended up 0.99 of it instead of 1.04. A factor is a quotient of
    // a target by a measurement, and both of them vary with height, so the
    // quotient has to be formed at the height where it is applied.
    //
    // THE TARGETS ARE THE REGISTRY'S; THE MEASUREMENTS ARE THIS MODEL'S.
    // Between the hip and the navel and between the navel and the chest the
    // target is a straight line between two rows. Below the hip it eases into
    // the model's own body, so the field releases at the crotch. Above the
    // chest the FACTOR is held and eased to 1 by the neck -- held rather than
    // extrapolated, because the canon has no row for the trunk's breadth at
    // the shoulder and a straight line drawn past its last point is a guess
    // wearing arithmetic.
    constexpr int TRUNK_SAMPLES = 65;
    std::array<float, TRUNK_SAMPLES> kw_at{};
    std::array<float, TRUNK_SAMPLES> kf_at{};
    const auto sample_frac = [&](int i) {
        return crotchf
               + (nekf - crotchf) * static_cast<float>(i)
                     / static_cast<float>(TRUNK_SAMPLES - 1);
    };
    const auto mix = [](float a, float b, float t) { return a + (b - a) * t; };
    const auto smooth = [](float t) {
        const float c = std::clamp(t, 0.0f, 1.0f);
        return c * c * (3.0f - 2.0f * c);
    };
    float k_che_w_held = 1.0f;
    for (int i = 0; i < TRUNK_SAMPLES; ++i) {
        const float f = sample_frac(i);
        const Slice sl = slice_at(f);
        const float have_w = sl.half_w * 2.0f;
        // ABOVE THE CHEST THE TWO HALVES PART COMPANY, and each has its own
        // reason. BREADTH: the canon's last row is the chest's, and the trunk
        // genuinely goes on widening into the shoulder girdle above it, so the
        // FACTOR is held -- a straight line drawn past the last row would
        // squeeze the shoulders by a quarter. DEPTH: the ribcage is deepest at
        // the nipple line and does not get deeper towards the collarbone, so
        // the chest-depth row is a valid CEILING all the way to the armpit and
        // the target is carried up unchanged. Holding the depth factor instead
        // (the first version did) leaves the top half of the very band the
        // judge reads as "chest depth" uncorrected: measured cost was a chest
        // 8.8 % over the canon and a belly that came out SHALLOWER than it,
        // which is the one thing this pass exists to prevent.
        const bool above_chest = f > chef;
        float want_w = 0.0f;
        if (above_chest) {
            want_w = have_w * k_che_w_held;
        } else if (f <= hipf) {
            want_w = mix(have_w, want_hip_w, smooth((f - crotchf) / (hipf - crotchf)));
        } else if (f <= navf) {
            want_w = mix(want_hip_w, want_nav_w, (f - hipf) / (navf - hipf));
        } else {
            want_w = mix(want_nav_w, want_che_w, (f - navf) / (chef - navf));
        }
        const float kw = ratio("trunk breadth", want_w, have_w);
        // THE BACK RIDES WITH THE RIBCAGE AND THE BELLY GROWS IN FRONT. Not a
        // stylistic choice: a waist that thickens by pushing the SPINE
        // backwards is a hunchback, and the back of a standing man is nearly
        // flat from the shoulder blades to the sacrum whatever he weighs. So
        // the backward reach takes the same factor as the width -- both are
        // the skeleton underneath -- and the whole of the depth correction is
        // spent on the front.
        float kf = kw;
        if (above_chest) {
            kf = ratio("trunk front",
                       std::max(0.02f, want_che_d - sl.back * kw), sl.front);
        } else if (f > navf) {
            const float want_d = mix(want_nav_d, want_che_d, (f - navf) / (chef - navf));
            kf = ratio("trunk front",
                       std::max(0.02f, want_d - sl.back * kw), sl.front);
        } else if (f > hipf) {
            // Between the hip and the navel the canon has a depth row at ONE
            // end only, so the belly is grown towards it and the hip is left
            // to the width's own factor.
            const float t = (f - hipf) / (navf - hipf);
            const float want_d = mix((sl.front + sl.back) * kw, want_nav_d, smooth(t));
            kf = ratio("trunk front",
                       std::max(0.02f, want_d - sl.back * kw), sl.front);
        }
        if (trace) {
            std::fprintf(stderr, "  [trace] f=%.3f w=%.3f front=%.3f back=%.3f "
                                 "-> kw=%.3f kf=%.3f\n",
                         static_cast<double>(f), static_cast<double>(have_w),
                         static_cast<double>(sl.front), static_cast<double>(sl.back),
                         static_cast<double>(kw), static_cast<double>(kf));
        }
        kw_at[static_cast<std::size_t>(i)] = kw;
        kf_at[static_cast<std::size_t>(i)] = kf;
        if (!above_chest) {
            k_che_w_held = kw;
        }
    }
    /// The two trunk factors at a height, read out of the table above and
    /// eased to 1 between the shoulder and the neck so the throat and the head
    /// are never touched.
    const auto trunk_k = [&](float y, float& kw, float& kf, float& kb) {
        const float f = (y - lo) / height;
        if (f <= crotchf) {
            kw = 1.0f;
            kf = 1.0f;
            kb = 1.0f;
            return;
        }
        const float u = std::clamp((f - crotchf) / (nekf - crotchf), 0.0f, 1.0f)
                        * static_cast<float>(TRUNK_SAMPLES - 1);
        const auto i0 = static_cast<std::size_t>(u);
        const std::size_t i1 = std::min<std::size_t>(i0 + 1, TRUNK_SAMPLES - 1);
        const float t = u - static_cast<float>(i0);
        kw = mix(kw_at[i0], kw_at[i1], t);
        kf = mix(kf_at[i0], kf_at[i1], t);
        if (f > HF(dfn::config::BODY_SHOULDER_HEIGHT_FRAC)) {
            const float e = smooth((f - HF(dfn::config::BODY_SHOULDER_HEIGHT_FRAC))
                                   / std::max(1e-4f,
                                              nekf - HF(dfn::config::BODY_SHOULDER_HEIGHT_FRAC)));
            kw = mix(kw, 1.0f, e);
            kf = mix(kf, 1.0f, e);
        }
        kb = kw; // the back is bone, and bone does not follow the belly
    };

    {
        float hw = 1.0f;
        float hf = 1.0f;
        float nw = 1.0f;
        float nf = 1.0f;
        float cw = 1.0f;
        float cf = 1.0f;
        float ignore = 1.0f;
        trunk_k(lo + hipf * height, hw, hf, ignore);
        trunk_k(lo + navf * height, nw, nf, ignore);
        trunk_k(lo + chef * height, cw, cf, ignore);
        std::printf("[import] reshape: arm x%.2f, leg x%.2f, hand length x%.2f | "
                    "trunk breadth hips x%.2f navel x%.2f chest x%.2f | front "
                    "navel x%.2f chest x%.2f%s\n",
                    static_cast<double>(k_arm), static_cast<double>(k_leg),
                    static_cast<double>(k_hand_long), static_cast<double>(hw),
                    static_cast<double>(nw), static_cast<double>(cw),
                    static_cast<double>(nf), static_cast<double>(cf),
                    clamped > 0 ? " (some CLAMPED, see above)" : "");
    }

    // --- the displacement -------------------------------------------------
    // LIMBS: a radial scale about the bone's own axis, summed over the
    // vertex's own weights. TRUNK: a field of the rest POSITION alone, gated
    // by how much of the vertex belongs to the trunk. Both are continuous, and
    // that is the entire seam story.
    std::vector<glm::vec3> target(vn);
    for (std::size_t i = 0; i < vn; ++i) {
        const auto& v = skin.vertices[i];
        const glm::vec3 r = rest[i];
        glm::vec3 d{0.0f};
        float w_arm = 0.0f;
        for (int k = 0; k < 4; ++k) {
            const std::size_t j = v.joints[k];
            const float w = v.weights[k];
            if (w <= 0.0f || j >= n) {
                continue;
            }
            const int b = bone_of_joint[j];
            if (is_arm(b)) {
                w_arm += w;
            }
            float k_rad = 1.0f;
            float k_along = 1.0f;
            if (b == bi(Bone::UpperArmL) || b == bi(Bone::UpperArmR)
                || b == bi(Bone::ForearmL) || b == bi(Bone::ForearmR)) {
                k_rad = k_arm;
            } else if (b == bi(Bone::HandL) || b == bi(Bone::HandR)) {
                k_rad = k_arm;
                k_along = k_hand_long;
            } else if (b == bi(Bone::ThighL) || b == bi(Bone::ThighR)
                       || b == bi(Bone::ShinL) || b == bi(Bone::ShinR)) {
                k_rad = k_leg;
            } else {
                continue; // head, feet: the canon fit already sized them
            }
            // THE HAND IS ONE PIECE, MEASURED FROM THE WRIST. This model
            // carries fifteen finger joints under each hand, and shrinking
            // each phalanx about its OWN joint shortens the bones while
            // leaving them exactly where they were -- the first run of this
            // pass did that and moved the hand by 3 mm out of the 37 it owed.
            // What makes a hand shorter is the whole of it sliding back
            // towards the wrist, so every hand vertex, finger or palm, is
            // scaled about the WRIST along the FOREARM's direction.
            const bool hand = b == bi(Bone::HandL) || b == bi(Bone::HandR);
            const bool left = b == bi(Bone::HandL);
            const glm::vec3 p = hand ? jpos(left ? Bone::HandL : Bone::HandR)
                                     : glm::vec3{model[j][3]};
            glm::vec3 axis{0.0f};
            if (hand) {
                axis = p - jpos(left ? Bone::ForearmL : Bone::ForearmR);
            }
            for (std::size_t c = 0; c < n && !hand; ++c) {
                if (skeleton.joints[c].parent == static_cast<int32_t>(j)) {
                    axis = glm::vec3{model[c][3]} - p;
                    break;
                }
            }
            if (glm::length(axis) < 1e-5f && !hand) {
                const int32_t par = skeleton.joints[j].parent;
                axis = par >= 0
                           ? p - glm::vec3{model[static_cast<std::size_t>(par)][3]}
                           : glm::vec3{0.0f, -1.0f, 0.0f};
            }
            const float alen = glm::length(axis);
            if (alen < 1e-5f) {
                continue;
            }
            const glm::vec3 e = axis / alen;
            const glm::vec3 off = r - p;
            const float along = glm::dot(off, e);
            const glm::vec3 perp = off - e * along;
            d += w * (perp * (k_rad - 1.0f) + e * (along * (k_along - 1.0f)));
        }
        // HOW MUCH OF THIS VERTEX IS TRUNK, and the shape of this one line is
        // worth its comment. The obvious answer, 1 - w_arm, is wrong in a way
        // that hides: the deepest vertices of the chest sit by the ARMPIT and
        // routinely carry half their weight on the upper arm, so they got half
        // a correction -- while the instrument, which partitions the body by
        // the HEAVIEST bone, counted every one of them as trunk and duly
        // reported the chest 11 % over the canon with every printed factor
        // looking right. The gate below agrees with the instrument (a vertex
        // mostly on the trunk is trunk) without becoming a step: it is a
        // smoothstep in the arm weight, so it is still a continuous function
        // of a continuous field and a seam still cannot open.
        const float t_arm = std::clamp((w_arm - 0.35f) / 0.50f, 0.0f, 1.0f);
        const float w_body = 1.0f - t_arm * t_arm * (3.0f - 2.0f * t_arm);
        if (w_body > 0.0f) {
            float kw = 1.0f;
            float kf = 1.0f;
            float kb = 1.0f;
            trunk_k(r.y, kw, kf, kb);
            const glm::vec3 off = r - spine_at(r.y);
            const float fz = glm::dot(off, fwd);
            // The front/back split is smoothed over a 2 cm band about the
            // spine plane, so the change of factor is a slope and not a crease
            // down the middle of the body.
            const float t = std::clamp(fz / 0.02f * 0.5f + 0.5f, 0.0f, 1.0f);
            const float e = t * t * (3.0f - 2.0f * t);
            const float kz = kb + (kf - kb) * e;
            d += w_body * glm::vec3{off.x * (kw - 1.0f), 0.0f, 0.0f};
            d += w_body * fwd * (fz * (kz - 1.0f));
        }
        target[i] = r + d;
    }

    // --- welding: the seam, made exact and MEASURED -----------------------
    // Two vertices at the same place in different primitives (this model ships
    // two, a knight ships six) are the same point of the same body. The field
    // above already moves them almost identically; "almost" is a hairline of
    // background between the arm and the sleeve, and it is exactly the kind of
    // defect that is invisible until it is in a frame. Averaging the group's
    // target closes it, and the largest gap closed is printed because a weld
    // that silently did nothing and a weld that silently saved the model look
    // the same from outside.
    {
        std::map<std::array<int64_t, 3>, std::vector<std::size_t>> groups;
        for (std::size_t i = 0; i < vn; ++i) {
            const glm::vec3 p = rest[i];
            groups[{static_cast<int64_t>(std::llround(p.x * 20000.0f)),
                    static_cast<int64_t>(std::llround(p.y * 20000.0f)),
                    static_cast<int64_t>(std::llround(p.z * 20000.0f))}]
                .push_back(i);
        }
        float worst = 0.0f;
        std::size_t welded = 0;
        for (const auto& [key, ids] : groups) {
            if (ids.size() < 2) {
                continue;
            }
            glm::vec3 avg{0.0f};
            for (const std::size_t i : ids) {
                avg += target[i];
            }
            avg /= static_cast<float>(ids.size());
            for (const std::size_t i : ids) {
                worst = std::max(worst, glm::length(target[i] - avg));
                target[i] = avg;
            }
            welded += ids.size();
        }
        std::printf("[import] reshape: welded %zu coincident vertices in %zu "
                    "seams; widest gap closed %.4f mm\n",
                    welded, groups.size(), static_cast<double>(worst * 1000.0f));
    }

    // --- back into bind space, and the NORMALS with it --------------------
    // NORMALS ARE REBUILT FROM THE MOVED GEOMETRY, NOT CARRIED. A radial
    // scale is not a rotation: the normal of a squashed cylinder is not the
    // normal of the cylinder, and reusing the file's normals is precisely what
    // "всё кривенько" looks like when it is lit. Area-weighted face normals
    // are summed per vertex IN THE REST POSE (the shape the eye sees) and then
    // pushed back into bind space by the transpose of the vertex's own blend,
    // which is the exact inverse of what skinning does to a normal
    // (n_rest = (M^-1)^T n_bind  =>  n_bind = M^T n_rest).
    std::vector<glm::vec3> nrm(vn, glm::vec3{0.0f});
    for (std::size_t t = 0; t + 2 < skin.indices.size(); t += 3) {
        const std::size_t a = skin.indices[t];
        const std::size_t b = skin.indices[t + 1];
        const std::size_t c = skin.indices[t + 2];
        if (a >= vn || b >= vn || c >= vn) {
            continue;
        }
        // NOT normalised: the cross product's length is twice the triangle's
        // area, and weighting by area is what keeps a fan of slivers from
        // outvoting the one big face a vertex actually lies on.
        const glm::vec3 fn = glm::cross(target[b] - target[a], target[c] - target[a]);
        nrm[a] += fn;
        nrm[b] += fn;
        nrm[c] += fn;
    }
    std::size_t degenerate = 0;
    for (std::size_t i = 0; i < vn; ++i) {
        const glm::mat4 inv = glm::inverse(vmat[i]);
        skin.vertices[i].position = glm::vec3{inv * glm::vec4{target[i], 1.0f}};
        if (glm::length(nrm[i]) < 1e-12f) {
            ++degenerate; // keep whatever the file had rather than invent one
            continue;
        }
        const glm::vec3 rn = glm::normalize(nrm[i]);
        const glm::vec3 bn = glm::transpose(glm::mat3{vmat[i]}) * rn;
        skin.vertices[i].normal =
            glm::length(bn) > 1e-9f ? glm::normalize(bn) : skin.vertices[i].normal;
    }
    if (degenerate > 0) {
        std::fprintf(stderr, "[import] reshape: %zu vertices touch no triangle with "
                             "area; their normals are the file's own\n",
                     degenerate);
    }
}

void usage() {
    std::fprintf(stderr,
                 "dfn_import_gltf <in.gltf|in.glb> --out <out.dfo> [--name N] "
                 "[--height M] [--yaw DEG] [--skin I]\n"
                 "  --height  scale the model so it stands M metres tall (0 = keep)\n"
                 "  --fit-hips  scale by the HIP JOINT instead of the bounding box\n"
                 "  --fit-canon rescale the SEGMENTS to docs/design/HUMAN_SCALE.md\n"
                 "  --reshape   deform the SKIN to the canon's body shape: limb\n"
                 "              thickness, hand length and the trunk's waist.\n"
                 "              Needs --fit-canon; the two answer different\n"
                 "              complaints (joints vs flesh)\n"
                 "  --reshape-trace  --reshape, plus the trunk's factor at\n"
                 "              every sampled height\n"
                 "  --skin-palette paint the mesh by BODY PART (the box body's\n"
                 "                 five clothing colours) instead of by the file's\n"
                 "                 own materials -- for a model whose materials are\n"
                 "                 a mannequin's rather than a person's\n"
                 "  --yaw     degrees baked in so the model faces -Z (default 180)\n");
}

} // namespace

int main(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const auto next = [&](const char* what) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "[import] %s needs a value\n", what);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--out") {
            opt.out = next("--out");
        } else if (a == "--name") {
            opt.name = next("--name");
        } else if (a == "--skin-palette") {
            opt.skin_palette = true;
        } else if (a == "--fit-canon") {
            opt.fit_canon = true;
        } else if (a == "--reshape") {
            opt.reshape = true;
        } else if (a == "--reshape-trace") {
            opt.reshape = true;
            opt.reshape_trace = true;
        } else if (a == "--fit-hips") {
            opt.fit_hips = true;
        } else if (a == "--height") {
            opt.height_m = std::strtof(next("--height"), nullptr);
        } else if (a == "--yaw") {
            opt.yaw_deg = std::strtof(next("--yaw"), nullptr);
        } else if (a == "--skin") {
            opt.skin_index = std::atoi(next("--skin"));
        } else if (a == "-h" || a == "--help") {
            usage();
            return 0;
        } else if (opt.input.empty()) {
            opt.input = a;
        } else {
            std::fprintf(stderr, "[import] unexpected argument \"%s\"\n", a.c_str());
            return 2;
        }
    }
    if (opt.input.empty() || opt.out.empty()) {
        usage();
        return 2;
    }

    cgltf_options options{};
    cgltf_data* data = nullptr;
    if (cgltf_parse_file(&options, opt.input.c_str(), &data) != cgltf_result_success) {
        std::fprintf(stderr, "[import] cannot parse \"%s\"\n", opt.input.c_str());
        return 1;
    }
    if (cgltf_load_buffers(&options, data, opt.input.c_str()) != cgltf_result_success) {
        std::fprintf(stderr, "[import] cannot load buffers of \"%s\"\n",
                     opt.input.c_str());
        cgltf_free(data);
        return 1;
    }
    if (data->skins_count == 0
        || static_cast<cgltf_size>(opt.skin_index) >= data->skins_count) {
        std::fprintf(stderr, "[import] \"%s\" has %zu skins; --skin %d is not one "
                             "of them -- REFUSED\n",
                     opt.input.c_str(), static_cast<size_t>(data->skins_count),
                     opt.skin_index);
        cgltf_free(data);
        return 1;
    }
    const cgltf_skin* skin = &data->skins[opt.skin_index];
    if (skin->joints_count > dfn::skel::MAX_PALETTE_BONES) {
        std::fprintf(stderr,
                     "[import] skin has %zu joints, palette holds %u -- REFUSED "
                     "(split the mesh, do not truncate the skeleton)\n",
                     static_cast<size_t>(skin->joints_count),
                     dfn::skel::MAX_PALETTE_BONES);
        cgltf_free(data);
        return 1;
    }

    JointOrder order;
    if (!build_joint_order(skin, order)) {
        cgltf_free(data);
        return 1;
    }
    dfn::render::RegistryObject obj;
    obj.name = opt.name;
    obj.kind = "character";
    obj.source = "import:" + std::filesystem::path(opt.input).filename().string();
    if (!read_skeleton(skin, order, obj.skeleton)) {
        cgltf_free(data);
        return 1;
    }

    // EVERY triangle primitive bound to this skin, merged into one stream.
    // ONE DRAW PER CHARACTER is the point: the palette is per draw, and six
    // draws sharing one skeleton would mean building and binding the same 41
    // matrices six times a frame for no gain whatsoever.
    // ALBEDO PER SUBMESH, from the material glTF actually carries. A skinned
    // character is routinely several primitives with several materials -- this
    // one is two, M_Main and M_Joints -- and until this wave every one of them
    // was written the same flat blue, which is why the character read as a
    // mannequin. `base_color_rgba` below resolves each primitive's own
    // baseColorFactor; the blue survives only as the answer for a primitive
    // with NO material, where a white body would read as untextured error
    // geometry rather than as a stand-in.
    constexpr uint32_t PLACEHOLDER_SKIN_RGBA = 0xFF9FB4CFu; // 0xAABBGGRR
    uint32_t merged_parts = 0;
    for (cgltf_size n = 0; n < data->nodes_count; ++n) {
        const cgltf_node& node = data->nodes[n];
        if (node.skin != skin || node.mesh == nullptr) {
            continue;
        }
        for (cgltf_size p = 0; p < node.mesh->primitives_count; ++p) {
            if (node.mesh->primitives[p].type != cgltf_primitive_type_triangles) {
                continue;
            }
            if (!read_skin(&node.mesh->primitives[p], order, skin, obj.skin,
                           base_color_rgba(node.mesh->primitives[p].material,
                                           PLACEHOLDER_SKIN_RGBA))) {
                cgltf_free(data);
                return 1;
            }
            ++merged_parts;
        }
    }
    if (merged_parts == 0) {
        std::fprintf(stderr, "[import] no triangle primitive uses this skin -- REFUSED\n");
        cgltf_free(data);
        return 1;
    }
    std::printf("[import] merged %u skinned parts into one stream\n", merged_parts);
    read_clips(data, order, obj.clips);

    // THE AXIS FIX: everything above the skeleton's root joint. See
    // apply_pre_transform -- this is the "Z_UP" node the sample assets carry,
    // and skipping it imports a character lying on its face.
    const auto mat_close = [](const glm::mat4& a, const glm::mat4& b, float eps) {
        for (int c = 0; c < 4; ++c) {
            for (int r = 0; r < 4; ++r) {
                if (std::fabs(a[c][r] - b[c][r]) > eps) {
                    return false;
                }
            }
        }
        return true;
    };
    glm::mat4 pre{1.0f};
    bool pre_set = false;
    for (std::size_t i = 0; i < obj.skeleton.joints.size(); ++i) {
        if (obj.skeleton.joints[i].parent >= 0) {
            continue;
        }
        cgltf_float wm[16];
        cgltf_node_transform_world(order.nodes[i], wm);
        const glm::mat4 above = glm::make_mat4(wm) * glm::inverse(node_local(order.nodes[i]));
        if (!pre_set) {
            pre = above;
            pre_set = true;
        } else if (!mat_close(pre, above, 1e-4f)) {
            std::fprintf(stderr, "[import] NOTE: the skin's root joints hang under "
                                 "DIFFERENT parents; the first one's frame wins\n");
        }
    }
    pre = glm::rotate(glm::mat4{1.0f}, glm::radians(opt.yaw_deg),
                      glm::vec3{0.0f, 1.0f, 0.0f})
          * pre;
    apply_pre_transform(obj.skeleton, obj.skin, obj.clips, pre);
    cgltf_free(data);
    data = nullptr;

    // Height AFTER the axis fix and BEFORE the scale, measured from the
    // bind-pose vertices -- which at bind pose ARE the model (J*B = I), so
    // this is the mesh's own reading and not a joint's guess about it.
    float lo = 0.0f;
    float hi = 0.0f;
    for (std::size_t i = 0; i < obj.skin.vertices.size(); ++i) {
        const float y = obj.skin.vertices[i].position.y;
        lo = i == 0 ? y : std::min(lo, y);
        hi = i == 0 ? y : std::max(hi, y);
    }
    const float raw_height = hi - lo;
    float scale = 1.0f;
    float fit_note_from = raw_height;
    float fit_note_to = raw_height;
    const char* fit_by = "bbox"; // --fit-hips / --fit-canon overwrite it
    if (opt.fit_hips) {
        // FIT BY THE HIP JOINT, and it is the only anchor that survives a
        // helmet. The reference knight's bounding box is 2.315 m because his
        // plume is 0.5 m of it; scaling THAT to 1.8 gives a man 1.4 m tall
        // wearing a normal helmet, and the error is invisible in a screenshot
        // because a stylised character is supposed to look stylised.
        //
        // The hip is where our own rig is anchored too (BODY_HIP_HEIGHT_FRAC,
        // Drillis & Contini via Winter), so this scales the model INTO our
        // proportions rather than beside them -- one measurement, two
        // consumers, which is the only kind that cannot drift.
        const dfn::anim::SkeletonBinding pre_bind =
            dfn::anim::bind_skeleton(obj.skeleton);
        const int32_t hip = pre_bind.joint[dfn::anim::bone_index(
            dfn::anim::Bone::Pelvis)];
        if (hip < 0) {
            std::fprintf(stderr, "[import] --fit-hips: no joint maps to the pelvis "
                                 "-- falling back to the bounding box\n");
        } else {
            std::vector<glm::mat4> local(obj.skeleton.size());
            std::vector<glm::mat4> model(obj.skeleton.size());
            dfn::skel::skeleton_bind_local(obj.skeleton, local);
            dfn::skel::skeleton_model_matrices(obj.skeleton, local, model);
            const float hip_y =
                model[static_cast<std::size_t>(hip)][3][1] - lo;
            const dfn::anim::RigProportions props =
                dfn::anim::RigProportions::from_config();
            const float want = props.standing_hip_height();
            if (hip_y > 1e-4f) {
                scale = want / hip_y;
                fit_note_from = hip_y;
                fit_note_to = want;
                fit_by = "hips";
            }
        }
    }
    if (fit_by[0] == 'b' && opt.height_m > 0.0f && raw_height > 1e-4f) {
        scale = opt.height_m / raw_height;
        fit_note_to = opt.height_m;
    }
    // A SANITY BAND ON THE RESULT, because both fits can be wrong and neither
    // is wrong in a way a screenshot shows. --fit-hips assumes HUMAN
    // proportions: on the stylised reference knight (a head a fifth of him,
    // hips at 17 % of his height instead of 53 %) it asks for a 5.4 m giant,
    // and a giant in the distance just looks near. Loud, and it still writes
    // the file: the operator asked for this scale and may mean it.
    const float final_height = raw_height * scale;
    if (final_height < 1.2f || final_height > 2.6f) {
        std::fprintf(stderr,
                     "[import] WARNING: the imported model stands %.2f m tall. "
                     "A person is 1.6-2.0 m; --fit-hips assumes HUMAN "
                     "proportions and a stylised model does not have them.\n",
                     static_cast<double>(final_height));
    }
    if (opt.fit_canon) {
        const skel::Skeleton before_fit = obj.skeleton;
        fit_to_canon(obj.skeleton, obj.skin,
                     dfn::anim::RigProportions::from_config());
        carry_bind_change_into_clips(before_fit, obj.skeleton, obj.clips);
        // THE HEIGHT IS RE-MEASURED AFTER THE FIT, and skipping this step is
        // how the reference base came out SIXTEEN METRES TALL with perfect
        // proportions: the fit moves joints, so the scale computed from the
        // pre-fit bounding box no longer describes the figure it is applied
        // to. Measured on the REST pose, like everything else the fit reads.
        float flo = 0.0f;
        float fhi = 0.0f;
        rest_extent(obj.skeleton, obj.skin, flo, fhi);
        const float fitted = fhi - flo;
        const float target = opt.height_m > 0.0f
                                 ? opt.height_m
                                 : raw_height; // keep the file's own stature
        if (fitted > 1e-4f && target > 1e-4f) {
            scale = target / fitted;
            fit_note_from = fitted;
            fit_note_to = target;
            fit_by = "canon";
        }
    }
    apply_scale(obj.skeleton, obj.skin, obj.clips, scale);

    // AFTER THE SCALE AND BEFORE THE GROUNDING, and the order is forced rather
    // than chosen. The reshape's every target is a length in METRES read from
    // the registry, so it has to run on a model that already stands at its
    // final stature; and it moves the skin, so the grounding that puts the
    // soles on y = 0 has to run after it and measure what came out.
    if (opt.reshape) {
        reshape_to_commoner(obj.skeleton, obj.skin,
                            dfn::anim::RigProportions::from_config(),
                            opt.reshape_trace);
    }

    // GROUNDING: the REST pose's soles are put on y = 0 and the figure is
    // centred on x = 0. The engine places a character by its GROUND POINT
    // (docs/RIG.md: the owner entity's Transform is the capsule bottom), so a
    // model whose origin is anywhere else is buried or hovering -- and buried
    // is what it was: the fitted base drew with its feet 0.761 m under the
    // grass and only a white scalp above it, which reads as "the model is
    // tiny" and not as "the model is sunk". Done LAST, because every step
    // above moves the soles.
    {
        float glo = 0.0f;
        float ghi = 0.0f;
        rest_extent(obj.skeleton, obj.skin, glo, ghi);
        const dfn::anim::Rig grig =
            dfn::anim::rest_rig_for(obj.skeleton, obj.skin.vertices);
        const dfn::anim::SkinnedRigBinding gsb =
            dfn::anim::bind_skinned_rig(grig, obj.skeleton);
        std::vector<glm::mat4> gpal(obj.skeleton.size());
        dfn::anim::skinning_palette(grig, obj.skeleton, gsb, dfn::anim::LocalPose{},
                                    gpal);
        float xlo = 0.0f;
        float xhi = 0.0f;
        float zlo = 0.0f;
        float zhi = 0.0f;
        for (std::size_t i = 0; i < obj.skin.vertices.size(); ++i) {
            const glm::vec3 q =
                dfn::anim::cpu_skin_position(obj.skin.vertices[i], gpal);
            xlo = i == 0 ? q.x : std::min(xlo, q.x);
            xhi = i == 0 ? q.x : std::max(xhi, q.x);
            zlo = i == 0 ? q.z : std::min(zlo, q.z);
            zhi = i == 0 ? q.z : std::max(zhi, q.z);
        }
        const glm::vec3 shift{-(xlo + xhi) * 0.5f, -glo, -(zlo + zhi) * 0.5f};
        const skel::Skeleton before_ground = obj.skeleton;
        for (dfn::skel::SkeletonJoint& j : obj.skeleton.joints) {
            if (j.parent < 0) {
                j.bind_translation += shift;
            }
        }
        // ...AND INTO THE CLIPS, which key the root's translation too: without
        // this the first frame of any clip put the soles back where they were.
        carry_bind_change_into_clips(before_ground, obj.skeleton, obj.clips);
        // The vertices are NOT moved: the shift went into the root joint, so
        // the palette carries it, and the inverse binds stay the file's own.
        std::printf("[import] grounded: soles to y=0 (moved %+.3f m), centred "
                    "(%+.3f, %+.3f)\n",
                    static_cast<double>(shift.y), static_cast<double>(shift.x),
                    static_cast<double>(shift.z));
    }

    // --- PAINT BY BODY PART (--skin-palette) -------------------------------
    // WHY A MODEL'S OWN MATERIALS ARE SOMETIMES THE WRONG ANSWER. The visible
    // character ships two materials, M_Main and M_Joints, and their base
    // colours are orange and purple: it is a MANNEQUIN, and the file says so
    // honestly. Importing that faithfully draws an orange man, which is a
    // correct import and a wrong character. This paints each vertex by which
    // BODY PART it belongs to instead, in the five colours the fifteen-box
    // body already wears -- so the two bodies behind the DFN_BODY_BOXES door
    // differ by their geometry and not by their wardrobe.
    //
    // THE PART IS THE VERTEX'S HEAVIEST JOINT, walked up to the nearest joint
    // a rig bone actually binds. Thirty-eight of this skeleton's fifty-three
    // joints are fingers, spine links and toes that no bone maps; a vertex on
    // a knuckle has to become a HAND, not a default.
    if (opt.skin_palette) {
        const dfn::anim::SkeletonBinding pal_bind =
            dfn::anim::bind_skeleton(obj.skeleton);
        std::vector<int32_t> bone_of(obj.skeleton.size(), -1);
        for (uint32_t b = 0; b < dfn::anim::BONE_COUNT; ++b) {
            const int32_t j = pal_bind.joint[b];
            if (j >= 0 && static_cast<std::size_t>(j) < bone_of.size()) {
                bone_of[static_cast<std::size_t>(j)] = static_cast<int32_t>(b);
            }
        }
        // Parent-before-child (Skeleton.h's invariant) makes the walk a single
        // forward pass instead of a loop per vertex.
        std::vector<int32_t> nearest = bone_of;
        for (std::size_t j = 0; j < obj.skeleton.joints.size(); ++j) {
            if (nearest[j] >= 0) {
                continue;
            }
            const int32_t parent = obj.skeleton.joints[j].parent;
            nearest[j] = parent >= 0 ? nearest[static_cast<std::size_t>(parent)] : -1;
        }
        std::array<uint32_t, dfn::anim::BONE_COUNT> colour{};
        for (uint32_t b = 0; b < dfn::anim::BONE_COUNT; ++b) {
            colour[b] = dfn::anim::segment_colour(static_cast<dfn::anim::Bone>(b));
        }
        std::size_t painted = 0;
        for (dfn::platform::SkinnedVertex& v : obj.skin.vertices) {
            int best = 0;
            for (int k = 1; k < 4; ++k) {
                if (v.weights[k] > v.weights[best]) {
                    best = k;
                }
            }
            const auto j = static_cast<std::size_t>(v.joints[best]);
            const int32_t b = j < nearest.size() ? nearest[j] : -1;
            if (b < 0) {
                continue; // above the pelvis in the hierarchy: leave the file's own
            }
            v.color_rgba = colour[static_cast<std::size_t>(b)];
            ++painted;
        }
        std::printf("[import] --skin-palette: painted %zu of %zu vertices by body "
                    "part\n", painted, obj.skin.vertices.size());
    }

    // --- THE COVERAGE REPORT, and it is the whole reason this tool links the
    // character zone. An import that maps four bones of fifteen still writes a
    // valid file and still draws; what it cannot do is walk. Printing the map
    // is what turns that into a thing somebody sees on the day it happens.
    const dfn::anim::SkeletonBinding binding = dfn::anim::bind_skeleton(obj.skeleton);
    std::printf("[import] %s: %zu joints, %zu vertices, %zu triangles, %zu clips\n",
                obj.name.c_str(), obj.skeleton.joints.size(),
                obj.skin.vertices.size(), obj.skin.indices.size() / 3,
                obj.clips.size());
    std::printf("[import] fit by %s: %.3f m -> %.3f m (scale %.4f); bbox height "
                "%.3f m -> %.3f m; yaw %+.0f deg\n",
                fit_by, static_cast<double>(fit_note_from),
                static_cast<double>(fit_note_to), static_cast<double>(scale),
                static_cast<double>(raw_height),
                static_cast<double>(raw_height * scale),
                static_cast<double>(opt.yaw_deg));
    std::printf("[import] rig coverage %u/%u bones:\n", binding.bound_count,
                dfn::anim::BONE_COUNT);
    for (uint32_t b = 0; b < dfn::anim::BONE_COUNT; ++b) {
        const auto bone = static_cast<dfn::anim::Bone>(b);
        const int32_t j = binding.joint[b];
        std::printf("    %-10s <- %s\n",
                    std::string(dfn::anim::bone_name(bone)).c_str(),
                    j >= 0 ? obj.skeleton.joints[static_cast<std::size_t>(j)]
                                 .name.c_str()
                           : "(UNBOUND)");
    }
    for (const dfn::skel::AnimClip& c : obj.clips) {
        std::printf("[import] clip \"%s\": %.3f s, %zu channels\n", c.name.c_str(),
                    static_cast<double>(c.duration_s), c.channels.size());
    }

    if (!dfn::render::write_object(obj, opt.out)) {
        std::fprintf(stderr, "[import] failed to write \"%s\"\n", opt.out.c_str());
        return 1;
    }
    std::printf("[import] wrote %s (hash %016llx)\n", opt.out.c_str(),
                static_cast<unsigned long long>(
                    dfn::render::object_content_hash(obj)));
    return 0;
}
