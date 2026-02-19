#include "Asset/ScriptLoader.h"

#include "Asset/ScriptAsset.h"
#include "Types/Traits.h"

using AltinaEngine::Forward;
using AltinaEngine::Core::Container::DestroyPolymorphic;

namespace AltinaEngine::Asset {
    namespace Container = Core::Container;

    namespace {
        template <typename TDerived, typename... Args>
        auto MakeSharedAsset(Args&&... args) -> TShared<IAsset> {
            using Container::kSmartPtrUseManagedAllocator;
            using Container::TAllocator;
            using Container::TAllocatorTraits;
            using Container::TPolymorphicDeleter;
            using Container::TShared;

            TDerived* ptr = nullptr;
            if constexpr (kSmartPtrUseManagedAllocator) {
                TAllocator<TDerived> allocator;
                ptr = TAllocatorTraits<TAllocator<TDerived>>::Allocate(allocator, 1);
                if (ptr == nullptr) {
                    return {};
                }

                try {
                    TAllocatorTraits<TAllocator<TDerived>>::Construct(
                        allocator, ptr, Forward<Args>(args)...);
                } catch (...) {
                    TAllocatorTraits<TAllocator<TDerived>>::Deallocate(allocator, ptr, 1);
                    return {};
                }
            } else {
                ptr = new TDerived(Forward<Args>(args)...); // NOLINT
            }

            return TShared<IAsset>(
                ptr, TPolymorphicDeleter<IAsset>(&DestroyPolymorphic<IAsset, TDerived>));
        }
    } // namespace

    auto FScriptLoader::CanLoad(EAssetType type) const noexcept -> bool {
        return type == EAssetType::Script;
    }

    auto FScriptLoader::Load(const FAssetDesc& desc, IAssetStream& /*stream*/) -> TShared<IAsset> {
        if (desc.Script.TypeName.IsEmptyString()) {
            return {};
        }

        return MakeSharedAsset<FScriptAsset>(desc.Script.AssemblyPath, desc.Script.TypeName);
    }
} // namespace AltinaEngine::Asset
