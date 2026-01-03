#pragma once
#include "Traits.h"
#include "Types/Concepts.h"
#include "Container/String.h"

namespace AltinaEngine::Core::Reflection {
    class FArchive;

    class ISerializer {
    public:
        ISerializer()          = default;
        virtual ~ISerializer() = default;

        ISerializer(const ISerializer&)            = delete;
        ISerializer& operator=(const ISerializer&) = delete;

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

        virtual void              BeginObject(Container::FStringView name = {}) {}
        virtual void              EndObject() {}
        virtual void              BeginArray(usize size) {}
        virtual void              EndArray() {}
        virtual void              WriteFieldName(Container::FStringView name) {}

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
            using UnderlyingType = TUnderlyingType<T>;
            Write(static_cast<UnderlyingType>(value));
        }

        template <CCustomExternalSerializable T> void Write(const T& value) {
            TCustomSerializeRule<T>::Serialize(value, *this);
        }

        template <typename T> void Serialize(const T& value) { Write(value); }
        void                       WriteSize(usize size) { Write(static_cast<u64>(size)); }

    protected:
        virtual void WriteBytes(const void* data, usize size) = 0;
    };

    class IDeserializer {
    public:
        IDeserializer()          = default;
        virtual ~IDeserializer() = default;

        IDeserializer(const IDeserializer&)            = delete;
        IDeserializer& operator=(const IDeserializer&) = delete;

        // 基础类型的语义化读取接口（文本格式可重写）
        virtual i8     ReadInt8() {
            i8 v;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual i16 ReadInt16() {
            i16 v;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual i32 ReadInt32() {
            i32 v;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual i64 ReadInt64() {
            i64 v;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual u8 ReadUInt8() {
            u8 v;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual u16 ReadUInt16() {
            u16 v;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual u32 ReadUInt32() {
            u32 v;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual u64 ReadUInt64() {
            u64 v;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual f32 ReadFloat() {
            f32 v;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual f64 ReadDouble() {
            f64 v;
            ReadBytes(&v, sizeof(v));
            return v;
        }
        virtual bool ReadBool() {
            bool v;
            ReadBytes(&v, sizeof(v));
            return v;
        }

        virtual void BeginObject() {}
        virtual void EndObject() {}
        virtual void BeginArray(usize& outSize) { outSize = 0; }
        virtual void EndArray() {}
        virtual bool TryReadFieldName(Container::FStringView expectedName) { return true; }

        template <CScalar T> T Read() {
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

        template <CEnum T> T Read() {
            using UnderlyingType = TUnderlyingType<T>;
            return static_cast<T>(Read<UnderlyingType>());
        }

        template <CCustomExternalSerializable T> T Read() {
            return TCustomSerializeRule<T>::Deserialize(*this);
        }

        template <typename T> T Deserialize() { return Read<T>(); }

        usize                   ReadSize() { return static_cast<usize>(Read<u64>()); }

    protected:
        virtual void ReadBytes(void* data, usize size) = 0;
    };

} // namespace AltinaEngine::Core::Reflection