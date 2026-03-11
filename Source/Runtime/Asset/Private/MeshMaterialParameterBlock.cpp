#include "Asset/MeshMaterialParameterBlock.h"

#include "Reflection/Serializer.h"
#include "Types/Traits.h"

using AltinaEngine::Move;

namespace AltinaEngine::Asset {
    namespace {
        constexpr u32 kParamBlockVersion = 1U;

        constexpr u64 kFnvOffset64 = 1469598103934665603ULL;
        constexpr u64 kFnvPrime64  = 1099511628211ULL;

        auto          HashBytes(u64 seed, const void* data, usize size) -> u64 {
            if (data == nullptr || size == 0U) {
                return seed;
            }
            auto  hash = seed;
            auto* ptr  = static_cast<const u8*>(data);
            for (usize i = 0U; i < size; ++i) {
                hash ^= ptr[i];
                hash *= kFnvPrime64;
            }
            return hash;
        }

        template <typename T> auto HashValue(u64 seed, const T& value) -> u64 {
            return HashBytes(seed, &value, sizeof(T));
        }
    } // namespace

    void FMeshMaterialParameterBlock::Clear() noexcept {
        mScalars.Clear();
        mVectors.Clear();
        mMatrices.Clear();
        mTextures.Clear();
    }

    auto FMeshMaterialParameterBlock::SetScalar(FMaterialParamId id, f32 value) -> bool {
        if (id == 0U) {
            return false;
        }

        bool changed = false;
        for (auto& param : mScalars) {
            if (param.mNameHash == id) {
                if (param.mValue != value) {
                    param.mValue = value;
                    changed      = true;
                }
                return changed;
            }
        }

        mScalars.PushBack({ id, value });
        return true;
    }

    auto FMeshMaterialParameterBlock::SetVector(FMaterialParamId id, const Math::FVector4f& value)
        -> bool {
        if (id == 0U) {
            return false;
        }

        bool changed = false;
        for (auto& param : mVectors) {
            if (param.mNameHash == id) {
                bool differs = false;
                for (u32 i = 0U; i < 4U; ++i) {
                    if (param.mValue.mComponents[i] != value.mComponents[i]) {
                        differs = true;
                        break;
                    }
                }
                if (differs) {
                    param.mValue = value;
                    changed      = true;
                }
                return changed;
            }
        }

        mVectors.PushBack({ id, value });
        return true;
    }

    auto FMeshMaterialParameterBlock::SetMatrix(FMaterialParamId id, const Math::FMatrix4x4f& value)
        -> bool {
        if (id == 0U) {
            return false;
        }

        bool changed = false;
        for (auto& param : mMatrices) {
            if (param.mNameHash == id) {
                bool differs = false;
                for (u32 r = 0U; r < 4U && !differs; ++r) {
                    for (u32 c = 0U; c < 4U; ++c) {
                        if (param.mValue.mElements[r][c] != value.mElements[r][c]) {
                            differs = true;
                            break;
                        }
                    }
                }
                if (differs) {
                    param.mValue = value;
                    changed      = true;
                }
                return changed;
            }
        }

        mMatrices.PushBack({ id, value });
        return true;
    }

    auto FMeshMaterialParameterBlock::SetTexture(FMaterialParamId id, EMeshMaterialTextureType type,
        FAssetHandle texture, u32 samplerFlags) -> bool {
        if (id == 0U) {
            return false;
        }

        bool changed = false;
        for (auto& param : mTextures) {
            if (param.mNameHash == id) {
                if (param.mTexture != texture || param.mSamplerFlags != samplerFlags
                    || param.mType != type) {
                    param.mTexture      = Move(texture);
                    param.mSamplerFlags = samplerFlags;
                    param.mType         = type;
                    changed             = true;
                }
                return changed;
            }
        }

        FMeshMaterialTextureParam param{};
        param.mNameHash     = id;
        param.mType         = type;
        param.mTexture      = Move(texture);
        param.mSamplerFlags = samplerFlags;
        mTextures.PushBack(Move(param));
        return true;
    }

    auto FMeshMaterialParameterBlock::FindScalarParam(FMaterialParamId id) const noexcept
        -> const FMeshMaterialScalarParam* {
        for (const auto& param : mScalars) {
            if (param.mNameHash == id) {
                return &param;
            }
        }
        return nullptr;
    }

    auto FMeshMaterialParameterBlock::FindVectorParam(FMaterialParamId id) const noexcept
        -> const FMeshMaterialVectorParam* {
        for (const auto& param : mVectors) {
            if (param.mNameHash == id) {
                return &param;
            }
        }
        return nullptr;
    }

    auto FMeshMaterialParameterBlock::FindMatrixParam(FMaterialParamId id) const noexcept
        -> const FMeshMaterialMatrixParam* {
        for (const auto& param : mMatrices) {
            if (param.mNameHash == id) {
                return &param;
            }
        }
        return nullptr;
    }

    auto FMeshMaterialParameterBlock::FindTextureParam(FMaterialParamId id) const noexcept
        -> const FMeshMaterialTextureParam* {
        for (const auto& param : mTextures) {
            if (param.mNameHash == id) {
                return &param;
            }
        }
        return nullptr;
    }

