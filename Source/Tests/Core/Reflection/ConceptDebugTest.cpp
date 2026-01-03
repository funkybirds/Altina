#include "TestHarness.h"
#include "Base/AltinaBase.h"
#include "Reflection/Serialization.h"
#include "Reflection/BinarySerializer.h"
#include "Reflection/BinaryDeserializer.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core;
using namespace AltinaEngine::Core::Reflection;

// Copy-paste exact FPoint2D definition from SerializationInternalTests.cpp
struct FPoint2D {
    f32 mX;
    f32 mY;

    FPoint2D() : mX(0.0f), mY(0.0f) {}
    FPoint2D(f32 x, f32 y) : mX(x), mY(y) {}

    auto Serialize(ISerializer& serializer) const -> void {
        serializer.Write(mX);
        serializer.Write(mY);
    }

    static auto Deserialize(IDeserializer& deserializer) -> FPoint2D {
        FPoint2D result;
        result.mX = deserializer.Read<f32>();
        result.mY = deserializer.Read<f32>();
        return result;
    }

    auto operator==(const FPoint2D& other) const -> bool {
        return mX == other.mX && mY == other.mY;
    }
};

// Test that concept is satisfied
static_assert(CCustomInternalSerializable<FPoint2D>, "FPoint2D should be internal serializable");

// Test template path selection at compile time
template <typename T> constexpr auto TestSerializationPath() -> int {
    if constexpr (CTriviallySerializable<T>) {
        return 1; // Trivial
    } else if constexpr (CCustomInternalSerializable<T>) {
        return 2; // Internal
    } else if constexpr (CCustomExternalSerializable<T>) {
        return 3; // External
    } else {
        return 4; // Dynamic
    }
}

static_assert(TestSerializationPath<FPoint2D>() == 2, "FPoint2D should use internal path");
static_assert(TestSerializationPath<f32>() == 1, "f32 should use trivial path");

TEST_CASE("Concept.Debug.FPoint2D") {
    constexpr int path = TestSerializationPath<FPoint2D>();
    REQUIRE(path == 2); // Should be internal serialization path

    // Actually test serialization
    FBinarySerializer   serializer;
    FBinaryDeserializer deserializer;
    FPoint2D            original(3.5f, 7.2f);

    SerializeInvoker(original, serializer);
    deserializer.SetBuffer(serializer.GetBuffer());
    FPoint2D result = DeserializeInvoker<FPoint2D>(deserializer);

    REQUIRE(result == original);
}
