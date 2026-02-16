#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Ids.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"

namespace AltinaEngine::GameScene {
    namespace Container = Core::Container;
    using Container::FString;
    using Container::FStringView;
    using Container::TVector;

    class FWorld;

    /**
     * @brief Component container identified by an opaque ID.
     */
    class AE_ENGINE_API FGameObject {
    public:
        [[nodiscard]] auto GetId() const noexcept -> FGameObjectId { return mId; }
        [[nodiscard]] auto IsActive() const noexcept -> bool { return mActive; }
        void               SetActive(bool active) noexcept { mActive = active; }

        [[nodiscard]] auto GetName() const noexcept -> const FString& { return mName; }
        void               SetName(FStringView name);

        template <typename T> [[nodiscard]] auto AddComponent() -> FComponentId;
        [[nodiscard]] auto AddComponentByType(FComponentTypeHash type) -> FComponentId;
        void               RemoveComponent(FComponentId id);

        template <typename T> [[nodiscard]] auto HasComponent() const -> bool;
        template <typename T> [[nodiscard]] auto GetComponent() const -> FComponentId;
        [[nodiscard]] auto                       GetAllComponents() const -> TVector<FComponentId>;

    private:
        friend class FWorld;

        void                  SetWorld(FWorld* world) noexcept { mWorld = world; }
        void                  SetId(FGameObjectId id) noexcept { mId = id; }
        void                  AddComponentId(FComponentId id);
        void                  RemoveComponentId(FComponentId id);

        FWorld*               mWorld = nullptr;
        FGameObjectId         mId{};
        FString               mName{};
        bool                  mActive = true;
        TVector<FComponentId> mComponents{};
    };
} // namespace AltinaEngine::GameScene
