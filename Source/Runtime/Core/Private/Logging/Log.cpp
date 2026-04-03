#include "Logging/Log.h"

#include "../../Public/Threading/Atomic.h"
#include <iostream>
#include "../../Public/Threading/Mutex.h"
#include "Container/String.h"
#include <string>
#include <string_view>
#include <type_traits>

#if __has_include(<stacktrace>)
    #include <stacktrace>
    #define AE_HAS_STD_STACKTRACE 1
#else
    #define AE_HAS_STD_STACKTRACE 0
#endif

namespace Container = AltinaEngine::Core::Container;
using Container::FString;
using Container::FStringView;

namespace AltinaEngine::Core::Logging {
    namespace {
        template <usize N> constexpr auto LiteralView(const TChar (&Text)[N]) -> FStringView {
            return { Text, N > 0 ? N - 1 : 0 }; // remove null terminator
        }

        constexpr TChar                       kDefaultCategory[] = TEXT("Default");
        constexpr TChar                       kTraceLabel[]      = TEXT("TRACE");
        constexpr TChar                       kDebugLabel[]      = TEXT("DEBUG");
        constexpr TChar                       kInfoLabel[]       = TEXT("INFO");
        constexpr TChar                       kWarningLabel[]    = TEXT("WARN");
        constexpr TChar                       kErrorLabel[]      = TEXT("ERROR");
        constexpr TChar                       kFatalLabel[]      = TEXT("FATAL");

        // ANSI escape sequences for colored console output.
        // Keep this platform-agnostic (no platform headers in Core).
        constexpr TChar                       kAnsiReset[]  = TEXT("\x1b[0m");
        constexpr TChar                       kAnsiRed[]    = TEXT("\x1b[31m");
        constexpr TChar                       kAnsiYellow[] = TEXT("\x1b[33m");

        Threading::FAtomicInt32               gMinimumLevel(static_cast<int32_t>(ELogLevel::Info));
        FLogSink                              gUserSink = nullptr;
        void*                                 gUserData = nullptr;
        Threading::FMutex                     gLogMutex;
        thread_local std::basic_string<TChar> gThreadDefaultCategory;
        thread_local bool                     gThreadHasCustomCategory = false;

        constexpr TChar                       kStackTraceHeader[] = TEXT("StackTrace:");

        auto ShouldEmitStackTrace(const ELogLevel Level) noexcept -> bool {
            return (Level == ELogLevel::Error) || (Level == ELogLevel::Fatal);
        }

        void AppendAscii(FString& Out, const std::string_view Text) {
            for (const char ch : Text) {
                Out.Append(static_cast<TChar>(ch));
            }
        }

        void AppendStackTrace(FString& Out) {
#if AE_HAS_STD_STACKTRACE && defined(__cpp_lib_stacktrace)
            // Keep the output bounded; ERROR/FATAL are rare, so a little extra work is OK.
            constexpr usize kSkipFrames = 3;  // skip logger frames
            constexpr usize kMaxFrames  = 32; // keep logs readable

            const auto      trace = std::stacktrace::current(kSkipFrames, kMaxFrames);
            if (trace.empty()) {
                return;
            }

            Out.Append(TEXT('\n'));
            Out.Append(LiteralView(kStackTraceHeader));
            Out.Append(TEXT('\n'));

            usize index = 0;
            for (const auto& entry : trace) {
                Out.Append(TEXT("  #"));
                Out.AppendNumber(index++);
                Out.Append(TEXT(": "));

                AppendAscii(Out, entry.description());

                const auto file = entry.source_file();
                if (!file.empty()) {
                    Out.Append(TEXT(" ("));
                    AppendAscii(Out, file);
                    Out.Append(TEXT(':'));
                    Out.AppendNumber(entry.source_line());
                    Out.Append(TEXT(')'));
                }

                Out.Append(TEXT('\n'));
            }
#else
            (void)Out;
#endif
        }

