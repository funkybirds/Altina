#pragma once

#include "Rendering/RenderingAPI.h"

#include "Math/Vector.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiResourceView.h"
#include "Container/SmartPtr.h"

namespace AltinaEngine::Rendering::Atmosphere {
    namespace Math = Core::Math;

    struct AE_RENDERING_API FAtmosphereSkyDesc {
        Math::FVector3f mRayleighScattering    = Math::FVector3f(0.0f);
        f32             mRayleighScaleHeightKm = 0.0f;

        Math::FVector3f mMieScattering    = Math::FVector3f(0.0f);
        Math::FVector3f mMieAbsorption    = Math::FVector3f(0.0f);
        f32             mMieScaleHeightKm = 0.0f;
        f32             mMieAnisotropy    = 0.0f;

        Math::FVector3f mOzoneAbsorption     = Math::FVector3f(0.0f);
        f32             mOzoneCenterHeightKm = 0.0f;
        f32             mOzoneThicknessKm    = 0.0f;

        Math::FVector3f mGroundAlbedo     = Math::FVector3f(0.0f);
        Math::FVector3f mSolarTint        = Math::FVector3f(0.0f);
        f32             mSolarIlluminance = 0.0f;
        f32             mSunAngularRadius = 0.0f;

        f32             mPlanetRadiusKm     = 0.0f;
        f32             mAtmosphereHeightKm = 0.0f;
        f32             mViewHeightKm       = 0.0f;
        f32             mExposure           = 0.0f;
        u64             mVersion            = 0ULL;
    };

    struct AE_RENDERING_API FAtmosphereSkyResources {
        Rhi::FRhiBufferRef             mParamsBuffer;
        Rhi::FRhiTextureRef            mTransmittanceLut;
        Rhi::FRhiShaderResourceViewRef mTransmittanceLutSrv;
        Rhi::FRhiTextureRef            mIrradianceLut;
        Rhi::FRhiShaderResourceViewRef mIrradianceLutSrv;
        Rhi::FRhiTextureRef            mScatteringLut;
        Rhi::FRhiShaderResourceViewRef mScatteringLutSrv;
        Rhi::FRhiTextureRef            mSingleMieScatteringLut;
        Rhi::FRhiShaderResourceViewRef mSingleMieScatteringLutSrv;

        [[nodiscard]] auto             IsValid() const noexcept -> bool {
            return mParamsBuffer && mTransmittanceLut && mIrradianceLut && mScatteringLut
                && mSingleMieScatteringLut;
        }
    };

    class AE_RENDERING_API FAtmosphereSystem final {
    public:
        static auto        Get() noexcept -> FAtmosphereSystem&;

        [[nodiscard]] auto EnsureSkyResources(const FAtmosphereSkyDesc& desc,
            const Math::FVector3f& sunDirection) -> const FAtmosphereSkyResources*;

        [[nodiscard]] auto GetResources() const noexcept -> const FAtmosphereSkyResources*;

        void               Reset() noexcept;

    private:
        struct FGpuState;

        FAtmosphereSystem();

        FAtmosphereSkyResources            mResources{};
        FAtmosphereSkyDesc                 mCachedDesc{};
        Math::FVector3f                    mCachedSunDirection = Math::FVector3f(0.0f, 1.0f, 0.0f);
        Core::Container::TOwner<FGpuState> mGpuState;
        bool                               mHasCache = false;
    };
} // namespace AltinaEngine::Rendering::Atmosphere
