#pragma once
#include "Types/Concepts.h"
#include <type_traits>

namespace AltinaEngine::Core::Reflection {
    class FArchive;
    class ISerializer;
    class IDeserializer;

    template <typename T> class TCustomSerializeRule;

    template <typename T>
    concept CCustomInternalSerializable =
        requires(T t, const T ct, ISerializer& serializer, IDeserializer& deserializer) {
            { ct.Serialize(serializer) } -> CSameAs<void>;
            { T::Deserialize(deserializer) } -> CSameAs<T>;
        };

    template <typename T>
    concept CCustomExternalSerializable =
        requires(const T t, ISerializer& serializer, IDeserializer& deserializer) {
            { TCustomSerializeRule<T>::Serialize(t, serializer) } -> CSameAs<void>;
            { TCustomSerializeRule<T>::Deserialize(deserializer) } -> CSameAs<T>;
        };

    template <typename T>
    concept CCustomSerializable = CCustomInternalSerializable<T> || CCustomExternalSerializable<T>;

    template <typename T>
    concept CTriviallySerializable = CScalar<T> || CEnum<T>;

    template <typename T>
    concept CStaticSerializable = CCustomSerializable<T> || CTriviallySerializable<T>;

    template <typename T>
    concept CPointer = std::is_pointer_v<T>;

    // Forward declaration for SerializeInvoker
    template <typename T>
        requires CDefinedType<T> && (!CPointer<T>)
    void SerializeInvoker(T&, ISerializer&);

    // Forward declaration for DeserializeInvoker
    template <typename T>
        requires CDefinedType<T> && (!CPointer<T>)
    auto DeserializeInvoker(IDeserializer&) -> T;

    template <typename T>
    concept CSerializable = requires(T t, ISerializer& serializer) {
        { SerializeInvoker<T>(t, serializer) } -> CSameAs<void>;
    };

    template <typename T>
    concept CDeserializable = requires(IDeserializer& deserializer) {
        { DeserializeInvoker<T>(deserializer) } -> CSameAs<T>;
    };

} // namespace AltinaEngine::Core::Reflection