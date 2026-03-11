#include "Engine/GameScene/CameraComponent.h"
#include "Engine/GameScene/ComponentRegistry.h"
#include "Engine/GameScene/DirectionalLightComponent.h"
#include "Engine/GameScene/MeshMaterialComponent.h"
#include "Engine/GameScene/PointLightComponent.h"
#include "Engine/GameScene/ScriptComponent.h"
#include "Engine/GameScene/SkyCubeComponent.h"
#include "Engine/GameScene/StaticMeshFilterComponent.h"
#include "Engine/GameScene/World.h"
#include "Utility/Json.h"
#include "Utility/String/CodeConvert.h"
#include "Utility/String/UuidParser.h"

namespace AltinaEngine::GameScene {
    namespace {
        using Core::Utility::Json::EJsonType;
        using Core::Utility::Json::FindObjectValueInsensitive;
        using Core::Utility::Json::FJsonValue;
        using Core::Utility::Json::GetBoolValue;
        using Core::Utility::Json::GetNumberValue;
        using Core::Utility::Json::GetStringValue;

        auto ParseAssetHandleJson(const FJsonValue& value, Asset::FAssetHandle& outHandle) -> bool {
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

        void DeserializeCameraComponentJson(
            FWorld& world, FComponentId id, const FJsonValue& value) {
            if (value.Type != EJsonType::Object) {
                return;
            }
            auto&  component = world.ResolveComponent<FCameraComponent>(id);
            double number    = 0.0;
            if (GetNumberValue(FindObjectValueInsensitive(value, "fovYRadians"), number)) {
                component.SetFovYRadians(static_cast<f32>(number));
            }
            if (GetNumberValue(FindObjectValueInsensitive(value, "nearPlane"), number)) {
                component.SetNearPlane(static_cast<f32>(number));
            }
            if (GetNumberValue(FindObjectValueInsensitive(value, "farPlane"), number)) {
                component.SetFarPlane(static_cast<f32>(number));
            }
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

        void DeserializeDirectionalLightComponentJson(
            FWorld& world, FComponentId id, const FJsonValue& value) {
            if (value.Type != EJsonType::Object) {
                return;
            }

            auto& component = world.ResolveComponent<FDirectionalLightComponent>(id);
            if (const auto* colorValue = FindObjectValueInsensitive(value, "color");
                colorValue != nullptr && colorValue->Type == EJsonType::Array
                && colorValue->Array.Size() >= 3) {
                double x = 0.0;
                double y = 0.0;
                double z = 0.0;
                if (GetNumberValue(colorValue->Array[0], x)
                    && GetNumberValue(colorValue->Array[1], y)
                    && GetNumberValue(colorValue->Array[2], z)) {
                    component.mColor = Core::Math::FVector3f(
                        static_cast<f32>(x), static_cast<f32>(y), static_cast<f32>(z));
                }
            }

            double number = 0.0;
            if (GetNumberValue(FindObjectValueInsensitive(value, "intensity"), number)) {
                component.mIntensity = static_cast<f32>(number);
            }
            bool flag = false;
            if (GetBoolValue(FindObjectValueInsensitive(value, "castShadows"), flag)) {
                component.mCastShadows = flag;
            }
            if (GetNumberValue(FindObjectValueInsensitive(value, "shadowCascadeCount"), number)) {
                component.mShadowCascadeCount = static_cast<u32>(number);
            }
            if (GetNumberValue(FindObjectValueInsensitive(value, "shadowSplitLambda"), number)) {
                component.mShadowSplitLambda = static_cast<f32>(number);
            }
            if (GetNumberValue(FindObjectValueInsensitive(value, "shadowMaxDistance"), number)) {
                component.mShadowMaxDistance = static_cast<f32>(number);
            }
            if (GetNumberValue(FindObjectValueInsensitive(value, "shadowMapSize"), number)) {
                component.mShadowMapSize = static_cast<u32>(number);
            }
            if (GetNumberValue(FindObjectValueInsensitive(value, "shadowReceiverBias"), number)) {
                component.mShadowReceiverBias = static_cast<f32>(number);
            }
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

        void DeserializePointLightComponentJson(
            FWorld& world, FComponentId id, const FJsonValue& value) {
            if (value.Type != EJsonType::Object) {
                return;
            }

            auto& component = world.ResolveComponent<FPointLightComponent>(id);
            if (const auto* colorValue = FindObjectValueInsensitive(value, "color");
                colorValue != nullptr && colorValue->Type == EJsonType::Array
                && colorValue->Array.Size() >= 3) {
                double x = 0.0;
                double y = 0.0;
                double z = 0.0;
                if (GetNumberValue(colorValue->Array[0], x)
                    && GetNumberValue(colorValue->Array[1], y)
                    && GetNumberValue(colorValue->Array[2], z)) {
                    component.mColor = Core::Math::FVector3f(
                        static_cast<f32>(x), static_cast<f32>(y), static_cast<f32>(z));
                }
            }

            double number = 0.0;
            if (GetNumberValue(FindObjectValueInsensitive(value, "intensity"), number)) {
                component.mIntensity = static_cast<f32>(number);
            }
            if (GetNumberValue(FindObjectValueInsensitive(value, "range"), number)) {
                component.mRange = static_cast<f32>(number);
            }
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

        void DeserializeStaticMeshComponentJson(
            FWorld& world, FComponentId id, const FJsonValue& value) {
            if (value.Type != EJsonType::Object) {
                return;
            }
            auto* assetValue = FindObjectValueInsensitive(value, "asset");
            if (assetValue == nullptr) {
                return;
            }

            Asset::FAssetHandle handle{};
            if (!ParseAssetHandleJson(*assetValue, handle)) {
                return;
            }

            auto& component = world.ResolveComponent<FStaticMeshFilterComponent>(id);
            component.SetStaticMeshAsset(handle);
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

        void DeserializeMeshMaterialComponentJson(
            FWorld& world, FComponentId id, const FJsonValue& value) {
            if (value.Type != EJsonType::Object) {
                return;
            }

            const auto* slotsValue = FindObjectValueInsensitive(value, "slots");
            if (slotsValue == nullptr || slotsValue->Type != EJsonType::Array) {
                return;
            }

            auto& component = world.ResolveComponent<FMeshMaterialComponent>(id);
            u32   slotIndex = 0U;
            for (const auto* slotValue : slotsValue->Array) {
                if (slotValue == nullptr || slotValue->Type != EJsonType::Object) {
                    ++slotIndex;
                    continue;
                }
                const auto* templateValue = FindObjectValueInsensitive(*slotValue, "template");
                if (templateValue == nullptr) {
                    ++slotIndex;
                    continue;
                }

                Asset::FAssetHandle handle{};
                if (ParseAssetHandleJson(*templateValue, handle)) {
                    component.SetMaterialTemplate(slotIndex, handle);
                }
                ++slotIndex;
            }
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

        void DeserializeScriptComponentJson(
            FWorld& world, FComponentId id, const FJsonValue& value) {
            if (value.Type != EJsonType::Object) {
                return;
            }
            auto&                          component = world.ResolveComponent<FScriptComponent>(id);

            Core::Container::FNativeString assemblyPath{};
            if (GetStringValue(FindObjectValueInsensitive(value, "assemblyPath"), assemblyPath)) {
                component.SetAssemblyPath(assemblyPath.ToView());
            }

            Core::Container::FNativeString typeName{};
            if (GetStringValue(FindObjectValueInsensitive(value, "typeName"), typeName)) {
                component.SetTypeName(typeName.ToView());
            }

            const auto* scriptAssetValue = FindObjectValueInsensitive(value, "scriptAsset");
            if (scriptAssetValue != nullptr) {
                Asset::FAssetHandle handle{};
                if (ParseAssetHandleJson(*scriptAssetValue, handle)) {
                    component.SetScriptAsset(handle);
                }
            }
        }

        void SerializeSkyCubeComponentJson(
            FWorld& world, FComponentId id, Core::Reflection::ISerializer& serializer) {
            const auto& component = world.ResolveComponent<FSkyCubeComponent>(id);
            serializer.BeginObject({});
            serializer.WriteFieldName(TEXT("cubeMapAsset"));
            WriteAssetHandleJson(serializer, component.GetCubeMapAsset());
            serializer.EndObject();
        }

        void DeserializeSkyCubeComponentJson(
            FWorld& world, FComponentId id, const FJsonValue& value) {
            if (value.Type != EJsonType::Object) {
                return;
            }
            const auto* handleValue = FindObjectValueInsensitive(value, "cubeMapAsset");
            if (handleValue == nullptr) {
                return;
            }

            Asset::FAssetHandle handle{};
            if (!ParseAssetHandleJson(*handleValue, handle)) {
                return;
            }

            auto& component = world.ResolveComponent<FSkyCubeComponent>(id);
            component.SetCubeMapAsset(handle);
        }

        template <typename T>
        void RegisterJsonSerializer(FnSerializeJson serializer, FnDeserializeJson deserializer) {
            auto&                    registry = GetComponentRegistry();
            const FComponentTypeHash typeHash = GetComponentTypeHash<T>();

            FComponentTypeEntry      entry{};
            if (const auto* existing = registry.Find(typeHash); existing != nullptr) {
                entry = *existing;
            } else {
                entry = BuildComponentTypeEntry<T>();
            }

            entry.TypeName        = Core::TypeMeta::TMetaTypeInfo<T>::kName;
            entry.SerializeJson   = serializer;
            entry.DeserializeJson = deserializer;
            registry.Register(entry);
        }
    } // namespace

    void RegisterComponentJson_AltinaEngineEngine() {
        static bool registered = false;
        if (registered) {
            return;
        }
        registered = true;

        RegisterJsonSerializer<FCameraComponent>(
            &SerializeCameraComponentJson, &DeserializeCameraComponentJson);
        RegisterJsonSerializer<FDirectionalLightComponent>(
            &SerializeDirectionalLightComponentJson, &DeserializeDirectionalLightComponentJson);
        RegisterJsonSerializer<FPointLightComponent>(
            &SerializePointLightComponentJson, &DeserializePointLightComponentJson);
        RegisterJsonSerializer<FStaticMeshFilterComponent>(
            &SerializeStaticMeshComponentJson, &DeserializeStaticMeshComponentJson);
        RegisterJsonSerializer<FMeshMaterialComponent>(
            &SerializeMeshMaterialComponentJson, &DeserializeMeshMaterialComponentJson);
        RegisterJsonSerializer<FScriptComponent>(
            &SerializeScriptComponentJson, &DeserializeScriptComponentJson);
        RegisterJsonSerializer<FSkyCubeComponent>(
            &SerializeSkyCubeComponentJson, &DeserializeSkyCubeComponentJson);
    }
} // namespace AltinaEngine::GameScene
