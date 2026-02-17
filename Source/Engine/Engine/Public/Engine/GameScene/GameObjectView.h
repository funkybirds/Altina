#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/ComponentRef.h"
#include "Engine/GameScene/Ids.h"
#include "Math/LinAlg/SpatialTransform.h"

namespace AltinaEngine::GameScene {
    namespace LinAlg = Core::Math::LinAlg;
    class FWorld;

    /**
     * @brief Lightweight view for manipulating a game object via IDs.
     */
    class AE_ENGINE_API FGameObjectView {
    public:
        FGameObjectView() = default;
        FGameObjectView(FWorld* world, FGameObjectId id) : mWorld(world), mId(id) {}

        [[nodiscard]] auto IsValid() const noexcept -> bool;
        [[nodiscard]] auto GetId() const noexcept -> FGameObjectId { return mId; }

        template <typename T> [[nodiscard]] auto Add() -> TComponentRef<T>;
        template <typename T> [[nodiscard]] auto Has() const -> bool;
        template <typename T> [[nodiscard]] auto Get() const -> TComponentRef<T>;
        template <typename T> void               Remove();

        void                                     SetActive(bool active);
        [[nodiscard]] auto                       IsActive() const -> bool;

        [[nodiscard]] auto GetLocalTransform() const noexcept -> LinAlg::FSpatialTransform;
        [[nodiscard]] auto GetWorldTransform() const noexcept -> LinAlg::FSpatialTransform;
        void               SetLocalTransform(const LinAlg::FSpatialTransform& transform);
        void               SetWorldTransform(const LinAlg::FSpatialTransform& transform);

        [[nodiscard]] auto GetParent() const noexcept -> FGameObjectId;
        void               SetParent(FGameObjectId parent);
        void               ClearParent();

    private:
        FWorld*       mWorld = nullptr;
        FGameObjectId mId{};
    };
} // namespace AltinaEngine::GameScene