        auto LevelToLabel(const ELogLevel Level) noexcept -> FStringView {
            switch (Level) {
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

        auto LevelToAnsiColor(const ELogLevel Level) noexcept -> FStringView {
            switch (Level) {
                case ELogLevel::Warning:
                    return LiteralView(kAnsiYellow);
                case ELogLevel::Error:
                case ELogLevel::Fatal:
                    return LiteralView(kAnsiRed);
                default:
                    return {};
            }
        }

        auto ResolveCategory(FStringView Category) noexcept -> FStringView {
            const auto defaultTag = LiteralView(kDefaultCategory);
            if (Category.IsEmpty() || Category == defaultTag) {
                return FLogger::GetDefaultCategory();
            }
            return Category;
        }

        auto ConsoleStream() noexcept -> std::basic_ostream<TChar>& {
            if constexpr (std::is_same_v<TChar, wchar_t>) {
                return std::wcout;
            } else {
                return std::cout;
            }
        }

        void DefaultSink(ELogLevel Level, FStringView Category, FStringView Message, void*) {
            auto&             stream = ConsoleStream();
            const FStringView label  = LevelToLabel(Level);
            const FStringView color  = LevelToAnsiColor(Level);

            if (!color.IsEmpty()) {
                stream.write(color.Data(), static_cast<std::streamsize>(color.Length()));
            }

            stream << TEXT('[');
            if (!label.IsEmpty()) {
                stream.write(label.Data(), static_cast<std::streamsize>(label.Length()));
            }
            stream << TEXT(']') << TEXT('[');
            if (!Category.IsEmpty()) {
                stream.write(Category.Data(), static_cast<std::streamsize>(Category.Length()));
            }
            stream << TEXT(']') << TEXT(' ');
            if (!Message.IsEmpty()) {
                stream.write(Message.Data(), static_cast<std::streamsize>(Message.Length()));
            }

            if (!color.IsEmpty()) {
                const auto reset = LiteralView(kAnsiReset);
                stream.write(reset.Data(), static_cast<std::streamsize>(reset.Length()));
            }

            stream << TEXT('\n');
            stream.flush();
        }
    } // namespace

    void FLogger::SetLogLevel(ELogLevel Level) noexcept {
        gMinimumLevel.Store(static_cast<int32_t>(Level));
    }

    auto FLogger::GetLogLevel() noexcept -> ELogLevel {
        return static_cast<ELogLevel>(gMinimumLevel.Load());
    }

    void FLogger::SetLogSink(FLogSink Sink, void* UserData) {
        AltinaEngine::Core::Threading::FScopedLock lock(gLogMutex);
        gUserSink = Sink;
        gUserData = UserData;
    }

    void FLogger::ResetLogSink() { SetLogSink(nullptr, nullptr); }

    auto FLogger::HasCustomLogSink() noexcept -> bool { return gUserSink != nullptr; }

    void FLogger::Log(ELogLevel Level, FStringView Category, FStringView Message) {
        if (!ShouldLog(Level)) {
            return;
        }

        Dispatch(Level, Category, Message);
    }

    void FLogger::LogToDefaultSink(ELogLevel Level, FStringView Category, FStringView Message) {
        if (!ShouldLog(Level)) {
            return;
        }
        const auto resolvedCategory = ResolveCategory(Category);

        if (!ShouldEmitStackTrace(Level)) {
            DefaultSink(Level, resolvedCategory, Message, nullptr);
            return;
        }

        FString composed;
        composed.Append(Message);
        AppendStackTrace(composed);
        DefaultSink(Level, resolvedCategory, composed.ToView(), nullptr);
    }

    auto FLogger::ShouldLog(ELogLevel Level) noexcept -> bool {
        return Level >= static_cast<ELogLevel>(gMinimumLevel.Load());
    }

    void FLogger::SetDefaultCategory(FStringView Category) {
        if (Category.IsEmpty()) {
            ResetDefaultCategory();
            return;
        }

        gThreadDefaultCategory.assign(Category.Data(), Category.Length());
        gThreadHasCustomCategory = true;
    }

    void FLogger::ResetDefaultCategory() noexcept {
        gThreadDefaultCategory.clear();
        gThreadHasCustomCategory = false;
    }

    auto FLogger::GetDefaultCategory() noexcept -> FStringView {
        if (gThreadHasCustomCategory && !gThreadDefaultCategory.empty()) {
            return { gThreadDefaultCategory.data(),
                static_cast<usize>(gThreadDefaultCategory.size()) };
        }

        return LiteralView(kDefaultCategory);
    }

    void FLogger::Dispatch(ELogLevel Level, FStringView Category, FStringView Message) {
        const auto resolvedCategory = ResolveCategory(Category);
        FLogSink   sinkCopy         = nullptr;
        void*      userDataCopy     = nullptr;
        {
            Threading::FScopedLock lock(gLogMutex);
            sinkCopy     = gUserSink;
            userDataCopy = gUserData;
        }

        if (!ShouldEmitStackTrace(Level)) {
            if (sinkCopy) {
                sinkCopy(Level, resolvedCategory, Message, userDataCopy);
            } else {
                DefaultSink(Level, resolvedCategory, Message, nullptr);
            }
            return;
        }

        FString composed;
        composed.Append(Message);
        AppendStackTrace(composed);
        const auto view = composed.ToView();

        if (sinkCopy) {
            sinkCopy(Level, resolvedCategory, view, userDataCopy);
        } else {
            DefaultSink(Level, resolvedCategory, view, nullptr);
        }
    }

} // namespace AltinaEngine::Core::Logging
