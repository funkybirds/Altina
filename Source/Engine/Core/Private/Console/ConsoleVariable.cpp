#include "../../Public/Console/ConsoleVariable.h"

#include "../../Public/Container/SmartPtr.h"
#include "../../Public/Container/Function.h"
#include "../../Public/Algorithm/CStringUtils.h"

#include <cstdio>
#include <cctype>

#if defined(__cpp_char8_t)
    #define AE_CVAR_CHAR8_LIST(X) X(char8_t)
#else
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

namespace AltinaEngine::Core::Console {
    namespace {
        template <typename T> struct TConsoleIsUnsigned : AltinaEngine::TFalseType {};
        template <> struct TConsoleIsUnsigned<unsigned char> : AltinaEngine::TTrueType {};
        template <> struct TConsoleIsUnsigned<unsigned short> : AltinaEngine::TTrueType {};
        template <> struct TConsoleIsUnsigned<unsigned int> : AltinaEngine::TTrueType {};
        template <> struct TConsoleIsUnsigned<unsigned long> : AltinaEngine::TTrueType {};
        template <> struct TConsoleIsUnsigned<unsigned long long> : AltinaEngine::TTrueType {};
#if defined(__cpp_char8_t)
        template <> struct TConsoleIsUnsigned<char8_t> : AltinaEngine::TTrueType {};
#endif
        template <> struct TConsoleIsUnsigned<char16_t> : AltinaEngine::TTrueType {};
        template <> struct TConsoleIsUnsigned<char32_t> : AltinaEngine::TTrueType {};
        template <> struct TConsoleIsUnsigned<wchar_t> : AltinaEngine::TTrueType {};

        auto MakeFStringFromCString(const char* text) -> FString {
            FString out;
            if (text == nullptr) {
                return out;
            }
            for (const char* c = text; *c != '\0'; ++c) {
                out.Append(static_cast<TChar>(*c));
            }
            return out;
        }

