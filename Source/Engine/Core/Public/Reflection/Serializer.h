#pragma once

#include "Traits.h"
#include "Container/String.h"
#include "Types/NonCopyable.h"

namespace AltinaEngine::Core::Reflection {
    class FArchive;
    class FObject;

    class AE_CORE_API ISerializer : public FNonCopyableClass {
    public:
        ISerializer()           = default;
        ~ISerializer() override = default;

        ISerializer(const ISerializer&)                            = delete;
        auto         operator=(const ISerializer&) -> ISerializer& = delete;

        virtual void WriteInt8(i8 value) { WriteBytes(&value, sizeof(value)); }
        virtual void WriteInt16(i16 value) { WriteBytes(&value, sizeof(value)); }
        virtual void WriteInt32(i32 value) { WriteBytes(&value, sizeof(value)); }
        virtual void WriteInt64(i64 value) { WriteBytes(&value, sizeof(value)); }
        virtual void WriteUInt8(u8 value) { WriteBytes(&value, sizeof(value)); }
        virtual void WriteUInt16(u16 value) { WriteBytes(&value, sizeof(value)); }
        virtual void WriteUInt32(u32 value) { WriteBytes(&value, sizeof(value)); }
        virtual void WriteUInt64(u64 value) { WriteBytes(&value, sizeof(value)); }
        virtual void WriteFloat(f32 value) { WriteBytes(&value, sizeof(value)); }
        virtual void WriteDouble(f64 value) { WriteBytes(&value, sizeof(value)); }
        virtual void WriteBool(bool value) { WriteBytes(&value, sizeof(value)); }
        virtual void WriteString(Container::FStringView value) {
            WriteBytes(value.Data(), value.Length());
        }

        virtual void              BeginObject(Container::FStringView /*name*/) {}
        virtual void              EndObject() {}
        virtual void              BeginArray(usize /*size*/) {}
        virtual void              EndArray() {}
        virtual void              WriteFieldName(Container::FStringView /*name*/) {}

        template <CScalar T> void Write(const T& value) {
            if constexpr (CSameAs<T, i8>)
                WriteInt8(value);
            else if constexpr (CSameAs<T, i16>)
                WriteInt16(value);
            else if constexpr (CSameAs<T, i32>)
                WriteInt32(value);
            else if constexpr (CSameAs<T, i64>)
                WriteInt64(value);
            else if constexpr (CSameAs<T, u8>)
                WriteUInt8(value);
            else if constexpr (CSameAs<T, u16>)
                WriteUInt16(value);
            else if constexpr (CSameAs<T, u32>)
                WriteUInt32(value);
            else if constexpr (CSameAs<T, u64>)
                WriteUInt64(value);
            else if constexpr (CSameAs<T, f32>)
                WriteFloat(value);
            else if constexpr (CSameAs<T, f64>)
                WriteDouble(value);
            else if constexpr (CSameAs<T, bool>)
                WriteBool(value);
            else
                WriteBytes(&value, sizeof(T));
        }

        template <CEnum T> void Write(const T& value) {
            using TUnderlyingType = TUnderlyingType<T>;
            Write(static_cast<TUnderlyingType>(value));
        }

        template <CCustomExternalSerializable T> void Write(const T& value) {
            TCustomSerializeRule<T>::Serialize(value, *this);
        }

        template <typename T> void Serialize(const T& value) { Write(value); }
        void                       WriteSize(usize size) { Write(static_cast<u64>(size)); }

    protected:
        virtual void WriteBytes(const void* data, usize size) = 0;
    };

    class AE_CORE_API IDeserializer : public FNonCopyableClass {
    public:
        IDeserializer()           = default;
        ~IDeserializer() override = default;

        IDeserializer(const IDeserializer&)                            = delete;
        auto         operator=(const IDeserializer&) -> IDeserializer& = delete;

        virtual auto ReadInt8() -> i8 {
            i8 v = 0;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual auto ReadInt16() -> i16 {
            i16 v;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual auto ReadInt32() -> i32 {
            i32 v = 0;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual auto ReadInt64() -> i64 {
            i64 v = 0;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual auto ReadUInt8() -> u8 {
            u8 v = 0;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual auto ReadUInt16() -> u16 {
            u16 v = 0;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual auto ReadUInt32() -> u32 {
            u32 v = 0;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual auto ReadUInt64() -> u64 {
            u64 v = 0;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual auto ReadFloat() -> f32 {
            f32 v = NAN;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual auto ReadDouble() -> f64 {
            f64 v = NAN;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual auto ReadBool() -> bool {
            bool v = false;
            ReadBytes(&v, sizeof(v));
            return v;
        }

        virtual void BeginObject() {}
        virtual void EndObject() {}
        virtual void BeginArray(usize& outSize) { outSize = 0; }
        virtual void EndArray() {}
        virtual auto TryReadFieldName(Container::FStringView /*expectedName*/) -> bool {
            return true;
        }

        template <CScalar T> auto Read() -> T {
            if constexpr (CSameAs<T, i8>)
                return ReadInt8();
            else if constexpr (CSameAs<T, i16>)
                return ReadInt16();
            else if constexpr (CSameAs<T, i32>)
                return ReadInt32();
            else if constexpr (CSameAs<T, i64>)
                return ReadInt64();
            else if constexpr (CSameAs<T, u8>)
                return ReadUInt8();
            else if constexpr (CSameAs<T, u16>)
                return ReadUInt16();
            else if constexpr (CSameAs<T, u32>)
                return ReadUInt32();
            else if constexpr (CSameAs<T, u64>)
                return ReadUInt64();
            else if constexpr (CSameAs<T, f32>)
                return ReadFloat();
            else if constexpr (CSameAs<T, f64>)
                return ReadDouble();
            else if constexpr (CSameAs<T, bool>)
                return ReadBool();
            else {
                T value;
                ReadBytes(&value, sizeof(T));
                return value;
            }
        }

        template <CEnum T> auto Read() -> T {
            using TUnderlyingType = TUnderlyingType<T>;
            return static_cast<T>(Read<TUnderlyingType>());
        }

        template <CCustomExternalSerializable T> auto Read() -> T {
            return TCustomSerializeRule<T>::Deserialize(*this);
        }

        template <typename T> auto Deserialize() -> T { return Read<T>(); }
        auto                       ReadSize() -> usize { return static_cast<usize>(Read<u64>()); }

    protected:
        virtual void ReadBytes(void* data, usize size) = 0;
    };

} // namespace AltinaEngine::Core::Reflection