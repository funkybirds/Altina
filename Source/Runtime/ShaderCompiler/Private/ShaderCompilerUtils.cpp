#include "ShaderCompilerUtils.h"
#include "Platform/PlatformFileSystem.h"
#include "Utility/Filesystem/Path.h"
#include "Utility/Filesystem/PathUtils.h"

#include <atomic>

using AltinaEngine::Core::Container::TBasicString;
namespace AltinaEngine::ShaderCompiler::Detail {
    namespace Container = Core::Container;
    namespace {} // namespace

    void AppendDiagnosticLine(FString& diagnostics, const TChar* line) {
        if ((line == nullptr) || (line[0] == static_cast<TChar>(0))) {
            return;
        }
        if (!diagnostics.IsEmptyString()) {
            diagnostics.Append(TEXT("\n"));
        }
        diagnostics.Append(line);
    }

    void AppendDiagnostic(FString& diagnostics, const FString& text) {
        if (text.IsEmptyString()) {
            return;
        }
        if (!diagnostics.IsEmptyString()) {
            diagnostics.Append(TEXT("\n"));
        }
        diagnostics.Append(text.GetData(), text.Length());
    }

    auto BuildTempOutputPath(
        const FString& sourcePath, const FString& suffix, const FString& extension) -> FString {
        static std::atomic<u32> counter{ 0 };

        auto                    dir = Core::Utility::Filesystem::GetTempDirectory();
        if (dir.IsEmpty()) {
            dir = Core::Utility::Filesystem::GetCurrentWorkingDir();
        }
        dir /= TEXT("AltinaEngine");
        dir /= TEXT("ShaderCompile");
        Core::Platform::CreateDirectories(dir.GetString());

        Core::Utility::Filesystem::FPath basePath(sourcePath);
        auto                             stem = basePath.Stem();
        if (stem.IsEmpty()) {
            stem = FString(TEXT("shader"));
        }

        const auto uniqueId = counter.fetch_add(1, std::memory_order_relaxed);
        FString    filename = stem;
        filename.Append(TEXT("_"));
        filename.Append(FString::ToString(uniqueId));
        filename.Append(TEXT("_"));
        filename.Append(suffix);
        filename.Append(extension);

        const Core::Utility::Filesystem::FPath fullPath = dir / filename;
        return fullPath.GetString();
    }

} // namespace AltinaEngine::ShaderCompiler::Detail
