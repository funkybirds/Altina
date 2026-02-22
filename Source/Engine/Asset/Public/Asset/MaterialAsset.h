#pragma once

#include "Asset/AssetLoader.h"
#include "Asset/AssetTypes.h"
#include "Asset/MeshMaterialParameterBlock.h"
#include "Container/String.h"
#include "Container/Vector.h"

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::TVector;

    using Container::FString;

    struct AE_ASSET_API FMaterialShaderSource {
        FAssetHandle Asset{};
        FString      Entry;
    };

    struct AE_ASSET_API FMaterialPassTemplate {
        FString                     Name;
        bool                        HasVertex  = false;
        bool                        HasPixel   = false;
        bool                        HasCompute = false;
        FMaterialShaderSource       Vertex;
        FMaterialShaderSource       Pixel;
        FMaterialShaderSource       Compute;
        FMeshMaterialParameterBlock Overrides;
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
