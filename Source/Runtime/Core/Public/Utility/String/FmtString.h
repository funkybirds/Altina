#pragma once

#include "Container/String.h"
#include "Types/Traits.h"

#include <format>
#include <string_view>

using AltinaEngine::Forward;

namespace AltinaEngine::Core::Utility::String {
    namespace Detail {
        template <typename CharT>
        [[nodiscard]] constexpr auto SafeStringViewData(const CharT* data, usize length)
            -> const CharT* {
            static constexpr CharT kEmpty[1] = { static_cast<CharT>(0) };
            return (data == nullptr && length == 0) ? kEmpty : data;
        }
    } // namespace Detail

    /**
     * Formats arguments into an FString using std::format-compatible placeholders.
     */
    template <typename... Args>
    using TFormatString = TTypeSelect<CSameAs<TChar, wchar_t>, std::wformat_string<Args...>,
        std::format_string<Args...>>::Type;

    template <typename... Args>
    [[nodiscard]] inline auto FmtString(TFormatString<Args...> format, Args&&... args)
        -> Container::FString {
        const auto buffer = std::format(format, Forward<Args>(args)...);
        return Container::FString(buffer.data(), static_cast<usize>(buffer.size()));
    }
} // namespace AltinaEngine::Core::Utility::String

namespace std {
    template <typename CharT>
    struct formatter<AltinaEngine::Core::Container::TBasicStringView<CharT>, CharT> :
        formatter<std::basic_string_view<CharT>, CharT> {
        template <typename FormatContext>
        auto format(const AltinaEngine::Core::Container::TBasicStringView<CharT>& value,
            FormatContext&                                                        ctx) const {
            const std::basic_string_view<CharT> view(
                AltinaEngine::Core::Utility::String::Detail::SafeStringViewData(
                    value.Data(), value.Length()),
                value.Length());
            return formatter<std::basic_string_view<CharT>, CharT>::format(view, ctx);
        }
    };

    template <typename CharT>
    struct formatter<AltinaEngine::Core::Container::TBasicString<CharT>, CharT> :
        formatter<std::basic_string_view<CharT>, CharT> {
        template <typename FormatContext>
        auto format(const AltinaEngine::Core::Container::TBasicString<CharT>& value,
            FormatContext&                                                    ctx) const {
            const std::basic_string_view<CharT> view(
                AltinaEngine::Core::Utility::String::Detail::SafeStringViewData(
                    value.GetData(), value.Length()),
                value.Length());
            return formatter<std::basic_string_view<CharT>, CharT>::format(view, ctx);
        }
    };
} // namespace std
