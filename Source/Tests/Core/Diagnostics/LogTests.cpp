#include <utility>
#include <vector>

#include "TestHarness.h"

#include "Container/String.h"
#include "Logging/Log.h"

using AltinaEngine::Core::Container::FString;
using AltinaEngine::Core::Container::FStringView;
using namespace AltinaEngine::Core::Logging;

namespace {
    struct FCapturedLog {
        ELogLevel Level;
        FString   Category;
        FString   Message;
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

    AltinaEngine::LogInfo(TEXT("Value {}"), 42);

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

    AltinaEngine::LogInfo(TEXT("Skip me"));
    REQUIRE_EQ(Captured.size(), 0U);

    AltinaEngine::LogError(TEXT("Emit {}"), TEXT("!"));
    REQUIRE_EQ(Captured.size(), 1U);
    REQUIRE_EQ(Captured[0].Level, ELogLevel::Error);

    FLogger::ResetLogSink();
    FLogger::SetLogLevel(ELogLevel::Info);
    FLogger::ResetDefaultCategory();
}
