#include "DebugGui/Widgets/DebugGuiContext.h"
#include "Math/Geometry2D.h"

namespace AltinaEngine::DebugGui::Private {
    auto FDebugGuiContext::Gizmo(FStringView label, FVector2f& value) -> bool {
        Text(label);

        const f32 availableW = mContentMax.X() - mContentMin.X();
        f32       size       = mTheme->GizmoSize;
        if (size > availableW) {
            size = availableW;
        }
        if (size < 40.0f) {
            size = 40.0f;
        }

        const FRect     r{ mCursor, FVector2f(mCursor.X() + size, mCursor.Y() + size) };
        const f32       centerHalf = mTheme->GizmoCenterHalfSize;
        const f32       axisPad    = mTheme->GizmoPadding;
        const f32       axisLenRaw = size * 0.5f - axisPad;
        const f32       axisLen    = (axisLenRaw > 8.0f) ? axisLenRaw : 8.0f;

        const FVector2f center((r.Min.X() + r.Max.X()) * 0.5f, (r.Min.Y() + r.Max.Y()) * 0.5f);
        const FVector2f xEnd(center.X() + axisLen, center.Y());
        const FVector2f yEnd(center.X(), center.Y() - axisLen);

        const FRect     centerRect{ FVector2f(center.X() - centerHalf, center.Y() - centerHalf),
            FVector2f(center.X() + centerHalf, center.Y() + centerHalf) };

        const u64       baseId = HashId(label);
        const u64       idX    = baseId ^ 0xB339D7E68E5F6A11ULL;
        const u64       idY    = baseId ^ 0x63E6D6D7CC5AAE97ULL;
        const u64       idXY   = baseId ^ 0x58A4D1B2FE0C7C43ULL;

        const f32       hitR      = mTheme->GizmoHitRadius;
        const f32       hitR2     = hitR * hitR;
        const bool      hoveredXY = PointInRect(mInput.MousePos, centerRect);
        const f32       distXSq   = DistPointSegmentSq(mInput.MousePos, center, xEnd);
        const f32       distYSq   = DistPointSegmentSq(mInput.MousePos, center, yEnd);
        const bool      hoveredX  = distXSq <= hitR2;
        const bool      hoveredY  = distYSq <= hitR2;

        u64             hoveredId = 0ULL;
        if (hoveredXY) {
            hoveredId = idXY;
        } else if (hoveredX && hoveredY) {
            hoveredId = (distXSq <= distYSq) ? idX : idY;
        } else if (hoveredX) {
            hoveredId = idX;
        } else if (hoveredY) {
            hoveredId = idY;
        }

        if (hoveredId != 0ULL) {
            mUi->HotId              = hoveredId;
            mUi->bWantsCaptureMouse = true;
        }

        if (hoveredId != 0ULL && mInput.bMousePressed) {
            mUi->ActiveId = hoveredId;
            mUi->FocusId  = hoveredId;
        }

        const bool activeX  = (mUi->ActiveId == idX);
        const bool activeY  = (mUi->ActiveId == idY);
        const bool activeXY = (mUi->ActiveId == idXY);
        const bool active   = activeX || activeY || activeXY;

        bool       changed = false;
        if (active && mInput.bMouseDown) {
            const f32 dx = static_cast<f32>(mInput.MouseDeltaX) * mTheme->GizmoDragSensitivity;
            const f32 dy = static_cast<f32>(mInput.MouseDeltaY) * mTheme->GizmoDragSensitivity;

            if ((activeX || activeXY) && dx != 0.0f) {
                value.X() += dx;
                changed = true;
            }
            if ((activeY || activeXY) && dy != 0.0f) {
                // Screen-space Y grows downward; invert to get conventional gizmo semantics.
                value.Y() -= dy;
                changed = true;
            }
            mUi->bWantsCaptureMouse = true;
        }

        if (active && mInput.bMouseReleased) {
            mUi->ActiveId = 0ULL;
        }

        auto ResolveAxisColor = [&](u64 id, FColor32 base) -> FColor32 {
            if (mUi->ActiveId == id) {
                return mTheme->GizmoAxisActive;
            }
            if (hoveredId == id) {
                return mTheme->GizmoAxisHover;
            }
            return base;
        };

        DrawRectFilled(r, mTheme->GizmoBg);
        DrawRect(r, mTheme->GizmoBorder, 1.0f);

        const FColor32 xCol  = ResolveAxisColor(idX, mTheme->GizmoAxisX);
        const FColor32 yCol  = ResolveAxisColor(idY, mTheme->GizmoAxisY);
        const FColor32 xyCol = ResolveAxisColor(idXY, mTheme->GizmoAxisXY);
        DrawLine(center, xEnd, xCol, mTheme->GizmoAxisThickness);
        DrawLine(center, yEnd, yCol, mTheme->GizmoAxisThickness);
        DrawRectFilled(centerRect, xyCol);
        DrawRect(centerRect, mTheme->GizmoBorder, 1.0f);

        DrawText(FVector2f(xEnd.X() + 3.0f, xEnd.Y() - 6.0f), xCol, TEXT("X"));
        DrawText(FVector2f(yEnd.X() + 3.0f, yEnd.Y() - 6.0f), yCol, TEXT("Y"));

        FString valueText;
        valueText.Assign(TEXT("x="));
        valueText.AppendNumber(value.X());
        valueText.Append(TEXT(" y="));
        valueText.AppendNumber(value.Y());
        DrawText(FVector2f(r.Min.X(), r.Max.Y() + 3.0f), mTheme->Text, valueText.ToView());

        AdvanceItem(FVector2f(size, size + mTheme->GizmoBottomSpacingY + 14.0f));
        return changed;
    }
} // namespace AltinaEngine::DebugGui::Private
