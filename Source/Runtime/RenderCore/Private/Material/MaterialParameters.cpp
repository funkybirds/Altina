#include "Material/MaterialParameters.h"

#include "Reflection/Serializer.h"
#include "Types/Traits.h"

using AltinaEngine::Move;

namespace AltinaEngine::RenderCore {
    namespace {
        constexpr u32 kParamBlockVersion = 1U;
    } // namespace

    void FMaterialSchema::AddScalar(FMaterialParamId id) {
        mParams.PushBack({ id, EMaterialParamType::Scalar });
    }

    void FMaterialSchema::AddVector(FMaterialParamId id) {
        mParams.PushBack({ id, EMaterialParamType::Vector });
    }

    void FMaterialSchema::AddMatrix(FMaterialParamId id) {
        mParams.PushBack({ id, EMaterialParamType::Matrix });
    }

    void FMaterialSchema::AddTexture(FMaterialParamId id) {
        mParams.PushBack({ id, EMaterialParamType::Texture });
    }

    void FMaterialSchema::Clear() noexcept { mParams.Clear(); }

    auto FMaterialSchema::Find(FMaterialParamId id) const noexcept -> const FMaterialParamDesc* {
        for (const auto& entry : mParams) {
            if (entry.NameHash == id) {
                return &entry;
            }
        }
        return nullptr;
    }

    void FMaterialParameterBlock::Clear() noexcept {
        mScalars.Clear();
        mVectors.Clear();
        mMatrices.Clear();
        mTextures.Clear();
    }

    auto FMaterialParameterBlock::SetScalar(FMaterialParamId id, f32 value) -> bool {
        if (id == 0U) {
            return false;
        }

        bool changed = false;
        for (auto& param : mScalars) {
            if (param.NameHash == id) {
                if (param.Value != value) {
                    param.Value = value;
                    changed     = true;
                }
                return changed;
            }
        }

        mScalars.PushBack({ id, value });
        return true;
    }

    auto FMaterialParameterBlock::SetVector(FMaterialParamId id, const Math::FVector4f& value)
        -> bool {
        if (id == 0U) {
            return false;
        }

        bool changed = false;
        for (auto& param : mVectors) {
            if (param.NameHash == id) {
                bool differs = false;
                for (u32 i = 0U; i < 4U; ++i) {
                    if (param.Value.mComponents[i] != value.mComponents[i]) {
                        differs = true;
                        break;
                    }
                }
                if (differs) {
                    param.Value = value;
                    changed     = true;
                }
                return changed;
            }
        }

        mVectors.PushBack({ id, value });
        return true;
    }

    auto FMaterialParameterBlock::SetMatrix(FMaterialParamId id, const Math::FMatrix4x4f& value)
        -> bool {
        if (id == 0U) {
            return false;
        }

        bool changed = false;
        for (auto& param : mMatrices) {
            if (param.NameHash == id) {
                bool differs = false;
                for (u32 r = 0U; r < 4U && !differs; ++r) {
                    for (u32 c = 0U; c < 4U; ++c) {
                        if (param.Value.mElements[r][c] != value.mElements[r][c]) {
                            differs = true;
                            break;
                        }
                    }
                }
                if (differs) {
                    param.Value = value;
                    changed     = true;
                }
                return changed;
            }
        }

        mMatrices.PushBack({ id, value });
        return true;
    }

    auto FMaterialParameterBlock::SetTexture(FMaterialParamId id,
        Rhi::FRhiShaderResourceViewRef srv, Rhi::FRhiSamplerRef sampler, u32 samplerFlags) -> bool {
        if (id == 0U) {
            return false;
        }

        bool changed = false;
        for (auto& param : mTextures) {
            if (param.NameHash == id) {
                const bool sameSrv     = (param.SRV.Get() == srv.Get());
                const bool sameSampler = (param.Sampler.Get() == sampler.Get());
                if (!sameSrv || !sameSampler || param.SamplerFlags != samplerFlags) {
                    param.SRV          = Move(srv);
                    param.Sampler      = Move(sampler);
                    param.SamplerFlags = samplerFlags;
                    changed            = true;
                }
                return changed;
            }
        }

        FMaterialTextureParam param{};
        param.NameHash     = id;
        param.SRV          = Move(srv);
        param.Sampler      = Move(sampler);
        param.SamplerFlags = samplerFlags;
        mTextures.PushBack(Move(param));
        return true;
    }

