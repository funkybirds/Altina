#include "ShaderCompiler/ShaderCompiler.h"
#include "ShaderCompilerBackend.h"
#include "Container/String.h"

namespace AltinaEngine::ShaderCompiler {
    using Core::Container::FString;
    using Core::Container::FStringView;

    namespace {
        class FShaderCompiler final : public IShaderCompiler {
        public:
            auto Compile(const FShaderCompileRequest& request) -> FShaderCompileResult override;

            void CompileAsync(const FShaderCompileRequest& request,
                FOnShaderCompiled onCompleted) override;

        private:
            auto SelectBackend(const FShaderCompileRequest& request, FString& diagnostics)
                -> Detail::IShaderCompilerBackend*;

        private:
            Detail::FDxcCompilerBackend  mDxcBackend;
            Detail::FSlangCompilerBackend mSlangBackend;
        };

        void AppendLine(FString& dst, const TChar* line) {
            if ((line == nullptr) || (line[0] == static_cast<TChar>(0))) {
                return;
            }
            if (!dst.IsEmptyString()) {
                dst.Append(TEXT("\n"));
            }
            dst.Append(line);
        }

        void AppendText(FString& dst, const FString& text) {
            if (text.IsEmptyString()) {
                return;
            }
            if (!dst.IsEmptyString()) {
                dst.Append(TEXT("\n"));
            }
            dst.Append(text.GetData(), text.Length());
        }

        void AppendBackendStatus(FString& dst, const Detail::IShaderCompilerBackend& backend) {
            const auto name = backend.GetDisplayName();
            if (name.IsEmpty()) {
                return;
            }
            if (!dst.IsEmptyString()) {
                dst.Append(TEXT("\n"));
            }
            dst.Append(name.Data(), name.Length());
            dst.Append(TEXT(": "));
            dst.Append(backend.IsAvailable() ? TEXT("available") : TEXT("disabled"));
        }
    } // namespace

    auto FShaderCompiler::SelectBackend(const FShaderCompileRequest& request, FString& diagnostics)
        -> Detail::IShaderCompilerBackend* {
        Detail::IShaderCompilerBackend* primary  = nullptr;
        Detail::IShaderCompilerBackend* fallback = nullptr;

        if (request.mOptions.mTargetBackend == Rhi::ERhiBackend::Vulkan) {
            primary  = &mSlangBackend;
            fallback = &mDxcBackend;
        } else if (request.mSource.mLanguage == EShaderSourceLanguage::Slang) {
            primary  = &mSlangBackend;
            fallback = &mDxcBackend;
        } else {
            primary  = &mDxcBackend;
            fallback = &mSlangBackend;
        }

        if (primary->IsAvailable()) {
            return primary;
        }

        if (fallback->IsAvailable()) {
            AppendLine(diagnostics,
                TEXT("Preferred shader compiler backend unavailable; using fallback."));
            return fallback;
        }

        AppendLine(diagnostics, TEXT("No shader compiler backend available."));
        AppendBackendStatus(diagnostics, mDxcBackend);
        AppendBackendStatus(diagnostics, mSlangBackend);
        return nullptr;
    }

    auto FShaderCompiler::Compile(const FShaderCompileRequest& request) -> FShaderCompileResult {
        FString selectionNotes;
        auto*   backend = SelectBackend(request, selectionNotes);
        if (backend == nullptr) {
            FShaderCompileResult result;
            result.mSucceeded   = false;
            result.mDiagnostics = selectionNotes;
            return result;
        }

        auto result = backend->Compile(request);
        AppendText(result.mDiagnostics, selectionNotes);
        return result;
    }

    void FShaderCompiler::CompileAsync(const FShaderCompileRequest& request,
        FOnShaderCompiled onCompleted) {
        auto result = Compile(request);
        if (onCompleted) {
            onCompleted(result);
        }
    }

    auto GetShaderCompiler() -> IShaderCompiler& {
        static FShaderCompiler compiler;
        return compiler;
    }

} // namespace AltinaEngine::ShaderCompiler
