#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/Ids.h"

namespace AltinaEngine::GameScene {
    class FWorld;

    /**
     * @brief Safe component reference using world + component ID.
     * @note Method implementations require FWorld to be defined.
     */
    template <typename T> class TComponentRef {
    public:
        TComponentRef() = default;
        TComponentRef(FWorld* world, FComponentId id) : mWorld(world), mId(id) {}

        [[nodiscard]] auto IsValid() const noexcept -> bool;
        [[nodiscard]] auto GetId() const noexcept -> FComponentId { return mId; }

        [[nodiscard]] auto Get() -> T&;
        [[nodiscard]] auto Get() const -> const T&;

    private:
        FWorld*      mWorld = nullptr;
        FComponentId mId{};
    };
} // namespace AltinaEngine::GameScene
