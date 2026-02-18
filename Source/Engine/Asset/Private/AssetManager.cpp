#include "Asset/AssetManager.h"

#include "Platform/PlatformFileSystem.h"
#include <cstring>

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;
    using Container::TVector;
    using Core::Platform::ReadFileBytes;

    namespace {
        class FMemoryAssetStream final : public IAssetStream {
        public:
            explicit FMemoryAssetStream(const TVector<u8>& data)
                : mData(data.Data()), mSize(data.Size()) {}

            [[nodiscard]] auto Size() const noexcept -> usize override { return mSize; }
            [[nodiscard]] auto Tell() const noexcept -> usize override { return mOffset; }

            void               Seek(usize offset) noexcept override {
                mOffset = (offset > mSize) ? mSize : offset;
            }

            auto Read(void* outBuffer, usize bytesToRead) -> usize override {
                if (outBuffer == nullptr || bytesToRead == 0U || mData == nullptr) {
                    return 0U;
                }

                const usize remaining = mSize - mOffset;
                const usize toRead    = (bytesToRead > remaining) ? remaining : bytesToRead;
                if (toRead == 0U) {
                    return 0U;
                }

                std::memcpy(outBuffer, mData + mOffset, static_cast<size_t>(toRead));
                mOffset += toRead;
                return toRead;
            }

        private:
            const u8* mData   = nullptr;
            usize     mSize   = 0;
            usize     mOffset = 0;
        };

        auto HandlesMatch(const FAssetHandle& left, const FAssetHandle& right) -> bool {
            return left.Uuid == right.Uuid && left.Type == right.Type;
        }
    } // namespace

    FAssetManager::FAssetManager() = default;

    void FAssetManager::SetRegistry(const FAssetRegistry* registry) noexcept {
        mRegistry = registry;
    }

    void FAssetManager::RegisterLoader(IAssetLoader* loader) {
        if (loader == nullptr) {
            return;
        }

        mLoaders.PushBack(loader);
    }

    void FAssetManager::UnregisterLoader(IAssetLoader* loader) {
        if (loader == nullptr || mLoaders.IsEmpty()) {
            return;
        }

        for (usize index = 0; index < mLoaders.Size(); ++index) {
            if (mLoaders[index] == loader) {
                const usize lastIndex = mLoaders.Size() - 1;
                if (index != lastIndex) {
                    mLoaders[index] = mLoaders[lastIndex];
                }
                mLoaders.PopBack();
                return;
            }
        }
    }

    auto FAssetManager::Load(const FAssetHandle& handle) -> TShared<IAsset> {
        if (mRegistry == nullptr || !handle.IsValid()) {
            return {};
        }

        const FAssetHandle resolved = mRegistry->ResolveRedirector(handle);
        if (!resolved.IsValid()) {
            return {};
        }

        if (TShared<IAsset> cached = FindLoaded(resolved)) {
            return cached;
        }

        const FAssetDesc* desc = mRegistry->GetDesc(resolved);
        if (desc == nullptr) {
            return {};
        }

        IAssetLoader* loader = FindLoader(desc->Handle.Type);
        if (loader == nullptr) {
            return {};
        }

        TVector<u8> bytes;
        if (!desc->CookedPath.IsEmptyString()) {
            if (!ReadFileBytes(desc->CookedPath, bytes)) {
                return {};
            }
        } else if (desc->Handle.Type != EAssetType::Script) {
            return {};
        }

        FMemoryAssetStream stream(bytes);
        TShared<IAsset>    asset = loader->Load(*desc, stream);
        if (asset) {
            mCache.PushBack({ resolved, asset });
        }

        return asset;
    }

    void FAssetManager::Unload(const FAssetHandle& handle) {
        const isize index = FindCacheIndex(handle);
        if (index < 0) {
            return;
        }

        const usize lastIndex   = mCache.Size() - 1;
        const usize removeIndex = static_cast<usize>(index);
        if (removeIndex != lastIndex) {
            mCache[removeIndex] = mCache[lastIndex];
        }
        mCache.PopBack();
    }

    void FAssetManager::ClearCache() { mCache.Clear(); }

    auto FAssetManager::FindLoaded(const FAssetHandle& handle) const -> TShared<IAsset> {
        const isize index = FindCacheIndex(handle);
        if (index < 0) {
            return {};
        }

        return mCache[static_cast<usize>(index)].Asset;
    }

    auto FAssetManager::FindLoader(EAssetType type) const noexcept -> IAssetLoader* {
        for (auto* loader : mLoaders) {
            if (loader != nullptr && loader->CanLoad(type)) {
                return loader;
            }
        }

        return nullptr;
    }

    auto FAssetManager::FindCacheIndex(const FAssetHandle& handle) const noexcept -> isize {
        if (!handle.IsValid()) {
            return -1;
        }

        for (usize index = 0; index < mCache.Size(); ++index) {
            if (HandlesMatch(mCache[index].Handle, handle)) {
                return static_cast<isize>(index);
            }
        }

        return -1;
    }

} // namespace AltinaEngine::Asset
