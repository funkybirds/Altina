#pragma once

#include "DebugGui/Core/Types.h"

namespace AltinaEngine::DebugGui {
    enum class EDebugGuiFontRole : u8 {
        Body = 0,
        Small,
        WindowTitle,
        Menu,
        Tab,
        Section,
        Label,
        Status
    };

    struct FDebugGuiFontStyle {
        f32 mScale = 1.0f;
    };

    struct FDebugGuiFontSet {
        FDebugGuiFontStyle mBody{};
        FDebugGuiFontStyle mSmall{ 0.84f };
        FDebugGuiFontStyle mWindowTitle{ 1.0f };
        FDebugGuiFontStyle mMenu{ 1.0f };
        FDebugGuiFontStyle mTab{ 1.02f };
        FDebugGuiFontStyle mSection{ 1.02f };
        FDebugGuiFontStyle mLabel{ 1.0f };
        FDebugGuiFontStyle mStatus{ 0.86f };
    };

    struct FDebugGuiSurfaceStyle {
        FColor32 mBg              = MakeColor32(34, 36, 40, 228);
        FColor32 mBorder          = MakeColor32(255, 153, 64, 42);
        f32      mCornerRadius    = 10.0f;
        f32      mBorderThickness = 0.0f;
    };

    struct FDebugGuiTabStyle {
        FColor32 mBarBg            = MakeColor32(28, 30, 34, 250);
        FColor32 mBarSeparator     = MakeColor32(0, 0, 0, 0);
        FColor32 mText             = MakeColor32(181, 186, 194, 255);
        FColor32 mTextActive       = MakeColor32(255, 255, 255, 255);
        FColor32 mItemBg           = MakeColor32(0, 0, 0, 0);
        FColor32 mItemHoveredBg    = MakeColor32(50, 43, 38, 236);
        FColor32 mItemActiveBg     = MakeColor32(168, 98, 40, 232);
        FColor32 mUnderline        = MakeColor32(214, 122, 48, 224);
        FColor32 mUnderlineHovered = MakeColor32(214, 122, 48, 84);
        FColor32 mDivider          = MakeColor32(255, 153, 64, 20);
        FColor32 mIcon             = MakeColor32(208, 213, 220, 224);
        FColor32 mIconActive       = MakeColor32(255, 255, 255, 255);
        f32      mHeight           = 48.0f;
        f32      mTextPadX         = 16.0f;
        f32      mTextPadY         = 11.0f;
        f32      mIconSize         = 14.0f;
        f32      mIconPadX         = 10.0f;
        f32      mUnderlineHeight  = 3.0f;
    };

    struct FDebugGuiMenuStyle {
        FColor32 mBarBg           = MakeColor32(30, 32, 36, 232);
        FColor32 mPopupBg         = MakeColor32(36, 38, 43, 248);
        FColor32 mItemBgHovered   = MakeColor32(62, 51, 42, 245);
        FColor32 mItemText        = MakeColor32(227, 230, 235, 255);
        FColor32 mItemTextMuted   = MakeColor32(147, 153, 162, 255);
        FColor32 mSeparator       = MakeColor32(255, 153, 64, 30);
        FColor32 mSelectionMarker = MakeColor32(255, 140, 48, 255);
        f32      mHeight          = 84.0f;
        f32      mItemHeight      = 34.0f;
        f32      mItemPadX        = 16.0f;
        f32      mPopupPad        = 10.0f;
        f32      mPopupRadius     = 10.0f;
    };

    struct FDebugGuiSectionStyle {
        FColor32 mLine               = MakeColor32(255, 153, 64, 40);
        FColor32 mLabelText          = MakeColor32(227, 230, 235, 255);
        FColor32 mSecondaryText      = MakeColor32(147, 153, 162, 255);
        FColor32 mPrimaryLabelBg     = MakeColor32(255, 140, 48, 255);
        FColor32 mPrimaryLabelText   = MakeColor32(255, 255, 255, 255);
        FColor32 mSecondaryLabelBg   = MakeColor32(78, 83, 91, 255);
        FColor32 mSecondaryLabelText = MakeColor32(235, 237, 240, 255);
        f32      mSpacingY           = 14.0f;
        f32      mUnderlineThickness = 2.0f;
        f32      mLabelPadX          = 12.0f;
        f32      mLabelPadY          = 7.0f;
        f32      mLabelRadius        = 999.0f;
    };

