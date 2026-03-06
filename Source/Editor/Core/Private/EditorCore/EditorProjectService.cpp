#include "EditorCore/EditorProjectService.h"

#include "Algorithm/CStringUtils.h"
#include "Logging/Log.h"
#include "Platform/PlatformFileSystem.h"
#include "Utility/Filesystem/Path.h"
#include "Utility/Json.h"
#include "Utility/String/CodeConvert.h"

#include <cstring>

namespace AltinaEngine::Editor::Core {
    namespace {
        namespace AECore = AltinaEngine::Core;
        using AltinaEngine::Core::Algorithm::ToLowerChar;
        using AltinaEngine::Core::Container::FNativeString;
        using AltinaEngine::Core::Container::FNativeStringView;
        using AltinaEngine::Core::Container::FString;
        using AltinaEngine::Core::Container::FStringView;
        using AltinaEngine::Core::Container::TVector;
        using AltinaEngine::Core::Utility::Filesystem::FPath;
        using AltinaEngine::Core::Utility::Json::FindObjectValue;
        using AltinaEngine::Core::Utility::Json::FJsonDocument;
        using AltinaEngine::Core::Utility::Json::GetStringValue;
        using AltinaEngine::Core::Utility::String::FromUtf8;

        auto SplitTokens(FNativeStringView commandLine, TVector<FNativeString>& outTokens) -> void {
            outTokens.Clear();
            const char* data   = commandLine.Data();
            const usize length = commandLine.Length();
            usize       index  = 0;
            while (index < length) {
                while (index < length && data[index] <= ' ') {
                    ++index;
                }
                if (index >= length) {
                    break;
                }

                FNativeString token;
                bool          inQuote = false;
                while (index < length) {
                    const char ch = data[index];
                    if (ch == '"') {
                        inQuote = !inQuote;
                        ++index;
                        continue;
                    }
                    if (!inQuote && data[index] <= ' ') {
                        break;
                    }
                    token.Append(ch);
                    ++index;
                }

                if (!token.IsEmptyString()) {
                    outTokens.PushBack(token);
                }
            }
        }

        auto StartsWithIgnoreCase(FNativeStringView text, const char* prefix) -> bool {
            if (prefix == nullptr) {
                return false;
            }
            const usize prefixLength = static_cast<usize>(std::strlen(prefix));
            if (text.Length() < prefixLength) {
                return false;
            }
            for (usize i = 0; i < prefixLength; ++i) {
                if (ToLowerChar(text[i]) != ToLowerChar(prefix[i])) {
                    return false;
                }
            }
            return true;
        }

        auto ParseStringOption(
            const TVector<FNativeString>& tokens, const char* prefix, FString& outValue) -> bool {
            for (const auto& token : tokens) {
                const auto view = token.ToView();
                if (!StartsWithIgnoreCase(view, prefix)) {
                    continue;
                }
                const usize   n = static_cast<usize>(std::strlen(prefix));
                FNativeString value;
                value.Append(view.Data() + n, view.Length() - n);
                outValue = FromUtf8(value);
                return true;
            }
            return false;
        }

        auto BuildFallbackProjectPath(FStringView demoName) -> FString {
            auto exeDir = AECore::Platform::GetExecutableDir();
            if (exeDir.IsEmptyString()) {
                return {};
            }
            FPath p(exeDir);
            p = (p / TEXT("../../../Demo") / demoName / TEXT("Config/EditorProject.json"))
                    .Normalized();
            return p.GetString();
        }

        auto MakeAbsoluteFromProjectFile(FStringView projectFilePath, FStringView value)
            -> FString {
            if (value.IsEmpty()) {
                return {};
            }

            FPath input(value);
            if (input.IsAbsolute()) {
                return input.Normalized().GetString();
            }

            FPath base(projectFilePath);
            base = base.ParentPath();
            base /= value;
            return base.Normalized().GetString();
        }
    } // namespace

    auto FEditorProjectService::LoadFromCommandLine(
        FStringView commandLine, FEditorProjectSettings& outSettings) const -> bool {
        outSettings = {};

        TVector<FNativeString> tokens;
        {
            const auto cmdUtf8 = AECore::Utility::String::ToUtf8Bytes(FString(commandLine));
            SplitTokens(cmdUtf8.ToView(), tokens);
        }

        FString explicitProjectPath;
        FString demoName(TEXT("Minimal"));
        ParseStringOption(tokens, "-EditorProject=", explicitProjectPath);
        ParseStringOption(tokens, "-Demo=", demoName);

        FString projectPath;
        if (!explicitProjectPath.IsEmptyString()) {
            projectPath = explicitProjectPath;
        } else {
            auto exeDir = AECore::Platform::GetExecutableDir();
            if (!exeDir.IsEmptyString()) {
                FPath stagedPath(exeDir);
                stagedPath  = (stagedPath / TEXT("Assets/Config/EditorProject.json")).Normalized();
                projectPath = stagedPath.GetString();
            }
            if (projectPath.IsEmptyString() || !AECore::Platform::IsPathExist(projectPath)) {
                projectPath = BuildFallbackProjectPath(demoName);
            }
        }

        if (projectPath.IsEmptyString() || !AECore::Platform::IsPathExist(projectPath)) {
            LogWarning(TEXT("Editor project file not found; fallback to default world."));
            return false;
        }

        FNativeString jsonText;
        if (!AECore::Platform::ReadFileTextUtf8(projectPath, jsonText)) {
            LogWarning(TEXT("Failed to read editor project file: {}"), projectPath.ToView());
            return false;
        }

        FJsonDocument document;
        if (!document.Parse(jsonText.ToView())) {
            LogWarning(TEXT("Failed to parse editor project file: {}"), projectPath.ToView());
            return false;
        }

        const auto* root = document.GetRoot();
        if (root == nullptr) {
            return false;
        }

        FNativeString nativeValue;
        if (const auto* demoNameValue = FindObjectValue(*root, "DemoName");
            GetStringValue(demoNameValue, nativeValue)) {
            outSettings.DemoName = FromUtf8(nativeValue);
        } else {
            outSettings.DemoName = demoName;
        }

        if (const auto* moduleValue = FindObjectValue(*root, "DemoModule");
            GetStringValue(moduleValue, nativeValue)) {
            outSettings.DemoModulePath =
                MakeAbsoluteFromProjectFile(projectPath.ToView(), FromUtf8(nativeValue).ToView());
        }

        if (const auto* configValue = FindObjectValue(*root, "DefaultConfig");
            GetStringValue(configValue, nativeValue)) {
            outSettings.ConfigOverride = FromUtf8(nativeValue);
        }

        if (const auto* assetRootValue = FindObjectValue(*root, "AssetRoot");
            GetStringValue(assetRootValue, nativeValue)) {
            outSettings.AssetRootOverride =
                MakeAbsoluteFromProjectFile(projectPath.ToView(), FromUtf8(nativeValue).ToView());
        }

        outSettings.SourcePath = projectPath;
        outSettings.bLoaded    = !outSettings.DemoModulePath.IsEmptyString();
        return outSettings.bLoaded;
    }
} // namespace AltinaEngine::Editor::Core
