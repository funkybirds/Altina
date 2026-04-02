#include "Engine/GameScene/ComponentRegistry.h"

#include "Logging/Log.h"

namespace AltinaEngine::GameScene {
    namespace {
        FComponentRegistry gComponentRegistry;

        auto               StripCppTypeKeyword(Core::Container::FNativeStringView typeName)
            -> Core::Container::FNativeStringView {
            constexpr Core::Container::FNativeStringView classPrefix("class ");
            constexpr Core::Container::FNativeStringView structPrefix("struct ");
            if (typeName.StartsWith(classPrefix)) {
                return typeName.Substr(classPrefix.Length());
            }
            if (typeName.StartsWith(structPrefix)) {
                return typeName.Substr(structPrefix.Length());
            }
            return typeName;
        }
    } // namespace

    void FComponentRegistry::Register(const FComponentTypeEntry& entry) {
        if (entry.TypeHash == 0 || entry.Create == nullptr) {
            return;
        }

        auto it = mEntries.FindIt(entry.TypeHash);
        if (it != mEntries.end()) {
            const bool hasNameConflict = !it->second.TypeName.IsEmpty() && !entry.TypeName.IsEmpty()
                && it->second.TypeName != entry.TypeName;
            it->second = entry;
            if (hasNameConflict) {
                LogWarningCat(TEXT("Engine.GameScene"),
                    TEXT("GameScene component registry: replaced type hash {}"), entry.TypeHash);
            }
            return;
        }

        mEntries[entry.TypeHash] = entry;
    }

    auto FComponentRegistry::Has(FComponentTypeHash type) const -> bool {
        return mEntries.HasKey(type);
    }

    auto FComponentRegistry::Find(FComponentTypeHash type) const -> const FComponentTypeEntry* {
        auto it = mEntries.FindIt(type);
        if (it == mEntries.end()) {
            return nullptr;
        }
        return &it->second;
    }

    auto FComponentRegistry::FindByTypeName(Core::Container::FNativeStringView typeName) const
        -> const FComponentTypeEntry* {
        if (typeName.IsEmpty()) {
            return nullptr;
        }

        const auto strippedQuery = StripCppTypeKeyword(typeName);
        for (const auto& [hash, entry] : mEntries) {
            (void)hash;
            if (entry.TypeName.IsEmpty()) {
                continue;
            }

            if (entry.TypeName == typeName) {
                return &entry;
            }

            const auto strippedEntry = StripCppTypeKeyword(entry.TypeName);
            if (strippedEntry == strippedQuery) {
                return &entry;
            }
        }
        return nullptr;
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

    void FComponentRegistry::SerializeJson(
        FWorld& world, FComponentId id, Core::Reflection::ISerializer& s) const {
        const auto* entry = Find(id.Type);
        if (entry == nullptr || entry->SerializeJson == nullptr) {
            return;
        }
        entry->SerializeJson(world, id, s);
    }

    void FComponentRegistry::DeserializeJson(
        FWorld& world, FComponentId id, const Core::Utility::Json::FJsonValue& value) const {
        const auto* entry = Find(id.Type);
        if (entry == nullptr || entry->DeserializeJson == nullptr) {
            return;
        }
        entry->DeserializeJson(world, id, value);
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
} // namespace AltinaEngine::GameScene