    struct FDebugGuiEditorStyle {
        FColor32              mAppBg                 = MakeColor32(22, 23, 26, 255);
        FColor32              mAppGlowA              = MakeColor32(255, 140, 48, 14);
        FColor32              mAppGlowB              = MakeColor32(255, 180, 96, 10);
        FColor32              mPanelBodyBg           = MakeColor32(255, 255, 255, 0);
        FColor32              mPanelBodyFallbackBg   = MakeColor32(38, 40, 45, 132);
        FColor32              mPanelContentText      = MakeColor32(227, 230, 235, 255);
        FColor32              mPanelContentMutedText = MakeColor32(147, 153, 162, 255);
        FColor32              mViewportBg            = MakeColor32(18, 19, 22, 232);
        FColor32              mViewportStatusText    = MakeColor32(160, 166, 175, 255);
        FColor32              mSplitter              = MakeColor32(111, 142, 178, 0);
        FColor32              mSplitterHovered       = MakeColor32(255, 140, 48, 118);
        FColor32              mDropHint              = MakeColor32(255, 140, 48, 52);
        FColor32              mPlaceholderIconBg     = MakeColor32(255, 140, 48, 36);
        FColor32              mPlaceholderIconFg     = MakeColor32(255, 167, 94, 255);
        FDebugGuiSurfaceStyle mWindowSurface{};
        FDebugGuiSurfaceStyle mPanelSurface{};
        FDebugGuiSurfaceStyle mInsetSurface{ MakeColor32(42, 44, 49, 220),
            MakeColor32(255, 153, 64, 26), 10.0f, 0.0f };
        FDebugGuiTabStyle     mTabs{};
        FDebugGuiMenuStyle    mMenu{};
        FDebugGuiSectionStyle mSections{};
        f32                   mWorkspacePadding    = 18.0f;
        f32                   mSplitterSize        = 10.0f;
        f32                   mPanelPadding        = 14.0f;
        f32                   mMinPanelWidth       = 140.0f;
        f32                   mMinCenterWidth      = 260.0f;
        f32                   mMinTopHeight        = 180.0f;
        f32                   mMinBottomHeight     = 100.0f;
        f32                   mStatusBarHeight     = 24.0f;
        f32                   mPanelGap            = 0.0f;
        f32                   mMenuItemMarkerWidth = 6.0f;
    };

    struct FDebugGuiTheme {
        FDebugGuiFontSet     mFonts{};
        FDebugGuiEditorStyle mEditor{};

        // Colors.
        FColor32             mWindowBg     = MakeColor32(34, 36, 40, 232);
        FColor32             mWindowBorder = MakeColor32(255, 153, 64, 24);
        FColor32             mTitleBarBg   = MakeColor32(28, 30, 34, 252);
        FColor32             mText         = MakeColor32(227, 230, 235, 255);
        FColor32             mTitleText    = MakeColor32(244, 246, 248, 255);

        FColor32             mSeparator = MakeColor32(255, 153, 64, 26);

        FColor32             mButtonBg        = MakeColor32(58, 46, 37, 196);
        FColor32             mButtonHoveredBg = MakeColor32(78, 58, 42, 255);
        FColor32             mButtonActiveBg  = MakeColor32(97, 68, 44, 255);
        FColor32             mButtonBorder    = MakeColor32(255, 153, 64, 30);
        FColor32             mButtonText      = MakeColor32(244, 246, 248, 255);

        FColor32             mCheckboxBoxBg     = MakeColor32(53, 56, 62, 216);
        FColor32             mCheckboxBoxBorder = MakeColor32(255, 153, 64, 30);
        FColor32             mCheckboxMark      = MakeColor32(255, 140, 48, 255);

        FColor32             mSliderBg     = MakeColor32(48, 50, 56, 220);
        FColor32             mSliderBorder = MakeColor32(255, 153, 64, 22);
        FColor32             mSliderFill   = MakeColor32(255, 140, 48, 255);

        FColor32             mInputBg           = MakeColor32(48, 50, 56, 228);
        FColor32             mInputActiveBg     = MakeColor32(58, 60, 66, 240);
        FColor32             mInputBorder       = MakeColor32(255, 153, 64, 22);
        FColor32             mInputActiveBorder = MakeColor32(255, 140, 48, 96);
        FColor32             mInputText         = MakeColor32(232, 235, 239, 255);

        FColor32             mGizmoBg         = MakeColor32(42, 44, 49, 220);
        FColor32             mGizmoBorder     = MakeColor32(255, 153, 64, 24);
        FColor32             mGizmoAxisX      = MakeColor32(220, 80, 80, 255);
        FColor32             mGizmoAxisY      = MakeColor32(100, 210, 120, 255);
        FColor32             mGizmoAxisXy     = MakeColor32(255, 140, 48, 255);
        FColor32             mGizmoAxisHover  = MakeColor32(245, 245, 245, 255);
        FColor32             mGizmoAxisActive = MakeColor32(255, 210, 120, 255);

