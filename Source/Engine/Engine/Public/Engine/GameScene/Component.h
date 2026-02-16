#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Ids.h"

namespace AltinaEngine::GameScene {
    class FWorld;

    /**
     * @brief Base class for scene components.
     */
    class AE_ENGINE_API FComponent {
    public:
        virtual ~FComponent() = default;

        [[nodiscard]] auto GetId() const noexcept -> FComponentId { return mId; }
        [[nodiscard]] auto GetOwner() const noexcept -> FGameObjectId { return mOwner; }
        [[nodiscard]] auto IsEnabled() const noexcept -> bool { return mEnabled; }

        void               SetEnabled(bool enabled) noexcept;

        virtual void       OnCreate() {}
        virtual void       OnDestroy() {}
        virtual void       OnEnable() {}
        virtual void       OnDisable() {}
        virtual void       Tick(float /*dt*/) {}

    protected:
        FComponent() = default;

    private:
        friend class FWorld;

        void Initialize(FComponentId id, FGameObjectId owner) noexcept {
            mId    = id;
            mOwner = owner;
        }

        FComponentId  mId{};
        FGameObjectId mOwner{};
        bool          mEnabled = true;
    };

    inline void FComponent::SetEnabled(bool enabled) noexcept {
        if (mEnabled == enabled) {
            return;
        }

        mEnabled = enabled;
        if (mEnabled) {
            OnEnable();
        } else {
            OnDisable();
        }
    }
} // namespace AltinaEngine::GameScene
