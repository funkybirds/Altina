#include "ShipCameraModesNative.h"

#include "Engine/GameScene/GameObjectView.h"
#include "Engine/GameScene/World.h"
#include "Input/InputSystem.h"

#include "SpaceshipGlobals.h"
#include "SpaceshipMath.h"
#include "SpaceshipNativeContext.h"

#include <cmath>

using AltinaEngine::Core::Math::FQuaternion;
using AltinaEngine::Core::Math::FVector3f;

namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts {
    namespace {
        constexpr f32       kMouseSensitivity = 0.0025f;
        constexpr f32       kPitchLimit       = 1.25f; // ~71 deg

        constexpr f32       kDistanceMin        = 0.12f;
        constexpr f32       kDistanceMax        = 1.50f;
        constexpr f32       kDistanceWheelSpeed = 0.12f;

        constexpr FVector3f kCockpitOffset(0.0f, 0.02f, 0.07f);
        constexpr FVector3f kThirdPersonTargetOffset(0.0f, 0.02f, 0.0f);
    } // namespace

    void FShipCameraModesNative::OnCreate() {
        mTpYawRad   = 0.0f;
        mTpPitchRad = 0.2f;
        mTpDistance = 0.35f;
        SetMode(EMode::FirstPerson);
    }

    void FShipCameraModesNative::Tick(float /*dt*/) {
        auto* input = GetInput();
        if (input != nullptr && input->WasKeyPressed(Input::EKey::C)) {
            SetMode((mMode == EMode::FirstPerson) ? EMode::ThirdPerson : EMode::FirstPerson);
        }

        if (mMode == EMode::ThirdPerson) {
            UpdateThirdPersonInput();
            ApplyThirdPerson();
        }
    }

    void FShipCameraModesNative::SetMode(EMode mode) {
        mMode                     = mode;
        gThirdPersonCameraEnabled = (mode == EMode::ThirdPerson);

        auto* world = GetWorld();
        if (world == nullptr) {
            return;
        }

        auto view = world->Object(GetOwner());
        if (!view.IsValid()) {
            return;
        }

        if (mode == EMode::FirstPerson) {
            auto t        = view.GetLocalTransform();
            t.Translation = kCockpitOffset;
            t.Rotation    = FQuaternion::Identity();
            view.SetLocalTransform(t);
            return;
        }

        ApplyThirdPerson();
    }

    void FShipCameraModesNative::UpdateThirdPersonInput() {
        auto* input = GetInput();
        if (input == nullptr) {
            return;
        }
        if (!input->HasFocus()) {
            return;
        }

        const i32 dx = input->GetMouseDeltaX();
        const i32 dy = input->GetMouseDeltaY();

        mTpYawRad += static_cast<f32>(dx) * kMouseSensitivity;
        mTpPitchRad += -static_cast<f32>(dy) * kMouseSensitivity;
        mTpPitchRad = Clamp(mTpPitchRad, -kPitchLimit, kPitchLimit);

        const f32 wheel = input->GetMouseWheelDelta();
        if (std::fabs(wheel) > 1e-5f) {
            mTpDistance -= wheel * kDistanceWheelSpeed;
            mTpDistance = Clamp(mTpDistance, kDistanceMin, kDistanceMax);
        }
    }

    void FShipCameraModesNative::ApplyThirdPerson() {
        auto* world = GetWorld();
        if (world == nullptr) {
            return;
        }

        auto view = world->Object(GetOwner());
        if (!view.IsValid()) {
            return;
        }

        // Orbit around the ship in local space (camera object is parented to the ship).
        const FVector3f backward(0.0f, 0.0f, -mTpDistance);
        const FVector3f yawed = RotateY(backward, mTpYawRad);

        // Pitch around local X in the yawed frame.
        const f32       sp = std::sin(mTpPitchRad);
        const f32       cp = std::cos(mTpPitchRad);
        const FVector3f pitched(
            yawed.X(), yawed.Y() * cp - yawed.Z() * sp, yawed.Y() * sp + yawed.Z() * cp);

        const FVector3f localPos = kThirdPersonTargetOffset + pitched;

        auto            t = view.GetLocalTransform();
        t.Translation     = localPos;

        // Look at the target (origin + offset).
        const FVector3f toTarget = kThirdPersonTargetOffset - localPos;
        t.Rotation               = LookRotationFromForward(toTarget);
        view.SetLocalTransform(t);
    }

    auto FShipCameraModesNative::LookRotationFromForward(const FVector3f& forward) noexcept
        -> FQuaternion {
        const f32 lenSq =
            forward.X() * forward.X() + forward.Y() * forward.Y() + forward.Z() * forward.Z();
        if (lenSq <= 1e-8f) {
            return FQuaternion::Identity();
        }

        const f32       invLen = 1.0f / std::sqrt(lenSq);
        const FVector3f f(forward.X() * invLen, forward.Y() * invLen, forward.Z() * invLen);

        const f32       yaw   = std::atan2(f.X(), f.Z());
        const f32       xz    = std::sqrt(f.X() * f.X() + f.Z() * f.Z());
        const f32       pitch = std::atan2(f.Y(), xz);

        return FromEuler(pitch, yaw, 0.0f);
    }
} // namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts
