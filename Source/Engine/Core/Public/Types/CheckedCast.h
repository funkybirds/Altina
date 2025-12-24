#pragma once

#include "Concepts.h"

namespace AltinaEngine
{

    // CheckedCast: uses dynamic_cast when the dynamic conversion is available
    // (as detected by TTypeIsDynamicConvertible), otherwise falls back to
    // static_cast. This single template handles pointers, references and
    // value conversions.
    template <typename To, typename From>
    inline To CheckedCast(From&& from) noexcept
    {
        if constexpr (TTypeIsDynamicConvertible<To, From>::Value)
        {
            return dynamic_cast<To>(static_cast<From&&>(from));
        }
        else
        {
            return static_cast<To>(static_cast<From&&>(from));
        }
    }

} // namespace AltinaEngine
