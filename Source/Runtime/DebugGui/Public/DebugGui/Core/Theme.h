#pragma once

#include "DebugGui/Core/Types.h"

namespace AltinaEngine::DebugGui {
    struct FDebugGuiTheme {
        // Colors.
        FColor32  mWindowBg     = MakeColor32(15, 15, 15, 200);
        FColor32  mWindowBorder = MakeColor32(255, 255, 255, 140);
        FColor32  mTitleBarBg   = MakeColor32(25, 25, 25, 220);
        FColor32  mText         = MakeColor32(220, 220, 220, 255);
        FColor32  mTitleText    = MakeColor32(255, 255, 255, 255);

        FColor32  mSeparator = MakeColor32(255, 255, 255, 80);

        FColor32  mButtonBg        = MakeColor32(60, 60, 60, 220);
        FColor32  mButtonHoveredBg = MakeColor32(80, 80, 80, 220);
        FColor32  mButtonActiveBg  = MakeColor32(100, 100, 100, 240);
        FColor32  mButtonBorder    = MakeColor32(255, 255, 255, 100);
        FColor32  mButtonText      = MakeColor32(255, 255, 255, 255);

        FColor32  mCheckboxBoxBg     = MakeColor32(30, 30, 30, 255);
        FColor32  mCheckboxBoxBorder = MakeColor32(255, 255, 255, 120);
        FColor32  mCheckboxMark      = MakeColor32(140, 200, 140, 255);

        FColor32  mSliderBg     = MakeColor32(40, 40, 40, 255);
        FColor32  mSliderBorder = MakeColor32(255, 255, 255, 90);
        FColor32  mSliderFill   = MakeColor32(120, 160, 220, 255);

        FColor32  mInputBg           = MakeColor32(25, 25, 25, 255);
        FColor32  mInputActiveBg     = MakeColor32(30, 30, 30, 255);
        FColor32  mInputBorder       = MakeColor32(255, 255, 255, 90);
        FColor32  mInputActiveBorder = MakeColor32(255, 255, 255, 160);
        FColor32  mInputText         = MakeColor32(220, 220, 220, 255);

        FColor32  mGizmoBg         = MakeColor32(20, 22, 26, 255);
        FColor32  mGizmoBorder     = MakeColor32(255, 255, 255, 110);
        FColor32  mGizmoAxisX      = MakeColor32(220, 80, 80, 255);
        FColor32  mGizmoAxisY      = MakeColor32(100, 210, 120, 255);
        FColor32  mGizmoAxisXy     = MakeColor32(120, 160, 220, 255);
        FColor32  mGizmoAxisHover  = MakeColor32(245, 245, 245, 255);
        FColor32  mGizmoAxisActive = MakeColor32(255, 210, 120, 255);

        FColor32  mScrollBarTrackBg       = MakeColor32(10, 10, 10, 90);
        FColor32  mScrollBarTrackBorder   = MakeColor32(255, 255, 255, 60);
        FColor32  mScrollBarThumbBg       = MakeColor32(110, 110, 110, 180);
        FColor32  mScrollBarThumbHoverBg  = MakeColor32(140, 140, 140, 200);
        FColor32  mScrollBarThumbActiveBg = MakeColor32(160, 160, 160, 220);
        FColor32  mScrollBarThumbBorder   = MakeColor32(255, 255, 255, 80);

        FColor32  mCollapseButtonBg       = MakeColor32(45, 45, 45, 160);
        FColor32  mCollapseButtonHoverBg  = MakeColor32(60, 60, 60, 200);
        FColor32  mCollapseButtonActiveBg = MakeColor32(80, 80, 80, 220);
        FColor32  mCollapseButtonBorder   = MakeColor32(255, 255, 255, 90);
        FColor32  mCollapseIcon           = MakeColor32(240, 240, 240, 255);

        FColor32  mSelectedRowBg            = MakeColor32(60, 80, 110, 220);
        FColor32  mHoveredRowBg             = MakeColor32(55, 55, 55, 200);
        FColor32  mTreeExpandIcon           = MakeColor32(210, 210, 210, 255);
        FColor32  mTreeText                 = MakeColor32(220, 220, 220, 255);
        FColor32  mIconPlaceholderFile      = MakeColor32(72, 98, 148, 255);
        FColor32  mIconPlaceholderDirectory = MakeColor32(166, 132, 70, 255);
        FColor32  mIconItemBorder           = MakeColor32(255, 255, 255, 90);

        // Metrics.
        FVector2f mWindowDefaultSize = FVector2f(460.0f, 260.0f);
        FVector2f mWindowDefaultPos  = FVector2f(10.0f, 10.0f);
        f32       mWindowPadding     = 8.0f;
        f32       mWindowSpacing     = 10.0f;
        f32       mTitleBarHeight    = 18.0f;
        f32       mTitleTextOffsetY  = 4.0f;

        f32       mSeparatorPaddingY = 4.0f;
        f32       mItemSpacingY      = 4.0f;

        f32       mButtonPaddingX = 6.0f;
        f32       mButtonPaddingY = 3.0f;

        f32       mCheckboxBoxSize     = 14.0f;
        f32       mCheckboxTextOffsetX = 8.0f;
        f32       mCheckboxMarkInset   = 3.0f;

        f32       mSliderHeight         = 16.0f;
        f32       mSliderBottomSpacingY = 4.0f;

        f32       mInputHeight         = 18.0f;
        f32       mInputTextOffsetX    = 6.0f;
        f32       mInputTextOffsetY    = 4.0f;
        f32       mInputBottomSpacingY = 6.0f;

        f32       mGizmoSize            = 96.0f;
        f32       mGizmoPadding         = 10.0f;
        f32       mGizmoAxisThickness   = 2.0f;
        f32       mGizmoHitRadius       = 6.0f;
        f32       mGizmoCenterHalfSize  = 7.0f;
        f32       mGizmoBottomSpacingY  = 6.0f;
        f32       mGizmoDragSensitivity = 1.0f;

        f32       mScrollBarWidth          = 10.0f;
        f32       mScrollBarPadding        = 2.0f;
        f32       mScrollBarThumbMinHeight = 14.0f;

        f32       mCollapseButtonSize     = 12.0f;
        f32       mCollapseButtonPadX     = 6.0f;
        f32       mCollapseButtonOffsetY  = 3.0f;
        f32       mCollapseIconHalfWidth  = 4.0f;
        f32       mCollapseIconHalfHeight = 3.0f;
        f32       mTreeRowHeight          = 18.0f;
        f32       mTreeIndent             = 18.0f;
        f32       mTreeArrowSize          = 6.0f;
        f32       mTreeTextPadX           = 4.0f;
        f32       mIconLabelPadY          = 4.0f;
        f32       mIconInnerPadding       = 6.0f;

        f32       mFontScale       = 1.0f;
        f32       mFontSdfSoftness = -0.01f;
        f32       mFontSdfEdge     = 0.5f;
        f32       mUiScale         = 1.0f;
    };
} // namespace AltinaEngine::DebugGui
