#include "ShaderCompiler/ShaderCompiler.h"
#include "ShaderCompilerBackend.h"
#include "Container/HashMap.h"
#include "Container/SmartPtr.h"
#include "Container/String.h"
#include "Container/Vector.h"
#include "Threading/ConditionVariable.h"
#include "Threading/Mutex.h"

namespace AltinaEngine::ShaderCompiler {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::FStringView;
    using Container::MakeShared;
    using Container::THashFunc;
    using Container::THashMap;
    using Container::TShared;
    using Container::TVector;
    using Core::Threading::FConditionVariable;
    using Core::Threading::FMutex;
    using Core::Threading::FScopedLock;

    namespace {
        struct FShaderCompileCacheKey {
            FString               mPath;
            FString               mEntryPoint;
            EShaderStage          mStage    = EShaderStage::Vertex;
            EShaderSourceLanguage mLanguage = EShaderSourceLanguage::Hlsl;
            TVector<FString>      mIncludeDirs;
            TVector<FShaderMacro> mDefines;
            Rhi::ERhiBackend      mTargetBackend       = Rhi::ERhiBackend::Unknown;
            EShaderOptimization   mOptimization        = EShaderOptimization::Default;
            bool                  mDebugInfo           = false;
            bool                  mEnableBindless      = false;
            u32                   mVulkanSpace         = 0U;
            u32                   mVulkanConstantShift = 0U;
            u32                   mVulkanTextureShift  = 0U;
            u32                   mVulkanSamplerShift  = 0U;
            u32                   mVulkanStorageShift  = 0U;
            FString               mTargetProfile;
            FString               mCompilerPathOverride;
            FString               mShaderModelOverride;
            FShaderPermutationId  mPermutationId{};

            [[nodiscard]] auto    operator==(const FShaderCompileCacheKey& other) const noexcept
                -> bool {
                return mStage == other.mStage && mLanguage == other.mLanguage
                    && mTargetBackend == other.mTargetBackend
                    && mOptimization == other.mOptimization && mDebugInfo == other.mDebugInfo
                    && mEnableBindless == other.mEnableBindless
                    && mVulkanSpace == other.mVulkanSpace
                    && mVulkanConstantShift == other.mVulkanConstantShift
                    && mVulkanTextureShift == other.mVulkanTextureShift
                    && mVulkanSamplerShift == other.mVulkanSamplerShift
                    && mVulkanStorageShift == other.mVulkanStorageShift
                    && mPermutationId.mHash == other.mPermutationId.mHash && mPath == other.mPath
                    && mEntryPoint == other.mEntryPoint && mTargetProfile == other.mTargetProfile
                    && mCompilerPathOverride == other.mCompilerPathOverride
                    && mShaderModelOverride == other.mShaderModelOverride
                    && VectorsEqual(mIncludeDirs, other.mIncludeDirs)
                    && MacrosEqual(mDefines, other.mDefines);
            }

        private:
            template <typename T>
            [[nodiscard]] static auto VectorsEqual(
                const TVector<T>& lhs, const TVector<T>& rhs) noexcept -> bool {
                if (lhs.Size() != rhs.Size()) {
                    return false;
                }
                for (usize i = 0U; i < lhs.Size(); ++i) {
                    if (!(lhs[i] == rhs[i])) {
                        return false;
                    }
                }
                return true;
            }

            [[nodiscard]] static auto MacrosEqual(const TVector<FShaderMacro>& lhs,
                const TVector<FShaderMacro>& rhs) noexcept -> bool {
                if (lhs.Size() != rhs.Size()) {
                    return false;
                }
                for (usize i = 0U; i < lhs.Size(); ++i) {
                    if (!(lhs[i].mName == rhs[i].mName) || !(lhs[i].mValue == rhs[i].mValue)) {
                        return false;
                    }
                }
                return true;
            }
        };

