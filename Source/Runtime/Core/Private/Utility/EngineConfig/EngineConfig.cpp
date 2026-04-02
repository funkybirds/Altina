#include "Utility/EngineConfig/EngineConfig.h"

#include "Algorithm/CStringUtils.h"
#include "CoreMinimal.h"
#include "Logging/Log.h"
#include "Platform/PlatformFileSystem.h"
#include "Types/NumericProperties.h"
#include "Utility/String/CodeConvert.h"

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>

using AltinaEngine::Move;
using AltinaEngine::Core::Algorithm::ToLowerChar;
using AltinaEngine::Core::Container::FNativeString;
using AltinaEngine::Core::Container::FNativeStringView;
using AltinaEngine::Core::Container::FString;
using AltinaEngine::Core::Container::FStringView;
using AltinaEngine::Core::Container::TVector;
using AltinaEngine::Core::Platform::GetExecutableDir;
using AltinaEngine::Core::Platform::IsPathExist;
using AltinaEngine::Core::Platform::ReadFileTextUtf8;
using AltinaEngine::Core::Utility::Json::EJsonType;
using AltinaEngine::Core::Utility::Json::FindObjectValue;
using AltinaEngine::Core::Utility::Json::FJsonValue;
using AltinaEngine::Core::Utility::Json::GetBoolValue;
using AltinaEngine::Core::Utility::Json::GetNumberValue;
using AltinaEngine::Core::Utility::Json::GetStringValue;
using AltinaEngine::Core::Utility::String::FromUtf8;

namespace AltinaEngine::Core::Utility::EngineConfig {
    namespace {
        constexpr char kOverridePrefixA[] = "-Config:";
        constexpr char kOverridePrefixB[] = "-Config=";

        auto           ToNativeString(FStringView text) -> FNativeString {
            FNativeString out;
            if (text.Length() == 0) {
                return out;
            }
            out.Reserve(text.Length());
            for (usize i = 0; i < text.Length(); ++i) {
                out.Append(static_cast<char>(text[i]));
            }
            return out;
        }

        auto MakeConfigPath(FStringView suffix) -> FString {
            FString baseDir = GetExecutableDir();
            if (baseDir.IsEmptyString()) {
                return {};
            }
            baseDir.Append(suffix);
            return baseDir;
        }

        auto LoadDefaultConfigText(FNativeString& outText) -> bool {
            FString path = MakeConfigPath(TEXT("/Assets/Config/DefaultGame.json"));
            if (!path.IsEmptyString() && IsPathExist(path)) {
                return ReadFileTextUtf8(path, outText);
            }
            path = MakeConfigPath(TEXT("/Asset/Config/DefaultGame.json"));
            if (!path.IsEmptyString() && IsPathExist(path)) {
                return ReadFileTextUtf8(path, outText);
            }
            return false;
        }

        auto StartsWithIgnoreCase(FNativeStringView text, const char* prefix) -> bool {
            if (prefix == nullptr) {
                return false;
            }
            const usize prefixLength = static_cast<usize>(std::strlen(prefix));
            if (text.Length() < prefixLength) {
                return false;
            }
            for (usize i = 0; i < prefixLength; ++i) {
                if (ToLowerChar(text[i]) != ToLowerChar(prefix[i])) {
                    return false;
                }
            }
            return true;
        }

        auto SplitTokens(FNativeStringView commandLine, TVector<FNativeString>& outTokens) -> void {
            outTokens.Clear();
            const char* data   = commandLine.Data();
            const usize length = commandLine.Length();
            usize       index  = 0;
            while (index < length) {
                while (index < length && std::isspace(static_cast<unsigned char>(data[index]))) {
                    ++index;
                }
                if (index >= length) {
                    break;
                }

                FNativeString token;
                bool          inQuote = false;
                while (index < length) {
                    const char ch = data[index];
                    if (ch == '"') {
                        inQuote = !inQuote;
                        ++index;
                        continue;
                    }
                    if (!inQuote && std::isspace(static_cast<unsigned char>(data[index])) != 0) {
                        break;
                    }
                    token.Append(ch);
                    ++index;
                }

                if (!token.IsEmptyString()) {
                    outTokens.PushBack(Move(token));
                }
            }
        }

