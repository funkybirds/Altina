#pragma once

#include "Reflection/Serializer.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"
#include "Utility/Json.h"

namespace AltinaEngine::Core::Reflection {
    using Container::FNativeString;
    using Container::FNativeStringView;
    using Container::TVector;
    namespace Json = Core::Utility::Json;

    /**
     * @brief JSON deserializer backed by Utility::Json.
     *
     * Supports object/array traversal with field lookups and ordered reads.
     */
    class AE_CORE_API FJsonDeserializer final : public IDeserializer {
    public:
        FJsonDeserializer() = default;
        explicit FJsonDeserializer(FNativeStringView text) { (void)SetText(text); }
        ~FJsonDeserializer() override = default;

        [[nodiscard]] auto SetText(FNativeStringView text) -> bool;
        [[nodiscard]] auto GetError() const noexcept -> FNativeStringView {
            return mError.ToView();
        }

        auto ReadInt8() -> i8 override { return ReadNumber<i8>(); }
        auto ReadInt16() -> i16 override { return ReadNumber<i16>(); }
        auto ReadInt32() -> i32 override { return ReadNumber<i32>(); }
        auto ReadInt64() -> i64 override { return ReadNumber<i64>(); }
        auto ReadUInt8() -> u8 override { return ReadNumber<u8>(); }
        auto ReadUInt16() -> u16 override { return ReadNumber<u16>(); }
        auto ReadUInt32() -> u32 override { return ReadNumber<u32>(); }
        auto ReadUInt64() -> u64 override { return ReadNumber<u64>(); }
        auto ReadFloat() -> f32 override { return ReadNumber<f32>(); }
        auto ReadDouble() -> f64 override { return ReadNumber<f64>(); }
        auto ReadBool() -> bool override;

        void BeginObject() override;
        void EndObject() override;
        void BeginArray(usize& outSize) override;
        void EndArray() override;
        auto TryReadFieldName(FStringView expectedName) -> bool override;

    protected:
        void ReadBytes(void* data, usize size) override;

    private:
        enum class EScopeType : u8 {
            Object,
            Array
        };
        struct FScope {
            EScopeType              Type    = EScopeType::Object;
            const Json::FJsonValue* Value   = nullptr;
            usize                   Index   = 0;
            const Json::FJsonValue* Pending = nullptr;
        };

        const Json::FJsonValue* NextValue();
        static auto             ToNativeString(FStringView text) -> FNativeString;

        template <typename T> T ReadNumber();

        Json::FJsonDocument     mDocument;
        const Json::FJsonValue* mRoot              = nullptr;
        bool                    mRootConsumed      = false;
        bool                    mForceUseRootValue = false;
        bool                    mImplicitRootArray = false;
        usize                   mRootArrayIndex    = 0;
        TVector<FScope>         mStack;
        FNativeString           mError;
    };

} // namespace AltinaEngine::Core::Reflection
