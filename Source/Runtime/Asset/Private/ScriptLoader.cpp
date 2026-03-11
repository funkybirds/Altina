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
            return Container::MakeSharedAs<IAsset, TDerived>(Forward<Args>(args)...);
        }
    } // namespace

    auto FScriptLoader::CanLoad(EAssetType type) const noexcept -> bool {
        return type == EAssetType::Script;
    }

    auto FScriptLoader::Load(const FAssetDesc& desc, IAssetStream& /*stream*/) -> TShared<IAsset> {
        if (desc.mScript.mTypeName.IsEmptyString()) {
            return {};
        }

        return MakeSharedAsset<FScriptAsset>(desc.mScript.mAssemblyPath, desc.mScript.mTypeName);
    }
} // namespace AltinaEngine::Asset
