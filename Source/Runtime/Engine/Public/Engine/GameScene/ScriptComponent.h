#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Component.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Asset/AssetTypes.h"
#include "Scripting/ManagedInterop.h"
#include "Reflection/ReflectionAnnotations.h"
#include "Reflection/ReflectionFwd.h"

namespace AltinaEngine::Asset {
    class FAssetManager;
}

namespace AltinaEngine::GameScene {

    class ACLASS() AE_ENGINE_API FScriptComponent final : public FComponent {
    public:
        FScriptComponent() = default;

        void               SetAssemblyPath(Core::Container::FNativeStringView path);
        void               SetTypeName(Core::Container::FNativeStringView typeName);
        void               SetScriptAsset(AltinaEngine::Asset::FAssetHandle handle);

        [[nodiscard]] auto GetAssemblyPath() const noexcept -> Core::Container::FNativeStringView;
        [[nodiscard]] auto GetTypeName() const noexcept -> Core::Container::FNativeStringView;
        [[nodiscard]] auto GetScriptAsset() const noexcept -> AltinaEngine::Asset::FAssetHandle;

        static void        SetAssetManager(AltinaEngine::Asset::FAssetManager* manager);
        [[nodiscard]] static auto GetAssetManager() noexcept -> AltinaEngine::Asset::FAssetManager*;

        void                      OnCreate() override;
        void                      OnDestroy() override;
        void                      OnEnable() override;
        void                      OnDisable() override;
        void                      Tick(float dt) override;

    private:
        template <auto Member>
        friend struct AltinaEngine::Core::Reflection::Detail::TAutoMemberAccessor;

        auto TryCreateInstance() -> bool;
        auto RefreshFromAsset() -> bool;
        void EnsureOnCreateInvoked();

    public:
        APROPERTY()
        Core::Container::FNativeString mAssemblyPath{};

        APROPERTY()
        Core::Container::FNativeString mTypeName{};

        APROPERTY()
        AltinaEngine::Asset::FAssetHandle mScriptAsset{};

        u64                               mManagedHandle        = 0;
        bool                              mCreatedCalled        = false;
        bool                              mOnCreateInvoked      = false;
        bool                              mAssetResolved        = false;
        bool                              mLoggedTick           = false;
        bool                              mLoggedCreate         = false;
        bool                              mLoggedCreateFailure  = false;
        bool                              mLoggedResolveFailure = false;
        bool                              mLoggedResolved       = false;
    };
} // namespace AltinaEngine::GameScene
