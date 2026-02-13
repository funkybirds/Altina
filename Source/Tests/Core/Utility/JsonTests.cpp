#include "TestHarness.h"

#include "Utility/Json.h"

#include <string_view>

namespace {
    namespace Container = AltinaEngine::Core::Container;
    using AltinaEngine::Core::Utility::Json::EJsonType;
    using AltinaEngine::Core::Utility::Json::FindObjectValue;
    using AltinaEngine::Core::Utility::Json::FindObjectValueInsensitive;
    using AltinaEngine::Core::Utility::Json::FJsonDocument;
    using AltinaEngine::Core::Utility::Json::FJsonValue;
    using AltinaEngine::Core::Utility::Json::GetBoolValue;
    using AltinaEngine::Core::Utility::Json::GetNumberValue;
    using AltinaEngine::Core::Utility::Json::GetStringValue;
    using Container::FNativeString;
    using Container::FNativeStringView;

    auto EqualsLiteral(const FNativeString& value, const char* expected) -> bool {
        const std::string_view view(value.GetData(), value.Length());
        return view == std::string_view(expected);
    }
} // namespace

TEST_CASE("Json parse simple object") {
    const char*   json = "{\"Name\":\"Test\",\"Value\":12.5,\"Flag\":true}";
    FJsonDocument doc;
    REQUIRE(doc.Parse(FNativeStringView(json)));

    const FJsonValue* root = doc.GetRoot();
    REQUIRE(root != nullptr);
    REQUIRE(root->Type == EJsonType::Object);

    FNativeString name;
    REQUIRE(GetStringValue(FindObjectValue(*root, "Name"), name));
    REQUIRE(EqualsLiteral(name, "Test"));

    double value = 0.0;
    REQUIRE(GetNumberValue(FindObjectValue(*root, "Value"), value));
    REQUIRE_CLOSE(value, 12.5, 0.0001);

    bool flag = false;
    REQUIRE(GetBoolValue(FindObjectValue(*root, "Flag"), flag));
    REQUIRE(flag);
}

TEST_CASE("Json parse arrays and nested objects") {
    const char* json =
        "{\n"
        "  \"Meta\": {\"Version\": 1, \"Name\": \"Demo\", \"Tags\": [\"A\", \"B\"]},\n"
        "  \"Values\": [\n"
        "    {\"Id\": 1, \"Enabled\": true},\n"
        "    {\"Id\": 2, \"Enabled\": false}\n"
        "  ],\n"
        "  \"Numbers\": [0, -1, 3.5, 1e3, -2.5E-2],\n"
        "  \"Text\": \"Line1\\nLine2\\tTabbed\\\"Quote\\\"\\\\Backslash\",\n"
        "  \"NullValue\": null\n"
        "}";
    FJsonDocument doc;
    REQUIRE(doc.Parse(FNativeStringView(json)));

    const FJsonValue* root = doc.GetRoot();
    REQUIRE(root != nullptr);
    REQUIRE(root->Type == EJsonType::Object);

    const FJsonValue* meta = FindObjectValue(*root, "Meta");
    REQUIRE(meta != nullptr);
    REQUIRE(meta->Type == EJsonType::Object);

    double version = 0.0;
    REQUIRE(GetNumberValue(FindObjectValue(*meta, "Version"), version));
    REQUIRE_EQ(static_cast<int>(version), 1);

    FNativeString name;
    REQUIRE(GetStringValue(FindObjectValue(*meta, "Name"), name));
    REQUIRE(EqualsLiteral(name, "Demo"));

    const FJsonValue* tags = FindObjectValue(*meta, "Tags");
    REQUIRE(tags != nullptr);
    REQUIRE(tags->Type == EJsonType::Array);
    REQUIRE_EQ(tags->Array.Size(), static_cast<AltinaEngine::usize>(2));

    FNativeString tag0;
    REQUIRE(GetStringValue(tags->Array[0], tag0));
    REQUIRE(EqualsLiteral(tag0, "A"));

    const FJsonValue* values = FindObjectValue(*root, "Values");
    REQUIRE(values != nullptr);
    REQUIRE(values->Type == EJsonType::Array);
    REQUIRE_EQ(values->Array.Size(), static_cast<AltinaEngine::usize>(2));

    const FJsonValue* firstValue = values->Array[0];
    REQUIRE(firstValue != nullptr);
    REQUIRE(firstValue->Type == EJsonType::Object);

    double id = 0.0;
    REQUIRE(GetNumberValue(FindObjectValue(*firstValue, "Id"), id));
    REQUIRE_EQ(static_cast<int>(id), 1);

    bool enabled = false;
    REQUIRE(GetBoolValue(FindObjectValue(*firstValue, "Enabled"), enabled));
    REQUIRE(enabled);

    const FJsonValue* numbers = FindObjectValue(*root, "Numbers");
    REQUIRE(numbers != nullptr);
    REQUIRE(numbers->Type == EJsonType::Array);
    REQUIRE_EQ(numbers->Array.Size(), static_cast<AltinaEngine::usize>(5));

    double n0 = 0.0;
    REQUIRE(GetNumberValue(numbers->Array[0], n0));
    REQUIRE_EQ(static_cast<int>(n0), 0);

    double n3 = 0.0;
    REQUIRE(GetNumberValue(numbers->Array[3], n3));
    REQUIRE_CLOSE(n3, 1000.0, 0.0001);

    FNativeString text;
    REQUIRE(GetStringValue(FindObjectValue(*root, "Text"), text));
    REQUIRE(EqualsLiteral(text, "Line1\nLine2\tTabbed\"Quote\"\\Backslash"));

    const FJsonValue* nullValue = FindObjectValue(*root, "NullValue");
    REQUIRE(nullValue != nullptr);
    REQUIRE(nullValue->Type == EJsonType::Null);
}

