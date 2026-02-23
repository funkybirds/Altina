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
        bool                     HasFillMode             = false;
        bool                     HasCullMode             = false;
        bool                     HasFrontFace            = false;
        bool                     HasDepthBias            = false;
        bool                     HasDepthBiasClamp       = false;
        bool                     HasSlopeScaledDepthBias = false;
        bool                     HasDepthClip            = false;
        bool                     HasConservativeRaster   = false;

        EMaterialRasterFillMode  FillMode             = EMaterialRasterFillMode::Solid;
        EMaterialRasterCullMode  CullMode             = EMaterialRasterCullMode::Back;
        EMaterialRasterFrontFace FrontFace            = EMaterialRasterFrontFace::CCW;
        i32                      DepthBias            = 0;
        f32                      DepthBiasClamp       = 0.0f;
        f32                      SlopeScaledDepthBias = 0.0f;
        bool                     DepthClip            = true;
        bool                     ConservativeRaster   = false;

        [[nodiscard]] auto       HasAny() const noexcept -> bool {
            return HasFillMode || HasCullMode || HasFrontFace || HasDepthBias || HasDepthBiasClamp
                || HasSlopeScaledDepthBias || HasDepthClip || HasConservativeRaster;
        }
    };

    struct AE_ASSET_API FMaterialShaderSource {
        FAssetHandle Asset{};
        FString      Entry;
    };

    struct AE_ASSET_API FMaterialPassTemplate {
        FString                       Name;
        FString                       Preset;
        bool                          HasVertex  = false;
        bool                          HasPixel   = false;
        bool                          HasCompute = false;
        FMaterialShaderSource         Vertex;
        FMaterialShaderSource         Pixel;
        FMaterialShaderSource         Compute;
        FMeshMaterialParameterBlock   Overrides;
        FMaterialRasterStateOverrides RasterOverrides;
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
