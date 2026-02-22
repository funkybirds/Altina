#include "Asset/ScriptAsset.h"

using AltinaEngine::Move;

namespace AltinaEngine::Asset {
    FScriptAsset::FScriptAsset(FNativeString assemblyPath, FNativeString typeName)
        : mAssemblyPath(Move(assemblyPath)), mTypeName(Move(typeName)) {}

    auto FScriptAsset::GetAssemblyPath() const noexcept -> FNativeStringView {
        return mAssemblyPath.ToView();
    }

    auto FScriptAsset::GetTypeName() const noexcept -> FNativeStringView {
        return mTypeName.ToView();
    }
} // namespace AltinaEngine::Asset
