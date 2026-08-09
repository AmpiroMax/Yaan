/*
Created: 09:08:2026 - 22:12:57
Last updated: 09:08:2026 - 22:12:57
Module: engine/render
File: engine/render/sources/RenderSystemResources.cpp

Responsibility:
- The half of RenderSystem that manages RESOURCES AND SCREENS rather than the
  frame: carried-light collection, the overlay blit, the debug water plane and
  the per-body water meshes. Split out of RenderSystem.cpp, which had reached
  817 lines against the 800-line hard limit (Rule 21) before terrain LOD added
  a line to it.

Key items:
- RenderSystem::collect_point_lights, draw_overlay, set_internal_resolution,
  set_water / clear_water, set_water_bodies / clear_water_bodies.

Dependencies:
- Uses: RenderSystem.h, Materials.h, WaterMesher, engine/core (ecs, components).
- Used by: dfn_render.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- This is the same class as RenderSystem.cpp, split only for the line limit:
  members and invariants are shared, so read both before changing either.
*/
/*
UPD:
- 09:08:2026 - 22:12:57: Split out of RenderSystem.cpp (Rule 21).
*/

#include "engine/render/sources/RenderSystem.h"

#include "engine/core/components/sources/Components.h"
#include "engine/core/ecs/sources/World.h"
#include "engine/render/sources/Materials.h"
#include "engine/render/sources/SkyModel.h" // TORCH_COLOR / TORCH_RADIUS_M
#include "engine/render/sources/WaterMesher.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/matrix.hpp>

