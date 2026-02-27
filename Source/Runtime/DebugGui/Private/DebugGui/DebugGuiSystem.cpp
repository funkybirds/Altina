#include "DebugGui/DebugGui.h"

#include "CoreMinimal.h"

#include "Container/String.h"
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
        using Container::FString;
        using Container::FStringView;
        using Container::TVector;
        using Core::Logging::ELogLevel;
        using Core::Logging::LogError;
        using Core::Math::FVector2f;
        using Core::Threading::FMutex;
        using Core::Threading::FScopedLock;
        using Core::Utility::Filesystem::FPath;

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

#include "FontAtlas8x8.inl"

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

        struct FFontAtlas {
            static constexpr u32 kGlyphW     = 8U;
            static constexpr u32 kGlyphH     = 8U;
            static constexpr u32 kFirstChar  = 32U;
            static constexpr u32 kLastChar   = 126U;
            static constexpr u32 kGlyphCount = (kLastChar - kFirstChar + 1U);
            static constexpr u32 kCols       = 16U;
            static constexpr u32 kRows       = (kGlyphCount + kCols - 1U) / kCols;
            static constexpr u32 kAtlasW     = kCols * kGlyphW;
            static constexpr u32 kAtlasH     = kRows * kGlyphH;

            TVector<u8>          Pixels; // RGBA8

            void                 Build() {
                Pixels.Resize(static_cast<usize>(kAtlasW) * static_cast<usize>(kAtlasH) * 4U);
                for (usize i = 0; i < Pixels.Size(); ++i) {
                    Pixels[i] = 0U;
                }
                // Reserve a solid white texel at (0,0) with alpha=1 for non-text primitives.
                if (!Pixels.IsEmpty()) {
                    Pixels[0] = 255U;
                    Pixels[1] = 255U;
                    Pixels[2] = 255U;
                    Pixels[3] = 255U;
                }

                for (u32 ch = kFirstChar; ch <= kLastChar; ++ch) {
                    const u32 glyphIndex = ch - kFirstChar;
                    const u32 col        = glyphIndex % kCols;
                    const u32 row        = glyphIndex / kCols;
                    const u32 baseX      = col * kGlyphW;
                    const u32 baseY      = row * kGlyphH;

                    const u8* glyphRows = GetFont8x8Glyph(static_cast<u8>(ch));
                    for (u32 y = 0U; y < kGlyphH; ++y) {
                        const u8 bits = glyphRows ? glyphRows[y] : 0U;
                        for (u32 x = 0U; x < kGlyphW; ++x) {
                            const bool  on = (bits & static_cast<u8>(1U << (7U - x))) != 0U;
                            const u32   px = baseX + x;
                            const u32   py = baseY + y;
                            const usize idx = (static_cast<usize>(py) * kAtlasW + px) * 4U;
                            Pixels[idx + 0U] = 255U;
                            Pixels[idx + 1U] = 255U;
                            Pixels[idx + 2U] = 255U;
                            Pixels[idx + 3U] = on ? 255U : 0U;
                        }
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
                const f32 x0         = static_cast<f32>(col * kGlyphW);
                const f32 y0         = static_cast<f32>(row * kGlyphH);
                const f32 x1         = x0 + static_cast<f32>(kGlyphW);
                const f32 y1         = y0 + static_cast<f32>(kGlyphH);
                outU0                = x0 * invW;
                outV0                = y0 * invH;
                outU1                = x1 * invW;
                outV1                = y1 * invH;
            }
        };

        class FDebugGuiContext final : public IDebugGui {
        public:
            explicit FDebugGuiContext(FDrawData& drawData, FClipRectStack& clip,
                const FGuiInput& input, FUIState& ui, const FVector2f& displaySize,
                const FFontAtlas& atlas, TVector<FString>& windowOrder)
                : mDrawData(&drawData)
                , mClip(&clip)
                , mInput(input)
                , mUi(&ui)
                , mDisplaySize(displaySize)
                , mAtlas(&atlas)
                , mSolidU(0.5f / static_cast<f32>(FFontAtlas::kAtlasW))
                , mSolidV(0.5f / static_cast<f32>(FFontAtlas::kAtlasH))
                , mWindowOrder(&windowOrder) {}

            [[nodiscard]] auto GetWindowRect() const noexcept -> FRect { return mWindowRect; }
            [[nodiscard]] auto GetContentRect() const noexcept -> FRect {
                return { mContentMin, mContentMax };
            }
            [[nodiscard]] auto IsMouseHoveringRect(const FRect& rect) const noexcept -> bool {
                return PointInRect(mInput.MousePos, rect);
            }
            [[nodiscard]] auto DebugHashId(FStringView label) const noexcept -> u64 {
                return HashId(label);
            }

            void PushClipRect(const FRect& rect) override {
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
            void DrawLine(
                const FVector2f& p0, const FVector2f& p1, FColor32 color, f32 thickness) override {
                AddLine(p0, p1, color, thickness);
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

                // Track order for default placement.
                usize windowIndex = 0U;
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
                mWindowPos  = FVector2f(10.0f, 10.0f);
                mWindowSize = FVector2f(460.0f, 260.0f);
                if (mWindowOrder != nullptr) {
                    // Place windows in columns if the display height is too small for pure vertical
                    // stacking.
                    const f32 cellW = mWindowSize.X() + 10.0f;
                    const f32 cellH = mWindowSize.Y() + 10.0f;
                    const f32 usableH =
                        (mDisplaySize.Y() > 10.0f) ? (mDisplaySize.Y() - 10.0f) : 0.0f;
                    const u32 perCol = (usableH > cellH) ? static_cast<u32>(usableH / cellH) : 1U;

                    const u32 col = static_cast<u32>(windowIndex) / perCol;
                    const u32 row = static_cast<u32>(windowIndex) % perCol;
                    mWindowPos    = FVector2f(10.0f + static_cast<f32>(col) * cellW,
                           10.0f + static_cast<f32>(row) * cellH);
                }

                const FRect windowRect{ mWindowPos,
                    FVector2f(mWindowPos.X() + mWindowSize.X(), mWindowPos.Y() + mWindowSize.Y()) };

                const f32   titleBarH = 18.0f;
                const FRect titleRect{ mWindowPos,
                    FVector2f(mWindowPos.X() + mWindowSize.X(), mWindowPos.Y() + titleBarH) };

                mWindowRect = windowRect;

                DrawRectFilled(windowRect, MakeColor32(15, 15, 15, 200));
                DrawRect(windowRect, MakeColor32(255, 255, 255, 140), 1.0f);
                DrawRectFilled(titleRect, MakeColor32(25, 25, 25, 220));
                DrawText(FVector2f(mWindowPos.X() + 8.0f, mWindowPos.Y() + 4.0f),
                    MakeColor32(255, 255, 255, 255), title);

                const f32       pad = 8.0f;
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
                DrawText(mCursor, MakeColor32(220, 220, 220, 255), text);
                AdvanceLine();
            }

            void Separator() override {
                const f32       y = mCursor.Y() + 4.0f;
                const FVector2f a(mContentMin.X(), y);
                const FVector2f b(mContentMax.X(), y);
                DrawLine(a, b, MakeColor32(255, 255, 255, 80), 1.0f);
                mCursor = FVector2f(mCursor.X(), y + 6.0f);
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

                FColor32 bg = hovered ? MakeColor32(80, 80, 80, 220) : MakeColor32(60, 60, 60, 220);
                if (mUi->ActiveId == id) {
                    bg = MakeColor32(100, 100, 100, 240);
                }
                DrawRectFilled(r, bg);
                DrawRect(r, MakeColor32(255, 255, 255, 100), 1.0f);

                const FVector2f textPos(r.Min.X() + 6.0f, r.Min.Y() + 3.0f);
                DrawText(textPos, MakeColor32(255, 255, 255, 255), label);

                AdvanceItem(size);
                return pressed;
            }

            [[nodiscard]] bool Checkbox(FStringView label, bool& value) override {
                const u64       id  = HashId(label);
                const f32       box = 14.0f;
                const FRect     boxRect{ mCursor, FVector2f(mCursor.X() + box, mCursor.Y() + box) };
                const FVector2f textPos(mCursor.X() + box + 8.0f, mCursor.Y() + 3.0f);
                const f32       w = box + 8.0f + CalcTextWidth(label);
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

                DrawRectFilled(boxRect, MakeColor32(30, 30, 30, 255));
                DrawRect(boxRect, MakeColor32(255, 255, 255, 120), 1.0f);
                if (value) {
                    const FRect mark{ FVector2f(boxRect.Min.X() + 3.0f, boxRect.Min.Y() + 3.0f),
                        FVector2f(boxRect.Max.X() - 3.0f, boxRect.Max.Y() - 3.0f) };
                    DrawRectFilled(mark, MakeColor32(140, 200, 140, 255));
                }
                DrawText(textPos, MakeColor32(220, 220, 220, 255), label);

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
                const f32   h  = 16.0f;
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

                DrawRectFilled(r, MakeColor32(40, 40, 40, 255));
                DrawRect(r, MakeColor32(255, 255, 255, 90), 1.0f);
                const f32   norm  = (value - minValue) / (maxValue - minValue);
                const f32   fillW = (norm < 0.0f) ? 0.0f : ((norm > 1.0f) ? 1.0f : norm);
                const FRect fill{ r.Min, FVector2f(r.Min.X() + w * fillW, r.Max.Y()) };
                DrawRectFilled(fill, MakeColor32(120, 160, 220, 255));

                AdvanceItem(FVector2f(w, h + 4.0f));
                return changed;
            }

            [[nodiscard]] bool InputText(FStringView label, Container::FString& value) override {
                Text(label);
                const u64   id = HashId(label);
                const f32   w  = mContentMax.X() - mContentMin.X();
                const f32   h  = 18.0f;
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
                DrawRectFilled(
                    r, active ? MakeColor32(30, 30, 30, 255) : MakeColor32(25, 25, 25, 255));
                DrawRect(r, MakeColor32(255, 255, 255, active ? 160 : 90), 1.0f);
                DrawText(FVector2f(r.Min.X() + 6.0f, r.Min.Y() + 4.0f),
                    MakeColor32(220, 220, 220, 255), value.ToView());

                AdvanceItem(FVector2f(w, h + 6.0f));
                return changed;
            }

        private:
            void AdvanceLine() {
                mCursor = FVector2f(
                    mContentMin.X(), mCursor.Y() + static_cast<f32>(FFontAtlas::kGlyphH) + 4.0f);
            }

            void AdvanceItem(const FVector2f& itemSize) {
                mCursor = FVector2f(mContentMin.X(), mCursor.Y() + itemSize.Y() + 4.0f);
            }

            [[nodiscard]] auto CalcTextWidth(FStringView s) const noexcept -> f32 {
                return static_cast<f32>(s.Length()) * static_cast<f32>(FFontAtlas::kGlyphW);
            }

            [[nodiscard]] auto CalcButtonSize(FStringView label) const noexcept -> FVector2f {
                const f32 w = CalcTextWidth(label) + 12.0f;
                const f32 h = static_cast<f32>(FFontAtlas::kGlyphH) + 6.0f;
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

            void AddRectFilled(const FRect& rect, FColor32 color) {
                AddQuad(rect.Min, FVector2f(rect.Max.X(), rect.Min.Y()), rect.Max,
                    FVector2f(rect.Min.X(), rect.Max.Y()), mSolidU, mSolidV, mSolidU, mSolidV,
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
                AddQuad(p0 + off, p1 + off, p1 - off, p0 - off, mSolidU, mSolidV, mSolidU, mSolidV,
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
                        cursor =
                            FVector2f(pos.X(), cursor.Y() + static_cast<f32>(FFontAtlas::kGlyphH));
                        continue;
                    }
                    if (c == static_cast<TChar>('\r')) {
                        continue;
                    }

                    f32 u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f;
                    mAtlas->GetGlyphUV(static_cast<u32>(c), u0, v0, u1, v1);

                    const FVector2f p0 = cursor;
                    const FVector2f p1(
                        cursor.X() + static_cast<f32>(FFontAtlas::kGlyphW), cursor.Y());
                    const FVector2f p2(cursor.X() + static_cast<f32>(FFontAtlas::kGlyphW),
                        cursor.Y() + static_cast<f32>(FFontAtlas::kGlyphH));
                    const FVector2f p3(
                        cursor.X(), cursor.Y() + static_cast<f32>(FFontAtlas::kGlyphH));
                    AddQuad(p0, p1, p2, p3, u0, v0, u1, v1, color);
                    cursor =
                        FVector2f(cursor.X() + static_cast<f32>(FFontAtlas::kGlyphW), cursor.Y());
                }
            }

            FDrawData*        mDrawData = nullptr;
            FClipRectStack*   mClip     = nullptr;
            FGuiInput         mInput{};
            FUIState*         mUi          = nullptr;
            FVector2f         mDisplaySize = FVector2f(0.0f, 0.0f);
            const FFontAtlas* mAtlas       = nullptr;
            f32               mSolidU      = 0.0f;
            f32               mSolidV      = 0.0f;

            FVector2f         mWindowPos  = FVector2f(0.0f, 0.0f);
            FVector2f         mWindowSize = FVector2f(0.0f, 0.0f);
            FVector2f         mContentMin = FVector2f(0.0f, 0.0f);
            FVector2f         mContentMax = FVector2f(0.0f, 0.0f);
            FVector2f         mCursor     = FVector2f(0.0f, 0.0f);
            FRect             mWindowRect{};
            FString           mCurrentWindowTitle;
            TVector<FString>* mWindowOrder = nullptr;
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
                ctx.Begin();

                const auto& bbDesc = backBuffer->GetDesc();
                const u32   w      = bbDesc.mWidth;
                const u32   h      = bbDesc.mHeight;

                UpdateConstants(w, h);

                ctx.RHISetGraphicsPipeline(mPipeline.Get());
                ctx.RHISetPrimitiveTopology(Rhi::ERhiPrimitiveTopology::TriangleList);

                Rhi::FRhiViewportRect vp{};
                vp.mX      = 0.0f;
                vp.mY      = 0.0f;
                vp.mWidth  = static_cast<f32>(w);
                vp.mHeight = static_cast<f32>(h);
                ctx.RHISetViewport(vp);

                Rhi::FRhiRenderPassColorAttachment colorAtt{};
                colorAtt.mView    = mBackBufferRtv.Get();
                colorAtt.mLoadOp  = Rhi::ERhiLoadOp::Load;
                colorAtt.mStoreOp = Rhi::ERhiStoreOp::Store;
                Rhi::FRhiRenderPassDesc rp{};
                rp.mDebugName.Assign(TEXT("DebugGui.RenderPass"));
                rp.mColorAttachmentCount = 1U;
                rp.mColorAttachments     = &colorAtt;
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
                ctx.End();

                auto* commandList = commandContext->GetCommandList();
                if (commandList == nullptr) {
                    return;
                }

                auto queue = device.GetQueue(Rhi::ERhiQueueType::Graphics);
                if (!queue) {
                    return;
                }

                Rhi::FRhiCommandList* lists[] = { commandList };
                Rhi::FRhiSubmitInfo   submit{};
                submit.mCommandLists     = lists;
                submit.mCommandListCount = 1U;
                queue->Submit(submit);
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
                mSampler = device.CreateSampler(s);
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
                    e.mBinding    = 0U;
                    e.mType       = Rhi::ERhiBindingType::SampledTexture;
                    e.mVisibility = Rhi::ERhiShaderStageFlags::Pixel;
                    layout.mEntries.PushBack(e);
                }
                // s0
                {
                    Rhi::FRhiBindGroupLayoutEntry e{};
                    e.mBinding    = 0U;
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
                    e.mBinding = 0U;
                    e.mType    = Rhi::ERhiBindingType::SampledTexture;
                    e.mTexture = mFontTexture.Get();
                    bg.mEntries.PushBack(e);
                }
                {
                    Rhi::FRhiBindGroupEntry e{};
                    e.mBinding = 0U;
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

            if (!CompileStage(TEXT("VSMain"), Shader::EShaderStage::Vertex, mVertexShader)
                || !CompileStage(TEXT("PSMain"), Shader::EShaderStage::Pixel, mPixelShader)) {
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

            void SetExternalStats(const FDebugGuiExternalStats& stats) noexcept override {
                FScopedLock lock(mMutex);
                mExternalStats = stats;
            }

            void TickGameThread(const Input::FInputSystem& input, f32 dtSeconds, u32 displayWidth,
                u32 displayHeight) override {
                const FVector2f displaySize(
                    static_cast<f32>(displayWidth), static_cast<f32>(displayHeight));

                // Toggle via F1.
                const bool             toggle = input.WasKeyPressed(Input::EKey::F1);

                bool                   enabled = false;
                FDebugGuiExternalStats ext{};
                {
                    FScopedLock lock(mMutex);
                    if (toggle) {
                        mEnabledGameThread = !mEnabledGameThread;
                    }
                    enabled = mEnabledGameThread;
                    ext     = mExternalStats;
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

                FDebugGuiContext ctx(
                    mPending, mClip, guiInput, mUi, displaySize, mFont, mWindowOrder);
                ctx.PushClipRect({ FVector2f(0.0f, 0.0f), displaySize });

                // Built-in panels.
                if (mShowStats) {
                    DrawStatsWindow(ctx, dtSeconds, ext);
                }
                if (mShowConsole) {
                    DrawConsoleWindow(ctx, guiInput);
                }
                if (mShowCVars) {
                    DrawCVarsWindow(ctx);
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

            void DrawCVarsWindow(IDebugGui& gui) {
                if (!gui.BeginWindow(TEXT("DebugGui CVars"), nullptr)) {
                    return;
                }

                gui.Text(TEXT("Click a CVar to edit:"));
                gui.Separator();

                TVector<FString> names;
                Core::Console::FConsoleVariable::ForEach(
                    [&](const Core::Console::FConsoleVariable& v) { names.PushBack(v.GetName()); });

                // List (limited).
                const usize maxShow = (names.Size() > 40U) ? 40U : names.Size();
                for (usize i = 0; i < maxShow; ++i) {
                    if (gui.Button(names[i].ToView())) {
                        mSelectedCVarName = names[i];
                        if (auto* cvar = Core::Console::FConsoleVariable::Find(mSelectedCVarName)) {
                            mSelectedCVarValue = cvar->GetString();
                        } else {
                            mSelectedCVarValue.Clear();
                        }
                    }
                }

                gui.Separator();
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
                }

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

            mutable FMutex         mMutex;
            bool                   mEnabledGameThread   = true;
            bool                   mEnabledRenderThread = true;
            TVector<FPanelEntry>   mPanels;
            FDebugGuiExternalStats mExternalStats{};

            FUIState               mUi{};
            FFontAtlas             mFont{};
            FClipRectStack         mClip{};
            FDrawData              mPending{};
            FDrawData              mRender{};
            FDebugGuiRendererD3D11 mRenderer{};
            FDebugGuiFrameStats    mLastStats{};

            // Window order used for deterministic stacking.
            TVector<FString>       mWindowOrder;

            // Console/log capture.
            static constexpr u32   kMaxLogLines = 2000U;
            mutable FMutex         mLogMutex;
            TVector<FLogLine>      mLogLines;
            u32                    mLogStart            = 0U; // Oldest entry index in ring buffer.
            u32                    mLogCount            = 0U;
            bool                   mHasAttachedSink     = false;
            bool                   mForwardLogToDefault = true;
            FString                mConsoleInput;
            FString                mConsoleFilter;
            i32                    mConsoleScrollOffset = 0; // 0: bottom/newest
            TVector<u32>           mConsoleFilteredOrderIndices;

            // CVars panel state.
            FString                mSelectedCVarName;
            FString                mSelectedCVarValue;

            // Panel toggles.
            bool                   mShowStats   = true;
            bool                   mShowConsole = true;
            bool                   mShowCVars   = true;
        };
    } // namespace

    auto CreateDebugGuiSystem() -> IDebugGuiSystem* {
        return new FDebugGuiSystem(); // NOLINT
    }

    void DestroyDebugGuiSystem(IDebugGuiSystem* sys) {
        delete sys; // NOLINT
    }
} // namespace AltinaEngine::DebugGui
