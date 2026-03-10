#pragma once

#include "RenderCoreAPI.h"

#include "Material/MaterialPass.h"

#include "Container/Vector.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"
#include "Rhi/RhiResourceView.h"
#include "Rhi/RhiSampler.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Core::Reflection {
    class ISerializer;
    class IDeserializer;
} // namespace AltinaEngine::Core::Reflection

namespace AltinaEngine::RenderCore {
    namespace Container = Core::Container;
    namespace Math      = Core::Math;
    using Container::TVector;

    struct FMaterialScalarParam {
        FMaterialParamId mNameHash = 0U;
        f32              mValue    = 0.0f;
    };

    struct FMaterialVectorParam {
        FMaterialParamId mNameHash = 0U;
        Math::FVector4f  mValue    = Math::FVector4f(0.0f);
    };

    struct FMaterialMatrixParam {
        FMaterialParamId  mNameHash = 0U;
        Math::FMatrix4x4f mValue    = Math::FMatrix4x4f(0.0f);
    };

    struct FMaterialTextureParam {
        FMaterialParamId               mNameHash = 0U;
        Rhi::FRhiShaderResourceViewRef mSrv;
        Rhi::FRhiSamplerRef            mSampler;
        u32                            mSamplerFlags = 0U;
    };

    enum class EMaterialParamType : u8 {
        Scalar = 0,
        Vector,
        Matrix,
        Texture
    };

    struct FMaterialParamDesc {
        FMaterialParamId   mNameHash = 0U;
        EMaterialParamType mType     = EMaterialParamType::Scalar;
    };

    class AE_RENDER_CORE_API FMaterialSchema final {
    public:
        void               AddScalar(FMaterialParamId id);
        void               AddVector(FMaterialParamId id);
        void               AddMatrix(FMaterialParamId id);
        void               AddTexture(FMaterialParamId id);
        void               Clear() noexcept;

        [[nodiscard]] auto Find(FMaterialParamId id) const noexcept -> const FMaterialParamDesc*;
        [[nodiscard]] auto GetParams() const noexcept -> const TVector<FMaterialParamDesc>& {
            return mParams;
        }

    private:
        TVector<FMaterialParamDesc> mParams;
    };

    class AE_RENDER_CORE_API FMaterialParameterBlock final {
    public:
        void               Clear() noexcept;

        auto               SetScalar(FMaterialParamId id, f32 value) -> bool;
        auto               SetVector(FMaterialParamId id, const Math::FVector4f& value) -> bool;
        auto               SetMatrix(FMaterialParamId id, const Math::FMatrix4x4f& value) -> bool;
        auto               SetTexture(FMaterialParamId id, Rhi::FRhiShaderResourceViewRef srv,
                          Rhi::FRhiSamplerRef sampler, u32 samplerFlags) -> bool;

        [[nodiscard]] auto FindScalarParam(FMaterialParamId id) const noexcept
            -> const FMaterialScalarParam*;
        [[nodiscard]] auto FindVectorParam(FMaterialParamId id) const noexcept
            -> const FMaterialVectorParam*;
        [[nodiscard]] auto FindMatrixParam(FMaterialParamId id) const noexcept
            -> const FMaterialMatrixParam*;
        [[nodiscard]] auto FindTextureParam(FMaterialParamId id) const noexcept
            -> const FMaterialTextureParam*;

        [[nodiscard]] auto GetScalars() const noexcept -> const TVector<FMaterialScalarParam>& {
            return mScalars;
        }
        [[nodiscard]] auto GetVectors() const noexcept -> const TVector<FMaterialVectorParam>& {
            return mVectors;
        }
        [[nodiscard]] auto GetMatrices() const noexcept -> const TVector<FMaterialMatrixParam>& {
            return mMatrices;
        }
        [[nodiscard]] auto GetTextures() const noexcept -> const TVector<FMaterialTextureParam>& {
            return mTextures;
        }

        void        Serialize(Core::Reflection::ISerializer& serializer) const;
        static auto Deserialize(Core::Reflection::IDeserializer& deserializer)
            -> FMaterialParameterBlock;

    private:
        TVector<FMaterialScalarParam>  mScalars;
        TVector<FMaterialVectorParam>  mVectors;
        TVector<FMaterialMatrixParam>  mMatrices;
        TVector<FMaterialTextureParam> mTextures;
    };

} // namespace AltinaEngine::RenderCore
