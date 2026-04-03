#pragma once
#include "Types/Aliases.h"
#include "Types/Traits.h"
#include "Utility/Assert.h"
namespace AltinaEngine::Core::Container {

    template <CPointer T> class TNonNullPtr {
    public:
        using TPointer = T;

        constexpr TNonNullPtr() noexcept                           = delete;
        constexpr TNonNullPtr(decltype(nullptr)) noexcept          = delete;
        auto operator=(decltype(nullptr)) noexcept -> TNonNullPtr& = delete;

        TNonNullPtr(TPointer ptr) noexcept : mPtr(ptr) { Validate(ptr); }

        constexpr TNonNullPtr(const TNonNullPtr&) noexcept          = default;
        constexpr TNonNullPtr(TNonNullPtr&&) noexcept               = default;
        auto operator=(const TNonNullPtr&) noexcept -> TNonNullPtr& = default;
        auto operator=(TNonNullPtr&&) noexcept -> TNonNullPtr&      = default;

        template <CPointer U>
            requires CStaticConvertible<U, TPointer>
        TNonNullPtr(U ptr) noexcept : mPtr(static_cast<TPointer>(ptr)) {
            Validate(mPtr);
        }

        template <CPointer U>
            requires CStaticConvertible<U, TPointer>
        constexpr TNonNullPtr(const TNonNullPtr<U>& other) noexcept
            : mPtr(static_cast<TPointer>(other.Get())) {}

        template <CPointer U>
            requires CStaticConvertible<U, TPointer>
        auto operator=(const TNonNullPtr<U>& other) noexcept -> TNonNullPtr& {
            mPtr = static_cast<TPointer>(other.Get());
            return *this;
        }

        auto operator=(TPointer ptr) noexcept -> TNonNullPtr& {
            Validate(ptr);
            mPtr = ptr;
            return *this;
        }

        [[nodiscard]] constexpr auto     Get() const noexcept -> TPointer { return mPtr; }
        [[nodiscard]] constexpr auto     operator->() const noexcept -> TPointer { return mPtr; }
        [[nodiscard]] constexpr explicit operator bool() const noexcept { return true; }
        [[nodiscard]] constexpr          operator TPointer() const noexcept { return mPtr; }

        [[nodiscard]] constexpr decltype(auto) operator*() const
            requires requires(TPointer ptr) { *ptr; }
        {
            return *mPtr;
        }

        [[nodiscard]] constexpr decltype(auto) operator[](usize index) const
            requires requires(TPointer ptr) { ptr[index]; }
        {
            return mPtr[index];
        }

    private:
        static void Validate(TPointer ptr) noexcept {
            Utility::Assert(ptr != nullptr, TEXT("Core.Container"),
                TEXT("TNonNullPtr must not be constructed or assigned from nullptr"));
        }

        TPointer mPtr;
    };

} // namespace AltinaEngine::Core::Container
