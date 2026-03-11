#pragma once

#include "Asset/AssetLoader.h"
#include "Asset/AssetTypes.h"
#include "Asset/MeshMaterialParameterBlock.h"
#include "Container/String.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::TVector;

    using Container::FString;

    enum class EMaterialRasterFillMode : u8 {
        Solid = 0,
        Wireframe
    };

    enum class EMaterialRasterCullMode : u8 {
        None = 0,
        Front,
        Back
    };

    enum class EMaterialRasterFrontFace : u8 {
        CCW = 0,
        CW
    };

    // Optional per-pass overrides for rasterizer-related pipeline state.
    // Defaults match RHI defaults; only fields with Has* = true are applied.
    struct AE_ASSET_API FMaterialRasterStateOverrides {
        bool                     mHasFillMode             = false;
        bool                     mHasCullMode             = false;
        bool                     mHasFrontFace            = false;
        bool                     mHasDepthBias            = false;
        bool                     mHasDepthBiasClamp       = false;
        bool                     mHasSlopeScaledDepthBias = false;
        bool                     mHasDepthClip            = false;
        bool                     mHasConservativeRaster   = false;

        EMaterialRasterFillMode  mFillMode             = EMaterialRasterFillMode::Solid;
        EMaterialRasterCullMode  mCullMode             = EMaterialRasterCullMode::Back;
        EMaterialRasterFrontFace mFrontFace            = EMaterialRasterFrontFace::CCW;
        i32                      mDepthBias            = 0;
        f32                      mDepthBiasClamp       = 0.0f;
        f32                      mSlopeScaledDepthBias = 0.0f;
        bool                     mDepthClip            = true;
        bool                     mConservativeRaster   = false;

        [[nodiscard]] auto       HasAny() const noexcept -> bool {
            return mHasFillMode || mHasCullMode || mHasFrontFace || mHasDepthBias
                || mHasDepthBiasClamp || mHasSlopeScaledDepthBias || mHasDepthClip
                || mHasConservativeRaster;
        }
    };

    struct AE_ASSET_API FMaterialShaderSource {
        FAssetHandle mAsset{};
        FString      mEntry;
    };

    struct AE_ASSET_API FMaterialPassTemplate {
        FString                       mName;
        FString                       mPreset;
        bool                          mHasVertex  = false;
        bool                          mHasPixel   = false;
        bool                          mHasCompute = false;
        FMaterialShaderSource         mVertex;
        FMaterialShaderSource         mPixel;
        FMaterialShaderSource         mCompute;
        FMeshMaterialParameterBlock   mOverrides;
        FMaterialRasterStateOverrides mRasterOverrides;
    };

    class AE_ASSET_API FMaterialAsset final : public IAsset {
    public:
        FMaterialAsset(FString name, TVector<FMaterialPassTemplate> passes,
            TVector<TVector<FString>> precompileVariants);

        [[nodiscard]] auto GetName() const noexcept -> const FString& { return mName; }
        [[nodiscard]] auto GetPasses() const noexcept -> const TVector<FMaterialPassTemplate>& {
            return mPasses;
        }
        [[nodiscard]] auto GetPrecompileVariants() const noexcept
            -> const TVector<TVector<FString>>& {
            return mPrecompileVariants;
        }

    private:
        FString                        mName;
        TVector<FMaterialPassTemplate> mPasses;
        TVector<TVector<FString>>      mPrecompileVariants;
    };

} // namespace AltinaEngine::Asset
