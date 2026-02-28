#pragma once

#include "Engine/GameScene/NativeScriptComponent.h"
#include "Math/Quaternion.h"
#include "Math/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts {
    class FShipCameraModesNative final : public GameScene::FNativeScriptComponent {
    public:
        void OnCreate() override;
        void Tick(float dt) override;

    private:
        enum class EMode : u8 {
            FirstPerson = 0,
            ThirdPerson,
        };

        void                      SetMode(EMode mode);
        void                      UpdateThirdPersonInput();
        void                      ApplyThirdPerson();

        [[nodiscard]] static auto LookRotationFromForward(
            const Core::Math::FVector3f& forward) noexcept -> Core::Math::FQuaternion;

        EMode mMode = EMode::FirstPerson;

        f32   mTpYawRad   = 0.0f;
        f32   mTpPitchRad = 0.0f;
        f32   mTpDistance = 0.0f;
    };
} // namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts
