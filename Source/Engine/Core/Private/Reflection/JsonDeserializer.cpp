#include "Reflection/JsonDeserializer.h"

#include "Platform/Generic/GenericPlatformDecl.h"

namespace AltinaEngine::Core::Reflection {
    namespace {
        auto ToChar(TChar c) -> char { return (c <= 0x7f) ? static_cast<char>(c) : '?'; }
    } // namespace

    auto FJsonDeserializer::SetText(FNativeStringView text) -> bool {
        mError.Clear();
        mDocument.Clear();
        mRoot              = nullptr;
        mRootConsumed      = false;
        mForceUseRootValue = false;
        mImplicitRootArray = false;
        mRootArrayIndex    = 0;
        mStack.Clear();

        if (!mDocument.Parse(text)) {
            mError = FNativeString(mDocument.GetError());
            return false;
        }

        mRoot = mDocument.GetRoot();
        if (mRoot == nullptr) {
            mError = "Json: empty root";
            return false;
        }
        return true;
    }

    auto FJsonDeserializer::ReadBool() -> bool {
        const auto* value = NextValue();
        if (value == nullptr) {
            return false;
        }
        if (value->Type == Json::EJsonType::Bool) {
            return value->Bool;
        }
        if (value->Type == Json::EJsonType::Number) {
            return value->Number != 0.0;
        }
        return false;
    }

    void FJsonDeserializer::BeginObject() {
        mForceUseRootValue = true;
        const auto* value  = NextValue();
        mForceUseRootValue = false;
        if (value == nullptr || value->Type != Json::EJsonType::Object) {
            return;
        }
        mStack.PushBack({ EScopeType::Object, value, 0U, nullptr });
    }

    void FJsonDeserializer::EndObject() {
        if (!mStack.IsEmpty()) {
            mStack.PopBack();
        }
    }

    void FJsonDeserializer::BeginArray(usize& outSize) {
        outSize            = 0;
        mForceUseRootValue = true;
        const auto* value  = NextValue();
        mForceUseRootValue = false;
        if (value == nullptr || value->Type != Json::EJsonType::Array) {
            return;
        }
        outSize = value->Array.Size();
        mStack.PushBack({ EScopeType::Array, value, 0U, nullptr });
    }

    void FJsonDeserializer::EndArray() {
        if (!mStack.IsEmpty()) {
            mStack.PopBack();
        }
    }

    auto FJsonDeserializer::TryReadFieldName(FStringView expectedName) -> bool {
        if (mStack.IsEmpty()) {
            return false;
        }
        auto& scope = mStack.Back();
        if (scope.Type != EScopeType::Object || scope.Value == nullptr) {
            return false;
        }

        const auto  key   = ToNativeString(expectedName);
        const auto* value = Json::FindObjectValue(*scope.Value, key.CStr());
        if (value == nullptr) {
            return false;
        }
        scope.Pending = value;
        return true;
    }

    void FJsonDeserializer::ReadBytes(void* data, usize size) {
        if ((data == nullptr) || size == 0U) {
            return;
        }

        const auto* value = NextValue();
        if (value == nullptr || value->Type != Json::EJsonType::String) {
            Platform::Generic::Memset(data, 0, size);
            return;
        }

        const usize copySize = (value->String.Length() < size) ? value->String.Length() : size;
        Platform::Generic::Memcpy(data, value->String.GetData(), copySize);
        if (copySize < size) {
            Platform::Generic::Memset(static_cast<u8*>(data) + copySize, 0, size - copySize);
        }
    }

    const Json::FJsonValue* FJsonDeserializer::NextValue() {
        if (mStack.IsEmpty()) {
            if (mForceUseRootValue) {
                mRootConsumed = true;
                return mRoot;
            }

            if (mRoot == nullptr) {
                return nullptr;
            }

            if (mImplicitRootArray || (mRoot->Type == Json::EJsonType::Array && !mRootConsumed)) {
                mImplicitRootArray = true;
                if (mRootArrayIndex < mRoot->Array.Size()) {
                    mRootConsumed = true;
                    return mRoot->Array[mRootArrayIndex++];
                }
                return nullptr;
            }

            if (mRootConsumed) {
                return nullptr;
            }

            mRootConsumed = true;
            return mRoot;
        }

        auto& scope = mStack.Back();
        if (scope.Pending != nullptr) {
            const auto* value = scope.Pending;
            scope.Pending     = nullptr;
            return value;
        }

        if (scope.Type == EScopeType::Array) {
            if (scope.Value == nullptr || scope.Index >= scope.Value->Array.Size()) {
                return nullptr;
            }
            return scope.Value->Array[scope.Index++];
        }

        if (scope.Value == nullptr || scope.Index >= scope.Value->Object.Size()) {
            return nullptr;
        }
        return scope.Value->Object[scope.Index++].Value;
    }

    auto FJsonDeserializer::ToNativeString(FStringView text) -> FNativeString {
        if (text.Length() == 0U) {
            return {};
        }
        FNativeString out;
        out.Reserve(text.Length());
        for (usize i = 0U; i < text.Length(); ++i) {
            out.Append(ToChar(text.Data()[i]));
        }
        return out;
    }

    template <typename T> T FJsonDeserializer::ReadNumber() {
        const auto* value = NextValue();
        if (value == nullptr) {
            return static_cast<T>(0);
        }
        if (value->Type == Json::EJsonType::Number) {
            return static_cast<T>(value->Number);
        }
        if (value->Type == Json::EJsonType::Bool) {
            return static_cast<T>(value->Bool ? 1 : 0);
        }
        return static_cast<T>(0);
    }

    template i8  FJsonDeserializer::ReadNumber<i8>();
    template i16 FJsonDeserializer::ReadNumber<i16>();
    template i32 FJsonDeserializer::ReadNumber<i32>();
    template i64 FJsonDeserializer::ReadNumber<i64>();
    template u8  FJsonDeserializer::ReadNumber<u8>();
    template u16 FJsonDeserializer::ReadNumber<u16>();
    template u32 FJsonDeserializer::ReadNumber<u32>();
    template u64 FJsonDeserializer::ReadNumber<u64>();
    template f32 FJsonDeserializer::ReadNumber<f32>();
    template f64 FJsonDeserializer::ReadNumber<f64>();

} // namespace AltinaEngine::Core::Reflection
