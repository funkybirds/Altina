#include "Engine/GameScene/PbrSkyComponent.h"

#include "Engine/GameScene/ComponentRegistry.h"
#include "Engine/GameScene/World.h"

namespace AltinaEngine::GameScene {
    namespace {
        void SerializePbrSkyParameters(
            Core::Reflection::ISerializer& serializer, const FPbrSkyParameters& parameters) {
            serializer.Write(parameters.mRayleighScattering.X());
            serializer.Write(parameters.mRayleighScattering.Y());
            serializer.Write(parameters.mRayleighScattering.Z());
            serializer.Write(parameters.mRayleighScaleHeightKm);

            serializer.Write(parameters.mMieScattering.X());
            serializer.Write(parameters.mMieScattering.Y());
            serializer.Write(parameters.mMieScattering.Z());
            serializer.Write(parameters.mMieAbsorption.X());
            serializer.Write(parameters.mMieAbsorption.Y());
            serializer.Write(parameters.mMieAbsorption.Z());
            serializer.Write(parameters.mMieScaleHeightKm);
            serializer.Write(parameters.mMieAnisotropy);

            serializer.Write(parameters.mOzoneAbsorption.X());
            serializer.Write(parameters.mOzoneAbsorption.Y());
            serializer.Write(parameters.mOzoneAbsorption.Z());
            serializer.Write(parameters.mOzoneCenterHeightKm);
            serializer.Write(parameters.mOzoneThicknessKm);

            serializer.Write(parameters.mGroundAlbedo.X());
            serializer.Write(parameters.mGroundAlbedo.Y());
            serializer.Write(parameters.mGroundAlbedo.Z());
            serializer.Write(parameters.mSolarTint.X());
            serializer.Write(parameters.mSolarTint.Y());
            serializer.Write(parameters.mSolarTint.Z());
            serializer.Write(parameters.mSolarIlluminance);
            serializer.Write(parameters.mSunAngularRadius);

            serializer.Write(parameters.mPlanetRadiusKm);
            serializer.Write(parameters.mAtmosphereHeightKm);
            serializer.Write(parameters.mViewHeightKm);
            serializer.Write(parameters.mExposure);
        }

        void DeserializePbrSkyParameters(
            Core::Reflection::IDeserializer& deserializer, FPbrSkyParameters& parameters) {
            parameters.mRayleighScattering = Math::FVector3f(
                deserializer.Read<f32>(), deserializer.Read<f32>(), deserializer.Read<f32>());
            parameters.mRayleighScaleHeightKm = deserializer.Read<f32>();

            parameters.mMieScattering = Math::FVector3f(
                deserializer.Read<f32>(), deserializer.Read<f32>(), deserializer.Read<f32>());
            parameters.mMieAbsorption = Math::FVector3f(
                deserializer.Read<f32>(), deserializer.Read<f32>(), deserializer.Read<f32>());
            parameters.mMieScaleHeightKm = deserializer.Read<f32>();
            parameters.mMieAnisotropy    = deserializer.Read<f32>();

            parameters.mOzoneAbsorption = Math::FVector3f(
                deserializer.Read<f32>(), deserializer.Read<f32>(), deserializer.Read<f32>());
            parameters.mOzoneCenterHeightKm = deserializer.Read<f32>();
            parameters.mOzoneThicknessKm    = deserializer.Read<f32>();

            parameters.mGroundAlbedo = Math::FVector3f(
                deserializer.Read<f32>(), deserializer.Read<f32>(), deserializer.Read<f32>());
            parameters.mSolarTint = Math::FVector3f(
                deserializer.Read<f32>(), deserializer.Read<f32>(), deserializer.Read<f32>());
            parameters.mSolarIlluminance = deserializer.Read<f32>();
            parameters.mSunAngularRadius = deserializer.Read<f32>();

            parameters.mPlanetRadiusKm     = deserializer.Read<f32>();
            parameters.mAtmosphereHeightKm = deserializer.Read<f32>();
            parameters.mViewHeightKm       = deserializer.Read<f32>();
            parameters.mExposure           = deserializer.Read<f32>();
        }

        void SerializePbrSkyComponent(
            FWorld& world, FComponentId id, Core::Reflection::ISerializer& serializer) {
            const auto& component = world.ResolveComponent<FPbrSkyComponent>(id);
            SerializePbrSkyParameters(serializer, component.GetParameters());
            serializer.Write(component.GetVersion());
        }

        void DeserializePbrSkyComponent(
            FWorld& world, FComponentId id, Core::Reflection::IDeserializer& deserializer) {
            auto& component = world.ResolveComponent<FPbrSkyComponent>(id);
            DeserializePbrSkyParameters(deserializer, component.mParameters);
            component.SetVersion(deserializer.Read<u64>());
        }

        struct FPbrSkyComponentRegistryHook final {
            FPbrSkyComponentRegistryHook() {
                FComponentTypeEntry entry = BuildComponentTypeEntry<FPbrSkyComponent>();
                entry.Serialize           = &SerializePbrSkyComponent;
                entry.Deserialize         = &DeserializePbrSkyComponent;
                GetComponentRegistry().Register(entry);
            }
        };

        FPbrSkyComponentRegistryHook gPbrSkyComponentRegistryHook{};
    } // namespace

    void FPbrSkyComponent::MarkDirty() noexcept {
        ++mVersion;
        if (mVersion == 0ULL) {
            mVersion = 1ULL;
        }
    }

    void FPbrSkyComponent::SetVersion(u64 version) noexcept {
        mVersion = (version == 0ULL) ? 1ULL : version;
    }
} // namespace AltinaEngine::GameScene
