#include "DebugGui/DebugGui.h"

#include "CoreMinimal.h"

#include "Container/SmartPtr.h"
#include "Container/String.h"
#include "Container/HashMap.h"
#include "Container/Vector.h"
#include "Logging/Log.h"
#include "Console/ConsoleVariable.h"
#include "Platform/PlatformFileSystem.h"
#include "Platform/Generic/GenericPlatformDecl.h"
#include "Threading/Mutex.h"

#include "Input/InputSystem.h"
#include "Input/Keys.h"

#include "Rhi/Command/RhiCmdContextAdapter.h"
#include "Rhi/RhiBindGroup.h"
#include "Rhi/RhiBindGroupLayout.h"
#include "Rhi/RhiBuffer.h"
#include "Rhi/RhiDevice.h"
#include "Rhi/RhiInit.h"
#include "Rhi/RhiPipeline.h"
#include "Rhi/RhiPipelineLayout.h"
#include "Rhi/RhiQueue.h"
#include "Rhi/RhiResourceView.h"
#include "Rhi/RhiSampler.h"
#include "Rhi/RhiShader.h"
#include "Rhi/RhiStructs.h"
#include "Rhi/RhiTexture.h"
#include "Rhi/RhiViewport.h"

#include "ShaderCompiler/ShaderCompiler.h"
#include "ShaderCompiler/ShaderRhiBindings.h"
#include "Shader/ShaderTypes.h"

#include "Utility/Filesystem/Path.h"
#include "Utility/Filesystem/PathUtils.h"

#include <cmath>

namespace AltinaEngine::DebugGui {
    namespace {
        using AltinaEngine::Move;
        namespace Container = Core::Container;
        using Container::DestroyPolymorphic;
        using Container::FString;
        using Container::FStringView;
        using Container::MakeUniqueAs;
        using Container::TVector;
        using Core::Logging::ELogLevel;
        using Core::Logging::LogError;
        using Core::Math::FVector2f;
        using Core::Threading::FMutex;
        using Core::Threading::FScopedLock;
        using Core::Utility::Filesystem::FPath;

        [[nodiscard]] auto MapSampledTextureBinding(u32 binding) noexcept -> u32 {
            return (Rhi::RHIGetBackend() == Rhi::ERhiBackend::Vulkan) ? (1000U + binding) : binding;
        }

        [[nodiscard]] auto MapSamplerBinding(u32 binding) noexcept -> u32 {
            return (Rhi::RHIGetBackend() == Rhi::ERhiBackend::Vulkan) ? (2000U + binding) : binding;
        }

        struct FDrawVertex {
            f32 X     = 0.0f;
            f32 Y     = 0.0f;
            f32 U     = 0.0f;
            f32 V     = 0.0f;
            u32 Color = 0U; // RGBA8
        };

        struct FDrawCmd {
            u32   IndexCount  = 0U;
            u32   IndexOffset = 0U;
            FRect ClipRect{};
        };

        struct FDrawData {
            TVector<FDrawVertex> Vertices;
            TVector<u32>         Indices;
            TVector<FDrawCmd>    Cmds;

            void                 Clear() {
                Vertices.Clear();
                Indices.Clear();
                Cmds.Clear();
            }
        };

        struct FClipRectStack {
            TVector<FRect> Stack;

            void           Clear() { Stack.Clear(); }
            void           Push(const FRect& r) { Stack.PushBack(r); }
            void           Pop() {
                if (!Stack.IsEmpty()) {
                    Stack.PopBack();
                }
            }

            [[nodiscard]] auto Current(const FVector2f& displaySize) const noexcept -> FRect {
                if (Stack.IsEmpty()) {
                    return { FVector2f(0.0f, 0.0f), displaySize };
                }
                return Stack.Back();
            }
        };

        struct FUIState {
            u64  HotId                 = 0ULL;
            u64  ActiveId              = 0ULL;
            u64  FocusId               = 0ULL;
            bool bWantsCaptureMouse    = false;
            bool bWantsCaptureKeyboard = false;

            void ClearTransient() noexcept {
                HotId                 = 0ULL;
                bWantsCaptureMouse    = false;
                bWantsCaptureKeyboard = false;
            }
        };

        [[nodiscard]] auto PointInRect(const FVector2f& p, const FRect& r) noexcept -> bool {
            return p.X() >= r.Min.X() && p.X() < r.Max.X() && p.Y() >= r.Min.Y()
                && p.Y() < r.Max.Y();
        }

        [[nodiscard]] auto IntersectRect(const FRect& a, const FRect& b) noexcept -> FRect {
            const f32 minX = (a.Min.X() > b.Min.X()) ? a.Min.X() : b.Min.X();
            const f32 minY = (a.Min.Y() > b.Min.Y()) ? a.Min.Y() : b.Min.Y();
            const f32 maxX = (a.Max.X() < b.Max.X()) ? a.Max.X() : b.Max.X();
            const f32 maxY = (a.Max.Y() < b.Max.Y()) ? a.Max.Y() : b.Max.Y();
            return { FVector2f(minX, minY), FVector2f(maxX, maxY) };
        }

#include "FontAtlas32x32.inl"

        struct FGuiInput {
            const Input::FInputSystem* Input                = nullptr;
            FVector2f                  MousePos             = FVector2f(0.0f, 0.0f);
            bool                       bMouseDown           = false;
            bool                       bMousePressed        = false;
            bool                       bMouseReleased       = false;
            f32                        MouseWheelDelta      = 0.0f;
            bool                       bKeyEnterPressed     = false;
            bool                       bKeyBackspacePressed = false;
        };

        struct FWindowState {
            bool      bInitialized = false;
            bool      bCollapsed   = false;
            FVector2f Pos          = FVector2f(10.0f, 10.0f);
            FVector2f Size         = FVector2f(460.0f, 260.0f);
        };

        struct FFontAtlas {
            // Atlas glyph size (texture resolution per glyph).
            static constexpr u32 kAtlasGlyphW = 32U;
            static constexpr u32 kAtlasGlyphH = 32U;
            // Draw glyph size (screen-space size per glyph). Keep the DebugGui layout stable.
            static constexpr u32 kDrawGlyphW = 7U;
            static constexpr u32 kDrawGlyphH = 11U;
            static constexpr u32 kFirstChar  = 32U;
            static constexpr u32 kLastChar   = 126U;
            static constexpr u32 kGlyphCount = (kLastChar - kFirstChar + 1U);
            static constexpr u32 kCols       = 16U;
            static constexpr u32 kRows       = (kGlyphCount + kCols - 1U) / kCols;
            static constexpr u32 kAtlasW     = kCols * kAtlasGlyphW;
            static constexpr u32 kAtlasH     = kRows * kAtlasGlyphH;
            // Use the unused last cell (index 95) as a reserved solid texel for non-text
            // primitives. Glyphs occupy indices [0..kGlyphCount-1] => [0..94]. With 16 cols, we
            // have 96 cells total.
            static constexpr u32 kSolidTexelX = (kCols - 1U) * kAtlasGlyphW;
            static constexpr u32 kSolidTexelY = (kRows - 1U) * kAtlasGlyphH;

            TVector<u8>          Pixels; // RGBA8

            void                 Build() {
                Pixels.Resize(static_cast<usize>(kAtlasW) * static_cast<usize>(kAtlasH) * 4U);
                for (usize i = 0; i < Pixels.Size(); ++i) {
                    Pixels[i] = 0U;
                }

                for (u32 ch = kFirstChar; ch <= kLastChar; ++ch) {
                    const u32 glyphIndex = ch - kFirstChar;
                    const u32 col        = glyphIndex % kCols;
                    const u32 row        = glyphIndex / kCols;
                    const u32 baseX      = col * kAtlasGlyphW;
                    const u32 baseY      = row * kAtlasGlyphH;

                    const u8* glyph = GetFont32x32Glyph(static_cast<u8>(ch));
                    for (u32 y = 0U; y < kAtlasGlyphH; ++y) {
                        for (u32 x = 0U; x < kAtlasGlyphW; ++x) {
                            const u8    a = glyph ? glyph[y * kAtlasGlyphW + x] : 0U;
                            const u32   px = baseX + x;
                            const u32   py = baseY + y;
                            const usize idx = (static_cast<usize>(py) * kAtlasW + px) * 4U;
                            Pixels[idx + 0U] = 255U;
                            Pixels[idx + 1U] = 255U;
                            Pixels[idx + 2U] = 255U;
                            Pixels[idx + 3U] = a;
                        }
                    }
                }

                // Reserve a solid white cell (RGBA=1) for non-text primitives.
                // Fill the entire cell so it is robust under linear filtering.
                for (u32 y = 0U; y < kAtlasGlyphH; ++y) {
                    for (u32 x = 0U; x < kAtlasGlyphW; ++x) {
                        const u32   px = kSolidTexelX + x;
                        const u32   py = kSolidTexelY + y;
                        const usize idx = (static_cast<usize>(py) * kAtlasW + px) * 4U;
                        if (idx + 3U >= Pixels.Size()) {
                            continue;
                        }
                        Pixels[idx + 0U] = 255U;
                        Pixels[idx + 1U] = 255U;
                        Pixels[idx + 2U] = 255U;
                        Pixels[idx + 3U] = 255U;
                    }
                }
            }

            void GetGlyphUV(u32 ch, f32& outU0, f32& outV0, f32& outU1, f32& outV1) const noexcept {
                if (ch < kFirstChar || ch > kLastChar) {
                    ch = static_cast<u32>('?');
                }
                const u32 glyphIndex = ch - kFirstChar;
                const u32 col        = glyphIndex % kCols;
                const u32 row        = glyphIndex / kCols;
                const f32 invW       = 1.0f / static_cast<f32>(kAtlasW);
                const f32 invH       = 1.0f / static_cast<f32>(kAtlasH);
                const f32 x0         = static_cast<f32>(col * kAtlasGlyphW);
                const f32 y0         = static_cast<f32>(row * kAtlasGlyphH);
                const f32 x1         = x0 + static_cast<f32>(kAtlasGlyphW);
                const f32 y1         = y0 + static_cast<f32>(kAtlasGlyphH);
                // Inset by half a texel to avoid linear-filtering bleed from adjacent cells.
                outU0 = (x0 + 0.5f) * invW;
                outV0 = (y0 + 0.5f) * invH;
                outU1 = (x1 - 0.5f) * invW;
                outV1 = (y1 - 0.5f) * invH;
            }
        };

