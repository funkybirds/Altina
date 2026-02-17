#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Ids.h"
#include "Container/String.h"
#include "Container/StringView.h"
#include "Container/Vector.h"
#include "Math/LinAlg/SpatialTransform.h"

namespace AltinaEngine::GameScene {
    namespace Container = Core::Container;
    namespace LinAlg    = Core::Math::LinAlg;
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

        [[nodiscard]] auto GetParent() const noexcept -> FGameObjectId { return mParent; }
        void               SetParent(FGameObjectId parent) noexcept {
            mParent         = parent;
            mTransformDirty = true;
        }
        void ClearParent() noexcept {
            mParent = {};
            mWorldTransform = mLocalTransform;
            mTransformDirty = true;
        }

        [[nodiscard]] auto GetLocalTransform() const noexcept
            -> const LinAlg::FSpatialTransform& {
            return mLocalTransform;
        }
        [[nodiscard]] auto GetWorldTransform() const noexcept
            -> const LinAlg::FSpatialTransform& {
            return mWorldTransform;
        }

        void SetLocalTransform(const LinAlg::FSpatialTransform& transform) noexcept {
            mLocalTransform = transform;
            if (!mParent.IsValid()) {
                mWorldTransform = transform;
            }
            mTransformDirty = true;
        }
        void SetWorldTransform(const LinAlg::FSpatialTransform& transform) noexcept {
            if (!mParent.IsValid()) {
                mLocalTransform = transform;
            }
            mWorldTransform = transform;
            mTransformDirty = true;
        }

        void UpdateWorldTransform() noexcept {
            mWorldTransform = mLocalTransform;
            mTransformDirty = false;
        }
        void UpdateWorldTransform(const LinAlg::FSpatialTransform& parentWorld) noexcept {
            mWorldTransform = parentWorld * mLocalTransform;
            mTransformDirty = false;
        }

        [[nodiscard]] auto IsTransformDirty() const noexcept -> bool { return mTransformDirty; }
        void               MarkTransformDirty() noexcept { mTransformDirty = true; }

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
        FGameObjectId         mParent{};
        LinAlg::FSpatialTransform mLocalTransform = LinAlg::FSpatialTransform::Identity();
        LinAlg::FSpatialTransform mWorldTransform = LinAlg::FSpatialTransform::Identity();
        bool                  mTransformDirty = false;
        u32                   mTransformUpdateId = 0;
        u32                   mTransformChangedId = 0;
        FString               mName{};
        bool                  mActive = true;
        TVector<FComponentId> mComponents{};
    };
} // namespace AltinaEngine::GameScene
