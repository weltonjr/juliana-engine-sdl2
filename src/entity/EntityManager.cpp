#include "entity/EntityManager.h"
#include <algorithm>
#include <cstdio>

EntityManager::EntityManager(const DefinitionRegistry& registry)
    : registry_(registry)
{
}

EntityID EntityManager::Spawn(const std::string& def_id, float x, float y) {
    const ObjectDef* def = registry_.GetObject(def_id);
    if (!def) {
        std::fprintf(stderr, "Cannot spawn unknown object: %s\n", def_id.c_str());
        return 0;
    }

    EntityID id = next_id_++;
    Entity entity;
    entity.id = id;
    entity.definition = def;
    entity.pos_x = x;
    entity.pos_y = y;
    entity.prev_pos_x = x;
    entity.prev_pos_y = y;
    entity.width = def->size_w;
    entity.height = def->size_h;
    entity.mass = def->mass;
    entity.max_fall_speed = def->max_fall_speed;
    entity.step_up = def->step_up;
    entity.solid = def->solid;

    // Seed per-instance properties from the definition defaults.
    // Scripts can then mutate these (e.g. decrement HP) without affecting other instances.
    for (const auto& [k, v] : def->properties)
        entity.instance_properties[k] = v;

    entities_[id] = entity;
    RebuildSortedIDs();

    std::printf("Spawned entity %u (%s) at %.1f, %.1f\n",
        id, def_id.c_str(), x, y);
    return id;
}

void EntityManager::QueueDestroy(EntityID id) {
    destroy_queue_.push_back(id);
}

void EntityManager::ProcessQueues() {
    if (destroy_queue_.empty()) return;

    for (EntityID id : destroy_queue_) {
        entities_.erase(id);
    }
    destroy_queue_.clear();
    RebuildSortedIDs();
}

Entity* EntityManager::GetEntity(EntityID id) {
    auto it = entities_.find(id);
    return it != entities_.end() ? &it->second : nullptr;
}

const Entity* EntityManager::GetEntity(EntityID id) const {
    auto it = entities_.find(id);
    return it != entities_.end() ? &it->second : nullptr;
}

void EntityManager::ForEach(const std::function<void(Entity&)>& callback) {
    for (EntityID id : sorted_ids_) {
        auto it = entities_.find(id);
        if (it != entities_.end()) {
            callback(it->second);
        }
    }
}

void EntityManager::ForEach(const std::function<void(const Entity&)>& callback) const {
    for (EntityID id : sorted_ids_) {
        auto it = entities_.find(id);
        if (it != entities_.end()) {
            callback(it->second);
        }
    }
}

void EntityManager::RebuildSortedIDs() {
    sorted_ids_.clear();
    sorted_ids_.reserve(entities_.size());
    for (auto& [id, _] : entities_) {
        sorted_ids_.push_back(id);
    }
    std::sort(sorted_ids_.begin(), sorted_ids_.end());
}
