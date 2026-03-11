#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Engine/GameScene/Ids.h"
#include "Container/HashMap.h"
#include "Container/StringView.h"
#include "Reflection/Serializer.h"
#include "Reflection/Serialization.h"
#include "Types/Meta.h"
#include "Types/Traits.h"
#include "Utility/Json.h"

using AltinaEngine::CClassBaseOf;
namespace AltinaEngine::GameScene {
    namespace Container = Core::Container;
    namespace TypeMeta  = Core::TypeMeta;
    using Container::FNativeStringView;
    using Container::THashMap;

    class FWorld;

    /**
     * @brief Context passed to component factory functions.
     */
    struct FComponentCreateContext {
        FWorld*       World = nullptr;
        FGameObjectId Owner{};
    };

    using FnCreate          = FComponentId (*)(FComponentCreateContext&);
    using FnDestroy         = void (*)(FWorld&, FComponentId);
    using FnSerialize       = void (*)(FWorld&, FComponentId, Core::Reflection::ISerializer&);
    using FnSerializeJson   = void (*)(FWorld&, FComponentId, Core::Reflection::ISerializer&);
    using FnDeserializeJson = void (*)(
        FWorld&, FComponentId, const Core::Utility::Json::FJsonValue&);
    using FnDeserialize = void (*)(FWorld&, FComponentId, Core::Reflection::IDeserializer&);

    /**
     * @brief Registry entry describing a component type.
     */
    struct FComponentTypeEntry {
        FComponentTypeHash TypeHash = 0;
        FNativeStringView  TypeName{};
        FnCreate           Create          = nullptr;
        FnDestroy          Destroy         = nullptr;
        FnSerialize        Serialize       = nullptr;
        FnSerializeJson    SerializeJson   = nullptr;
        FnDeserializeJson  DeserializeJson = nullptr;
        FnDeserialize      Deserialize     = nullptr;
    };

    /**
     * @brief Registry for component types and their lifecycle hooks.
     */
    class AE_ENGINE_API FComponentRegistry {
    public:
        void               Register(const FComponentTypeEntry& entry);

        [[nodiscard]] auto Has(FComponentTypeHash type) const -> bool;
        [[nodiscard]] auto Find(FComponentTypeHash type) const -> const FComponentTypeEntry*;
        [[nodiscard]] auto FindByTypeName(FNativeStringView typeName) const
            -> const FComponentTypeEntry*;

        [[nodiscard]] auto Create(FComponentTypeHash type, FComponentCreateContext& ctx) const
            -> FComponentId;
        void Destroy(FWorld& world, FComponentId id) const;
        void Serialize(FWorld& world, FComponentId id, Core::Reflection::ISerializer& s) const;
        void SerializeJson(FWorld& world, FComponentId id, Core::Reflection::ISerializer& s) const;
        void DeserializeJson(
            FWorld& world, FComponentId id, const Core::Utility::Json::FJsonValue& value) const;
        void Deserialize(FWorld& world, FComponentId id, Core::Reflection::IDeserializer& d) const;

    private:
        THashMap<FComponentTypeHash, FComponentTypeEntry> mEntries;
    };

    AE_ENGINE_API auto GetComponentRegistry() -> FComponentRegistry&;

    namespace Detail {
        template <typename T>
        auto CreateComponentThunk(FComponentCreateContext& ctx) -> FComponentId;
        template <typename T> void DestroyComponentThunk(FWorld& world, FComponentId id);
        template <typename T>
        void SerializeComponentThunk(
            FWorld& world, FComponentId id, Core::Reflection::ISerializer& s);
        template <typename T>
        void DeserializeComponentThunk(
            FWorld& world, FComponentId id, Core::Reflection::IDeserializer& d);
    } // namespace Detail

    template <typename T>
    [[nodiscard]] inline auto BuildComponentTypeEntry() -> FComponentTypeEntry {
        static_assert(CClassBaseOf<FComponent, T>, "Component types must derive from FComponent.");

        FComponentTypeEntry entry{};
        entry.TypeHash        = GetComponentTypeHash<T>();
        entry.TypeName        = TypeMeta::TMetaTypeInfo<T>::kName;
        entry.Create          = &Detail::CreateComponentThunk<T>;
        entry.Destroy         = &Detail::DestroyComponentThunk<T>;
        entry.Serialize       = &Detail::SerializeComponentThunk<T>;
        entry.SerializeJson   = nullptr;
        entry.DeserializeJson = nullptr;
        entry.Deserialize     = &Detail::DeserializeComponentThunk<T>;
        return entry;
    }

    template <typename T> inline void RegisterComponentType() {
        const FComponentTypeHash typeHash = GetComponentTypeHash<T>();
        auto&                    registry = GetComponentRegistry();
        if (!registry.Has(typeHash)) {
            registry.Register(BuildComponentTypeEntry<T>());
        }
    }
} // namespace AltinaEngine::GameScene
