#include "Logging/Log.h"

#include <atomic>
#include <iostream>
#include <mutex>
#include <type_traits>

using AltinaEngine::Core::Container::TStringView;

namespace AltinaEngine::Core::Logging
{
    namespace
    {
        template <AltinaEngine::usize N>
        constexpr TStringView LiteralView(const AltinaEngine::TChar (&Text)[N])
        {
            return { Text, N > 0 ? N - 1 : 0 }; // remove null terminator
        }

        constexpr AltinaEngine::TChar kDefaultCategory[] = TEXT("Default");
        constexpr AltinaEngine::TChar kTraceLabel[] = TEXT("TRACE");
        constexpr AltinaEngine::TChar kDebugLabel[] = TEXT("DEBUG");
        constexpr AltinaEngine::TChar kInfoLabel[] = TEXT("INFO");
        constexpr AltinaEngine::TChar kWarningLabel[] = TEXT("WARN");
        constexpr AltinaEngine::TChar kErrorLabel[] = TEXT("ERROR");
        constexpr AltinaEngine::TChar kFatalLabel[] = TEXT("FATAL");

        std::atomic<ELogLevel> GMinimumLevel{ ELogLevel::Info };
        FLogSink GUserSink = nullptr;
        void* GUserData = nullptr;
        std::mutex GLogMutex;

        TStringView LevelToLabel(ELogLevel Level) noexcept
        {
            switch (Level)
            {
                case ELogLevel::Trace:
                    return LiteralView(kTraceLabel);
                case ELogLevel::Debug:
                    return LiteralView(kDebugLabel);
                case ELogLevel::Info:
                    return LiteralView(kInfoLabel);
                case ELogLevel::Warning:
                    return LiteralView(kWarningLabel);
                case ELogLevel::Error:
                    return LiteralView(kErrorLabel);
                case ELogLevel::Fatal:
                default:
                    return LiteralView(kFatalLabel);
            }
        }

        std::basic_ostream<AltinaEngine::TChar>& ConsoleStream() noexcept
        {
            if constexpr (std::is_same_v<AltinaEngine::TChar, wchar_t>)
            {
                return std::wcout;
            }
            else
            {
                return std::cout;
            }
        }

        void DefaultSink(ELogLevel Level,
                         TStringView Category,
                         TStringView Message,
                         void*)
        {
            auto& Stream = ConsoleStream();
            const TStringView Label = LevelToLabel(Level);

            Stream << TEXT('[');
            if (!Label.IsEmpty())
            {
                Stream.write(Label.Data(), static_cast<std::streamsize>(Label.Length()));
            }
            Stream << TEXT(']') << TEXT('[');
            if (!Category.IsEmpty())
            {
                Stream.write(Category.Data(), static_cast<std::streamsize>(Category.Length()));
            }
            Stream << TEXT(']') << TEXT(' ');
            if (!Message.IsEmpty())
            {
                Stream.write(Message.Data(), static_cast<std::streamsize>(Message.Length()));
            }
            Stream << TEXT('\n');
            Stream.flush();
        }
    } // namespace

    void FLogger::SetLogLevel(ELogLevel Level) noexcept
    {
        GMinimumLevel.store(Level, std::memory_order_relaxed);
    }

    ELogLevel FLogger::GetLogLevel() noexcept
    {
        return GMinimumLevel.load(std::memory_order_relaxed);
    }

    void FLogger::SetLogSink(FLogSink Sink, void* UserData)
    {
        std::scoped_lock Lock(GLogMutex);
        GUserSink = Sink;
        GUserData = UserData;
    }

    void FLogger::ResetLogSink()
    {
        SetLogSink(nullptr, nullptr);
    }

    void FLogger::Log(ELogLevel Level,
                      TStringView Category,
                      TStringView Message)
    {
        if (!ShouldLog(Level))
        {
            return;
        }

        Dispatch(Level, Category, Message);
    }

    bool FLogger::ShouldLog(ELogLevel Level) noexcept
    {
        return Level >= GMinimumLevel.load(std::memory_order_relaxed);
    }

    TStringView FLogger::GetDefaultCategory() noexcept
    {
        return LiteralView(kDefaultCategory);
    }

    void FLogger::Dispatch(ELogLevel Level,
                           TStringView Category,
                           TStringView Message)
    {
        FLogSink SinkCopy = nullptr;
        void* UserDataCopy = nullptr;
        {
            std::scoped_lock Lock(GLogMutex);
            SinkCopy = GUserSink;
            UserDataCopy = GUserData;
        }

        if (SinkCopy)
        {
            SinkCopy(Level, Category, Message, UserDataCopy);
        }
        else
        {
            DefaultSink(Level, Category, Message, nullptr);
        }
    }

} // namespace AltinaEngine::Core::Logging