        template <typename T>
        auto ToFStringScalar(T value) -> FString {
            if constexpr (AltinaEngine::TTypeSameAs<T, bool>::Value) {
                return MakeFStringFromCString(value ? "true" : "false");
            } else if constexpr (AltinaEngine::CFloatingPoint<T>) {
                char buf[64] = {};
                if constexpr (AltinaEngine::TTypeSameAs<T, long double>::Value) {
                    std::snprintf(buf, sizeof(buf), "%Lg", value);
                } else {
                    std::snprintf(buf, sizeof(buf), "%g", static_cast<double>(value));
                }
                return MakeFStringFromCString(buf);
            } else if constexpr (AltinaEngine::CIntegral<T>) {
                char buf[64] = {};
                if constexpr (!TConsoleIsUnsigned<T>::Value) {
                    std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(value));
                } else {
                    std::snprintf(buf, sizeof(buf), "%llu", static_cast<unsigned long long>(value));
                }
                return MakeFStringFromCString(buf);
            } else {
                return FString();
            }
        }
    } // namespace

    FConsoleVariable::FConsoleVariable(const FString& name, FConsoleValue value, EType type)
        : mName(name), mValue(AltinaEngine::Move(value)), mType(type) {}

    const FString& FConsoleVariable::GetName() const noexcept { return mName; }

    FString FConsoleVariable::GetString() const noexcept {
        FScopedLock lock(mMutex);
        if (auto s = mValue.TryGet<FString>()) {
            return *s;
        }
        if (auto b = mValue.TryGet<bool>()) {
            return ToFStringScalar(*b);
        }

        #define AE_CVAR_TRY_STRING(Type)                 \
            if (auto v = mValue.TryGet<Type>()) {        \
                return ToFStringScalar(*v);              \
            }

        AE_CVAR_SCALAR_LIST(AE_CVAR_TRY_STRING)
        #undef AE_CVAR_TRY_STRING
        return FString();
    }

    int FConsoleVariable::GetInt() const noexcept { return GetValue<int>(); }

    long long FConsoleVariable::GetInt64() const noexcept { return GetValue<long long>(); }

    unsigned long long FConsoleVariable::GetUInt64() const noexcept {
        return GetValue<unsigned long long>();
    }

    float FConsoleVariable::GetFloat() const noexcept { return GetValue<float>(); }

    double FConsoleVariable::GetDouble() const noexcept { return GetValue<double>(); }

    bool FConsoleVariable::GetBool() const noexcept { return GetValue<bool>(); }

    void FConsoleVariable::SetFromString(const FString& v) noexcept {
        FScopedLock lock(mMutex);

        if (!mValue.HasValue()) {
            mType = GuessType(v);
            switch (mType) {
            case EType::Bool:
                mValue.Emplace<bool>(ParseBool(v));
                return;
            case EType::Float:
                mValue.Emplace<float>(ParseFloat<float>(v));
                return;
            case EType::Int:
                mValue.Emplace<int>(ParseIntegral<int>(v));
                return;
            case EType::String:
            default:
                mValue.Emplace<FString>(v);
                return;
            }
        }

        if (mValue.Is<FString>()) {
            mValue.Emplace<FString>(v);
            mType = EType::String;
            return;
        }

        if (mValue.Is<bool>()) {
            mValue.Emplace<bool>(ParseBool(v));
            mType = EType::Bool;
            return;
        }

        if (mValue.Is<float>()) {
            mValue.Emplace<float>(ParseFloat<float>(v));
            mType = EType::Float;
            return;
        }
        if (mValue.Is<double>()) {
            mValue.Emplace<double>(ParseFloat<double>(v));
            mType = EType::Float;
            return;
        }
        if (mValue.Is<long double>()) {
            mValue.Emplace<long double>(ParseFloat<long double>(v));
            mType = EType::Float;
            return;
        }

        #define AE_CVAR_SET_INT(Type)                        \
            if (mValue.Is<Type>()) {                         \
                mValue.Emplace<Type>(ParseIntegral<Type>(v)); \
                mType = EType::Int;                          \
                return;                                      \
            }

        AE_CVAR_SET_INT(char)
        AE_CVAR_SET_INT(signed char)
        AE_CVAR_SET_INT(unsigned char)
        AE_CVAR_SET_INT(short)
        AE_CVAR_SET_INT(unsigned short)
        AE_CVAR_SET_INT(int)
        AE_CVAR_SET_INT(unsigned int)
        AE_CVAR_SET_INT(long)
        AE_CVAR_SET_INT(unsigned long)
        AE_CVAR_SET_INT(long long)
        AE_CVAR_SET_INT(unsigned long long)
    #if defined(__cpp_char8_t)
        AE_CVAR_SET_INT(char8_t)
    #endif
        AE_CVAR_SET_INT(char16_t)
        AE_CVAR_SET_INT(char32_t)
        AE_CVAR_SET_INT(wchar_t)
        #undef AE_CVAR_SET_INT

        mType = GuessType(v);
        switch (mType) {
        case EType::Bool:
            mValue.Emplace<bool>(ParseBool(v));
            break;
        case EType::Float:
            mValue.Emplace<float>(ParseFloat<float>(v));
            break;
        case EType::Int:
            mValue.Emplace<int>(ParseIntegral<int>(v));
            break;
        case EType::String:
        default:
            mValue.Emplace<FString>(v);
            break;
        }
    }

    void FConsoleVariable::SetFromString(const TChar* v) noexcept {
        if (v == nullptr) {
            SetFromString(FString());
            return;
        }
        SetFromString(FString(v));
    }

    FConsoleVariable* FConsoleVariable::Register(
        const TChar* name, const TChar* defaultValue) noexcept {
        if (name == nullptr || name[0] == static_cast<TChar>(0)) {
            return nullptr;
        }
        FString nameStr(name);
        FString valueStr;
        if (defaultValue != nullptr) {
            valueStr.Assign(defaultValue);
        }
        FConsoleValue value;
        value.Emplace<FString>(AltinaEngine::Move(valueStr));
        return RegisterInternal(nameStr, AltinaEngine::Move(value), EType::String);
    }

    FConsoleVariable* FConsoleVariable::RegisterInternal(
        const FString& name, FConsoleValue&& value, EType type) noexcept {
        FScopedLock lock(gRegistryMutex);
        auto        it = gRegistry.find(name);
        if (it != gRegistry.end())
            return it->second.Get();
        auto var = Container::MakeShared<FConsoleVariable>(name, AltinaEngine::Move(value), type);
        gRegistry.emplace(name, AltinaEngine::Move(var));
        return gRegistry.find(name)->second.Get();
    }

    FConsoleVariable* FConsoleVariable::Find(const TChar* name) noexcept {
        if (name == nullptr || name[0] == static_cast<TChar>(0)) {
            return nullptr;
        }
        FString nameStr(name);
        FScopedLock lock(gRegistryMutex);
        auto        it = gRegistry.find(nameStr);
        return it != gRegistry.end() ? it->second.Get() : nullptr;
    }

    void FConsoleVariable::ForEach(TFunction<void(const FConsoleVariable&)> fn) noexcept {
        FScopedLock lock(gRegistryMutex);
        for (auto const& p : gRegistry)
            fn(*p.second);
    }

    auto FConsoleVariable::GuessType(const FString& v) noexcept -> EType {
        if (v.IsEmptyString())
            return EType::String;
        auto s = v.ToLowerCopy();
        if (s.Length() == 4 && s[0] == 't' && s[1] == 'r' && s[2] == 'u' && s[3] == 'e')
            return EType::Bool;
        if (s.Length() == 5 && s[0] == 'f' && s[1] == 'a' && s[2] == 'l' && s[3] == 's'
            && s[4] == 'e')
            return EType::Bool;
        if (s.Length() == 2 && ((s[0] == 'o' && s[1] == 'n') || (s[0] == 'y' && s[1] == 'e')))
            return EType::Bool;

        bool hasDot   = false;
        bool hasDigit = false;
        for (usize i = 0; i < v.Length(); ++i) {
            char c = static_cast<char>(v.ToView().Data()[i]);
            if (c == '.')
                hasDot = true;
            if (std::isdigit(static_cast<unsigned char>(c)))
                hasDigit = true;
            else if (c == '+' || c == '-')
                continue;
            else if (c == '.')
                continue;
            else
                return EType::String;
        }
        if (hasDigit && hasDot)
            return EType::Float;
        if (hasDigit)
            return EType::Int;
        return EType::String;
    }

    auto FConsoleVariable::ParseBool(const FString& v) noexcept -> bool {
        auto s = v.ToLowerCopy();
        if (s.Length() == 1 && (s[0] == '1'))
            return true;
        if (s.Length() == 4 && s[0] == 't' && s[1] == 'r' && s[2] == 'u' && s[3] == 'e')
            return true;
        if (s.Length() == 3 && s[0] == 'y' && s[1] == 'e' && s[2] == 's')
            return true;
        if (s.Length() == 2 && s[0] == 'o' && s[1] == 'n')
            return true;
        return false;
    }

    size_t FConsoleVariable::FStringHash::operator()(const FString& s) const noexcept {
        size_t h    = 5381;
        auto   view = s.ToView();
        for (usize i = 0; i < view.Length(); ++i)
            h = ((h << 5) + h) + static_cast<unsigned char>(view.Data()[i]);
        return h;
    }

    bool FConsoleVariable::FStringEqual::operator()(
        const FString& a, const FString& b) const noexcept {
        if (a.Length() != b.Length())
            return false;
        auto va = a.ToView();
        auto vb = b.ToView();
        for (usize i = 0; i < va.Length(); ++i) {
            if (va.Data()[i] != vb.Data()[i])
                return false;
        }
        return true;
    }

    // static definitions
    FConsoleVariable::RegistryMap FConsoleVariable::gRegistry;
    FMutex                        FConsoleVariable::gRegistryMutex;

} // namespace AltinaEngine::Core::Console

#undef AE_CVAR_SCALAR_LIST
#undef AE_CVAR_CHAR8_LIST
