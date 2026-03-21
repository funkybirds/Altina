#pragma once

#include "DebugGui/Core/DebugGuiCoreTypes.h"
#include "DebugGui/Core/FontAtlas.h"
#include "DebugGui/Core/IconAtlas.h"
#include "Math/Common.h"

namespace AltinaEngine::DebugGui::Private {
    class FDebugGuiContext final : public IDebugGui {
    public:
        explicit FDebugGuiContext(FDrawData& drawData, FClipRectStack& clip, const FGuiInput& input,
            FUIState& ui, const FVector2f& displaySize, const FFontAtlas& atlas,
            const FIconAtlas& iconAtlas, const FDebugGuiTheme& theme, TVector<FString>& windowOrder,
            Container::THashMap<u64, FWindowState>& windows, u64& draggingWindowKey,
            FVector2f& dragOffset)
            : mDrawData(&drawData)
            , mClip(&clip)
            , mInput(input)
            , mUi(&ui)
            , mDisplaySize(displaySize)
            , mAtlas(&atlas)
            , mIconAtlas(&iconAtlas)
            , mTheme(&theme)
            , mFontScale((theme.mFontScale > 0.01f) ? theme.mFontScale : 1.0f)
            // Inset by half a texel to avoid sampling bleed (linear sampler).
            , mSolidU0((static_cast<f32>(FFontAtlas::kSolidTexelX) + 0.5f)
                  / static_cast<f32>(FFontAtlas::kAtlasW))
            , mSolidV0((static_cast<f32>(FFontAtlas::kSolidTexelY) + 0.5f)
                  / static_cast<f32>(FFontAtlas::kAtlasH))
            , mSolidU1(
                  (static_cast<f32>(FFontAtlas::kSolidTexelX + FFontAtlas::kAtlasGlyphW) - 0.5f)
                  / static_cast<f32>(FFontAtlas::kAtlasW))
            , mSolidV1(
                  (static_cast<f32>(FFontAtlas::kSolidTexelY + FFontAtlas::kAtlasGlyphH) - 0.5f)
                  / static_cast<f32>(FFontAtlas::kAtlasH))
            , mWindowOrder(&windowOrder)
            , mWindows(&windows)
            , mDraggingWindowKey(&draggingWindowKey)
            , mDragOffset(&dragOffset) {}

        [[nodiscard]] auto GetWindowRect() const noexcept -> FRect { return mWindowRect; }
        [[nodiscard]] auto GetContentRect() const noexcept -> FRect {
            return { mContentMin, mContentMax };
        }
        [[nodiscard]] auto GetTheme() const noexcept -> const FDebugGuiTheme& { return *mTheme; }
        [[nodiscard]] auto IsMouseHoveringRect(const FRect& rect) const noexcept -> bool {
            return PointInRect(mInput.mMousePos, rect);
        }
        [[nodiscard]] auto DebugHashId(FStringView label) const noexcept -> u64 {
            return HashId(label);
        }
        [[nodiscard]] auto GetCursorPos() const noexcept -> FVector2f { return mCursor; }
        void               SetCursorPos(const FVector2f& p) noexcept { mCursor = p; }

        void               PushClipRect(const FRect& rect) override {
            const FRect cur = mClip->Current(mDisplaySize);
            mClip->Push(IntersectRect(cur, rect));
        }
        void PopClipRect() override { mClip->Pop(); }

        void DrawRectFilled(const FRect& rect, FColor32 color) override {
            AddRectFilled(rect, color);
        }
        void DrawRect(const FRect& rect, FColor32 color, f32 thickness) override {
            AddRect(rect, color, thickness);
        }
        void DrawRoundedRectFilled(const FRect& rect, FColor32 color, f32 rounding,
            EDebugGuiCornerFlags cornerFlags = EDebugGuiCornerFlags::All) override {
            AddRoundedRectFilled(rect, color, rounding, cornerFlags);
        }
        void DrawRoundedRect(const FRect& rect, FColor32 color, f32 rounding, f32 thickness,
            EDebugGuiCornerFlags cornerFlags = EDebugGuiCornerFlags::All) override {
            AddRoundedRect(rect, color, rounding, thickness, cornerFlags);
        }
        void DrawCapsuleFilled(
            const FVector2f& a, const FVector2f& b, f32 radius, FColor32 color) override {
            AddCapsuleFilled(a, b, radius, color);
        }
        void DrawCapsule(const FVector2f& a, const FVector2f& b, f32 radius, FColor32 color,
            f32 thickness) override {
            AddCapsule(a, b, radius, color, thickness);
        }
        void DrawLine(
            const FVector2f& p0, const FVector2f& p1, FColor32 color, f32 thickness) override {
            AddLine(p0, p1, color, thickness);
        }
        void DrawTriangleFilled(const FVector2f& p0, const FVector2f& p1, const FVector2f& p2,
            FColor32 color) override {
            AddTriangleFilled(p0, p1, p2, color);
        }
        void DrawText(const FVector2f& pos, FColor32 color, FStringView text) override {
            AddText(pos, color, text, mFontScale);
        }
        void DrawTextStyled(const FVector2f& pos, FColor32 color, FStringView text,
            EDebugGuiFontRole role) override {
            AddText(pos, color, text, ResolveFontScale(role));
        }
        void DrawImage(const FRect& rect, u64 imageId, FColor32 tint) override {
            if (imageId == 0ULL) {
                return;
            }
            AddQuad(rect.Min, FVector2f(rect.Max.X(), rect.Min.Y()), rect.Max,
                FVector2f(rect.Min.X(), rect.Max.Y()), 0.0f, 0.0f, 1.0f, 1.0f, tint,
                EDrawTextureMode::ExternalImage, imageId);
        }
        void DrawIcon(const FRect& rect, FDebugGuiIconId iconId, FColor32 tint) override {
            if (iconId == kInvalidDebugGuiIconId || mIconAtlas == nullptr) {
                return;
            }

            f32 u0 = 0.0f;
            f32 v0 = 0.0f;
            f32 u1 = 1.0f;
            f32 v1 = 1.0f;
            if (!mIconAtlas->TryGetIconUV(iconId, u0, v0, u1, v1)) {
                return;
            }

            AddQuad(rect.Min, FVector2f(rect.Max.X(), rect.Min.Y()), rect.Max,
                FVector2f(rect.Min.X(), rect.Max.Y()), u0, v0, u1, v1, tint,
                EDrawTextureMode::InternalIcon);
        }
        [[nodiscard]] auto MeasureText(FStringView text, EDebugGuiFontRole role) const
            -> FVector2f override {
            return FVector2f(CalcTextWidth(text, ResolveFontScale(role)), GetGlyphHeight(role));
        }

