#include "Engine/GameScene/World.h"

#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/PbrSkyComponent.h"
#include "Engine/GameScene/PointLightComponent.h"
#include "Engine/GameScene/SkyCubeComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Reflection/Serializer.h"
#include "Utility/Json.h"
#include "Utility/Assert.h"
#include "Utility/String/CodeConvert.h"
#include "Utility/String/StringViewUtility.h"
#include "Utility/String/UuidParser.h"

using AltinaEngine::Move;
using AltinaEngine::Core::Container::FStringView;
using AltinaEngine::Core::Utility::Assert;
namespace AltinaEngine::GameScene {
    namespace {
        TAtomic<u32>             gNextWorldId(1);

        auto                     AcquireWorldId() -> u32 { return gNextWorldId.FetchAdd(1); }

        const FComponentTypeHash kCameraComponentType = GetComponentTypeHash<FCameraComponent>();
        const FComponentTypeHash kStaticMeshComponentType =
            GetComponentTypeHash<FStaticMeshFilterComponent>();
        const FComponentTypeHash kMeshMaterialComponentType =
            GetComponentTypeHash<FMeshMaterialComponent>();
        const FComponentTypeHash kDirectionalLightComponentType =
            GetComponentTypeHash<FDirectionalLightComponent>();
        const FComponentTypeHash kPointLightComponentType =
            GetComponentTypeHash<FPointLightComponent>();
        const FComponentTypeHash kSkyCubeComponentType = GetComponentTypeHash<FSkyCubeComponent>();
        const FComponentTypeHash kPbrSkyComponentType  = GetComponentTypeHash<FPbrSkyComponent>();

        enum class EWorldObjectRecordKind : u8 {
            Raw    = 0U,
            Prefab = 1U,
        };

        constexpr u32 kWorldSerializationVersion = 2U;

        auto          ReadTransform(Core::Reflection::IDeserializer& deserializer)
            -> Core::Math::LinAlg::FSpatialTransform {
            Core::Math::LinAlg::FSpatialTransform transform{};
            transform.Rotation.x = deserializer.Read<f32>();
            transform.Rotation.y = deserializer.Read<f32>();
            transform.Rotation.z = deserializer.Read<f32>();
            transform.Rotation.w = deserializer.Read<f32>();

            transform.Translation.mComponents[0] = deserializer.Read<f32>();
            transform.Translation.mComponents[1] = deserializer.Read<f32>();
            transform.Translation.mComponents[2] = deserializer.Read<f32>();

            transform.Scale.mComponents[0] = deserializer.Read<f32>();
            transform.Scale.mComponents[1] = deserializer.Read<f32>();
            transform.Scale.mComponents[2] = deserializer.Read<f32>();
            return transform;
        }

        auto ReadString(Core::Reflection::IDeserializer& deserializer) -> Core::Container::FString {
            const u32 length = deserializer.Read<u32>();
            if (length == 0U) {
                return {};
            }

            Core::Container::TVector<TChar> text{};
            text.Resize(length);
            for (u32 i = 0U; i < length; ++i) {
                text[i] = deserializer.Read<TChar>();
            }
            return Core::Container::FString(text.Data(), static_cast<usize>(length));
        }

        auto ReadNativeString(Core::Reflection::IDeserializer& deserializer)
            -> Core::Container::FNativeString {
            const u32 length = deserializer.Read<u32>();
            if (length == 0U) {
                return {};
            }

            Core::Container::TVector<char> text{};
            text.Resize(length);
            for (u32 i = 0U; i < length; ++i) {
                text[i] = deserializer.Read<char>();
            }
            return Core::Container::FNativeString(text.Data(), static_cast<usize>(length));
        }

        auto WriteTransform(Core::Reflection::ISerializer& serializer,
            const Core::Math::LinAlg::FSpatialTransform&   transform) -> void {
            serializer.Write(transform.Rotation.x);
            serializer.Write(transform.Rotation.y);
            serializer.Write(transform.Rotation.z);
            serializer.Write(transform.Rotation.w);

            serializer.Write(transform.Translation.mComponents[0]);
            serializer.Write(transform.Translation.mComponents[1]);
            serializer.Write(transform.Translation.mComponents[2]);

            serializer.Write(transform.Scale.mComponents[0]);
            serializer.Write(transform.Scale.mComponents[1]);
            serializer.Write(transform.Scale.mComponents[2]);
        }

        auto WriteString(Core::Reflection::ISerializer& serializer, FStringView value) -> void {
            serializer.Write(static_cast<u32>(value.Length()));
            for (usize i = 0U; i < value.Length(); ++i) {
                serializer.Write(value.Data()[i]);
            }
        }

        auto WriteNativeString(Core::Reflection::ISerializer& serializer,
            Core::Container::FNativeStringView                value) -> void {
            serializer.Write(static_cast<u32>(value.Length()));
            for (usize i = 0U; i < value.Length(); ++i) {
                serializer.Write(value.Data()[i]);
            }
        }

        auto WriteTransformJson(Core::Reflection::ISerializer& serializer,
            const Core::Math::LinAlg::FSpatialTransform&       transform) -> void {
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("rotation"));
            serializer.BeginArray(4);
            serializer.Write(transform.Rotation.x);
            serializer.Write(transform.Rotation.y);
            serializer.Write(transform.Rotation.z);
            serializer.Write(transform.Rotation.w);
            serializer.EndArray();

            serializer.WriteFieldName(TEXT("translation"));
            serializer.BeginArray(3);
            serializer.Write(transform.Translation.mComponents[0]);
            serializer.Write(transform.Translation.mComponents[1]);
            serializer.Write(transform.Translation.mComponents[2]);
            serializer.EndArray();

            serializer.WriteFieldName(TEXT("scale"));
            serializer.BeginArray(3);
            serializer.Write(transform.Scale.mComponents[0]);
            serializer.Write(transform.Scale.mComponents[1]);
            serializer.Write(transform.Scale.mComponents[2]);
            serializer.EndArray();
            serializer.EndObject();
        }

        auto WriteStringJson(Core::Reflection::ISerializer& serializer, FStringView value) -> void {
            serializer.WriteString(value);
        }

        auto WriteNativeStringJson(Core::Reflection::ISerializer& serializer,
            Core::Container::FNativeStringView                    value) -> void {
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
            Core::Container::FNativeString native(value.Data(), value.Length());
            auto                           wide = Core::Utility::String::FromUtf8(native);
            serializer.WriteString(wide.ToView());
#else
            serializer.WriteString(Core::Container::FStringView(value.Data(), value.Length()));
#endif
        }

