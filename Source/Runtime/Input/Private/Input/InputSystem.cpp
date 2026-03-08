#include "Input/InputSystem.h"
#include "Logging/Log.h"

namespace AltinaEngine::Input {
    namespace {
        using Core::Logging::LogInfoCat;
        constexpr auto kInputTraceCategory = TEXT("Input.Trace.Space");
    } // namespace

    void FInputSystem::ClearFrameState() {
        mKeysPressedThisFrame.Clear();
        mKeysReleasedThisFrame.Clear();
        mMouseButtonsPressedThisFrame.Clear();
        mMouseButtonsReleasedThisFrame.Clear();
        mCharInputs.Clear();
        mMouseDeltaX     = 0;
        mMouseDeltaY     = 0;
        mMouseWheelDelta = 0.0f;
    }

    auto FInputSystem::IsKeyDown(EKey InKey) const noexcept -> bool {
        return mPressedKeys.Count(InKey) != 0U;
    }

    auto FInputSystem::WasKeyPressed(EKey InKey) const noexcept -> bool {
        const bool pressed = mKeysPressedThisFrame.Count(InKey) != 0U;
        if (InKey == EKey::Space && pressed) {
            LogInfoCat(kInputTraceCategory,
                TEXT("InputSystem::WasKeyPressed(Space)=true focus={} down={}"),
                mHasFocus ? 1U : 0U, IsKeyDown(EKey::Space) ? 1U : 0U);
        }
        return pressed;
    }

    auto FInputSystem::WasKeyReleased(EKey InKey) const noexcept -> bool {
        return mKeysReleasedThisFrame.Count(InKey) != 0U;
    }

    auto FInputSystem::IsMouseButtonDown(u32 InButton) const noexcept -> bool {
        return mPressedMouseButtons.Count(InButton) != 0U;
    }

    auto FInputSystem::WasMouseButtonPressed(u32 InButton) const noexcept -> bool {
        return mMouseButtonsPressedThisFrame.Count(InButton) != 0U;
    }

    auto FInputSystem::WasMouseButtonReleased(u32 InButton) const noexcept -> bool {
        return mMouseButtonsReleasedThisFrame.Count(InButton) != 0U;
    }

    auto FInputSystem::GetPressedKeys() const noexcept -> const FKeySet& { return mPressedKeys; }

    auto FInputSystem::GetPressedMouseButtons() const noexcept -> const THashSet<u32>& {
        return mPressedMouseButtons;
    }

    auto FInputSystem::GetKeysPressedThisFrame() const noexcept -> const FKeySet& {
        return mKeysPressedThisFrame;
    }

    auto FInputSystem::GetKeysReleasedThisFrame() const noexcept -> const FKeySet& {
        return mKeysReleasedThisFrame;
    }

    auto FInputSystem::GetMouseButtonsPressedThisFrame() const noexcept -> const THashSet<u32>& {
        return mMouseButtonsPressedThisFrame;
    }

    auto FInputSystem::GetMouseButtonsReleasedThisFrame() const noexcept -> const THashSet<u32>& {
        return mMouseButtonsReleasedThisFrame;
    }

    auto FInputSystem::GetMouseX() const noexcept -> i32 { return mMouseX; }

    auto FInputSystem::GetMouseY() const noexcept -> i32 { return mMouseY; }

    auto FInputSystem::GetMouseDeltaX() const noexcept -> i32 { return mMouseDeltaX; }

    auto FInputSystem::GetMouseDeltaY() const noexcept -> i32 { return mMouseDeltaY; }

    auto FInputSystem::GetMouseWheelDelta() const noexcept -> f32 { return mMouseWheelDelta; }

    auto FInputSystem::GetCharInputs() const noexcept -> const TVector<u32>& { return mCharInputs; }

    auto FInputSystem::GetWindowWidth() const noexcept -> u32 { return mWindowWidth; }

    auto FInputSystem::GetWindowHeight() const noexcept -> u32 { return mWindowHeight; }

    auto FInputSystem::HasFocus() const noexcept -> bool { return mHasFocus; }

    void FInputSystem::SetMousePositionNoDelta(i32 x, i32 y) noexcept {
        mHasMousePosition = true;
        mMouseX           = x;
        mMouseY           = y;
    }

    void FInputSystem::OnWindowResized(u32 InWidth, u32 InHeight) {
        mWindowWidth  = InWidth;
        mWindowHeight = InHeight;
    }

    void FInputSystem::OnWindowFocusGained() {
        mHasFocus = true;
        // Focus transitions can enqueue stale transient events from the platform queue.
        // Flush per-frame state to avoid reporting phantom "pressed this frame" edges.
        ClearFrameState();
    }

    void FInputSystem::OnWindowFocusLost() {
        mHasFocus = false;
        ClearFrameState();
        mPressedKeys.Clear();
        mPressedMouseButtons.Clear();
    }

    void FInputSystem::OnKeyDown(EKey InKey, bool InRepeat) {
        if (!mHasFocus) {
            if (InKey == EKey::Space) {
                LogInfoCat(kInputTraceCategory,
                    TEXT("InputSystem::OnKeyDown(Space) ignored (no focus) repeat={}"),
                    InRepeat ? 1U : 0U);
            }
            return;
        }
        // Robustness: some platforms / message pumps may mark the initial keydown as "repeat"
        // depending on focus changes or message ordering. We treat a transition from "not down"
        // to "down" as a press regardless of the repeat flag.
        if (mPressedKeys.Count(InKey) == 0U) {
            (void)InRepeat;
            mKeysPressedThisFrame.Insert(InKey);
            if (InKey == EKey::Space) {
                LogInfoCat(kInputTraceCategory,
                    TEXT("InputSystem::OnKeyDown(Space) inserted PressedThisFrame repeat={}"),
                    InRepeat ? 1U : 0U);
            }
        }
        mPressedKeys.Insert(InKey);
    }

    void FInputSystem::OnKeyUp(EKey InKey) {
        if (!mHasFocus) {
            if (InKey == EKey::Space) {
                LogInfoCat(
                    kInputTraceCategory, TEXT("InputSystem::OnKeyUp(Space) ignored (no focus)"));
            }
            return;
        }
        mPressedKeys.Erase(InKey);
        mKeysReleasedThisFrame.Insert(InKey);
        if (InKey == EKey::Space) {
            LogInfoCat(kInputTraceCategory, TEXT("InputSystem::OnKeyUp(Space) released"));
        }
    }

    void FInputSystem::OnCharInput(u32 InCharCode) {
        if (!mHasFocus) {
            return;
        }
        mCharInputs.PushBack(InCharCode);
    }

    void FInputSystem::OnMouseMove(i32 InPositionX, i32 InPositionY) {
        if (mHasMousePosition) {
            mMouseDeltaX += (InPositionX - mMouseX);
            mMouseDeltaY += (InPositionY - mMouseY);
        } else {
            mHasMousePosition = true;
        }
        mMouseX = InPositionX;
        mMouseY = InPositionY;
    }

    void FInputSystem::OnMouseButtonDown(u32 InButton) {
        if (mPressedMouseButtons.Count(InButton) == 0U) {
            mMouseButtonsPressedThisFrame.Insert(InButton);
        }
        mPressedMouseButtons.Insert(InButton);
    }

    void FInputSystem::OnMouseButtonUp(u32 InButton) {
        mPressedMouseButtons.Erase(InButton);
        mMouseButtonsReleasedThisFrame.Insert(InButton);
    }

    void FInputSystem::OnMouseWheel(f32 InDelta) { mMouseWheelDelta += InDelta; }
} // namespace AltinaEngine::Input