        auto TrimAsciiWhitespace(FNativeStringView text) -> FNativeStringView {
            usize start = 0;
            usize end   = text.Length();
            while (start < end && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
                ++start;
            }
            while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1])) != 0) {
                --end;
            }
            return { text.Data() + start, end - start };
        }

        auto StripQuotes(FNativeStringView text) -> FNativeStringView {
            if (text.Length() >= 2 && text[0] == '"' && text[text.Length() - 1] == '"') {
                return { text.Data() + 1, text.Length() - 2 };
            }
            return text;
        }

        auto EqualsIgnoreCase(FNativeStringView left, const char* right) -> bool {
            if (right == nullptr) {
                return false;
            }
            const usize rightLength = static_cast<usize>(std::strlen(right));
            if (left.Length() != rightLength) {
                return false;
            }
            for (usize i = 0; i < rightLength; ++i) {
                if (ToLowerChar(left[i]) != ToLowerChar(right[i])) {
                    return false;
                }
            }
            return true;
        }

        auto ParseBoolOverride(FNativeStringView text, bool& outValue) -> bool {
            const auto trimmed = TrimAsciiWhitespace(text);
            if (EqualsIgnoreCase(trimmed, "true")) {
                outValue = true;
                return true;
            }
            if (EqualsIgnoreCase(trimmed, "false")) {
                outValue = false;
                return true;
            }
            return false;
        }

        auto ParseInt64Override(FNativeStringView text, i64& outValue) -> bool {
            auto trimmed = TrimAsciiWhitespace(text);
            trimmed      = StripQuotes(trimmed);
            if (trimmed.Length() == 0) {
                return false;
            }

            FNativeString buffer;
            buffer.Append(trimmed.Data(), trimmed.Length());

            errno          = 0;
            char*      end = nullptr;
            const auto value =
                static_cast<long long>(std::strtoll(buffer.CStr(), &end, 0 /* base */));
            if (end == buffer.CStr() || end == nullptr || *end != '\0') {
                return false;
            }
            if (errno == ERANGE) {
                return false;
            }
            outValue = static_cast<i64>(value);
            return true;
        }

        auto ParseUint64Override(FNativeStringView text, u64& outValue) -> bool {
            auto trimmed = TrimAsciiWhitespace(text);
            trimmed      = StripQuotes(trimmed);
            if (trimmed.Length() == 0) {
                return false;
            }
            if (trimmed[0] == '-') {
                return false;
            }

            FNativeString buffer;
            buffer.Append(trimmed.Data(), trimmed.Length());

            errno          = 0;
            char*      end = nullptr;
            const auto value =
                static_cast<unsigned long long>(std::strtoull(buffer.CStr(), &end, 0 /* base */));
            if (end == buffer.CStr() || end == nullptr || *end != '\0') {
                return false;
            }
            if (errno == ERANGE) {
                return false;
            }
            outValue = static_cast<u64>(value);
            return true;
        }

        auto ParseFloat64Override(FNativeStringView text, f64& outValue) -> bool {
            auto trimmed = TrimAsciiWhitespace(text);
            trimmed      = StripQuotes(trimmed);
            if (trimmed.Length() == 0) {
                return false;
            }

            FNativeString buffer;
            buffer.Append(trimmed.Data(), trimmed.Length());

            errno            = 0;
            char*      end   = nullptr;
            const auto value = std::strtod(buffer.CStr(), &end);
            if (end == buffer.CStr() || end == nullptr || *end != '\0') {
                return false;
            }
            if (errno == ERANGE) {
                return false;
            }
            outValue = static_cast<f64>(value);
            return true;
        }

        auto ParseStringArrayOverride(FNativeStringView text, TVector<FString>& outValues) -> bool {
            auto trimmed   = TrimAsciiWhitespace(text);
            bool bracketed = false;
            if (trimmed.Length() >= 2 && trimmed[0] == '['
                && trimmed[trimmed.Length() - 1] == ']') {
                trimmed   = { trimmed.Data() + 1, trimmed.Length() - 2 };
                bracketed = true;
            }

            if (trimmed.Length() == 0) {
                outValues.Clear();
                return true;
            }

            bool hasComma = false;
            for (usize i = 0; i < trimmed.Length(); ++i) {
                if (trimmed[i] == ',') {
                    hasComma = true;
                    break;
                }
            }
            if (!hasComma && !bracketed) {
                return false;
            }

            outValues.Clear();
            usize       start  = 0;
            const char* data   = trimmed.Data();
            const usize length = trimmed.Length();
            while (start <= length) {
                usize end = start;
                while (end < length && data[end] != ',') {
                    ++end;
                }
                auto part = TrimAsciiWhitespace({ data + start, end - start });
                part      = StripQuotes(part);
                if (part.Length() > 0) {
                    FNativeString piece;
                    piece.Append(part.Data(), part.Length());
                    outValues.PushBack(FromUtf8(piece));
                }
                if (end >= length) {
                    break;
                }
                start = end + 1;
            }
            return !outValues.IsEmpty();
        }

        auto FindJsonValueByPath(const FJsonValue* root, FStringView path) -> const FJsonValue* {
            if (root == nullptr || path.Length() == 0) {
                return nullptr;
            }
            FNativeString nativePath = ToNativeString(path);
            if (nativePath.IsEmptyString()) {
                return nullptr;
            }

            const char*       data    = nativePath.GetData();
            const usize       length  = nativePath.Length();
            usize             start   = 0;
            const FJsonValue* current = root;
            while (start < length && current != nullptr) {
                while (start < length && data[start] == '/') {
                    ++start;
                }
                if (start >= length) {
                    break;
                }
                usize end = start;
                while (end < length && data[end] != '/') {
                    ++end;
                }

                FNativeString key;
                if (end > start) {
                    key.Append(data + start, end - start);
                }
                current = (key.IsEmptyString() || current == nullptr)
                    ? nullptr
                    : FindObjectValue(*current, key.CStr());

                if (end >= length) {
                    break;
                }
                start = end + 1;
            }
            return current;
        }

        auto ReadStringArrayFromJson(const FJsonValue* value) -> TVector<FString> {
            TVector<FString> out;
            if (value == nullptr || value->Type != EJsonType::Array) {
                return out;
            }

            for (const auto* entry : value->Array) {
                FNativeString native;
                if (!GetStringValue(entry, native)) {
                    continue;
                }
                out.PushBack(FromUtf8(native));
            }
            return out;
        }
    } // namespace

    auto FConfigCollection::ParseJsonConfig(FNativeStringView jsonText) -> bool {
        mDocument.Clear();
        return mDocument.Parse(jsonText);
    }

    void FConfigCollection::ApplyStartupParamOverrides(
        const FStartupParameters& startupParameters) {
        mOverrides.Clear();

        const FNativeStringView commandLine = startupParameters.mCommandLine.ToView();
        if (commandLine.Length() == 0) {
            return;
        }

        TVector<FNativeString> tokens;
        SplitTokens(commandLine, tokens);

        for (const auto& token : tokens) {
            const FNativeStringView tokenView    = token.ToView();
            usize                   prefixLength = 0;
            if (StartsWithIgnoreCase(tokenView, kOverridePrefixA)) {
                prefixLength = static_cast<usize>(std::strlen(kOverridePrefixA));
            } else if (StartsWithIgnoreCase(tokenView, kOverridePrefixB)) {
                prefixLength = static_cast<usize>(std::strlen(kOverridePrefixB));
            } else {
                continue;
            }

            if (tokenView.Length() <= prefixLength) {
                continue;
            }

            const FNativeStringView payload       = { tokenView.Data() + prefixLength,
                      tokenView.Length() - prefixLength };
            const char*             payloadData   = payload.Data();
            const usize             payloadLength = payload.Length();
            usize                   equalPos      = 0;
            while (equalPos < payloadLength && payloadData[equalPos] != '=') {
                ++equalPos;
            }
            if (equalPos == 0 || equalPos >= payloadLength) {
                continue;
            }

            FNativeStringView keyView(payloadData, equalPos);
            FNativeStringView valueView(payloadData + equalPos + 1, payloadLength - equalPos - 1);
            keyView   = TrimAsciiWhitespace(keyView);
            valueView = TrimAsciiWhitespace(valueView);
            if (keyView.Length() == 0) {
                continue;
            }

            FNativeString keyNative;
            keyNative.Append(keyView.Data(), keyView.Length());
            FString          key = FromUtf8(keyNative);

            FOverrideValue   overrideValue{};
            bool             boolValue = false;
            TVector<FString> arrayValue;
            if (ParseBoolOverride(valueView, boolValue)) {
                overrideValue.Type      = EOverrideType::Bool;
                overrideValue.BoolValue = boolValue;
            } else if (ParseStringArrayOverride(valueView, arrayValue)) {
                overrideValue.Type             = EOverrideType::StringArray;
                overrideValue.StringArrayValue = Move(arrayValue);
            } else {
                FNativeString trimmed;
                const auto    stripped = StripQuotes(valueView);
                trimmed.Append(stripped.Data(), stripped.Length());
                overrideValue.Type        = EOverrideType::String;
                overrideValue.StringValue = FromUtf8(trimmed);
            }

            mOverrides[key] = Move(overrideValue);
        }
    }

    void FConfigCollection::Clear() noexcept {
        mDocument.Clear();
        mOverrides.Clear();
    }

    auto FConfigCollection::GetBool(FStringView path) const noexcept -> bool {
        if (const auto* overrideValue = FindOverride(path)) {
            if (overrideValue->Type == EOverrideType::Bool) {
                return overrideValue->BoolValue;
            }
            if (overrideValue->Type == EOverrideType::String) {
                FNativeString native = ToNativeString(overrideValue->StringValue.ToView());
                bool          value  = false;
                if (ParseBoolOverride(native.ToView(), value)) {
                    return value;
                }
            }
            return false;
        }

        const auto* root  = mDocument.GetRoot();
        const auto* value = FindJsonValueByPath(root, path);
        bool        out   = false;
        if (GetBoolValue(value, out)) {
            return out;
        }
        return false;
    }

    auto FConfigCollection::GetStringArray(FStringView path) const -> TVector<FString> {
        if (const auto* overrideValue = FindOverride(path)) {
            if (overrideValue->Type == EOverrideType::StringArray) {
                return overrideValue->StringArrayValue;
            }
            if (overrideValue->Type == EOverrideType::String) {
                TVector<FString> out;
                out.PushBack(overrideValue->StringValue);
                return out;
            }
        }

        const auto* root  = mDocument.GetRoot();
        const auto* value = FindJsonValueByPath(root, path);
        return ReadStringArrayFromJson(value);
    }

    auto FConfigCollection::GetString(FStringView path) const -> FString {
        if (const auto* overrideValue = FindOverride(path)) {
            if (overrideValue->Type == EOverrideType::String) {
                return overrideValue->StringValue;
            }
            if (overrideValue->Type == EOverrideType::StringArray) {
                if (!overrideValue->StringArrayValue.IsEmpty()) {
                    return overrideValue->StringArrayValue[0];
                }
                return {};
            }
            if (overrideValue->Type == EOverrideType::Bool) {
                return overrideValue->BoolValue ? FString(TEXT("true")) : FString(TEXT("false"));
            }
            return {};
        }

        const auto* root  = mDocument.GetRoot();
        const auto* value = FindJsonValueByPath(root, path);
        if (value == nullptr) {
            return {};
        }

        FNativeString native;
        if (GetStringValue(value, native)) {
            return FromUtf8(native);
        }

        if (value->Type == EJsonType::Bool) {
            return value->Bool ? FString(TEXT("true")) : FString(TEXT("false"));
        }
        if (value->Type == EJsonType::Number) {
            return FString::ToString(value->Number);
        }
        return {};
    }

    auto FConfigCollection::GetInt32(FStringView path) const noexcept -> i32 {
        const auto value = GetInt64(path);
        if (value < TNumericProperty<i32>::Min || value > TNumericProperty<i32>::Max) {
            return 0;
        }
        return static_cast<i32>(value);
    }

    auto FConfigCollection::GetInt64(FStringView path) const noexcept -> i64 {
        if (const auto* overrideValue = FindOverride(path)) {
            if (overrideValue->Type == EOverrideType::Bool) {
                return overrideValue->BoolValue ? 1 : 0;
            }

            const auto parseFromString = [&](FStringView text) -> i64 {
                FNativeString native = ToNativeString(text);
                i64           out    = 0;
                if (ParseInt64Override(native.ToView(), out)) {
                    return out;
                }
                return 0;
            };

            if (overrideValue->Type == EOverrideType::String) {
                return parseFromString(overrideValue->StringValue.ToView());
            }
            if (overrideValue->Type == EOverrideType::StringArray) {
                if (!overrideValue->StringArrayValue.IsEmpty()) {
                    return parseFromString(overrideValue->StringArrayValue[0].ToView());
                }
                return 0;
            }
            return 0;
        }

        const auto* root  = mDocument.GetRoot();
        const auto* value = FindJsonValueByPath(root, path);
        if (value == nullptr) {
            return 0;
        }

        double number = 0.0;
        if (GetNumberValue(value, number)) {
            if (number < static_cast<double>(TNumericProperty<i64>::Min)
                || number > static_cast<double>(TNumericProperty<i64>::Max)) {
                return 0;
            }
            return static_cast<i64>(number);
        }

        if (value->Type == EJsonType::Bool) {
            return value->Bool ? 1 : 0;
        }
        if (value->Type == EJsonType::String) {
            i64 out = 0;
            if (ParseInt64Override(value->String.ToView(), out)) {
                return out;
            }
        }
        return 0;
    }

    auto FConfigCollection::GetFloat32(FStringView path) const noexcept -> f32 {
        return static_cast<f32>(GetFloat64(path));
    }

    auto FConfigCollection::GetFloat64(FStringView path) const noexcept -> f64 {
        if (const auto* overrideValue = FindOverride(path)) {
            if (overrideValue->Type == EOverrideType::Bool) {
                return overrideValue->BoolValue ? 1.0 : 0.0;
            }

            const auto parseFromString = [&](FStringView text) -> f64 {
                FNativeString native = ToNativeString(text);
                f64           out    = 0.0;
                if (ParseFloat64Override(native.ToView(), out)) {
                    return out;
                }
                return 0.0;
            };

            if (overrideValue->Type == EOverrideType::String) {
                return parseFromString(overrideValue->StringValue.ToView());
            }
            if (overrideValue->Type == EOverrideType::StringArray) {
                if (!overrideValue->StringArrayValue.IsEmpty()) {
                    return parseFromString(overrideValue->StringArrayValue[0].ToView());
                }
                return 0.0;
            }
            return 0.0;
        }

        const auto* root  = mDocument.GetRoot();
        const auto* value = FindJsonValueByPath(root, path);
        if (value == nullptr) {
            return 0.0;
        }

        double number = 0.0;
        if (GetNumberValue(value, number)) {
            return static_cast<f64>(number);
        }

        if (value->Type == EJsonType::Bool) {
            return value->Bool ? 1.0 : 0.0;
        }
        if (value->Type == EJsonType::String) {
            f64 out = 0.0;
            if (ParseFloat64Override(value->String.ToView(), out)) {
                return out;
            }
        }
        return 0.0;
    }

    auto FConfigCollection::GetUint32(FStringView path) const noexcept -> u32 {
        const auto value = GetUint64(path);
        if (value > TNumericProperty<u32>::Max) {
            return 0;
        }
        return static_cast<u32>(value);
    }

    auto FConfigCollection::GetUint64(FStringView path) const noexcept -> u64 {
        if (const auto* overrideValue = FindOverride(path)) {
            if (overrideValue->Type == EOverrideType::Bool) {
                return overrideValue->BoolValue ? 1u : 0u;
            }

            const auto parseFromString = [&](FStringView text) -> u64 {
                FNativeString native = ToNativeString(text);
                u64           out    = 0;
                if (ParseUint64Override(native.ToView(), out)) {
                    return out;
                }
                return 0;
            };

            if (overrideValue->Type == EOverrideType::String) {
                return parseFromString(overrideValue->StringValue.ToView());
            }
            if (overrideValue->Type == EOverrideType::StringArray) {
                if (!overrideValue->StringArrayValue.IsEmpty()) {
                    return parseFromString(overrideValue->StringArrayValue[0].ToView());
                }
                return 0;
            }
            return 0;
        }

        const auto* root  = mDocument.GetRoot();
        const auto* value = FindJsonValueByPath(root, path);
        if (value == nullptr) {
            return 0;
        }

        double number = 0.0;
        if (GetNumberValue(value, number)) {
            if (number < 0.0 || number > static_cast<double>(TNumericProperty<u64>::Max)) {
                return 0;
            }
            return static_cast<u64>(number);
        }

        if (value->Type == EJsonType::Bool) {
            return value->Bool ? 1u : 0u;
        }
        if (value->Type == EJsonType::String) {
            u64 out = 0;
            if (ParseUint64Override(value->String.ToView(), out)) {
                return out;
            }
        }
        return 0;
    }

    auto FConfigCollection::FindOverride(FStringView path) const -> const FOverrideValue* {
        if (path.Length() == 0) {
            return nullptr;
        }
        auto it = mOverrides.FindIt(FString(path));
        if (it == mOverrides.end()) {
            return nullptr;
        }
        return &it->second;
    }

    namespace {
        FConfigCollection gEngineConfig;
        bool              gEngineConfigInitialized = false;
    } // namespace

    auto GetGlobalConfig() noexcept -> const FConfigCollection& { return gEngineConfig; }

    void InitializeGlobalConfig(const FStartupParameters& startupParameters) {
        if (gEngineConfigInitialized) {
            return;
        }

        gEngineConfig.Clear();

        FNativeString jsonText;
        if (LoadDefaultConfigText(jsonText)) {
            if (!gEngineConfig.ParseJsonConfig(jsonText.ToView())) {
                LogWarningCat(TEXT("EngineConfig"), TEXT("Failed to parse DefaultGame.json."));
            }
        } else {
            LogWarningCat(
                TEXT("EngineConfig"), TEXT("DefaultGame.json not found under Assets/Config."));
        }

        gEngineConfig.ApplyStartupParamOverrides(startupParameters);
        gEngineConfigInitialized = true;
    }
} // namespace AltinaEngine::Core::Utility::EngineConfig
