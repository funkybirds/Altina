#include "../../Public/Console/ConsoleVariable.h"

#include "../../Public/Container/SmartPtr.h"
#include "../../Public/Container/Function.h"
#include "../../Public/Algorithm/CStringUtils.h"
#include <cstdlib>
#include <cctype>

namespace AltinaEngine::Core::Console {

    FConsoleVariable::FConsoleVariable(const FString& name, const FString& defaultValue)
        : mName(name), mValue(defaultValue), mType(GuessType(mValue)) {}

    const FString& FConsoleVariable::GetName() const noexcept { return mName; }

    FString        FConsoleVariable::GetString() const noexcept {
        FScopedLock lock(mMutex);
        return mValue;
    }

    AE_CORE_API int FConsoleVariable::GetInt() const noexcept {
        FScopedLock lock(mMutex);
        auto        s   = mValue;
        bool        neg = false;
        usize       idx = 0;
        if (s.Length() > 0 && (s[0] == '+' || s[0] == '-')) {
            neg = (s[0] == '-');
            idx = 1;
        }
        int result = 0;
        for (; idx < s.Length(); ++idx) {
            unsigned char c = static_cast<unsigned char>(s[idx]);
            if (std::isdigit(c)) {
                result = result * 10 + (s[idx] - '0');
            } else {
                return 0;
            }
        }
        return neg ? -result : result;
    }

    AE_CORE_API float FConsoleVariable::GetFloat() const noexcept {
        FScopedLock lock(mMutex);
        auto        view = mValue.ToView();
        const usize n    = view.Length();
        char*       buf  = new char[n + 1];
        for (usize i = 0; i < n; ++i)
            buf[i] = static_cast<char>(view.Data()[i]);
        buf[n]  = '\0';
        float f = 0.0f;
        try {
            f = std::strtof(buf, nullptr);
        } catch (...) {
            f = 0.0f;
        }
        delete[] buf;
        return f;
    }

    AE_CORE_API bool FConsoleVariable::GetBool() const noexcept {
        FScopedLock lock(mMutex);
        auto        s = mValue.ToLowerCopy();
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

    AE_CORE_API void FConsoleVariable::SetFromString(const FString& v) noexcept {
        FScopedLock lock(mMutex);
        mValue = v;
    }

    AE_CORE_API FConsoleVariable* FConsoleVariable::Register(
        const FString& name, const FString& defaultValue) noexcept {
        FScopedLock lock(gRegistryMutex);
        auto        it = gRegistry.find(name);
        if (it != gRegistry.end())
            return it->second.Get();
        auto var = Container::MakeShared<FConsoleVariable>(name, defaultValue);
        gRegistry.emplace(name, AltinaEngine::Move(var));
        return gRegistry.find(name)->second.Get();
    }

    AE_CORE_API FConsoleVariable* FConsoleVariable::Find(const FString& name) noexcept {
        FScopedLock lock(gRegistryMutex);
        auto        it = gRegistry.find(name);
        return it != gRegistry.end() ? it->second.Get() : nullptr;
    }

    AE_CORE_API void FConsoleVariable::ForEach(
        TFunction<void(const FConsoleVariable&)> fn) noexcept {
        FScopedLock lock(gRegistryMutex);
        for (auto const& p : gRegistry)
            fn(*p.second);
    }

    FConsoleVariable::EType FConsoleVariable::GuessType(const FString& v) noexcept {
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
