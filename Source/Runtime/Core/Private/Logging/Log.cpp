#include "Logging/Log.h"

#include "../../Public/Threading/Atomic.h"
#include <iostream>
#include "../../Public/Threading/Mutex.h"
#include <string>
#include <type_traits>

namespace Container = AltinaEngine::Core::Container;
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

        Threading::FAtomicInt32               gMinimumLevel(static_cast<int32_t>(ELogLevel::Info));
        FLogSink                              gUserSink = nullptr;
        void*                                 gUserData = nullptr;
        Threading::FMutex                     gLogMutex;
        thread_local std::basic_string<TChar> gThreadDefaultCategory;
        thread_local bool                     gThreadHasCustomCategory = false;

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

    void FLogger::Log(ELogLevel Level, FStringView Category, FStringView Message) {
        if (!ShouldLog(Level)) {
            return;
        }

        Dispatch(Level, Category, Message);
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
        FLogSink sinkCopy     = nullptr;
        void*    userDataCopy = nullptr;
        {
            Threading::FScopedLock lock(gLogMutex);
            sinkCopy     = gUserSink;
            userDataCopy = gUserData;
        }

        if (sinkCopy) {
            sinkCopy(Level, Category, Message, userDataCopy);
        } else {
            DefaultSink(Level, Category, Message, nullptr);
        }
    }

} // namespace AltinaEngine::Core::Logging
