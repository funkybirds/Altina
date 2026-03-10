#include "Shader/ShaderRegistry.h"

#include "Types/Traits.h"
#include "Rhi/RhiShader.h"

namespace AltinaEngine::RenderCore {
    auto FShaderRegistry::MakeKey(
        FStringView name, EShaderStage stage, FShaderPermutationId permutation) -> FShaderKey {
        FShaderKey key{};
        key.mName.Assign(name);
        key.mStage       = stage;
        key.mPermutation = permutation;
        return key;
    }

    auto FShaderRegistry::MakeAssetKey(FStringView assetPath, FStringView entry, EShaderStage stage,
        FShaderPermutationId permutation) -> FShaderKey {
        FString name;
        name.Assign(assetPath);
        if (!entry.IsEmpty()) {
            name.Append(TEXT(":"));
            name.Append(entry);
        }
        return MakeKey(name.ToView(), stage, permutation);
    }

    void FShaderRegistry::Clear() {
        FScopedLock lock(mMutex);
        mEntries.Clear();
    }

    auto FShaderRegistry::GetEntryCount() const noexcept -> usize {
        FScopedLock lock(mMutex);
        return mEntries.Num();
    }

    auto FShaderRegistry::Contains(const FShaderKey& key) const noexcept -> bool {
        if (!key.IsValid()) {
            return false;
        }
        FScopedLock lock(mMutex);
        return mEntries.FindIt(key) != mEntries.end();
    }

    auto FShaderRegistry::FindShader(const FShaderKey& key) const noexcept -> Rhi::FRhiShaderRef {
        if (!key.IsValid()) {
            return {};
        }
        FScopedLock lock(mMutex);
        if (const auto it = mEntries.FindIt(key); it != mEntries.end()) {
            return it->second;
        }
        return {};
    }

    auto FShaderRegistry::RegisterShader(FShaderKey key, Rhi::FRhiShaderRef shader) -> bool {
        if (!key.IsValid() || !shader) {
            return false;
        }
        FScopedLock lock(mMutex);
        mEntries.InsertOrAssign(Move(key), Move(shader));
        return true;
    }

    auto FShaderRegistry::RemoveShader(const FShaderKey& key) -> bool {
        if (!key.IsValid()) {
            return false;
        }
        FScopedLock lock(mMutex);
        return mEntries.Erase(key) > 0;
    }

} // namespace AltinaEngine::RenderCore
