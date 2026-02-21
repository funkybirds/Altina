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

    auto FMeshMaterialParameterBlock::SetVector(FMaterialParamId id, const Math::FVector4f& value)
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

    auto FMeshMaterialParameterBlock::SetMatrix(FMaterialParamId id, const Math::FMatrix4x4f& value)
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

    auto FMeshMaterialParameterBlock::SetTexture(FMaterialParamId id, EMeshMaterialTextureType type,
        FAssetHandle texture, u32 samplerFlags) -> bool {
        if (id == 0U) {
            return false;
        }

        bool changed = false;
        for (auto& param : mTextures) {
            if (param.NameHash == id) {
                if (param.Texture != texture || param.SamplerFlags != samplerFlags
                    || param.Type != type) {
                    param.Texture      = Move(texture);
                    param.SamplerFlags = samplerFlags;
                    param.Type         = type;
                    changed            = true;
                }
                return changed;
            }
        }

        FMeshMaterialTextureParam param{};
        param.NameHash     = id;
        param.Type         = type;
        param.Texture      = Move(texture);
        param.SamplerFlags = samplerFlags;
        mTextures.PushBack(Move(param));
        return true;
    }

    auto FMeshMaterialParameterBlock::FindScalarParam(FMaterialParamId id) const noexcept
        -> const FMeshMaterialScalarParam* {
        for (const auto& param : mScalars) {
            if (param.NameHash == id) {
                return &param;
            }
        }
        return nullptr;
    }

    auto FMeshMaterialParameterBlock::FindVectorParam(FMaterialParamId id) const noexcept
        -> const FMeshMaterialVectorParam* {
        for (const auto& param : mVectors) {
            if (param.NameHash == id) {
                return &param;
            }
        }
        return nullptr;
    }

    auto FMeshMaterialParameterBlock::FindMatrixParam(FMaterialParamId id) const noexcept
        -> const FMeshMaterialMatrixParam* {
        for (const auto& param : mMatrices) {
            if (param.NameHash == id) {
                return &param;
            }
        }
        return nullptr;
    }

    auto FMeshMaterialParameterBlock::FindTextureParam(FMaterialParamId id) const noexcept
        -> const FMeshMaterialTextureParam* {
        for (const auto& param : mTextures) {
            if (param.NameHash == id) {
                return &param;
            }
        }
        return nullptr;
    }

    auto FMeshMaterialParameterBlock::GetHash() const noexcept -> u64 {
        u64 hash = kFnvOffset64;

        for (const auto& param : mScalars) {
            hash = HashValue(hash, param.NameHash);
            hash = HashValue(hash, param.Value);
        }

        for (const auto& param : mVectors) {
            hash = HashValue(hash, param.NameHash);
            for (u32 i = 0U; i < 4U; ++i) {
                hash = HashValue(hash, param.Value.mComponents[i]);
            }
        }

        for (const auto& param : mMatrices) {
            hash = HashValue(hash, param.NameHash);
            for (u32 r = 0U; r < 4U; ++r) {
                for (u32 c = 0U; c < 4U; ++c) {
                    hash = HashValue(hash, param.Value.mElements[r][c]);
                }
            }
        }

        for (const auto& param : mTextures) {
            hash = HashValue(hash, param.NameHash);
            hash = HashValue(hash, static_cast<u8>(param.Type));
            hash = HashBytes(hash, param.Texture.Uuid.Data(), FUuid::kByteCount);
            hash = HashValue(hash, static_cast<u8>(param.Texture.Type));
            hash = HashValue(hash, param.SamplerFlags);
        }

        return hash;
    }

    void FMeshMaterialParameterBlock::Serialize(Core::Reflection::ISerializer& serializer) const {
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

        serializer.Write(static_cast<u32>(mTextures.Size()));
        for (const auto& param : mTextures) {
            serializer.Write(param.NameHash);
            serializer.Write(static_cast<u8>(param.Type));
            for (usize i = 0U; i < FUuid::kByteCount; ++i) {
                serializer.Write(param.Texture.Uuid.GetBytes()[i]);
            }
            serializer.Write(static_cast<u8>(param.Texture.Type));
            serializer.Write(param.SamplerFlags);
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
            param.NameHash = deserializer.Read<u32>();
            param.Value    = deserializer.Read<f32>();
            result.mScalars.PushBack(param);
        }

        const u32 vectorCount = deserializer.Read<u32>();
        result.mVectors.Reserve(vectorCount);
        for (u32 i = 0U; i < vectorCount; ++i) {
            FMeshMaterialVectorParam param{};
            param.NameHash = deserializer.Read<u32>();
            for (u32 c = 0U; c < 4U; ++c) {
                param.Value.mComponents[c] = deserializer.Read<f32>();
            }
            result.mVectors.PushBack(param);
        }

        const u32 matrixCount = deserializer.Read<u32>();
        result.mMatrices.Reserve(matrixCount);
        for (u32 i = 0U; i < matrixCount; ++i) {
            FMeshMaterialMatrixParam param{};
            param.NameHash = deserializer.Read<u32>();
            for (u32 r = 0U; r < 4U; ++r) {
                for (u32 c = 0U; c < 4U; ++c) {
                    param.Value.mElements[r][c] = deserializer.Read<f32>();
                }
            }
            result.mMatrices.PushBack(param);
        }

        const u32 textureCount = deserializer.Read<u32>();
        result.mTextures.Reserve(textureCount);
        for (u32 i = 0U; i < textureCount; ++i) {
            FMeshMaterialTextureParam param{};
            param.NameHash = deserializer.Read<u32>();
            param.Type     = static_cast<EMeshMaterialTextureType>(deserializer.Read<u8>());

            FUuid::FBytes bytes{};
            for (usize b = 0U; b < FUuid::kByteCount; ++b) {
                bytes[b] = deserializer.Read<u8>();
            }
            param.Texture.Uuid = FUuid(bytes);
            param.Texture.Type = static_cast<EAssetType>(deserializer.Read<u8>());
            param.SamplerFlags = deserializer.Read<u32>();
            result.mTextures.PushBack(param);
        }

        return result;
    }

} // namespace AltinaEngine::Asset