        [[nodiscard]] auto GetDisplaySize() const noexcept -> FVector2f override {
            return mDisplaySize;
        }
        [[nodiscard]] auto GetMousePos() const noexcept -> FVector2f override {
            return mInput.mMousePos;
        }
        [[nodiscard]] auto IsMouseDown() const noexcept -> bool override {
            return mInput.mMouseDown;
        }
        [[nodiscard]] auto WasMousePressed() const noexcept -> bool override {
            return mInput.mMousePressed;
        }
        [[nodiscard]] auto WasMouseReleased() const noexcept -> bool override {
            return mInput.mMouseReleased;
        }

        auto BeginWindow(FStringView title, bool* open) -> bool override {
            if (title.IsEmpty()) {
                return false;
            }
            if (open != nullptr && !(*open)) {
                return false;
            }

            if (mWindows == nullptr) {
                return false;
            }

            const auto& th = *mTheme;

            // Track order for default placement.
            usize       windowIndex = 0U;
            if (mWindowOrder != nullptr) {
                bool found = false;
                for (const auto& s : *mWindowOrder) {
                    if (s.ToView() == title) {
                        found = true;
                        break;
                    }
                    ++windowIndex;
                }
                if (!found) {
                    windowIndex = mWindowOrder->Size();
                    mWindowOrder->PushBack(FString(title));
                }
            }

            mCurrentWindowTitle.Assign(title);
            const u64 windowKey = HashWindowKey(title);
            auto&     state     = (*mWindows)[windowKey];
            if (!state.mInitialized) {
                state.mInitialized = true;
                state.mSize        = th.mWindowDefaultSize;
                state.mPos         = th.mWindowDefaultPos;

                // Built-in panels: keep Console hidden by default, and start Stats/CVars in a
                // collapsed state to reduce screen clutter on first launch.
                if (title == FStringView(TEXT("DebugGui Stats"))
                    || title == FStringView(TEXT("DebugGui CVars"))) {
                    state.mCollapsed = true;
                }

                if (mWindowOrder != nullptr) {
                    // Place windows in columns if the display height is too small for pure
                    // vertical stacking.
                    const f32 cellW   = state.mSize.X() + th.mWindowSpacing;
                    const f32 cellH   = state.mSize.Y() + th.mWindowSpacing;
                    const f32 usableH = (mDisplaySize.Y() > th.mWindowDefaultPos.Y())
                        ? (mDisplaySize.Y() - th.mWindowDefaultPos.Y())
                        : 0.0f;
                    const u32 perCol  = (usableH > cellH) ? static_cast<u32>(usableH / cellH) : 1U;

                    const u32 col = static_cast<u32>(windowIndex) / perCol;
                    const u32 row = static_cast<u32>(windowIndex) % perCol;
                    state.mPos = FVector2f(th.mWindowDefaultPos.X() + static_cast<f32>(col) * cellW,
                        th.mWindowDefaultPos.Y() + static_cast<f32>(row) * cellH);
                }
            }

            mWindowPos  = state.mPos;
            mWindowSize = state.mSize;

            const f32 titleBarH = th.mTitleBarHeight;

            // Title bar collapse button (right side).
            const f32 kTitlePadX = th.mCollapseButtonPadX;
            const f32 kBtnSize   = th.mCollapseButtonSize;

            f32       drawH = 0.0f;
            FRect     windowRect{};
            FRect     titleRect{};
            FVector2f btnMin(0.0f, 0.0f);
            FRect     btnRect{};

            auto      buildRects = [&]() -> void {
                drawH      = state.mCollapsed ? (titleBarH + 2.0f) : mWindowSize.Y();
                windowRect = { .Min = mWindowPos,
                         .Max = FVector2f(mWindowPos.X() + mWindowSize.X(), mWindowPos.Y() + drawH) };
                titleRect  = { .Min = mWindowPos,
                          .Max =
                         FVector2f(mWindowPos.X() + mWindowSize.X(), mWindowPos.Y() + titleBarH) };
                btnMin  = FVector2f(mWindowPos.X() + mWindowSize.X() - kTitlePadX - kBtnSize,
                          mWindowPos.Y() + th.mCollapseButtonOffsetY);
                btnRect = { .Min = btnMin,
                         .Max         = FVector2f(btnMin.X() + kBtnSize, btnMin.Y() + kBtnSize) };
                mWindowRect = windowRect;
            };

            buildRects();

            // Collapse toggle interaction.
            const u64  collapseId = HashId(TEXT("##WindowCollapse"));
            const bool btnHovered = PointInRect(mInput.mMousePos, btnRect);
            if (btnHovered) {
                mUi->mHotId             = collapseId;
                mUi->mWantsCaptureMouse = true;
            }
            if (btnHovered && mInput.mMousePressed) {
                mUi->mActiveId = collapseId;
                mUi->mFocusId  = collapseId;
            }
            if (mUi->mActiveId == collapseId && mInput.mMouseReleased) {
                if (btnHovered) {
                    state.mCollapsed = !state.mCollapsed;
                }
                mUi->mActiveId = 0ULL;
            }

            // Drag interaction (on title bar excluding collapse button).
            const u64  dragId = HashId(TEXT("##WindowDrag"));
            const bool titleHovered =
                PointInRect(mInput.mMousePos, titleRect) && !PointInRect(mInput.mMousePos, btnRect);
            if (titleHovered && mInput.mMousePressed) {
                mUi->mActiveId          = dragId;
                mUi->mFocusId           = dragId;
                mUi->mWantsCaptureMouse = true;
                if (mDraggingWindowKey != nullptr) {
                    *mDraggingWindowKey = windowKey;
                }
                if (mDragOffset != nullptr) {
                    *mDragOffset = mInput.mMousePos - state.mPos;
                }
            }
            if (mUi->mActiveId == dragId && mInput.mMouseDown && mDraggingWindowKey != nullptr
                && *mDraggingWindowKey == windowKey && mDragOffset != nullptr) {
                state.mPos = mInput.mMousePos - *mDragOffset;
                mWindowPos = state.mPos;
            }
            if (mUi->mActiveId == dragId && mInput.mMouseReleased) {
                mUi->mActiveId = 0ULL;
                if (mDraggingWindowKey != nullptr) {
                    *mDraggingWindowKey = 0ULL;
                }
            }

            // Update rects after drag/collapse changes for correct visuals this frame.
            buildRects();

            const bool btnHoveredDraw = PointInRect(mInput.mMousePos, btnRect);

            const f32  windowRadius = th.mEditor.mWindowSurface.mCornerRadius;
            DrawRoundedRectFilled(windowRect, th.mWindowBg, windowRadius);
            if ((th.mWindowBorder >> 24U) != 0U) {
                DrawRoundedRect(windowRect, th.mWindowBorder, windowRadius, 1.0f);
            }
            DrawRoundedRectFilled(titleRect, th.mTitleBarBg, windowRadius);
            DrawTextStyled(FVector2f(mWindowPos.X() + th.mWindowPadding,
                               mWindowPos.Y() + th.mTitleTextOffsetY),
                th.mTitleText, title, EDebugGuiFontRole::WindowTitle);

            // Collapse button visuals.
            const bool     btnActive = (mUi->mActiveId == collapseId);
            const FColor32 btnBg     = btnActive
                    ? th.mCollapseButtonActiveBg
                    : (btnHoveredDraw ? th.mCollapseButtonHoverBg : th.mCollapseButtonBg);
            DrawRectFilled(btnRect, btnBg);
            DrawRect(btnRect, th.mCollapseButtonBorder, 1.0f);
            // Triangle icon (no font dependency).
            const FVector2f c((btnRect.Min.X() + btnRect.Max.X()) * 0.5f,
                (btnRect.Min.Y() + btnRect.Max.Y()) * 0.5f);
            const f32       hw = th.mCollapseIconHalfWidth;
            const f32       hh = th.mCollapseIconHalfHeight;
            if (state.mCollapsed) {
                // Down triangle.
                DrawTriangleFilled(FVector2f(c.X() - hw, c.Y() - hh),
                    FVector2f(c.X() + hw, c.Y() - hh), FVector2f(c.X(), c.Y() + hh),
                    th.mCollapseIcon);
            } else {
                // Up triangle.
                DrawTriangleFilled(FVector2f(c.X() - hw, c.Y() + hh),
                    FVector2f(c.X() + hw, c.Y() + hh), FVector2f(c.X(), c.Y() - hh),
                    th.mCollapseIcon);
            }

            const f32       pad = th.mWindowPadding;
            const FVector2f contentMin(mWindowPos.X() + pad, mWindowPos.Y() + titleBarH + pad);
            const FVector2f contentMax(
                mWindowPos.X() + mWindowSize.X() - pad, mWindowPos.Y() + mWindowSize.Y() - pad);

            mContentMin = contentMin;
            mContentMax = contentMax;
            mCursor     = contentMin;

            // Basic capture rules: if the mouse is over the window and interacting, capture it.
            if (PointInRect(mInput.mMousePos, windowRect)
                && (mInput.mMouseDown || mInput.mMousePressed || mInput.mMouseWheelDelta != 0.0f)) {
                mUi->mWantsCaptureMouse = true;
            }

            if (state.mCollapsed) {
                return false;
            }

            PushClipRect({ .Min = contentMin, .Max = contentMax });
            return true;
        }

