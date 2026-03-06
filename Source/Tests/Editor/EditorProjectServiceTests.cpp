#include "TestHarness.h"

#include "EditorCore/EditorProjectService.h"
#include "Utility/String/CodeConvert.h"

TEST_CASE("EditorProjectService loads explicit project file from command line") {
    using AltinaEngine::Core::Container::FString;
    using AltinaEngine::Core::Utility::String::FromUtf8Bytes;

    const auto projectPath = FromUtf8Bytes(AE_SOURCE_DIR "/Demo/Minimal/Config/EditorProject.json",
        sizeof(AE_SOURCE_DIR "/Demo/Minimal/Config/EditorProject.json") - 1U);

    FString    commandLine(TEXT("-EditorProject="));
    commandLine.Append(projectPath.CStr(), projectPath.Length());
    commandLine.Append(TEXT(" -Demo=Minimal"));

    AltinaEngine::Editor::Core::FEditorProjectService  service{};
    AltinaEngine::Editor::Core::FEditorProjectSettings settings{};
    REQUIRE(service.LoadFromCommandLine(commandLine.ToView(), settings));
    REQUIRE(settings.bLoaded);
    REQUIRE(!settings.DemoModulePath.IsEmptyString());
    REQUIRE(!settings.AssetRootOverride.IsEmptyString());
}
