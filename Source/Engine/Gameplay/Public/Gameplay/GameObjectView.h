#pragma once

#include "Gameplay/GameplayAPI.h"
#include "Gameplay/ComponentRef.h"
#include "Gameplay/Ids.h"

namespace AltinaEngine::Gameplay {
    class FWorld;

    /**
     * @brief Lightweight view for manipulating a game object via IDs.
     */
    class AE_GAMEPLAY_API FGameObjectView {
    public:
        FGameObjectView() = default;
        FGameObjectView(FWorld* world, FGameObjectId id) : mWorld(world), mId(id) {}

        [[nodiscard]] auto IsValid() const noexcept -> bool;
        [[nodiscard]] auto GetId() const noexcept -> FGameObjectId { return mId; }

        template <typename T> [[nodiscard]] auto Add() -> TComponentRef<T>;
        template <typename T> [[nodiscard]] auto Has() const -> bool;
        template <typename T> [[nodiscard]] auto Get() const -> TComponentRef<T>;
        template <typename T> void               Remove();

        void               SetActive(bool active);
        [[nodiscard]] auto IsActive() const -> bool;

    private:
        FWorld*       mWorld = nullptr;
        FGameObjectId mId{};
    };
} // namespace AltinaEngine::Gameplay
