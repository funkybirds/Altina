#pragma once

#include <format>
#include <string>
#include "../Types/Traits.h"

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"
#include "../Types/Traits.h"
#include "../Container/StringView.h"

namespace AltinaEngine::Core::Logging
{
    template <bool B, typename TTrue, typename TFalse> struct TSelect
    {
        using Type = TTrue; // NOLINT
    };
    template <typename TTrue, typename TFalse> struct TSelect<false, TTrue, TFalse>
    {
        using Type = TFalse; // NOLINT
    };

    using Container::TStringView;

    template <typename... Args>
    using TFormatString =
        TSelect<TTypeSameAs_v<TChar, wchar_t>, std::wformat_string<Args...>, std::format_string<Args...>>::Type;

    enum class ELogLevel : u8
    {
        Trace = 0,
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    using FLogSink = void (*)(
        ELogLevel Level, TStringView Category, TStringView Message, void* UserData); // NOLINT(*-identifier-naming)

    class AE_CORE_API FLogger
    {
    public:
        static void SetLogLevel(ELogLevel Level) noexcept;
        static auto GetLogLevel() noexcept -> ELogLevel;

        static void SetLogSink(FLogSink Sink, void* UserData = nullptr);
        static void ResetLogSink();

        static void Log(ELogLevel Level, TStringView Category, TStringView Message);

        static void Log(ELogLevel Level, TStringView Message) { Log(Level, GetDefaultCategory(), Message); }

        static void SetDefaultCategory(TStringView Category);
        static void ResetDefaultCategory() noexcept;
        static auto GetDefaultCategory() noexcept -> TStringView;

        template <typename... Args>
        static void Logf(ELogLevel Level, TStringView Category, TFormatString<Args...> Format, Args&&... args)
        {
            if (!ShouldLog(Level))
            {
                return;
            }

            const std::basic_string<AltinaEngine::TChar> buffer =
                std::format(Format, AltinaEngine::Forward<Args>(args)...);
            Dispatch(Level, Category, TStringView(buffer.data(), static_cast<usize>(buffer.size())));
        }

    private:
        static auto ShouldLog(ELogLevel Level) noexcept -> bool;
        static void Dispatch(ELogLevel Level, TStringView Category, TStringView Message);
    };

    template <typename... Args>
    void LogInfoCategory(TStringView Category, TFormatString<Args...> Format, Args&&... args)
    {
        FLogger::Logf(ELogLevel::Info, Category, Format, AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args> void LogInfo(TFormatString<Args...> Format, Args&&... args)
    {
        LogInfoCategory(FLogger::GetDefaultCategory(), Format, AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args> void LogInfoCat(TStringView Category, TFormatString<Args...> Format, Args&&... args)
    {
        LogInfoCategory(Category, Format, AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args>
    void LogWarningCategory(TStringView Category, TFormatString<Args...> Format, Args&&... args)
    {
        FLogger::Logf(ELogLevel::Warning, Category, Format, AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args> void LogWarning(TFormatString<Args...> Format, Args&&... args)
    {
        LogWarningCategory(FLogger::GetDefaultCategory(), Format, AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args> void LogWarningCat(TStringView Category, TFormatString<Args...> Format, Args&&... args)
    {
        LogWarningCategory(Category, Format, AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args>
    void LogErrorCategory(TStringView Category, TFormatString<Args...> Format, Args&&... args)
    {
        FLogger::Logf(ELogLevel::Error, Category, Format, AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args> void LogError(TFormatString<Args...> Format, Args&&... args)
    {
        LogErrorCategory(FLogger::GetDefaultCategory(), Format, AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args> void LogErrorCat(TStringView Category, TFormatString<Args...> Format, Args&&... args)
    {
        LogErrorCategory(Category, Format, AltinaEngine::Forward<Args>(args)...);
    }

} // namespace AltinaEngine::Core::Logging

namespace AltinaEngine
{
    namespace Logging = Core::Logging;

    using Logging::ELogLevel;
    using Logging::FLogger;
    using Logging::LogError;
    using Logging::LogErrorCat;
    using Logging::LogErrorCategory;
    using Logging::LogInfo;
    using Logging::LogInfoCat;
    using Logging::LogInfoCategory;
    using Logging::LogWarning;
    using Logging::LogWarningCat;
    using Logging::LogWarningCategory;
    using Logging::TFormatString;
} // namespace AltinaEngine
