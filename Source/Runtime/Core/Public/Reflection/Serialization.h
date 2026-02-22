//
// Created by funky on 2026/1/3.
//

#ifndef ALTINAENGINE_SERIALIZATION_H
#define ALTINAENGINE_SERIALIZATION_H

#include "Traits.h"
#include "Serializer.h"
#include "ReflectionBase.h"
#include "Types/Meta.h"

namespace AltinaEngine::Core::Reflection {
    using namespace TypeMeta;

    // Forward declarations
    namespace Detail {
        AE_CORE_API void DynamicSerializeInvokerImpl(void* ptr, ISerializer& serializer, u64 hash);
        AE_CORE_API void DynamicDeserializeInvokerImpl(
            void* ptr, IDeserializer& deserializer, u64 hash);
    } // namespace Detail

    // SerializeInvoker template with requirements
    template <typename T>
        requires CDefinedType<T> && (!CPointer<T>)
    void SerializeInvoker(T& t, ISerializer& serializer) {
        if constexpr (CTriviallySerializable<T>) {
            serializer.Write(t);
        } else if constexpr (CCustomInternalSerializable<T>) {
            t.Serialize(serializer);
        } else if constexpr (CCustomExternalSerializable<T>) {
            TCustomSerializeRule<T>::Serialize(t, serializer);
        } else {
            Detail::DynamicSerializeInvokerImpl(
                &t, serializer, FMetaTypeInfo::Create<T>().GetHash());
        }
    }

    // DeserializeInvokerImpl template with requirements
    template <typename T>
        requires CDefinedType<T> && (!CPointer<T>)
    void DeserializeInvokerImpl(T* t, IDeserializer& deserializer) {
        if constexpr (CTriviallySerializable<T>) {
            *t = deserializer.Read<T>();
        } else if constexpr (CCustomInternalSerializable<T>) {
            *t = T::Deserialize(deserializer);
        } else if constexpr (CCustomExternalSerializable<T>) {
            *t = TCustomSerializeRule<T>::Deserialize(deserializer);
        } else {
            Detail::DynamicDeserializeInvokerImpl(
                t, deserializer, FMetaTypeInfo::Create<T>().GetHash());
        }
    }

    // DeserializeInvoker wrapper template
    template <typename T>
        requires CDefinedType<T> && (!CPointer<T>)
    auto DeserializeInvoker(IDeserializer& deserializer) -> T {
        T result;
        DeserializeInvokerImpl(&result, deserializer);
        return result;
    }

} // namespace AltinaEngine::Core::Reflection

#endif // ALTINAENGINE_SERIALIZATION_H
