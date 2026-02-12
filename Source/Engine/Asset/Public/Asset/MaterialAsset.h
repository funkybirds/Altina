#pragma once

#include "Asset/AssetBinary.h"
#include "Asset/AssetLoader.h"
#include "Container/Vector.h"

namespace AltinaEngine::Asset {
    using Core::Container::TVector;

    struct AE_ASSET_API FMaterialRuntimeDesc {
        u32 ShadingModel = 0;
        u32 BlendMode    = 0;
        u32 Flags        = 0;
        f32 AlphaCutoff  = 0.0f;
    };

    class AE_ASSET_API FMaterialAsset final : public IAsset {
    public:
        FMaterialAsset(FMaterialRuntimeDesc desc, TVector<FMaterialScalarParam> scalars,
            TVector<FMaterialVectorParam> vectors, TVector<FMaterialTextureParam> textures);

        [[nodiscard]] auto GetDesc() const noexcept -> const FMaterialRuntimeDesc& { return mDesc; }
        [[nodiscard]] auto GetScalars() const noexcept -> const TVector<FMaterialScalarParam>& {
            return mScalars;
        }
        [[nodiscard]] auto GetVectors() const noexcept -> const TVector<FMaterialVectorParam>& {
            return mVectors;
        }
        [[nodiscard]] auto GetTextures() const noexcept -> const TVector<FMaterialTextureParam>& {
            return mTextures;
        }

    private:
        FMaterialRuntimeDesc           mDesc{};
        TVector<FMaterialScalarParam>  mScalars;
        TVector<FMaterialVectorParam>  mVectors;
        TVector<FMaterialTextureParam> mTextures;
    };

} // namespace AltinaEngine::Asset
