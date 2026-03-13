#include "DebugGui/Widgets/DebugGuiContext.h"

namespace AltinaEngine::DebugGui::Private {
    auto FDebugGuiContext::Button(FStringView label) -> bool {
        const u64       id   = HashId(label);
        const FVector2f size = CalcButtonSize(label);
        const FRect     r{ mCursor, FVector2f(mCursor.X() + size.X(), mCursor.Y() + size.Y()) };

        const bool      hovered = PointInRect(mInput.mMousePos, r);
        if (hovered) {
            mUi->mHotId             = id;
            mUi->mWantsCaptureMouse = true;
        }

        bool pressed = false;
        if (hovered && mInput.mMousePressed) {
            mUi->mActiveId = id;
            mUi->mFocusId  = id;
        }
        if (mUi->mActiveId == id) {
            if (mInput.mMouseReleased) {
                if (hovered) {
                    pressed = true;
                }
                mUi->mActiveId = 0ULL;
            }
        }

        FColor32 bg = hovered ? mTheme->mButtonHoveredBg : mTheme->mButtonBg;
        if (mUi->mActiveId == id) {
            bg = mTheme->mButtonActiveBg;
        }
        DrawRectFilled(r, bg);
        DrawRect(r, mTheme->mButtonBorder, 1.0f);

        const FVector2f textPos(
            r.Min.X() + mTheme->mButtonPaddingX, r.Min.Y() + mTheme->mButtonPaddingY);
        DrawText(textPos, mTheme->mButtonText, label);

        AdvanceItem(size);
        return pressed;
    }

    auto FDebugGuiContext::TreeViewItem(const FTreeViewItemDesc& desc) -> FTreeViewItemResult {
        FTreeViewItemResult result{};
        if (desc.mLabel.IsEmpty()) {
            return result;
        }

        const f32   rowHeight  = (mTheme->mTreeRowHeight > 0.0f) ? mTheme->mTreeRowHeight : 18.0f;
        const f32   indentStep = (mTheme->mTreeIndent > 0.0f) ? mTheme->mTreeIndent : 14.0f;
        const f32   arrowSize  = (mTheme->mTreeArrowSize > 0.0f) ? mTheme->mTreeArrowSize : 6.0f;

        const f32   indent   = static_cast<f32>(desc.mDepth) * indentStep;
        const FRect clipRect = mClip->Current(mDisplaySize);
        const f32 rowMaxX = (clipRect.Max.X() > mCursor.X()) ? clipRect.Max.X() : mDisplaySize.X();
        const FRect rowRect{ mCursor, FVector2f(rowMaxX, mCursor.Y() + rowHeight) };
        const f32 textStartX = rowRect.Min.X() + indent + arrowSize + mTheme->mTreeTextPadX + 3.0f;
        const f32 textWidth =
            static_cast<f32>(desc.mLabel.Length()) * static_cast<f32>(FFontAtlas::kDrawGlyphW);
        const FRect textHitRect{ FVector2f(textStartX - 2.0f, rowRect.Min.Y()),
            FVector2f(textStartX + textWidth + 4.0f, rowRect.Max.Y()) };

        const u64   baseRowId = BuildChildHashId(TEXT("##TreeRow"), desc.mLabel);
        const u64   rowId     = baseRowId ^ (static_cast<u64>(desc.mDepth) << 32U)
            ^ static_cast<u64>(static_cast<u32>(rowRect.Min.Y()));
        const bool hovered =
            PointInRect(mInput.mMousePos, rowRect) || PointInRect(mInput.mMousePos, textHitRect);
        if (hovered) {
            mUi->mHotId             = rowId;
            mUi->mWantsCaptureMouse = true;
        }
        if (hovered && mInput.mMousePressed) {
            mUi->mActiveId = rowId;
            mUi->mFocusId  = rowId;
        }
        if (mUi->mActiveId == rowId && mInput.mMouseReleased) {
            if (hovered) {
                result.mClicked = true;
                if (mUi->mLastClickId == rowId
                    && (mUi->mFrameIndex - mUi->mLastClickFrame) <= 20ULL) {
                    result.mDoubleClicked = true;
                }
                mUi->mLastClickId    = rowId;
                mUi->mLastClickFrame = mUi->mFrameIndex;
            }
            mUi->mActiveId = 0ULL;
        }
        if (hovered && mInput.mMousePressedRight) {
            result.mContextMenuRequested = true;
            mUi->mWantsCaptureMouse      = true;
        }

        FColor32 rowColor = 0U;
        if (desc.mSelected) {
            rowColor = mTheme->mSelectedRowBg;
        } else if (hovered) {
            rowColor = mTheme->mHoveredRowBg;
        }
        if ((rowColor >> 24U) != 0U) {
            DrawRectFilled(rowRect, rowColor);
        }
        if (hovered) {
            DrawRect(rowRect, mTheme->mTreeExpandIcon, 1.0f);
        }

        const f32 arrowCenterX = rowRect.Min.X() + indent + arrowSize * 0.5f + 2.0f;
        const f32 arrowCenterY = rowRect.Min.Y() + rowHeight * 0.5f;
        if (desc.mHasChildren) {
            const FRect toggleHitRect{ FVector2f(rowRect.Min.X() + indent, rowRect.Min.Y()),
                FVector2f(rowRect.Min.X() + indent + arrowSize + mTheme->mTreeTextPadX + 4.0f,
                    rowRect.Max.Y()) };
            const bool  arrowHovered = PointInRect(mInput.mMousePos, toggleHitRect);
            if (arrowHovered && mInput.mMousePressed) {
                mUi->mWantsCaptureMouse = true;
            }
            if (arrowHovered && mInput.mMouseReleased) {
                result.mToggleExpanded  = true;
                mUi->mWantsCaptureMouse = true;
            }

            const f32 hw = arrowSize * 0.5f;
            const f32 hh = arrowSize * 0.5f;
            if (desc.mExpanded) {
                DrawTriangleFilled(FVector2f(arrowCenterX - hw, arrowCenterY - hh),
                    FVector2f(arrowCenterX + hw, arrowCenterY - hh),
                    FVector2f(arrowCenterX, arrowCenterY + hh), mTheme->mTreeExpandIcon);
            } else {
                DrawTriangleFilled(FVector2f(arrowCenterX - hh, arrowCenterY - hw),
                    FVector2f(arrowCenterX - hh, arrowCenterY + hw),
                    FVector2f(arrowCenterX + hh, arrowCenterY), mTheme->mTreeExpandIcon);
            }
        }

        DrawText(FVector2f(textStartX, rowRect.Min.Y() + 3.0f), mTheme->mTreeText, desc.mLabel);

        auto trailingWidgetDraw = desc.mTrailingWidgetDraw;
        if (trailingWidgetDraw) {
            trailingWidgetDraw(*this, rowRect);
        }

        mCursor = FVector2f(rowRect.Min.X(), mCursor.Y() + rowHeight + mTheme->mItemSpacingY);
        return result;
    }