        void EndWindow() override {
            PopClipRect();
            mCurrentWindowTitle.Clear();
        }

        void Text(FStringView text) override {
            if (text.IsEmpty()) {
                AdvanceLine();
                return;
            }
            DrawTextStyled(mCursor, mTheme->mText, text, EDebugGuiFontRole::Body);
            AdvanceLine();
        }

        void Separator() override {
            const f32       y = mCursor.Y() + mTheme->mSeparatorPaddingY;
            const FVector2f a(mContentMin.X(), y);
            const FVector2f b(mContentMax.X(), y);
            DrawLine(a, b, mTheme->mSeparator, 1.0f);
            mCursor = FVector2f(mCursor.X(), y + mTheme->mSeparatorPaddingY + 2.0f);
        }

        [[nodiscard]] auto Button(FStringView label) -> bool override;
        [[nodiscard]] auto Checkbox(FStringView label, bool& value) -> bool override;
        [[nodiscard]] auto SliderFloat(FStringView label, f32& value, f32 minValue, f32 maxValue)
            -> bool override;
        [[nodiscard]] auto InputText(FStringView label, Container::FString& value) -> bool override;
        [[nodiscard]] auto Gizmo(FStringView label, FVector2f& value) -> bool override;
        [[nodiscard]] auto TreeViewItem(const FTreeViewItemDesc& desc)
            -> FTreeViewItemResult override;
        [[nodiscard]] auto TextedIconView(const FTextedIconViewDesc& desc)
            -> FTextedIconViewResult override;

