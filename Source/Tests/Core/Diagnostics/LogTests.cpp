#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

#include "TestHarness.h"

#include "Container/String.h"
#include "Logging/Log.h"

namespace Container = AltinaEngine::Core::Container;
using Container::FString;
using Container::FStringView;
using namespace AltinaEngine::Core::Logging;
using AltinaEngine::TChar;

namespace {
    struct FCapturedLog {
        ELogLevel Level;
        FString   Category;
        FString   Message;
    };

    template <typename CharT> struct FStreamRedirect {
        std::basic_ostream<CharT>&   Stream;
        std::basic_streambuf<CharT>* OldBuffer;

        FStreamRedirect(std::basic_ostream<CharT>& InStream, std::basic_streambuf<CharT>* NewBuffer)
            : Stream(InStream), OldBuffer(InStream.rdbuf(NewBuffer)) {}

        ~FStreamRedirect() { Stream.rdbuf(OldBuffer); }

        FStreamRedirect(const FStreamRedirect&)            = delete;
        FStreamRedirect& operator=(const FStreamRedirect&) = delete;
    };

    void CaptureSink(ELogLevel Level, FStringView Category, FStringView Message, void* UserData) {
        auto*        Storage = static_cast<std::vector<FCapturedLog>*>(UserData);
        FCapturedLog Entry;
        Entry.Level    = Level;
        Entry.Category = FString(Category.Data(), Category.Length());
        Entry.Message  = FString(Message.Data(), Message.Length());
        Storage->push_back(std::move(Entry));
    }
} // namespace

TEST_CASE("Logger formats text via sink") {
    std::vector<FCapturedLog> Captured;
    FLogger::SetLogLevel(ELogLevel::Trace);
    FLogger::SetDefaultCategory(TEXT("Test"));
    FLogger::SetLogSink(&CaptureSink, &Captured);

    AltinaEngine::LogInfoCat(TEXT("Default"), TEXT("Value {}"), 42);

    REQUIRE_EQ(Captured.size(), 1U);
    REQUIRE_EQ(Captured[0].Level, ELogLevel::Info);
    const auto CategoryView = Captured[0].Category.ToView();
    REQUIRE_EQ(CategoryView.Length(), 4U);
    REQUIRE_EQ(CategoryView[0], TEXT('T'));
    REQUIRE_EQ(CategoryView[3], TEXT('t'));

    const auto MessageView = Captured[0].Message.ToView();
    REQUIRE_EQ(MessageView.Length(), 8U);
    REQUIRE_EQ(MessageView[6], TEXT('4'));
    REQUIRE_EQ(MessageView[7], TEXT('2'));

    FLogger::ResetLogSink();
    FLogger::ResetDefaultCategory();
}

TEST_CASE("Logger respects minimum log level") {
    std::vector<FCapturedLog> Captured;
    FLogger::SetLogSink(&CaptureSink, &Captured);
    FLogger::SetLogLevel(ELogLevel::Warning);
    FLogger::SetDefaultCategory(TEXT("Test"));

    AltinaEngine::LogInfoCat(TEXT("Default"), TEXT("Skip me"));
    REQUIRE_EQ(Captured.size(), 0U);

    AltinaEngine::LogErrorCat(TEXT("Default"), TEXT("Emit {}"), TEXT("!"));
    REQUIRE_EQ(Captured.size(), 1U);
    REQUIRE_EQ(Captured[0].Level, ELogLevel::Error);

    FLogger::ResetLogSink();
    FLogger::SetLogLevel(ELogLevel::Info);
    FLogger::ResetDefaultCategory();
}

TEST_CASE("Logger appends stacktrace for error and fatal") {
    std::vector<FCapturedLog> Captured;
    FLogger::SetLogLevel(ELogLevel::Trace);
    FLogger::SetDefaultCategory(TEXT("Test"));
    FLogger::SetLogSink(&CaptureSink, &Captured);

    AltinaEngine::LogErrorCat(TEXT("Default"), TEXT("Boom"));
    REQUIRE_EQ(Captured.size(), 1U);
    REQUIRE_EQ(Captured[0].Level, ELogLevel::Error);
    REQUIRE(Captured[0].Message.ToView().Contains(TEXT("StackTrace:")));

    Captured.clear();
    FLogger::Log(ELogLevel::Fatal, TEXT("Test"), TEXT("Kaboom"));
    REQUIRE_EQ(Captured.size(), 1U);
    REQUIRE_EQ(Captured[0].Level, ELogLevel::Fatal);
    REQUIRE(Captured[0].Message.ToView().Contains(TEXT("StackTrace:")));

    FLogger::ResetLogSink();
    FLogger::SetLogLevel(ELogLevel::Info);
    FLogger::ResetDefaultCategory();
}

TEST_CASE("Default sink emits ANSI colors for warning and error levels") {
    FLogger::SetLogLevel(ELogLevel::Trace);

#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
    auto& consoleStream = std::wcout;
#else
    auto& consoleStream = std::cout;
#endif

    {
        std::basic_ostringstream<TChar> capture;
        FStreamRedirect<TChar>          redirect(consoleStream, capture.rdbuf());

        FLogger::LogToDefaultSink(ELogLevel::Warning, TEXT("Test"), TEXT("Warn"));
        const auto out = capture.str();

        REQUIRE(out.find(TEXT("\x1b[33m")) != std::basic_string<TChar>::npos);
        REQUIRE(out.find(TEXT("\x1b[0m")) != std::basic_string<TChar>::npos);
    }

    {
        std::basic_ostringstream<TChar> capture;
        FStreamRedirect<TChar>          redirect(consoleStream, capture.rdbuf());

        FLogger::LogToDefaultSink(ELogLevel::Error, TEXT("Test"), TEXT("Err"));
        const auto out = capture.str();

        REQUIRE(out.find(TEXT("\x1b[31m")) != std::basic_string<TChar>::npos);
        REQUIRE(out.find(TEXT("\x1b[0m")) != std::basic_string<TChar>::npos);
    }

    {
        std::basic_ostringstream<TChar> capture;
        FStreamRedirect<TChar>          redirect(consoleStream, capture.rdbuf());

        FLogger::LogToDefaultSink(ELogLevel::Info, TEXT("Test"), TEXT("Info"));
        const auto out = capture.str();

        REQUIRE(out.find(TEXT("\x1b[")) == std::basic_string<TChar>::npos);
    }

    FLogger::SetLogLevel(ELogLevel::Info);
}
