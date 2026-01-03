#include "TestHarness.h"
#include "Base/AltinaBase.h"
#include "Reflection/BinarySerializer.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/Serialization.h"
#include "Reflection/Traits.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core;
using namespace AltinaEngine::Core::Reflection;

// Legacy type without internal serialization methods
struct FLegacyVector3 {
    f32 x;
    f32 y;
    f32 z;

    FLegacyVector3() : x(0.0f), y(0.0f), z(0.0f) {}
    FLegacyVector3(f32 x_, f32 y_, f32 z_) : x(x_), y(y_), z(z_) {}

    auto operator==(const FLegacyVector3& other) const -> bool {
        return x == other.x && y == other.y && z == other.z;
    }
};

// External serialization rule specialization
template <> class AltinaEngine::Core::Reflection::TCustomSerializeRule<FLegacyVector3> {
public:
    static auto Serialize(const FLegacyVector3& value, ISerializer& serializer) -> void {
        SerializeInvoker(value.x, serializer);
        SerializeInvoker(value.y, serializer);
        SerializeInvoker(value.z, serializer);
    }

    static auto Deserialize(IDeserializer& deserializer) -> FLegacyVector3 {
        FLegacyVector3 result;
        result.x = DeserializeInvoker<f32>(deserializer);
        result.y = DeserializeInvoker<f32>(deserializer);
        result.z = DeserializeInvoker<f32>(deserializer);
        return result;
    }
};

TEST_CASE("Reflection.Serialization.External.Vector3") {
    FBinarySerializer   serializer;
    FBinaryDeserializer deserializer;
    FLegacyVector3      original(1.5f, 2.7f, -3.2f);

    SerializeInvoker(original, serializer);
    deserializer.SetBuffer(serializer.GetBuffer());
    FLegacyVector3 result = DeserializeInvoker<FLegacyVector3>(deserializer);

    REQUIRE(result == original);
    REQUIRE(result.x == original.x);
    REQUIRE(result.y == original.y);
    REQUIRE(result.z == original.z);
}