    private:
        [[nodiscard]] static auto SnapToPixel(f32 value) noexcept -> f32 {
            return static_cast<f32>(Core::Math::RoundedCast<i32>(value));
        }

        [[nodiscard]] auto GetGlyphWidth() const noexcept -> f32 {
            return FFontAtlas::GetGlyphWidth(mFontScale);
        }

        [[nodiscard]] auto GetGlyphHeight() const noexcept -> f32 {
            return FFontAtlas::GetGlyphHeight(mFontScale);
        }

        [[nodiscard]] auto ResolveFontScale(EDebugGuiFontRole role) const noexcept -> f32 {
            return ResolveFontRoleScale(*mTheme, role);
        }

        [[nodiscard]] auto GetGlyphWidth(EDebugGuiFontRole role) const noexcept -> f32 {
            return FFontAtlas::GetGlyphWidth(*mTheme, role);
        }

        [[nodiscard]] auto GetGlyphHeight(EDebugGuiFontRole role) const noexcept -> f32 {
            return FFontAtlas::GetGlyphHeight(*mTheme, role);
        }

        void AdvanceLine() {
            mCursor =
                FVector2f(mContentMin.X(), mCursor.Y() + GetGlyphHeight() + mTheme->mItemSpacingY);
        }

        void AdvanceItem(const FVector2f& itemSize) {
            mCursor =
                FVector2f(mContentMin.X(), mCursor.Y() + itemSize.Y() + mTheme->mItemSpacingY);
        }

        [[nodiscard]] auto CalcTextWidth(FStringView s, f32 fontScale) const noexcept -> f32 {
            if (mAtlas == nullptr) {
                return static_cast<f32>(s.Length()) * FFontAtlas::GetGlyphWidth(fontScale);
            }
            f32 width = 0.0f;
            for (usize i = 0; i < s.Length(); ++i) {
                const auto metrics = mAtlas->GetGlyphMetrics(static_cast<u32>(s[i]));
                const f32  adv     = (metrics.mAdvance > 0.0f) ? (metrics.mAdvance * fontScale)
                                                               : FFontAtlas::GetGlyphWidth(fontScale);
                width += adv;
            }
            return width;
        }

        [[nodiscard]] auto CalcTextWidth(FStringView s) const noexcept -> f32 {
            return CalcTextWidth(s, mFontScale);
        }

        [[nodiscard]] auto CalcButtonSize(FStringView label) const noexcept -> FVector2f {
            const f32 w = CalcTextWidth(label) + mTheme->mButtonPaddingX * 2.0f;
            const f32 h = GetGlyphHeight() + mTheme->mButtonPaddingY * 2.0f;
            return FVector2f(w, h);
        }

        [[nodiscard]] auto HashId(FStringView label) const noexcept -> u64 {
            // FNV-1a over TChar units, mixed with current window title.
            constexpr u64 kOffset = 1469598103934665603ULL;
            constexpr u64 kPrime  = 1099511628211ULL;
            u64           h       = kOffset;
            auto          mixView = [&](FStringView v) -> void {
                for (usize i = 0; i < v.Length(); ++i) {
                    h ^= static_cast<u64>(static_cast<u32>(v[i]));
                    h *= kPrime;
                }
            };
            mixView(mCurrentWindowTitle.ToView());
            mixView(label);
            return h;
        }

        [[nodiscard]] auto BuildChildHashId(FStringView prefix, FStringView label) const noexcept
            -> u64 {
            constexpr u64 kOffset = 1469598103934665603ULL;
            constexpr u64 kPrime  = 1099511628211ULL;
            u64           h       = kOffset;
            auto          mixView = [&](FStringView v) -> void {
                for (usize i = 0; i < v.Length(); ++i) {
                    h ^= static_cast<u64>(static_cast<u32>(v[i]));
                    h *= kPrime;
                }
            };
            mixView(mCurrentWindowTitle.ToView());
            mixView(prefix);
            mixView(label);
            return h;
        }

        [[nodiscard]] static auto HashWindowKey(FStringView title) noexcept -> u64 {
            constexpr u64 kOffset = 1469598103934665603ULL;
            constexpr u64 kPrime  = 1099511628211ULL;
            u64           h       = kOffset;
            for (usize i = 0; i < title.Length(); ++i) {
                h ^= static_cast<u64>(static_cast<u32>(title[i]));
                h *= kPrime;
            }
            return h;
        }

