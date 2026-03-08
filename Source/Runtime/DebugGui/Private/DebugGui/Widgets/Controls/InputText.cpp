#include "DebugGui/Widgets/DebugGuiContext.h"

#include "Input/InputSystem.h"
#include "Input/Keys.h"
namespace AltinaEngine::DebugGui::Private {
    auto FDebugGuiContext::InputText(FStringView label, Container::FString& value) -> bool {
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
            if (mInput.Input != nullptr) {
                const auto& chars = mInput.Input->GetCharInputs();
                for (const auto code : chars) {
                    if (code == 0U || code < 32U) {
                        continue;
                    }
                    value.Append(static_cast<TChar>(code));
                    changed = true;
                }

                if (chars.IsEmpty()) {
                    const bool shift = mInput.Input->IsKeyDown(Input::EKey::LeftShift)
                        || mInput.Input->IsKeyDown(Input::EKey::RightShift);

                    auto TryAppendAlpha = [&](Input::EKey key, char lower) {
                        if (!mInput.Input->WasKeyPressed(key)) {
                            return;
                        }
                        const char c = shift ? static_cast<char>(lower - ('a' - 'A')) : lower;
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
                mUi->ActiveId = 0ULL;
            }
        }

        const bool active = (mUi->ActiveId == id);
        DrawRectFilled(r, active ? mTheme->InputActiveBg : mTheme->InputBg);
        DrawRect(r, active ? mTheme->InputActiveBorder : mTheme->InputBorder, 1.0f);
        DrawText(
            FVector2f(r.Min.X() + mTheme->InputTextOffsetX, r.Min.Y() + mTheme->InputTextOffsetY),
            mTheme->InputText, value.ToView());

        AdvanceItem(FVector2f(w, h));
        return changed;
    }
} // namespace AltinaEngine::DebugGui::Private
