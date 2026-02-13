#pragma once

#include "../Types/Aliases.h"
#include "../Types/Traits.h"

namespace AltinaEngine::Core::Container {
    // Minimal reference wrapper similar to std::reference_wrapper
    template <typename T> class TRef {
    public:
        using Type = T;

        explicit TRef(T& ref) noexcept : mPtr(std::addressof(ref)) {}

        // Access
        auto Get() const noexcept -> T& { return *mPtr; }
        auto operator()() const noexcept -> T& { return *mPtr; }
             operator T&() const noexcept { return *mPtr; }

        // No ownership semantics: copyable and assignable
        TRef(const TRef& other) noexcept                           = default;
        auto        operator=(const TRef& other) noexcept -> TRef& = default;

        // Helpers
        static auto From(T& v) noexcept -> TRef { return TRef(v); }

    private:
        T* mPtr;
    };

    // Helper to deduce type like std::ref
    template <typename T> auto MakeRef(T& v) noexcept -> TRef<T> { return TRef<T>(v); }

} // namespace AltinaEngine::Core::Container