        void BeginCmdIfNeeded(EDrawTextureMode textureMode, u64 textureId = 0ULL) {
            if (mDrawData->mCmds.IsEmpty()) {
                FDrawCmd cmd{};
                cmd.mIndexOffset = 0U;
                cmd.mIndexCount  = 0U;
                cmd.mTextureId   = textureId;
                cmd.mTextureMode = textureMode;
                cmd.mClipRect    = mClip->Current(mDisplaySize);
                mDrawData->mCmds.PushBack(cmd);
                return;
            }

            const FRect cur  = mClip->Current(mDisplaySize);
            const FRect last = mDrawData->mCmds.Back().mClipRect;
            const bool  same = (cur.Min.X() == last.Min.X()) && (cur.Min.Y() == last.Min.Y())
                && (cur.Max.X() == last.Max.X()) && (cur.Max.Y() == last.Max.Y());
            const bool sameTexture = (mDrawData->mCmds.Back().mTextureId == textureId);
            const bool sameMode    = (mDrawData->mCmds.Back().mTextureMode == textureMode);
            if (!same || !sameTexture || !sameMode) {
                FDrawCmd cmd{};
                cmd.mIndexOffset = static_cast<u32>(mDrawData->mIndices.Size());
                cmd.mIndexCount  = 0U;
                cmd.mTextureId   = textureId;
                cmd.mTextureMode = textureMode;
                cmd.mClipRect    = cur;
                mDrawData->mCmds.PushBack(cmd);
            }
        }

        void AddQuad(const FVector2f& p0, const FVector2f& p1, const FVector2f& p2,
            const FVector2f& p3, f32 u0, f32 v0, f32 u1, f32 v1, FColor32 color,
            EDrawTextureMode textureMode = EDrawTextureMode::InternalFont, u64 textureId = 0ULL) {
            BeginCmdIfNeeded(textureMode, textureId);
            const u32 base = static_cast<u32>(mDrawData->mVertices.Size());
            mDrawData->mVertices.PushBack(
                { .mX = p0.X(), .mY = p0.Y(), .mU = u0, .mV = v0, .mColor = color });
            mDrawData->mVertices.PushBack(
                { .mX = p1.X(), .mY = p1.Y(), .mU = u1, .mV = v0, .mColor = color });
            mDrawData->mVertices.PushBack(
                { .mX = p2.X(), .mY = p2.Y(), .mU = u1, .mV = v1, .mColor = color });
            mDrawData->mVertices.PushBack(
                { .mX = p3.X(), .mY = p3.Y(), .mU = u0, .mV = v1, .mColor = color });

            mDrawData->mIndices.PushBack(base + 0U);
            mDrawData->mIndices.PushBack(base + 1U);
            mDrawData->mIndices.PushBack(base + 2U);
            mDrawData->mIndices.PushBack(base + 0U);
            mDrawData->mIndices.PushBack(base + 2U);
            mDrawData->mIndices.PushBack(base + 3U);

            mDrawData->mCmds.Back().mIndexCount += 6U;
        }

        void AddTriangleFilled(
            const FVector2f& p0, const FVector2f& p1, const FVector2f& p2, FColor32 color) {
            BeginCmdIfNeeded(EDrawTextureMode::InternalFont, 0ULL);
            const u32 base = static_cast<u32>(mDrawData->mVertices.Size());
            const f32 u    = (mSolidU0 + mSolidU1) * 0.5f;
            const f32 v    = (mSolidV0 + mSolidV1) * 0.5f;
            mDrawData->mVertices.PushBack(
                { .mX = p0.X(), .mY = p0.Y(), .mU = u, .mV = v, .mColor = color });
            mDrawData->mVertices.PushBack(
                { .mX = p1.X(), .mY = p1.Y(), .mU = u, .mV = v, .mColor = color });
            mDrawData->mVertices.PushBack(
                { .mX = p2.X(), .mY = p2.Y(), .mU = u, .mV = v, .mColor = color });

            mDrawData->mIndices.PushBack(base + 0U);
            mDrawData->mIndices.PushBack(base + 1U);
            mDrawData->mIndices.PushBack(base + 2U);

            mDrawData->mCmds.Back().mIndexCount += 3U;
        }

        void AddRectFilled(const FRect& rect, FColor32 color) {
            AddQuad(rect.Min, FVector2f(rect.Max.X(), rect.Min.Y()), rect.Max,
                FVector2f(rect.Min.X(), rect.Max.Y()), mSolidU0, mSolidV0, mSolidU1, mSolidV1,
                color);
        }

        void AddRect(const FRect& rect, FColor32 color, f32 thickness) {
            const f32 t = (thickness > 0.0f) ? thickness : 1.0f;
            AddRectFilled(
                { .Min = rect.Min, .Max = FVector2f(rect.Max.X(), rect.Min.Y() + t) }, color);
            AddRectFilled(
                { .Min = FVector2f(rect.Min.X(), rect.Max.Y() - t), .Max = rect.Max }, color);
            AddRectFilled({ .Min   = FVector2f(rect.Min.X(), rect.Min.Y() + t),
                              .Max = FVector2f(rect.Min.X() + t, rect.Max.Y() - t) },
                color);
            AddRectFilled({ .Min   = FVector2f(rect.Max.X() - t, rect.Min.Y() + t),
                              .Max = FVector2f(rect.Max.X(), rect.Max.Y() - t) },
                color);
        }

