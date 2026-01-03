#include "TestHarness.h"
#include "Base/AltinaBase.h"
#include "Reflection/BinarySerializer.h"
#include "Reflection/BinaryDeserializer.h"
#include "Reflection/Serialization.h"

using namespace AltinaEngine;
using namespace AltinaEngine::Core;
using namespace AltinaEngine::Core::Reflection;

TEST_CASE("Reflection.Serialization.Primitive.Bool") {
    FBinarySerializer   serializer;
    FBinaryDeserializer deserializer;
    bool                original = true;
    SerializeInvoker(original, serializer);
    deserializer.SetBuffer(serializer.GetBuffer());
    bool result = DeserializeInvoker<bool>(deserializer);
    REQUIRE(result == original);
}

TEST_CASE("Reflection.Serialization.Primitive.I32") {
    FBinarySerializer   serializer;
    FBinaryDeserializer deserializer;
    i32                 original = -123456;
    SerializeInvoker(original, serializer);
    deserializer.SetBuffer(serializer.GetBuffer());
    i32 result = DeserializeInvoker<i32>(deserializer);
    REQUIRE(result == original);
}

TEST_CASE("Reflection.Serialization.Primitive.U64") {
    FBinarySerializer   serializer;
    FBinaryDeserializer deserializer;
    u64                 original = 12345678901234567890ULL;
    SerializeInvoker(original, serializer);
    deserializer.SetBuffer(serializer.GetBuffer());
    u64 result = DeserializeInvoker<u64>(deserializer);
    REQUIRE(result == original);
}

TEST_CASE("Reflection.Serialization.Primitive.F32") {
    FBinarySerializer   serializer;
    FBinaryDeserializer deserializer;
    f32                 original = 3.14159f;
    SerializeInvoker(original, serializer);
    deserializer.SetBuffer(serializer.GetBuffer());
    f32 result = DeserializeInvoker<f32>(deserializer);
    REQUIRE(result == original);
}

TEST_CASE("Reflection.Serialization.Primitive.Multiple") {
    FBinarySerializer   serializer;
    FBinaryDeserializer deserializer;
    i32                 value1 = 42;
    f32                 value2 = 3.14f;
    bool                value3 = true;

    SerializeInvoker(value1, serializer);
    SerializeInvoker(value2, serializer);
    SerializeInvoker(value3, serializer);

    deserializer.SetBuffer(serializer.GetBuffer());
    i32  result1 = DeserializeInvoker<i32>(deserializer);
    f32  result2 = DeserializeInvoker<f32>(deserializer);
    bool result3 = DeserializeInvoker<bool>(deserializer);

    REQUIRE(result1 == value1);
    REQUIRE(result2 == value2);
    REQUIRE(result3 == value3);
}
