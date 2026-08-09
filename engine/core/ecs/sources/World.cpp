/*
Created: 09:08:2026 - 00:42:03
Last updated: 09:08:2026 - 00:42:03
Module: engine/core/ecs
File: engine/core/ecs/sources/World.cpp

Responsibility:
- Non-template World internals: entity lifecycle, batch spawn/destroy, the
  entity<->group index, deferred destruction.

Key items:
- World::spawn/destroy/spawn_batch/destroy_batch/destroy_group/set_group.

Dependencies:
- Uses: World.h.
- Used by: dfn_core.

AI Agents Notice (must follow):
- Follow docs/ARCHITECTURE.md strictly.
- destroy_batch visits each pool ONCE with the whole batch (Rule 11); keep it
  that way when touching this file.
*/
/*
UPD:
- 09:08:2026 - 00:42:03: Stage 2 — implementation.
*/

#include "engine/core/ecs/sources/World.h"

#include <algorithm>

namespace dfn::ecs {

World::World() = default;
World::~World() = default;

// --- Entity lifecycle --------------------------------------------------------

EntityId World::spawn() {
    uint32_t index;
    if (!free_list_.empty()) {
        index = free_list_.back();
        free_list_.pop_back();
    } else {
        index = static_cast<uint32_t>(generations_.size());
        generations_.push_back(0);
        alive_.push_back(false);
        slot_group_.push_back(NO_GROUP);
        slot_group_pos_.push_back(0);
    }
    alive_[index] = true;
    slot_group_[index] = NO_GROUP;
    ++live_count_;
    return EntityId{index, generations_[index]};
}

void World::release_slot(EntityId id) {
    detach_from_group(id);
    alive_[id.index] = false;
    ++generations_[id.index];
    free_list_.push_back(id.index);
    --live_count_;
}

void World::destroy(EntityId id) {
    if (!alive(id)) {
        return;
    }
    for (auto& [tid, pool] : pools_) {
        pool->remove(id);
    }
    release_slot(id);
}

void World::destroy_deferred(EntityId id) { deferred_destroy_.push_back(id); }

void World::flush_destroyed() {
    if (deferred_destroy_.empty()) {
        return;
    }
    // Batch semantics: double-queued and dead ids are skipped by destroy_batch.
    std::vector<EntityId> queued;
    queued.swap(deferred_destroy_); // handlers may re-queue during destruction
    destroy_batch(queued);
}

bool World::alive(EntityId id) const {
    return !id.is_null() && id.index < generations_.size() && alive_[id.index]
        && generations_[id.index] == id.generation;
}

std::size_t World::entity_count() const { return live_count_; }

void World::clear() {
    for (uint32_t i = 0; i < generations_.size(); ++i) {
        if (alive_[i]) {
            alive_[i] = false;
            ++generations_[i];
        }
    }
    for (auto& [tid, pool] : pools_) {
        pool->clear();
    }
    free_list_.clear();
    for (uint32_t i = 0; i < generations_.size(); ++i) {
        free_list_.push_back(i);
    }
    slot_group_.assign(slot_group_.size(), NO_GROUP);
    groups_.clear();
    deferred_destroy_.clear();
    resources_.clear();
    live_count_ = 0;
}

// --- Batch lifecycle (Rule 11) -----------------------------------------------

void World::spawn_batch(std::span<EntityId> out_ids, GroupId group) {
    for (EntityId& out : out_ids) {
        out = spawn();
        if (group != NO_GROUP) {
            attach_to_group(out, group);
        }
    }
}

void World::destroy_batch(std::span<const EntityId> ids) {
    // Filter to currently-live ids once; also deduplicates double-queued ids
    // (the first occurrence kills the slot, the second fails alive()).
    std::vector<EntityId> live;
    live.reserve(ids.size());
    for (const EntityId id : ids) {
        if (alive(id) && std::find(live.begin(), live.end(), id) == live.end()) {
            live.push_back(id);
        }
    }
    if (live.empty()) {
        return;
    }
    // One virtual call per pool with the whole batch (Rule 11).
    for (auto& [tid, pool] : pools_) {
        pool->remove_batch(live);
    }
    for (const EntityId id : live) {
        release_slot(id);
    }
}

std::size_t World::destroy_group(GroupId group) {
    const auto it = groups_.find(group);
    if (it == groups_.end() || it->second.empty()) {
        return 0;
    }
    // Copy: release_slot() mutates the group vector while we iterate.
    const std::vector<EntityId> members = it->second;
    destroy_batch(members);
    return members.size();
}

// --- Entity <-> group index ---------------------------------------------------

void World::attach_to_group(EntityId id, GroupId group) {
    auto& members = groups_[group];
    slot_group_[id.index] = group;
    slot_group_pos_[id.index] = static_cast<uint32_t>(members.size());
    members.push_back(id);
}

void World::detach_from_group(EntityId id) {
    const GroupId group = slot_group_[id.index];
    if (group == NO_GROUP) {
        return;
    }
    auto& members = groups_[group];
    const uint32_t pos = slot_group_pos_[id.index];
    const uint32_t last = static_cast<uint32_t>(members.size()) - 1;
    if (pos != last) {
        members[pos] = members[last];
        slot_group_pos_[members[pos].index] = pos;
    }
    members.pop_back();
    slot_group_[id.index] = NO_GROUP;
    if (members.empty()) {
        groups_.erase(group);
    }
}

void World::set_group(EntityId id, GroupId group) {
    if (!alive(id)) {
        return;
    }
    if (slot_group_[id.index] == group) {
        return;
    }
    detach_from_group(id);
    if (group != NO_GROUP) {
        attach_to_group(id, group);
    }
}

GroupId World::group_of(EntityId id) const {
    return alive(id) ? slot_group_[id.index] : NO_GROUP;
}

std::span<const EntityId> World::entities_in_group(GroupId group) const {
    const auto it = groups_.find(group);
    if (it == groups_.end()) {
        return {};
    }
    return it->second;
}

} // namespace dfn::ecs
