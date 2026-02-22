#pragma once

#include "../Types/Aliases.h"
#include "../Types/Traits.h"

using AltinaEngine::CCopyConstructible;
using AltinaEngine::CMoveConstructible;
using AltinaEngine::Forward;
using AltinaEngine::Move;
namespace AltinaEngine::Core::Container {
    using AltinaEngine::usize;

    namespace Detail {
        template <typename T> struct TVariantAlwaysFalse : TFalseType {};

        template <typename T, typename... Ts> struct TVariantIndex;
        template <typename T, typename... Rest> struct TVariantIndex<T, T, Rest...> {
            static constexpr usize Value = 0U;
        };
        template <typename T, typename First, typename... Rest>
        struct TVariantIndex<T, First, Rest...> {
            static constexpr usize Value = 1U + TVariantIndex<T, Rest...>::Value;
        };
        template <typename T> struct TVariantIndex<T> {
            static_assert(TVariantAlwaysFalse<T>::Value, "TVariant does not contain this type.");
        };

        template <typename... Ts> struct TVariantMaxSize;
        template <typename T> struct TVariantMaxSize<T> {
            static constexpr usize Value = sizeof(T);
        };
        template <typename T, typename... Ts> struct TVariantMaxSize<T, Ts...> {
            static constexpr usize Tail  = TVariantMaxSize<Ts...>::Value;
            static constexpr usize Value = sizeof(T) > Tail ? sizeof(T) : Tail;
        };

        template <typename... Ts> struct TVariantMaxAlign;
        template <typename T> struct TVariantMaxAlign<T> {
            static constexpr usize Value = alignof(T);
        };
        template <typename T, typename... Ts> struct TVariantMaxAlign<T, Ts...> {
            static constexpr usize Tail  = TVariantMaxAlign<Ts...>::Value;
            static constexpr usize Value = alignof(T) > Tail ? alignof(T) : Tail;
        };

        template <usize I, typename... Ts> struct TVariantTypeAt;
        template <typename T, typename... Ts> struct TVariantTypeAt<0, T, Ts...> {
            using Type = T;
        };
        template <usize I, typename T, typename... Ts> struct TVariantTypeAt<I, T, Ts...> {
            using Type = TVariantTypeAt<I - 1, Ts...>::Type;
        };
    } // namespace Detail

