#pragma once

#include "Input/InputAPI.h"
#include "Input/Keys.h"
#include "Container/HashSet.h"
#include "Container/Vector.h"
#include "Types/Aliases.h"

namespace AltinaEngine::Input {
    using Core::Container::THashSet;
    using Core::Container::TVector;

    struct FKeyHash {
        auto operator()(EKey Key) const noexcept -> usize { return static_cast<usize>(Key); }
    };

    using FKeySet = THashSet<EKey, FKeyHash>;

    class AE_INPUT_API FInputSystem final {
    public:
        void ClearFrameState();

        [[nodiscard]] auto IsKeyDown(EKey InKey) const noexcept -> bool;
        [[nodiscard]] auto WasKeyPressed(EKey InKey) const noexcept -> bool;
        [[nodiscard]] auto WasKeyReleased(EKey InKey) const noexcept -> bool;

        [[nodiscard]] auto IsMouseButtonDown(u32 InButton) const noexcept -> bool;
        [[nodiscard]] auto WasMouseButtonPressed(u32 InButton) const noexcept -> bool;
        [[nodiscard]] auto WasMouseButtonReleased(u32 InButton) const noexcept -> bool;

        [[nodiscard]] auto GetPressedKeys() const noexcept -> const FKeySet&;
        [[nodiscard]] auto GetPressedMouseButtons() const noexcept -> const THashSet<u32>&;
        [[nodiscard]] auto GetKeysPressedThisFrame() const noexcept -> const FKeySet&;
        [[nodiscard]] auto GetKeysReleasedThisFrame() const noexcept -> const FKeySet&;
        [[nodiscard]] auto GetMouseButtonsPressedThisFrame() const noexcept -> const THashSet<u32>&;
        [[nodiscard]] auto GetMouseButtonsReleasedThisFrame() const noexcept
            -> const THashSet<u32>&;

        [[nodiscard]] auto GetMouseX() const noexcept -> i32;
        [[nodiscard]] auto GetMouseY() const noexcept -> i32;
        [[nodiscard]] auto GetMouseDeltaX() const noexcept -> i32;
        [[nodiscard]] auto GetMouseDeltaY() const noexcept -> i32;
        [[nodiscard]] auto GetMouseWheelDelta() const noexcept -> f32;
        [[nodiscard]] auto GetCharInputs() const noexcept -> const TVector<u32>&;

        [[nodiscard]] auto GetWindowWidth() const noexcept -> u32;
        [[nodiscard]] auto GetWindowHeight() const noexcept -> u32;
        [[nodiscard]] auto HasFocus() const noexcept -> bool;

        void OnWindowResized(u32 InWidth, u32 InHeight);
        void OnWindowFocusGained();
        void OnWindowFocusLost();
        void OnKeyDown(EKey InKey, bool InRepeat);
        void OnKeyUp(EKey InKey);
        void OnCharInput(u32 InCharCode);
        void OnMouseMove(i32 InPositionX, i32 InPositionY);
        void OnMouseButtonDown(u32 InButton);
        void OnMouseButtonUp(u32 InButton);
        void OnMouseWheel(f32 InDelta);

    private:
        FKeySet mPressedKeys;
        FKeySet mKeysPressedThisFrame;
        FKeySet mKeysReleasedThisFrame;

        THashSet<u32> mPressedMouseButtons;
        THashSet<u32> mMouseButtonsPressedThisFrame;
        THashSet<u32> mMouseButtonsReleasedThisFrame;

        TVector<u32> mCharInputs;

        u32  mWindowWidth         = 0U;
        u32  mWindowHeight        = 0U;
        bool mHasFocus            = false;
        i32  mMouseX              = 0;
        i32  mMouseY              = 0;
        i32  mMouseDeltaX         = 0;
        i32  mMouseDeltaY         = 0;
        bool mHasMousePosition    = false;
        f32  mMouseWheelDelta     = 0.0f;
    };
} // namespace AltinaEngine::Input
