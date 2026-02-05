#pragma once

#include "../Types/Traits.h"

namespace AltinaEngine::Core::Container {

    template <typename T> class TCountRef {
    public:
        using TPointer = T*;

        constexpr TCountRef() noexcept = default;
        constexpr TCountRef(decltype(nullptr)) noexcept : mPtr(nullptr) {}

        static auto Adopt(TPointer ptr) noexcept -> TCountRef { return TCountRef(ptr, EMode::Adopt); }

        explicit TCountRef(TPointer ptr) noexcept : mPtr(ptr) { AddRef(); }

        TCountRef(const TCountRef& other) noexcept : mPtr(other.mPtr) { AddRef(); }

        TCountRef(TCountRef&& other) noexcept : mPtr(other.mPtr) { other.mPtr = nullptr; }

        template <typename U>
            requires requires(U* ptr) { static_cast<T*>(ptr); }
        TCountRef(const TCountRef<U>& other) noexcept : mPtr(static_cast<T*>(other.Get())) {
            AddRef();
        }

        template <typename U>
            requires requires(U* ptr) { static_cast<T*>(ptr); }
        TCountRef(TCountRef<U>&& other) noexcept : mPtr(static_cast<T*>(other.mPtr)) {
            other.mPtr = nullptr;
        }

        ~TCountRef() { Release(); }

        auto operator=(const TCountRef& other) noexcept -> TCountRef& {
            if (this != &other) {
                Reset();
                mPtr = other.mPtr;
                AddRef();
            }
            return *this;
        }

        auto operator=(TCountRef&& other) noexcept -> TCountRef& {
            if (this != &other) {
                Reset();
                mPtr       = other.mPtr;
                other.mPtr = nullptr;
            }
            return *this;
        }

        void Reset() noexcept { Release(); }

        void Reset(TPointer ptr) noexcept {
            Reset();
            mPtr = ptr;
            AddRef();
        }

        void ResetAdopt(TPointer ptr) noexcept {
            Reset();
            mPtr = ptr;
        }

        void Swap(TCountRef& other) noexcept {
            TPointer tmp = mPtr;
            mPtr         = other.mPtr;
            other.mPtr   = tmp;
        }

        [[nodiscard]] auto Get() const noexcept -> TPointer { return mPtr; }
        [[nodiscard]] auto operator->() const noexcept -> TPointer { return mPtr; }
        [[nodiscard]] auto operator*() const -> T& { return *mPtr; }
        explicit           operator bool() const noexcept { return mPtr != nullptr; }

        [[nodiscard]] auto GetRefCount() const noexcept -> u32 {
            return mPtr ? mPtr->GetRefCount() : 0U;
        }

    private:
        template <typename U> friend class TCountRef;

        enum class EMode : u8 { AddRef, Adopt };

        explicit TCountRef(TPointer ptr, EMode mode) noexcept : mPtr(ptr) {
            if (mPtr && mode == EMode::AddRef) {
                mPtr->AddRef();
            }
        }

        void AddRef() noexcept {
            if (mPtr) {
                mPtr->AddRef();
            }
        }

        void Release() noexcept {
            if (mPtr) {
                mPtr->Release();
                mPtr = nullptr;
            }
        }

        TPointer mPtr = nullptr;
    };

    template <typename T> auto MakeCountRef(T* ptr) noexcept -> TCountRef<T> {
        return TCountRef<T>::Adopt(ptr);
    }

} // namespace AltinaEngine::Core::Container
