#pragma once

#include "Container/String.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::ShaderCompiler::Detail {
    using Core::Container::FString;
    using Core::Container::FNativeString;
    using Core::Container::TVector;

    struct FProcessOutput {
        bool    mSucceeded = false;
        u32     mExitCode  = 0;
        FString mOutput;
    };

    auto RunProcess(const FString& exePath, const TVector<FString>& args) -> FProcessOutput;

    auto ReadFileBytes(const FString& path, TVector<u8>& outBytes) -> bool;
    auto ReadFileTextUtf8(const FString& path, FNativeString& outText) -> bool;

    auto BuildTempOutputPath(const FString& sourcePath, const FString& suffix,
        const FString& extension) -> FString;

    void RemoveFileIfExists(const FString& path);
    void AppendDiagnostic(FString& diagnostics, const FString& text);
    void AppendDiagnosticLine(FString& diagnostics, const TChar* line);

} // namespace AltinaEngine::ShaderCompiler::Detail
