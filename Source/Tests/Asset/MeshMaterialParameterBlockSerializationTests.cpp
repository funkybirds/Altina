#include "TestHarness.h"
#include "Base/AltinaBase.h"
#include "Asset/MeshMaterialParameterBlock.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/BinarySerializer.h"
#include "Reflection/JsonDeserializer.h"
#include "Reflection/JsonSerializer.h"
#include "Reflection/Serialization.h"
#include "Reflection/Traits.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Asset;
using namespace AltinaEngine::Core;
using namespace AltinaEngine::Core::Reflection;

namespace {
    auto MakeUuid(u8 seed) -> FUuid {
        FUuid::FBytes bytes{};
        for (usize i = 0U; i < FUuid::kByteCount; ++i) {
            bytes[i] = static_cast<u8>(seed + i);
        }
        return FUuid(bytes);
    }

    void RequireBlockEquals(
        const FMeshMaterialParameterBlock& lhs, const FMeshMaterialParameterBlock& rhs) {
        REQUIRE(lhs.GetScalars().Size() == rhs.GetScalars().Size());
        REQUIRE(lhs.GetVectors().Size() == rhs.GetVectors().Size());
        REQUIRE(lhs.GetMatrices().Size() == rhs.GetMatrices().Size());
        REQUIRE(lhs.GetTextures().Size() == rhs.GetTextures().Size());

        for (usize i = 0U; i < lhs.GetScalars().Size(); ++i) {
            REQUIRE(lhs.GetScalars()[i].NameHash == rhs.GetScalars()[i].NameHash);
            REQUIRE(lhs.GetScalars()[i].Value == rhs.GetScalars()[i].Value);
        }
        for (usize i = 0U; i < lhs.GetVectors().Size(); ++i) {
            REQUIRE(lhs.GetVectors()[i].NameHash == rhs.GetVectors()[i].NameHash);
            for (u32 c = 0U; c < 4U; ++c) {
                REQUIRE(lhs.GetVectors()[i].Value.mComponents[c]
                    == rhs.GetVectors()[i].Value.mComponents[c]);
            }
        }
        for (usize i = 0U; i < lhs.GetMatrices().Size(); ++i) {
            REQUIRE(lhs.GetMatrices()[i].NameHash == rhs.GetMatrices()[i].NameHash);
            for (u32 r = 0U; r < 4U; ++r) {
                for (u32 c = 0U; c < 4U; ++c) {
                    REQUIRE(lhs.GetMatrices()[i].Value.mElements[r][c]
                        == rhs.GetMatrices()[i].Value.mElements[r][c]);
                }
            }
        }
        for (usize i = 0U; i < lhs.GetTextures().Size(); ++i) {
            REQUIRE(lhs.GetTextures()[i].NameHash == rhs.GetTextures()[i].NameHash);
            REQUIRE(lhs.GetTextures()[i].Type == rhs.GetTextures()[i].Type);
            REQUIRE(lhs.GetTextures()[i].Texture == rhs.GetTextures()[i].Texture);
            REQUIRE(lhs.GetTextures()[i].SamplerFlags == rhs.GetTextures()[i].SamplerFlags);
        }
    }
} // namespace

static_assert(CCustomInternalSerializable<FMeshMaterialParameterBlock>,
    "FMeshMaterialParameterBlock should satisfy CCustomInternalSerializable");

TEST_CASE("Asset.MeshMaterialParameterBlock.Serialization.BinaryRoundTrip") {
    const FMaterialParamId      scalarId  = 0xA1B2C3D4U;
    const FMaterialParamId      vectorId  = 0x11223344U;
    const FMaterialParamId      matrixId  = 0x55667788U;
    const FMaterialParamId      textureId = 0x99AABBCCU;

    FMeshMaterialParameterBlock original;
    original.SetScalar(scalarId, 0.75f);
    original.SetVector(vectorId, Math::FVector4f(1.0f, 2.0f, 3.0f, 4.0f));

    Math::FMatrix4x4f matrix(0.0f);
    for (u32 i = 0U; i < 4U; ++i) {
        matrix.mElements[i][i] = static_cast<f32>(i + 1U);
    }
    original.SetMatrix(matrixId, matrix);

    FAssetHandle textureHandle{};
    textureHandle.Uuid = MakeUuid(10U);
    textureHandle.Type = EAssetType::Texture2D;
    original.SetTexture(textureId, EMeshMaterialTextureType::Texture2D, textureHandle, 123U);

    FBinarySerializer serializer;
    SerializeInvoker(original, serializer);

    FBinaryDeserializer deserializer;
    deserializer.SetBuffer(serializer.GetBuffer());
    const auto decoded = DeserializeInvoker<FMeshMaterialParameterBlock>(deserializer);

    RequireBlockEquals(original, decoded);
}

TEST_CASE("Asset.MeshMaterialParameterBlock.Serialization.JsonRoundTrip") {
    const FMaterialParamId      scalarId  = 0x01020304U;
    const FMaterialParamId      vectorId  = 0x10203040U;
    const FMaterialParamId      matrixId  = 0x50607080U;
    const FMaterialParamId      textureId = 0x0A0B0C0DU;

    FMeshMaterialParameterBlock original;
    original.SetScalar(scalarId, 0.25f);
    original.SetVector(vectorId, Math::FVector4f(0.25f, 0.5f, 0.75f, 1.0f));

    Math::FMatrix4x4f matrix(0.0f);
    matrix.mElements[0][0] = 2.0f;
    matrix.mElements[1][1] = 3.0f;
    matrix.mElements[2][2] = 4.0f;
    matrix.mElements[3][3] = 5.0f;
    original.SetMatrix(matrixId, matrix);

    FAssetHandle textureHandle{};
    textureHandle.Uuid = MakeUuid(42U);
    textureHandle.Type = EAssetType::Texture2D;
    original.SetTexture(textureId, EMeshMaterialTextureType::Texture2D, textureHandle, 77U);

    FJsonSerializer serializer;
    SerializeInvoker(original, serializer);

    FJsonDeserializer deserializer;
    REQUIRE(deserializer.SetText(serializer.GetText()));
    const auto decoded = DeserializeInvoker<FMeshMaterialParameterBlock>(deserializer);

    RequireBlockEquals(original, decoded);
}
