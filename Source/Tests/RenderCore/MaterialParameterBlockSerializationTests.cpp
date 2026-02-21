#include "TestHarness.h"
#include "Base/AltinaBase.h"
#include "Material/MaterialParameters.h"
#include "Material/MaterialPass.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/BinarySerializer.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core;
using namespace AltinaEngine::Core::Reflection;
using namespace AltinaEngine::RenderCore;

TEST_CASE("RenderCore.MaterialParameters.Serialization.RoundTrip") {
    const auto              roughnessId = HashMaterialParamName(TEXT("Roughness"));
    const auto              colorId     = HashMaterialParamName(TEXT("BaseColor"));
    const auto              matrixId    = HashMaterialParamName(TEXT("World"));

    FMaterialParameterBlock original;
    original.SetScalar(roughnessId, 0.7f);
    original.SetVector(colorId, Math::FVector4f(0.1f, 0.2f, 0.3f, 1.0f));

    Math::FMatrix4x4f world(0.0f);
    for (u32 i = 0U; i < 4U; ++i) {
        world.mElements[i][i] = 1.0f + static_cast<f32>(i);
    }
    original.SetMatrix(matrixId, world);

    FBinarySerializer serializer;
    original.Serialize(serializer);

    FBinaryDeserializer deserializer;
    deserializer.SetBuffer(serializer.GetBuffer());
    const auto  decoded = FMaterialParameterBlock::Deserialize(deserializer);

    const auto* scalar = decoded.FindScalarParam(roughnessId);
    REQUIRE(scalar != nullptr);
    REQUIRE(scalar->Value == 0.7f);

    const auto* vector = decoded.FindVectorParam(colorId);
    REQUIRE(vector != nullptr);
    REQUIRE(vector->Value.mComponents[0] == 0.1f);
    REQUIRE(vector->Value.mComponents[1] == 0.2f);
    REQUIRE(vector->Value.mComponents[2] == 0.3f);
    REQUIRE(vector->Value.mComponents[3] == 1.0f);

    const auto* matrix = decoded.FindMatrixParam(matrixId);
    REQUIRE(matrix != nullptr);
    for (u32 i = 0U; i < 4U; ++i) {
        for (u32 j = 0U; j < 4U; ++j) {
            const f32 expected = (i == j) ? (1.0f + static_cast<f32>(i)) : 0.0f;
            REQUIRE(matrix->Value.mElements[i][j] == expected);
        }
    }
}