TEST_CASE("Json parse root array") {
    const char*   json = "[\"a\", 1, false, null, {\"k\":\"v\"}]";
    FJsonDocument doc;
    REQUIRE(doc.Parse(FNativeStringView(json)));

    const FJsonValue* root = doc.GetRoot();
    REQUIRE(root != nullptr);
    REQUIRE(root->Type == EJsonType::Array);
    REQUIRE_EQ(root->Array.Size(), static_cast<AltinaEngine::usize>(5));

    FNativeString str;
    REQUIRE(GetStringValue(root->Array[0], str));
    REQUIRE(EqualsLiteral(str, "a"));

    double num = 0.0;
    REQUIRE(GetNumberValue(root->Array[1], num));
    REQUIRE_EQ(static_cast<int>(num), 1);

    bool flag = true;
    REQUIRE(GetBoolValue(root->Array[2], flag));
    REQUIRE(!flag);

    REQUIRE(root->Array[3]->Type == EJsonType::Null);

    const FJsonValue* obj = root->Array[4];
    REQUIRE(obj != nullptr);
    REQUIRE(obj->Type == EJsonType::Object);

    FNativeString value;
    REQUIRE(GetStringValue(FindObjectValue(*obj, "k"), value));
    REQUIRE(EqualsLiteral(value, "v"));
}

TEST_CASE("Json case-insensitive key lookup") {
    const char*   json = "{\"SchemaVersion\":1}";
    FJsonDocument doc;
    REQUIRE(doc.Parse(FNativeStringView(json)));

    const FJsonValue* root = doc.GetRoot();
    REQUIRE(root != nullptr);

    const FJsonValue* value = FindObjectValueInsensitive(*root, "sCheMaVeRsIoN");
    REQUIRE(value != nullptr);
    REQUIRE(value->Type == EJsonType::Number);
}

TEST_CASE("Json unicode escape ascii") {
    const char*   json = "{\"Text\":\"\\u0041\\u0042\"}";
    FJsonDocument doc;
    REQUIRE(doc.Parse(FNativeStringView(json)));

    const FJsonValue* root = doc.GetRoot();
    REQUIRE(root != nullptr);

    FNativeString text;
    REQUIRE(GetStringValue(FindObjectValue(*root, "Text"), text));
    REQUIRE(EqualsLiteral(text, "AB"));
}

TEST_CASE("Json rejects invalid input") {
    FJsonDocument doc;
    REQUIRE(!doc.Parse(FNativeStringView("{")));
    REQUIRE(!doc.Parse(FNativeStringView("{} trailing")));
    REQUIRE(doc.GetError().Length() > 0);
}

TEST_CASE("Json rejects trailing comma and bad escape") {
    FJsonDocument doc;
    REQUIRE(!doc.Parse(FNativeStringView("{\"a\":1,}")));
    REQUIRE(!doc.Parse(FNativeStringView("[1,]")));
    REQUIRE(!doc.Parse(FNativeStringView("{\"a\":\"\\x\"}")));
}
