#pragma once

#include "Aliases.h"
#include "Traits.h"

namespace AltinaEngine {
    template <typename T>
    concept CNumericPropertySupported = CIntegral<T> || CFloatingPoint<T> || CEnum<T>;

    namespace Detail::NumericProperty {
        template <CIntegral T> [[nodiscard]] consteval auto IntegralMax() -> T {
            constexpr usize kBits = sizeof(T) * 8U;
            if constexpr (TTypeIsSigned<T>::Value) {
                if constexpr (kBits == 64U) {
                    return static_cast<T>(0x7FFFFFFFFFFFFFFFULL);
                } else {
                    return static_cast<T>((1ULL << (kBits - 1U)) - 1ULL);
                }
            } else {
                if constexpr (kBits == 64U) {
                    return static_cast<T>(0xFFFFFFFFFFFFFFFFULL);
                } else {
                    return static_cast<T>((1ULL << kBits) - 1ULL);
                }
            }
        }

        template <CIntegral T> [[nodiscard]] consteval auto IntegralMin() -> T {
            if constexpr (TTypeIsSigned<T>::Value) {
                return static_cast<T>(-static_cast<i64>(IntegralMax<T>()) - 1LL);
            } else {
                return static_cast<T>(0);
            }
        }
    } // namespace Detail::NumericProperty

    template <CNumericPropertySupported T> struct TNumericProperty {
        [[nodiscard]] static consteval auto PositiveMaxValue() -> T {
            if constexpr (CEnum<T>) {
                using TBase = TUnderlyingType<T>;
                return static_cast<T>(TNumericProperty<TBase>::PositiveMaxValue());
            } else if constexpr (CIntegral<T>) {
                return Detail::NumericProperty::IntegralMax<T>();
            }
        }

        [[nodiscard]] static consteval auto PositiveMinValue() -> T {
            if constexpr (CEnum<T>) {
                using TBase = TUnderlyingType<T>;
                return static_cast<T>(TNumericProperty<TBase>::PositiveMinValue());
            } else if constexpr (CIntegral<T>) {
                if constexpr (CSameAs<T, bool>) {
                    return true;
                } else {
                    return static_cast<T>(1);
                }
            }
        }

        [[nodiscard]] static consteval auto MaxValue() -> T { return PositiveMaxValue(); }

        [[nodiscard]] static consteval auto MinValue() -> T {
            if constexpr (CEnum<T>) {
                using TBase = TUnderlyingType<T>;
                return static_cast<T>(TNumericProperty<TBase>::MinValue());
            } else if constexpr (CIntegral<T>) {
                return Detail::NumericProperty::IntegralMin<T>();
            }
        }

        [[nodiscard]] static consteval auto EpsValue() -> T {
            if constexpr (CEnum<T>) {
                return static_cast<T>(0);
            } else if constexpr (CSameAs<T, bool>) {
                return false;
            } else if constexpr (CIntegral<T>) {
                return static_cast<T>(1);
            }
        }

        static constexpr T PositiveMax = PositiveMaxValue();
        static constexpr T PositiveMin = PositiveMinValue();
        static constexpr T Max         = MaxValue();
        static constexpr T Min         = MinValue();
        static constexpr T Eps         = EpsValue();
    };

    template <> struct TNumericProperty<f32> {
        static constexpr f32                PositiveMax = 3.4028234e+38F;
        static constexpr f32                PositiveMin = 1.1754944e-38F;
        static constexpr f32                Max         = PositiveMax;
        static constexpr f32                Min         = -PositiveMax;
        static constexpr f32                Eps         = 1.1920929e-07F;

        [[nodiscard]] static consteval auto PositiveMaxValue() -> f32 { return PositiveMax; }
        [[nodiscard]] static consteval auto PositiveMinValue() -> f32 { return PositiveMin; }
        [[nodiscard]] static consteval auto MaxValue() -> f32 { return Max; }
        [[nodiscard]] static consteval auto MinValue() -> f32 { return Min; }
        [[nodiscard]] static consteval auto EpsValue() -> f32 { return Eps; }
    };

    template <> struct TNumericProperty<f64> {
        static constexpr f64                PositiveMax = 1.7976931348623157e+308;
        static constexpr f64                PositiveMin = 2.2250738585072014e-308;
        static constexpr f64                Max         = PositiveMax;
        static constexpr f64                Min         = -PositiveMax;
        static constexpr f64                Eps         = 2.2204460492503131e-16;

        [[nodiscard]] static consteval auto PositiveMaxValue() -> f64 { return PositiveMax; }
        [[nodiscard]] static consteval auto PositiveMinValue() -> f64 { return PositiveMin; }
        [[nodiscard]] static consteval auto MaxValue() -> f64 { return Max; }
        [[nodiscard]] static consteval auto MinValue() -> f64 { return Min; }
        [[nodiscard]] static consteval auto EpsValue() -> f64 { return Eps; }
    };
} // namespace AltinaEngine
