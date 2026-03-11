#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/ComponentRegistry.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/PointLightComponent.h"
#include "Engine/GameScene/ScriptComponent.h"
#include "Engine/GameScene/SkyCubeComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Utility/String/CodeConvert.h"

namespace AltinaEngine::GameScene {
    namespace {
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

        void SerializeCameraComponentJson(
            FWorld& world, FComponentId id, Core::Reflection::ISerializer& serializer) {
            const auto& component = world.ResolveComponent<FCameraComponent>(id);
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("fovYRadians"));
            serializer.Write(component.GetFovYRadians());
            serializer.WriteFieldName(TEXT("nearPlane"));
            serializer.Write(component.GetNearPlane());
            serializer.WriteFieldName(TEXT("farPlane"));
            serializer.Write(component.GetFarPlane());
            serializer.EndObject();
        }

        void SerializeDirectionalLightComponentJson(
            FWorld& world, FComponentId id, Core::Reflection::ISerializer& serializer) {
            const auto& component = world.ResolveComponent<FDirectionalLightComponent>(id);
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("color"));
            serializer.BeginArray(3);
            serializer.Write(component.mColor.X());
            serializer.Write(component.mColor.Y());
            serializer.Write(component.mColor.Z());
            serializer.EndArray();
            serializer.WriteFieldName(TEXT("intensity"));
            serializer.Write(component.mIntensity);
            serializer.WriteFieldName(TEXT("castShadows"));
            serializer.Write(component.mCastShadows);
            serializer.WriteFieldName(TEXT("shadowCascadeCount"));
            serializer.Write(component.mShadowCascadeCount);
            serializer.WriteFieldName(TEXT("shadowSplitLambda"));
            serializer.Write(component.mShadowSplitLambda);
            serializer.WriteFieldName(TEXT("shadowMaxDistance"));
            serializer.Write(component.mShadowMaxDistance);
            serializer.WriteFieldName(TEXT("shadowMapSize"));
            serializer.Write(component.mShadowMapSize);
            serializer.WriteFieldName(TEXT("shadowReceiverBias"));
            serializer.Write(component.mShadowReceiverBias);
            serializer.EndObject();
        }

        void SerializePointLightComponentJson(
            FWorld& world, FComponentId id, Core::Reflection::ISerializer& serializer) {
            const auto& component = world.ResolveComponent<FPointLightComponent>(id);
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("color"));
            serializer.BeginArray(3);
            serializer.Write(component.mColor.X());
            serializer.Write(component.mColor.Y());
            serializer.Write(component.mColor.Z());
            serializer.EndArray();
            serializer.WriteFieldName(TEXT("intensity"));
            serializer.Write(component.mIntensity);
            serializer.WriteFieldName(TEXT("range"));
            serializer.Write(component.mRange);
            serializer.EndObject();
        }

        void SerializeStaticMeshComponentJson(
            FWorld& world, FComponentId id, Core::Reflection::ISerializer& serializer) {
            const auto& component = world.ResolveComponent<FStaticMeshFilterComponent>(id);
            const auto& mesh      = component.GetStaticMesh();

            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("asset"));
            WriteAssetHandleJson(serializer, component.GetStaticMeshAsset());
            serializer.WriteFieldName(TEXT("lodCount"));
            serializer.Write(mesh.GetLodCount());
            serializer.WriteFieldName(TEXT("valid"));
            serializer.Write(mesh.IsValid());
            serializer.WriteFieldName(TEXT("lods"));
            serializer.BeginArray(mesh.mLods.Size());
            for (const auto& lod : mesh.mLods) {
                serializer.BeginObject({});
                serializer.WriteFieldName(TEXT("vertexCount"));
                serializer.Write(lod.GetVertexCount());
                serializer.WriteFieldName(TEXT("indexCount"));
                serializer.Write(lod.GetIndexCount());
                serializer.WriteFieldName(TEXT("sectionCount"));
                serializer.Write(static_cast<u32>(lod.mSections.Size()));
                serializer.WriteFieldName(TEXT("screenSize"));
                serializer.Write(lod.mScreenSize);
                serializer.EndObject();
            }
            serializer.EndArray();
            serializer.EndObject();
        }

        void SerializeMeshMaterialComponentJson(
            FWorld& world, FComponentId id, Core::Reflection::ISerializer& serializer) {
            const auto& component = world.ResolveComponent<FMeshMaterialComponent>(id);
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

        void SerializeScriptComponentJson(
            FWorld& world, FComponentId id, Core::Reflection::ISerializer& serializer) {
            const auto& component = world.ResolveComponent<FScriptComponent>(id);
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("assemblyPath"));
            WriteNativeStringJson(serializer, component.GetAssemblyPath());
            serializer.WriteFieldName(TEXT("typeName"));
            WriteNativeStringJson(serializer, component.GetTypeName());
            serializer.WriteFieldName(TEXT("scriptAsset"));
            WriteAssetHandleJson(serializer, component.GetScriptAsset());
            serializer.EndObject();
        }

        void SerializeSkyCubeComponentJson(
            FWorld& world, FComponentId id, Core::Reflection::ISerializer& serializer) {
            const auto& component = world.ResolveComponent<FSkyCubeComponent>(id);
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("cubeMapAsset"));
            WriteAssetHandleJson(serializer, component.GetCubeMapAsset());
            serializer.EndObject();
        }

        template <typename T> void RegisterJsonSerializer(FnSerializeJson serializer) {
            auto&                    registry = GetComponentRegistry();
            const FComponentTypeHash typeHash = GetComponentTypeHash<T>();

            FComponentTypeEntry      entry{};
            if (const auto* existing = registry.Find(typeHash); existing != nullptr) {
                entry = *existing;
            } else {
                entry = BuildComponentTypeEntry<T>();
            }

            entry.TypeName      = Core::TypeMeta::TMetaTypeInfo<T>::kName;
            entry.SerializeJson = serializer;
            registry.Register(entry);
        }
    } // namespace

    void RegisterComponentJson_AltinaEngineEngine() {
        static bool registered = false;
        if (registered) {
            return;
        }
        registered = true;

        RegisterJsonSerializer<FCameraComponent>(&SerializeCameraComponentJson);
        RegisterJsonSerializer<FDirectionalLightComponent>(&SerializeDirectionalLightComponentJson);
        RegisterJsonSerializer<FPointLightComponent>(&SerializePointLightComponentJson);
        RegisterJsonSerializer<FStaticMeshFilterComponent>(&SerializeStaticMeshComponentJson);
        RegisterJsonSerializer<FMeshMaterialComponent>(&SerializeMeshMaterialComponentJson);
        RegisterJsonSerializer<FScriptComponent>(&SerializeScriptComponentJson);
        RegisterJsonSerializer<FSkyCubeComponent>(&SerializeSkyCubeComponentJson);
    }
} // namespace AltinaEngine::GameScene
