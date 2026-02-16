#include "Shader/ShaderRegistry.h"

#include "Types/Traits.h"

namespace AltinaEngine::RenderCore {
    auto FShaderRegistry::MakeKey(FStringView name, EShaderStage stage,
        FShaderPermutationId permutation) -> FShaderKey {
        FShaderKey key{};
        key.Name.Assign(name);
        key.Stage       = stage;
        key.Permutation = permutation;
        return key;
    }

    void FShaderRegistry::Clear() {
        FScopedLock lock(mMutex);
        mEntries.clear();
    }

    auto FShaderRegistry::GetEntryCount() const noexcept -> usize {
        FScopedLock lock(mMutex);
        return static_cast<usize>(mEntries.size());
    }

    auto FShaderRegistry::Contains(const FShaderKey& key) const noexcept -> bool {
        if (!key.IsValid()) {
            return false;
        }
        FScopedLock lock(mMutex);
        return mEntries.find(key) != mEntries.end();
    }

    auto FShaderRegistry::FindShader(const FShaderKey& key) const noexcept -> Rhi::FRhiShaderRef {
        if (!key.IsValid()) {
            return {};
        }
        FScopedLock lock(mMutex);
        if (const auto it = mEntries.find(key); it != mEntries.end()) {
            return it->second;
        }
        return {};
    }

    auto FShaderRegistry::RegisterShader(FShaderKey key, Rhi::FRhiShaderRef shader) -> bool {
        if (!key.IsValid() || !shader) {
            return false;
        }
        FScopedLock lock(mMutex);
        mEntries.insert_or_assign(Move(key), Move(shader));
        return true;
    }

    auto FShaderRegistry::RemoveShader(const FShaderKey& key) -> bool {
        if (!key.IsValid()) {
            return false;
        }
        FScopedLock lock(mMutex);
        return mEntries.erase(key) > 0;
    }

} // namespace AltinaEngine::RenderCore
