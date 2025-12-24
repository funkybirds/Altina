#pragma once

#include <format>
#include <string>
#include <type_traits>

#include "../Base/CoreAPI.h"
#include "../Types/Aliases.h"
#include "../Types/Traits.h"
#include "../Container/StringView.h"

namespace AltinaEngine::Core::Logging
{
    using AltinaEngine::Core::Container::TStringView;

    template <typename... Args>
    using TFormatString = std::conditional_t<std::is_same_v<TChar, wchar_t>,
                                             std::wformat_string<Args...>,
                                             std::format_string<Args...>>;

    enum class ELogLevel : AltinaEngine::u8
    {
        Trace = 0,
        Debug,
        Info,
        Warning,
        Error,
        Fatal
    };

    using FLogSink = void (*)(ELogLevel Level,
                              TStringView Category,
                              TStringView Message,
                              void* UserData);

    class AE_CORE_API FLogger
    {
    public:
        static void SetLogLevel(ELogLevel Level) noexcept;
        static ELogLevel GetLogLevel() noexcept;

        static void SetLogSink(FLogSink Sink, void* UserData = nullptr);
        static void ResetLogSink();

        static void Log(ELogLevel Level,
                        TStringView Category,
                        TStringView Message);

        static void Log(ELogLevel Level,
                        TStringView Message)
        {
            Log(Level, GetDefaultCategory(), Message);
        }

        static void SetDefaultCategory(TStringView Category);
        static void ResetDefaultCategory() noexcept;
        static TStringView GetDefaultCategory() noexcept;

        template <typename... Args>
        static void Logf(ELogLevel Level,
                 TStringView Category,
                 TFormatString<Args...> Format,
                         Args&&... args)
        {
            if (!ShouldLog(Level))
            {
                return;
            }

            const std::basic_string<AltinaEngine::TChar> Buffer = std::format(
                Format, AltinaEngine::Forward<Args>(args)...);
            Dispatch(Level, Category,
                     TStringView(Buffer.data(), static_cast<usize>(Buffer.size())));
        }

    private:
        static bool ShouldLog(ELogLevel Level) noexcept;
        static void Dispatch(ELogLevel Level,
                             TStringView Category,
                             TStringView Message);
    };

    template <typename... Args>
    inline void LogInfoCategory(TStringView Category,
                                TFormatString<Args...> Format,
                                Args&&... args)
    {
        FLogger::Logf(ELogLevel::Info,
                      Category,
                      Format,
                      AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogInfo(TFormatString<Args...> Format,
                        Args&&... args)
    {
        LogInfoCategory(FLogger::GetDefaultCategory(),
                        Format,
                        AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogInfoCat(TStringView Category,
                           TFormatString<Args...> Format,
                           Args&&... args)
    {
        LogInfoCategory(Category, Format, AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogWarningCategory(TStringView Category,
                                   TFormatString<Args...> Format,
                                   Args&&... args)
    {
        FLogger::Logf(ELogLevel::Warning,
                      Category,
                      Format,
                      AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogWarning(TFormatString<Args...> Format,
                           Args&&... args)
    {
        LogWarningCategory(FLogger::GetDefaultCategory(),
                           Format,
                           AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogWarningCat(TStringView Category,
                              TFormatString<Args...> Format,
                              Args&&... args)
    {
        LogWarningCategory(Category, Format, AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogErrorCategory(TStringView Category,
                                 TFormatString<Args...> Format,
                                 Args&&... args)
    {
        FLogger::Logf(ELogLevel::Error,
                      Category,
                      Format,
                      AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogError(TFormatString<Args...> Format,
                         Args&&... args)
    {
        LogErrorCategory(FLogger::GetDefaultCategory(),
                         Format,
                         AltinaEngine::Forward<Args>(args)...);
    }

    template <typename... Args>
    inline void LogErrorCat(TStringView Category,
                            TFormatString<Args...> Format,
                            Args&&... args)
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
}
