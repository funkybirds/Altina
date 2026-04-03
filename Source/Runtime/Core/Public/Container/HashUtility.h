#pragma once

#include "../Types/Aliases.h"
#include "../Types/Traits.h"

namespace AltinaEngine::Core::Container {
    template <typename T> class TBasicString;
    template <typename T> class TBasicStringView;

    namespace Detail {
        [[nodiscard]] constexpr auto MixU64(u64 value) noexcept -> u64 {
            value += 0x9e3779b97f4a7c15ULL;
            value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
            value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
            return value ^ (value >> 31U);
        }

        [[nodiscard]] constexpr auto FoldToUsize(u64 value) noexcept -> usize {
            if constexpr (sizeof(usize) >= sizeof(u64)) {
                return static_cast<usize>(value);
            } else {
                return static_cast<usize>(value ^ (value >> 32U));
            }
        }

        [[nodiscard]] inline auto HashBytes(const void* data, usize size) noexcept -> usize {
            auto        hash  = static_cast<u64>(1469598103934665603ULL);
            const auto* bytes = static_cast<const u8*>(data);
            for (usize i = 0; i < size; ++i) {
                hash ^= static_cast<u64>(bytes[i]);
                hash *= static_cast<u64>(1099511628211ULL);
            }
            return FoldToUsize(MixU64(hash));
        }
    } // namespace Detail

    template <typename T> struct THashFunc {
        auto operator()(const T&) const noexcept -> usize = delete;
    };

    template <CIntegral T> struct THashFunc<T> {
        auto operator()(T value) const noexcept -> usize {
            return Detail::FoldToUsize(Detail::MixU64(static_cast<u64>(value)));
        }
    };

    template <CFloatingPoint T> struct THashFunc<T> {
        auto operator()(T value) const noexcept -> usize {
            return Detail::HashBytes(&value, sizeof(T));
        }
    };

    template <CPointer T> struct THashFunc<T> {
        auto operator()(T value) const noexcept -> usize {
            return Detail::FoldToUsize(
                Detail::MixU64(static_cast<u64>(reinterpret_cast<usize>(value))));
        }
    };

    template <typename T>
        requires CEnum<T>
    struct THashFunc<T> {
        auto operator()(T value) const noexcept -> usize {
            return THashFunc<TUnderlyingType<T>>{}(ToUnderlying(value));
        }
    };

    template <typename T> struct THashFunc<T*> {
        auto operator()(const T* value) const noexcept -> usize {
            return Detail::FoldToUsize(
                Detail::MixU64(static_cast<u64>(reinterpret_cast<usize>(value))));
        }
    };

    template <typename CharT> struct THashFunc<TBasicStringView<CharT>> {
        auto operator()(TBasicStringView<CharT> value) const noexcept -> usize {
            return Detail::HashBytes(value.Data(), value.Length() * sizeof(CharT));
        }
    };

    template <typename CharT> struct THashFunc<TBasicString<CharT>> {
        auto operator()(const TBasicString<CharT>& value) const noexcept -> usize {
            return THashFunc<TBasicStringView<CharT>>{}(value.ToView());
        }
    };
} // namespace AltinaEngine::Core::Container

namespace AltinaEngine {
    template <typename T>
    [[nodiscard]] inline auto GetInternalHash(const T& value) noexcept -> usize {
        return Core::Container::THashFunc<T>{}(value);
    }

    [[nodiscard]] inline auto InternalHashCombine(u64 seed, u64 value) noexcept -> u64 {
        return seed ^ (value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U));
    }
} // namespace AltinaEngine