#include "Utility/Uuid.h"

#include <random>

namespace AltinaEngine {
    namespace {
        template <typename CharT>
        constexpr auto HexToNibble(CharT value, u8& out) noexcept -> bool {
            const auto zero = static_cast<CharT>('0');
            const auto nine = static_cast<CharT>('9');
            const auto a    = static_cast<CharT>('a');
            const auto f    = static_cast<CharT>('f');
            const auto A    = static_cast<CharT>('A');
            const auto F    = static_cast<CharT>('F');

            if (value >= zero && value <= nine) {
                out = static_cast<u8>(value - zero);
                return true;
            }
            if (value >= a && value <= f) {
                out = static_cast<u8>(value - a + 10);
                return true;
            }
            if (value >= A && value <= F) {
                out = static_cast<u8>(value - A + 10);
                return true;
            }
            return false;
        }

        template <typename CharT>
        auto TryParseImpl(Core::Container::TBasicStringView<CharT> text, FUuid& out) noexcept
            -> bool {
            const auto length = text.Length();
            if (length != FUuid::kCompactStringLength && length != FUuid::kStringLength) {
                return false;
            }

            const bool hasHyphens = length == FUuid::kStringLength;
            if (hasHyphens) {
                if (text[8] != static_cast<CharT>('-') || text[13] != static_cast<CharT>('-')
                    || text[18] != static_cast<CharT>('-')
                    || text[23] != static_cast<CharT>('-')) {
                    return false;
                }
            }

            FUuid::FBytes bytes{};
            usize         byteIndex = 0;
            for (usize i = 0; i < length;) {
                if (hasHyphens && (i == 8 || i == 13 || i == 18 || i == 23)) {
                    ++i;
                    continue;
                }

                if (i + 1 >= length) {
                    return false;
                }

                u8 hi = 0;
                u8 lo = 0;
                if (!HexToNibble(text[i], hi) || !HexToNibble(text[i + 1], lo)) {
                    return false;
                }

                bytes[byteIndex] = static_cast<u8>((hi << 4) | lo);
                ++byteIndex;
                i += 2;
            }

            if (byteIndex != FUuid::kByteCount) {
                return false;
            }

            out = FUuid(bytes);
            return true;
        }

        template <typename CharT>
        void AppendHex(Core::Container::TBasicString<CharT>& out, u8 value) {
            constexpr char kDigits[] = "0123456789abcdef";
            out.Append(static_cast<CharT>(kDigits[(value >> 4) & 0xF]));
            out.Append(static_cast<CharT>(kDigits[value & 0xF]));
        }

        template <typename CharT>
        auto ToStringImpl(const FUuid& value) -> Core::Container::TBasicString<CharT> {
            Core::Container::TBasicString<CharT> out;
            out.Reserve(FUuid::kStringLength);

            const auto& bytes = value.GetBytes();
            for (usize i = 0; i < FUuid::kByteCount; ++i) {
                if (i == 4 || i == 6 || i == 8 || i == 10) {
                    out.Append(static_cast<CharT>('-'));
                }
                AppendHex(out, bytes[i]);
            }

            return out;
        }
    } // namespace

    auto FUuid::New() -> FUuid {
        std::random_device rd;
        FBytes             bytes{};
        for (usize i = 0; i < kByteCount; ++i) {
            bytes[i] = static_cast<u8>(rd());
        }

        bytes[6] = static_cast<u8>((bytes[6] & 0x0F) | 0x40);
        bytes[8] = static_cast<u8>((bytes[8] & 0x3F) | 0x80);

        return FUuid(bytes);
    }

#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
    auto FUuid::TryParse(Core::Container::FStringView text, FUuid& out) noexcept -> bool {
        return TryParseImpl(text, out);
    }
#endif

    auto FUuid::TryParse(Core::Container::FNativeStringView text, FUuid& out) noexcept -> bool {
        return TryParseImpl(text, out);
    }

    auto FUuid::ToString() const -> Core::Container::FString {
        return ToStringImpl<TChar>(*this);
    }

    auto FUuid::ToNativeString() const -> Core::Container::FNativeString {
        return ToStringImpl<char>(*this);
    }

} // namespace AltinaEngine