        auto WriteAssetHandleJson(
            Core::Reflection::ISerializer& serializer, const Asset::FAssetHandle& handle) -> void {
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("valid"));
            serializer.Write(handle.IsValid());
            serializer.WriteFieldName(TEXT("type"));
            serializer.Write(static_cast<u8>(handle.mType));
            serializer.WriteFieldName(TEXT("uuid"));
            const auto uuidText = handle.mUuid.ToString();
            serializer.WriteString(uuidText.ToView());
            serializer.EndObject();
        }

        auto ParseAssetHandleJson(
            const Core::Utility::Json::FJsonValue& value, Asset::FAssetHandle& outHandle) -> bool {
            using Core::Utility::Json::EJsonType;
            using Core::Utility::Json::FindObjectValueInsensitive;
            using Core::Utility::Json::GetBoolValue;
            using Core::Utility::Json::GetNumberValue;
            using Core::Utility::Json::GetStringValue;

            if (value.Type != EJsonType::Object) {
                return false;
            }

            bool valid = false;
            (void)GetBoolValue(FindObjectValueInsensitive(value, "valid"), valid);
            if (!valid) {
                outHandle = {};
                return true;
            }

            double typeNumber = 0.0;
            if (!GetNumberValue(FindObjectValueInsensitive(value, "type"), typeNumber)) {
                return false;
            }

            Core::Container::FNativeString uuidText;
            if (!GetStringValue(FindObjectValueInsensitive(value, "uuid"), uuidText)) {
                return false;
            }

            FUuid uuid{};
            if (!Core::Utility::String::ParseUuid(uuidText.ToView(), uuid)) {
                return false;
            }

            outHandle.mType = static_cast<Asset::EAssetType>(static_cast<u8>(typeNumber));
            outHandle.mUuid = uuid;
            return outHandle.IsValid();
        }

    } // namespace

    FWorld::FWorld() : mWorldId(AcquireWorldId()) {}

    FWorld::FWorld(u32 worldId) : mWorldId(worldId == 0 ? AcquireWorldId() : worldId) {}

    FWorld::~FWorld() {
        for (u32 index = 0; index < static_cast<u32>(mGameObjects.Size()); ++index) {
            if (!mGameObjects[index].Alive) {
                continue;
            }
            FGameObjectId id{};
            id.Index      = index;
            id.Generation = mGameObjects[index].Generation;
            id.WorldId    = mWorldId;
            DestroyGameObjectById(id);
        }

        for (auto& entry : mComponentStorage) {
            if (entry.second) {
                entry.second->DestroyAll(*this);
            }
        }

        mComponentStorage.Clear();
        mPrefabRoots.Clear();
        mGameObjects.Clear();
        mFreeGameObjects.Clear();
    }

    auto FWorld::CreateGameObject(FStringView name) -> FGameObjectView {
        const auto id = CreateGameObjectId(name);
        return Object(id);
    }

    auto FWorld::CreateGameObjectId(FStringView name) -> FGameObjectId {
        u32 index = 0;
        if (!mFreeGameObjects.IsEmpty()) {
            index = mFreeGameObjects.Back();
            mFreeGameObjects.PopBack();
        } else {
            mGameObjects.EmplaceBack();
            index = static_cast<u32>(mGameObjects.Size() - 1);
        }

        auto handle = mGameObjectPool.Allocate();
        if (!handle) {
            mFreeGameObjects.PushBack(index);
            return {};
        }

        auto& slot  = mGameObjects[index];
        slot.Handle = Move(handle);
        slot.Alive  = true;
        if (slot.Generation == 0) {
            slot.Generation = 1;
        }

        FGameObjectId id{};
        id.Index      = index;
        id.Generation = slot.Generation;
        id.WorldId    = mWorldId;

        auto* obj = slot.Handle.Get();
        obj->SetWorld(this);
        obj->SetId(id);
        obj->SetName(name);
        obj->SetActive(true);
        return id;
    }

    auto FWorld::CreateGameObjectWithId(FGameObjectId id) -> FGameObject* {
        if (!id.IsValid()) {
            return nullptr;
        }

        const u32 index = id.Index;
        if (index >= static_cast<u32>(mGameObjects.Size())) {
            mGameObjects.Resize(static_cast<usize>(index) + 1U);
        }

        auto handle = mGameObjectPool.Allocate();
        if (!handle) {
            return nullptr;
        }

        auto& slot      = mGameObjects[index];
        slot.Handle     = Move(handle);
        slot.Alive      = true;
        slot.Generation = (id.Generation == 0U) ? 1U : id.Generation;

        FGameObjectId fixedId = id;
        fixedId.Generation    = slot.Generation;
        fixedId.WorldId       = mWorldId;

        auto* obj = slot.Handle.Get();
        obj->SetWorld(this);
        obj->SetId(fixedId);
        obj->SetActive(true);

        const auto count = static_cast<u32>(mFreeGameObjects.Size());
        for (u32 i = 0U; i < count; ++i) {
            if (mFreeGameObjects[i] == index) {
                if (i + 1U < count) {
                    mFreeGameObjects[i] = mFreeGameObjects[count - 1U];
                }
                mFreeGameObjects.PopBack();
                break;
            }
        }

        return obj;
    }

    void FWorld::DestroyGameObject(FGameObjectView object) {
        DestroyGameObjectById(object.GetId());
    }

    void FWorld::DestroyGameObjectById(FGameObjectId id) {
        auto* obj = ResolveGameObject(id);
        if (obj == nullptr) {
            return;
        }

        UnregisterPrefabRoot(id);

        auto components = obj->GetAllComponents();
        for (const auto& componentId : components) {
            DestroyComponent(componentId);
        }

        auto& slot = mGameObjects[id.Index];
        mGameObjectPool.Deallocate(Move(slot.Handle));
        slot.Alive = false;
        slot.Generation++;
        if (slot.Generation == 0) {
            slot.Generation = 1;
        }
        mFreeGameObjects.PushBack(id.Index);
    }

    auto FWorld::IsAlive(FGameObjectId id) const noexcept -> bool {
        if (!id.IsValid() || id.WorldId != mWorldId) {
            return false;
        }
        if (id.Index >= static_cast<u32>(mGameObjects.Size())) {
            return false;
        }
        const auto& slot = mGameObjects[id.Index];
        return slot.Alive && slot.Generation == id.Generation && slot.Handle;
    }

    auto FWorld::GetAllGameObjectIds() const -> TVector<FGameObjectId> {
        TVector<FGameObjectId> out;
        out.Reserve(mGameObjects.Size());
        for (u32 index = 0; index < static_cast<u32>(mGameObjects.Size()); ++index) {
            const auto& slot = mGameObjects[index];
            if (!slot.Alive || !slot.Handle) {
                continue;
            }
            FGameObjectId id{};
            id.Index      = index;
            id.Generation = slot.Generation;
            id.WorldId    = mWorldId;
            out.PushBack(id);
        }
        return out;
    }

    auto FWorld::GetGameObjectName(FGameObjectId id) const -> Core::Container::FString {
        const auto* obj = ResolveGameObject(id);
        if (obj == nullptr) {
            return {};
        }
        return obj->GetName();
    }

    auto FWorld::GetGameObjectParent(FGameObjectId id) const -> FGameObjectId {
        const auto* obj = ResolveGameObject(id);
        if (obj == nullptr) {
            return {};
        }
        return obj->GetParent();
    }

    auto FWorld::CreateComponent(FGameObjectId owner, FComponentTypeHash type) -> FComponentId {
        if (!IsAlive(owner)) {
            return {};
        }

        FComponentCreateContext ctx{};
        ctx.World = this;
        ctx.Owner = owner;
        return GetComponentRegistry().Create(type, ctx);
    }

    void FWorld::DestroyComponent(FComponentId id) {
        if (!id.IsValid()) {
            return;
        }

        auto* storage = FindComponentStorage(id.Type);
        if (storage == nullptr) {
            return;
        }
        storage->Destroy(*this, id);
    }

    auto FWorld::IsAlive(FComponentId id) const noexcept -> bool {
        if (!id.IsValid()) {
            return false;
        }
        auto* storage = FindComponentStorage(id.Type);
        return storage != nullptr && storage->IsAlive(id);
    }

    auto FWorld::GetAllComponents(FGameObjectId owner) const -> TVector<FComponentId> {
        const auto* obj = ResolveGameObject(owner);
        if (obj == nullptr) {
            return {};
        }
        return obj->GetAllComponents();
    }

    void FWorld::SetGameObjectActive(FGameObjectId id, bool active) {
        auto* obj = ResolveGameObject(id);
        if (obj == nullptr) {
            return;
        }
        const bool wasActive = obj->IsActive();
        if (wasActive == active) {
            return;
        }
        obj->SetActive(active);
        OnGameObjectActiveChanged(id, active);
    }

    auto FWorld::IsGameObjectActive(FGameObjectId id) const -> bool {
        const auto* obj = ResolveGameObject(id);
        if (obj == nullptr) {
            return false;
        }
        return obj->IsActive();
    }

    void FWorld::Tick(float InDeltaTime) {
        TVector<FComponentStorageBase*> storages;
        storages.Reserve(mComponentStorage.Num());
        for (auto& entry : mComponentStorage) {
            if (entry.second) {
                storages.PushBack(entry.second.Get());
            }
        }

        for (auto* storage : storages) {
            if (storage != nullptr) {
                storage->Tick(*this, InDeltaTime);
            }
        }

        UpdateTransforms();
    }

    void FWorld::UpdateTransforms() {
        ++mTransformUpdateId;
        if (mTransformUpdateId == 0) {
            mTransformUpdateId = 1;
        }

        for (u32 index = 0; index < static_cast<u32>(mGameObjects.Size()); ++index) {
            if (!mGameObjects[index].Alive) {
                continue;
            }
            FGameObjectId id{};
            id.Index      = index;
            id.Generation = mGameObjects[index].Generation;
            id.WorldId    = mWorldId;
            (void)UpdateTransformRecursive(id, mTransformUpdateId);
        }
    }

    auto FWorld::GetActiveCameraComponents() const noexcept -> const TVector<FComponentId>& {
        return mActiveCameraComponents;
    }

    auto FWorld::GetActiveStaticMeshComponents() const noexcept -> const TVector<FComponentId>& {
        return mActiveStaticMeshComponents;
    }

    auto FWorld::GetActiveMeshMaterialComponents() const noexcept -> const TVector<FComponentId>& {
        return mActiveMeshMaterialComponents;
    }

    auto FWorld::GetActiveDirectionalLightComponents() const noexcept
        -> const TVector<FComponentId>& {
        return mActiveDirectionalLightComponents;
    }

    auto FWorld::GetActivePointLightComponents() const noexcept -> const TVector<FComponentId>& {
        return mActivePointLightComponents;
    }

    auto FWorld::GetActiveSkyCubeComponents() const noexcept -> const TVector<FComponentId>& {
        return mActiveSkyCubeComponents;
    }

    auto FWorld::GetActivePbrSkyComponents() const noexcept -> const TVector<FComponentId>& {
        return mActivePbrSkyComponents;
    }

    void FWorld::RegisterPrefabRoot(
        FGameObjectId root, const Engine::GameSceneAsset::FPrefabDescriptor& descriptor) {
        if (!IsAlive(root)) {
            return;
        }
        mPrefabRoots[root] = descriptor;
    }

    void FWorld::UnregisterPrefabRoot(FGameObjectId root) { (void)mPrefabRoots.Remove(root); }

    auto FWorld::TryGetPrefabDescriptor(FGameObjectId root) const
        -> const Engine::GameSceneAsset::FPrefabDescriptor* {
        auto it = mPrefabRoots.FindIt(root);
        if (it == mPrefabRoots.end()) {
            return nullptr;
        }
        if (!IsAlive(root)) {
            return nullptr;
        }
        return &it->second;
    }

    auto FWorld::IsPrefabRoot(FGameObjectId root) const -> bool {
        return TryGetPrefabDescriptor(root) != nullptr;
    }

    auto FWorld::IsDescendantOfPrefabRoot(FGameObjectId id) const -> bool {
        const auto* object = ResolveGameObject(id);
        if (object == nullptr) {
            return false;
        }

        FGameObjectId parent = object->GetParent();
        while (parent.IsValid()) {
            if (IsPrefabRoot(parent)) {
                return true;
            }

            const auto* parentObject = ResolveGameObject(parent);
            if (parentObject == nullptr) {
                break;
            }
            parent = parentObject->GetParent();
        }
        return false;
    }

    auto FWorld::GetSerializableGameObjects() const -> TVector<FGameObjectId> {
        TVector<FGameObjectId> objects{};
        for (u32 index = 0; index < static_cast<u32>(mGameObjects.Size()); ++index) {
            const auto& slot = mGameObjects[index];
            if (!slot.Alive || !slot.Handle) {
                continue;
            }

            FGameObjectId id{};
            id.Index      = index;
            id.Generation = slot.Generation;
            id.WorldId    = mWorldId;

            if (IsDescendantOfPrefabRoot(id) && !IsPrefabRoot(id)) {
                continue;
            }
            objects.PushBack(id);
        }
        return objects;
    }

    void FWorld::Serialize(Core::Reflection::ISerializer& serializer) const {
        serializer.Write(kWorldSerializationVersion);
        serializer.Write(mWorldId);

        const auto serializableObjects = GetSerializableGameObjects();
        serializer.Write(static_cast<u32>(serializableObjects.Size()));

        auto& registry = GetComponentRegistry();
        for (const auto& id : serializableObjects) {
            const auto* obj = ResolveGameObject(id);
            if (obj == nullptr) {
                continue;
            }

            const auto* prefabDescriptor = TryGetPrefabDescriptor(id);
            const auto  recordKind = (prefabDescriptor != nullptr) ? EWorldObjectRecordKind::Prefab
                                                                   : EWorldObjectRecordKind::Raw;

            serializer.Write(static_cast<u8>(recordKind));

            serializer.Write(id.Index);
            serializer.Write(id.Generation);

            WriteString(serializer, obj->GetName().ToView());
            serializer.Write(obj->IsActive());

            const auto parent = obj->GetParent();
            serializer.Write(parent.IsValid());
            if (parent.IsValid()) {
                serializer.Write(parent.Index);
                serializer.Write(parent.Generation);
            }

            WriteTransform(serializer, obj->GetLocalTransform());

            if (recordKind == EWorldObjectRecordKind::Prefab) {
                WriteNativeString(serializer, prefabDescriptor->LoaderType.ToView());
                prefabDescriptor->AssetHandle.Serialize(serializer);
                continue;
            }

            TVector<FComponentId> serializableComponents;
            for (const auto& componentId : obj->GetAllComponents()) {
                const auto* entry = registry.Find(componentId.Type);
                if (entry == nullptr || entry->Serialize == nullptr
                    || entry->Deserialize == nullptr) {
                    continue;
                }
                serializableComponents.PushBack(componentId);
            }

            serializer.Write(static_cast<u32>(serializableComponents.Size()));
            for (const auto& componentId : serializableComponents) {
                const auto* component = ResolveComponentBase(componentId);
                const bool  enabled   = component ? component->IsEnabled() : true;

                serializer.Write(componentId.Type);
                serializer.Write(enabled);
                registry.Serialize(const_cast<FWorld&>(*this), componentId, serializer);
            }
        }
    }

    void FWorld::SerializeJson(Core::Reflection::ISerializer& serializer) const {
        auto& registry = GetComponentRegistry();

        serializer.BeginObject({});
        serializer.WriteFieldName(TEXT("version"));
        serializer.Write(kWorldSerializationVersion);

        serializer.WriteFieldName(TEXT("worldId"));
        serializer.Write(mWorldId);

        serializer.WriteFieldName(TEXT("objects"));
        serializer.BeginArray(0);

        for (const auto& id : GetSerializableGameObjects()) {
            const auto* obj = ResolveGameObject(id);
            if (obj == nullptr) {
                continue;
            }

            const auto* prefabDescriptor = TryGetPrefabDescriptor(id);
            const bool  isPrefabRecord   = prefabDescriptor != nullptr;

            serializer.BeginObject({});

            serializer.WriteFieldName(TEXT("recordKind"));
            if (isPrefabRecord) {
                serializer.WriteString(TEXT("Prefab"));
            } else {
                serializer.WriteString(TEXT("Raw"));
            }

            serializer.WriteFieldName(TEXT("id"));
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("index"));
            serializer.Write(id.Index);
            serializer.WriteFieldName(TEXT("generation"));
            serializer.Write(id.Generation);
            serializer.EndObject();

            serializer.WriteFieldName(TEXT("name"));
            WriteStringJson(serializer, obj->GetName().ToView());

            serializer.WriteFieldName(TEXT("active"));
            serializer.Write(obj->IsActive());

            serializer.WriteFieldName(TEXT("hasParent"));
            const auto parent    = obj->GetParent();
            const bool hasParent = parent.IsValid();
            serializer.Write(hasParent);
            if (hasParent) {
                serializer.WriteFieldName(TEXT("parent"));
                serializer.BeginObject({});
                serializer.WriteFieldName(TEXT("index"));
                serializer.Write(parent.Index);
                serializer.WriteFieldName(TEXT("generation"));
                serializer.Write(parent.Generation);
                serializer.EndObject();
            }

            serializer.WriteFieldName(TEXT("transform"));
            WriteTransformJson(serializer, obj->GetLocalTransform());

            if (isPrefabRecord) {
                serializer.WriteFieldName(TEXT("prefab"));
                serializer.BeginObject({});
                serializer.WriteFieldName(TEXT("loaderType"));
                WriteNativeStringJson(serializer, prefabDescriptor->LoaderType.ToView());
                serializer.WriteFieldName(TEXT("asset"));
                WriteAssetHandleJson(serializer, prefabDescriptor->AssetHandle);
                serializer.EndObject();
                serializer.EndObject();
                continue;
            }

            TVector<FComponentId> serializableComponents;
            for (const auto& componentId : obj->GetAllComponents()) {
                const auto* entry = registry.Find(componentId.Type);
                if (entry == nullptr) {
                    continue;
                }
                serializableComponents.PushBack(componentId);
            }

            serializer.WriteFieldName(TEXT("components"));
            serializer.BeginArray(static_cast<usize>(serializableComponents.Size()));
            for (const auto& componentId : serializableComponents) {
                const auto* entry = registry.Find(componentId.Type);
                if (entry == nullptr) {
                    continue;
                }

                const auto* component = ResolveComponentBase(componentId);
                const bool  enabled   = component ? component->IsEnabled() : true;

                serializer.BeginObject({});
                serializer.WriteFieldName(TEXT("type"));
                serializer.Write(componentId.Type);
                serializer.WriteFieldName(TEXT("typeName"));
                if (entry->TypeName.IsEmpty()) {
                    serializer.WriteString(TEXT("UnknownComponent"));
                } else {
                    WriteNativeStringJson(serializer, entry->TypeName);
                }
                serializer.WriteFieldName(TEXT("enabled"));
                serializer.Write(enabled);
                serializer.WriteFieldName(TEXT("data"));
                if (entry->SerializeJson != nullptr) {
                    registry.SerializeJson(const_cast<FWorld&>(*this), componentId, serializer);
                } else {
                    serializer.BeginObject({});
                    serializer.EndObject();
                }
                serializer.EndObject();
            }
            serializer.EndArray();

            serializer.EndObject();
        }

        serializer.EndArray();
        serializer.EndObject();
    }

    auto FWorld::DeserializeJson(Core::Container::FNativeStringView jsonText,
        Asset::FAssetManager& assetManager, EWorldDeserializeMode mode) -> TOwner<FWorld> {
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FindObjectValueInsensitive;
        using Core::Utility::Json::FJsonDocument;
        using Core::Utility::Json::FJsonValue;
        using Core::Utility::Json::GetBoolValue;
        using Core::Utility::Json::GetNumberValue;
        using Core::Utility::Json::GetStringValue;

        struct FPendingParentLink {
            FGameObjectId mChild{};
            FGameObjectId mParentSerialized{};
        };

        auto ParseSerializedId = [](const FJsonValue& value, FGameObjectId& outId) -> bool {
            if (value.Type != EJsonType::Object) {
                return false;
            }
            double indexNumber      = 0.0;
            double generationNumber = 0.0;
            if (!GetNumberValue(FindObjectValueInsensitive(value, "index"), indexNumber)) {
                return false;
            }
            if (!GetNumberValue(
                    FindObjectValueInsensitive(value, "generation"), generationNumber)) {
                return false;
            }
            if (indexNumber < 0.0 || generationNumber <= 0.0) {
                return false;
            }
            outId.Index      = static_cast<u32>(indexNumber);
            outId.Generation = static_cast<u32>(generationNumber);
            return true;
        };

        auto ParseTransform = [](const FJsonValue&                       value,
                                  Core::Math::LinAlg::FSpatialTransform& outTransform) -> bool {
            if (value.Type != EJsonType::Object) {
                return false;
            }

            const auto* rotation    = FindObjectValueInsensitive(value, "rotation");
            const auto* translation = FindObjectValueInsensitive(value, "translation");
            const auto* scale       = FindObjectValueInsensitive(value, "scale");
            if (rotation == nullptr || translation == nullptr || scale == nullptr) {
                return false;
            }
            if (rotation->Type != EJsonType::Array || translation->Type != EJsonType::Array
                || scale->Type != EJsonType::Array) {
                return false;
            }
            if (rotation->Array.Size() != 4 || translation->Array.Size() != 3
                || scale->Array.Size() != 3) {
                return false;
            }

            double valueNumber = 0.0;
            if (!GetNumberValue(rotation->Array[0], valueNumber)) {
                return false;
            }
            outTransform.Rotation.x = static_cast<f32>(valueNumber);
            if (!GetNumberValue(rotation->Array[1], valueNumber)) {
                return false;
            }
            outTransform.Rotation.y = static_cast<f32>(valueNumber);
            if (!GetNumberValue(rotation->Array[2], valueNumber)) {
                return false;
            }
            outTransform.Rotation.z = static_cast<f32>(valueNumber);
            if (!GetNumberValue(rotation->Array[3], valueNumber)) {
                return false;
            }
            outTransform.Rotation.w = static_cast<f32>(valueNumber);

            if (!GetNumberValue(translation->Array[0], valueNumber)) {
                return false;
            }
            outTransform.Translation.mComponents[0] = static_cast<f32>(valueNumber);
            if (!GetNumberValue(translation->Array[1], valueNumber)) {
                return false;
            }
            outTransform.Translation.mComponents[1] = static_cast<f32>(valueNumber);
            if (!GetNumberValue(translation->Array[2], valueNumber)) {
                return false;
            }
            outTransform.Translation.mComponents[2] = static_cast<f32>(valueNumber);

            if (!GetNumberValue(scale->Array[0], valueNumber)) {
                return false;
            }
            outTransform.Scale.mComponents[0] = static_cast<f32>(valueNumber);
            if (!GetNumberValue(scale->Array[1], valueNumber)) {
                return false;
            }
            outTransform.Scale.mComponents[1] = static_cast<f32>(valueNumber);
            if (!GetNumberValue(scale->Array[2], valueNumber)) {
                return false;
            }
            outTransform.Scale.mComponents[2] = static_cast<f32>(valueNumber);
            return true;
        };

        FJsonDocument document{};
        if (!document.Parse(jsonText)) {
            return {};
        }

        const FJsonValue* root = document.GetRoot();
        if (root == nullptr || root->Type != EJsonType::Object) {
            return {};
        }

        double versionNumber = 0.0;
        if (!GetNumberValue(FindObjectValueInsensitive(*root, "version"), versionNumber)) {
            return {};
        }

        const u32 version = static_cast<u32>(versionNumber);
        Assert(version == kWorldSerializationVersion, TEXT("GameScene.World"),
            "FWorld::DeserializeJson version mismatch. expected={}, actual={}",
            kWorldSerializationVersion, version);
        if (version != kWorldSerializationVersion) {
            return {};
        }

        double worldIdNumber = 0.0;
        if (!GetNumberValue(FindObjectValueInsensitive(*root, "worldId"), worldIdNumber)) {
            return {};
        }

        const auto* objectsValue = FindObjectValueInsensitive(*root, "objects");
        if (objectsValue == nullptr || objectsValue->Type != EJsonType::Array) {
            return {};
        }

        auto world = Core::Container::MakeUnique<FWorld>(static_cast<u32>(worldIdNumber));
        world->SetDeserializeMode(mode);
        auto&                                                     registry = GetComponentRegistry();

        THashMap<FGameObjectId, FGameObjectId, FGameObjectIdHash> objectIdRemap{};
        TVector<FPendingParentLink>                               pendingParents{};
        objectIdRemap.Reserve(objectsValue->Array.Size());
        pendingParents.Reserve(objectsValue->Array.Size());

        for (const auto* objectNode : objectsValue->Array) {
            if (objectNode == nullptr || objectNode->Type != EJsonType::Object) {
                return {};
            }

            Core::Container::FNativeString recordKindText{};
            if (!GetStringValue(
                    FindObjectValueInsensitive(*objectNode, "recordKind"), recordKindText)) {
                return {};
            }

            FGameObjectId serializedId{};
            const auto*   idNode = FindObjectValueInsensitive(*objectNode, "id");
            if (idNode == nullptr || !ParseSerializedId(*idNode, serializedId)) {
                return {};
            }
            serializedId.WorldId = world->GetWorldId();

            Core::Container::FNativeString objectName{};
            if (!GetStringValue(FindObjectValueInsensitive(*objectNode, "name"), objectName)) {
                return {};
            }

            bool active = true;
            if (!GetBoolValue(FindObjectValueInsensitive(*objectNode, "active"), active)) {
                return {};
            }

            bool hasParent = false;
            if (!GetBoolValue(FindObjectValueInsensitive(*objectNode, "hasParent"), hasParent)) {
                return {};
            }

            FGameObjectId parentSerializedId{};
            if (hasParent) {
                const auto* parentNode = FindObjectValueInsensitive(*objectNode, "parent");
                if (parentNode == nullptr || !ParseSerializedId(*parentNode, parentSerializedId)) {
                    return {};
                }
                parentSerializedId.WorldId = world->GetWorldId();
            }

            Core::Math::LinAlg::FSpatialTransform localTransform{};
            const auto* transformNode = FindObjectValueInsensitive(*objectNode, "transform");
            if (transformNode == nullptr || !ParseTransform(*transformNode, localTransform)) {
                return {};
            }

            const bool isRawRecord =
                Core::Utility::String::EqualLiteralI(recordKindText.ToView(), "raw");
            const bool isPrefabRecord =
                Core::Utility::String::EqualLiteralI(recordKindText.ToView(), "prefab");
            if (!isRawRecord && !isPrefabRecord) {
                return {};
            }

            if (isRawRecord) {
                auto* object = world->CreateGameObjectWithId(serializedId);
                if (object == nullptr) {
                    return {};
                }
                const auto actualId = object->GetId();
                object->SetName(Core::Utility::String::FromUtf8(objectName).ToView());
                object->SetLocalTransform(localTransform);
                world->SetGameObjectActive(actualId, active);

                objectIdRemap[serializedId] = actualId;
                if (hasParent) {
                    pendingParents.PushBack({ actualId, parentSerializedId });
                }

                const auto* componentsNode = FindObjectValueInsensitive(*objectNode, "components");
                if (componentsNode == nullptr || componentsNode->Type != EJsonType::Array) {
                    return {};
                }

                for (const auto* componentNode : componentsNode->Array) {
                    if (componentNode == nullptr || componentNode->Type != EJsonType::Object) {
                        return {};
                    }

                    double typeNumber = 0.0;
                    if (!GetNumberValue(
                            FindObjectValueInsensitive(*componentNode, "type"), typeNumber)) {
                        return {};
                    }
                    auto componentType = static_cast<FComponentTypeHash>(typeNumber);

                    bool enabled = true;
                    if (!GetBoolValue(
                            FindObjectValueInsensitive(*componentNode, "enabled"), enabled)) {
                        return {};
                    }

                    const auto* dataNode = FindObjectValueInsensitive(*componentNode, "data");
                    if (dataNode == nullptr || dataNode->Type != EJsonType::Object) {
                        return {};
                    }

                    const auto* componentEntry = registry.Find(componentType);
                    if (componentEntry == nullptr) {
                        Core::Container::FNativeString typeNameText{};
                        if (GetStringValue(FindObjectValueInsensitive(*componentNode, "typeName"),
                                typeNameText)) {
                            componentEntry = registry.FindByTypeName(typeNameText.ToView());
                            if (componentEntry != nullptr) {
                                componentType = componentEntry->TypeHash;
                            }
                        }
                    }
                    Assert(componentEntry != nullptr && componentEntry->Create != nullptr
                            && componentEntry->DeserializeJson != nullptr,
                        TEXT("GameScene.World"),
                        "Component type is not json-deserializable. type={}", componentType);
                    if (componentEntry == nullptr || componentEntry->DeserializeJson == nullptr) {
                        return {};
                    }

                    const auto componentId = world->CreateComponent(actualId, componentType);
                    if (!componentId.IsValid()) {
                        return {};
                    }

                    registry.DeserializeJson(*world, componentId, *dataNode);

                    auto* component = world->ResolveComponentBase(componentId);
                    if (component == nullptr) {
                        return {};
                    }

                    component->Initialize(world.Get(), componentId, actualId);
                    if (!enabled) {
                        component->mEnabled = true;
                        component->SetEnabled(false);
                    }
                }
                continue;
            }

            const auto* prefabNode = FindObjectValueInsensitive(*objectNode, "prefab");
            if (prefabNode == nullptr || prefabNode->Type != EJsonType::Object) {
                return {};
            }

            Core::Container::FNativeString loaderType{};
            if (!GetStringValue(
                    FindObjectValueInsensitive(*prefabNode, "loaderType"), loaderType)) {
                return {};
            }

            Asset::FAssetHandle prefabAsset{};
            const auto*         assetNode = FindObjectValueInsensitive(*prefabNode, "asset");
            if (assetNode == nullptr || !ParseAssetHandleJson(*assetNode, prefabAsset)) {
                return {};
            }

            const auto prefabResult =
                Engine::GameSceneAsset::GetPrefabInstantiatorRegistry().Instantiate(
                    loaderType.ToView(), *world, assetManager, prefabAsset);
            if (!prefabResult.Root.IsValid() || !world->IsAlive(prefabResult.Root)) {
                return {};
            }

            auto* prefabRoot = world->ResolveGameObject(prefabResult.Root);
            if (prefabRoot == nullptr) {
                return {};
            }

            prefabRoot->SetName(Core::Utility::String::FromUtf8(objectName).ToView());
            prefabRoot->SetLocalTransform(localTransform);
            world->SetGameObjectActive(prefabResult.Root, active);

            Engine::GameSceneAsset::FPrefabDescriptor descriptor{};
            descriptor.LoaderType  = loaderType;
            descriptor.AssetHandle = prefabAsset;
            world->RegisterPrefabRoot(prefabResult.Root, descriptor);

            objectIdRemap[serializedId] = prefabResult.Root;
            if (hasParent) {
                pendingParents.PushBack({ prefabResult.Root, parentSerializedId });
            }
        }

        for (const auto& link : pendingParents) {
            auto parentIt = objectIdRemap.FindIt(link.mParentSerialized);
            if (parentIt == objectIdRemap.end()) {
                return {};
            }

            auto* childObject = world->ResolveGameObject(link.mChild);
            if (childObject == nullptr) {
                return {};
            }
            childObject->SetParent(parentIt->second);
        }

        world->mFreeGameObjects.Clear();
        for (u32 index = 0U; index < static_cast<u32>(world->mGameObjects.Size()); ++index) {
            const auto& slot = world->mGameObjects[index];
            if (!slot.Alive || !slot.Handle) {
                world->mFreeGameObjects.PushBack(index);
            }
        }

        world->SetDeserializeMode(EWorldDeserializeMode::NormalRuntime);
        world->UpdateTransforms();
        return world;
    }

    auto FWorld::Deserialize(Core::Reflection::IDeserializer& deserializer,
        Asset::FAssetManager& assetManager, EWorldDeserializeMode mode) -> TOwner<FWorld> {
        struct FPendingParentLink {
            FGameObjectId mChild{};
            FGameObjectId mParentSerialized{};
        };

        const u32 version = deserializer.Read<u32>();
        Assert(version == kWorldSerializationVersion, TEXT("GameScene.World"),
            "FWorld::Deserialize version mismatch. expected={}, actual={}",
            kWorldSerializationVersion, version);

        const u32 worldId     = deserializer.Read<u32>();
        const u32 objectCount = deserializer.Read<u32>();

        auto      world = Core::Container::MakeUnique<FWorld>(worldId);
        world->SetDeserializeMode(mode);
        auto&                                                     registry = GetComponentRegistry();

        THashMap<FGameObjectId, FGameObjectId, FGameObjectIdHash> objectIdRemap{};
        TVector<FPendingParentLink>                               pendingParents{};
        pendingParents.Reserve(objectCount);

        for (u32 recordIndex = 0U; recordIndex < objectCount; ++recordIndex) {
            const auto recordKindRaw = deserializer.Read<u8>();
            Assert(recordKindRaw <= static_cast<u8>(EWorldObjectRecordKind::Prefab),
                TEXT("GameScene.World"), "Invalid world object record kind: {}", recordKindRaw);

            const auto    recordKind = static_cast<EWorldObjectRecordKind>(recordKindRaw);

            FGameObjectId serializedId{};
            serializedId.Index      = deserializer.Read<u32>();
            serializedId.Generation = deserializer.Read<u32>();
            serializedId.WorldId    = worldId;

            const auto    objectName = ReadString(deserializer);
            const bool    active     = deserializer.Read<bool>();
            const bool    hasParent  = deserializer.Read<bool>();
            FGameObjectId parentSerializedId{};
            if (hasParent) {
                parentSerializedId.Index      = deserializer.Read<u32>();
                parentSerializedId.Generation = deserializer.Read<u32>();
                parentSerializedId.WorldId    = worldId;
            }

            const auto localTransform = ReadTransform(deserializer);

            if (recordKind == EWorldObjectRecordKind::Raw) {
                auto* object = world->CreateGameObjectWithId(serializedId);
                Assert(object != nullptr, TEXT("GameScene.World"),
                    "Failed to create raw game object during deserialize. index={}, generation={}",
                    serializedId.Index, serializedId.Generation);

                object->SetName(objectName.ToView());
                object->SetLocalTransform(localTransform);
                object->SetActive(active);

                const auto actualId         = object->GetId();
                objectIdRemap[serializedId] = actualId;
                if (hasParent) {
                    pendingParents.PushBack({ actualId, parentSerializedId });
                }

                const u32 componentCount = deserializer.Read<u32>();
                for (u32 componentIndex = 0U; componentIndex < componentCount; ++componentIndex) {
                    const auto  componentType = deserializer.Read<FComponentTypeHash>();
                    const bool  enabled       = deserializer.Read<bool>();

                    const auto* componentEntry = registry.Find(componentType);
                    Assert(componentEntry != nullptr && componentEntry->Create != nullptr
                            && componentEntry->Deserialize != nullptr,
                        TEXT("GameScene.World"),
                        "Component type is not deserializable during world deserialize. type={}",
                        componentType);

                    const auto componentId = world->CreateComponent(actualId, componentType);
                    Assert(componentId.IsValid(), TEXT("GameScene.World"),
                        "Failed to create component during world deserialize. type={}",
                        componentType);

                    registry.Deserialize(*world, componentId, deserializer);

                    auto* component = world->ResolveComponentBase(componentId);
                    Assert(component != nullptr, TEXT("GameScene.World"),
                        "Failed to resolve component after deserialize. type={}", componentType);

                    component->Initialize(world.Get(), componentId, actualId);
                    if (enabled) {
                        component->mEnabled = true;
                        world->OnComponentEnabledChanged(componentId, actualId, true);
                    } else {
                        component->mEnabled = true;
                        component->SetEnabled(false);
                    }
                }
                continue;
            }

            const auto loaderType  = ReadNativeString(deserializer);
            const auto assetHandle = Asset::FAssetHandle::Deserialize(deserializer);

            const auto prefabResult =
                Engine::GameSceneAsset::GetPrefabInstantiatorRegistry().Instantiate(
                    loaderType.ToView(), *world, assetManager, assetHandle);
            Assert(prefabResult.Root.IsValid() && world->IsAlive(prefabResult.Root),
                TEXT("GameScene.World"),
                "Failed to instantiate prefab during world deserialize. loaderType={}",
                loaderType.ToView());

            auto* prefabRoot = world->ResolveGameObject(prefabResult.Root);
            Assert(prefabRoot != nullptr, TEXT("GameScene.World"),
                "Prefab instantiate returned an invalid root object.");

            prefabRoot->SetName(objectName.ToView());
            prefabRoot->SetLocalTransform(localTransform);
            world->SetGameObjectActive(prefabResult.Root, active);

            Engine::GameSceneAsset::FPrefabDescriptor descriptor{};
            descriptor.LoaderType  = loaderType;
            descriptor.AssetHandle = assetHandle;
            world->RegisterPrefabRoot(prefabResult.Root, descriptor);

            objectIdRemap[serializedId] = prefabResult.Root;
            if (hasParent) {
                pendingParents.PushBack({ prefabResult.Root, parentSerializedId });
            }
        }

        for (const auto& link : pendingParents) {
            auto parentIt = objectIdRemap.FindIt(link.mParentSerialized);
            Assert(parentIt != objectIdRemap.end(), TEXT("GameScene.World"),
                "Missing parent remap during world deserialize. parent index={}, generation={}",
                link.mParentSerialized.Index, link.mParentSerialized.Generation);

            const auto parentActual = parentIt->second;
            auto*      childObject  = world->ResolveGameObject(link.mChild);
            Assert(childObject != nullptr, TEXT("GameScene.World"),
                "Missing child object while applying parent links during world deserialize.");
            childObject->SetParent(parentActual);
        }

        world->mFreeGameObjects.Clear();
        for (u32 index = 0U; index < static_cast<u32>(world->mGameObjects.Size()); ++index) {
            const auto& slot = world->mGameObjects[index];
            if (!slot.Alive || !slot.Handle) {
                world->mFreeGameObjects.PushBack(index);
            }
        }

        world->SetDeserializeMode(EWorldDeserializeMode::NormalRuntime);
        world->UpdateTransforms();
        return world;
    }

    auto FWorld::ShouldInvokeComponentLifecycles() const noexcept -> bool {
        return mDeserializeMode == EWorldDeserializeMode::NormalRuntime;
    }

    void FWorld::SetDeserializeMode(EWorldDeserializeMode mode) noexcept {
        mDeserializeMode = mode;
    }

    void FWorld::AddActiveComponent(TVector<FComponentId>& list, FComponentId id) {
        const auto count = static_cast<u32>(list.Size());
        for (u32 index = 0; index < count; ++index) {
            if (list[index] == id) {
                return;
            }
        }
        list.PushBack(id);
    }

    void FWorld::RemoveActiveComponent(TVector<FComponentId>& list, FComponentId id) {
        const auto count = static_cast<u32>(list.Size());
        for (u32 index = 0; index < count; ++index) {
            if (list[index] == id) {
                if (index != count - 1) {
                    list[index] = list[count - 1];
                }
                list.PopBack();
                return;
            }
        }
    }

    void FWorld::OnComponentCreated(FComponentId id, FGameObjectId owner) {
        if (!IsAlive(id) || !IsGameObjectActive(owner)) {
            return;
        }

        if (id.Type == kCameraComponentType) {
            const auto& component = ResolveComponent<FCameraComponent>(id);
            if (component.IsEnabled()) {
                AddActiveComponent(mActiveCameraComponents, id);
            }
            return;
        }

        if (id.Type == kStaticMeshComponentType) {
            const auto& component = ResolveComponent<FStaticMeshFilterComponent>(id);
            if (component.IsEnabled()) {
                AddActiveComponent(mActiveStaticMeshComponents, id);
            }
            return;
        }

        if (id.Type == kMeshMaterialComponentType) {
            const auto& component = ResolveComponent<FMeshMaterialComponent>(id);
            if (component.IsEnabled()) {
                AddActiveComponent(mActiveMeshMaterialComponents, id);
            }
        }

        if (id.Type == kDirectionalLightComponentType) {
            const auto& component = ResolveComponent<FDirectionalLightComponent>(id);
            if (component.IsEnabled()) {
                AddActiveComponent(mActiveDirectionalLightComponents, id);
            }
            return;
        }

        if (id.Type == kPointLightComponentType) {
            const auto& component = ResolveComponent<FPointLightComponent>(id);
            if (component.IsEnabled()) {
                AddActiveComponent(mActivePointLightComponents, id);
            }
            return;
        }

        if (id.Type == kSkyCubeComponentType) {
            const auto& component = ResolveComponent<FSkyCubeComponent>(id);
            if (component.IsEnabled()) {
                AddActiveComponent(mActiveSkyCubeComponents, id);
            }
            return;
        }

        if (id.Type == kPbrSkyComponentType) {
            const auto& component = ResolveComponent<FPbrSkyComponent>(id);
            if (component.IsEnabled()) {
                AddActiveComponent(mActivePbrSkyComponents, id);
            }
        }
    }

    void FWorld::OnComponentDestroyed(FComponentId id, FGameObjectId /*owner*/) {
        if (id.Type == kCameraComponentType) {
            RemoveActiveComponent(mActiveCameraComponents, id);
            return;
        }

        if (id.Type == kStaticMeshComponentType) {
            RemoveActiveComponent(mActiveStaticMeshComponents, id);
            return;
        }

        if (id.Type == kMeshMaterialComponentType) {
            RemoveActiveComponent(mActiveMeshMaterialComponents, id);
            return;
        }

        if (id.Type == kDirectionalLightComponentType) {
            RemoveActiveComponent(mActiveDirectionalLightComponents, id);
            return;
        }

        if (id.Type == kPointLightComponentType) {
            RemoveActiveComponent(mActivePointLightComponents, id);
            return;
        }

        if (id.Type == kSkyCubeComponentType) {
            RemoveActiveComponent(mActiveSkyCubeComponents, id);
            return;
        }

        if (id.Type == kPbrSkyComponentType) {
            RemoveActiveComponent(mActivePbrSkyComponents, id);
        }
    }

    void FWorld::OnComponentEnabledChanged(FComponentId id, FGameObjectId owner, bool enabled) {
        if (id.Type == kCameraComponentType) {
            if (enabled && IsGameObjectActive(owner)) {
                AddActiveComponent(mActiveCameraComponents, id);
            } else {
                RemoveActiveComponent(mActiveCameraComponents, id);
            }
            return;
        }

        if (id.Type == kStaticMeshComponentType) {
            if (enabled && IsGameObjectActive(owner)) {
                AddActiveComponent(mActiveStaticMeshComponents, id);
            } else {
                RemoveActiveComponent(mActiveStaticMeshComponents, id);
            }
            return;
        }

        if (id.Type == kMeshMaterialComponentType) {
            if (enabled && IsGameObjectActive(owner)) {
                AddActiveComponent(mActiveMeshMaterialComponents, id);
            } else {
                RemoveActiveComponent(mActiveMeshMaterialComponents, id);
            }
            return;
        }

        if (id.Type == kDirectionalLightComponentType) {
            if (enabled && IsGameObjectActive(owner)) {
                AddActiveComponent(mActiveDirectionalLightComponents, id);
            } else {
                RemoveActiveComponent(mActiveDirectionalLightComponents, id);
            }
            return;
        }

        if (id.Type == kPointLightComponentType) {
            if (enabled && IsGameObjectActive(owner)) {
                AddActiveComponent(mActivePointLightComponents, id);
            } else {
                RemoveActiveComponent(mActivePointLightComponents, id);
            }
            return;
        }

        if (id.Type == kSkyCubeComponentType) {
            if (enabled && IsGameObjectActive(owner)) {
                AddActiveComponent(mActiveSkyCubeComponents, id);
            } else {
                RemoveActiveComponent(mActiveSkyCubeComponents, id);
            }
            return;
        }

        if (id.Type == kPbrSkyComponentType) {
            if (enabled && IsGameObjectActive(owner)) {
                AddActiveComponent(mActivePbrSkyComponents, id);
            } else {
                RemoveActiveComponent(mActivePbrSkyComponents, id);
            }
        }
    }

    void FWorld::OnGameObjectActiveChanged(FGameObjectId owner, bool active) {
        auto* obj = ResolveGameObject(owner);
        if (obj == nullptr) {
            return;
        }

        const auto components = obj->GetAllComponents();
        for (const auto& id : components) {
            if (!IsAlive(id)) {
                continue;
            }

            if (id.Type == kCameraComponentType) {
                if (active && ResolveComponent<FCameraComponent>(id).IsEnabled()) {
                    AddActiveComponent(mActiveCameraComponents, id);
                } else {
                    RemoveActiveComponent(mActiveCameraComponents, id);
                }
                continue;
            }

            if (id.Type == kStaticMeshComponentType) {
                if (active && ResolveComponent<FStaticMeshFilterComponent>(id).IsEnabled()) {
                    AddActiveComponent(mActiveStaticMeshComponents, id);
                } else {
                    RemoveActiveComponent(mActiveStaticMeshComponents, id);
                }
                continue;
            }

            if (id.Type == kMeshMaterialComponentType) {
                if (active && ResolveComponent<FMeshMaterialComponent>(id).IsEnabled()) {
                    AddActiveComponent(mActiveMeshMaterialComponents, id);
                } else {
                    RemoveActiveComponent(mActiveMeshMaterialComponents, id);
                }
                continue;
            }

            if (id.Type == kDirectionalLightComponentType) {
                if (active && ResolveComponent<FDirectionalLightComponent>(id).IsEnabled()) {
                    AddActiveComponent(mActiveDirectionalLightComponents, id);
                } else {
                    RemoveActiveComponent(mActiveDirectionalLightComponents, id);
                }
                continue;
            }

            if (id.Type == kPointLightComponentType) {
                if (active && ResolveComponent<FPointLightComponent>(id).IsEnabled()) {
                    AddActiveComponent(mActivePointLightComponents, id);
                } else {
                    RemoveActiveComponent(mActivePointLightComponents, id);
                }
                continue;
            }

            if (id.Type == kSkyCubeComponentType) {
                if (active && ResolveComponent<FSkyCubeComponent>(id).IsEnabled()) {
                    AddActiveComponent(mActiveSkyCubeComponents, id);
                } else {
                    RemoveActiveComponent(mActiveSkyCubeComponents, id);
                }
                continue;
            }

            if (id.Type == kPbrSkyComponentType) {
                if (active && ResolveComponent<FPbrSkyComponent>(id).IsEnabled()) {
                    AddActiveComponent(mActivePbrSkyComponents, id);
                } else {
                    RemoveActiveComponent(mActivePbrSkyComponents, id);
                }
            }
        }
    }

    void FWorld::LinkComponentToOwner(FGameObjectId owner, FComponentId id) {
        auto* obj = ResolveGameObject(owner);
        if (obj == nullptr) {
            return;
        }
        obj->AddComponentId(id);
    }

    void FWorld::UnlinkComponentFromOwner(FGameObjectId owner, FComponentId id) {
        auto* obj = ResolveGameObject(owner);
        if (obj == nullptr) {
            return;
        }
        obj->RemoveComponentId(id);
    }

    auto FWorld::ResolveGameObject(FGameObjectId id) -> FGameObject* {
        if (!IsAlive(id)) {
            return nullptr;
        }
        return mGameObjects[id.Index].Handle.Get();
    }

    auto FWorld::ResolveGameObject(FGameObjectId id) const -> const FGameObject* {
        if (!IsAlive(id)) {
            return nullptr;
        }
        return mGameObjects[id.Index].Handle.Get();
    }

    auto FWorld::ResolveComponentBase(FComponentId id) -> FComponent* {
        if (!id.IsValid()) {
            return nullptr;
        }
        auto* storage = FindComponentStorage(id.Type);
        if (storage == nullptr) {
            return nullptr;
        }
        return storage->ResolveBase(id);
    }

    auto FWorld::ResolveComponentBase(FComponentId id) const -> const FComponent* {
        if (!id.IsValid()) {
            return nullptr;
        }
        auto* storage = FindComponentStorage(id.Type);
        if (storage == nullptr) {
            return nullptr;
        }
        return storage->ResolveBase(id);
    }

    auto FWorld::UpdateTransformRecursive(FGameObjectId id, u32 updateId) -> bool {
        auto* obj = ResolveGameObject(id);
        if (obj == nullptr) {
            return false;
        }

        if (obj->mTransformUpdateId == updateId) {
            return obj->mTransformChangedId == updateId;
        }

        bool               parentChanged = false;
        const auto         parentId      = obj->mParent;
        const FGameObject* parentObj     = nullptr;
        if (parentId.IsValid()) {
            parentChanged = UpdateTransformRecursive(parentId, updateId);
            parentObj     = ResolveGameObject(parentId);
        }

        const bool shouldUpdate = obj->mTransformDirty || parentChanged;
        if (shouldUpdate) {
            if (parentObj != nullptr) {
                obj->UpdateWorldTransform(parentObj->GetWorldTransform());
            } else {
                obj->UpdateWorldTransform();
            }
            obj->mTransformChangedId = updateId;
        }

        obj->mTransformUpdateId = updateId;
        return shouldUpdate;
    }

    auto FWorld::FindComponentStorage(FComponentTypeHash type) const -> FComponentStorageBase* {
        auto it = mComponentStorage.FindIt(type);
        if (it == mComponentStorage.end()) {
            return nullptr;
        }
        return it->second.Get();
    }

    void FGameObject::SetName(FStringView name) { mName = FString(name); }

    auto FGameObject::AddComponentByType(FComponentTypeHash type) -> FComponentId {
        if (mWorld == nullptr) {
            return {};
        }
        return mWorld->CreateComponent(mId, type);
    }

    void FGameObject::RemoveComponent(FComponentId id) {
        if (mWorld == nullptr) {
            return;
        }
        mWorld->DestroyComponent(id);
    }

    auto FGameObject::GetAllComponents() const -> TVector<FComponentId> { return mComponents; }

    void FGameObject::AddComponentId(FComponentId id) { mComponents.PushBack(id); }

    void FGameObject::RemoveComponentId(FComponentId id) {
        const auto count = static_cast<u32>(mComponents.Size());
        for (u32 index = 0; index < count; ++index) {
            if (mComponents[index] == id) {
                if (index != count - 1) {
                    mComponents[index] = mComponents[count - 1];
                }
                mComponents.PopBack();
                return;
            }
        }
    }

    auto FGameObjectView::IsValid() const noexcept -> bool {
        return mWorld != nullptr && mWorld->IsAlive(mId);
    }

    void FGameObjectView::SetActive(bool active) {
        if (mWorld == nullptr) {
            return;
        }
        mWorld->SetGameObjectActive(mId, active);
    }

    auto FGameObjectView::IsActive() const -> bool {
        return mWorld != nullptr && mWorld->IsGameObjectActive(mId);
    }

    auto FGameObjectView::GetLocalTransform() const noexcept -> LinAlg::FSpatialTransform {
        if (mWorld == nullptr) {
            return LinAlg::FSpatialTransform::Identity();
        }
        const auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return LinAlg::FSpatialTransform::Identity();
        }
        return obj->GetLocalTransform();
    }

    auto FGameObjectView::GetWorldTransform() const noexcept -> LinAlg::FSpatialTransform {
        if (mWorld == nullptr) {
            return LinAlg::FSpatialTransform::Identity();
        }
        const auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return LinAlg::FSpatialTransform::Identity();
        }
        return obj->GetWorldTransform();
    }

    void FGameObjectView::SetLocalTransform(const LinAlg::FSpatialTransform& transform) {
        if (mWorld == nullptr) {
            return;
        }
        auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return;
        }
        obj->SetLocalTransform(transform);
    }

    void FGameObjectView::SetWorldTransform(const LinAlg::FSpatialTransform& transform) {
        if (mWorld == nullptr) {
            return;
        }
        auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return;
        }
        obj->SetWorldTransform(transform);
    }

    auto FGameObjectView::GetParent() const noexcept -> FGameObjectId {
        if (mWorld == nullptr) {
            return {};
        }
        const auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return {};
        }
        return obj->GetParent();
    }

    auto FGameObjectView::GetName() const -> Core::Container::FString {
        if (mWorld == nullptr) {
            return {};
        }
        return mWorld->GetGameObjectName(mId);
    }

    auto FGameObjectView::GetAllComponents() const -> TVector<FComponentId> {
        if (mWorld == nullptr) {
            return {};
        }
        return mWorld->GetAllComponents(mId);
    }

    void FGameObjectView::SetParent(FGameObjectId parent) {
        if (mWorld == nullptr) {
            return;
        }
        auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return;
        }
        obj->SetParent(parent);
    }

    void FGameObjectView::ClearParent() {
        if (mWorld == nullptr) {
            return;
        }
        auto* obj = mWorld->ResolveGameObject(mId);
        if (obj == nullptr) {
            return;
        }
        obj->ClearParent();
    }
} // namespace AltinaEngine::GameScene
