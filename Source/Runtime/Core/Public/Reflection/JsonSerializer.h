#pragma once

#include "Reflection/Serializer.h"
#include "Container/String.h"
#include "Container/Vector.h"

namespace AltinaEngine::Core::Reflection {
    using Container::FNativeString;
    using Container::FNativeStringView;
    using Container::TVector;

    /**
     * @brief Simple JSON serializer using the Reflection serializer interface.
     *
     * This serializer emits a minimal JSON string. It relies on BeginObject/BeginArray
     * and WriteFieldName to build object structures.
     */
    class AE_CORE_API FJsonSerializer final : public ISerializer {
    public:
        FJsonSerializer()           = default;
        ~FJsonSerializer() override = default;

        [[nodiscard]] auto GetText() const -> FNativeStringView { return mText.ToView(); }
        [[nodiscard]] auto GetString() const -> const FNativeString& { return mText; }
        void               Clear();

        void               WriteInt8(i8 value) override { WriteNumber(value); }
        void               WriteInt16(i16 value) override { WriteNumber(value); }
        void               WriteInt32(i32 value) override { WriteNumber(value); }
        void               WriteInt64(i64 value) override { WriteNumber(value); }
        void               WriteUInt8(u8 value) override { WriteNumber(value); }
        void               WriteUInt16(u16 value) override { WriteNumber(value); }
        void               WriteUInt32(u32 value) override { WriteNumber(value); }
        void               WriteUInt64(u64 value) override { WriteNumber(value); }
        void               WriteFloat(f32 value) override { WriteNumber(value); }
        void               WriteDouble(f64 value) override { WriteNumber(value); }
        void               WriteBool(bool value) override;
        void               WriteString(FStringView value) override;

        void               BeginObject(FStringView name) override;
        void               EndObject() override;
        void               BeginArray(usize size) override;
        void               EndArray() override;
        void               WriteFieldName(FStringView name) override;

    protected:
        void WriteBytes(const void* data, usize size) override;

    private:
        enum class EScopeType : u8 {
            Object,
            Array
        };
        struct FScope {
            EScopeType Type       = EScopeType::Object;
            bool       First      = true;
            bool       AfterField = false;
        };

        void                       AppendChar(char c);
        void                       AppendLiteral(const char* text);
        void                       BeginValue();
        void                       BeginNamedValue(FStringView name);
        void                       EnsureRootArrayForAppend();
        void                       CloseRootArrayIfNeeded();
        void                       WriteQuotedString(const char* text, usize length);

        template <typename T> void WriteNumber(T value);

        FNativeString              mText;
        TVector<FScope>            mStack;
        bool                       mRootWritten     = false;
        bool                       mRootArrayActive = false;
    };

} // namespace AltinaEngine::Core::Reflection
