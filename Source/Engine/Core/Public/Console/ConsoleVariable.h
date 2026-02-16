
#pragma once

#include "../Container/String.h"
#include "../Container/Variant.h"
#include "../Container/HashMap.h"
#include "../Container/SmartPtr.h"
#include "../Container/Function.h"
#include "../Threading/Mutex.h"
#include "../Types/Traits.h"
#include <cstdlib>
#include <climits>

namespace AltinaEngine::Core::Console {

    using Container::FString;
    using Container::TFunction;
    using Container::THashMap;
    using Container::TShared;
    using Threading::FMutex;
    using Threading::FScopedLock;

#if defined(__cpp_char8_t)
    #define AE_CVAR_CHAR8_TYPE char8_t,
    #define AE_CVAR_CHAR8_LIST(X) X(char8_t)
#else
    #define AE_CVAR_CHAR8_TYPE
    #define AE_CVAR_CHAR8_LIST(X)
#endif

    #define AE_CVAR_SCALAR_LIST(X)                                                \
        X(bool)                                                                   \
        X(char)                                                                   \
        X(signed char)                                                            \
        X(unsigned char)                                                          \
        X(short)                                                                  \
        X(unsigned short)                                                         \
        X(int)                                                                    \
        X(unsigned int)                                                           \
        X(long)                                                                   \
        X(unsigned long)                                                          \
        X(long long)                                                              \
        X(unsigned long long)                                                     \
        AE_CVAR_CHAR8_LIST(X)                                                     \
        X(char16_t)                                                               \
        X(char32_t)                                                               \
        X(wchar_t)                                                                \
        X(float)                                                                  \
        X(double)                                                                 \
        X(long double)

    class AE_CORE_API FConsoleVariable {
    public:
        enum class EType {
            String,
            Int,
            Float,
            Bool,
        };

        using FConsoleValue = Container::TVariant<bool,
            char,
            signed char,
            unsigned char,
            short,
            unsigned short,
            int,
            unsigned int,
            long,
            unsigned long,
            long long,
            unsigned long long,
            AE_CVAR_CHAR8_TYPE char16_t,
            char32_t,
            wchar_t,
            float,
            double,
            long double,
            FString>;

        using FConsoleValueTypes = AltinaEngine::TTypeSet<bool,
            char,
            signed char,
            unsigned char,
            short,
            unsigned short,
            int,
            unsigned int,
            long,
            unsigned long,
            long long,
            unsigned long long,
            AE_CVAR_CHAR8_TYPE char16_t,
            char32_t,
            wchar_t,
            float,
            double,
            long double,
            FString>;

        template <typename T>
        static constexpr bool CConsoleValueType =
            AltinaEngine::TTypeIsAnyOf<typename AltinaEngine::TDecay<T>::TType,
                FConsoleValueTypes>::Value;

        FConsoleVariable(const FString& name, FConsoleValue value, EType type);

        const FString&           GetName() const noexcept;

        FString                  GetString() const noexcept;

        int                      GetInt() const noexcept;

        long long                GetInt64() const noexcept;

        unsigned long long       GetUInt64() const noexcept;

        float                    GetFloat() const noexcept;

        double                   GetDouble() const noexcept;

        bool                     GetBool() const noexcept;

        void                     SetFromString(const FString& v) noexcept;

        void                     SetFromString(const TChar* v) noexcept;

        template <typename T>
            requires CConsoleValueType<T>
        void Set(T&& value) noexcept {
            FScopedLock lock(mMutex);
            using TDecayed = typename AltinaEngine::TDecay<T>::TType;
            mValue.Emplace<TDecayed>(AltinaEngine::Forward<T>(value));
            mType = TypeFrom<TDecayed>();
        }

        template <typename T>
            requires CConsoleValueType<T>
        auto GetValue() const noexcept -> typename AltinaEngine::TDecay<T>::TType {
            using TDecayed = typename AltinaEngine::TDecay<T>::TType;
            if constexpr (AltinaEngine::TTypeSameAs<TDecayed, FString>::Value) {
                return GetString();
            }
            FScopedLock lock(mMutex);
            TDecayed    out{};
            if (TryGetScalarValue(mValue, out)) {
                return out;
            }
            if (auto s = mValue.TryGet<FString>()) {
                return ParseScalar<TDecayed>(*s);
            }
            return out;
        }

        EType                    GetType() const noexcept { return mType; }

        // Registry helpers (header-only convenience API)
        template <typename T>
            requires CConsoleValueType<T>
                 && !AltinaEngine::TTypeSameAs<typename AltinaEngine::TDecay<T>::TType,
                        FString>::Value
        static FConsoleVariable* Register(const TChar* name, T&& defaultValue) noexcept {
            if (name == nullptr || name[0] == static_cast<TChar>(0)) {
                return nullptr;
            }
            FString nameStr(name);
            FConsoleValue value;
            using TDecayed = typename AltinaEngine::TDecay<T>::TType;
            value.Emplace<TDecayed>(AltinaEngine::Forward<T>(defaultValue));
            return RegisterInternal(nameStr, AltinaEngine::Move(value), TypeFrom<TDecayed>());
        }

        static FConsoleVariable* Register(const TChar* name, const TChar* defaultValue) noexcept;

        static FConsoleVariable* Find(const TChar* name) noexcept;

        static void              ForEach(TFunction<void(const FConsoleVariable&)> fn) noexcept;

    private:
        template <typename T> static constexpr auto TypeFrom() noexcept -> EType {
            if constexpr (AltinaEngine::TTypeSameAs<T, FString>::Value) {
                return EType::String;
            } else if constexpr (AltinaEngine::TTypeSameAs<T, bool>::Value) {
                return EType::Bool;
            } else if constexpr (AltinaEngine::CFloatingPoint<T>) {
                return EType::Float;
            } else {
                return EType::Int;
            }
        }

