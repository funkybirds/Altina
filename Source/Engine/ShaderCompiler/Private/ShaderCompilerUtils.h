#pragma once

#include "Container/String.h"

namespace AltinaEngine::ShaderCompiler::Detail {
    using Core::Container::FString;

    auto BuildTempOutputPath(const FString& sourcePath, const FString& suffix,
        const FString& extension) -> FString;

    void AppendDiagnostic(FString& diagnostics, const FString& text);
    void AppendDiagnosticLine(FString& diagnostics, const TChar* line);

} // namespace AltinaEngine::ShaderCompiler::Detail
