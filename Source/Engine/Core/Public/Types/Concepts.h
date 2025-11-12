#pragma once

#include "Aliases.h"
#include "Traits.h"

namespace AltinaEngine
{

    template <typename T>
    concept IScalar = TTypeIsIntegral<T>::Value || TTypeIsFloatingPoint<T>::Value;

} // namespace AltinaEngine