        FColor32             mScrollBarTrackBg       = MakeColor32(255, 255, 255, 18);
        FColor32             mScrollBarTrackBorder   = MakeColor32(255, 255, 255, 0);
        FColor32             mScrollBarThumbBg       = MakeColor32(255, 153, 64, 88);
        FColor32             mScrollBarThumbHoverBg  = MakeColor32(255, 153, 64, 128);
        FColor32             mScrollBarThumbActiveBg = MakeColor32(255, 140, 48, 172);
        FColor32             mScrollBarThumbBorder   = MakeColor32(255, 255, 255, 0);

        FColor32             mCollapseButtonBg       = MakeColor32(255, 255, 255, 0);
        FColor32             mCollapseButtonHoverBg  = MakeColor32(70, 56, 44, 255);
        FColor32             mCollapseButtonActiveBg = MakeColor32(92, 66, 43, 255);
        FColor32             mCollapseButtonBorder   = MakeColor32(255, 255, 255, 0);
        FColor32             mCollapseIcon           = MakeColor32(214, 219, 226, 255);

        FColor32             mSelectedRowBg            = MakeColor32(255, 140, 48, 46);
        FColor32             mHoveredRowBg             = MakeColor32(255, 140, 48, 24);
        FColor32             mTreeExpandIcon           = MakeColor32(176, 183, 192, 255);
        FColor32             mTreeText                 = MakeColor32(196, 201, 208, 255);
        FColor32             mIconPlaceholderFile      = MakeColor32(255, 172, 96, 255);
        FColor32             mIconPlaceholderDirectory = MakeColor32(255, 140, 48, 255);
        FColor32             mIconItemBorder           = MakeColor32(255, 255, 255, 0);

        // Metrics.
        FVector2f            mWindowDefaultSize = FVector2f(460.0f, 260.0f);
        FVector2f            mWindowDefaultPos  = FVector2f(10.0f, 10.0f);
        f32                  mWindowPadding     = 8.0f;
        f32                  mWindowSpacing     = 10.0f;
        f32                  mTitleBarHeight    = 18.0f;
        f32                  mTitleTextOffsetY  = 4.0f;

        f32                  mSeparatorPaddingY = 4.0f;
        f32                  mItemSpacingY      = 4.0f;

        f32                  mButtonPaddingX = 10.0f;
        f32                  mButtonPaddingY = 6.0f;

        f32                  mCheckboxBoxSize     = 14.0f;
        f32                  mCheckboxTextOffsetX = 8.0f;
        f32                  mCheckboxMarkInset   = 3.0f;

        f32                  mSliderHeight         = 18.0f;
        f32                  mSliderBottomSpacingY = 4.0f;

        f32                  mInputHeight         = 22.0f;
        f32                  mInputTextOffsetX    = 6.0f;
        f32                  mInputTextOffsetY    = 5.0f;
        f32                  mInputBottomSpacingY = 6.0f;

        f32                  mGizmoSize            = 96.0f;
        f32                  mGizmoPadding         = 10.0f;
        f32                  mGizmoAxisThickness   = 2.0f;
        f32                  mGizmoHitRadius       = 6.0f;
        f32                  mGizmoCenterHalfSize  = 7.0f;
        f32                  mGizmoBottomSpacingY  = 6.0f;
        f32                  mGizmoDragSensitivity = 1.0f;

        f32                  mScrollBarWidth          = 8.0f;
        f32                  mScrollBarPadding        = 2.0f;
        f32                  mScrollBarThumbMinHeight = 14.0f;

        f32                  mCollapseButtonSize     = 14.0f;
        f32                  mCollapseButtonPadX     = 6.0f;
        f32                  mCollapseButtonOffsetY  = 3.0f;
        f32                  mCollapseIconHalfWidth  = 4.0f;
        f32                  mCollapseIconHalfHeight = 3.0f;
        f32                  mTreeRowHeight          = 22.0f;
        f32                  mTreeIndent             = 18.0f;
        f32                  mTreeArrowSize          = 6.0f;
        f32                  mTreeTextPadX           = 8.0f;
        f32                  mIconLabelPadY          = 3.0f;
        f32                  mIconInnerPadding       = 8.0f;

        f32                  mFontScale       = 1.0f;
        f32                  mFontSdfSoftness = 0.0f;
        f32                  mFontSdfEdge     = 0.5f;
        f32                  mUiScale         = 1.0f;
    };
} // namespace AltinaEngine::DebugGui
