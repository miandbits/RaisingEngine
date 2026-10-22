# Windows Korean IME verification

## Result: partial, with two open input issues

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
dialog) and **undo granularity during composition**. They remain unresolved in
this commit. Production input handling was not changed speculatively in the
remaining time. The next fix needs native message/input-method event ordering
evidence, followed by actual Microsoft IME retesting; a passing synthetic test
alone is insufficient.

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
