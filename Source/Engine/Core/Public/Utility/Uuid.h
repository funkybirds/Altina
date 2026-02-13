#pragma once

#include "Base/CoreAPI.h"
#include "Container/Array.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Types/Aliases.h"

namespace AltinaEngine {
    namespace Container = Core::Container;

    struct AE_CORE_API FUuid {
        static constexpr usize kByteCount           = 16;
        static constexpr usize kCompactStringLength = 32;
        static constexpr usize kStringLength        = 36;

        using FBytes = Container::TArray<u8, kByteCount>;

        constexpr FUuid() = default;
        constexpr explicit FUuid(const FBytes& bytes) noexcept : mBytes(bytes) {}

        [[nodiscard]] static auto           New() -> FUuid;
        [[nodiscard]] static constexpr auto Nil() noexcept -> FUuid { return FUuid{}; }
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
        [[nodiscard]] static auto TryParse(Container::FStringView text, FUuid& out) noexcept
            -> bool;
#endif
        [[nodiscard]] static auto TryParse(Container::FNativeStringView text, FUuid& out) noexcept
            -> bool;

        [[nodiscard]] constexpr auto IsNil() const noexcept -> bool {
            for (usize i = 0; i < kByteCount; ++i) {
                if (mBytes[i] != 0U) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] constexpr auto GetBytes() const noexcept -> const FBytes& { return mBytes; }
        [[nodiscard]] constexpr auto Data() const noexcept -> const u8* { return mBytes.Data(); }

        [[nodiscard]] auto           ToString() const -> Container::FString;
        [[nodiscard]] auto           ToNativeString() const -> Container::FNativeString;

        [[nodiscard]] constexpr auto operator==(const FUuid& other) const noexcept -> bool {
            for (usize i = 0; i < kByteCount; ++i) {
                if (mBytes[i] != other.mBytes[i]) {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] constexpr auto operator!=(const FUuid& other) const noexcept -> bool {
            return !(*this == other);
        }

    private:
        FBytes mBytes{};
    };

} // namespace AltinaEngine
