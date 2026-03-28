#pragma once

#include "entity/Entity.h"
#include "package/DefinitionRegistry.h"
#include <vector>
#include <unordered_map>
#include <functional>

class EntityManager {
public:
    EntityManager(const DefinitionRegistry& registry);

    EntityID Spawn(const std::string& def_id, float x, float y);
    void QueueDestroy(EntityID id);
    void ProcessQueues();

    Entity* GetEntity(EntityID id);
    const Entity* GetEntity(EntityID id) const;

    // Iterate all entities in ID order
    void ForEach(const std::function<void(Entity&)>& callback);
    void ForEach(const std::function<void(const Entity&)>& callback) const;

    size_t Count() const { return entities_.size(); }

private:
    const DefinitionRegistry& registry_;
    std::unordered_map<EntityID, Entity> entities_;
    std::vector<EntityID> sorted_ids_; // maintained in sorted order
    EntityID next_id_ = 1;

    std::vector<EntityID> destroy_queue_;

    void RebuildSortedIDs();
};
