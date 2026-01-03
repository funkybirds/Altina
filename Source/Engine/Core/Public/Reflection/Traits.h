#pragma once
#include "Types/Concepts.h"

namespace AltinaEngine::Core::Reflection {
    class FArchive;
    class ISerializer;
    class IDeserializer;

    template <typename T> class TCustomSerializeRule;

    template <typename T>
    concept CCustomInternalSerializable = requires(T t) {
        { t.Serialize(Declval<FArchive&>()) } -> CSameAs<void>;
        { T::Deserialize(Declval<FArchive&>()) } -> CSameAs<T>;
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

} // namespace AltinaEngine::Core::Reflection