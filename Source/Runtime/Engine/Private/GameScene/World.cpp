#include "Engine/GameScene/World.h"

#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/PointLightComponent.h"
#include "Engine/GameScene/SkyCubeComponent.h"
#include "Engine/GameScene/ScriptComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Reflection/Serializer.h"
#include "Utility/Assert.h"
#include "Utility/String/CodeConvert.h"

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
        const FComponentTypeHash kScriptComponentType = GetComponentTypeHash<FScriptComponent>();
        const FComponentTypeHash kDirectionalLightComponentType =
            GetComponentTypeHash<FDirectionalLightComponent>();
        const FComponentTypeHash kPointLightComponentType =
            GetComponentTypeHash<FPointLightComponent>();
        const FComponentTypeHash kSkyCubeComponentType = GetComponentTypeHash<FSkyCubeComponent>();

        enum class EWorldObjectRecordKind : u8 {
            Raw    = 0U,
            Prefab = 1U,
        };

        constexpr u32 kWorldSerializationVersion = 2U;

        auto          WriteTransform(Core::Reflection::ISerializer& serializer,
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
            serializer.Write(static_cast<u8>(handle.Type));
            serializer.WriteFieldName(TEXT("uuid"));
            const auto uuidText = handle.Uuid.ToString();
            serializer.WriteString(uuidText.ToView());
            serializer.EndObject();
        }

        auto WriteCameraComponentJson(
            Core::Reflection::ISerializer& serializer, const FCameraComponent& component) -> void {
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("fovYRadians"));
            serializer.Write(component.GetFovYRadians());
            serializer.WriteFieldName(TEXT("nearPlane"));
            serializer.Write(component.GetNearPlane());
            serializer.WriteFieldName(TEXT("farPlane"));
            serializer.Write(component.GetFarPlane());
            serializer.EndObject();
        }

        auto WriteStaticMeshComponentJson(Core::Reflection::ISerializer& serializer,
            const FStaticMeshFilterComponent&                            component) -> void {
            const auto& mesh = component.GetStaticMesh();
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("asset"));
            WriteAssetHandleJson(serializer, component.GetStaticMeshAsset());
            serializer.WriteFieldName(TEXT("lodCount"));
            serializer.Write(mesh.GetLodCount());
            serializer.WriteFieldName(TEXT("valid"));
            serializer.Write(mesh.IsValid());
            serializer.WriteFieldName(TEXT("lods"));
            serializer.BeginArray(mesh.Lods.Size());
            for (const auto& lod : mesh.Lods) {
                serializer.BeginObject({});
                serializer.WriteFieldName(TEXT("vertexCount"));
                serializer.Write(lod.GetVertexCount());
                serializer.WriteFieldName(TEXT("indexCount"));
                serializer.Write(lod.GetIndexCount());
                serializer.WriteFieldName(TEXT("sectionCount"));
                serializer.Write(static_cast<u32>(lod.Sections.Size()));
                serializer.WriteFieldName(TEXT("screenSize"));
                serializer.Write(lod.ScreenSize);
                serializer.EndObject();
            }
            serializer.EndArray();
            serializer.EndObject();
        }

        auto WriteMeshMaterialComponentJson(Core::Reflection::ISerializer& serializer,
            const FMeshMaterialComponent&                                  component) -> void {
            const auto& materials = component.GetMaterials();
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("slotCount"));
            serializer.Write(static_cast<u32>(materials.Size()));
            serializer.WriteFieldName(TEXT("slots"));
            serializer.BeginArray(materials.Size());
            for (const auto& slot : materials) {
                serializer.BeginObject({});
                serializer.WriteFieldName(TEXT("template"));
                WriteAssetHandleJson(serializer, slot.Template);
                serializer.WriteFieldName(TEXT("paramCounts"));
                serializer.BeginObject({});
                serializer.WriteFieldName(TEXT("scalars"));
                serializer.Write(static_cast<u32>(slot.Parameters.GetScalars().Size()));
                serializer.WriteFieldName(TEXT("vectors"));
                serializer.Write(static_cast<u32>(slot.Parameters.GetVectors().Size()));
                serializer.WriteFieldName(TEXT("matrices"));
                serializer.Write(static_cast<u32>(slot.Parameters.GetMatrices().Size()));
                serializer.WriteFieldName(TEXT("textures"));
                serializer.Write(static_cast<u32>(slot.Parameters.GetTextures().Size()));
                serializer.WriteFieldName(TEXT("hash"));
                serializer.Write(slot.Parameters.GetHash());
                serializer.EndObject();
                serializer.EndObject();
            }
            serializer.EndArray();
            serializer.EndObject();
        }

        auto WriteScriptComponentJson(
            Core::Reflection::ISerializer& serializer, const FScriptComponent& component) -> void {
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("assemblyPath"));
            WriteNativeStringJson(serializer, component.GetAssemblyPath());
            serializer.WriteFieldName(TEXT("typeName"));
            WriteNativeStringJson(serializer, component.GetTypeName());
            serializer.WriteFieldName(TEXT("scriptAsset"));
            WriteAssetHandleJson(serializer, component.GetScriptAsset());
            serializer.EndObject();
        }

        auto WriteSkyCubeComponentJson(
            Core::Reflection::ISerializer& serializer, const FSkyCubeComponent& component) -> void {
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("cubeMapAsset"));
            WriteAssetHandleJson(serializer, component.GetCubeMapAsset());
            serializer.EndObject();
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

            const auto serializableComponents = obj->GetAllComponents();

            serializer.WriteFieldName(TEXT("components"));
            serializer.BeginArray(static_cast<usize>(serializableComponents.Size()));
            for (const auto& componentId : serializableComponents) {
                const auto* component = ResolveComponentBase(componentId);
                const bool  enabled   = component ? component->IsEnabled() : true;

                serializer.BeginObject({});
                serializer.WriteFieldName(TEXT("type"));
                serializer.Write(componentId.Type);
                serializer.WriteFieldName(TEXT("typeName"));
                if (componentId.Type == kCameraComponentType) {
                    serializer.WriteString(TEXT("CameraComponent"));
                } else if (componentId.Type == kStaticMeshComponentType) {
                    serializer.WriteString(TEXT("StaticMeshFilterComponent"));
                } else if (componentId.Type == kMeshMaterialComponentType) {
                    serializer.WriteString(TEXT("MeshMaterialComponent"));
                } else if (componentId.Type == kScriptComponentType) {
                    serializer.WriteString(TEXT("ScriptComponent"));
                } else if (componentId.Type == kSkyCubeComponentType) {
                    serializer.WriteString(TEXT("SkyCubeComponent"));
                } else {
                    serializer.WriteString(TEXT("UnknownComponent"));
                }
                serializer.WriteFieldName(TEXT("enabled"));
                serializer.Write(enabled);
                serializer.WriteFieldName(TEXT("data"));
                if (componentId.Type == kCameraComponentType) {
                    WriteCameraComponentJson(
                        serializer, ResolveComponent<FCameraComponent>(componentId));
                } else if (componentId.Type == kStaticMeshComponentType) {
                    WriteStaticMeshComponentJson(
                        serializer, ResolveComponent<FStaticMeshFilterComponent>(componentId));
                } else if (componentId.Type == kMeshMaterialComponentType) {
                    WriteMeshMaterialComponentJson(
                        serializer, ResolveComponent<FMeshMaterialComponent>(componentId));
                } else if (componentId.Type == kScriptComponentType) {
                    WriteScriptComponentJson(
                        serializer, ResolveComponent<FScriptComponent>(componentId));
                } else if (componentId.Type == kSkyCubeComponentType) {
                    WriteSkyCubeComponentJson(
                        serializer, ResolveComponent<FSkyCubeComponent>(componentId));
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

    auto FWorld::Deserialize(Core::Reflection::IDeserializer& deserializer) -> TOwner<FWorld> {
        (void)deserializer;
        Assert(false, TEXT("GameScene.World"),
            "FWorld::Deserialize is disabled in Prefab Serialization V2 phase.");
        // TODO: Re-enable world loading in a follow-up phase by reconstructing prefab roots
        // through PrefabInstantiatorRegistry (loaderType + assetHandle).
        return {};
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
