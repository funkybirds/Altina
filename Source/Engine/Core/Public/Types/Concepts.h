#pragma once

#include "Aliases.h"
#include "Traits.h"

namespace AltinaEngine
{

    template <typename T>
    concept IScalar = TTypeIsIntegral<T>::Value || TTypeIsFloatingPoint<T>::Value;

    template <typename T>
    concept IIntegral = TTypeIsIntegral<T>::Value;

    template <typename T>
    concept IFloatingPoint = TTypeIsFloatingPoint<T>::Value;

    template <typename T>
    concept ISignedIntegral = TTypeIsIntegral<T>::Value && TTypeIsSigned<T>::Value;

    template <typename T, typename U>
    concept ISameSizeAs = sizeof(T) == sizeof(U);

    template <typename T, typename... Ts>
    concept ISameAsAll = (sizeof...(Ts) == 0) || (TTypeSameAs<T, Ts>::Value && ...);

    template <typename T>
    concept IRandomReadable = TTypeIsRandomReadable<T>::Value;

    template <typename T>
    concept IRandomWritable = TTypeIsRandomWritable<T>::Value;

    template <typename It>
    concept IReadableIterator = TTypeIsReadableIterator<It>::Value;

    template <typename It>
    concept IWritableIterator = TTypeIsWritableIterator<It>::Value;

    template <typename It>
    concept IRandomAccessIterator = TTypeIsRandomAccessIterator<It>::Value;

} // namespace AltinaEngine