        struct FShaderCompileCacheKeyHash {
            [[nodiscard]] static auto HashCombine(usize seed, usize value) noexcept -> usize {
                constexpr usize kMul = (sizeof(usize) == 8U) ? 0x9e3779b97f4a7c15ULL : 0x9e3779b9U;
                return seed ^ (value + kMul + (seed << 6U) + (seed >> 2U));
            }

            template <typename T>
            [[nodiscard]] static auto HashVector(const TVector<T>& values) noexcept -> usize {
                usize hash = THashFunc<usize>{}(values.Size());
                for (const auto& value : values) {
                    hash = HashCombine(hash, THashFunc<T>{}(value));
                }
                return hash;
            }

            [[nodiscard]] static auto HashMacros(const TVector<FShaderMacro>& values) noexcept
                -> usize {
                usize hash = THashFunc<usize>{}(values.Size());
                for (const auto& value : values) {
                    hash = HashCombine(hash, THashFunc<FString>{}(value.mName));
                    hash = HashCombine(hash, THashFunc<FString>{}(value.mValue));
                }
                return hash;
            }

            auto operator()(const FShaderCompileCacheKey& key) const noexcept -> usize {
                usize hash = THashFunc<FString>{}(key.mPath);
                hash       = HashCombine(hash, THashFunc<FString>{}(key.mEntryPoint));
                hash       = HashCombine(hash, THashFunc<EShaderStage>{}(key.mStage));
                hash       = HashCombine(hash, THashFunc<EShaderSourceLanguage>{}(key.mLanguage));
                hash       = HashCombine(hash, HashVector(key.mIncludeDirs));
                hash       = HashCombine(hash, HashMacros(key.mDefines));
                hash       = HashCombine(hash, THashFunc<Rhi::ERhiBackend>{}(key.mTargetBackend));
                hash       = HashCombine(hash, THashFunc<EShaderOptimization>{}(key.mOptimization));
                hash       = HashCombine(hash, THashFunc<bool>{}(key.mDebugInfo));
                hash       = HashCombine(hash, THashFunc<bool>{}(key.mEnableBindless));
                hash       = HashCombine(hash, THashFunc<u32>{}(key.mVulkanSpace));
                hash       = HashCombine(hash, THashFunc<u32>{}(key.mVulkanConstantShift));
                hash       = HashCombine(hash, THashFunc<u32>{}(key.mVulkanTextureShift));
                hash       = HashCombine(hash, THashFunc<u32>{}(key.mVulkanSamplerShift));
                hash       = HashCombine(hash, THashFunc<u32>{}(key.mVulkanStorageShift));
                hash       = HashCombine(hash, THashFunc<FString>{}(key.mTargetProfile));
                hash       = HashCombine(hash, THashFunc<FString>{}(key.mCompilerPathOverride));
                hash       = HashCombine(hash, THashFunc<FString>{}(key.mShaderModelOverride));
                hash       = HashCombine(hash, THashFunc<u64>{}(key.mPermutationId.mHash));
                return hash;
            }
        };

        struct FPendingShaderCompile {
            FMutex               mMutex{};
            FConditionVariable   mCondition{};
            bool                 mCompleted = false;
            FShaderCompileResult mResult{};
        };

        struct FShaderCompileCache {
            FMutex mMutex{};
            THashMap<FShaderCompileCacheKey, FShaderCompileResult, FShaderCompileCacheKeyHash>
                mEntries;
            THashMap<FShaderCompileCacheKey, TShared<FPendingShaderCompile>,
                FShaderCompileCacheKeyHash>
                mPendingEntries;
        };