        [[nodiscard]] static auto ClampNonNegative(f32 v) noexcept -> f32 {
            return (v > 0.0f) ? v : 0.0f;
        }

        [[nodiscard]] static auto Min2(f32 a, f32 b) noexcept -> f32 { return (a < b) ? a : b; }

        [[nodiscard]] static auto CalcArcSegments90(f32 radius) noexcept -> u32 {
            // 90-degree arc segments. Keep small and deterministic for stable tests.
            // radius in pixels, typical GUI values: 2..16.
            const f32 r   = ClampNonNegative(radius);
            u32       seg = static_cast<u32>(r * 0.25f) + 4U;
            if (seg < 4U) {
                seg = 4U;
            }
            if (seg > 16U) {
                seg = 16U;
            }
            return seg;
        }

        void AddArcFilled(const FVector2f& center, f32 radius, f32 startAngleRad, f32 endAngleRad,
            u32 segments, FColor32 color) {
            if (radius <= 0.0f || segments == 0U) {
                return;
            }

            const f32 step = (endAngleRad - startAngleRad) / static_cast<f32>(segments);
            f32       a0   = startAngleRad;
            FVector2f p0(center.X() + Core::Math::Cos(a0) * radius,
                center.Y() + Core::Math::Sin(a0) * radius);
            for (u32 i = 0U; i < segments; ++i) {
                const f32       a1 = startAngleRad + step * static_cast<f32>(i + 1U);
                const FVector2f p1(center.X() + Core::Math::Cos(a1) * radius,
                    center.Y() + Core::Math::Sin(a1) * radius);
                AddTriangleFilled(center, p0, p1, color);
                p0 = p1;
            }
        }

        void AddArcStroke(const FVector2f& center, f32 radius, f32 startAngleRad, f32 endAngleRad,
            u32 segments, FColor32 color, f32 thickness) {
            if (radius <= 0.0f || segments == 0U) {
                return;
            }

            const f32 step = (endAngleRad - startAngleRad) / static_cast<f32>(segments);
            f32       a0   = startAngleRad;
            FVector2f p0(center.X() + Core::Math::Cos(a0) * radius,
                center.Y() + Core::Math::Sin(a0) * radius);
            for (u32 i = 0U; i < segments; ++i) {
                const f32       a1 = startAngleRad + step * static_cast<f32>(i + 1U);
                const FVector2f p1(center.X() + Core::Math::Cos(a1) * radius,
                    center.Y() + Core::Math::Sin(a1) * radius);
                AddLine(p0, p1, color, thickness);
                p0 = p1;
            }
        }

        void AddRoundedRectFilled(
            const FRect& rect, FColor32 color, f32 rounding, EDebugGuiCornerFlags cornerFlags) {
            const f32 w = rect.Max.X() - rect.Min.X();
            const f32 h = rect.Max.Y() - rect.Min.Y();
            if (w <= 0.0f || h <= 0.0f) {
                return;
            }

            f32 r = ClampNonNegative(rounding);
            r     = Min2(r, Min2(w * 0.5f, h * 0.5f));
            if (r <= 0.0f) {
                AddRectFilled(rect, color);
                return;
            }

            const f32 tl = HasAnyCornerFlag(cornerFlags, EDebugGuiCornerFlags::TopLeft) ? r : 0.0f;
            const f32 tr = HasAnyCornerFlag(cornerFlags, EDebugGuiCornerFlags::TopRight) ? r : 0.0f;
            const f32 br =
                HasAnyCornerFlag(cornerFlags, EDebugGuiCornerFlags::BottomRight) ? r : 0.0f;
            const f32 bl =
                HasAnyCornerFlag(cornerFlags, EDebugGuiCornerFlags::BottomLeft) ? r : 0.0f;

            const f32          minX = rect.Min.X();
            const f32          minY = rect.Min.Y();
            const f32          maxX = rect.Max.X();
            const f32          maxY = rect.Max.Y();

            TVector<FVector2f> points;
            points.Reserve(static_cast<usize>(CalcArcSegments90(r) * 4U + 8U));

            const auto pushPoint = [&points](const FVector2f& point) {
                if (!points.IsEmpty()) {
                    const auto& last = points.Back();
                    if (Core::Math::Abs(last.X() - point.X()) <= 0.001f
                        && Core::Math::Abs(last.Y() - point.Y()) <= 0.001f) {
                        return;
                    }
                }
                points.PushBack(point);
            };

            const auto appendArc = [&](const FVector2f& center, f32 radius, f32 startAngle,
                                       f32 endAngle) {
                if (radius <= 0.0f) {
                    return;
                }
                const u32 seg  = CalcArcSegments90(radius);
                const f32 step = (endAngle - startAngle) / static_cast<f32>(seg);
                for (u32 i = 0U; i <= seg; ++i) {
                    const f32 angle = startAngle + step * static_cast<f32>(i);
                    pushPoint(FVector2f(center.X() + Core::Math::Cos(angle) * radius,
                        center.Y() + Core::Math::Sin(angle) * radius));
                }
            };

            constexpr f32 kPi = Core::Math::kPiF;

            pushPoint(FVector2f(minX + tl, minY));
            pushPoint(FVector2f(maxX - tr, minY));
            if (tr > 0.0f) {
                appendArc(FVector2f(maxX - tr, minY + tr), tr, -0.5f * kPi, 0.0f);
            } else {
                pushPoint(FVector2f(maxX, minY));
            }

            pushPoint(FVector2f(maxX, maxY - br));
            if (br > 0.0f) {
                appendArc(FVector2f(maxX - br, maxY - br), br, 0.0f, 0.5f * kPi);
            } else {
                pushPoint(FVector2f(maxX, maxY));
            }

            pushPoint(FVector2f(minX + bl, maxY));
            if (bl > 0.0f) {
                appendArc(FVector2f(minX + bl, maxY - bl), bl, 0.5f * kPi, kPi);
            } else {
                pushPoint(FVector2f(minX, maxY));
            }

            pushPoint(FVector2f(minX, minY + tl));
            if (tl > 0.0f) {
                appendArc(FVector2f(minX + tl, minY + tl), tl, kPi, 1.5f * kPi);
            } else {
                pushPoint(FVector2f(minX, minY));
            }

            if (points.Size() < 3U) {
                AddRectFilled(rect, color);
                return;
            }

            const FVector2f center((minX + maxX) * 0.5f, (minY + maxY) * 0.5f);
            for (usize i = 0U; i < points.Size(); ++i) {
                const usize next = (i + 1U < points.Size()) ? (i + 1U) : 0U;
                AddTriangleFilled(center, points[i], points[next], color);
            }
        }

