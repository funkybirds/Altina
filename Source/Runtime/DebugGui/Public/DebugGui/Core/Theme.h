#pragma once

#include "DebugGui/Core/Types.h"

namespace AltinaEngine::DebugGui {
    struct FDebugGuiTheme {
        // Colors.
        FColor32  WindowBg     = MakeColor32(15, 15, 15, 200);
        FColor32  WindowBorder = MakeColor32(255, 255, 255, 140);
        FColor32  TitleBarBg   = MakeColor32(25, 25, 25, 220);
        FColor32  Text         = MakeColor32(220, 220, 220, 255);
        FColor32  TitleText    = MakeColor32(255, 255, 255, 255);

        FColor32  Separator = MakeColor32(255, 255, 255, 80);

        FColor32  ButtonBg        = MakeColor32(60, 60, 60, 220);
        FColor32  ButtonHoveredBg = MakeColor32(80, 80, 80, 220);
        FColor32  ButtonActiveBg  = MakeColor32(100, 100, 100, 240);
        FColor32  ButtonBorder    = MakeColor32(255, 255, 255, 100);
        FColor32  ButtonText      = MakeColor32(255, 255, 255, 255);

        FColor32  CheckboxBoxBg     = MakeColor32(30, 30, 30, 255);
        FColor32  CheckboxBoxBorder = MakeColor32(255, 255, 255, 120);
        FColor32  CheckboxMark      = MakeColor32(140, 200, 140, 255);

        FColor32  SliderBg     = MakeColor32(40, 40, 40, 255);
        FColor32  SliderBorder = MakeColor32(255, 255, 255, 90);
        FColor32  SliderFill   = MakeColor32(120, 160, 220, 255);

        FColor32  InputBg           = MakeColor32(25, 25, 25, 255);
        FColor32  InputActiveBg     = MakeColor32(30, 30, 30, 255);
        FColor32  InputBorder       = MakeColor32(255, 255, 255, 90);
        FColor32  InputActiveBorder = MakeColor32(255, 255, 255, 160);
        FColor32  InputText         = MakeColor32(220, 220, 220, 255);

        FColor32  GizmoBg         = MakeColor32(20, 22, 26, 255);
        FColor32  GizmoBorder     = MakeColor32(255, 255, 255, 110);
        FColor32  GizmoAxisX      = MakeColor32(220, 80, 80, 255);
        FColor32  GizmoAxisY      = MakeColor32(100, 210, 120, 255);
        FColor32  GizmoAxisXY     = MakeColor32(120, 160, 220, 255);
        FColor32  GizmoAxisHover  = MakeColor32(245, 245, 245, 255);
        FColor32  GizmoAxisActive = MakeColor32(255, 210, 120, 255);

        FColor32  ScrollBarTrackBg       = MakeColor32(10, 10, 10, 90);
        FColor32  ScrollBarTrackBorder   = MakeColor32(255, 255, 255, 60);
        FColor32  ScrollBarThumbBg       = MakeColor32(110, 110, 110, 180);
        FColor32  ScrollBarThumbHoverBg  = MakeColor32(140, 140, 140, 200);
        FColor32  ScrollBarThumbActiveBg = MakeColor32(160, 160, 160, 220);
        FColor32  ScrollBarThumbBorder   = MakeColor32(255, 255, 255, 80);

        FColor32  CollapseButtonBg       = MakeColor32(45, 45, 45, 160);
        FColor32  CollapseButtonHoverBg  = MakeColor32(60, 60, 60, 200);
        FColor32  CollapseButtonActiveBg = MakeColor32(80, 80, 80, 220);
        FColor32  CollapseButtonBorder   = MakeColor32(255, 255, 255, 90);
        FColor32  CollapseIcon           = MakeColor32(240, 240, 240, 255);

        FColor32  SelectedRowBg = MakeColor32(60, 80, 110, 220);
        FColor32  HoveredRowBg  = MakeColor32(55, 55, 55, 200);

        // Metrics.
        FVector2f WindowDefaultSize = FVector2f(460.0f, 260.0f);
        FVector2f WindowDefaultPos  = FVector2f(10.0f, 10.0f);
        f32       WindowPadding     = 8.0f;
        f32       WindowSpacing     = 10.0f;
        f32       TitleBarHeight    = 18.0f;
        f32       TitleTextOffsetY  = 4.0f;

        f32       SeparatorPaddingY = 4.0f;
        f32       ItemSpacingY      = 4.0f;

        f32       ButtonPaddingX = 6.0f;
        f32       ButtonPaddingY = 3.0f;

        f32       CheckboxBoxSize     = 14.0f;
        f32       CheckboxTextOffsetX = 8.0f;
        f32       CheckboxMarkInset   = 3.0f;

        f32       SliderHeight         = 16.0f;
        f32       SliderBottomSpacingY = 4.0f;

        f32       InputHeight         = 18.0f;
        f32       InputTextOffsetX    = 6.0f;
        f32       InputTextOffsetY    = 4.0f;
        f32       InputBottomSpacingY = 6.0f;

        f32       GizmoSize            = 96.0f;
        f32       GizmoPadding         = 10.0f;
        f32       GizmoAxisThickness   = 2.0f;
        f32       GizmoHitRadius       = 6.0f;
        f32       GizmoCenterHalfSize  = 7.0f;
        f32       GizmoBottomSpacingY  = 6.0f;
        f32       GizmoDragSensitivity = 1.0f;

        f32       ScrollBarWidth          = 10.0f;
        f32       ScrollBarPadding        = 2.0f;
        f32       ScrollBarThumbMinHeight = 14.0f;

        f32       CollapseButtonSize     = 12.0f;
        f32       CollapseButtonPadX     = 6.0f;
        f32       CollapseButtonOffsetY  = 3.0f;
        f32       CollapseIconHalfWidth  = 4.0f;
        f32       CollapseIconHalfHeight = 3.0f;
    };
} // namespace AltinaEngine::DebugGui
