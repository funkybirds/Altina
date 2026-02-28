#pragma once

#include "Engine/GameScene/NativeScriptComponent.h"
#include "Container/String.h"
#include "Container/Vector.h"
#include "Math/Quaternion.h"
#include "Math/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts {
    class FShipOrbitControllerNative final : public GameScene::FNativeScriptComponent {
    public:
        void OnCreate() override;
        void Tick(float dt) override;

    private:
        enum class EShipState : u8 {
            EarthOrbit = 0,
            Transfer,
            MoonOrbit,
        };

        void               UpdateMouseOffsets();
        void               UpdateTimeScale(float dt);
        [[nodiscard]] auto HandleStateSwitchInput(const Core::Math::FVector3f& earthPos,
            const Core::Math::FVector3f& moonPos, const Core::Math::FVector3f& shipPos,
            bool canSwitchEarth, bool canSwitchMoon) -> bool;
        void               AdvanceOrbit(float scaledDt);

        [[nodiscard]] auto EvalShipPosition(const Core::Math::FVector3f& earthPos,
            const Core::Math::FVector3f& moonPos) const -> Core::Math::FVector3f;
        void               ApplyOrientation(
                          float dt, const Core::Math::FVector3f& earthPos, const Core::Math::FVector3f& moonPos);

        void UpdateWindowTitle(bool canSwitchEarth, bool canSwitchMoon);

        void BuildTransferLut(
            const Core::Math::FVector3f& earthPos, const Core::Math::FVector3f& moonPos);
        [[nodiscard]] auto        ThetaFromS01(float s01) const -> float;

        [[nodiscard]] static auto AxisX(const Core::Math::FVector3f& earthPos,
            const Core::Math::FVector3f& moonPos) -> Core::Math::FVector3f;
        [[nodiscard]] static auto JoinEarth(const Core::Math::FVector3f& earthPos,
            const Core::Math::FVector3f& axisX) -> Core::Math::FVector3f;
        [[nodiscard]] static auto JoinMoon(const Core::Math::FVector3f& moonPos,
            const Core::Math::FVector3f& axisX) -> Core::Math::FVector3f;

        [[nodiscard]] static auto CanSwitchAtJoinEarth(const Core::Math::FVector3f& earthPos,
            const Core::Math::FVector3f& moonPos, const Core::Math::FVector3f& shipPos) -> bool;
        [[nodiscard]] static auto CanSwitchAtJoinMoon(const Core::Math::FVector3f& earthPos,
            const Core::Math::FVector3f& moonPos, const Core::Math::FVector3f& shipPos) -> bool;

        static auto               EvalTransferPosition(const Core::Math::FVector3f& earthPos,
                          const Core::Math::FVector3f& axisX, const Core::Math::FVector3f& axisZ, float theta,
                          float a, float e) -> Core::Math::FVector3f;
        static auto               EvalCircleTangent(const Core::Math::FVector3f& axisX,
                          const Core::Math::FVector3f& axisZ, float phase, float dirSign)
            -> Core::Math::FVector3f;
        [[nodiscard]] static auto DistXZ(
            const Core::Math::FVector3f& a, const Core::Math::FVector3f& b) -> float;
        [[nodiscard]] static auto     Wrap01(float x) -> float;

        EShipState                    mState = EShipState::EarthOrbit;

        f32                           mT         = 0.0f;
        f32                           mTimeScale = 1.0f;

        f32                           mOrbitPhaseRad    = 0.0f;
        f32                           mTransferThetaRad = 0.0f;
        f32                           mTransferS01      = 0.0f;

        bool                          mTransferLutBuilt = false;
        Core::Container::TVector<f32> mTransferLutTheta{};
        Core::Container::TVector<f32> mTransferLutS01{};
        f32                           mTransferSAtTheta0  = 0.0f;
        f32                           mTransferSAtThetaPi = 0.0f;

        f32                           mYawOffsetRad   = 0.0f;
        f32                           mPitchOffsetRad = 0.0f;

        bool                          mTitleInitialized        = false;
        EShipState                    mLastTitleState          = EShipState::EarthOrbit;
        bool                          mLastTitleCanSwitchEarth = false;
        bool                          mLastTitleCanSwitchMoon  = false;
        Core::Container::FString      mLastWindowTitle{};

        bool                          mSpaceWasDown = false;
    };
} // namespace AltinaEngine::Demo::SpaceshipGame::NativeScripts
