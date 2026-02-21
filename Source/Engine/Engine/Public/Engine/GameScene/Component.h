#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Ids.h"
#include "Reflection/ReflectionAnnotations.h"
#include "Reflection/ReflectionFwd.h"

namespace AltinaEngine::GameScene {
    class FWorld;

    /**
     * @brief Base class for scene components.
     */
    class ACLASS(Abstract) AE_ENGINE_API FComponent {
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
        template <auto Member>
        friend struct AltinaEngine::Core::Reflection::Detail::TAutoMemberAccessor;

        void Initialize(FWorld* world, FComponentId id, FGameObjectId owner) noexcept {
            mWorld = world;
            mId    = id;
            mOwner = owner;
        }

        FWorld* mWorld = nullptr;

    public:
        APROPERTY()
        FComponentId mId{};

        APROPERTY()
        FGameObjectId mOwner{};

        APROPERTY()
        bool mEnabled = true;
    };
} // namespace AltinaEngine::GameScene