        auto BuildCompileCacheKey(const FShaderCompileRequest& request) -> FShaderCompileCacheKey {
            FShaderCompileCacheKey key{};
            key.mPath                 = request.mSource.mPath;
            key.mEntryPoint           = request.mSource.mEntryPoint;
            key.mStage                = request.mSource.mStage;
            key.mLanguage             = request.mSource.mLanguage;
            key.mIncludeDirs          = request.mSource.mIncludeDirs;
            key.mDefines              = request.mSource.mDefines;
            key.mTargetBackend        = request.mOptions.mTargetBackend;
            key.mOptimization         = request.mOptions.mOptimization;
            key.mDebugInfo            = request.mOptions.mDebugInfo;
            key.mEnableBindless       = request.mOptions.mEnableBindless;
            key.mVulkanSpace          = request.mOptions.mVulkanBinding.mSpace;
            key.mVulkanConstantShift  = request.mOptions.mVulkanBinding.mConstantBufferShift;
            key.mVulkanTextureShift   = request.mOptions.mVulkanBinding.mTextureShift;
            key.mVulkanSamplerShift   = request.mOptions.mVulkanBinding.mSamplerShift;
            key.mVulkanStorageShift   = request.mOptions.mVulkanBinding.mStorageShift;
            key.mTargetProfile        = request.mOptions.mTargetProfile;
            key.mCompilerPathOverride = request.mOptions.mCompilerPathOverride;
            key.mShaderModelOverride  = request.mOptions.mShaderModelOverride;
            key.mPermutationId        = request.mPermutationId;
            return key;
        }

        auto GetCompileCache() -> FShaderCompileCache& {
            static FShaderCompileCache sCache{};
            return sCache;
        }

        class FShaderCompiler final : public IShaderCompiler {
        public:
            auto Compile(const FShaderCompileRequest& request) -> FShaderCompileResult override;

            void CompileAsync(
                const FShaderCompileRequest& request, FOnShaderCompiled onCompleted) override;

        private:
            auto SelectBackend(const FShaderCompileRequest& request, FString& diagnostics)
                -> Detail::IShaderCompilerBackend*;

        private:
            Detail::FDxcCompilerBackend   mDxcBackend;
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

        if (request.mOptions.mTargetBackend == Rhi::ERhiBackend::DirectX11) {
            primary  = &mSlangBackend;
            fallback = &mDxcBackend;
        } else if (request.mOptions.mTargetBackend == Rhi::ERhiBackend::Vulkan) {
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
        const auto                     cacheKey = BuildCompileCacheKey(request);
        TShared<FPendingShaderCompile> pendingCompile;
        bool                           shouldCompile = false;
        {
            auto&       cache = GetCompileCache();
            FScopedLock lock(cache.mMutex);
            if (const auto it = cache.mEntries.FindIt(cacheKey); it != cache.mEntries.end()) {
                return it->second;
            }
            if (const auto it = cache.mPendingEntries.FindIt(cacheKey);
                it != cache.mPendingEntries.end()) {
                pendingCompile = it->second;
            } else {
                pendingCompile = MakeShared<FPendingShaderCompile>();
                cache.mPendingEntries.InsertOrAssign(cacheKey, pendingCompile);
                shouldCompile = true;
            }
        }

        if (!shouldCompile) {
            FScopedLock pendingLock(pendingCompile->mMutex);
            while (!pendingCompile->mCompleted) {
                pendingCompile->mCondition.Wait(pendingCompile->mMutex);
            }
            return pendingCompile->mResult;
        }

        FShaderCompileResult result;
        FString              selectionNotes;
        auto*                backend = SelectBackend(request, selectionNotes);
        if (backend == nullptr) {
            result.mStage       = request.mSource.mStage;
            result.mSucceeded   = false;
            result.mDiagnostics = selectionNotes;
        } else {
            result = backend->Compile(request);
            AppendText(result.mDiagnostics, selectionNotes);
        }

        {
            FScopedLock pendingLock(pendingCompile->mMutex);
            pendingCompile->mResult    = result;
            pendingCompile->mCompleted = true;
        }
        {
            auto&       cache = GetCompileCache();
            FScopedLock lock(cache.mMutex);
            cache.mEntries.InsertOrAssign(cacheKey, result);
            cache.mPendingEntries.Remove(cacheKey);
        }
        pendingCompile->mCondition.NotifyAll();
        return result;
    }

    void FShaderCompiler::CompileAsync(
        const FShaderCompileRequest& request, FOnShaderCompiled onCompleted) {
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
