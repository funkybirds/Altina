#pragma once

#include "Base/EditorUIAPI.h"
#include "Container/String.h"
#include "Container/Vector.h"
#include "CoreMinimal.h"

namespace AltinaEngine::DebugGui {
    class IDebugGui;
    class IDebugGuiSystem;
} // namespace AltinaEngine::DebugGui

namespace AltinaEngine::Editor::UI {
    inline constexpr u64 kEditorViewportImageId = 0xE17D0001ULL;

    enum class EEditorUiCommand : u8 {
        None = 0,
        Play,
        Pause,
        Step,
        Stop,
        Exit
    };

    struct FEditorViewportRequest {
        u32  Width       = 0U;
        u32  Height      = 0U;
        bool bHovered    = false;
        bool bFocused    = false;
        bool bHasContent = false;
    };

    class AE_EDITOR_UI_API FEditorUiModule final {
    public:
        void               RegisterDefaultPanels(DebugGui::IDebugGuiSystem* debugGuiSystem);
        [[nodiscard]] auto GetViewportRequest() const noexcept -> FEditorViewportRequest;
        [[nodiscard]] auto ConsumeUiCommands()
            -> ::AltinaEngine::Core::Container::TVector<EEditorUiCommand>;

    private:
        enum class EDockArea : u8 {
            Left = 0,
            Center,
            Right,
            Bottom
        };

        struct FPanelState {
            ::AltinaEngine::Core::Container::FString Name;
            EDockArea                                Area     = EDockArea::Center;
            bool                                     bVisible = true;
        };

        struct FDockState {
            f32 LeftRatio    = 0.2f;
            f32 RightRatio   = 0.24f;
            f32 BottomRatio  = 0.28f;
            i32 ActiveLeft   = 0;
            i32 ActiveCenter = 0;
            i32 ActiveRight  = 0;
            i32 ActiveBottom = 0;
        };

        void DrawRootUi(DebugGui::IDebugGuiSystem* debugGuiSystem, DebugGui::IDebugGui& gui);

        DebugGui::IDebugGuiSystem*                                 mDebugGuiSystem  = nullptr;
        bool                                                       mRegistered      = false;
        bool                                                       mFocusedViewport = false;
        i32                                                        mOpenMenu        = -1;
        i32                                                        mDraggingPanel   = -1;
        i32                                                        mActiveSplitter  = 0;
        FDockState                                                 mDock{};
        FEditorViewportRequest                                     mViewportRequest{};
        ::AltinaEngine::Core::Container::TVector<FPanelState>      mPanels;
        ::AltinaEngine::Core::Container::TVector<EEditorUiCommand> mPendingCommands;
    };
} // namespace AltinaEngine::Editor::UI