        void AddRoundedRect(const FRect& rect, FColor32 color, f32 rounding, f32 thickness,
            EDebugGuiCornerFlags cornerFlags) {
            const f32 w = rect.Max.X() - rect.Min.X();
            const f32 h = rect.Max.Y() - rect.Min.Y();
            if (w <= 0.0f || h <= 0.0f) {
                return;
            }

            f32 r = ClampNonNegative(rounding);
            r     = Min2(r, Min2(w * 0.5f, h * 0.5f));
            if (r <= 0.0f) {
                AddRect(rect, color, thickness);
                return;
            }

            const f32 tl = HasAnyCornerFlag(cornerFlags, EDebugGuiCornerFlags::TopLeft) ? r : 0.0f;
            const f32 tr = HasAnyCornerFlag(cornerFlags, EDebugGuiCornerFlags::TopRight) ? r : 0.0f;
            const f32 br =
                HasAnyCornerFlag(cornerFlags, EDebugGuiCornerFlags::BottomRight) ? r : 0.0f;
            const f32 bl =
                HasAnyCornerFlag(cornerFlags, EDebugGuiCornerFlags::BottomLeft) ? r : 0.0f;

            constexpr f32 kPi   = Core::Math::kPiF;
            const u32     seg90 = CalcArcSegments90(r);

            const f32     minX = rect.Min.X();
            const f32     minY = rect.Min.Y();
            const f32     maxX = rect.Max.X();
            const f32     maxY = rect.Max.Y();

            // Edges.
            AddLine(FVector2f(minX + tl, minY), FVector2f(maxX - tr, minY), color, thickness);
            AddLine(FVector2f(maxX, minY + tr), FVector2f(maxX, maxY - br), color, thickness);
            AddLine(FVector2f(maxX - br, maxY), FVector2f(minX + bl, maxY), color, thickness);
            AddLine(FVector2f(minX, maxY - bl), FVector2f(minX, minY + tl), color, thickness);

            // Corners.
            if (tr > 0.0f) {
                AddArcStroke(FVector2f(maxX - tr, minY + tr), tr, -0.5f * kPi, 0.0f, seg90, color,
                    thickness);
            }
            if (br > 0.0f) {
                AddArcStroke(
                    FVector2f(maxX - br, maxY - br), br, 0.0f, 0.5f * kPi, seg90, color, thickness);
            }
            if (bl > 0.0f) {
                AddArcStroke(
                    FVector2f(minX + bl, maxY - bl), bl, 0.5f * kPi, kPi, seg90, color, thickness);
            }
            if (tl > 0.0f) {
                AddArcStroke(
                    FVector2f(minX + tl, minY + tl), tl, kPi, 1.5f * kPi, seg90, color, thickness);
            }
        }

        void AddCapsuleFilled(const FVector2f& a, const FVector2f& b, f32 radius, FColor32 color) {
            const f32 r = ClampNonNegative(radius);
            if (r <= 0.0f) {
                return;
            }

            const FVector2f d    = b - a;
            const f32       len2 = d.X() * d.X() + d.Y() * d.Y();
            if (len2 <= 0.0001f) {
                // Degenerates to a circle.
                constexpr f32 kPi = Core::Math::kPiF;
                const u32     seg = CalcArcSegments90(r) * 4U;
                AddArcFilled(a, r, 0.0f, 2.0f * kPi, seg, color);
                return;
            }

            const f32       invLen = 1.0f / Core::Math::Sqrt(len2);
            const FVector2f u(d.X() * invLen, d.Y() * invLen);
            const FVector2f p(-u.Y(), u.X());
            const FVector2f off(p.X() * r, p.Y() * r);

            // Middle quad (non-overlapping with end caps).
            AddQuad(
                a + off, b + off, b - off, a - off, mSolidU0, mSolidV0, mSolidU1, mSolidV1, color);

            constexpr f32 kPi    = Core::Math::kPiF;
            const f32     ang    = Core::Math::Atan2(u.Y(), u.X());
            const u32     seg180 = CalcArcSegments90(r) * 2U;

            // End caps: semicircles meeting the quad edges at dot(u)==0.
            AddArcFilled(a, r, ang + 0.5f * kPi, ang + 1.5f * kPi, seg180, color);
            AddArcFilled(b, r, ang - 0.5f * kPi, ang + 0.5f * kPi, seg180, color);
        }

