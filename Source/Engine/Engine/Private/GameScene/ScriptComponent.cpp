#include "Engine/GameScene/ScriptComponent.h"

#include "Asset/AssetManager.h"
#include "Asset/ScriptAsset.h"
#include "Logging/Log.h"

namespace AltinaEngine::GameScene {
    namespace {
        AltinaEngine::Asset::FAssetManager* gScriptAssetManager = nullptr;
    }

    void FScriptComponent::SetAssetManager(AltinaEngine::Asset::FAssetManager* manager) {
        gScriptAssetManager = manager;
    }

    auto FScriptComponent::GetAssetManager() noexcept -> AltinaEngine::Asset::FAssetManager* {
        return gScriptAssetManager;
    }

    void FScriptComponent::SetAssemblyPath(Core::Container::FNativeStringView path) {
        mAssemblyPath.Assign(path);
        mAssetResolved = false;
    }

    void FScriptComponent::SetTypeName(Core::Container::FNativeStringView typeName) {
        mTypeName.Assign(typeName);
        mAssetResolved = false;
    }

    void FScriptComponent::SetScriptAsset(AltinaEngine::Asset::FAssetHandle handle) {
        mScriptAsset = handle;
        mAssetResolved = false;
    }

    auto FScriptComponent::GetAssemblyPath() const noexcept -> Core::Container::FNativeStringView {
        return mAssemblyPath.ToView();
    }

    auto FScriptComponent::GetTypeName() const noexcept -> Core::Container::FNativeStringView {
        return mTypeName.ToView();
    }

    auto FScriptComponent::GetScriptAsset() const noexcept -> AltinaEngine::Asset::FAssetHandle {
        return mScriptAsset;
    }

    void FScriptComponent::OnCreate() {
        mCreatedCalled = true;
        if (TryCreateInstance()) {
            EnsureOnCreateInvoked();
        }
    }

    void FScriptComponent::OnDestroy() {
        const auto* api = Scripting::GetManagedApi();
        if (api && mManagedHandle != 0) {
            if (api->OnDestroy) {
                api->OnDestroy(mManagedHandle);
            }
            if (api->DestroyInstance) {
                api->DestroyInstance(mManagedHandle);
            }
        }

        mManagedHandle = 0;
        mCreatedCalled = false;
        mOnCreateInvoked = false;
    }

    void FScriptComponent::OnEnable() {
        if (!TryCreateInstance()) {
            return;
        }

        EnsureOnCreateInvoked();

        const auto* api = Scripting::GetManagedApi();
        if (api && api->OnEnable && mManagedHandle != 0) {
            api->OnEnable(mManagedHandle);
        }
    }

    void FScriptComponent::OnDisable() {
        const auto* api = Scripting::GetManagedApi();
        if (api && api->OnDisable && mManagedHandle != 0) {
            api->OnDisable(mManagedHandle);
        }
    }

    void FScriptComponent::Tick(float dt) {
        if (!TryCreateInstance()) {
            return;
        }

        EnsureOnCreateInvoked();

        const auto* api = Scripting::GetManagedApi();
        if (api && api->Tick && mManagedHandle != 0) {
            if (!mLoggedTick) {
                mLoggedTick = true;
                LogInfoCat(TEXT("Scripting.Managed"),
                    TEXT("ScriptComponent Tick invoked (handle={})."), mManagedHandle);
            }
            api->Tick(mManagedHandle, dt);
        }
    }

    auto FScriptComponent::TryCreateInstance() -> bool {
        if (mManagedHandle != 0) {
            return true;
        }

        if (mScriptAsset.IsValid()) {
            if (!RefreshFromAsset()) {
                return false;
            }
        }

        if (mTypeName.IsEmptyString()) {
            return false;
        }

        const auto* api = Scripting::GetManagedApi();
        if (!api || !api->CreateInstance) {
            return false;
        }

        Scripting::FManagedCreateArgs args{};
        if (!mAssemblyPath.IsEmptyString()) {
            args.mAssemblyPathUtf8 = mAssemblyPath.CStr();
        }
        args.mTypeNameUtf8 = mTypeName.CStr();

        const auto owner = GetOwner();
        args.mOwnerIndex = owner.Index;
        args.mOwnerGeneration = owner.Generation;
        args.mWorldId = owner.WorldId;

        mManagedHandle = api->CreateInstance(&args);
        return mManagedHandle != 0;
    }

    auto FScriptComponent::RefreshFromAsset() -> bool {
        if (!mScriptAsset.IsValid()) {
            return false;
        }
        if (mScriptAsset.Type != AltinaEngine::Asset::EAssetType::Script) {
            return false;
        }
        if (mAssetResolved) {
            return !mTypeName.IsEmptyString();
        }

        auto* manager = GetAssetManager();
        if (manager == nullptr) {
            return false;
        }

        auto asset = manager->Load(mScriptAsset);
        if (!asset) {
            return false;
        }

        auto* scriptAsset = static_cast<AltinaEngine::Asset::FScriptAsset*>(asset.Get());
        if (scriptAsset == nullptr) {
            return false;
        }

        const auto assemblyPath = scriptAsset->GetAssemblyPath();
        const auto typeName = scriptAsset->GetTypeName();
        if (typeName.IsEmpty()) {
            return false;
        }

        mAssemblyPath.Assign(assemblyPath);
        mTypeName.Assign(typeName);
        mAssetResolved = true;
        return true;
    }

    void FScriptComponent::EnsureOnCreateInvoked() {
        if (!mCreatedCalled || mOnCreateInvoked || mManagedHandle == 0) {
            return;
        }

        const auto* api = Scripting::GetManagedApi();
        if (api && api->OnCreate) {
            api->OnCreate(mManagedHandle);
            mOnCreateInvoked = true;
        }
    }
} // namespace AltinaEngine::GameScene
