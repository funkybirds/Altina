#include "TestHarness.h"
#include "Base/AltinaBase.h"
#include "Reflection/BinarySerializer.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/Serialization.h"
#include "Reflection/Traits.h"
#include "Reflection/Reflection.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core;
using namespace AltinaEngine::Core::Reflection;

struct FPoint2D {
    f32 mX;
    f32 mY;

    FPoint2D() : mX(0.0f), mY(0.0f) {}
    FPoint2D(f32 x, f32 y) : mX(x), mY(y) {}

    auto Serialize(ISerializer& serializer) const -> void {
        // Directly call serializer.Write for member fields
        // Do NOT use SerializeInvoker here to avoid circular dependency in concept checking
        serializer.Write(mX);
        serializer.Write(mY);
    }

    static auto Deserialize(IDeserializer& deserializer) -> FPoint2D {
        FPoint2D result;
        // Directly call deserializer.Read for member fields
        // Do NOT use DeserializeInvoker here to avoid circular dependency
        result.mX = deserializer.Read<f32>();
        result.mY = deserializer.Read<f32>();
        return result;
    }

    auto operator==(const FPoint2D& other) const -> bool {
        return mX == other.mX && mY == other.mY;
    }
};

// Compile-time assertion to verify FPoint2D satisfies the concept
static_assert(
    CCustomInternalSerializable<FPoint2D>, "FPoint2D should satisfy CCustomInternalSerializable");
static_assert(CTriviallySerializable<f32>, "f32 should be trivially serializable");
static_assert(!CCustomInternalSerializable<f32>, "f32 should not be custom internal serializable");

TEST_CASE("Reflection.Serialization.Internal.Point2D") {
    // FPoint2D uses internal serialization via Serialize/Deserialize methods
    // No type registration needed as it satisfies CCustomInternalSerializable
    FBinarySerializer   serializer;
    FBinaryDeserializer deserializer;
    FPoint2D            original(3.5f, 7.2f);

    SerializeInvoker(original, serializer);
    deserializer.SetBuffer(serializer.GetBuffer());
    FPoint2D result = DeserializeInvoker<FPoint2D>(deserializer);

    REQUIRE(result == original);
    REQUIRE(result.mX == original.mX);
    REQUIRE(result.mY == original.mY);
}