namespace dfn::render {

void RenderSystem::collect_point_lights(ecs::World& world,
                                        const FirstPersonCamera& camera,
                                        float alpha) {
    uint32_t count = 0;
    const auto add = [&](const glm::vec3& position, float radius,
                         const glm::vec3& color) {
        if (count >= platform::MAX_POINT_LIGHTS || radius <= 0.0f) {
            return;
        }
        platform::PointLight& out = environment_.point_lights[count];
        out.position = position;
        out.radius_m = radius;
        out.color = color;
        // WHICH lights cast is render's decision, never gameplay's: the first
        // MAX_SHADOW_POINT_LIGHTS get a cube map and the rest light without
        // one. Ordering is collection order, i.e. the carried torch — the one
        // light the player owns — is always among the shadowed ones.
        out.casts_shadow = count < platform::MAX_SHADOW_POINT_LIGHTS
                        && !point_shadows_off_;
        ++count;
    };

    world.view<components::CarriedLight, components::Transform>().each(
        [&](ecs::EntityId id, components::CarriedLight& light,
            components::Transform& curr) {
            if (!light.active) {
                return; // held but not lit — gameplay keeps the component
            }
            // Interpolate like any other renderable (Rule 12); a light that
            // moved at the fixed rate would strobe against 60+ fps geometry.
            glm::vec3 position = curr.position;
            glm::quat rotation = curr.rotation;
            if (const auto* prev = world.get<components::PreviousTransform>(id)) {
                position = glm::mix(prev->position, curr.position, alpha);
                rotation = glm::slerp(prev->rotation, curr.rotation, alpha);
            }
            // The offset is CARRIER-LOCAL: sim writes the hand, ~1.45 m above
            // the feet and 0.35 m to the right, and rotating it by the body
            // yaw is what makes the shadows swing when the player turns.
            const float radius = light.radius_m > 0.0f ? light.radius_m
                                                       : TORCH_RADIUS_M;
            glm::vec3 color = TORCH_COLOR;
            if (light.color_rgb != 0u) {
                color = {static_cast<float>((light.color_rgb >> 16) & 0xFFu) / 255.0f,
                         static_cast<float>((light.color_rgb >> 8) & 0xFFu) / 255.0f,
                         static_cast<float>(light.color_rgb & 0xFFu) / 255.0f};
            }
            add(position + rotation * light.offset, radius, color);
        });

    // Verification hook only (Rule 27): the tour freezes the player and no
    // entity carries a torch during a screenshot run, so DFN_TORCH=1 lights
    // one at the camera's hand. It stands down the moment a real CarriedLight
    // exists, so it can never quietly become the shipping path.
    if (torch_debug_ && count == 0) {
        const CameraPose pose = camera.interpolated_pose(alpha);
        const glm::vec3 right = camera.right(alpha);
        if (torch_ahead_m_ > 0.0f) {
            // BRAZIER PROBE (DFN_TORCH=2): the light stands away from the eye,
            // in front of the camera. A carried flame sits 0.35 m from the eye,
            // so nearly every shadow it casts hides BEHIND its own caster —
            // true of any real hand-held light and the reason interiors, not
            // open ground, are its acceptance test. Standing the same light off
            // at a distance puts casters between eye and flame, which is what
            // makes the cube map's work visible in one open-ground frame.
            const glm::vec3 fwd = camera.forward(alpha);
            const glm::vec3 flat = glm::normalize(glm::vec3{fwd.x, 0.0f, fwd.z}
                                                  + glm::vec3{1e-4f, 0.0f, 0.0f});
            add(pose.position + flat * torch_ahead_m_, TORCH_RADIUS_M, TORCH_COLOR);
            environment_.point_light_count = count;
            if (dark_frozen_) {
                environment_.ambient_darkness = frozen_darkness_;
            }
            return;
        }
        // Flattened forward: a hand does not rise when the eyes look up, and
        // sim's real CarriedLight is yaw-only for exactly the same reason.
        const glm::vec3 fwd = camera.forward(alpha);
        const glm::vec3 flat = glm::normalize(glm::vec3{fwd.x, 0.0f, fwd.z}
                                              + glm::vec3{1e-4f, 0.0f, 0.0f});
        add(pose.position + right * 0.35f + flat * 0.15f
                - glm::vec3{0.0f, 0.25f, 0.0f},
            TORCH_RADIUS_M, TORCH_COLOR);
    }
    environment_.point_light_count = count;
    if (dark_frozen_) {
        environment_.ambient_darkness = frozen_darkness_;
    }
}

void RenderSystem::set_internal_resolution(uint32_t width, uint32_t height) {
    if (width > 0 && height > 0) {
        internal_res_ = {width, height};
    }
}

void RenderSystem::draw_overlay(platform::IRenderer& renderer, const PixelCanvas& canvas,
                                const FirstPersonCamera& camera, float alpha) {
    if (overlay_mesh_ == 0 || unlit_program_ == 0 || canvas.width() == 0) {
        return;
    }
    // One upload per frame: IRenderer has no texture update (frozen contract),
    // so the canvas texture is recreated. At 640x360 that is 900 KB — cheap,
    // and only while a screen is open.
    if (overlay_texture_ != 0) {
        renderer.destroy_texture(platform::TextureHandle{overlay_texture_});
        overlay_texture_ = 0;
    }
    const platform::TextureHandle texture =
        renderer.create_texture(canvas.width(), canvas.height(),
                                platform::TextureFormat::RGBA8, canvas.pixels());
    if (!texture.valid()) {
        return;
    }
    overlay_texture_ = texture.id;

    // Quad placed just past the near plane, sized to EXACTLY fill the frustum
    // there (half height = d * tan(fov/2)), so one canvas pixel lands on one
    // internal pixel and the point sampler stays crisp (an overscan factor
    // duplicated pixel columns and softened the map). Depth test LESS lets it
    // cover every earlier submit.
    const float depth = std::max(camera.near_plane(), 0.01f) * 1.5f;
    const float half_h = depth * std::tan(camera.fov_y() * 0.5f);
    const float half_w = half_h * camera.aspect_ratio();
    const glm::mat4 model = glm::inverse(camera.view(alpha))
                          * glm::translate(glm::mat4(1.0f), {0.0f, 0.0f, -depth})
                          * glm::scale(glm::mat4(1.0f), {half_w, half_h, 1.0f});
    renderer.submit(platform::MeshHandle{overlay_mesh_},
                    platform::ProgramHandle{unlit_program_}, model, texture);
}

void RenderSystem::set_water(platform::IRenderer& renderer, float height_m,
                             glm::vec2 center_xz, float half_extent_m) {
    clear_water(renderer);

    // One quad; uv in water-tile units so the texture repeats every
    // LOOKDEV_WATER_UV_TILE_M meters (sampler wraps, shader scrolls).
    const float x0 = center_xz.x - half_extent_m;
    const float x1 = center_xz.x + half_extent_m;
    const float z0 = center_xz.y - half_extent_m;
    const float z1 = center_xz.y + half_extent_m;
    const float inv_tile = 1.0f / LOOKDEV_WATER_UV_TILE_M;
    const glm::vec3 up{0.0f, 1.0f, 0.0f};
    const std::array<platform::Vertex, 4> vertices{{
        {{x0, height_m, z0}, up, {x0 * inv_tile, z0 * inv_tile}, 0xFFFFFFFFu},
        {{x1, height_m, z0}, up, {x1 * inv_tile, z0 * inv_tile}, 0xFFFFFFFFu},
        {{x0, height_m, z1}, up, {x0 * inv_tile, z1 * inv_tile}, 0xFFFFFFFFu},
        {{x1, height_m, z1}, up, {x1 * inv_tile, z1 * inv_tile}, 0xFFFFFFFFu},
    }};
    const std::array<uint32_t, 6> indices{0, 3, 1, 0, 2, 3}; // CCW from +Y
    const platform::MeshHandle handle = renderer.create_mesh(vertices, indices);
    if (!handle.valid()) {
        return;
    }
    water_mesh_ = handle.id;
    // Beaches: sand band sits just above the waterline (tunable via environment()).
    environment_.sand_height_m = height_m + LOOKDEV_SAND_BLEND_M * 0.5f;
}

void RenderSystem::clear_water(platform::IRenderer& renderer) {
    if (water_mesh_ != 0) {
        renderer.destroy_mesh(platform::MeshHandle{water_mesh_});
        water_mesh_ = 0;
    }
}
void RenderSystem::set_water_bodies(platform::IRenderer& renderer,
                                    std::span<const math::LakePlane> lakes,
                                    std::span<const math::RiverStation> river_stations,
                                    std::span<const uint32_t> river_segment_offsets) {
    clear_water_bodies(renderer);

    const auto upload = [&](const MeshData& data) {
        if (data.vertices.empty()) {
            return;
        }
        const platform::MeshHandle handle =
            renderer.create_mesh(data.vertices, data.indices);
        if (handle.valid()) {
            water_body_meshes_.push_back(handle.id);
        }
    };

    for (const math::LakePlane& lake : lakes) {
        upload(build_lake_mesh(lake, LOOKDEV_WATER_UV_TILE_M,
                               LOOKDEV_WATER_EDGE_MARGIN_M));
    }
    // Segment i = stations [offsets[i], offsets[i+1]); a trailing offset equal
    // to the station count is tolerated but not required.
    const size_t station_count = river_stations.size();
    for (size_t i = 0; i < river_segment_offsets.size(); ++i) {
        const size_t begin = river_segment_offsets[i];
        const size_t end = i + 1 < river_segment_offsets.size()
                               ? river_segment_offsets[i + 1]
                               : station_count;
        if (begin >= end || end > station_count) {
            continue;
        }
        upload(build_river_mesh(river_stations.subspan(begin, end - begin),
                                LOOKDEV_WATER_UV_TILE_M,
                                LOOKDEV_WATER_EDGE_MARGIN_M));
    }
}

void RenderSystem::clear_water_bodies(platform::IRenderer& renderer) {
    for (const uint32_t mesh_id : water_body_meshes_) {
        renderer.destroy_mesh(platform::MeshHandle{mesh_id});
    }
    water_body_meshes_.clear();
}

} // namespace dfn::render
