#pragma once

#include "Engine/EngineAPI.h"
#include "Engine/GameScene/World.h"
#include "Container/HashMap.h"
#include "Container/SmartPtr.h"

namespace AltinaEngine::GameScene {
    namespace Container = Core::Container;
    using Container::THashMap;
    using Container::TOwner;

    struct AE_ENGINE_API FWorldHandle {
        u32                          Id = 0;

        [[nodiscard]] constexpr auto IsValid() const noexcept -> bool { return Id != 0; }

        [[nodiscard]] constexpr auto operator==(const FWorldHandle& other) const noexcept -> bool {
            return Id == other.Id;
        }

        [[nodiscard]] constexpr auto operator!=(const FWorldHandle& other) const noexcept -> bool {
            return !(*this == other);
        }
    };

    class AE_ENGINE_API FWorldManager {
    public:
        FWorldManager()  = default;
        ~FWorldManager() = default;

        FWorldManager(const FWorldManager&)                             = delete;
        auto operator=(const FWorldManager&) -> FWorldManager&          = delete;
        FWorldManager(FWorldManager&&)                                  = delete;
        auto               operator=(FWorldManager&&) -> FWorldManager& = delete;

        [[nodiscard]] auto CreateWorld() -> FWorldHandle;
        void               DestroyWorld(FWorldHandle handle);

        [[nodiscard]] auto GetWorld(FWorldHandle handle) noexcept -> FWorld*;
        [[nodiscard]] auto GetWorld(FWorldHandle handle) const noexcept -> const FWorld*;

        void               SetActiveWorld(FWorldHandle handle);
        [[nodiscard]] auto GetActiveWorldHandle() const noexcept -> FWorldHandle;
        [[nodiscard]] auto GetActiveWorld() noexcept -> FWorld*;
        [[nodiscard]] auto GetActiveWorld() const noexcept -> const FWorld*;

        void               Clear();

    private:
        THashMap<u32, TOwner<FWorld>> mWorlds;
        FWorldHandle                  mActiveWorld{};
    };
} // namespace AltinaEngine::GameScene
