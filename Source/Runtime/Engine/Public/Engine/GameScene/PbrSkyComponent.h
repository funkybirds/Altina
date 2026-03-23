#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Math/Vector.h"
#include "Reflection/ReflectionAnnotations.h"
#include "Reflection/ReflectionFwd.h"

namespace AltinaEngine::GameScene {
    namespace Math = Core::Math;

    struct AE_ENGINE_API FPbrSkyParameters {
        Math::FVector3f    mRayleighScattering = Math::FVector3f(0.005802f, 0.013558f, 0.033100f);
        f32                mRayleighScaleHeightKm = 8.0f;

        Math::FVector3f    mMieScattering    = Math::FVector3f(0.003996f, 0.003996f, 0.003996f);
        Math::FVector3f    mMieAbsorption    = Math::FVector3f(0.000444f, 0.000444f, 0.000444f);
        f32                mMieScaleHeightKm = 1.2f;
        f32                mMieAnisotropy    = 0.8f;

        Math::FVector3f    mOzoneAbsorption     = Math::FVector3f(0.000650f, 0.001881f, 0.000085f);
        f32                mOzoneCenterHeightKm = 25.0f;
        f32                mOzoneThicknessKm    = 15.0f;

        Math::FVector3f    mGroundAlbedo     = Math::FVector3f(0.08f, 0.08f, 0.08f);
        Math::FVector3f    mSolarTint        = Math::FVector3f(1.0f, 0.98f, 0.94f);
        f32                mSolarIlluminance = 22.0f;
        f32                mSunAngularRadius = 0.004675f;

        f32                mPlanetRadiusKm     = 6360.0f;
        f32                mAtmosphereHeightKm = 80.0f;
        f32                mViewHeightKm       = 0.1f;
        f32                mExposure           = 1.0f;

        [[nodiscard]] auto operator==(const FPbrSkyParameters& rhs) const noexcept -> bool {
            return mRayleighScattering.X() == rhs.mRayleighScattering.X()
                && mRayleighScattering.Y() == rhs.mRayleighScattering.Y()
                && mRayleighScattering.Z() == rhs.mRayleighScattering.Z()
                && mRayleighScaleHeightKm == rhs.mRayleighScaleHeightKm
                && mMieScattering.X() == rhs.mMieScattering.X()
                && mMieScattering.Y() == rhs.mMieScattering.Y()
                && mMieScattering.Z() == rhs.mMieScattering.Z()
                && mMieAbsorption.X() == rhs.mMieAbsorption.X()
                && mMieAbsorption.Y() == rhs.mMieAbsorption.Y()
                && mMieAbsorption.Z() == rhs.mMieAbsorption.Z()
                && mMieScaleHeightKm == rhs.mMieScaleHeightKm
                && mMieAnisotropy == rhs.mMieAnisotropy
                && mOzoneAbsorption.X() == rhs.mOzoneAbsorption.X()
                && mOzoneAbsorption.Y() == rhs.mOzoneAbsorption.Y()
                && mOzoneAbsorption.Z() == rhs.mOzoneAbsorption.Z()
                && mOzoneCenterHeightKm == rhs.mOzoneCenterHeightKm
                && mOzoneThicknessKm == rhs.mOzoneThicknessKm
                && mGroundAlbedo.X() == rhs.mGroundAlbedo.X()
                && mGroundAlbedo.Y() == rhs.mGroundAlbedo.Y()
                && mGroundAlbedo.Z() == rhs.mGroundAlbedo.Z()
                && mSolarTint.X() == rhs.mSolarTint.X() && mSolarTint.Y() == rhs.mSolarTint.Y()
                && mSolarTint.Z() == rhs.mSolarTint.Z()
                && mSolarIlluminance == rhs.mSolarIlluminance
                && mSunAngularRadius == rhs.mSunAngularRadius
                && mPlanetRadiusKm == rhs.mPlanetRadiusKm
                && mAtmosphereHeightKm == rhs.mAtmosphereHeightKm
                && mViewHeightKm == rhs.mViewHeightKm && mExposure == rhs.mExposure;
        }
    };

    class ACLASS() AE_ENGINE_API FPbrSkyComponent : public FComponent {
    public:
        void               MarkDirty() noexcept;
        void               SetVersion(u64 version) noexcept;
        [[nodiscard]] auto GetVersion() const noexcept -> u64 { return mVersion; }
        [[nodiscard]] auto GetParameters() const noexcept -> const FPbrSkyParameters& {
            return mParameters;
        }

    private:
        template <auto Member>
        friend struct AltinaEngine::Core::Reflection::Detail::TAutoMemberAccessor;

    public:
        APROPERTY()
        FPbrSkyParameters mParameters{};

    private:
        u64 mVersion = 1ULL;
    };
} // namespace AltinaEngine::GameScene