    template <typename... TArgs> class TVariant {
    public:
        static_assert(sizeof...(TArgs) > 0, "TVariant requires at least one alternative type.");
        static constexpr usize kInvalidIndex = static_cast<usize>(-1);

        TVariant() noexcept : mStorage{}, mIndex(kInvalidIndex) {}

        template <typename T>
            requires TTypeIsAnyOf<typename TDecay<T>::TType, TTypeSet<TArgs...>>::Value
        explicit TVariant(T&& value) : mStorage{}, mIndex(kInvalidIndex) {
            Emplace<typename TDecay<T>::TType>(Forward<T>(value));
        }

        TVariant(const TVariant& other) : mStorage{}, mIndex(kInvalidIndex) {
            static_assert((CCopyConstructible<TArgs> && ...),
                "TVariant copy requires all alternatives to be copy-constructible.");
            CopyFrom(other);
        }

        TVariant(TVariant&& other) noexcept : mStorage{}, mIndex(kInvalidIndex) {
            static_assert((CMoveConstructible<TArgs> && ...),
                "TVariant move requires all alternatives to be move-constructible.");
            MoveFrom(Move(other));
        }

        ~TVariant() { Destroy(); }

        auto operator=(const TVariant& other) -> TVariant& {
            static_assert((CCopyConstructible<TArgs> && ...),
                "TVariant copy requires all alternatives to be copy-constructible.");
            if (this != &other) {
                Destroy();
                CopyFrom(other);
            }
            return *this;
        }

        auto operator=(TVariant&& other) noexcept -> TVariant& {
            static_assert((CMoveConstructible<TArgs> && ...),
                "TVariant move requires all alternatives to be move-constructible.");
            if (this != &other) {
                Destroy();
                MoveFrom(Move(other));
            }
            return *this;
        }

        template <typename T>
            requires TTypeIsAnyOf<typename TDecay<T>::TType, TTypeSet<TArgs...>>::Value
        auto operator=(T&& value) -> TVariant& {
            Emplace<typename TDecay<T>::TType>(Forward<T>(value));
            return *this;
        }

        [[nodiscard]] auto HasValue() const noexcept -> bool { return mIndex != kInvalidIndex; }
        [[nodiscard]] auto Index() const noexcept -> usize { return mIndex; }

        void               Reset() noexcept { Destroy(); }

        template <typename T> [[nodiscard]] auto Is() const noexcept -> bool {
            return mIndex == IndexOf<typename TDecay<T>::TType>();
        }

        template <typename T, typename... Args> auto Emplace(Args&&... args) -> T& {
            using TDecayed = TDecay<T>::TType;
            static_assert(TTypeIsAnyOf<TDecayed, TTypeSet<TArgs...>>::Value,
                "TVariant::Emplace type must be one of the variant alternatives.");
            Destroy();
            new (mStorage) TDecayed(Forward<Args>(args)...);
            mIndex = IndexOf<TDecayed>();
            return *reinterpret_cast<TDecayed*>(mStorage);
        }

        template <typename T> [[nodiscard]] auto Get() -> T& {
            using TDecayed = TDecay<T>::TType;
            static_assert(TTypeIsAnyOf<TDecayed, TTypeSet<TArgs...>>::Value,
                "TVariant::Get type must be one of the variant alternatives.");
            return *reinterpret_cast<TDecayed*>(mStorage);
        }

        template <typename T> [[nodiscard]] auto Get() const -> const T& {
            using TDecayed = TDecay<T>::TType;
            static_assert(TTypeIsAnyOf<TDecayed, TTypeSet<TArgs...>>::Value,
                "TVariant::Get type must be one of the variant alternatives.");
            return *reinterpret_cast<const TDecayed*>(mStorage);
        }

        template <typename T> [[nodiscard]] auto TryGet() noexcept -> T* {
            return Is<T>() ? &Get<T>() : nullptr;
        }

        template <typename T> [[nodiscard]] auto TryGet() const noexcept -> const T* {
            return Is<T>() ? &Get<T>() : nullptr;
        }

    private:
        template <typename T> static constexpr auto IndexOf() noexcept -> usize {
            using TDecayed = TDecay<T>::TType;
            return Detail::TVariantIndex<TDecayed, TArgs...>::Value;
        }

        void Destroy() noexcept {
            if (!HasValue())
                return;
            DestroyAt(mIndex);
            mIndex = kInvalidIndex;
        }

        template <usize I = 0> void DestroyAt(usize index) noexcept {
            if constexpr (I < sizeof...(TArgs)) {
                if (index == I) {
                    using T = Detail::TVariantTypeAt<I, TArgs...>::Type;
                    reinterpret_cast<T*>(mStorage)->~T();
                } else {
                    DestroyAt<I + 1>(index);
                }
            }
        }

        void CopyFrom(const TVariant& other) {
            if (!other.HasValue()) {
                mIndex = kInvalidIndex;
                return;
            }
            CopyAt(other.mIndex, other);
        }

        template <usize I = 0> void CopyAt(usize index, const TVariant& other) {
            if constexpr (I < sizeof...(TArgs)) {
                if (index == I) {
                    using T = Detail::TVariantTypeAt<I, TArgs...>::Type;
                    new (mStorage) T(*reinterpret_cast<const T*>(other.mStorage));
                    mIndex = I;
                } else {
                    CopyAt<I + 1>(index, other);
                }
            }
        }

        void MoveFrom(TVariant&& other) noexcept {
            if (!other.HasValue()) {
                mIndex = kInvalidIndex;
                return;
            }
            MoveAt(other.mIndex, Move(other));
        }

        template <usize I = 0> void MoveAt(usize index, TVariant&& other) noexcept {
            if constexpr (I < sizeof...(TArgs)) {
                if (index == I) {
                    using T = Detail::TVariantTypeAt<I, TArgs...>::Type;
                    new (mStorage) T(Move(*reinterpret_cast<T*>(other.mStorage)));
                    mIndex = I;
                    other.Destroy();
                } else {
                    MoveAt<I + 1>(index, Move(other));
                }
            }
        }

        static constexpr usize kStorageSize  = Detail::TVariantMaxSize<TArgs...>::Value;
        static constexpr usize kStorageAlign = Detail::TVariantMaxAlign<TArgs...>::Value;

        alignas(kStorageAlign) unsigned char mStorage[kStorageSize];
        usize mIndex = kInvalidIndex;
    };
} // namespace AltinaEngine::Core::Container