    auto FDebugGuiContext::TextedIconView(const FTextedIconViewDesc& desc)
        -> FTextedIconViewResult {
        FTextedIconViewResult result{};
        if (desc.mLabel.IsEmpty()) {
            return result;
        }

        const FRect itemRect = desc.mRect;
        if (itemRect.Max.X() <= itemRect.Min.X() || itemRect.Max.Y() <= itemRect.Min.Y()) {
            return result;
        }

        const u64  itemId  = BuildChildHashId(TEXT("##TextedIcon"), desc.mLabel);
        const bool hovered = PointInRect(mInput.mMousePos, itemRect);
        if (hovered) {
            mUi->mHotId             = itemId;
            mUi->mWantsCaptureMouse = true;
        }
        if (hovered && mInput.mMousePressed) {
            mUi->mActiveId = itemId;
            mUi->mFocusId  = itemId;
        }
        if (mUi->mActiveId == itemId && mInput.mMouseReleased) {
            if (hovered) {
                result.mClicked = true;
                if (mUi->mLastClickId == itemId
                    && (mUi->mFrameIndex - mUi->mLastClickFrame) <= 20ULL) {
                    result.mDoubleClicked = true;
                }
                mUi->mLastClickId    = itemId;
                mUi->mLastClickFrame = mUi->mFrameIndex;
            }
            mUi->mActiveId = 0ULL;
        }
        if (hovered && mInput.mMousePressedRight) {
            result.mContextMenuRequested = true;
            mUi->mWantsCaptureMouse      = true;
        }

        FColor32 bg = 0U;
        if (desc.mSelected) {
            bg = mTheme->mSelectedRowBg;
        } else if (hovered) {
            bg = mTheme->mHoveredRowBg;
        }
        if ((bg >> 24U) != 0U) {
            DrawRectFilled(itemRect, bg);
        }

        const f32 labelHeight = static_cast<f32>(FFontAtlas::kDrawGlyphH) + mTheme->mIconLabelPadY;
        const f32 innerPad    = mTheme->mIconInnerPadding;
        const FRect iconRect{ FVector2f(itemRect.Min.X() + innerPad, itemRect.Min.Y() + innerPad),
            FVector2f(itemRect.Max.X() - innerPad, itemRect.Max.Y() - labelHeight) };
        const FColor32 placeholder =
            desc.mIsDirectory ? mTheme->mIconPlaceholderDirectory : mTheme->mIconPlaceholderFile;
        DrawRoundedRectFilled(iconRect, placeholder, 4.0f);
        DrawRoundedRect(iconRect, mTheme->mIconItemBorder, 4.0f, 1.0f);
        DrawImage(iconRect, desc.mImageId, MakeColor32(255, 255, 255, 255));

        const f32 textX = itemRect.Min.X() + innerPad;
        const f32 textY = itemRect.Max.Y() - static_cast<f32>(FFontAtlas::kDrawGlyphH) - 1.0f;
        DrawText(FVector2f(textX, textY), mTheme->mTreeText, desc.mLabel);
        return result;
    }
} // namespace AltinaEngine::DebugGui::Private
