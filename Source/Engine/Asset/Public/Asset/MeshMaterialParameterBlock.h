#pragma once

#include "AssetAPI.h"
#include "Asset/AssetTypes.h"
#include "Container/Vector.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Core::Reflection {
    class ISerializer;
    class IDeserializer;
} // namespace AltinaEngine::Core::Reflection

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    namespace Math      = Core::Math;
    using Container::TVector;

    using FMaterialParamId = u32;

    enum class EMeshMaterialTextureType : u8 {
        Texture2D = 0
    };

    struct AE_ASSET_API FMeshMaterialScalarParam {
        FMaterialParamId NameHash = 0U;
        f32              Value    = 0.0f;
    };

    struct AE_ASSET_API FMeshMaterialVectorParam {
        FMaterialParamId NameHash = 0U;
        Math::FVector4f  Value    = Math::FVector4f(0.0f);
    };

    struct AE_ASSET_API FMeshMaterialMatrixParam {
        FMaterialParamId  NameHash = 0U;
        Math::FMatrix4x4f Value    = Math::FMatrix4x4f(0.0f);
    };

    struct AE_ASSET_API FMeshMaterialTextureParam {
        FMaterialParamId         NameHash = 0U;
        EMeshMaterialTextureType Type     = EMeshMaterialTextureType::Texture2D;
        FAssetHandle             Texture;
        u32                      SamplerFlags = 0U;
    };

    class AE_ASSET_API FMeshMaterialParameterBlock final {
    public:
        void Clear() noexcept;

        auto SetScalar(FMaterialParamId id, f32 value) -> bool;
        auto SetVector(FMaterialParamId id, const Math::FVector4f& value) -> bool;
        auto SetMatrix(FMaterialParamId id, const Math::FMatrix4x4f& value) -> bool;
        auto SetTexture(FMaterialParamId id, EMeshMaterialTextureType type, FAssetHandle texture,
            u32 samplerFlags) -> bool;

        [[nodiscard]] auto FindScalarParam(FMaterialParamId id) const noexcept
            -> const FMeshMaterialScalarParam*;
        [[nodiscard]] auto FindVectorParam(FMaterialParamId id) const noexcept
            -> const FMeshMaterialVectorParam*;
        [[nodiscard]] auto FindMatrixParam(FMaterialParamId id) const noexcept
            -> const FMeshMaterialMatrixParam*;
        [[nodiscard]] auto FindTextureParam(FMaterialParamId id) const noexcept
            -> const FMeshMaterialTextureParam*;

        [[nodiscard]] auto GetScalars() const noexcept -> const TVector<FMeshMaterialScalarParam>& {
            return mScalars;
        }
        [[nodiscard]] auto GetVectors() const noexcept -> const TVector<FMeshMaterialVectorParam>& {
            return mVectors;
        }
        [[nodiscard]] auto GetMatrices() const noexcept
            -> const TVector<FMeshMaterialMatrixParam>& {
            return mMatrices;
        }
        [[nodiscard]] auto GetTextures() const noexcept
            -> const TVector<FMeshMaterialTextureParam>& {
            return mTextures;
        }

        [[nodiscard]] auto GetHash() const noexcept -> u64;

        void               Serialize(Core::Reflection::ISerializer& serializer) const;
        static auto        Deserialize(Core::Reflection::IDeserializer& deserializer)
            -> FMeshMaterialParameterBlock;

    private:
        TVector<FMeshMaterialScalarParam>  mScalars;
        TVector<FMeshMaterialVectorParam>  mVectors;
        TVector<FMeshMaterialMatrixParam>  mMatrices;
        TVector<FMeshMaterialTextureParam> mTextures;
    };

} // namespace AltinaEngine::Asset
