#include "TestHarness.h"
#include "Base/AltinaBase.h"
#include "Reflection/JsonSerializer.h"
#include "Reflection/JsonDeserializer.h"
#include "Reflection/Serialization.h"
#include "Reflection/Traits.h"
#include "Reflection/Reflection.h"
#include <cstdio>

using namespace AltinaEngine;
using namespace AltinaEngine::Core;
using namespace AltinaEngine::Core::Reflection;

struct FPoint2DJson {
    f32 mX;
    f32 mY;

    FPoint2DJson() : mX(0.0f), mY(0.0f) {}
    FPoint2DJson(f32 x, f32 y) : mX(x), mY(y) {}

    auto Serialize(ISerializer& serializer) const -> void {
        serializer.BeginArray(2);
        serializer.Write(mX);
        serializer.Write(mY);
        serializer.EndArray();
    }

    static auto Deserialize(IDeserializer& deserializer) -> FPoint2DJson {
        FPoint2DJson result;
        usize        count = 0;
        deserializer.BeginArray(count);
        result.mX = deserializer.Read<f32>();
        result.mY = deserializer.Read<f32>();
        deserializer.EndArray();
        return result;
    }

    auto operator==(const FPoint2DJson& other) const -> bool {
        return mX == other.mX && mY == other.mY;
    }
};

static_assert(CCustomInternalSerializable<FPoint2DJson>,
    "FPoint2DJson should satisfy CCustomInternalSerializable");
static_assert(CTriviallySerializable<f32>, "f32 should be trivially serializable");
static_assert(!CCustomInternalSerializable<f32>, "f32 should not be custom internal serializable");

TEST_CASE("Reflection.Serialization.Json.Point2D") {
    FJsonSerializer   serializer;
    FJsonDeserializer deserializer;
    FPoint2DJson      original(3.5f, 7.2f);

    SerializeInvoker(original, serializer);
    const bool parseOk = deserializer.SetText(serializer.GetText());
    if (!parseOk) {
        const auto text = serializer.GetString();
        const auto err  = deserializer.GetError();
        std::printf("JSON parse failed: %.*s\n", static_cast<int>(text.Length()), text.GetData());
        std::printf("Error: %.*s\n", static_cast<int>(err.Length()), err.Data());
        std::fflush(stdout);
    }
    REQUIRE(parseOk);
    FPoint2DJson result = DeserializeInvoker<FPoint2DJson>(deserializer);

    REQUIRE(result == original);
    REQUIRE(result.mX == original.mX);
    REQUIRE(result.mY == original.mY);
}