    auto FMaterialParameterBlock::FindScalarParam(FMaterialParamId id) const noexcept
        -> const FMaterialScalarParam* {
        for (const auto& param : mScalars) {
            if (param.NameHash == id) {
                return &param;
            }
        }
        return nullptr;
    }

    auto FMaterialParameterBlock::FindVectorParam(FMaterialParamId id) const noexcept
        -> const FMaterialVectorParam* {
        for (const auto& param : mVectors) {
            if (param.NameHash == id) {
                return &param;
            }
        }
        return nullptr;
    }

    auto FMaterialParameterBlock::FindMatrixParam(FMaterialParamId id) const noexcept
        -> const FMaterialMatrixParam* {
        for (const auto& param : mMatrices) {
            if (param.NameHash == id) {
                return &param;
            }
        }
        return nullptr;
    }

    auto FMaterialParameterBlock::FindTextureParam(FMaterialParamId id) const noexcept
        -> const FMaterialTextureParam* {
        for (const auto& param : mTextures) {
            if (param.NameHash == id) {
                return &param;
            }
        }
        return nullptr;
    }

    void FMaterialParameterBlock::Serialize(Core::Reflection::ISerializer& serializer) const {
        serializer.Write(kParamBlockVersion);

        serializer.Write(static_cast<u32>(mScalars.Size()));
        for (const auto& param : mScalars) {
            serializer.Write(param.NameHash);
            serializer.Write(param.Value);
        }

        serializer.Write(static_cast<u32>(mVectors.Size()));
        for (const auto& param : mVectors) {
            serializer.Write(param.NameHash);
            for (u32 i = 0U; i < 4U; ++i) {
                serializer.Write(param.Value.mComponents[i]);
            }
        }

        serializer.Write(static_cast<u32>(mMatrices.Size()));
        for (const auto& param : mMatrices) {
            serializer.Write(param.NameHash);
            for (u32 r = 0U; r < 4U; ++r) {
                for (u32 c = 0U; c < 4U; ++c) {
                    serializer.Write(param.Value.mElements[r][c]);
                }
            }
        }
    }

    auto FMaterialParameterBlock::Deserialize(Core::Reflection::IDeserializer& deserializer)
        -> FMaterialParameterBlock {
        FMaterialParameterBlock result;

        const u32               version = deserializer.Read<u32>();
        if (version != kParamBlockVersion) {
            return result;
        }

        const u32 scalarCount = deserializer.Read<u32>();
        result.mScalars.Reserve(scalarCount);
        for (u32 i = 0U; i < scalarCount; ++i) {
            FMaterialScalarParam param{};
            param.NameHash = deserializer.Read<u32>();
            param.Value    = deserializer.Read<f32>();
            result.mScalars.PushBack(param);
        }

        const u32 vectorCount = deserializer.Read<u32>();
        result.mVectors.Reserve(vectorCount);
        for (u32 i = 0U; i < vectorCount; ++i) {
            FMaterialVectorParam param{};
            param.NameHash = deserializer.Read<u32>();
            for (u32 c = 0U; c < 4U; ++c) {
                param.Value.mComponents[c] = deserializer.Read<f32>();
            }
            result.mVectors.PushBack(param);
        }

        const u32 matrixCount = deserializer.Read<u32>();
        result.mMatrices.Reserve(matrixCount);
        for (u32 i = 0U; i < matrixCount; ++i) {
            FMaterialMatrixParam param{};
            param.NameHash = deserializer.Read<u32>();
            for (u32 r = 0U; r < 4U; ++r) {
                for (u32 c = 0U; c < 4U; ++c) {
                    param.Value.mElements[r][c] = deserializer.Read<f32>();
                }
            }
            result.mMatrices.PushBack(param);
        }

        return result;
    }

} // namespace AltinaEngine::RenderCore
