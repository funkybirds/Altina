#include "ShipOrbitControllerNative.h"

#include "Application/PlatformWindow.h"
#include "Engine/GameScene/GameObjectView.h"
#include "Engine/GameScene/World.h"
#include "Input/InputSystem.h"

#include "CelestialMotion.h"
#include "SpaceshipConstants.h"
#include "SpaceshipGlobals.h"
#include "SpaceshipMath.h"
#include "SpaceshipNativeContext.h"

#include <cmath>

using AltinaEngine::Core::Container::FString;
using AltinaEngine::Core::Math::FQuaternion;
using AltinaEngine::Core::Math::FVector3f;

namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts {
    namespace {
        constexpr f32 kMouseSensitivity = 0.0025f;
        constexpr f32 kPitchLimit       = 1.35f; // ~77 deg
    } // namespace

    void FShipOrbitControllerNative::OnCreate() {
        mState     = EShipState::EarthOrbit;
        mT         = 0.0f;
        mTimeScale = 1.0f;

        // Start at a phase where the ship tangent roughly faces the Moon direction.
        mOrbitPhaseRad    = -0.5f * 3.14159265f;
        mTransferThetaRad = 0.0f;
        mTransferS01      = 0.0f;
        mYawOffsetRad     = 0.0f;
        mPitchOffsetRad   = 0.0f;

        mTitleInitialized        = false;
        mLastTitleState          = mState;
        mLastTitleCanSwitchEarth = false;
        mLastTitleCanSwitchMoon  = false;
        mLastWindowTitle         = FString();
        mSpaceWasDown            = false;

        // Bodies are static; build the LUT once.
        BuildTransferLut(
            FCelestialMotion::EarthPosition(0.0f), FCelestialMotion::MoonPosition(0.0f));
        mTransferS01 = mTransferSAtTheta0;
    }

    void FShipOrbitControllerNative::Tick(float dt) {
        mT += dt;

        UpdateTimeScale(dt);
        const f32       scaledDt = dt * mTimeScale;

        const FVector3f earthPos = FCelestialMotion::EarthPosition(mT);
        const FVector3f moonPos  = FCelestialMotion::MoonPosition(mT);

        if (!mTransferLutBuilt) {
            BuildTransferLut(earthPos, moonPos);
        }

        const FVector3f shipPosBeforeAdvance = EvalShipPosition(earthPos, moonPos);
        const bool canSwitchEarth = CanSwitchAtJoinEarth(earthPos, moonPos, shipPosBeforeAdvance);
        const bool canSwitchMoon  = CanSwitchAtJoinMoon(earthPos, moonPos, shipPosBeforeAdvance);
        UpdateWindowTitle(canSwitchEarth, canSwitchMoon);

        if (HandleStateSwitchInput(
                earthPos, moonPos, shipPosBeforeAdvance, canSwitchEarth, canSwitchMoon)) {
            // State changed; keep consistent for this frame.
            (void)EvalShipPosition(earthPos, moonPos);
        }

        AdvanceOrbit(scaledDt);
        const FVector3f shipPos = EvalShipPosition(earthPos, moonPos);

        if (auto* world = GetWorld()) {
            auto view = world->Object(GetOwner());
            if (view.IsValid()) {
                auto t        = view.GetWorldTransform();
                t.Translation = shipPos;
                view.SetWorldTransform(t);
            }
        }

        UpdateMouseOffsets();
        ApplyOrientation(dt, earthPos, moonPos);
    }

    void FShipOrbitControllerNative::UpdateWindowTitle(bool canSwitchEarth, bool canSwitchMoon) {
        const bool changed = (!mTitleInitialized) || (mLastTitleState != mState)
            || (mLastTitleCanSwitchEarth != canSwitchEarth)
            || (mLastTitleCanSwitchMoon != canSwitchMoon);
        if (!changed) {
            return;
        }

        mTitleInitialized        = true;
        mLastTitleState          = mState;
        mLastTitleCanSwitchEarth = canSwitchEarth;
        mLastTitleCanSwitchMoon  = canSwitchMoon;

        const TChar* switchText = TEXT("");
        switch (mState) {
            case EShipState::EarthOrbit:
                if (canSwitchEarth) {
                    switchText = TEXT(" | Hint: Space->Transfer");
                }
                break;
            case EShipState::Transfer:
                if (canSwitchEarth && canSwitchMoon) {
                    switchText = TEXT(" | Hint: Space->EarthOrbit / MoonOrbit");
                } else if (canSwitchEarth) {
                    switchText = TEXT(" | Hint: Space->EarthOrbit");
                } else if (canSwitchMoon) {
                    switchText = TEXT(" | Hint: Space->MoonOrbit");
                }
                break;
            case EShipState::MoonOrbit:
                if (canSwitchMoon) {
                    switchText = TEXT(" | Hint: Space->Transfer");
                }
                break;
        }

        const TChar* stateText = TEXT("Unknown");
        switch (mState) {
            case EShipState::EarthOrbit:
                stateText = TEXT("EarthOrbit");
                break;
            case EShipState::Transfer:
                stateText = TEXT("Transfer");
                break;
            case EShipState::MoonOrbit:
                stateText = TEXT("MoonOrbit");
                break;
        }

        FString title;
        title.Append(TEXT("SpaceshipGame | "));
        title.Append(stateText);
        title.Append(switchText);

        if (title == mLastWindowTitle) {
            return;
        }

        mLastWindowTitle = title;
        if (auto* window = GetWindow()) {
            window->SetTitle(title);
        }
    }

    void FShipOrbitControllerNative::UpdateMouseOffsets() {
        if (gThirdPersonCameraEnabled) {
            return;
        }

        auto* input = GetInput();
        if (input == nullptr) {
            return;
        }
        if (!input->HasFocus()) {
            return;
        }

        const i32 dx = input->GetMouseDeltaX();
        const i32 dy = input->GetMouseDeltaY();

        mYawOffsetRad += static_cast<f32>(dx) * kMouseSensitivity;
        mPitchOffsetRad += -static_cast<f32>(dy) * kMouseSensitivity;
        mPitchOffsetRad = Clamp(mPitchOffsetRad, -kPitchLimit, kPitchLimit);
    }

    void FShipOrbitControllerNative::UpdateTimeScale(float dt) {
        auto* input = GetInput();
        if (input == nullptr) {
            return;
        }

        constexpr f32 accel = 0.8f;
        if (input->IsKeyDown(Input::EKey::Q)) {
            mTimeScale += accel * dt;
        }
        if (input->IsKeyDown(Input::EKey::E)) {
            mTimeScale -= accel * dt;
        }
        mTimeScale = Clamp(mTimeScale, 0.1f, 6.0f);
    }

    auto FShipOrbitControllerNative::HandleStateSwitchInput(const FVector3f& earthPos,
        const FVector3f& moonPos, const FVector3f& shipPos, bool canSwitchEarth, bool canSwitchMoon)
        -> bool {
        auto* input = GetInput();
        if (input == nullptr) {
            return false;
        }

        bool changed = false;

        // Space edge detector (robust to focus issues).
        if (!input->HasFocus()) {
            mSpaceWasDown = false;
        }
        const bool spaceDown    = input->IsKeyDown(Input::EKey::Space);
        const bool spacePressed = spaceDown && !mSpaceWasDown;
        mSpaceWasDown           = spaceDown;

        if (spacePressed) {
            switch (mState) {
                case EShipState::EarthOrbit:
                    if (canSwitchEarth) {
                        mState            = EShipState::Transfer;
                        mTransferS01      = mTransferSAtTheta0;
                        mTransferThetaRad = 0.0f;
                        changed           = true;
                    }
                    break;
                case EShipState::Transfer:
                    if (canSwitchEarth) {
                        mState         = EShipState::EarthOrbit;
                        mOrbitPhaseRad = 3.14159265f;
                        changed        = true;
                    } else if (canSwitchMoon) {
                        mState         = EShipState::MoonOrbit;
                        mOrbitPhaseRad = 3.14159265f;
                        changed        = true;
                    }
                    break;
                case EShipState::MoonOrbit:
                    if (canSwitchMoon) {
                        mState            = EShipState::Transfer;
                        mTransferS01      = mTransferSAtThetaPi;
                        mTransferThetaRad = 3.14159265f;
                        changed           = true;
                    }
                    break;
            }
        }

        // Orbit-only debug state machine.
        if (input->WasKeyPressed(Input::EKey::Num2)) {
            if (mState == EShipState::Transfer && canSwitchEarth) {
                mState         = EShipState::EarthOrbit;
                mOrbitPhaseRad = 3.14159265f;
                changed        = true;
            }
        }
        if (input->WasKeyPressed(Input::EKey::Num3)) {
            if (mState == EShipState::EarthOrbit && canSwitchEarth) {
                mState            = EShipState::Transfer;
                mTransferS01      = mTransferSAtTheta0;
                mTransferThetaRad = 0.0f;
                changed           = true;
            } else if (mState == EShipState::MoonOrbit && canSwitchMoon) {
                mState            = EShipState::Transfer;
                mTransferS01      = mTransferSAtThetaPi;
                mTransferThetaRad = 3.14159265f;
                changed           = true;
            }
        }
        if (input->WasKeyPressed(Input::EKey::Num4)) {
            if (mState == EShipState::Transfer && canSwitchMoon) {
                mState         = EShipState::MoonOrbit;
                mOrbitPhaseRad = 3.14159265f;
                changed        = true;
            }
        }

        (void)earthPos;
        (void)moonPos;
        (void)shipPos;
        return changed;
    }

    void FShipOrbitControllerNative::AdvanceOrbit(float scaledDt) {
        switch (mState) {
            case EShipState::EarthOrbit:
                mOrbitPhaseRad += scaledDt * 0.9f;
                mOrbitPhaseRad = WrapAngleRad(mOrbitPhaseRad);
                break;
            case EShipState::MoonOrbit:
                // Reverse direction for tangent continuity at JoinMoon.
                mOrbitPhaseRad -= scaledDt * 1.8f;
                mOrbitPhaseRad = WrapAngleRad(mOrbitPhaseRad);
                break;
            case EShipState::Transfer:
            default:
            {
                constexpr f32 thetaDot   = 0.35f;
                constexpr f32 twoPi      = 6.2831853f;
                const f32     fracPerSec = thetaDot / twoPi;
                mTransferS01             = Wrap01(mTransferS01 + scaledDt * fracPerSec);
                mTransferThetaRad        = WrapAngleRad(ThetaFromS01(mTransferS01));
                break;
            }
        }
    }

    auto FShipOrbitControllerNative::EvalShipPosition(
        const FVector3f& earthPos, const FVector3f& moonPos) const -> FVector3f {
        const FVector3f earthToMoon = moonPos - earthPos;
        const FVector3f axisX       = NormalizeXZ(earthToMoon);
        const FVector3f axisZ       = PerpLeftXZ(axisX);

        switch (mState) {
            case EShipState::EarthOrbit:
            {
                const f32       c = std::cos(mOrbitPhaseRad);
                const f32       s = std::sin(mOrbitPhaseRad);
                const FVector3f local =
                    (axisX * FVector3f(FSpaceshipConstants::EarthOrbitRadius * c))
                    + (axisZ * FVector3f(FSpaceshipConstants::EarthOrbitRadius * s));
                return earthPos + local;
            }
            case EShipState::MoonOrbit:
            {
                const f32       c = std::cos(mOrbitPhaseRad);
                const f32       s = std::sin(mOrbitPhaseRad);
                const FVector3f local =
                    (axisX * FVector3f(FSpaceshipConstants::MoonOrbitRadius * c))
                    + (axisZ * FVector3f(FSpaceshipConstants::MoonOrbitRadius * s));
                return moonPos + local;
            }
            case EShipState::Transfer:
            {
                constexpr f32 r1 = FSpaceshipConstants::EarthOrbitRadius;
                const f32     r2 =
                    FSpaceshipConstants::EarthMoonDistance - FSpaceshipConstants::MoonOrbitRadius;
                const f32 a = 0.5f * (r1 + r2);
                const f32 e = (r2 - r1) / (r2 + r1);
                return EvalTransferPosition(earthPos, axisX, axisZ, mTransferThetaRad, a, e);
            }
            default:
                return earthPos;
        }
    }

    void FShipOrbitControllerNative::ApplyOrientation(
        float dt, const FVector3f& earthPos, const FVector3f& moonPos) {
        f32             baseYawRad = 0.0f;

        const FVector3f earthToMoon = moonPos - earthPos;
        const FVector3f axisX       = NormalizeXZ(earthToMoon);
        const FVector3f axisZ       = PerpLeftXZ(axisX);

        switch (mState) {
            case EShipState::EarthOrbit:
            {
                const FVector3f t = EvalCircleTangent(axisX, axisZ, mOrbitPhaseRad, +1.0f);
                baseYawRad        = std::atan2(t.X(), t.Z());
                break;
            }
            case EShipState::MoonOrbit:
            {
                const FVector3f t = EvalCircleTangent(axisX, axisZ, mOrbitPhaseRad, -1.0f);
                baseYawRad        = std::atan2(t.X(), t.Z());
                break;
            }
            case EShipState::Transfer:
            {
                const f32     scaledDt = dt * mTimeScale;

                constexpr f32 r1 = FSpaceshipConstants::EarthOrbitRadius;
                const f32     r2 =
                    FSpaceshipConstants::EarthMoonDistance - FSpaceshipConstants::MoonOrbitRadius;
                const f32       a = 0.5f * (r1 + r2);
                const f32       e = (r2 - r1) / (r2 + r1);

                const f32       absDt  = std::fabs(scaledDt);
                const f32       dTheta = (absDt * 0.02f > 0.001f) ? (absDt * 0.02f) : 0.001f;
                const FVector3f p0 =
                    EvalTransferPosition(earthPos, axisX, axisZ, mTransferThetaRad, a, e);
                const FVector3f p1 =
                    EvalTransferPosition(earthPos, axisX, axisZ, mTransferThetaRad + dTheta, a, e);
                const FVector3f d   = p1 - p0;
                const f32       len = LengthXZ(d);
                if (len > 1e-6f) {
                    baseYawRad = std::atan2(d.X(), d.Z());
                }
                break;
            }
            default:
                break;
        }

        const f32         yaw = baseYawRad + mYawOffsetRad;
        const FQuaternion q   = FromEuler(mPitchOffsetRad, yaw, 0.0f);

        if (auto* world = GetWorld()) {
            auto view = world->Object(GetOwner());
            if (view.IsValid()) {
                auto t     = view.GetWorldTransform();
                t.Rotation = q;
                view.SetWorldTransform(t);
            }
        }
    }

    auto FShipOrbitControllerNative::EvalTransferPosition(const FVector3f& earthPos,
        const FVector3f& axisX, const FVector3f& axisZ, float theta, float a, float e)
        -> FVector3f {
        const f32 p = a * (1.0f - e * e);
        const f32 r = p / (1.0f + e * std::cos(theta));

        const f32 phi = theta + 3.14159265f;
        const f32 c   = std::cos(phi);
        const f32 s   = std::sin(phi);
        return earthPos + (axisX * FVector3f(r * c)) + (axisZ * FVector3f(r * s));
    }

    auto FShipOrbitControllerNative::EvalCircleTangent(
        const FVector3f& axisX, const FVector3f& axisZ, float phase, float dirSign) -> FVector3f {
        const f32       s = std::sin(phase);
        const f32       c = std::cos(phase);
        const FVector3f t = (axisX * FVector3f(-s)) + (axisZ * FVector3f(c));
        return t * FVector3f(dirSign);
    }

    auto FShipOrbitControllerNative::DistXZ(const FVector3f& a, const FVector3f& b) -> float {
        const FVector3f d = a - b;
        return std::sqrt(d.X() * d.X() + d.Z() * d.Z());
    }

    auto FShipOrbitControllerNative::Wrap01(float x) -> float {
        x = std::fmod(x, 1.0f);
        if (x < 0.0f) {
            x += 1.0f;
        }
        return x;
    }

    void FShipOrbitControllerNative::BuildTransferLut(
        const FVector3f& earthPos, const FVector3f& moonPos) {
        const FVector3f axisX = AxisX(earthPos, moonPos);
        const FVector3f axisZ = PerpLeftXZ(axisX);

        constexpr f32   r1 = FSpaceshipConstants::EarthOrbitRadius;
        const f32       r2 =
            FSpaceshipConstants::EarthMoonDistance - FSpaceshipConstants::MoonOrbitRadius;
        const f32     a = 0.5f * (r1 + r2);
        const f32     e = (r2 - r1) / (r2 + r1);

        constexpr i32 n     = 4096;
        constexpr f32 twoPi = 6.2831853f;

        mTransferLutTheta.Resize(static_cast<usize>(n + 1));
        mTransferLutS01.Resize(static_cast<usize>(n + 1));

        FVector3f prev       = EvalTransferPosition(earthPos, axisX, axisZ, 0.0f, a, e);
        mTransferLutTheta[0] = 0.0f;
        mTransferLutS01[0]   = 0.0f;

        f32 total = 0.0f;
        for (i32 i = 1; i <= n; ++i) {
            const f32       theta = twoPi * (static_cast<f32>(i) / static_cast<f32>(n));
            const FVector3f p     = EvalTransferPosition(earthPos, axisX, axisZ, theta, a, e);
            total += DistXZ(p, prev);
            mTransferLutTheta[static_cast<usize>(i)] = theta;
            mTransferLutS01[static_cast<usize>(i)]   = total;
            prev                                     = p;
        }

        if (total > 1e-6f) {
            for (i32 i = 1; i <= n; ++i) {
                mTransferLutS01[static_cast<usize>(i)] /= total;
            }
        }

        mTransferSAtTheta0  = 0.0f;
        mTransferSAtThetaPi = mTransferLutS01[static_cast<usize>(n / 2)];
        mTransferLutBuilt   = true;
    }

    auto FShipOrbitControllerNative::ThetaFromS01(float s01) const -> float {
        if (mTransferLutS01.IsEmpty()) {
            return 0.0f;
        }

        s01 = Wrap01(s01);

        i32 lo = 0;
        i32 hi = static_cast<i32>(mTransferLutS01.Size() - 1);
        while (lo < hi) {
            const i32 mid = (lo + hi) / 2;
            if (mTransferLutS01[static_cast<usize>(mid)] < s01) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }

        const i32 i1 = lo;
        if (i1 <= 0) {
            return mTransferLutTheta[0];
        }

        const i32 maxI = static_cast<i32>(mTransferLutS01.Size());
        if (i1 >= maxI) {
            return mTransferLutTheta[mTransferLutTheta.Size() - 1];
        }

        const i32 i0    = i1 - 1;
        const f32 s0    = mTransferLutS01[static_cast<usize>(i0)];
        const f32 s1    = mTransferLutS01[static_cast<usize>(i1)];
        const f32 t0    = mTransferLutTheta[static_cast<usize>(i0)];
        const f32 t1    = mTransferLutTheta[static_cast<usize>(i1)];
        const f32 denom = s1 - s0;
        if (denom <= 1e-8f) {
            return t1;
        }

        const f32 a = (s01 - s0) / denom;
        return t0 + (t1 - t0) * a;
    }

    auto FShipOrbitControllerNative::AxisX(const FVector3f& earthPos, const FVector3f& moonPos)
        -> FVector3f {
        return NormalizeXZ(moonPos - earthPos);
    }

    auto FShipOrbitControllerNative::JoinEarth(const FVector3f& earthPos, const FVector3f& axisX)
        -> FVector3f {
        return earthPos + (axisX * FVector3f(-FSpaceshipConstants::EarthOrbitRadius));
    }

    auto FShipOrbitControllerNative::JoinMoon(const FVector3f& moonPos, const FVector3f& axisX)
        -> FVector3f {
        return moonPos + (axisX * FVector3f(-FSpaceshipConstants::MoonOrbitRadius));
    }

    auto FShipOrbitControllerNative::CanSwitchAtJoinEarth(
        const FVector3f& earthPos, const FVector3f& moonPos, const FVector3f& shipPos) -> bool {
        const FVector3f axisX = AxisX(earthPos, moonPos);
        const FVector3f join  = JoinEarth(earthPos, axisX);
        return DistXZ(shipPos, join) <= FSpaceshipConstants::OrbitSwitchEpsilon;
    }

    auto FShipOrbitControllerNative::CanSwitchAtJoinMoon(
        const FVector3f& earthPos, const FVector3f& moonPos, const FVector3f& shipPos) -> bool {
        const FVector3f axisX = AxisX(earthPos, moonPos);
        const FVector3f join  = JoinMoon(moonPos, axisX);
        return DistXZ(shipPos, join) <= FSpaceshipConstants::OrbitSwitchEpsilon;
    }
} // namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts
