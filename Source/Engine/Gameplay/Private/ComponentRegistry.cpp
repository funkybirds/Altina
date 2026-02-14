#include "Gameplay/ComponentRegistry.h"

#include "Logging/Log.h"

namespace AltinaEngine::Gameplay {
    namespace {
        FComponentRegistry gComponentRegistry;
    }

    void FComponentRegistry::Register(const FComponentTypeEntry& entry) {
        if (entry.TypeHash == 0 || entry.Create == nullptr) {
            return;
        }

        const bool existed = mEntries.HasKey(entry.TypeHash);
        mEntries[entry.TypeHash] = entry;

        if (existed) {
            LogWarning(TEXT("Gameplay component registry: replaced type hash {}"), entry.TypeHash);
        }
    }

    auto FComponentRegistry::Has(FComponentTypeHash type) const -> bool {
        return mEntries.HasKey(type);
    }

    auto FComponentRegistry::Find(FComponentTypeHash type) const -> const FComponentTypeEntry* {
        auto it = mEntries.find(type);
        if (it == mEntries.end()) {
            return nullptr;
        }
        return &it->second;
    }

    auto FComponentRegistry::Create(FComponentTypeHash type, FComponentCreateContext& ctx) const
        -> FComponentId {
        const auto* entry = Find(type);
        if (entry == nullptr || entry->Create == nullptr) {
            return {};
        }
        return entry->Create(ctx);
    }

    void FComponentRegistry::Destroy(FWorld& world, FComponentId id) const {
        const auto* entry = Find(id.Type);
        if (entry == nullptr || entry->Destroy == nullptr) {
            return;
        }
        entry->Destroy(world, id);
    }

    void FComponentRegistry::Serialize(
        FWorld& world, FComponentId id, Core::Reflection::ISerializer& s) const {
        const auto* entry = Find(id.Type);
        if (entry == nullptr || entry->Serialize == nullptr) {
            return;
        }
        entry->Serialize(world, id, s);
    }

    void FComponentRegistry::Deserialize(
        FWorld& world, FComponentId id, Core::Reflection::IDeserializer& d) const {
        const auto* entry = Find(id.Type);
        if (entry == nullptr || entry->Deserialize == nullptr) {
            return;
        }
        entry->Deserialize(world, id, d);
    }

    auto GetComponentRegistry() -> FComponentRegistry& { return gComponentRegistry; }
} // namespace AltinaEngine::Gameplay
