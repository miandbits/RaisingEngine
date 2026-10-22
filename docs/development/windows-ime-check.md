# Windows Korean IME verification

## Native ordering follow-up

The user enabled Hangul input for the previous cancellation build. Real
`ㅎ → 하` composition then exposed an earlier Windows event order: the final
IME commit arrived before Escape's key message, and between Control and Z.
The earlier native-message guard alone therefore did not resolve either case.

The new guard holds a final preedit commit while Escape or Control is down.
Escape/Ctrl+Z discard it before it enters text or undo history. Other keys,
Control release, and focus movement deliver the exact original commit event
(including replacement offsets). A trailing empty IME event cannot prematurely
flush the pending commit. Normal reset cancellation remains scoped to the
focused field. No document snapshots or destructive text replacement are used.

The interaction suite passes 10 cases including initialization/cleanup (8
substantive cases). New regressions reproduce commit-before-key ordering,
trailing empty IME events, preservation of text and undo/redo history, normal
save-key/Control-release commits, and focus-out commits in QTextEdit and
QPlainTextEdit as well as QLineEdit. Repository hygiene checks also pass.

Native acceptance of this additional ordering fix is pending the new process's
Hangul-mode switch. The new executable and disposable workshop project are
ready; synthetic checks are not a substitute for that final native retest.

## Follow-up: cancellation fix, native retest pending

The next 20-minute pass adds a Windows native-message guard for Escape and
Ctrl+Z before normal Qt shortcut handling. It only owns text composition in
its editor's widgets and child dialogs; unrelated windows and movement keys
are left alone. The existing Qt event guard remains the cross-platform path.

One underlying issue was confirmed in the installed Qt version's upstream
implementation: Windows `reset()` sends a commit event before cancelling the
IME. The editor now suppresses input-method events to the cancellation target
only during that synchronous reset, then clears its preedit. It does not
replace the field text, clear its undo history, or suppress normal save commits.
See [Qt 6.10.1 Windows input context](https://github.com/qt/qtbase/blob/v6.10.1/src/plugins/platforms/windows/qwindowsinputcontext.cpp).

The updated interaction suite passed 8 cases including initialization/cleanup
(6 substantive cases). New regressions model a platform reset that commits
text, verify both Escape and Ctrl+Z preserve earlier text and dialog visibility,
and verify that subsequent normal undo/redo still work. A Windows-message test
also covers owner isolation, non-cancel keys and second Escape after cancellation.

The final Windows editor also saved and reopened a disposable workshop copy
with the exact name `공방 제목 · 입력 검증` and a clean saved state. This GUI
roundtrip used Unicode text insertion, not an active Microsoft IME composition.

**Actual Microsoft IME acceptance remains pending.** The new process started
in Latin input mode; desktop input continued to produce Latin G. A user-assisted
Hangul switch/composition was requested but not yet received. Synthetic tests
and the source-confirmed reset fix do not prove native Escape/Ctrl+Z acceptance.
The prior failures below remain acceptance criteria until retested.

## Earlier native-input observations

On 2026-10-22, a 20-minute session used Qt 6.10.1 on Windows and Korean
Microsoft IME in Hangul mode, with a disposable workshop project and a separate
editor profile. The user enabled the input method and physically started a
syllable. Desktop key input then visibly composed `ㅎ → 하 → 한` in the name
inspector. This was real preedit, not pasted Unicode or synthetic Qt events.

The automated driver required Shift+g, Shift+k, Shift+s after the user's
initial composition. These keys retain the same Hangul letters on this layout.
Unshifted automated g had previously produced Latin text. Physical-key-only
reproduction is still needed to separate native IME behavior from input-driver
effects. Do not treat this session as a pass for every Windows keyboard path.

| Operation | Observed result |
| --- | --- |
| Ctrl+S with active preedit | The final character committed and was saved once. The editor showed a clean saved state. |
| Ctrl+C with active preedit, no selection | The IME committed the syllable; text remained intact and UI elements were not copied. Clipboard contents were not independently verified in this native-input case. |
| Escape in the inspector | The syllable became committed instead of being cancelled. Earlier text remained intact. This does not meet the intended preedit-cancel behavior. |
| Ctrl+Z with active preedit | The current insertion group was undone, including earlier text in the name field. Ctrl+Y restored the complete text. No project-history undo was observed. This does not meet the intended preedit-only first undo. |
| F6 with active preedit | The final character committed and the hierarchy updated; panel navigation did not lose the character. Reverse movement during native composition was not separately verified. |
| Ctrl+K with active preedit | The name committed and tool search opened without losing text. |
| Escape with preedit in tool search | The search dialog closed on the first Escape. This does not meet the intended first-Escape-only-cancels-preedit behavior. |
| Save, close and reopen | Passed: the exact visible test name `한ㅎㅎㅎㅎㅎ` persisted, and the reopened editor was clean. |

The two open issue groups are **Escape cancellation** (inspector and child
dialog) and **undo granularity during composition**. In the initial verification
commit, production input handling was unchanged. The follow-up above addresses
the reset path, but actual Microsoft IME retesting is still necessary; a passing
synthetic test alone is insufficient.

## Automated regression scope

The interaction target passed 6 cases including initialization and cleanup
(4 substantive cases). This session added a copy-during-preedit assertion to
the existing child-dialog test. It checks committed text, clipboard content,
and the synthetic preedit flag. These tests send QInputMethodEvent; they do
not model the observed native commit-before-shortcut behavior. Their pass must
not be reported as resolution of the two native-input issues above.

## Reproduction checklist

Use a disposable project and preserve existing user windows and files.

1. Focus an editable name field and select Korean Microsoft IME, Hangul (가).
2. Physically type g, k, s separately and observe `ㅎ → 하 → 한` with an
   active composition indicator. Pasting Hangul is not equivalent.
3. While composing, test Ctrl+S, Escape, Ctrl+Z, Ctrl+C, F6 and Ctrl+K separately.
4. Check that save and focus movement commit exactly once. Copy may commit the
   syllable in the native IME, but must preserve text and avoid element-copy.
5. The intended first Escape/Undo cancels preedit without closing a tool or
   undoing earlier committed text. Check both inspector and child dialogs.
6. Enter another syllable after each action to detect stuck input or duplicates.
7. Save, close and reopen; compare persisted text with the last visible value.

Record physical keyboard and automated driver results separately. Candidate
windows, Microsoft Old Hangul, Japanese conversion, and every custom text
widget remain outside this session's verified scope.

Microsoft documents the input methods and composing Hangul in its
[Korean IME documentation](https://learn.microsoft.com/en-us/globalization/input/korean-ime).
