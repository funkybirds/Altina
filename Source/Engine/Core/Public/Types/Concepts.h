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

    template <typename T, typename U>
    concept ISameSizeAs = sizeof(T) == sizeof(U);

} // namespace AltinaEngine