        void AddCapsule(
            const FVector2f& a, const FVector2f& b, f32 radius, FColor32 color, f32 thickness) {
            const f32 r = ClampNonNegative(radius);
            if (r <= 0.0f) {
                return;
            }

            const FVector2f d    = b - a;
            const f32       len2 = d.X() * d.X() + d.Y() * d.Y();
            if (len2 <= 0.0001f) {
                // Degenerates to a circle stroke.
                constexpr f32 kPi = Core::Math::kPiF;
                const u32     seg = CalcArcSegments90(r) * 4U;
                AddArcStroke(a, r, 0.0f, 2.0f * kPi, seg, color, thickness);
                return;
            }

            const f32       invLen = 1.0f / Core::Math::Sqrt(len2);
            const FVector2f u(d.X() * invLen, d.Y() * invLen);
            const FVector2f p(-u.Y(), u.X());
            const FVector2f off(p.X() * r, p.Y() * r);

            // Side edges.
            AddLine(a + off, b + off, color, thickness);
            AddLine(b - off, a - off, color, thickness);

            constexpr f32 kPi    = Core::Math::kPiF;
            const f32     ang    = Core::Math::Atan2(u.Y(), u.X());
            const u32     seg180 = CalcArcSegments90(r) * 2U;

            AddArcStroke(a, r, ang + 0.5f * kPi, ang + 1.5f * kPi, seg180, color, thickness);
            AddArcStroke(b, r, ang - 0.5f * kPi, ang + 0.5f * kPi, seg180, color, thickness);
        }

        void AddLine(const FVector2f& p0, const FVector2f& p1, FColor32 color, f32 thickness) {
            const f32       t    = (thickness > 0.0f) ? thickness : 1.0f;
            const FVector2f d    = p1 - p0;
            const f32       len2 = d.X() * d.X() + d.Y() * d.Y();
            if (len2 <= 0.0001f) {
                return;
            }
            const f32       invLen = 1.0f / Core::Math::Sqrt(len2);
            const FVector2f n(-d.Y() * invLen, d.X() * invLen);
            const f32       s = t * 0.5f;
            const FVector2f off(n.X() * s, n.Y() * s);
            AddQuad(p0 + off, p1 + off, p1 - off, p0 - off, mSolidU0, mSolidV0, mSolidU1, mSolidV1,
                color);
        }

        void AddText(const FVector2f& pos, FColor32 color, FStringView text, f32 fontScale) {
            if (text.IsEmpty()) {
                return;
            }
            const f32 glyphW = FFontAtlas::GetGlyphWidth(fontScale);
            const f32 glyphH = FFontAtlas::GetGlyphHeight(fontScale);
            const f32 baseX  = SnapToPixel(pos.X());
            FVector2f cursor(baseX, SnapToPixel(pos.Y()));
            for (usize i = 0; i < text.Length(); ++i) {
                const TChar c = text[i];
                if (c == static_cast<TChar>('\n')) {
                    cursor = FVector2f(baseX, SnapToPixel(cursor.Y() + glyphH));
                    continue;
                }
                if (c == static_cast<TChar>('\r')) {
                    continue;
                }

                f32 u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
                mAtlas->GetGlyphUV(static_cast<u32>(c), u0, v0, u1, v1);

                const f32       x0 = cursor.X();
                const f32       y0 = cursor.Y();
                const f32       x1 = cursor.X() + glyphW;
                const f32       y1 = cursor.Y() + glyphH;

                const FVector2f p0(x0, y0);
                const FVector2f p1(x1, y0);
                const FVector2f p2(x1, y1);
                const FVector2f p3(x0, y1);
                AddQuad(p0, p1, p2, p3, u0, v0, u1, v1, color);
                const auto metrics = mAtlas->GetGlyphMetrics(static_cast<u32>(c));
                const f32 adv = (metrics.mAdvance > 0.0f) ? (metrics.mAdvance * fontScale) : glyphW;
                cursor        = FVector2f(cursor.X() + adv, cursor.Y());
            }
        }

        FDrawData*                              mDrawData = nullptr;
        FClipRectStack*                         mClip     = nullptr;
        FGuiInput                               mInput{};
        FUIState*                               mUi          = nullptr;
        FVector2f                               mDisplaySize = FVector2f(0.0f, 0.0f);
        const FFontAtlas*                       mAtlas       = nullptr;
        const FIconAtlas*                       mIconAtlas   = nullptr;
        const FDebugGuiTheme*                   mTheme       = nullptr;
        f32                                     mFontScale   = 1.0f;
        f32                                     mSolidU0     = 0.0f;
        f32                                     mSolidV0     = 0.0f;
        f32                                     mSolidU1     = 0.0f;
        f32                                     mSolidV1     = 0.0f;

        FVector2f                               mWindowPos  = FVector2f(0.0f, 0.0f);
        FVector2f                               mWindowSize = FVector2f(0.0f, 0.0f);
        FVector2f                               mContentMin = FVector2f(0.0f, 0.0f);
        FVector2f                               mContentMax = FVector2f(0.0f, 0.0f);
        FVector2f                               mCursor     = FVector2f(0.0f, 0.0f);
        FRect                                   mWindowRect{};
        FString                                 mCurrentWindowTitle;
        TVector<FString>*                       mWindowOrder       = nullptr;
        Container::THashMap<u64, FWindowState>* mWindows           = nullptr;
        u64*                                    mDraggingWindowKey = nullptr;
        FVector2f*                              mDragOffset        = nullptr;
    };
} // namespace AltinaEngine::DebugGui::Private
