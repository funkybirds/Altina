#pragma once

#include "RenderCoreAPI.h"

#include "Container/HashMap.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Rhi/RhiRefs.h"
#include "Rhi/RhiShader.h"
#include "Shader/ShaderPermutation.h"
#include "Shader/ShaderTypes.h"
#include "Threading/Mutex.h"
#include "Types/Aliases.h"

namespace AltinaEngine::RenderCore {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::FStringView;
    using Container::THashMap;
    using Core::Threading::FMutex;
    using Core::Threading::FScopedLock;
    using Shader::EShaderStage;
    using Shader::FShaderPermutationId;

    class AE_RENDER_CORE_API FShaderRegistry {
    public:
        struct FShaderKey {
            FString              Name;
            EShaderStage         Stage       = EShaderStage::Vertex;
            FShaderPermutationId Permutation = {};

            [[nodiscard]] auto   IsValid() const noexcept -> bool { return !Name.IsEmptyString(); }

            friend auto          operator==(const FShaderKey& lhs, const FShaderKey& rhs) noexcept
                -> bool = default;
        };

        FShaderRegistry() = default;

        void               Clear();

        [[nodiscard]] auto GetEntryCount() const noexcept -> usize;
        [[nodiscard]] auto Contains(const FShaderKey& key) const noexcept -> bool;
        [[nodiscard]] auto FindShader(const FShaderKey& key) const noexcept -> Rhi::FRhiShaderRef;

        [[nodiscard]] auto RegisterShader(FShaderKey key, Rhi::FRhiShaderRef shader) -> bool;
        [[nodiscard]] auto RemoveShader(const FShaderKey& key) -> bool;

        static auto        MakeKey(FStringView name, EShaderStage stage,
                   FShaderPermutationId permutation = {}) -> FShaderKey;
        static auto        MakeAssetKey(FStringView assetPath, FStringView entry, EShaderStage stage,
                   FShaderPermutationId permutation = {}) -> FShaderKey;

    private:
        struct FShaderKeyHash {
            auto operator()(const FShaderKey& key) const noexcept -> usize {
                size_t hash = std::hash<FString>{}(key.Name);
                hash        = HashCombine(hash, static_cast<size_t>(key.Stage));
                hash        = HashCombine(hash, static_cast<size_t>(key.Permutation.mHash));
                return static_cast<usize>(hash);
            }

        private:
            static constexpr auto HashCombine(size_t seed, size_t value) noexcept -> size_t {
                constexpr size_t kMul = (sizeof(size_t) == 8) ? 0x9e3779b97f4a7c15ULL : 0x9e3779b9U;
                return seed ^ (value + kMul + (seed << 6) + (seed >> 2));
            }
        };

        struct FShaderKeyEqual {
            auto operator()(const FShaderKey& lhs, const FShaderKey& rhs) const noexcept -> bool {
                return lhs == rhs;
            }
        };

        using FRegistryMap =
            THashMap<FShaderKey, Rhi::FRhiShaderRef, FShaderKeyHash, FShaderKeyEqual>;

        mutable FMutex mMutex;
        FRegistryMap   mEntries;
    };

} // namespace AltinaEngine::RenderCore
