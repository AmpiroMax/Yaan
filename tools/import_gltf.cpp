/*
Module: tools
File: tools/import_gltf.cpp

Responsibility:
- dfn_import_gltf: reads a glTF 2.0 file (cgltf) with a skin and writes ONE
  .dfo v5 carrying the skinned mesh (SKIN), the skeleton (SKEL) and the clips
  (ANIM). The offline half of the character pipeline: the game never sees a
  .gltf, only the .dfo this tool bakes.

Key items:
- main(): CLI (--out, --height, --yaw, --skin, --name).
- read_skeleton() / read_skin() / read_clips(): the three sections.
- normalize_and_scale(): metres and facing, baked at import.

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
#include "engine/anim/sources/Rig.h"
#include "engine/core/config/sources/Constants.h"
#include "engine/core/skeleton/sources/Skeleton.h"
#include "engine/render/sources/ObjectRegistry.h"

#include <algorithm>
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
    for (skel::AnimClip& c : clips) {
        for (skel::AnimChannel& ch : c.channels) {
            if (ch.path != skel::AnimPath::Translation
                || ch.joint >= skeleton.joints.size()
                || skeleton.joints[ch.joint].parent >= 0) {
                continue;
            }
            for (glm::vec4& v : ch.values) {
                v = glm::vec4{glm::vec3{pre * glm::vec4{glm::vec3{v}, 1.0f}}, v.w};
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
    const dfn::anim::Rig rig =
        dfn::anim::Rig::build(dfn::anim::RigProportions::from_config());
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
        const dfn::anim::Rig r =
            dfn::anim::Rig::build(dfn::anim::RigProportions::from_config());
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

void usage() {
    std::fprintf(stderr,
                 "dfn_import_gltf <in.gltf|in.glb> --out <out.dfo> [--name N] "
                 "[--height M] [--yaw DEG] [--skin I]\n"
                 "  --height  scale the model so it stands M metres tall (0 = keep)\n"
                 "  --fit-hips  scale by the HIP JOINT instead of the bounding box\n"
                 "  --fit-canon rescale the SEGMENTS to docs/design/HUMAN_SCALE.md\n"
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
            dfn::anim::Rig::build(dfn::anim::RigProportions::from_config());
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
