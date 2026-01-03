#pragma once

namespace AltinaEngine {
    template <bool TValue> struct TBoolConstant {
        static constexpr bool Value = TValue; // NOLINT
    };

    using TTrueType  = TBoolConstant<true>;
    using TFalseType = TBoolConstant<false>;

    template <typename T> auto Declval() noexcept -> T&& {
        static_assert(false, "Declval can only be used in unevaluated contexts");
    }
} // namespace AltinaEngine