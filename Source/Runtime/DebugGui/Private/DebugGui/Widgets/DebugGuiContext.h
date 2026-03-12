#pragma once

#include "DebugGui/Core/DebugGuiCoreTypes.h"
#include "DebugGui/Core/FontAtlas.h"
#include "Math/Common.h"

namespace AltinaEngine::DebugGui::Private {
    class FDebugGuiContext final : public IDebugGui {
    public:
        explicit FDebugGuiContext(FDrawData& drawData, FClipRectStack& clip, const FGuiInput& input,
            FUIState& ui, const FVector2f& displaySize, const FFontAtlas& atlas,
            const FDebugGuiTheme& theme, TVector<FString>& windowOrder,
            Container::THashMap<u64, FWindowState>& windows, u64& draggingWindowKey,
            FVector2f& dragOffset)
            : mDrawData(&drawData)
            , mClip(&clip)
            , mInput(input)
            , mUi(&ui)
            , mDisplaySize(displaySize)
            , mAtlas(&atlas)
            , mTheme(&theme)
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
        void DrawRoundedRectFilled(const FRect& rect, FColor32 color, f32 rounding) override {
            AddRoundedRectFilled(rect, color, rounding);
        }
        void DrawRoundedRect(
            const FRect& rect, FColor32 color, f32 rounding, f32 thickness) override {
            AddRoundedRect(rect, color, rounding, thickness);
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
            AddText(pos, color, text);
        }
        void DrawImage(const FRect& rect, u64 imageId, FColor32 tint) override {
            if (imageId == 0ULL) {
                return;
            }
            AddQuad(rect.Min, FVector2f(rect.Max.X(), rect.Min.Y()), rect.Max,
                FVector2f(rect.Min.X(), rect.Max.Y()), 0.0f, 0.0f, 1.0f, 1.0f, tint, imageId);
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

            DrawRectFilled(windowRect, th.mWindowBg);
            DrawRect(windowRect, th.mWindowBorder, 1.0f);
            DrawRectFilled(titleRect, th.mTitleBarBg);
            DrawText(FVector2f(
                         mWindowPos.X() + th.mWindowPadding, mWindowPos.Y() + th.mTitleTextOffsetY),
                th.mTitleText, title);

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
            DrawText(mCursor, mTheme->mText, text);
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
        void AdvanceLine() {
            mCursor = FVector2f(mContentMin.X(),
                mCursor.Y() + static_cast<f32>(FFontAtlas::kDrawGlyphH) + mTheme->mItemSpacingY);
        }

        void AdvanceItem(const FVector2f& itemSize) {
            mCursor =
                FVector2f(mContentMin.X(), mCursor.Y() + itemSize.Y() + mTheme->mItemSpacingY);
        }

        static [[nodiscard]] auto CalcTextWidth(FStringView s) noexcept -> f32 {
            return static_cast<f32>(s.Length()) * static_cast<f32>(FFontAtlas::kDrawGlyphW);
        }

        [[nodiscard]] auto CalcButtonSize(FStringView label) const noexcept -> FVector2f {
            const f32 w = CalcTextWidth(label) + mTheme->mButtonPaddingX * 2.0f;
            const f32 h =
                static_cast<f32>(FFontAtlas::kDrawGlyphH) + mTheme->mButtonPaddingY * 2.0f;
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

        void BeginCmdIfNeeded(u64 textureId) {
            if (mDrawData->mCmds.IsEmpty()) {
                FDrawCmd cmd{};
                cmd.mIndexOffset = 0U;
                cmd.mIndexCount  = 0U;
                cmd.mTextureId   = textureId;
                cmd.mClipRect    = mClip->Current(mDisplaySize);
                mDrawData->mCmds.PushBack(cmd);
                return;
            }

            const FRect cur  = mClip->Current(mDisplaySize);
            const FRect last = mDrawData->mCmds.Back().mClipRect;
            const bool  same = (cur.Min.X() == last.Min.X()) && (cur.Min.Y() == last.Min.Y())
                && (cur.Max.X() == last.Max.X()) && (cur.Max.Y() == last.Max.Y());
            const bool sameTexture = (mDrawData->mCmds.Back().mTextureId == textureId);
            if (!same || !sameTexture) {
                FDrawCmd cmd{};
                cmd.mIndexOffset = static_cast<u32>(mDrawData->mIndices.Size());
                cmd.mIndexCount  = 0U;
                cmd.mTextureId   = textureId;
                cmd.mClipRect    = cur;
                mDrawData->mCmds.PushBack(cmd);
            }
        }

        void AddQuad(const FVector2f& p0, const FVector2f& p1, const FVector2f& p2,
            const FVector2f& p3, f32 u0, f32 v0, f32 u1, f32 v1, FColor32 color,
            u64 textureId = 0ULL) {
            BeginCmdIfNeeded(textureId);
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
            BeginCmdIfNeeded(0ULL);
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

        void AddRoundedRectFilled(const FRect& rect, FColor32 color, f32 rounding) {
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

            const f32 minX = rect.Min.X();
            const f32 minY = rect.Min.Y();
            const f32 maxX = rect.Max.X();
            const f32 maxY = rect.Max.Y();

            // 5 non-overlapping quads + 4 corner sectors.
            AddRectFilled(
                { .Min = FVector2f(minX + r, minY + r), .Max = FVector2f(maxX - r, maxY - r) },
                color);
            AddRectFilled(
                { .Min = FVector2f(minX + r, minY), .Max = FVector2f(maxX - r, minY + r) }, color);
            AddRectFilled(
                { .Min = FVector2f(minX + r, maxY - r), .Max = FVector2f(maxX - r, maxY) }, color);
            AddRectFilled(
                { .Min = FVector2f(minX, minY + r), .Max = FVector2f(minX + r, maxY - r) }, color);
            AddRectFilled(
                { .Min = FVector2f(maxX - r, minY + r), .Max = FVector2f(maxX, maxY - r) }, color);

            constexpr f32 kPi   = Core::Math::kPiF;
            const u32     seg90 = CalcArcSegments90(r);

            // Top-left (pi .. 3pi/2).
            AddArcFilled(FVector2f(minX + r, minY + r), r, kPi, 1.5f * kPi, seg90, color);
            // Top-right (-pi/2 .. 0).
            AddArcFilled(FVector2f(maxX - r, minY + r), r, -0.5f * kPi, 0.0f, seg90, color);
            // Bottom-right (0 .. pi/2).
            AddArcFilled(FVector2f(maxX - r, maxY - r), r, 0.0f, 0.5f * kPi, seg90, color);
            // Bottom-left (pi/2 .. pi).
            AddArcFilled(FVector2f(minX + r, maxY - r), r, 0.5f * kPi, kPi, seg90, color);
        }

        void AddRoundedRect(const FRect& rect, FColor32 color, f32 rounding, f32 thickness) {
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

            constexpr f32 kPi   = Core::Math::kPiF;
            const u32     seg90 = CalcArcSegments90(r);

            const f32     minX = rect.Min.X();
            const f32     minY = rect.Min.Y();
            const f32     maxX = rect.Max.X();
            const f32     maxY = rect.Max.Y();

            // Edges.
            AddLine(FVector2f(minX + r, minY), FVector2f(maxX - r, minY), color, thickness);
            AddLine(FVector2f(maxX, minY + r), FVector2f(maxX, maxY - r), color, thickness);
            AddLine(FVector2f(maxX - r, maxY), FVector2f(minX + r, maxY), color, thickness);
            AddLine(FVector2f(minX, maxY - r), FVector2f(minX, minY + r), color, thickness);

            // Corners.
            AddArcStroke(
                FVector2f(maxX - r, minY + r), r, -0.5f * kPi, 0.0f, seg90, color, thickness);
            AddArcStroke(
                FVector2f(maxX - r, maxY - r), r, 0.0f, 0.5f * kPi, seg90, color, thickness);
            AddArcStroke(
                FVector2f(minX + r, maxY - r), r, 0.5f * kPi, kPi, seg90, color, thickness);
            AddArcStroke(
                FVector2f(minX + r, minY + r), r, kPi, 1.5f * kPi, seg90, color, thickness);
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

        void AddText(const FVector2f& pos, FColor32 color, FStringView text) {
            if (text.IsEmpty()) {
                return;
            }
            FVector2f cursor = pos;
            for (usize i = 0; i < text.Length(); ++i) {
                const TChar c = text[i];
                if (c == static_cast<TChar>('\n')) {
                    cursor = FVector2f(pos.X(), cursor.Y() + FFontAtlas::kDrawGlyphH);
                    continue;
                }
                if (c == static_cast<TChar>('\r')) {
                    continue;
                }

                f32 u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
                mAtlas->GetGlyphUV(static_cast<u32>(c), u0, v0, u1, v1);

                const FVector2f p0 = cursor;
                const FVector2f p1(
                    cursor.X() + static_cast<f32>(FFontAtlas::kDrawGlyphW), cursor.Y());
                const FVector2f p2(cursor.X() + static_cast<f32>(FFontAtlas::kDrawGlyphW),
                    cursor.Y() + static_cast<f32>(FFontAtlas::kDrawGlyphH));
                const FVector2f p3(
                    cursor.X(), cursor.Y() + static_cast<f32>(FFontAtlas::kDrawGlyphH));
                AddQuad(p0, p1, p2, p3, u0, v0, u1, v1, color);
                cursor =
                    FVector2f(cursor.X() + static_cast<f32>(FFontAtlas::kDrawGlyphW), cursor.Y());
            }
        }

        FDrawData*                              mDrawData = nullptr;
        FClipRectStack*                         mClip     = nullptr;
        FGuiInput                               mInput{};
        FUIState*                               mUi          = nullptr;
        FVector2f                               mDisplaySize = FVector2f(0.0f, 0.0f);
        const FFontAtlas*                       mAtlas       = nullptr;
        const FDebugGuiTheme*                   mTheme       = nullptr;
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
