#include "Utility/Json.h"

#include "Algorithm/CStringUtils.h"
#include "Types/Traits.h"

#include <cctype>
#include <cstdlib>
#include <cstring>

using AltinaEngine::Move;
namespace AltinaEngine::Core::Utility::Json {
    namespace {
        class FJsonReader {
        public:
            FJsonReader(FNativeStringView text, TVector<FJsonValue*>& owned, FNativeString& error)
                : mText(text), mOwned(owned), mError(error) {}

            auto Parse(FJsonValue& out) -> bool {
                ResetValue(out);
                SkipWhitespace();
                if (!ParseValue(out)) {
                    return false;
                }
                SkipWhitespace();
                if (!IsEnd()) {
                    SetError("Trailing characters after JSON.");
                    return false;
                }
                return true;
            }

        private:
            static void ResetValue(FJsonValue& value) {
                value.Type   = EJsonType::Null;
                value.Number = 0.0;
                value.Bool   = false;
                value.String.Clear();
                value.Array.Clear();
                value.Object.Clear();
            }

            [[nodiscard]] auto IsEnd() const noexcept -> bool { return mIndex >= mText.Length(); }

            [[nodiscard]] auto Peek() const noexcept -> char {
                return IsEnd() ? '\0' : mText.Data()[mIndex];
            }

            auto Get() noexcept -> char {
                if (IsEnd()) {
                    return '\0';
                }
                return mText.Data()[mIndex++];
            }

            void SkipWhitespace() noexcept {
                while (!IsEnd()) {
                    const char ch = Peek();
                    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r') {
                        ++mIndex;
                        continue;
                    }
                    break;
                }
            }

            void SetError(const char* message) {
                if (!mError.IsEmptyString()) {
                    return;
                }
                if (message != nullptr) {
                    mError.Append(message);
                }
            }

            [[nodiscard]] auto ParseValue(FJsonValue& out) -> bool {
                SkipWhitespace();
                const char ch = Peek();
                if (ch == '{') {
                    return ParseObject(out);
                }
                if (ch == '[') {
                    return ParseArray(out);
                }
                if (ch == '"') {
                    out.Type = EJsonType::String;
                    return ParseString(out.String);
                }
                if ((ch == '-') || (ch >= '0' && ch <= '9')) {
                    out.Type = EJsonType::Number;
                    return ParseNumber(out.Number);
                }
                if (MatchLiteral("true")) {
                    out.Type = EJsonType::Bool;
                    out.Bool = true;
                    return true;
                }
                if (MatchLiteral("false")) {
                    out.Type = EJsonType::Bool;
                    out.Bool = false;
                    return true;
                }
                if (MatchLiteral("null")) {
                    out.Type = EJsonType::Null;
                    return true;
                }

                SetError("Invalid JSON token.");
                return false;
            }

            auto ParseObject(FJsonValue& out) -> bool {
                if (Get() != '{') {
                    SetError("Expected '{'.");
                    return false;
                }

                out.Type = EJsonType::Object;
                SkipWhitespace();
                if (Peek() == '}') {
                    Get();
                    return true;
                }

                while (true) {
                    FJsonPair pair;
                    if (!ParseString(pair.Key)) {
                        return false;
                    }

                    SkipWhitespace();
                    if (Get() != ':') {
                        SetError("Expected ':' after object key.");
                        return false;
                    }

                    FJsonValue* value = CreateValue();
                    if (!ParseValue(*value)) {
                        return false;
                    }
                    pair.Value = value;
                    out.Object.PushBack(Move(pair));

                    SkipWhitespace();
                    if (Peek() == ',') {
                        Get();
                        SkipWhitespace();
                        continue;
                    }
                    if (Peek() == '}') {
                        Get();
                        return true;
                    }

                    SetError("Expected ',' or '}' in object.");
                    return false;
                }
            }