    auto FMeshMaterialParameterBlock::GetHash() const noexcept -> u64 {
        u64 hash = kFnvOffset64;

        for (const auto& param : mScalars) {
            hash = HashValue(hash, param.mNameHash);
            hash = HashValue(hash, param.mValue);
        }

        for (const auto& param : mVectors) {
            hash = HashValue(hash, param.mNameHash);
            for (u32 i = 0U; i < 4U; ++i) {
                hash = HashValue(hash, param.mValue.mComponents[i]);
            }
        }

        for (const auto& param : mMatrices) {
            hash = HashValue(hash, param.mNameHash);
            for (u32 r = 0U; r < 4U; ++r) {
                for (u32 c = 0U; c < 4U; ++c) {
                    hash = HashValue(hash, param.mValue.mElements[r][c]);
                }
            }
        }

        for (const auto& param : mTextures) {
            hash = HashValue(hash, param.mNameHash);
            hash = HashValue(hash, static_cast<u8>(param.mType));
            hash = HashBytes(hash, param.mTexture.mUuid.Data(), FUuid::kByteCount);
            hash = HashValue(hash, static_cast<u8>(param.mTexture.mType));
            hash = HashValue(hash, param.mSamplerFlags);
        }

        return hash;
    }

    void FMeshMaterialParameterBlock::Serialize(Core::Reflection::ISerializer& serializer) const {
        serializer.Write(kParamBlockVersion);

        serializer.Write(static_cast<u32>(mScalars.Size()));
        for (const auto& param : mScalars) {
            serializer.Write(param.mNameHash);
            serializer.Write(param.mValue);
        }

        serializer.Write(static_cast<u32>(mVectors.Size()));
        for (const auto& param : mVectors) {
            serializer.Write(param.mNameHash);
            for (u32 i = 0U; i < 4U; ++i) {
                serializer.Write(param.mValue.mComponents[i]);
            }
        }

        serializer.Write(static_cast<u32>(mMatrices.Size()));
        for (const auto& param : mMatrices) {
            serializer.Write(param.mNameHash);
            for (u32 r = 0U; r < 4U; ++r) {
                for (u32 c = 0U; c < 4U; ++c) {
                    serializer.Write(param.mValue.mElements[r][c]);
                }
            }
        }

        serializer.Write(static_cast<u32>(mTextures.Size()));
        for (const auto& param : mTextures) {
            serializer.Write(param.mNameHash);
            serializer.Write(static_cast<u8>(param.mType));
            for (usize i = 0U; i < FUuid::kByteCount; ++i) {
                serializer.Write(param.mTexture.mUuid.GetBytes()[i]);
            }
            serializer.Write(static_cast<u8>(param.mTexture.mType));
            serializer.Write(param.mSamplerFlags);
        }
    }

    auto FMeshMaterialParameterBlock::Deserialize(Core::Reflection::IDeserializer& deserializer)
        -> FMeshMaterialParameterBlock {
        FMeshMaterialParameterBlock result;

        const u32                   version = deserializer.Read<u32>();
        if (version != kParamBlockVersion) {
            return result;
        }

        const u32 scalarCount = deserializer.Read<u32>();
        result.mScalars.Reserve(scalarCount);
        for (u32 i = 0U; i < scalarCount; ++i) {
            FMeshMaterialScalarParam param{};
            param.mNameHash = deserializer.Read<u32>();
            param.mValue    = deserializer.Read<f32>();
            result.mScalars.PushBack(param);
        }

        const u32 vectorCount = deserializer.Read<u32>();
        result.mVectors.Reserve(vectorCount);
        for (u32 i = 0U; i < vectorCount; ++i) {
            FMeshMaterialVectorParam param{};
            param.mNameHash = deserializer.Read<u32>();
            for (u32 c = 0U; c < 4U; ++c) {
                param.mValue.mComponents[c] = deserializer.Read<f32>();
            }
            result.mVectors.PushBack(param);
        }

        const u32 matrixCount = deserializer.Read<u32>();
        result.mMatrices.Reserve(matrixCount);
        for (u32 i = 0U; i < matrixCount; ++i) {
            FMeshMaterialMatrixParam param{};
            param.mNameHash = deserializer.Read<u32>();
            for (u32 r = 0U; r < 4U; ++r) {
                for (u32 c = 0U; c < 4U; ++c) {
                    param.mValue.mElements[r][c] = deserializer.Read<f32>();
                }
            }
            result.mMatrices.PushBack(param);
        }

        const u32 textureCount = deserializer.Read<u32>();
        result.mTextures.Reserve(textureCount);
        for (u32 i = 0U; i < textureCount; ++i) {
            FMeshMaterialTextureParam param{};
            param.mNameHash = deserializer.Read<u32>();
            param.mType     = static_cast<EMeshMaterialTextureType>(deserializer.Read<u8>());

            FUuid::FBytes bytes{};
            for (usize b = 0U; b < FUuid::kByteCount; ++b) {
                bytes[b] = deserializer.Read<u8>();
            }
            param.mTexture.mUuid = FUuid(bytes);
            param.mTexture.mType = static_cast<EAssetType>(deserializer.Read<u8>());
            param.mSamplerFlags  = deserializer.Read<u32>();
            result.mTextures.PushBack(param);
        }

        return result;
    }

} // namespace AltinaEngine::Asset
