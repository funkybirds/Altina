#include "Engine/GameScene/ScriptComponent.h"

#include "Asset/AssetManager.h"
#include "Asset/ScriptAsset.h"
#include "Logging/Log.h"

namespace AltinaEngine::GameScene {
    namespace {
        AltinaEngine::Asset::FAssetManager* gScriptAssetManager = nullptr;

        auto ToFStringFromUtf8(Core::Container::FNativeStringView text)
            -> Core::Container::FString {
            Core::Container::FString out;
            if (text.IsEmpty()) {
                return out;
            }
#if defined(AE_UNICODE) || defined(UNICODE) || defined(_UNICODE)
            out.Reserve(text.Length());
            for (AltinaEngine::usize i = 0; i < text.Length(); ++i) {
                out.Append(
                    static_cast<AltinaEngine::TChar>(static_cast<unsigned char>(text.Data()[i])));
            }
#else
            out.Append(text.Data(), text.Length());
#endif
            return out;
        }
    } // namespace

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
        mScriptAsset   = handle;
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

        mManagedHandle   = 0;
        mCreatedCalled   = false;
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
        if (!mLoggedTick) {
            mLoggedTick = true;
            LogInfoCat(TEXT("Scripting.Managed"), TEXT("ScriptComponent Tick entered."));
        }

        if (!TryCreateInstance()) {
            if (!mLoggedCreateFailure) {
                mLoggedCreateFailure = true;
                LogWarningCat(TEXT("Scripting.Managed"),
                    TEXT("ScriptComponent Tick skipped: managed instance not created."));
            }
            return;
        }

        EnsureOnCreateInvoked();

        const auto* api = Scripting::GetManagedApi();
        if (api && api->Tick && mManagedHandle != 0) {
            if (!mLoggedCreate) {
                mLoggedCreate = true;
                LogInfoCat(TEXT("Scripting.Managed"),
                    TEXT("ScriptComponent Tick forwarded to managed (handle={})."), mManagedHandle);
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
                if (!mLoggedResolveFailure) {
                    mLoggedResolveFailure = true;
                    LogWarningCat(TEXT("Scripting.Managed"),
                        TEXT("ScriptComponent RefreshFromAsset failed."));
                }
                return false;
            }
        }

        if (mTypeName.IsEmptyString()) {
            if (!mLoggedCreateFailure) {
                mLoggedCreateFailure = true;
                LogWarningCat(
                    TEXT("Scripting.Managed"), TEXT("ScriptComponent missing managed type name."));
            }
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

        const auto owner      = GetOwner();
        args.mOwnerIndex      = owner.Index;
        args.mOwnerGeneration = owner.Generation;
        args.mWorldId         = owner.WorldId;

        mManagedHandle = api->CreateInstance(&args);
        if (mManagedHandle == 0) {
            if (!mLoggedCreateFailure) {
                mLoggedCreateFailure = true;
                LogWarningCat(
                    TEXT("Scripting.Managed"), TEXT("ScriptComponent CreateInstance returned 0."));
            }
            return false;
        }
        return true;
    }

    auto FScriptComponent::RefreshFromAsset() -> bool {
        if (!mScriptAsset.IsValid()) {
            return false;
        }
        if (mScriptAsset.Type != AltinaEngine::Asset::EAssetType::Script) {
            if (!mLoggedResolveFailure) {
                mLoggedResolveFailure = true;
                LogWarningCat(
                    TEXT("Scripting.Managed"), TEXT("ScriptComponent asset type is not Script."));
            }
            return false;
        }
        if (mAssetResolved) {
            return !mTypeName.IsEmptyString();
        }

        auto* manager = GetAssetManager();
        if (manager == nullptr) {
            if (!mLoggedResolveFailure) {
                mLoggedResolveFailure = true;
                LogWarningCat(
                    TEXT("Scripting.Managed"), TEXT("ScriptComponent asset manager is null."));
            }
            return false;
        }

        auto asset = manager->Load(mScriptAsset);
        if (!asset) {
            if (!mLoggedResolveFailure) {
                mLoggedResolveFailure = true;
                LogWarningCat(
                    TEXT("Scripting.Managed"), TEXT("ScriptComponent asset load failed."));
            }
            return false;
        }

        auto* scriptAsset = static_cast<AltinaEngine::Asset::FScriptAsset*>(asset.Get());
        if (scriptAsset == nullptr) {
            if (!mLoggedResolveFailure) {
                mLoggedResolveFailure = true;
                LogWarningCat(TEXT("Scripting.Managed"),
                    TEXT("ScriptComponent asset is not a script asset instance."));
            }
            return false;
        }

        const auto assemblyPath = scriptAsset->GetAssemblyPath();
        const auto typeName     = scriptAsset->GetTypeName();
        if (typeName.IsEmpty()) {
            if (!mLoggedResolveFailure) {
                mLoggedResolveFailure = true;
                LogWarningCat(TEXT("Scripting.Managed"),
                    TEXT("ScriptComponent script asset missing type name."));
            }
            return false;
        }

        mAssemblyPath.Assign(assemblyPath);
        mTypeName.Assign(typeName);
        mAssetResolved = true;
        if (!mLoggedResolved) {
            mLoggedResolved         = true;
            const auto assemblyText = ToFStringFromUtf8(mAssemblyPath.ToView());
            const auto typeText     = ToFStringFromUtf8(mTypeName.ToView());
            LogInfoCat(TEXT("Scripting.Managed"),
                TEXT("ScriptComponent resolved asset: assembly='{}' type='{}'"),
                assemblyText.ToView(), typeText.ToView());
        }
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