            auto ParseArray(FJsonValue& out) -> bool {
                if (Get() != '[') {
                    SetError("Expected '['.");
                    return false;
                }

                out.Type = EJsonType::Array;
                SkipWhitespace();
                if (Peek() == ']') {
                    Get();
                    return true;
                }

                while (true) {
                    FJsonValue* value = CreateValue();
                    if (!ParseValue(*value)) {
                        return false;
                    }
                    out.Array.PushBack(value);

                    SkipWhitespace();
                    if (Peek() == ',') {
                        Get();
                        SkipWhitespace();
                        continue;
                    }
                    if (Peek() == ']') {
                        Get();
                        return true;
                    }

                    SetError("Expected ',' or ']' in array.");
                    return false;
                }
            }

            auto ParseString(FNativeString& out) -> bool {
                if (Get() != '"') {
                    SetError("Expected '\"' to begin string.");
                    return false;
                }

                out.Clear();
                while (!IsEnd()) {
                    const char ch = Get();
                    if (ch == '"') {
                        return true;
                    }

                    if (ch == '\\') {
                        const char esc = Get();
                        switch (esc) {
                            case '"':
                                out.Append('"');
                                break;
                            case '\\':
                                out.Append('\\');
                                break;
                            case '/':
                                out.Append('/');
                                break;
                            case 'b':
                                out.Append('\b');
                                break;
                            case 'f':
                                out.Append('\f');
                                break;
                            case 'n':
                                out.Append('\n');
                                break;
                            case 'r':
                                out.Append('\r');
                                break;
                            case 't':
                                out.Append('\t');
                                break;
                            case 'u':
                                if (!ParseUnicodeEscape(out)) {
                                    return false;
                                }
                                break;
                            default:
                                SetError("Invalid escape sequence.");
                                return false;
                        }
                        continue;
                    }

                    out.Append(ch);
                }

                SetError("Unterminated string.");
                return false;
            }

            auto ParseUnicodeEscape(FNativeString& out) -> bool {
                u32 codepoint = 0;
                for (int i = 0; i < 4; ++i) {
                    if (IsEnd()) {
                        SetError("Unexpected end in unicode escape.");
                        return false;
                    }
                    const char ch = Get();
                    codepoint <<= 4;
                    if (ch >= '0' && ch <= '9') {
                        codepoint |= static_cast<u32>(ch - '0');
                    } else if (ch >= 'a' && ch <= 'f') {
                        codepoint |= static_cast<u32>(10 + (ch - 'a'));
                    } else if (ch >= 'A' && ch <= 'F') {
                        codepoint |= static_cast<u32>(10 + (ch - 'A'));
                    } else {
                        SetError("Invalid unicode escape.");
                        return false;
                    }
                }

                if (codepoint <= 0x7FU) {
                    out.Append(static_cast<char>(codepoint));
                } else {
                    out.Append('?');
                }
                return true;
            }

            auto ParseNumber(double& out) -> bool {
                const usize start = mIndex;
                if (Peek() == '-' || Peek() == '+') {
                    Get();
                }

                bool hasDigits = false;
                while (std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
                    hasDigits = true;
                    Get();
                }

                if (Peek() == '.') {
                    Get();
                    while (std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
                        hasDigits = true;
                        Get();
                    }
                }

                if (!hasDigits) {
                    SetError("Invalid number.");
                    return false;
                }

                if (Peek() == 'e' || Peek() == 'E') {
                    Get();
                    if (Peek() == '+' || Peek() == '-') {
                        Get();
                    }
                    bool hasExp = false;
                    while (std::isdigit(static_cast<unsigned char>(Peek())) != 0) {
                        hasExp = true;
                        Get();
                    }
                    if (!hasExp) {
                        SetError("Invalid exponent.");
                        return false;
                    }
                }

                const usize end = mIndex;
                FNativeString token;
                if (end > start) {
                    token.Append(mText.Data() + start, end - start);
                }
                const char* cstr  = token.CStr();
                char*       endPtr = nullptr;
                out                = std::strtod(cstr, &endPtr);
                if (endPtr == cstr) {
                    SetError("Invalid number.");
                    return false;
                }
                return true;
            }

