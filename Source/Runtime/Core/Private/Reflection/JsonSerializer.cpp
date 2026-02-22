#include "Reflection/JsonSerializer.h"

#include "Platform/Generic/GenericPlatformDecl.h"

namespace AltinaEngine::Core::Reflection {
    namespace {
        auto ToChar(TChar c) -> char { return (c <= 0x7f) ? static_cast<char>(c) : '?'; }
    } // namespace

    void FJsonSerializer::Clear() {
        mText.Clear();
        mStack.Clear();
        mRootWritten = false;
    }

    void FJsonSerializer::WriteBool(bool value) {
        BeginValue();
        AppendLiteral(value ? "true" : "false");
        CloseRootArrayIfNeeded();
    }

    void FJsonSerializer::WriteString(FStringView value) {
        BeginValue();
        if (value.Length() == 0U) {
            AppendLiteral("\"\"");
            CloseRootArrayIfNeeded();
            return;
        }

        TVector<char> buffer;
        buffer.Resize(value.Length());
        for (usize i = 0U; i < value.Length(); ++i) {
            buffer[i] = ToChar(value.Data()[i]);
        }
        WriteQuotedString(buffer.Data(), buffer.Size());
        CloseRootArrayIfNeeded();
    }

    void FJsonSerializer::BeginObject(FStringView name) {
        if (!name.IsEmpty()) {
            BeginNamedValue(name);
        } else {
            BeginValue();
        }
        AppendChar('{');
        mStack.PushBack({ EScopeType::Object, true, false });
    }

    void FJsonSerializer::EndObject() {
        AppendChar('}');
        if (!mStack.IsEmpty()) {
            mStack.PopBack();
        }
        CloseRootArrayIfNeeded();
    }

    void FJsonSerializer::BeginArray(usize /*size*/) {
        BeginValue();
        AppendChar('[');
        mStack.PushBack({ EScopeType::Array, true, false });
    }

    void FJsonSerializer::EndArray() {
        AppendChar(']');
        if (!mStack.IsEmpty()) {
            mStack.PopBack();
        }
        CloseRootArrayIfNeeded();
    }

    void FJsonSerializer::WriteFieldName(FStringView name) {
        if (mStack.IsEmpty() || mStack.Back().Type != EScopeType::Object) {
            return;
        }

        auto& scope = mStack.Back();
        if (!scope.First) {
            AppendChar(',');
        }
        scope.First      = false;
        scope.AfterField = true;

        TVector<char> buffer;
        buffer.Resize(name.Length());
        for (usize i = 0U; i < name.Length(); ++i) {
            buffer[i] = ToChar(name.Data()[i]);
        }
        WriteQuotedString(buffer.Data(), buffer.Size());
        AppendChar(':');
    }

    void FJsonSerializer::WriteBytes(const void* data, usize size) {
        if ((data == nullptr) || size == 0U) {
            return;
        }
        const auto* bytes = static_cast<const char*>(data);
        BeginValue();
        WriteQuotedString(bytes, size);
        CloseRootArrayIfNeeded();
    }

    void FJsonSerializer::AppendChar(char c) { mText.Append(c); }

    void FJsonSerializer::AppendLiteral(const char* text) {
        if (text == nullptr) {
            return;
        }
        mText.Append(text);
    }

    void FJsonSerializer::BeginValue() {
        if (mStack.IsEmpty()) {
            if (mRootWritten) {
                EnsureRootArrayForAppend();
                return;
            }
            mRootWritten = true;
            return;
        }

        auto& scope = mStack.Back();
        if (scope.Type == EScopeType::Object) {
            if (scope.AfterField) {
                scope.AfterField = false;
                return;
            }
            if (!scope.First) {
                AppendChar(',');
            }
            scope.First = false;
            return;
        }

        if (!scope.First) {
            AppendChar(',');
        }
        scope.First = false;
    }

    void FJsonSerializer::BeginNamedValue(FStringView name) {
        if (mStack.IsEmpty() || mStack.Back().Type != EScopeType::Object) {
            BeginValue();
            return;
        }

        auto& scope = mStack.Back();
        if (!scope.First) {
            AppendChar(',');
        }
        scope.First      = false;
        scope.AfterField = true;

        TVector<char> buffer;
        buffer.Resize(name.Length());
        for (usize i = 0U; i < name.Length(); ++i) {
            buffer[i] = ToChar(name.Data()[i]);
        }
        WriteQuotedString(buffer.Data(), buffer.Size());
        AppendChar(':');
    }

    void FJsonSerializer::EnsureRootArrayForAppend() {
        if (!mRootArrayActive) {
            FNativeString wrapped;
            wrapped.Append('[');
            wrapped.Append(mText.ToView());
            wrapped.Append(']');
            mText            = Move(wrapped);
            mRootArrayActive = true;
        }

        if (mText.Length() > 0U && mText.GetData()[mText.Length() - 1U] == ']') {
            mText.PopBack();
        }

        AppendChar(',');
    }

    void FJsonSerializer::CloseRootArrayIfNeeded() {
        if (!mRootArrayActive || !mStack.IsEmpty()) {
            return;
        }
        if (mText.Length() == 0U || mText.GetData()[mText.Length() - 1U] != ']') {
            AppendChar(']');
        }
    }

    void FJsonSerializer::WriteQuotedString(const char* text, usize length) {
        AppendChar('"');
        for (usize i = 0U; i < length; ++i) {
            const char c = text[i];
            switch (c) {
                case '\\':
                    AppendLiteral("\\\\");
                    break;
                case '"':
                    AppendLiteral("\\\"");
                    break;
                case '\n':
                    AppendLiteral("\\n");
                    break;
                case '\r':
                    AppendLiteral("\\r");
                    break;
                case '\t':
                    AppendLiteral("\\t");
                    break;
                case '\b':
                    AppendLiteral("\\b");
                    break;
                case '\f':
                    AppendLiteral("\\f");
                    break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char                buf[7] = "\\u00";
                        const unsigned char v      = static_cast<unsigned char>(c);
                        const char          hex[]  = "0123456789ABCDEF";
                        buf[4]                     = hex[(v >> 4) & 0xF];
                        buf[5]                     = hex[v & 0xF];
                        buf[6]                     = '\0';
                        AppendLiteral(buf);
                    } else {
                        AppendChar(c);
                    }
                    break;
            }
        }
        AppendChar('"');
    }

    template <typename T> void FJsonSerializer::WriteNumber(T value) {
        BeginValue();
        auto text = FNativeString::ToString(value);
        AppendLiteral(text.CStr());
        CloseRootArrayIfNeeded();
    }

    template void FJsonSerializer::WriteNumber<i8>(i8 value);
    template void FJsonSerializer::WriteNumber<i16>(i16 value);
    template void FJsonSerializer::WriteNumber<i32>(i32 value);
    template void FJsonSerializer::WriteNumber<i64>(i64 value);
    template void FJsonSerializer::WriteNumber<u8>(u8 value);
    template void FJsonSerializer::WriteNumber<u16>(u16 value);
    template void FJsonSerializer::WriteNumber<u32>(u32 value);
    template void FJsonSerializer::WriteNumber<u64>(u64 value);
    template void FJsonSerializer::WriteNumber<f32>(f32 value);
    template void FJsonSerializer::WriteNumber<f64>(f64 value);

} // namespace AltinaEngine::Core::Reflection