        class FDebugGuiContext final : public IDebugGui {
        public:
            explicit FDebugGuiContext(FDrawData& drawData, FClipRectStack& clip,
                const FGuiInput& input, FUIState& ui, const FVector2f& displaySize,
                const FFontAtlas& atlas, const FDebugGuiTheme& theme, TVector<FString>& windowOrder,
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
            [[nodiscard]] auto GetTheme() const noexcept -> const FDebugGuiTheme& {
                return *mTheme;
            }
            [[nodiscard]] auto IsMouseHoveringRect(const FRect& rect) const noexcept -> bool {
                return PointInRect(mInput.MousePos, rect);
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

            [[nodiscard]] auto GetDisplaySize() const noexcept -> FVector2f override {
                return mDisplaySize;
            }
            [[nodiscard]] auto GetMousePos() const noexcept -> FVector2f override {
                return mInput.MousePos;
            }

            bool BeginWindow(FStringView title, bool* open) override {
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
                if (!state.bInitialized) {
                    state.bInitialized = true;
                    state.Size         = th.WindowDefaultSize;
                    state.Pos          = th.WindowDefaultPos;

                    // Built-in panels: keep Console hidden by default, and start Stats/CVars in a
                    // collapsed state to reduce screen clutter on first launch.
                    if (title == FStringView(TEXT("DebugGui Stats"))
                        || title == FStringView(TEXT("DebugGui CVars"))) {
                        state.bCollapsed = true;
                    }

                    if (mWindowOrder != nullptr) {
                        // Place windows in columns if the display height is too small for pure
                        // vertical stacking.
                        const f32 cellW   = state.Size.X() + th.WindowSpacing;
                        const f32 cellH   = state.Size.Y() + th.WindowSpacing;
                        const f32 usableH = (mDisplaySize.Y() > th.WindowDefaultPos.Y())
                            ? (mDisplaySize.Y() - th.WindowDefaultPos.Y())
                            : 0.0f;
                        const u32 perCol =
                            (usableH > cellH) ? static_cast<u32>(usableH / cellH) : 1U;

                        const u32 col = static_cast<u32>(windowIndex) / perCol;
                        const u32 row = static_cast<u32>(windowIndex) % perCol;
                        state.Pos =
                            FVector2f(th.WindowDefaultPos.X() + static_cast<f32>(col) * cellW,
                                th.WindowDefaultPos.Y() + static_cast<f32>(row) * cellH);
                    }
                }

                mWindowPos  = state.Pos;
                mWindowSize = state.Size;

                const f32 titleBarH = th.TitleBarHeight;

                // Title bar collapse button (right side).
                const f32 kTitlePadX = th.CollapseButtonPadX;
                const f32 kBtnSize   = th.CollapseButtonSize;

                f32       drawH = 0.0f;
                FRect     windowRect{};
                FRect     titleRect{};
                FVector2f btnMin(0.0f, 0.0f);
                FRect     btnRect{};

                auto      BuildRects = [&]() {
                    drawH      = state.bCollapsed ? (titleBarH + 2.0f) : mWindowSize.Y();
                    windowRect = { mWindowPos,
                        FVector2f(mWindowPos.X() + mWindowSize.X(), mWindowPos.Y() + drawH) };
                    titleRect  = { mWindowPos,
                         FVector2f(mWindowPos.X() + mWindowSize.X(), mWindowPos.Y() + titleBarH) };
                    btnMin = FVector2f(mWindowPos.X() + mWindowSize.X() - kTitlePadX - kBtnSize,
                             mWindowPos.Y() + th.CollapseButtonOffsetY);
                    btnRect = { btnMin, FVector2f(btnMin.X() + kBtnSize, btnMin.Y() + kBtnSize) };
                    mWindowRect = windowRect;
                };

                BuildRects();

                // Collapse toggle interaction.
                const u64  collapseId = HashId(TEXT("##WindowCollapse"));
                const bool btnHovered = PointInRect(mInput.MousePos, btnRect);
                if (btnHovered) {
                    mUi->HotId              = collapseId;
                    mUi->bWantsCaptureMouse = true;
                }
                if (btnHovered && mInput.bMousePressed) {
                    mUi->ActiveId = collapseId;
                    mUi->FocusId  = collapseId;
                }
                if (mUi->ActiveId == collapseId && mInput.bMouseReleased) {
                    if (btnHovered) {
                        state.bCollapsed = !state.bCollapsed;
                    }
                    mUi->ActiveId = 0ULL;
                }

                // Drag interaction (on title bar excluding collapse button).
                const u64  dragId       = HashId(TEXT("##WindowDrag"));
                const bool titleHovered = PointInRect(mInput.MousePos, titleRect)
                    && !PointInRect(mInput.MousePos, btnRect);
                if (titleHovered && mInput.bMousePressed) {
                    mUi->ActiveId           = dragId;
                    mUi->FocusId            = dragId;
                    mUi->bWantsCaptureMouse = true;
                    if (mDraggingWindowKey != nullptr) {
                        *mDraggingWindowKey = windowKey;
                    }
                    if (mDragOffset != nullptr) {
                        *mDragOffset = mInput.MousePos - state.Pos;
                    }
                }
                if (mUi->ActiveId == dragId && mInput.bMouseDown && mDraggingWindowKey != nullptr
                    && *mDraggingWindowKey == windowKey && mDragOffset != nullptr) {
                    state.Pos  = mInput.MousePos - *mDragOffset;
                    mWindowPos = state.Pos;
                }
                if (mUi->ActiveId == dragId && mInput.bMouseReleased) {
                    mUi->ActiveId = 0ULL;
                    if (mDraggingWindowKey != nullptr) {
                        *mDraggingWindowKey = 0ULL;
                    }
                }

                // Update rects after drag/collapse changes for correct visuals this frame.
                BuildRects();

                const bool btnHoveredDraw = PointInRect(mInput.MousePos, btnRect);

                DrawRectFilled(windowRect, th.WindowBg);
                DrawRect(windowRect, th.WindowBorder, 1.0f);
                DrawRectFilled(titleRect, th.TitleBarBg);
                DrawText(FVector2f(mWindowPos.X() + th.WindowPadding,
                             mWindowPos.Y() + th.TitleTextOffsetY),
                    th.TitleText, title);

                // Collapse button visuals.
                const bool     btnActive = (mUi->ActiveId == collapseId);
                const FColor32 btnBg     = btnActive
                        ? th.CollapseButtonActiveBg
                        : (btnHoveredDraw ? th.CollapseButtonHoverBg : th.CollapseButtonBg);
                DrawRectFilled(btnRect, btnBg);
                DrawRect(btnRect, th.CollapseButtonBorder, 1.0f);
                // Triangle icon (no font dependency).
                const FVector2f c((btnRect.Min.X() + btnRect.Max.X()) * 0.5f,
                    (btnRect.Min.Y() + btnRect.Max.Y()) * 0.5f);
                const f32       hw = th.CollapseIconHalfWidth;
                const f32       hh = th.CollapseIconHalfHeight;
                if (state.bCollapsed) {
                    // Down triangle.
                    DrawTriangleFilled(FVector2f(c.X() - hw, c.Y() - hh),
                        FVector2f(c.X() + hw, c.Y() - hh), FVector2f(c.X(), c.Y() + hh),
                        th.CollapseIcon);
                } else {
                    // Up triangle.
                    DrawTriangleFilled(FVector2f(c.X() - hw, c.Y() + hh),
                        FVector2f(c.X() + hw, c.Y() + hh), FVector2f(c.X(), c.Y() - hh),
                        th.CollapseIcon);
                }

                const f32       pad = th.WindowPadding;
                const FVector2f contentMin(mWindowPos.X() + pad, mWindowPos.Y() + titleBarH + pad);
                const FVector2f contentMax(
                    mWindowPos.X() + mWindowSize.X() - pad, mWindowPos.Y() + mWindowSize.Y() - pad);

                mContentMin = contentMin;
                mContentMax = contentMax;
                mCursor     = contentMin;

                // Basic capture rules: if the mouse is over the window and interacting, capture it.
                if (PointInRect(mInput.MousePos, windowRect)
                    && (mInput.bMouseDown || mInput.bMousePressed
                        || mInput.MouseWheelDelta != 0.0f)) {
                    mUi->bWantsCaptureMouse = true;
                }

                if (state.bCollapsed) {
                    return false;
                }

                PushClipRect({ contentMin, contentMax });
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
                DrawText(mCursor, mTheme->Text, text);
                AdvanceLine();
            }

            void Separator() override {
                const f32       y = mCursor.Y() + mTheme->SeparatorPaddingY;
                const FVector2f a(mContentMin.X(), y);
                const FVector2f b(mContentMax.X(), y);
                DrawLine(a, b, mTheme->Separator, 1.0f);
                mCursor = FVector2f(mCursor.X(), y + mTheme->SeparatorPaddingY + 2.0f);
            }

            [[nodiscard]] bool Button(FStringView label) override {
                const u64       id   = HashId(label);
                const FVector2f size = CalcButtonSize(label);
                const FRect r{ mCursor, FVector2f(mCursor.X() + size.X(), mCursor.Y() + size.Y()) };

                const bool  hovered = PointInRect(mInput.MousePos, r);
                if (hovered) {
                    mUi->HotId              = id;
                    mUi->bWantsCaptureMouse = true;
                }

                bool pressed = false;
                if (hovered && mInput.bMousePressed) {
                    mUi->ActiveId = id;
                    mUi->FocusId  = id;
                }
                if (mUi->ActiveId == id) {
                    if (mInput.bMouseReleased) {
                        if (hovered) {
                            pressed = true;
                        }
                        mUi->ActiveId = 0ULL;
                    }
                }

                FColor32 bg = hovered ? mTheme->ButtonHoveredBg : mTheme->ButtonBg;
                if (mUi->ActiveId == id) {
                    bg = mTheme->ButtonActiveBg;
                }
                DrawRectFilled(r, bg);
                DrawRect(r, mTheme->ButtonBorder, 1.0f);

                const FVector2f textPos(
                    r.Min.X() + mTheme->ButtonPaddingX, r.Min.Y() + mTheme->ButtonPaddingY);
                DrawText(textPos, mTheme->ButtonText, label);

                AdvanceItem(size);
                return pressed;
            }

            [[nodiscard]] bool Checkbox(FStringView label, bool& value) override {
                const u64       id  = HashId(label);
                const f32       box = mTheme->CheckboxBoxSize;
                const FRect     boxRect{ mCursor, FVector2f(mCursor.X() + box, mCursor.Y() + box) };
                const FVector2f textPos(mCursor.X() + box + mTheme->CheckboxTextOffsetX,
                    mCursor.Y() + mTheme->ButtonPaddingY);
                const f32       w = box + mTheme->CheckboxTextOffsetX + CalcTextWidth(label);
                const f32       h = box;
                const FRect     fullRect{ mCursor, FVector2f(mCursor.X() + w, mCursor.Y() + h) };

                const bool      hovered = PointInRect(mInput.MousePos, fullRect);
                if (hovered) {
                    mUi->HotId              = id;
                    mUi->bWantsCaptureMouse = true;
                }

                bool changed = false;
                if (hovered && mInput.bMousePressed) {
                    mUi->ActiveId = id;
                    mUi->FocusId  = id;
                }
                if (mUi->ActiveId == id && mInput.bMouseReleased) {
                    if (hovered) {
                        value   = !value;
                        changed = true;
                    }
                    mUi->ActiveId = 0ULL;
                }

                DrawRectFilled(boxRect, mTheme->CheckboxBoxBg);
                DrawRect(boxRect, mTheme->CheckboxBoxBorder, 1.0f);
                if (value) {
                    const f32   inset = mTheme->CheckboxMarkInset;
                    const FRect mark{ FVector2f(boxRect.Min.X() + inset, boxRect.Min.Y() + inset),
                        FVector2f(boxRect.Max.X() - inset, boxRect.Max.Y() - inset) };
                    DrawRectFilled(mark, mTheme->CheckboxMark);
                }
                DrawText(textPos, mTheme->Text, label);

                AdvanceItem(FVector2f(w, h));
                return changed;
            }

            [[nodiscard]] bool SliderFloat(
                FStringView label, f32& value, f32 minValue, f32 maxValue) override {
                if (maxValue <= minValue) {
                    return false;
                }

                Text(label);
                const u64   id = HashId(label);
                const f32   w  = mContentMax.X() - mContentMin.X();
                const f32   h  = mTheme->SliderHeight;
                const FRect r{ mCursor, FVector2f(mCursor.X() + w, mCursor.Y() + h) };

                const bool  hovered = PointInRect(mInput.MousePos, r);
                if (hovered) {
                    mUi->HotId              = id;
                    mUi->bWantsCaptureMouse = true;
                }

                if (hovered && mInput.bMousePressed) {
                    mUi->ActiveId = id;
                    mUi->FocusId  = id;
                }

                bool changed = false;
                if (mUi->ActiveId == id && mInput.bMouseDown) {
                    const f32 t  = (mInput.MousePos.X() - r.Min.X()) / (r.Max.X() - r.Min.X());
                    const f32 tt = (t < 0.0f) ? 0.0f : ((t > 1.0f) ? 1.0f : t);
                    const f32 newValue = minValue + tt * (maxValue - minValue);
                    if (newValue != value) {
                        value   = newValue;
                        changed = true;
                    }
                }
                if (mUi->ActiveId == id && mInput.bMouseReleased) {
                    mUi->ActiveId = 0ULL;
                }

                DrawRectFilled(r, mTheme->SliderBg);
                DrawRect(r, mTheme->SliderBorder, 1.0f);
                const f32   norm  = (value - minValue) / (maxValue - minValue);
                const f32   fillW = (norm < 0.0f) ? 0.0f : ((norm > 1.0f) ? 1.0f : norm);
                const FRect fill{ r.Min, FVector2f(r.Min.X() + w * fillW, r.Max.Y()) };
                DrawRectFilled(fill, mTheme->SliderFill);

                AdvanceItem(FVector2f(w, h + mTheme->SliderBottomSpacingY));
                return changed;
            }

            [[nodiscard]] bool InputText(FStringView label, Container::FString& value) override {
                Text(label);
                const u64   id = HashId(label);
                const f32   w  = mContentMax.X() - mContentMin.X();
                const f32   h  = mTheme->InputHeight;
                const FRect r{ mCursor, FVector2f(mCursor.X() + w, mCursor.Y() + h) };

                const bool  hovered = PointInRect(mInput.MousePos, r);
                if (hovered) {
                    mUi->HotId              = id;
                    mUi->bWantsCaptureMouse = true;
                }
                if (hovered && mInput.bMousePressed) {
                    mUi->ActiveId = id;
                    mUi->FocusId  = id;
                }

                bool changed = false;
                if (mUi->ActiveId == id) {
                    mUi->FocusId               = id;
                    mUi->bWantsCaptureKeyboard = true;
                    // Text input.
                    if (mInput.Input != nullptr) {
                        const auto& chars = mInput.Input->GetCharInputs();
                        for (const auto code : chars) {
                            if (code == 0U) {
                                continue;
                            }
                            if (code < 32U) {
                                continue;
                            }
                            value.Append(static_cast<TChar>(code));
                            changed = true;
                        }

                        // Fallback: synthesize basic ASCII from key presses when WM_CHAR is not
                        // available.
                        if (chars.IsEmpty()) {
                            const bool shift = mInput.Input->IsKeyDown(Input::EKey::LeftShift)
                                || mInput.Input->IsKeyDown(Input::EKey::RightShift);

                            auto TryAppendAlpha = [&](Input::EKey key, char lower) {
                                if (!mInput.Input->WasKeyPressed(key)) {
                                    return;
                                }
                                const char c =
                                    shift ? static_cast<char>(lower - ('a' - 'A')) : lower;
                                value.Append(static_cast<TChar>(c));
                                changed = true;
                            };

                            TryAppendAlpha(Input::EKey::A, 'a');
                            TryAppendAlpha(Input::EKey::B, 'b');
                            TryAppendAlpha(Input::EKey::C, 'c');
                            TryAppendAlpha(Input::EKey::D, 'd');
                            TryAppendAlpha(Input::EKey::E, 'e');
                            TryAppendAlpha(Input::EKey::F, 'f');
                            TryAppendAlpha(Input::EKey::G, 'g');
                            TryAppendAlpha(Input::EKey::H, 'h');
                            TryAppendAlpha(Input::EKey::I, 'i');
                            TryAppendAlpha(Input::EKey::J, 'j');
                            TryAppendAlpha(Input::EKey::K, 'k');
                            TryAppendAlpha(Input::EKey::L, 'l');
                            TryAppendAlpha(Input::EKey::M, 'm');
                            TryAppendAlpha(Input::EKey::N, 'n');
                            TryAppendAlpha(Input::EKey::O, 'o');
                            TryAppendAlpha(Input::EKey::P, 'p');
                            TryAppendAlpha(Input::EKey::Q, 'q');
                            TryAppendAlpha(Input::EKey::R, 'r');
                            TryAppendAlpha(Input::EKey::S, 's');
                            TryAppendAlpha(Input::EKey::T, 't');
                            TryAppendAlpha(Input::EKey::U, 'u');
                            TryAppendAlpha(Input::EKey::V, 'v');
                            TryAppendAlpha(Input::EKey::W, 'w');
                            TryAppendAlpha(Input::EKey::X, 'x');
                            TryAppendAlpha(Input::EKey::Y, 'y');
                            TryAppendAlpha(Input::EKey::Z, 'z');

                            auto TryAppendDigit = [&](Input::EKey key, char digit) {
                                if (!mInput.Input->WasKeyPressed(key)) {
                                    return;
                                }
                                value.Append(static_cast<TChar>(digit));
                                changed = true;
                            };

                            TryAppendDigit(Input::EKey::Num0, '0');
                            TryAppendDigit(Input::EKey::Num1, '1');
                            TryAppendDigit(Input::EKey::Num2, '2');
                            TryAppendDigit(Input::EKey::Num3, '3');
                            TryAppendDigit(Input::EKey::Num4, '4');
                            TryAppendDigit(Input::EKey::Num5, '5');
                            TryAppendDigit(Input::EKey::Num6, '6');
                            TryAppendDigit(Input::EKey::Num7, '7');
                            TryAppendDigit(Input::EKey::Num8, '8');
                            TryAppendDigit(Input::EKey::Num9, '9');

                            if (mInput.Input->WasKeyPressed(Input::EKey::Space)) {
                                value.Append(static_cast<TChar>(' '));
                                changed = true;
                            }
                        }
                    }

                    if (mInput.bKeyBackspacePressed && !value.IsEmptyString()) {
                        value.PopBack();
                        changed = true;
                    }

                    if (mInput.bMousePressed && !hovered) {
                        // Click outside deactivates.
                        mUi->ActiveId = 0ULL;
                    }
                }

                const bool active = (mUi->ActiveId == id);
                DrawRectFilled(r, active ? mTheme->InputActiveBg : mTheme->InputBg);
                DrawRect(r, active ? mTheme->InputActiveBorder : mTheme->InputBorder, 1.0f);
                DrawText(FVector2f(r.Min.X() + mTheme->InputTextOffsetX,
                             r.Min.Y() + mTheme->InputTextOffsetY),
                    mTheme->InputText, value.ToView());

                AdvanceItem(FVector2f(w, h));
                return changed;
            }

        private:
            void AdvanceLine() {
                mCursor = FVector2f(mContentMin.X(),
                    mCursor.Y() + static_cast<f32>(FFontAtlas::kDrawGlyphH) + mTheme->ItemSpacingY);
            }

            void AdvanceItem(const FVector2f& itemSize) {
                mCursor =
                    FVector2f(mContentMin.X(), mCursor.Y() + itemSize.Y() + mTheme->ItemSpacingY);
            }

            [[nodiscard]] auto CalcTextWidth(FStringView s) const noexcept -> f32 {
                return static_cast<f32>(s.Length()) * static_cast<f32>(FFontAtlas::kDrawGlyphW);
            }

            [[nodiscard]] auto CalcButtonSize(FStringView label) const noexcept -> FVector2f {
                const f32 w = CalcTextWidth(label) + mTheme->ButtonPaddingX * 2.0f;
                const f32 h =
                    static_cast<f32>(FFontAtlas::kDrawGlyphH) + mTheme->ButtonPaddingY * 2.0f;
                return FVector2f(w, h);
            }

            [[nodiscard]] auto HashId(FStringView label) const noexcept -> u64 {
                // FNV-1a over TChar units, mixed with current window title.
                constexpr u64 kOffset = 1469598103934665603ULL;
                constexpr u64 kPrime  = 1099511628211ULL;
                u64           h       = kOffset;
                auto          MixView = [&](FStringView v) {
                    for (usize i = 0; i < v.Length(); ++i) {
                        h ^= static_cast<u64>(static_cast<u32>(v[i]));
                        h *= kPrime;
                    }
                };
                MixView(mCurrentWindowTitle.ToView());
                MixView(label);
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

            void BeginCmdIfNeeded() {
                if (mDrawData->Cmds.IsEmpty()) {
                    FDrawCmd cmd{};
                    cmd.IndexOffset = 0U;
                    cmd.IndexCount  = 0U;
                    cmd.ClipRect    = mClip->Current(mDisplaySize);
                    mDrawData->Cmds.PushBack(cmd);
                    return;
                }

                const FRect cur  = mClip->Current(mDisplaySize);
                const FRect last = mDrawData->Cmds.Back().ClipRect;
                const bool  same = (cur.Min.X() == last.Min.X()) && (cur.Min.Y() == last.Min.Y())
                    && (cur.Max.X() == last.Max.X()) && (cur.Max.Y() == last.Max.Y());
                if (!same) {
                    FDrawCmd cmd{};
                    cmd.IndexOffset = static_cast<u32>(mDrawData->Indices.Size());
                    cmd.IndexCount  = 0U;
                    cmd.ClipRect    = cur;
                    mDrawData->Cmds.PushBack(cmd);
                }
            }

            void AddQuad(const FVector2f& p0, const FVector2f& p1, const FVector2f& p2,
                const FVector2f& p3, f32 u0, f32 v0, f32 u1, f32 v1, FColor32 color) {
                BeginCmdIfNeeded();
                const u32 base = static_cast<u32>(mDrawData->Vertices.Size());
                mDrawData->Vertices.PushBack({ p0.X(), p0.Y(), u0, v0, color });
                mDrawData->Vertices.PushBack({ p1.X(), p1.Y(), u1, v0, color });
                mDrawData->Vertices.PushBack({ p2.X(), p2.Y(), u1, v1, color });
                mDrawData->Vertices.PushBack({ p3.X(), p3.Y(), u0, v1, color });

                mDrawData->Indices.PushBack(base + 0U);
                mDrawData->Indices.PushBack(base + 1U);
                mDrawData->Indices.PushBack(base + 2U);
                mDrawData->Indices.PushBack(base + 0U);
                mDrawData->Indices.PushBack(base + 2U);
                mDrawData->Indices.PushBack(base + 3U);

                mDrawData->Cmds.Back().IndexCount += 6U;
            }

            void AddTriangleFilled(
                const FVector2f& p0, const FVector2f& p1, const FVector2f& p2, FColor32 color) {
                BeginCmdIfNeeded();
                const u32 base = static_cast<u32>(mDrawData->Vertices.Size());
                const f32 u    = (mSolidU0 + mSolidU1) * 0.5f;
                const f32 v    = (mSolidV0 + mSolidV1) * 0.5f;
                mDrawData->Vertices.PushBack({ p0.X(), p0.Y(), u, v, color });
                mDrawData->Vertices.PushBack({ p1.X(), p1.Y(), u, v, color });
                mDrawData->Vertices.PushBack({ p2.X(), p2.Y(), u, v, color });

                mDrawData->Indices.PushBack(base + 0U);
                mDrawData->Indices.PushBack(base + 1U);
                mDrawData->Indices.PushBack(base + 2U);

                mDrawData->Cmds.Back().IndexCount += 3U;
            }

            void AddRectFilled(const FRect& rect, FColor32 color) {
                AddQuad(rect.Min, FVector2f(rect.Max.X(), rect.Min.Y()), rect.Max,
                    FVector2f(rect.Min.X(), rect.Max.Y()), mSolidU0, mSolidV0, mSolidU1, mSolidV1,
                    color);
            }

            void AddRect(const FRect& rect, FColor32 color, f32 thickness) {
                const f32 t = (thickness > 0.0f) ? thickness : 1.0f;
                AddRectFilled({ rect.Min, FVector2f(rect.Max.X(), rect.Min.Y() + t) }, color);
                AddRectFilled({ FVector2f(rect.Min.X(), rect.Max.Y() - t), rect.Max }, color);
                AddRectFilled({ FVector2f(rect.Min.X(), rect.Min.Y() + t),
                                  FVector2f(rect.Min.X() + t, rect.Max.Y() - t) },
                    color);
                AddRectFilled({ FVector2f(rect.Max.X() - t, rect.Min.Y() + t),
                                  FVector2f(rect.Max.X(), rect.Max.Y() - t) },
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

            void AddArcFilled(const FVector2f& center, f32 radius, f32 startAngleRad,
                f32 endAngleRad, u32 segments, FColor32 color) {
                if (radius <= 0.0f || segments == 0U) {
                    return;
                }

                const f32 step = (endAngleRad - startAngleRad) / static_cast<f32>(segments);
                f32       a0   = startAngleRad;
                FVector2f p0(
                    center.X() + std::cos(a0) * radius, center.Y() + std::sin(a0) * radius);
                for (u32 i = 0U; i < segments; ++i) {
                    const f32       a1 = startAngleRad + step * static_cast<f32>(i + 1U);
                    const FVector2f p1(
                        center.X() + std::cos(a1) * radius, center.Y() + std::sin(a1) * radius);
                    AddTriangleFilled(center, p0, p1, color);
                    p0 = p1;
                }
            }

            void AddArcStroke(const FVector2f& center, f32 radius, f32 startAngleRad,
                f32 endAngleRad, u32 segments, FColor32 color, f32 thickness) {
                if (radius <= 0.0f || segments == 0U) {
                    return;
                }

                const f32 step = (endAngleRad - startAngleRad) / static_cast<f32>(segments);
                f32       a0   = startAngleRad;
                FVector2f p0(
                    center.X() + std::cos(a0) * radius, center.Y() + std::sin(a0) * radius);
                for (u32 i = 0U; i < segments; ++i) {
                    const f32       a1 = startAngleRad + step * static_cast<f32>(i + 1U);
                    const FVector2f p1(
                        center.X() + std::cos(a1) * radius, center.Y() + std::sin(a1) * radius);
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
                    { FVector2f(minX + r, minY + r), FVector2f(maxX - r, maxY - r) }, color);
                AddRectFilled({ FVector2f(minX + r, minY), FVector2f(maxX - r, minY + r) }, color);
                AddRectFilled({ FVector2f(minX + r, maxY - r), FVector2f(maxX - r, maxY) }, color);
                AddRectFilled({ FVector2f(minX, minY + r), FVector2f(minX + r, maxY - r) }, color);
                AddRectFilled({ FVector2f(maxX - r, minY + r), FVector2f(maxX, maxY - r) }, color);

                constexpr f32 kPi   = 3.14159265358979323846f;
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

                constexpr f32 kPi   = 3.14159265358979323846f;
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

            void AddCapsuleFilled(
                const FVector2f& a, const FVector2f& b, f32 radius, FColor32 color) {
                const f32 r = ClampNonNegative(radius);
                if (r <= 0.0f) {
                    return;
                }

                const FVector2f d    = b - a;
                const f32       len2 = d.X() * d.X() + d.Y() * d.Y();
                if (len2 <= 0.0001f) {
                    // Degenerates to a circle.
                    constexpr f32 kPi = 3.14159265358979323846f;
                    const u32     seg = CalcArcSegments90(r) * 4U;
                    AddArcFilled(a, r, 0.0f, 2.0f * kPi, seg, color);
                    return;
                }

                const f32       invLen = 1.0f / std::sqrt(len2);
                const FVector2f u(d.X() * invLen, d.Y() * invLen);
                const FVector2f p(-u.Y(), u.X());
                const FVector2f off(p.X() * r, p.Y() * r);

                // Middle quad (non-overlapping with end caps).
                AddQuad(a + off, b + off, b - off, a - off, mSolidU0, mSolidV0, mSolidU1, mSolidV1,
                    color);

                constexpr f32 kPi    = 3.14159265358979323846f;
                const f32     ang    = std::atan2(u.Y(), u.X());
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
                    constexpr f32 kPi = 3.14159265358979323846f;
                    const u32     seg = CalcArcSegments90(r) * 4U;
                    AddArcStroke(a, r, 0.0f, 2.0f * kPi, seg, color, thickness);
                    return;
                }

                const f32       invLen = 1.0f / std::sqrt(len2);
                const FVector2f u(d.X() * invLen, d.Y() * invLen);
                const FVector2f p(-u.Y(), u.X());
                const FVector2f off(p.X() * r, p.Y() * r);

                // Side edges.
                AddLine(a + off, b + off, color, thickness);
                AddLine(b - off, a - off, color, thickness);

                constexpr f32 kPi    = 3.14159265358979323846f;
                const f32     ang    = std::atan2(u.Y(), u.X());
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
                const f32       invLen = 1.0f / std::sqrt(len2);
                const FVector2f n(-d.Y() * invLen, d.X() * invLen);
                const f32       s = t * 0.5f;
                const FVector2f off(n.X() * s, n.Y() * s);
                AddQuad(p0 + off, p1 + off, p1 - off, p0 - off, mSolidU0, mSolidV0, mSolidU1,
                    mSolidV1, color);
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
                    cursor = FVector2f(
                        cursor.X() + static_cast<f32>(FFontAtlas::kDrawGlyphW), cursor.Y());
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

        [[nodiscard]] auto BuildLayoutHash(
            const TVector<Rhi::FRhiBindGroupLayoutEntry>& entries, u32 setIndex) -> u64 {
            constexpr u64 kOffset = 1469598103934665603ULL;
            constexpr u64 kPrime  = 1099511628211ULL;
            u64           hash    = kOffset;
            auto          mix     = [&](u64 value) { hash = (hash ^ value) * kPrime; };

            mix(setIndex);
            for (const auto& entry : entries) {
                mix(entry.mBinding);
                mix(static_cast<u64>(entry.mType));
                mix(static_cast<u64>(entry.mVisibility));
                mix(entry.mArrayCount);
                mix(entry.mHasDynamicOffset ? 1ULL : 0ULL);
            }
            return hash;
        }

        void UploadDynamicBuffer(Rhi::FRhiBuffer* buffer, const void* data, u64 sizeBytes) {
            if (buffer == nullptr || data == nullptr || sizeBytes == 0ULL) {
                return;
            }
            auto lock = buffer->Lock(0ULL, sizeBytes, Rhi::ERhiBufferLockMode::WriteDiscard);
            if (!lock.IsValid()) {
                return;
            }
            Core::Platform::Generic::Memcpy(lock.mData, data, static_cast<usize>(sizeBytes));
            buffer->Unlock(lock);
        }

        [[nodiscard]] auto BuildIncludeDir(const FPath& shaderPath) -> FPath {
            auto includeDir = shaderPath.ParentPath().ParentPath().ParentPath();
            if (!includeDir.IsEmpty()) {
                return includeDir;
            }
            return shaderPath.ParentPath();
        }

        [[nodiscard]] auto FindDebugGuiShaderPath() -> FPath {
            constexpr FStringView kAssetsRelPath = TEXT("Assets/Shader/Debug/DebugGui.hlsl");
            constexpr FStringView kLegacyRelPath = TEXT("Shader/Debug/DebugGui.hlsl");
            constexpr FStringView kSourceRelPath = TEXT("Source/Shader/Debug/DebugGui.hlsl");

            const FPath           exeDir(Core::Platform::GetExecutableDir());
            if (!exeDir.IsEmpty()) {
                const auto pAssets = exeDir / kAssetsRelPath;
                if (pAssets.Exists()) {
                    return pAssets;
                }
                const auto pLegacy = exeDir / kLegacyRelPath;
                if (pLegacy.Exists()) {
                    return pLegacy;
                }
                const auto exeParent = exeDir.ParentPath();
                if (!exeParent.IsEmpty() && exeParent != exeDir) {
                    const auto ppAssets = exeParent / kAssetsRelPath;
                    if (ppAssets.Exists()) {
                        return ppAssets;
                    }
                    const auto ppLegacy = exeParent / kLegacyRelPath;
                    if (ppLegacy.Exists()) {
                        return ppLegacy;
                    }
                }
            }

            const auto cwd = Core::Utility::Filesystem::GetCurrentWorkingDir();
            if (!cwd.IsEmpty()) {
                const auto pSource = cwd / kSourceRelPath;
                if (pSource.Exists()) {
                    return pSource;
                }
                const auto pAssets = cwd / kAssetsRelPath;
                if (pAssets.Exists()) {
                    return pAssets;
                }
                const auto pLegacy = cwd / kLegacyRelPath;
                if (pLegacy.Exists()) {
                    return pLegacy;
                }
            }

            FPath probe = cwd;
            for (u32 i = 0U; i < 6U && !probe.IsEmpty(); ++i) {
                const auto pSource = probe / kSourceRelPath;
                if (pSource.Exists()) {
                    return pSource;
                }
                const auto parent = probe.ParentPath();
                if (parent == probe) {
                    break;
                }
                probe = parent;
            }

            return {};
        }

        class FDebugGuiRendererD3D11 {
        public:
            void Render(Rhi::FRhiDevice& device, Rhi::FRhiViewport& viewport,
                const FDrawData& drawData, const FFontAtlas& atlas) {
                if (drawData.Cmds.IsEmpty() || drawData.Vertices.IsEmpty()
                    || drawData.Indices.IsEmpty()) {
                    return;
                }

                if (!EnsureResources(device, atlas)) {
                    return;
                }

                auto* backBuffer = viewport.GetBackBuffer();
                if (backBuffer == nullptr) {
                    return;
                }

                if (!EnsureBackBufferRtv(device, backBuffer)) {
                    return;
                }

                if (!EnsureGeometryBuffers(device, drawData)) {
                    return;
                }

                Rhi::FRhiCommandContextDesc ctxDesc{};
                ctxDesc.mQueueType = Rhi::ERhiQueueType::Graphics;
                ctxDesc.mDebugName.Assign(TEXT("DebugGui.Overlay"));
                auto commandContext = device.CreateCommandContext(ctxDesc);
                if (!commandContext) {
                    return;
                }

                auto* ops = dynamic_cast<Rhi::IRhiCmdContextOps*>(commandContext.Get());
                if (ops == nullptr) {
                    return;
                }

                Rhi::FRhiCmdContextAdapter ctx(*commandContext.Get(), *ops);

                const auto&                bbDesc = backBuffer->GetDesc();
                const u32                  w      = bbDesc.mWidth;
                const u32                  h      = bbDesc.mHeight;

                UpdateConstants(w, h);

                ctx.RHISetGraphicsPipeline(mPipeline.Get());
                ctx.RHISetPrimitiveTopology(Rhi::ERhiPrimitiveTopology::TriangleList);

                Rhi::FRhiViewportRect vp{};
                vp.mX      = 0.0f;
                vp.mY      = 0.0f;
                vp.mWidth  = static_cast<f32>(w);
                vp.mHeight = static_cast<f32>(h);
                ctx.RHISetViewport(vp);

                Rhi::FRhiTransitionInfo toRenderTarget{};
                toRenderTarget.mResource = backBuffer;
                toRenderTarget.mBefore   = Rhi::ERhiResourceState::Present;
                toRenderTarget.mAfter    = Rhi::ERhiResourceState::RenderTarget;
                Rhi::FRhiTransitionCreateInfo toRenderTargetBatch{};
                toRenderTargetBatch.mTransitions     = &toRenderTarget;
                toRenderTargetBatch.mTransitionCount = 1U;
                toRenderTargetBatch.mSrcQueue        = Rhi::ERhiQueueType::Graphics;
                toRenderTargetBatch.mDstQueue        = Rhi::ERhiQueueType::Graphics;
                ctx.RHIBeginTransition(toRenderTargetBatch);

                Rhi::FRhiRenderPassColorAttachment colorAtt[3]{};
                colorAtt[0].mView        = mBackBufferRtv.Get();
                colorAtt[0].mLoadOp      = Rhi::ERhiLoadOp::Load;
                colorAtt[0].mStoreOp     = Rhi::ERhiStoreOp::Store;
                u32 colorAttachmentCount = 1U;
                if (Rhi::RHIGetBackend() == Rhi::ERhiBackend::Vulkan) {
                    if (!EnsureAuxiliaryRtvs(device, w, h, bbDesc.mFormat)) {
                        return;
                    }
                    colorAtt[1].mView    = mAuxColorRtv1.Get();
                    colorAtt[1].mLoadOp  = Rhi::ERhiLoadOp::DontCare;
                    colorAtt[1].mStoreOp = Rhi::ERhiStoreOp::DontCare;
                    colorAtt[2].mView    = mAuxColorRtv2.Get();
                    colorAtt[2].mLoadOp  = Rhi::ERhiLoadOp::DontCare;
                    colorAtt[2].mStoreOp = Rhi::ERhiStoreOp::DontCare;
                    colorAttachmentCount = 3U;
                }
                Rhi::FRhiRenderPassDesc rp{};
                rp.mDebugName.Assign(TEXT("DebugGui.RenderPass"));
                rp.mColorAttachmentCount = colorAttachmentCount;
                rp.mColorAttachments     = colorAtt;
                ctx.RHIBeginRenderPass(rp);

                Rhi::FRhiVertexBufferView vb{};
                vb.mBuffer      = mVertexBuffer.Get();
                vb.mStrideBytes = sizeof(FDrawVertex);
                vb.mOffsetBytes = 0U;
                ctx.RHISetVertexBuffer(0U, vb);

                Rhi::FRhiIndexBufferView ib{};
                ib.mBuffer      = mIndexBuffer.Get();
                ib.mIndexType   = Rhi::ERhiIndexType::Uint32;
                ib.mOffsetBytes = 0U;
                ctx.RHISetIndexBuffer(ib);

                ctx.RHISetBindGroup(0U, mBindGroup.Get(), nullptr, 0U);

                u32 idxOffset = 0U;
                for (const auto& cmd : drawData.Cmds) {
                    const i32 sx = static_cast<i32>(cmd.ClipRect.Min.X());
                    const i32 sy = static_cast<i32>(cmd.ClipRect.Min.Y());
                    const i32 ex = static_cast<i32>(cmd.ClipRect.Max.X());
                    const i32 ey = static_cast<i32>(cmd.ClipRect.Max.Y());
                    const u32 sw = (ex > sx) ? static_cast<u32>(ex - sx) : 0U;
                    const u32 sh = (ey > sy) ? static_cast<u32>(ey - sy) : 0U;
                    if (sw == 0U || sh == 0U) {
                        idxOffset += cmd.IndexCount;
                        continue;
                    }

                    Rhi::FRhiScissorRect sc{};
                    sc.mX      = sx;
                    sc.mY      = sy;
                    sc.mWidth  = sw;
                    sc.mHeight = sh;
                    ctx.RHISetScissor(sc);

                    ctx.RHIDrawIndexed(cmd.IndexCount, 1U, idxOffset, 0, 0U);
                    idxOffset += cmd.IndexCount;
                }

                ctx.RHIEndRenderPass();

                Rhi::FRhiTransitionInfo toPresent{};
                toPresent.mResource = backBuffer;
                toPresent.mBefore   = Rhi::ERhiResourceState::RenderTarget;
                toPresent.mAfter    = Rhi::ERhiResourceState::Present;
                Rhi::FRhiTransitionCreateInfo toPresentBatch{};
                toPresentBatch.mTransitions     = &toPresent;
                toPresentBatch.mTransitionCount = 1U;
                toPresentBatch.mSrcQueue        = Rhi::ERhiQueueType::Graphics;
                toPresentBatch.mDstQueue        = Rhi::ERhiQueueType::Graphics;
                ctx.RHIBeginTransition(toPresentBatch);

                commandContext->RHIFlushContextDevice({});
            }

        private:
            struct FConstants {
                f32 ScaleX     = 1.0f;
                f32 ScaleY     = 1.0f;
                f32 TranslateX = -1.0f;
                f32 TranslateY = -1.0f;
            };

            bool EnsureResources(Rhi::FRhiDevice& device, const FFontAtlas& atlas);
            bool EnsureBackBufferRtv(Rhi::FRhiDevice& device, Rhi::FRhiTexture* backBuffer);
            bool EnsureAuxiliaryRtvs(
                Rhi::FRhiDevice& device, u32 width, u32 height, Rhi::ERhiFormat format);
            bool EnsureGeometryBuffers(Rhi::FRhiDevice& device, const FDrawData& drawData);
            void UpdateConstants(u32 w, u32 h);
            bool CompileShaders(Rhi::FRhiDevice& device);

            Rhi::FRhiTextureRef            mFontTexture;
            Rhi::FRhiShaderResourceViewRef mFontSrv;
            Rhi::FRhiSamplerRef            mSampler;
            Rhi::FRhiBufferRef             mConstantsBuffer;
            Rhi::FRhiBindGroupLayoutRef    mLayout;
            Rhi::FRhiPipelineLayoutRef     mPipelineLayout;
            Rhi::FRhiBindGroupRef          mBindGroup;
            Rhi::FRhiShaderRef             mVertexShader;
            Rhi::FRhiShaderRef             mPixelShader;
            Rhi::FRhiPipelineRef           mPipeline;

            Rhi::FRhiTexture*              mBackBufferTex = nullptr;
            Rhi::FRhiRenderTargetViewRef   mBackBufferRtv;
            Rhi::FRhiTextureRef            mAuxColorTex1;
            Rhi::FRhiTextureRef            mAuxColorTex2;
            Rhi::FRhiRenderTargetViewRef   mAuxColorRtv1;
            Rhi::FRhiRenderTargetViewRef   mAuxColorRtv2;
            u32                            mAuxWidth  = 0U;
            u32                            mAuxHeight = 0U;
            Rhi::ERhiFormat                mAuxFormat = Rhi::ERhiFormat::Unknown;

            Rhi::FRhiBufferRef             mVertexBuffer;
            Rhi::FRhiBufferRef             mIndexBuffer;
            u64                            mVertexBufferSize = 0ULL;
            u64                            mIndexBufferSize  = 0ULL;
        };

        bool FDebugGuiRendererD3D11::EnsureResources(
            Rhi::FRhiDevice& device, const FFontAtlas& atlas) {
            if (!mFontTexture) {
                Rhi::FRhiTextureDesc desc{};
                desc.mDebugName.Assign(TEXT("DebugGui.FontAtlas"));
                desc.mWidth       = FFontAtlas::kAtlasW;
                desc.mHeight      = FFontAtlas::kAtlasH;
                desc.mDepth       = 1U;
                desc.mMipLevels   = 1U;
                desc.mArrayLayers = 1U;
                desc.mFormat      = Rhi::ERhiFormat::R8G8B8A8Unorm;
                desc.mBindFlags   = Rhi::ERhiTextureBindFlags::ShaderResource;
                mFontTexture      = device.CreateTexture(desc);
                if (!mFontTexture) {
                    LogError(TEXT("DebugGui: Failed to create font texture."));
                    return false;
                }

                Rhi::FRhiTextureSubresource sub{};
                const u32                   rowPitch   = FFontAtlas::kAtlasW * 4U;
                const u32                   slicePitch = rowPitch * FFontAtlas::kAtlasH;
                device.UpdateTextureSubresource(
                    mFontTexture.Get(), sub, atlas.Pixels.Data(), rowPitch, slicePitch);

                Rhi::FRhiShaderResourceViewDesc srv{};
                srv.mDebugName.Assign(TEXT("DebugGui.FontAtlas.SRV"));
                srv.mTexture = mFontTexture.Get();
                srv.mFormat  = desc.mFormat;
                mFontSrv     = device.CreateShaderResourceView(srv);
                if (!mFontSrv) {
                    LogError(TEXT("DebugGui: Failed to create font SRV."));
                    return false;
                }
            }

            if (!mSampler) {
                Rhi::FRhiSamplerDesc s{};
                s.mDebugName.Assign(TEXT("DebugGui.Sampler"));
                // Debug text is frequently rendered at small sizes; use point sampling to avoid
                // blur.
                s.mFilter = Rhi::ERhiSamplerFilter::Nearest;
                mSampler  = device.CreateSampler(s);
                if (!mSampler) {
                    LogError(TEXT("DebugGui: Failed to create sampler."));
                    return false;
                }
            }

            if (!mLayout) {
                Rhi::FRhiBindGroupLayoutDesc layout{};
                layout.mDebugName.Assign(TEXT("DebugGui.Layout"));
                layout.mSetIndex = 0U;

                // b0
                {
                    Rhi::FRhiBindGroupLayoutEntry e{};
                    e.mBinding    = 0U;
                    e.mType       = Rhi::ERhiBindingType::ConstantBuffer;
                    e.mVisibility = Rhi::ERhiShaderStageFlags::Vertex;
                    layout.mEntries.PushBack(e);
                }
                // t0
                {
                    Rhi::FRhiBindGroupLayoutEntry e{};
                    e.mBinding    = MapSampledTextureBinding(0U);
                    e.mType       = Rhi::ERhiBindingType::SampledTexture;
                    e.mVisibility = Rhi::ERhiShaderStageFlags::Pixel;
                    layout.mEntries.PushBack(e);
                }
                // s0
                {
                    Rhi::FRhiBindGroupLayoutEntry e{};
                    e.mBinding    = MapSamplerBinding(0U);
                    e.mType       = Rhi::ERhiBindingType::Sampler;
                    e.mVisibility = Rhi::ERhiShaderStageFlags::Pixel;
                    layout.mEntries.PushBack(e);
                }

                layout.mLayoutHash = BuildLayoutHash(layout.mEntries, layout.mSetIndex);
                mLayout            = device.CreateBindGroupLayout(layout);
                if (!mLayout) {
                    LogError(TEXT("DebugGui: Failed to create bind group layout."));
                    return false;
                }
            }

            if (!mPipelineLayout) {
                Rhi::FRhiPipelineLayoutDesc p{};
                p.mDebugName.Assign(TEXT("DebugGui.PipelineLayout"));
                p.mBindGroupLayouts.PushBack(mLayout.Get());
                mPipelineLayout = device.CreatePipelineLayout(p);
                if (!mPipelineLayout) {
                    LogError(TEXT("DebugGui: Failed to create pipeline layout."));
                    return false;
                }
            }

            if (!mConstantsBuffer) {
                Rhi::FRhiBufferDesc cb{};
                cb.mDebugName.Assign(TEXT("DebugGui.Constants"));
                cb.mSizeBytes    = sizeof(FConstants);
                cb.mUsage        = Rhi::ERhiResourceUsage::Dynamic;
                cb.mBindFlags    = Rhi::ERhiBufferBindFlags::Constant;
                cb.mCpuAccess    = Rhi::ERhiCpuAccess::Write;
                mConstantsBuffer = device.CreateBuffer(cb);
                if (!mConstantsBuffer) {
                    LogError(TEXT("DebugGui: Failed to create constants buffer."));
                    return false;
                }
            }

            if (!mBindGroup) {
                Rhi::FRhiBindGroupDesc bg{};
                bg.mDebugName.Assign(TEXT("DebugGui.BindGroup"));
                bg.mLayout = mLayout.Get();
                {
                    Rhi::FRhiBindGroupEntry e{};
                    e.mBinding = 0U;
                    e.mType    = Rhi::ERhiBindingType::ConstantBuffer;
                    e.mBuffer  = mConstantsBuffer.Get();
                    e.mOffset  = 0ULL;
                    e.mSize    = sizeof(FConstants);
                    bg.mEntries.PushBack(e);
                }
                {
                    Rhi::FRhiBindGroupEntry e{};
                    e.mBinding = MapSampledTextureBinding(0U);
                    e.mType    = Rhi::ERhiBindingType::SampledTexture;
                    e.mTexture = mFontTexture.Get();
                    bg.mEntries.PushBack(e);
                }
                {
                    Rhi::FRhiBindGroupEntry e{};
                    e.mBinding = MapSamplerBinding(0U);
                    e.mType    = Rhi::ERhiBindingType::Sampler;
                    e.mSampler = mSampler.Get();
                    bg.mEntries.PushBack(e);
                }
                mBindGroup = device.CreateBindGroup(bg);
                if (!mBindGroup) {
                    LogError(TEXT("DebugGui: Failed to create bind group."));
                    return false;
                }
            }

            if (!mVertexShader || !mPixelShader) {
                if (!CompileShaders(device)) {
                    return false;
                }
            }

            if (!mPipeline) {
                Rhi::FRhiGraphicsPipelineDesc gp{};
                gp.mDebugName.Assign(TEXT("DebugGui.PSO"));
                gp.mPipelineLayout = mPipelineLayout.Get();
                gp.mVertexShader   = mVertexShader.Get();
                gp.mPixelShader    = mPixelShader.Get();

                {
                    Rhi::FRhiVertexAttributeDesc a{};
                    a.mSemanticName.Assign(TEXT("POSITION"));
                    a.mFormat            = Rhi::ERhiFormat::R32G32Float;
                    a.mInputSlot         = 0U;
                    a.mAlignedByteOffset = 0U;
                    gp.mVertexLayout.mAttributes.PushBack(a);
                }
                {
                    Rhi::FRhiVertexAttributeDesc a{};
                    a.mSemanticName.Assign(TEXT("TEXCOORD"));
                    a.mFormat            = Rhi::ERhiFormat::R32G32Float;
                    a.mInputSlot         = 0U;
                    a.mAlignedByteOffset = 8U;
                    gp.mVertexLayout.mAttributes.PushBack(a);
                }
                {
                    Rhi::FRhiVertexAttributeDesc a{};
                    a.mSemanticName.Assign(TEXT("COLOR"));
                    a.mFormat            = Rhi::ERhiFormat::R8G8B8A8Unorm;
                    a.mInputSlot         = 0U;
                    a.mAlignedByteOffset = 16U;
                    gp.mVertexLayout.mAttributes.PushBack(a);
                }

                gp.mRasterState.mCullMode   = Rhi::ERhiRasterCullMode::None;
                gp.mDepthState.mDepthEnable = false;
                gp.mDepthState.mDepthWrite  = false;
                gp.mBlendState.mBlendEnable = true;
                gp.mBlendState.mSrcColor    = Rhi::ERhiBlendFactor::SrcAlpha;
                gp.mBlendState.mDstColor    = Rhi::ERhiBlendFactor::InvSrcAlpha;
                gp.mBlendState.mColorOp     = Rhi::ERhiBlendOp::Add;
                gp.mBlendState.mSrcAlpha    = Rhi::ERhiBlendFactor::One;
                gp.mBlendState.mDstAlpha    = Rhi::ERhiBlendFactor::InvSrcAlpha;
                gp.mBlendState.mAlphaOp     = Rhi::ERhiBlendOp::Add;

                mPipeline = device.CreateGraphicsPipeline(gp);
                if (!mPipeline) {
                    LogError(TEXT("DebugGui: Failed to create graphics pipeline."));
                    return false;
                }
            }

            return true;
        }

        bool FDebugGuiRendererD3D11::EnsureBackBufferRtv(
            Rhi::FRhiDevice& device, Rhi::FRhiTexture* backBuffer) {
            if (mBackBufferTex == backBuffer && mBackBufferRtv) {
                return true;
            }
            mBackBufferTex = backBuffer;
            Rhi::FRhiRenderTargetViewDesc rtv{};
            rtv.mDebugName.Assign(TEXT("DebugGui.BackBuffer.RTV"));
            rtv.mTexture   = backBuffer;
            rtv.mFormat    = backBuffer->GetDesc().mFormat;
            mBackBufferRtv = device.CreateRenderTargetView(rtv);
            return static_cast<bool>(mBackBufferRtv);
        }

        bool FDebugGuiRendererD3D11::EnsureAuxiliaryRtvs(
            Rhi::FRhiDevice& device, u32 width, u32 height, Rhi::ERhiFormat format) {
            if (width == 0U || height == 0U || format == Rhi::ERhiFormat::Unknown) {
                return false;
            }
            const bool recreate =
                (mAuxWidth != width) || (mAuxHeight != height) || (mAuxFormat != format);
            if (recreate) {
                mAuxColorRtv1.Reset();
                mAuxColorRtv2.Reset();
                mAuxColorTex1.Reset();
                mAuxColorTex2.Reset();
                mAuxWidth  = width;
                mAuxHeight = height;
                mAuxFormat = format;
            }
            if (mAuxColorRtv1 && mAuxColorRtv2) {
                return true;
            }

            Rhi::FRhiTextureDesc texDesc{};
            texDesc.mDebugName.Assign(TEXT("DebugGui.AuxColor"));
            texDesc.mDimension   = Rhi::ERhiTextureDimension::Tex2D;
            texDesc.mWidth       = width;
            texDesc.mHeight      = height;
            texDesc.mDepth       = 1U;
            texDesc.mMipLevels   = 1U;
            texDesc.mArrayLayers = 1U;
            texDesc.mFormat      = format;
            texDesc.mBindFlags   = Rhi::ERhiTextureBindFlags::RenderTarget;
            mAuxColorTex1        = device.CreateTexture(texDesc);
            mAuxColorTex2        = device.CreateTexture(texDesc);
            if (!mAuxColorTex1 || !mAuxColorTex2) {
                return false;
            }

            Rhi::FRhiRenderTargetViewDesc rtvDesc{};
            rtvDesc.mDebugName.Assign(TEXT("DebugGui.AuxColor.RTV"));
            rtvDesc.mFormat = format;

            rtvDesc.mTexture = mAuxColorTex1.Get();
            mAuxColorRtv1    = device.CreateRenderTargetView(rtvDesc);
            rtvDesc.mTexture = mAuxColorTex2.Get();
            mAuxColorRtv2    = device.CreateRenderTargetView(rtvDesc);
            return static_cast<bool>(mAuxColorRtv1) && static_cast<bool>(mAuxColorRtv2);
        }

        bool FDebugGuiRendererD3D11::EnsureGeometryBuffers(
            Rhi::FRhiDevice& device, const FDrawData& drawData) {
            const u64 vbBytes = static_cast<u64>(drawData.Vertices.Size()) * sizeof(FDrawVertex);
            const u64 ibBytes = static_cast<u64>(drawData.Indices.Size()) * sizeof(u32);
            if (vbBytes == 0ULL || ibBytes == 0ULL) {
                return false;
            }

            if (!mVertexBuffer || mVertexBufferSize < vbBytes) {
                Rhi::FRhiBufferDesc d{};
                d.mDebugName.Assign(TEXT("DebugGui.VB"));
                d.mSizeBytes      = vbBytes;
                d.mUsage          = Rhi::ERhiResourceUsage::Dynamic;
                d.mBindFlags      = Rhi::ERhiBufferBindFlags::Vertex;
                d.mCpuAccess      = Rhi::ERhiCpuAccess::Write;
                mVertexBuffer     = device.CreateBuffer(d);
                mVertexBufferSize = vbBytes;
            }
            if (!mIndexBuffer || mIndexBufferSize < ibBytes) {
                Rhi::FRhiBufferDesc d{};
                d.mDebugName.Assign(TEXT("DebugGui.IB"));
                d.mSizeBytes     = ibBytes;
                d.mUsage         = Rhi::ERhiResourceUsage::Dynamic;
                d.mBindFlags     = Rhi::ERhiBufferBindFlags::Index;
                d.mCpuAccess     = Rhi::ERhiCpuAccess::Write;
                mIndexBuffer     = device.CreateBuffer(d);
                mIndexBufferSize = ibBytes;
            }
            if (!mVertexBuffer || !mIndexBuffer) {
                LogError(TEXT("DebugGui: Failed to allocate geometry buffers."));
                return false;
            }

            UploadDynamicBuffer(mVertexBuffer.Get(), drawData.Vertices.Data(), vbBytes);
            UploadDynamicBuffer(mIndexBuffer.Get(), drawData.Indices.Data(), ibBytes);
            return true;
        }

        void FDebugGuiRendererD3D11::UpdateConstants(u32 w, u32 h) {
            if (!mConstantsBuffer || w == 0U || h == 0U) {
                return;
            }
            FConstants c{};
            c.ScaleX     = 2.0f / static_cast<f32>(w);
            c.ScaleY     = 2.0f / static_cast<f32>(h);
            c.TranslateX = -1.0f;
            c.TranslateY = -1.0f;
            UploadDynamicBuffer(mConstantsBuffer.Get(), &c, sizeof(FConstants));
        }

        bool FDebugGuiRendererD3D11::CompileShaders(Rhi::FRhiDevice& device) {
            using ShaderCompiler::EShaderOptimization;
            using ShaderCompiler::EShaderSourceLanguage;
            using ShaderCompiler::FShaderCompileRequest;
            using ShaderCompiler::FShaderCompileResult;
            using ShaderCompiler::GetShaderCompiler;

            const auto shaderPath = FindDebugGuiShaderPath();
            if (shaderPath.IsEmpty() || !shaderPath.Exists()) {
                LogError(TEXT("DebugGui shader source not found: '{}'"),
                    shaderPath.GetString().ToView());
                return false;
            }

            auto CompileStage = [&](FStringView entry, Shader::EShaderStage stage,
                                    Rhi::FRhiShaderRef& out) -> bool {
                FShaderCompileRequest request{};
                request.mSource.mPath.Assign(shaderPath.GetString().ToView());
                request.mSource.mEntryPoint.Assign(entry);
                request.mSource.mStage    = stage;
                request.mSource.mLanguage = EShaderSourceLanguage::Hlsl;

                const auto includeDir = BuildIncludeDir(shaderPath);
                if (!includeDir.IsEmpty()) {
                    request.mSource.mIncludeDirs.PushBack(includeDir.GetString());
                }

                const auto backend = Rhi::RHIGetBackend();
                request.mOptions.mTargetBackend =
                    (backend != Rhi::ERhiBackend::Unknown) ? backend : Rhi::ERhiBackend::DirectX11;
                request.mOptions.mOptimization = EShaderOptimization::Default;
                request.mOptions.mDebugInfo    = false;

                FShaderCompileResult result = GetShaderCompiler().Compile(request);
                if (!result.mSucceeded) {
                    LogError(TEXT("DebugGui shader compile failed: entry='{}' stage={} diag={}"),
                        entry, static_cast<u32>(stage), result.mDiagnostics.ToView());
                    return false;
                }

                auto shaderDesc = ShaderCompiler::BuildRhiShaderDesc(result);
                shaderDesc.mDebugName.Assign(entry);
                out = device.CreateShader(shaderDesc);
                if (!out) {
                    LogError(TEXT("DebugGui: Failed to create RHI shader: '{}'"), entry);
                    return false;
                }
                return true;
            };

            if (!CompileStage(TEXT("DebugGuiVSMain"), Shader::EShaderStage::Vertex, mVertexShader)
                || !CompileStage(
                    TEXT("DebugGuiPSMain"), Shader::EShaderStage::Pixel, mPixelShader)) {
                return false;
            }
            return true;
        }

        class FDebugGuiSystem final : public IDebugGuiSystem {
        public:
            FDebugGuiSystem() { mFont.Build(); }
            ~FDebugGuiSystem() override { DetachLogSinkIfOwned(); }

            void SetEnabled(bool enabled) noexcept override {
                FScopedLock lock(mMutex);
                mEnabledGameThread = enabled;
            }

            [[nodiscard]] auto IsEnabled() const noexcept -> bool override {
                FScopedLock lock(mMutex);
                return mEnabledGameThread;
            }

            void SetShowStats(bool show) noexcept override {
                FScopedLock lock(mMutex);
                mShowStats = show;
            }

            void SetShowConsole(bool show) noexcept override {
                FScopedLock lock(mMutex);
                mShowConsole = show;
            }

            void SetShowCVars(bool show) noexcept override {
                FScopedLock lock(mMutex);
                mShowCVars = show;
            }

            [[nodiscard]] auto IsStatsShown() const noexcept -> bool override {
                FScopedLock lock(mMutex);
                return mShowStats;
            }

            [[nodiscard]] auto IsConsoleShown() const noexcept -> bool override {
                FScopedLock lock(mMutex);
                return mShowConsole;
            }

            [[nodiscard]] auto IsCVarsShown() const noexcept -> bool override {
                FScopedLock lock(mMutex);
                return mShowCVars;
            }

            void RegisterPanel(FStringView name, FPanelFn fn) override {
                if (name.IsEmpty() || !fn) {
                    return;
                }
                FScopedLock lock(mMutex);
                FPanelEntry e{};
                e.Name.Assign(name);
                e.Fn = Move(fn);
                mPanels.PushBack(Move(e));
            }

            void RegisterOverlay(FStringView name, FPanelFn fn) override {
                if (name.IsEmpty() || !fn) {
                    return;
                }
                FScopedLock lock(mMutex);
                FPanelEntry e{};
                e.Name.Assign(name);
                e.Fn = Move(fn);
                mOverlays.PushBack(Move(e));
            }

            void SetExternalStats(const FDebugGuiExternalStats& stats) noexcept override {
                FScopedLock lock(mMutex);
                mExternalStats = stats;
            }

            void SetTheme(const FDebugGuiTheme& theme) noexcept override {
                FScopedLock lock(mMutex);
                mTheme = theme;
            }

            [[nodiscard]] auto GetTheme() const noexcept -> FDebugGuiTheme override {
                FScopedLock lock(mMutex);
                return mTheme;
            }

            void TickGameThread(const Input::FInputSystem& input, f32 dtSeconds, u32 displayWidth,
                u32 displayHeight) override {
                const FVector2f displaySize(
                    static_cast<f32>(displayWidth), static_cast<f32>(displayHeight));

                // Toggle via F1.
                const bool             toggle = input.WasKeyPressed(Input::EKey::F1);

                bool                   enabled = false;
                FDebugGuiExternalStats ext{};
                FDebugGuiTheme         theme{};
                {
                    FScopedLock lock(mMutex);
                    if (toggle) {
                        mEnabledGameThread = !mEnabledGameThread;
                    }
                    enabled = mEnabledGameThread;
                    ext     = mExternalStats;
                    theme   = mTheme;
                }

                mUi.ClearTransient();
                mPending.Clear();
                mClip.Clear();
                mWindowOrder.Clear();

                if (!enabled || displayWidth == 0U || displayHeight == 0U) {
                    PublishPendingDrawData(enabled);
                    return;
                }

                EnsureLogSink();

                FGuiInput guiInput{};
                guiInput.Input    = &input;
                guiInput.MousePos = FVector2f(
                    static_cast<f32>(input.GetMouseX()), static_cast<f32>(input.GetMouseY()));
                guiInput.bMouseDown           = input.IsMouseButtonDown(0U);
                guiInput.bMousePressed        = input.WasMouseButtonPressed(0U);
                guiInput.bMouseReleased       = input.WasMouseButtonReleased(0U);
                guiInput.MouseWheelDelta      = input.GetMouseWheelDelta();
                guiInput.bKeyEnterPressed     = input.WasKeyPressed(Input::EKey::Enter);
                guiInput.bKeyBackspacePressed = input.WasKeyPressed(Input::EKey::Backspace);

                FDebugGuiContext ctx(mPending, mClip, guiInput, mUi, displaySize, mFont, theme,
                    mWindowOrder, mWindows, mDraggingWindowKey, mWindowDragOffset);
                ctx.PushClipRect({ FVector2f(0.0f, 0.0f), displaySize });

                // Built-in panels.
                if (mShowStats) {
                    DrawStatsWindow(ctx, dtSeconds, ext);
                }
                if (mShowConsole) {
                    DrawConsoleWindow(ctx, guiInput);
                }
                if (mShowCVars) {
                    DrawCVarsWindow(ctx, guiInput);
                }

                // Custom panels (can use widgets).
                {
                    FScopedLock lock(mMutex);
                    for (auto& p : mPanels) {
                        if (p.Fn) {
                            if (ctx.BeginWindow(p.Name.ToView(), nullptr)) {
                                p.Fn(ctx);
                                ctx.EndWindow();
                            }
                        }
                    }
                }

                // Overlays (no window chrome).
                {
                    FScopedLock lock(mMutex);
                    for (auto& o : mOverlays) {
                        if (o.Fn) {
                            o.Fn(ctx);
                        }
                    }
                }

                if (mUi.ActiveId != 0ULL) {
                    mUi.bWantsCaptureMouse = true;
                }

                ctx.PopClipRect();
                PublishPendingDrawData(enabled);
            }

            void RenderRenderThread(Rhi::FRhiDevice& device, Rhi::FRhiViewport& viewport) override {
                FDrawData drawData{};
                bool      enabled = false;
                {
                    FScopedLock lock(mMutex);
                    drawData = Move(mRender);
                    enabled  = mEnabledRenderThread;
                }
                if (!enabled) {
                    return;
                }
                mRenderer.Render(device, viewport, drawData, mFont);
            }

            [[nodiscard]] auto WantsCaptureKeyboard() const noexcept -> bool override {
                return mUi.bWantsCaptureKeyboard;
            }
            [[nodiscard]] auto WantsCaptureMouse() const noexcept -> bool override {
                return mUi.bWantsCaptureMouse;
            }

            [[nodiscard]] auto GetLastFrameStats() const noexcept -> FDebugGuiFrameStats override {
                FScopedLock lock(mMutex);
                return mLastStats;
            }

        private:
            struct FPanelEntry {
                FString  Name;
                FPanelFn Fn;
            };

            struct FLogLine {
                ELogLevel Level = ELogLevel::Info;
                FString   Text;
            };

            static void DebugGuiLogSink(
                ELogLevel level, FStringView category, FStringView message, void* userData) {
                auto* self = static_cast<FDebugGuiSystem*>(userData);
                if (!self) {
                    return;
                }
                if (self->mForwardLogToDefault) {
                    Core::Logging::FLogger::LogToDefaultSink(level, category, message);
                }
                self->AppendLogLine(level, category, message);
            }

            void EnsureLogSink() {
                if (mHasAttachedSink) {
                    return;
                }
                // Don't override another custom sink; DebugGui console will simply not capture.
                if (Core::Logging::FLogger::HasCustomLogSink()) {
                    return;
                }
                Core::Logging::FLogger::SetLogSink(&DebugGuiLogSink, this);
                mHasAttachedSink = true;
            }

            void DetachLogSinkIfOwned() {
                if (!mHasAttachedSink) {
                    return;
                }
                Core::Logging::FLogger::ResetLogSink();
                mHasAttachedSink = false;
            }

            void AppendLogLine(ELogLevel level, FStringView category, FStringView message) {
                FString line;
                line.Assign(TEXT("["));
                line.Append(category);
                line.Append(TEXT("] "));
                line.Append(message);

                FScopedLock lock(mLogMutex);
                if (mLogLines.Size() != kMaxLogLines) {
                    // Keep ring storage stable after first log line.
                    mLogLines.Resize(kMaxLogLines);
                    mLogStart = 0U;
                    mLogCount = 0U;
                }

                u32 writeIndex = 0U;
                if (mLogCount < kMaxLogLines) {
                    writeIndex = (mLogStart + mLogCount) % kMaxLogLines;
                    ++mLogCount;
                } else {
                    // Overwrite oldest.
                    writeIndex = mLogStart;
                    mLogStart  = (mLogStart + 1U) % kMaxLogLines;
                }

                mLogLines[writeIndex].Level = level;
                mLogLines[writeIndex].Text  = Move(line);
            }

            void DrawStatsWindow(IDebugGui& gui, f32 dtSeconds, const FDebugGuiExternalStats& ext) {
                if (!gui.BeginWindow(TEXT("DebugGui Stats"), nullptr)) {
                    return;
                }

                const f32 fps = (dtSeconds > 0.0f) ? (1.0f / dtSeconds) : 0.0f;
                FString   line;
                line.Assign(TEXT("Frame: "));
                line.AppendNumber(ext.FrameIndex);
                gui.Text(line.ToView());

                line.Clear();
                line.Assign(TEXT("dt(ms): "));
                line.AppendNumber(static_cast<int>(dtSeconds * 1000.0f));
                line.Append(TEXT("  fps: "));
                line.AppendNumber(static_cast<int>(fps));
                gui.Text(line.ToView());

                line.Clear();
                line.Assign(TEXT("Views: "));
                line.AppendNumber(ext.ViewCount);
                line.Append(TEXT("  SceneBatches: "));
                line.AppendNumber(ext.SceneBatchCount);
                gui.Text(line.ToView());

                const auto draw = GetLastFrameStats();
                line.Clear();
                line.Assign(TEXT("Draw: vtx="));
                line.AppendNumber(draw.VertexCount);
                line.Append(TEXT(" idx="));
                line.AppendNumber(draw.IndexCount);
                line.Append(TEXT(" cmd="));
                line.AppendNumber(draw.CmdCount);
                gui.Text(line.ToView());

                gui.EndWindow();
            }

            void DrawConsoleWindow(FDebugGuiContext& gui, const FGuiInput& input) {
                if (!gui.BeginWindow(TEXT("DebugGui Console"), nullptr)) {
                    return;
                }

                (void)gui.InputText(TEXT("Filter"), mConsoleFilter);
                gui.Separator();

                if (gui.IsMouseHoveringRect(gui.GetContentRect()) && input.MouseWheelDelta != 0.0f
                    && mUi.ActiveId == 0ULL) {
                    const i32 step = 3;
                    if (input.MouseWheelDelta > 0.0f) {
                        mConsoleScrollOffset += step;
                    } else {
                        mConsoleScrollOffset -= step;
                    }
                }

                mConsoleFilteredOrderIndices.Clear();
                {
                    FScopedLock lock(mLogMutex);
                    const u32   count = mLogCount;
                    mConsoleFilteredOrderIndices.Reserve(static_cast<usize>(count));
                    for (u32 orderIdx = 0U; orderIdx < count; ++orderIdx) {
                        const u32   ringIdx = (mLogStart + orderIdx) % kMaxLogLines;
                        const auto& line    = mLogLines[ringIdx].Text;
                        if (mConsoleFilter.IsEmptyString()) {
                            mConsoleFilteredOrderIndices.PushBack(orderIdx);
                            continue;
                        }
                        if (line.Find(mConsoleFilter.ToView(), 0) != FString::npos) {
                            mConsoleFilteredOrderIndices.PushBack(orderIdx);
                        }
                    }
                }

                const u32 matchCount = static_cast<u32>(mConsoleFilteredOrderIndices.Size());
                const u32 showCount  = (matchCount > 18U) ? 18U : matchCount;
                const i32 maxOffset =
                    (matchCount > showCount) ? static_cast<i32>(matchCount - showCount) : 0;
                if (mConsoleScrollOffset < 0) {
                    mConsoleScrollOffset = 0;
                }
                if (mConsoleScrollOffset > maxOffset) {
                    mConsoleScrollOffset = maxOffset;
                }

                const u32 startIndex = (matchCount > showCount)
                    ? static_cast<u32>(
                          static_cast<i32>(matchCount - showCount) - mConsoleScrollOffset)
                    : 0U;

                {
                    FScopedLock lock(mLogMutex);
                    for (u32 i = 0U; i < showCount; ++i) {
                        const u32 listIdx = startIndex + i;
                        if (listIdx >= matchCount) {
                            break;
                        }
                        const u32 orderIdx = mConsoleFilteredOrderIndices[listIdx];
                        const u32 ringIdx  = (mLogStart + orderIdx) % kMaxLogLines;
                        gui.Text(mLogLines[ringIdx].Text.ToView());
                    }
                }

                gui.Separator();
                (void)gui.InputText(TEXT("Command"), mConsoleInput);

                const u64 commandId = gui.DebugHashId(TEXT("Command"));
                if (input.bKeyEnterPressed && mUi.ActiveId == commandId
                    && !mConsoleInput.IsEmptyString()) {
                    mConsoleScrollOffset = 0;
                    ExecuteConsoleCommand(mConsoleInput);
                    mConsoleInput.Clear();
                    mUi.ActiveId = 0ULL;
                }

                gui.EndWindow();
            }

            void ExecuteConsoleCommand(const FString& command) {
                auto view = command.ToView();
                if (view.IsEmpty()) {
                    return;
                }

                // token0
                const auto spacePos = view.Find(TEXT(' '), 0);
                auto       cmd      = (spacePos == FString::npos) ? view : view.Substr(0, spacePos);
                if (cmd == FStringView(TEXT("help"))) {
                    AppendLogLine(ELogLevel::Info, TEXT("DebugGui"),
                        TEXT("Commands: help, set <cvar> <value>"));
                    return;
                }

                if (cmd == FStringView(TEXT("set"))) {
                    if (spacePos == FString::npos) {
                        AppendLogLine(ELogLevel::Warning, TEXT("DebugGui"),
                            TEXT("Usage: set <cvar> <value>"));
                        return;
                    }
                    auto       rest   = view.Substr(spacePos + 1, FString::npos);
                    const auto space2 = rest.Find(TEXT(' '), 0);
                    if (space2 == FString::npos) {
                        AppendLogLine(ELogLevel::Warning, TEXT("DebugGui"),
                            TEXT("Usage: set <cvar> <value>"));
                        return;
                    }
                    const auto nameView  = rest.Substr(0, space2);
                    const auto valueView = rest.Substr(space2 + 1, FString::npos);

                    FString    name(nameView);
                    auto*      cvar = Core::Console::FConsoleVariable::Find(name);
                    if (cvar == nullptr) {
                        AppendLogLine(
                            ELogLevel::Warning, TEXT("DebugGui"), TEXT("CVar not found."));
                        return;
                    }
                    FString value(valueView);
                    cvar->SetFromString(value);
                    AppendLogLine(ELogLevel::Info, TEXT("DebugGui"), TEXT("CVar updated."));
                    return;
                }

                AppendLogLine(
                    ELogLevel::Warning, TEXT("DebugGui"), TEXT("Unknown command. Type 'help'."));
            }

            void DrawCVarsWindow(FDebugGuiContext& gui, const FGuiInput& input) {
                if (!gui.BeginWindow(TEXT("DebugGui CVars"), nullptr)) {
                    return;
                }
                const auto& th = gui.GetTheme();

                // Editor (keep it at the top so it won't scroll away).
                if (!mSelectedCVarName.IsEmptyString()) {
                    FString title(TEXT("Selected: "));
                    title.Append(mSelectedCVarName);
                    gui.Text(title.ToView());
                    (void)gui.InputText(TEXT("Value"), mSelectedCVarValue);
                    if (gui.Button(TEXT("Apply"))) {
                        if (auto* cvar = Core::Console::FConsoleVariable::Find(mSelectedCVarName)) {
                            cvar->SetFromString(mSelectedCVarValue);
                        }
                    }
                    gui.Separator();
                }

                gui.Text(TEXT("Click a CVar to edit:"));
                (void)gui.InputText(TEXT("Filter"), mCVarsFilter);
                gui.Separator();

                TVector<FString> names;
                Core::Console::FConsoleVariable::ForEach(
                    [&](const Core::Console::FConsoleVariable& v) { names.PushBack(v.GetName()); });

                // Filtered index list.
                TVector<usize> filtered;
                filtered.Reserve(names.Size());
                for (usize i = 0; i < names.Size(); ++i) {
                    if (mCVarsFilter.IsEmptyString()
                        || names[i].Find(mCVarsFilter.ToView(), 0) != FString::npos) {
                        filtered.PushBack(i);
                    }
                }

                const FRect     content = gui.GetContentRect();
                const FVector2f listMin = gui.GetCursorPos();
                const FVector2f listMax = content.Max;
                const FRect     listRect{ listMin, listMax };
                const bool      listHovered = gui.IsMouseHoveringRect(listRect);

                // Mouse wheel scroll (only when no widget is active).
                if (listHovered && input.MouseWheelDelta != 0.0f && mUi.ActiveId == 0ULL) {
                    const i32 step = 3;
                    if (input.MouseWheelDelta > 0.0f) {
                        mCVarsScrollIndex -= step;
                    } else {
                        mCVarsScrollIndex += step;
                    }
                }

                const f32 itemHf       = th.InputHeight;
                const f32 listHf       = listRect.Max.Y() - listRect.Min.Y();
                const u32 visibleCount = (listHf > itemHf) ? static_cast<u32>(listHf / itemHf) : 1U;
                const u32 totalCount   = static_cast<u32>(filtered.Size());
                const i32 maxStart =
                    (totalCount > visibleCount) ? static_cast<i32>(totalCount - visibleCount) : 0;
                if (mCVarsScrollIndex < 0) {
                    mCVarsScrollIndex = 0;
                }
                if (mCVarsScrollIndex > maxStart) {
                    mCVarsScrollIndex = maxStart;
                }

                // Scroll bar (optional).
                const f32  kScrollBarW    = th.ScrollBarWidth;
                const f32  kScrollBarPad  = th.ScrollBarPadding;
                const bool needsScrollBar = (totalCount > visibleCount);

                FVector2f  textMax = listRect.Max;
                if (needsScrollBar
                    && (listRect.Max.X() - listRect.Min.X())
                        > (kScrollBarW + kScrollBarPad + 16.0f)) {
                    textMax =
                        FVector2f(listRect.Max.X() - kScrollBarW - kScrollBarPad, listRect.Max.Y());
                }

                const FRect listTextRect{ listRect.Min, textMax };
                const FRect scrollTrackRect{
                    FVector2f(textMax.X() + kScrollBarPad, listRect.Min.Y()), listRect.Max
                };

                if (needsScrollBar) {
                    const f32 trackH    = scrollTrackRect.Max.Y() - scrollTrackRect.Min.Y();
                    const f32 thumbMinH = th.ScrollBarThumbMinHeight;
                    f32       thumbH =
                        trackH * (static_cast<f32>(visibleCount) / static_cast<f32>(totalCount));
                    if (thumbH < thumbMinH) {
                        thumbH = thumbMinH;
                    }
                    if (thumbH > trackH) {
                        thumbH = trackH;
                    }

                    const f32 maxStartF = static_cast<f32>(maxStart > 0 ? maxStart : 1);
                    const f32 t =
                        (maxStart > 0) ? (static_cast<f32>(mCVarsScrollIndex) / maxStartF) : 0.0f;
                    const f32   thumbY = scrollTrackRect.Min.Y() + (trackH - thumbH) * t;
                    const FRect thumbRect{ FVector2f(scrollTrackRect.Min.X(), thumbY),
                        FVector2f(scrollTrackRect.Max.X(), thumbY + thumbH) };

                    const u64   sbId         = gui.DebugHashId(TEXT("##CVarsScrollBar"));
                    const bool  trackHovered = gui.IsMouseHoveringRect(scrollTrackRect);
                    const bool  thumbHovered = gui.IsMouseHoveringRect(thumbRect);
                    const bool  sbActive     = (mUi.ActiveId == sbId);

                    if ((trackHovered || thumbHovered) && input.bMousePressed) {
                        mUi.ActiveId           = sbId;
                        mUi.FocusId            = sbId;
                        mUi.bWantsCaptureMouse = true;
                        if (thumbHovered) {
                            mCVarsScrollDragOffsetY = input.MousePos.Y() - thumbRect.Min.Y();
                        } else {
                            mCVarsScrollDragOffsetY = thumbH * 0.5f;
                        }
                    }

                    if (sbActive && input.bMouseDown) {
                        mUi.bWantsCaptureMouse  = true;
                        const f32 desiredThumbY = input.MousePos.Y() - mCVarsScrollDragOffsetY;
                        const f32 minY          = scrollTrackRect.Min.Y();
                        const f32 maxY          = scrollTrackRect.Max.Y() - thumbH;
                        const f32 clampedY      = (desiredThumbY < minY)
                                 ? minY
                                 : ((desiredThumbY > maxY) ? maxY : desiredThumbY);
                        const f32 denom         = (trackH - thumbH);
                        const f32 tt = (denom > 0.0f) ? ((clampedY - minY) / denom) : 0.0f;
                        const i32 newStart =
                            static_cast<i32>(tt * static_cast<f32>(maxStart) + 0.5f);
                        mCVarsScrollIndex =
                            (newStart < 0) ? 0 : ((newStart > maxStart) ? maxStart : newStart);
                    }
                    if (sbActive && input.bMouseReleased) {
                        mUi.ActiveId = 0ULL;
                    }

                    // Draw track + thumb.
                    gui.DrawRectFilled(scrollTrackRect, th.ScrollBarTrackBg);
                    gui.DrawRect(scrollTrackRect, th.ScrollBarTrackBorder, 1.0f);
                    const FColor32 thumbCol = sbActive
                        ? th.ScrollBarThumbActiveBg
                        : (thumbHovered ? th.ScrollBarThumbHoverBg : th.ScrollBarThumbBg);
                    gui.DrawRectFilled(thumbRect, thumbCol);
                    gui.DrawRect(thumbRect, th.ScrollBarThumbBorder, 1.0f);
                }

                // List drawing + hit-test.
                gui.PushClipRect(listTextRect);
                const u32 start = static_cast<u32>(mCVarsScrollIndex);
                const u32 end =
                    (start + visibleCount < totalCount) ? (start + visibleCount) : totalCount;
                for (u32 order = start; order < end; ++order) {
                    const usize nameIndex = filtered[order];
                    const auto& name      = names[nameIndex];

                    const f32 y0 = listTextRect.Min.Y() + static_cast<f32>(order - start) * itemHf;
                    const FRect r{ FVector2f(listRect.Min.X(), y0),
                        FVector2f(listTextRect.Max.X(), y0 + itemHf) };

                    const u64   id      = gui.DebugHashId(name.ToView());
                    const bool  hovered = gui.IsMouseHoveringRect(r);
                    if (hovered) {
                        mUi.HotId              = id;
                        mUi.bWantsCaptureMouse = true;
                    }
                    if (hovered && input.bMousePressed) {
                        mUi.ActiveId = id;
                        mUi.FocusId  = id;
                    }

                    bool clicked = false;
                    if (mUi.ActiveId == id && input.bMouseReleased) {
                        if (hovered) {
                            clicked = true;
                        }
                        mUi.ActiveId = 0ULL;
                    }

                    const bool     selected = (!mSelectedCVarName.IsEmptyString()
                        && mSelectedCVarName.ToView() == name.ToView());
                    const FColor32 bg =
                        selected ? th.SelectedRowBg : (hovered ? th.HoveredRowBg : 0U);
                    if ((bg >> 24U) != 0U) {
                        gui.DrawRectFilled(r, bg);
                    }
                    gui.DrawText(
                        FVector2f(r.Min.X() + th.InputTextOffsetX, r.Min.Y() + th.InputTextOffsetY),
                        th.Text, name.ToView());

                    if (clicked) {
                        mSelectedCVarName = name;
                        if (auto* cvar = Core::Console::FConsoleVariable::Find(mSelectedCVarName)) {
                            mSelectedCVarValue = cvar->GetString();
                        } else {
                            mSelectedCVarValue.Clear();
                        }
                    }
                }
                gui.PopClipRect();

                // Consume remaining space (avoid stacking).
                gui.SetCursorPos(listRect.Max);

                gui.EndWindow();
            }

            void PublishPendingDrawData(bool enabled) {
                FScopedLock lock(mMutex);
                mLastStats.VertexCount = static_cast<u32>(mPending.Vertices.Size());
                mLastStats.IndexCount  = static_cast<u32>(mPending.Indices.Size());
                mLastStats.CmdCount    = static_cast<u32>(mPending.Cmds.Size());
                mRender                = Move(mPending);
                mEnabledRenderThread   = enabled;
            }

            mutable FMutex                         mMutex;
            bool                                   mEnabledGameThread   = true;
            bool                                   mEnabledRenderThread = true;
            TVector<FPanelEntry>                   mPanels;
            TVector<FPanelEntry>                   mOverlays;
            FDebugGuiExternalStats                 mExternalStats{};
            FDebugGuiTheme                         mTheme{};

            FUIState                               mUi{};
            FFontAtlas                             mFont{};
            FClipRectStack                         mClip{};
            FDrawData                              mPending{};
            FDrawData                              mRender{};
            FDebugGuiRendererD3D11                 mRenderer{};
            FDebugGuiFrameStats                    mLastStats{};

            // Window order used for deterministic stacking.
            TVector<FString>                       mWindowOrder;
            Container::THashMap<u64, FWindowState> mWindows;
            u64                                    mDraggingWindowKey = 0ULL;
            FVector2f                              mWindowDragOffset  = FVector2f(0.0f, 0.0f);

            // Console/log capture.
            static constexpr u32                   kMaxLogLines = 2000U;
            mutable FMutex                         mLogMutex;
            TVector<FLogLine>                      mLogLines;
            u32          mLogStart            = 0U; // Oldest entry index in ring buffer.
            u32          mLogCount            = 0U;
            bool         mHasAttachedSink     = false;
            bool         mForwardLogToDefault = true;
            FString      mConsoleInput;
            FString      mConsoleFilter;
            i32          mConsoleScrollOffset = 0; // 0: bottom/newest
            TVector<u32> mConsoleFilteredOrderIndices;

            // CVars panel state.
            FString      mSelectedCVarName;
            FString      mSelectedCVarValue;
            FString      mCVarsFilter;
            i32          mCVarsScrollIndex       = 0;
            f32          mCVarsScrollDragOffsetY = 0.0f;

            // Panel toggles.
            bool         mShowStats   = true;
            bool         mShowConsole = false;
            bool         mShowCVars   = true;
        };
    } // namespace

    auto CreateDebugGuiSystemOwner() -> FDebugGuiSystemOwner {
        return MakeUniqueAs<IDebugGuiSystem, FDebugGuiSystem>();
    }

    auto CreateDebugGuiSystem() -> IDebugGuiSystem* {
        auto owner = MakeUniqueAs<IDebugGuiSystem, FDebugGuiSystem>();
        return owner.Release();
    }

    void DestroyDebugGuiSystem(IDebugGuiSystem* sys) {
        DestroyPolymorphic<IDebugGuiSystem, FDebugGuiSystem>(sys);
    }
} // namespace AltinaEngine::DebugGui