            auto MatchLiteral(const char* literal) -> bool {
                if (literal == nullptr) {
                    return false;
                }
                const usize length = static_cast<usize>(std::strlen(literal));
                if ((mIndex + length) > mText.Length()) {
                    return false;
                }
                for (usize i = 0; i < length; ++i) {
                    if (mText.Data()[mIndex + i] != literal[i]) {
                        return false;
                    }
                }
                mIndex += length;
                return true;
            }

            auto CreateValue() -> FJsonValue* {
                auto* value = new FJsonValue();
                mOwned.PushBack(value);
                return value;
            }

        private:
            FNativeStringView     mText;
            usize                 mIndex = 0;
            TVector<FJsonValue*>& mOwned;
            FNativeString&        mError;
        };
    } // namespace

    FJsonDocument::~FJsonDocument() { DestroyValues(); }

    FJsonDocument::FJsonDocument(FJsonDocument&& other) noexcept
        : mRoot(other.mRoot), mOwned(Move(other.mOwned)), mError(Move(other.mError)) {
        other.mRoot = nullptr;
    }

    auto FJsonDocument::operator=(FJsonDocument&& other) noexcept -> FJsonDocument& {
        if (this != &other) {
            DestroyValues();
            mRoot       = other.mRoot;
            mOwned      = Move(other.mOwned);
            mError      = Move(other.mError);
            other.mRoot = nullptr;
        }
        return *this;
    }

    auto FJsonDocument::Parse(FNativeStringView text) -> bool {
        Clear();

        auto* root = new FJsonValue();
        mOwned.PushBack(root);

        FJsonReader reader(text, mOwned, mError);
        if (!reader.Parse(*root)) {
            DestroyValues();
            return false;
        }

        mRoot = root;
        return true;
    }

    void FJsonDocument::Clear() {
        DestroyValues();
        mError.Clear();
    }

    auto FJsonDocument::GetError() const noexcept -> FNativeStringView {
        return { mError.GetData(), mError.Length() };
    }

    void FJsonDocument::DestroyValues() {
        for (auto* value : mOwned) {
            delete value;
        }
        mOwned.Clear();
        mRoot = nullptr;
    }

    auto FindObjectValue(const FJsonValue& object, const char* key) -> const FJsonValue* {
        if (object.Type != EJsonType::Object) {
            return nullptr;
        }
        if (key == nullptr) {
            return nullptr;
        }
        for (const auto& pair : object.Object) {
            FNativeStringView keyView(pair.Key.GetData(), pair.Key.Length());
            if (keyView.Length() != std::strlen(key)) {
                continue;
            }
            bool match = true;
            for (usize i = 0; i < keyView.Length(); ++i) {
                if (keyView[i] != key[i]) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return pair.Value;
            }
        }
        return nullptr;
    }

    auto FindObjectValueInsensitive(const FJsonValue& object, const char* key)
        -> const FJsonValue* {
        if (object.Type != EJsonType::Object) {
            return nullptr;
        }
        if (key == nullptr) {
            return nullptr;
        }
        const usize keyLength = static_cast<usize>(std::strlen(key));
        for (const auto& pair : object.Object) {
            FNativeStringView keyView(pair.Key.GetData(), pair.Key.Length());
            if (keyView.Length() != keyLength) {
                continue;
            }
            bool match = true;
            for (usize i = 0; i < keyLength; ++i) {
                if (Core::Algorithm::ToLowerChar(keyView[i])
                    != Core::Algorithm::ToLowerChar(key[i])) {
                    match = false;
                    break;
                }
            }
            if (match) {
                return pair.Value;
            }
        }
        return nullptr;
    }

    auto GetStringValue(const FJsonValue* value, FNativeString& out) -> bool {
        if (value == nullptr || value->Type != EJsonType::String) {
            return false;
        }
        out = value->String;
        return true;
    }

    auto GetNumberValue(const FJsonValue* value, double& out) -> bool {
        if (value == nullptr || value->Type != EJsonType::Number) {
            return false;
        }
        out = value->Number;
        return true;
    }

    auto GetBoolValue(const FJsonValue* value, bool& out) -> bool {
        if (value == nullptr || value->Type != EJsonType::Bool) {
            return false;
        }
        out = value->Bool;
        return true;
    }

} // namespace AltinaEngine::Core::Utility::Json
