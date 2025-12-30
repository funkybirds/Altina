
#pragma once

#include "../Container/String.h"
#include "../Container/HashMap.h"
#include "../Container/SmartPtr.h"
#include "../Container/Function.h"
#include "../Threading/Mutex.h"
#include "../Algorithm/CStringUtils.h"
#include <cstdlib>
#include <cctype>

namespace AltinaEngine::Core::Console
{

    using Container::FString;
    using Container::TFunction;
    using Container::THashMap;
    using Container::TShared;
    using Threading::FMutex;
    using Threading::FScopedLock;

    class AE_CORE_API FConsoleVariable
    {
    public:
        enum class EType
        {
            String,
            Int,
            Float,
            Bool,
        };

        FConsoleVariable(const FString& name, const FString& defaultValue);

        const FString& GetName() const noexcept;

        FString GetString() const noexcept;

        int GetInt() const noexcept;

        float GetFloat() const noexcept;

        bool GetBool() const noexcept;

        void SetFromString(const FString& v) noexcept;

        EType                    GetType() const noexcept { return mType; }

        // Registry helpers (header-only convenience API)
        static FConsoleVariable* Register(const FString& name, const FString& defaultValue) noexcept;

        static FConsoleVariable* Find(const FString& name) noexcept;

        static void ForEach(TFunction<void(const FConsoleVariable&)> fn) noexcept;

    private:
        static EType GuessType(const FString& v) noexcept;

        FString        mName;
        FString        mValue;
        mutable FMutex mMutex;
        EType          mType;

        // Hash / equality helpers for FString as THashMap key
        struct FStringHash
        {
            auto operator()(const FString& s) const noexcept -> size_t;
        };

        struct FStringEqual
        {
            auto operator()(const FString& a, const FString& b) const noexcept -> bool;
        };

        using RegistryMap = THashMap<FString, TShared<FConsoleVariable>, FStringHash, FStringEqual>;

        static RegistryMap gRegistry;
        static FMutex      gRegistryMutex;
    };

} // namespace AltinaEngine::Core::Console