        static auto    GuessType(const FString& v) noexcept -> EType;
        static auto    ParseBool(const FString& v) noexcept -> bool;

        template <typename T>
        static constexpr auto IsSignedIntegral() noexcept -> bool {
            using TDecayed = typename AltinaEngine::TDecay<T>::TType;
            if constexpr (AltinaEngine::TTypeSameAs<TDecayed, signed char>::Value
                || AltinaEngine::TTypeSameAs<TDecayed, short>::Value
                || AltinaEngine::TTypeSameAs<TDecayed, int>::Value
                || AltinaEngine::TTypeSameAs<TDecayed, long>::Value
                || AltinaEngine::TTypeSameAs<TDecayed, long long>::Value
#if CHAR_MIN < 0
                || AltinaEngine::TTypeSameAs<TDecayed, char>::Value
#endif
#if defined(WCHAR_MIN) && (WCHAR_MIN < 0)
                || AltinaEngine::TTypeSameAs<TDecayed, wchar_t>::Value
#endif
            ) {
                return true;
            } else {
                return false;
            }
        }

        template <typename T>
        static auto ParseIntegral(const FString& v) noexcept -> T {
            bool  neg = false;
            usize idx = 0;
            if (v.Length() > 0 && (v[0] == '+' || v[0] == '-')) {
                neg = (v[0] == '-');
                idx = 1;
            }

            unsigned long long result = 0ULL;
            for (; idx < v.Length(); ++idx) {
                const TChar c = v[idx];
                if (c >= static_cast<TChar>('0') && c <= static_cast<TChar>('9')) {
                    result = result * 10ULL + static_cast<unsigned long long>(c - '0');
                } else {
                    return static_cast<T>(0);
                }
            }

            if constexpr (IsSignedIntegral<T>()) {
                return neg ? static_cast<T>(-static_cast<long long>(result))
                           : static_cast<T>(result);
            } else {
                if (neg) {
                    return static_cast<T>(0);
                }
                return static_cast<T>(result);
            }
        }

        template <typename T>
        static auto ParseFloat(const FString& v) noexcept -> T {
            auto view = v.ToView();
            const usize n = view.Length();
            char* buf = new char[n + 1];
            for (usize i = 0; i < n; ++i) {
                buf[i] = static_cast<char>(view.Data()[i]);
            }
            buf[n] = '\0';

            long double value = 0.0;
            try {
                value = std::strtold(buf, nullptr);
            } catch (...) {
                value = 0.0;
            }
            delete[] buf;
            return static_cast<T>(value);
        }

        template <typename T>
        static auto ParseScalar(const FString& v) noexcept -> T {
            if constexpr (AltinaEngine::TTypeSameAs<T, bool>::Value) {
                return ParseBool(v);
            } else if constexpr (AltinaEngine::CFloatingPoint<T>) {
                return ParseFloat<T>(v);
            } else {
                return ParseIntegral<T>(v);
            }
        }

        template <typename T>
        static auto TryGetScalarValue(const FConsoleValue& value, T& out) noexcept -> bool {
            if (auto v = value.TryGet<T>()) {
                out = *v;
                return true;
            }

            #define AE_CVAR_TRY_TYPE(Type)                           \
                if (auto v = value.TryGet<Type>()) {                 \
                    out = static_cast<T>(*v);                        \
                    return true;                                     \
                }

            AE_CVAR_SCALAR_LIST(AE_CVAR_TRY_TYPE)
            #undef AE_CVAR_TRY_TYPE
            return false;
        }

        static FConsoleVariable* RegisterInternal(
            const FString& name, FConsoleValue&& value, EType type) noexcept;

        FString        mName;
        FConsoleValue  mValue;
        mutable FMutex mMutex;
        EType          mType;

        // Hash / equality helpers for FString as THashMap key
        struct FStringHash {
            auto operator()(const FString& s) const noexcept -> size_t;
        };

        struct FStringEqual {
            auto operator()(const FString& a, const FString& b) const noexcept -> bool;
        };

        using RegistryMap = THashMap<FString, TShared<FConsoleVariable>, FStringHash, FStringEqual>;

        static RegistryMap gRegistry;
        static FMutex      gRegistryMutex;
    };

    template <typename T>
        requires FConsoleVariable::CConsoleValueType<T>
              && !AltinaEngine::TTypeSameAs<typename AltinaEngine::TDecay<T>::TType,
                     FString>::Value
    class TConsoleVariable {
    public:
        using TValue = typename AltinaEngine::TDecay<T>::TType;

        TConsoleVariable() noexcept = default;

        explicit TConsoleVariable(const TChar* name, T&& defaultValue) noexcept {
            mVariable = FConsoleVariable::Register(name, AltinaEngine::Forward<T>(defaultValue));
        }

        auto Get() const noexcept -> TValue {
            return mVariable ? mVariable->GetValue<TValue>() : TValue{};
        }

        void Set(TValue value) noexcept {
            if (mVariable) {
                mVariable->Set<TValue>(AltinaEngine::Move(value));
            }
        }

        auto GetRaw() const noexcept -> FConsoleVariable* { return mVariable; }

    private:
        FConsoleVariable* mVariable = nullptr;
    };

    #undef AE_CVAR_SCALAR_LIST
    #undef AE_CVAR_CHAR8_LIST
    #undef AE_CVAR_CHAR8_TYPE

} // namespace AltinaEngine::Core::Console
