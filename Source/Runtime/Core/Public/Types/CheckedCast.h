#pragma once

#include "Concepts.h"

namespace AltinaEngine {

    // CheckedCast: uses dynamic_cast when the dynamic conversion is available
    // (as detected by TTypeIsDynamicConvertible), otherwise falls back to
    // static_cast. This single template handles pointers, references and
    // value conversions.
    template <typename To, typename From> auto CheckedCast(From&& from) noexcept -> To {
        if constexpr (CDynamicConvertible<From, To>) {
            return dynamic_cast<To>(static_cast<From&&>(from));
        } else {
            return static_cast<To>(static_cast<From&&>(from));
        }
    }

} // namespace AltinaEngine
