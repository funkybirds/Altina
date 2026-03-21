#include "DebugGui/DebugGui.h"

#include "CoreMinimal.h"

#include "Container/SmartPtr.h"
#include "Container/String.h"
#include "Container/HashMap.h"
#include "Container/Vector.h"
#include "Logging/Log.h"
#include "Console/ConsoleVariable.h"
#include "Threading/Mutex.h"

#include "Input/InputSystem.h"
#include "Input/Keys.h"

#include "DebugGui/Core/DebugGuiCoreTypes.h"
#include "DebugGui/Core/FontAtlas.h"
#include "DebugGui/Core/IconAtlas.h"
#include "DebugGui/Widgets/DebugGuiContext.h"
#include "DebugGui/Rendering/DebugGuiRendererD3D11.h"

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
        using Core::Math::Clamp;
        using Core::Math::FVector2f;
        using Core::Threading::FMutex;
        using Core::Threading::FScopedLock;

        using namespace AltinaEngine::DebugGui::Private;
        class FDebugGuiSystem final : public IDebugGuiSystem {
        public:
            FDebugGuiSystem() {
                mFont.Build();
                mIconAtlas.Build();
            }
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

            void RegisterBackgroundOverlay(FStringView name, FPanelFn fn) override {
                if (name.IsEmpty() || !fn) {
                    return;
                }
                FScopedLock lock(mMutex);
                FPanelEntry e{};
                e.Name.Assign(name);
                e.Fn = Move(fn);
                mBackgroundOverlays.PushBack(Move(e));
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

            void SetImageTexture(u64 imageId, Rhi::FRhiTexture* texture) override {
                if (imageId == 0ULL) {
                    return;
                }
                FScopedLock lock(mMutex);
                if (texture == nullptr) {
                    mImageTextures.Erase(imageId);
                    return;
                }
                mImageTextures[imageId] = texture;
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
                FScopedLock    lock(mMutex);
                FDebugGuiTheme theme = mTheme;
                theme.mUiScale       = mRuntimeUiScale;
                return theme;
            }

            void TickGameThread(const Input::FInputSystem& input, f32 dtSeconds, u32 displayWidth,
                u32 displayHeight) override {
                const FVector2f displaySize(
                    static_cast<f32>(displayWidth), static_cast<f32>(displayHeight));

                // Toggle via F1.
                const bool             toggle = input.WasKeyPressed(Input::EKey::F1);

                bool                   enabled     = false;
                bool                   showStats   = false;
                bool                   showConsole = false;
                bool                   showCVars   = false;
                FDebugGuiExternalStats ext{};
                FDebugGuiTheme         theme{};
                TVector<FPanelEntry>   backgroundOverlays;
                TVector<FPanelEntry>   panels;
                TVector<FPanelEntry>   overlays;
                {
                    FScopedLock lock(mMutex);
                    if (toggle) {
                        mEnabledGameThread = !mEnabledGameThread;
                    }
                    enabled            = mEnabledGameThread;
                    showStats          = mShowStats;
                    showConsole        = mShowConsole;
                    showCVars          = mShowCVars;
                    ext                = mExternalStats;
                    theme              = mTheme;
                    backgroundOverlays = mBackgroundOverlays;
                    panels             = mPanels;
                    overlays           = mOverlays;
                }

                const f32 uiScale =
                    (ext.mDpiScale > 0.01f) ? Clamp(ext.mDpiScale, 0.5f, 4.0f) : 1.0f;
                {
                    FScopedLock lock(mMutex);
                    mRuntimeUiScale = uiScale;
                }
                FDebugGuiTheme scaledTheme = theme;
                ScaleThemeMetrics(scaledTheme, uiScale);
                scaledTheme.mUiScale = uiScale;

                mUi.ClearTransient();
                ++mUi.mFrameIndex;
                mPending.Clear();
                mPending.mFontScale =
                    (scaledTheme.mFontScale > 0.01f) ? scaledTheme.mFontScale : 1.0f;
                mPending.mFontSdfSoftness = Clamp(scaledTheme.mFontSdfSoftness, -0.25f, 0.25f);
                mPending.mFontSdfEdge     = Clamp(scaledTheme.mFontSdfEdge, 0.0f, 1.0f);
                mClip.Clear();
                mWindowOrder.Clear();

                if (!enabled || displayWidth == 0U || displayHeight == 0U) {
                    PublishPendingDrawData(enabled);
                    return;
                }

                EnsureLogSink();

                FGuiInput guiInput{};
                guiInput.mInput    = &input;
                guiInput.mMousePos = FVector2f(
                    static_cast<f32>(input.GetMouseX()), static_cast<f32>(input.GetMouseY()));
                guiInput.mMouseDeltaX         = input.GetMouseDeltaX();
                guiInput.mMouseDeltaY         = input.GetMouseDeltaY();
                guiInput.mMouseDown           = input.IsMouseButtonDown(0U);
                guiInput.mMousePressed        = input.WasMouseButtonPressed(0U);
                guiInput.mMouseReleased       = input.WasMouseButtonReleased(0U);
                guiInput.mMouseDownRight      = input.IsMouseButtonDown(1U);
                guiInput.mMousePressedRight   = input.WasMouseButtonPressed(1U);
                guiInput.mMouseReleasedRight  = input.WasMouseButtonReleased(1U);
                guiInput.mMouseWheelDelta     = input.GetMouseWheelDelta();
                guiInput.mKeyEnterPressed     = input.WasKeyPressed(Input::EKey::Enter);
                guiInput.mKeyBackspacePressed = input.WasKeyPressed(Input::EKey::Backspace);

                FDebugGuiContext ctx(mPending, mClip, guiInput, mUi, displaySize, mFont, mIconAtlas,
                    scaledTheme, mWindowOrder, mWindows, mDraggingWindowKey, mWindowDragOffset);
                ctx.PushClipRect({ FVector2f(0.0f, 0.0f), displaySize });

                // Background overlays (drawn below windows/panels).
                for (auto& o : backgroundOverlays) {
                    if (o.Fn) {
                        o.Fn(ctx);
                    }
                }

                // Built-in panels.
                if (showStats) {
                    DrawStatsWindow(ctx, dtSeconds, ext);
                }
                if (showConsole) {
                    DrawConsoleWindow(ctx, guiInput);
                }
                if (showCVars) {
                    DrawCVarsWindow(ctx, guiInput);
                }

                // Custom panels (can use widgets).
                for (auto& p : panels) {
                    if (p.Fn) {
                        if (ctx.BeginWindow(p.Name.ToView(), nullptr)) {
                            p.Fn(ctx);
                            ctx.EndWindow();
                        }
                    }
                }

                // Overlays (no window chrome).
                for (auto& o : overlays) {
                    if (o.Fn) {
                        o.Fn(ctx);
                    }
                }

                if (mUi.mActiveId != 0ULL) {
                    mUi.mWantsCaptureMouse = true;
                }

                ctx.PopClipRect();
                PublishPendingDrawData(enabled);
            }

            void RenderRenderThread(Rhi::FRhiDevice& device, Rhi::FRhiViewport& viewport) override {
                FDrawData                                drawData{};
                bool                                     enabled = false;
                FDebugGuiRendererD3D11::FImageTextureMap imageTextures{};
                {
                    FScopedLock lock(mMutex);
                    drawData      = mRender;
                    enabled       = mEnabledRenderThread;
                    imageTextures = mImageTextures;
                }
                if (!enabled) {
                    return;
                }
                mRenderer.SetExternalTextures(imageTextures);
                mRenderer.Render(device, viewport, drawData, mFont, mIconAtlas);
            }

            [[nodiscard]] auto WantsCaptureKeyboard() const noexcept -> bool override {
                return mUi.mWantsCaptureKeyboard;
            }
            [[nodiscard]] auto WantsCaptureMouse() const noexcept -> bool override {
                return mUi.mWantsCaptureMouse;
            }

            [[nodiscard]] auto GetLastFrameStats() const noexcept -> FDebugGuiFrameStats override {
                FScopedLock lock(mMutex);
                return mLastStats;
            }

        private:
            static void ScaleThemeMetrics(FDebugGuiTheme& theme, f32 uiScale) {
                if (uiScale <= 0.01f) {
                    return;
                }
                auto ScaleSurface = [uiScale](FDebugGuiSurfaceStyle& style) {
                    style.mCornerRadius *= uiScale;
                    style.mBorderThickness *= uiScale;
                };
                auto ScaleTabStyle = [uiScale](FDebugGuiTabStyle& style) {
                    style.mHeight *= uiScale;
                    style.mTextPadX *= uiScale;
                    style.mTextPadY *= uiScale;
                    style.mIconSize *= uiScale;
                    style.mIconPadX *= uiScale;
                    style.mUnderlineHeight *= uiScale;
                };
                auto ScaleMenuStyle = [uiScale](FDebugGuiMenuStyle& style) {
                    style.mHeight *= uiScale;
                    style.mItemHeight *= uiScale;
                    style.mItemPadX *= uiScale;
                    style.mPopupPad *= uiScale;
                    style.mPopupRadius *= uiScale;
                };
                auto ScaleSectionStyle = [uiScale](FDebugGuiSectionStyle& style) {
                    style.mSpacingY *= uiScale;
                    style.mUnderlineThickness *= uiScale;
                    style.mLabelPadX *= uiScale;
                    style.mLabelPadY *= uiScale;
                    style.mLabelRadius *= uiScale;
                };
                const FVector2f scaleVec(uiScale, uiScale);
                theme.mFonts.mBody.mScale *= uiScale;
                theme.mFonts.mSmall.mScale *= uiScale;
                theme.mFonts.mWindowTitle.mScale *= uiScale;
                theme.mFonts.mMenu.mScale *= uiScale;
                theme.mFonts.mTab.mScale *= uiScale;
                theme.mFonts.mSection.mScale *= uiScale;
                theme.mFonts.mLabel.mScale *= uiScale;
                theme.mFonts.mStatus.mScale *= uiScale;
                ScaleSurface(theme.mEditor.mWindowSurface);
                ScaleSurface(theme.mEditor.mPanelSurface);
                ScaleSurface(theme.mEditor.mInsetSurface);
                ScaleTabStyle(theme.mEditor.mTabs);
                ScaleMenuStyle(theme.mEditor.mMenu);
                ScaleSectionStyle(theme.mEditor.mSections);
                theme.mEditor.mWorkspacePadding *= uiScale;
                theme.mEditor.mSplitterSize *= uiScale;
                theme.mEditor.mPanelPadding *= uiScale;
                theme.mEditor.mMinPanelWidth *= uiScale;
                theme.mEditor.mMinCenterWidth *= uiScale;
                theme.mEditor.mMinTopHeight *= uiScale;
                theme.mEditor.mMinBottomHeight *= uiScale;
                theme.mEditor.mStatusBarHeight *= uiScale;
                theme.mEditor.mPanelGap *= uiScale;
                theme.mEditor.mMenuItemMarkerWidth *= uiScale;
                theme.mWindowDefaultSize *= scaleVec;
                theme.mWindowDefaultPos *= scaleVec;
                theme.mWindowPadding *= uiScale;
                theme.mWindowSpacing *= uiScale;
                theme.mTitleBarHeight *= uiScale;
                theme.mTitleTextOffsetY *= uiScale;
                theme.mSeparatorPaddingY *= uiScale;
                theme.mItemSpacingY *= uiScale;
                theme.mButtonPaddingX *= uiScale;
                theme.mButtonPaddingY *= uiScale;
                theme.mCheckboxBoxSize *= uiScale;
                theme.mCheckboxTextOffsetX *= uiScale;
                theme.mCheckboxMarkInset *= uiScale;
                theme.mSliderHeight *= uiScale;
                theme.mSliderBottomSpacingY *= uiScale;
                theme.mInputHeight *= uiScale;
                theme.mInputTextOffsetX *= uiScale;
                theme.mInputTextOffsetY *= uiScale;
                theme.mInputBottomSpacingY *= uiScale;
                theme.mGizmoSize *= uiScale;
                theme.mGizmoPadding *= uiScale;
                theme.mGizmoAxisThickness *= uiScale;
                theme.mGizmoHitRadius *= uiScale;
                theme.mGizmoCenterHalfSize *= uiScale;
                theme.mGizmoBottomSpacingY *= uiScale;
                theme.mGizmoDragSensitivity *= uiScale;
                theme.mScrollBarWidth *= uiScale;
                theme.mScrollBarPadding *= uiScale;
                theme.mScrollBarThumbMinHeight *= uiScale;
                theme.mCollapseButtonSize *= uiScale;
                theme.mCollapseButtonPadX *= uiScale;
                theme.mCollapseButtonOffsetY *= uiScale;
                theme.mCollapseIconHalfWidth *= uiScale;
                theme.mCollapseIconHalfHeight *= uiScale;
                theme.mTreeRowHeight *= uiScale;
                theme.mTreeIndent *= uiScale;
                theme.mTreeArrowSize *= uiScale;
                theme.mTreeTextPadX *= uiScale;
                theme.mIconLabelPadY *= uiScale;
                theme.mIconInnerPadding *= uiScale;
                theme.mFontScale *= uiScale;
            }

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
                line.AppendNumber(ext.mFrameIndex);
                gui.Text(line.ToView());

                line.Clear();
                line.Assign(TEXT("dt(ms): "));
                line.AppendNumber(static_cast<int>(dtSeconds * 1000.0f));
                line.Append(TEXT("  fps: "));
                line.AppendNumber(static_cast<int>(fps));
                gui.Text(line.ToView());

                line.Clear();
                line.Assign(TEXT("Views: "));
                line.AppendNumber(ext.mViewCount);
                line.Append(TEXT("  SceneBatches: "));
                line.AppendNumber(ext.mSceneBatchCount);
                gui.Text(line.ToView());

                const auto draw = GetLastFrameStats();
                line.Clear();
                line.Assign(TEXT("Draw: vtx="));
                line.AppendNumber(draw.mVertexCount);
                line.Append(TEXT(" idx="));
                line.AppendNumber(draw.mIndexCount);
                line.Append(TEXT(" cmd="));
                line.AppendNumber(draw.mCmdCount);
                gui.Text(line.ToView());

                gui.EndWindow();
            }

            void DrawConsoleWindow(FDebugGuiContext& gui, const FGuiInput& input) {
                if (!gui.BeginWindow(TEXT("DebugGui Console"), nullptr)) {
                    return;
                }

                (void)gui.InputText(TEXT("Filter"), mConsoleFilter);
                gui.Separator();

                if (gui.IsMouseHoveringRect(gui.GetContentRect()) && input.mMouseWheelDelta != 0.0f
                    && mUi.mActiveId == 0ULL) {
                    const i32 step = 3;
                    if (input.mMouseWheelDelta > 0.0f) {
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
                if (input.mKeyEnterPressed && mUi.mActiveId == commandId
                    && !mConsoleInput.IsEmptyString()) {
                    mConsoleScrollOffset = 0;
                    ExecuteConsoleCommand(mConsoleInput);
                    mConsoleInput.Clear();
                    mUi.mActiveId = 0ULL;
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
                if (listHovered && input.mMouseWheelDelta != 0.0f && mUi.mActiveId == 0ULL) {
                    const i32 step = 3;
                    if (input.mMouseWheelDelta > 0.0f) {
                        mCVarsScrollIndex -= step;
                    } else {
                        mCVarsScrollIndex += step;
                    }
                }

                const f32 itemHf       = th.mInputHeight;
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
                const f32  kScrollBarW    = th.mScrollBarWidth;
                const f32  kScrollBarPad  = th.mScrollBarPadding;
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
                    const f32 thumbMinH = th.mScrollBarThumbMinHeight;
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
                    const bool  sbActive     = (mUi.mActiveId == sbId);

                    if ((trackHovered || thumbHovered) && input.mMousePressed) {
                        mUi.mActiveId          = sbId;
                        mUi.mFocusId           = sbId;
                        mUi.mWantsCaptureMouse = true;
                        if (thumbHovered) {
                            mCVarsScrollDragOffsetY = input.mMousePos.Y() - thumbRect.Min.Y();
                        } else {
                            mCVarsScrollDragOffsetY = thumbH * 0.5f;
                        }
                    }

                    if (sbActive && input.mMouseDown) {
                        mUi.mWantsCaptureMouse  = true;
                        const f32 desiredThumbY = input.mMousePos.Y() - mCVarsScrollDragOffsetY;
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
                    if (sbActive && input.mMouseReleased) {
                        mUi.mActiveId = 0ULL;
                    }

                    // Draw track + thumb.
                    gui.DrawRectFilled(scrollTrackRect, th.mScrollBarTrackBg);
                    gui.DrawRect(scrollTrackRect, th.mScrollBarTrackBorder, 1.0f);
                    const FColor32 thumbCol = sbActive
                        ? th.mScrollBarThumbActiveBg
                        : (thumbHovered ? th.mScrollBarThumbHoverBg : th.mScrollBarThumbBg);
                    gui.DrawRectFilled(thumbRect, thumbCol);
                    gui.DrawRect(thumbRect, th.mScrollBarThumbBorder, 1.0f);
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
                        mUi.mHotId             = id;
                        mUi.mWantsCaptureMouse = true;
                    }
                    if (hovered && input.mMousePressed) {
                        mUi.mActiveId = id;
                        mUi.mFocusId  = id;
                    }

                    bool clicked = false;
                    if (mUi.mActiveId == id && input.mMouseReleased) {
                        if (hovered) {
                            clicked = true;
                        }
                        mUi.mActiveId = 0ULL;
                    }

                    const bool     selected = (!mSelectedCVarName.IsEmptyString()
                        && mSelectedCVarName.ToView() == name.ToView());
                    const FColor32 bg =
                        selected ? th.mSelectedRowBg : (hovered ? th.mHoveredRowBg : 0U);
                    if ((bg >> 24U) != 0U) {
                        gui.DrawRectFilled(r, bg);
                    }
                    gui.DrawText(FVector2f(r.Min.X() + th.mInputTextOffsetX,
                                     r.Min.Y() + th.mInputTextOffsetY),
                        th.mText, name.ToView());

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
                mLastStats.mVertexCount = static_cast<u32>(mPending.mVertices.Size());
                mLastStats.mIndexCount  = static_cast<u32>(mPending.mIndices.Size());
                mLastStats.mCmdCount    = static_cast<u32>(mPending.mCmds.Size());
                mRender                 = Move(mPending);
                mEnabledRenderThread    = enabled;
            }

            mutable FMutex                           mMutex;
            bool                                     mEnabledGameThread   = true;
            bool                                     mEnabledRenderThread = true;
            TVector<FPanelEntry>                     mPanels;
            TVector<FPanelEntry>                     mBackgroundOverlays;
            TVector<FPanelEntry>                     mOverlays;
            FDebugGuiRendererD3D11::FImageTextureMap mImageTextures;
            FDebugGuiExternalStats                   mExternalStats{};
            FDebugGuiTheme                           mTheme{};
            f32                                      mRuntimeUiScale = 1.0f;

            FUIState                                 mUi{};
            FFontAtlas                               mFont{};
            FIconAtlas                               mIconAtlas{};
            FClipRectStack                           mClip{};
            FDrawData                                mPending{};
            FDrawData                                mRender{};
            FDebugGuiRendererD3D11                   mRenderer{};
            FDebugGuiFrameStats                      mLastStats{};

            // Window order used for deterministic stacking.
            TVector<FString>                         mWindowOrder;
            Container::THashMap<u64, FWindowState>   mWindows;
            u64                                      mDraggingWindowKey = 0ULL;
            FVector2f                                mWindowDragOffset  = FVector2f(0.0f, 0.0f);

            // Console/log capture.
            static constexpr u32                     kMaxLogLines = 2000U;
            mutable FMutex                           mLogMutex;
            TVector<FLogLine>                        mLogLines;
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
            bool         mShowStats   = false;
            bool         mShowConsole = false;
            bool         mShowCVars   = false;
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
