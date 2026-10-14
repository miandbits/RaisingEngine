# Character expressions and portrait images

The Qt content editor exposes the existing character `portraits` map. No new
runtime or schema contract is introduced.

1. Import an image through the asset tools.
2. Open the character in the content list and choose **표정 · 초상화**.
3. Select **표정 추가**, enter a unique expression ID such as `dance`, and select
   the image asset. Save the content edit, then save the project.
4. In a script scene, select that character and expression in the portrait row.

Expression IDs use the standard lower-case ID rules. The image selector offers
image assets only. Duplicate IDs, missing selections and deletion or renaming
of an expression still referenced by a scene are rejected while keeping the
dialog open. To change a referenced expression's appearance, replace its image
under the same expression ID. To remove it, change the referencing scenes first.
Protection also covers expressions used indirectly by a speaking character's
`speaker-portrait` UI image, including its default `face` expression. It only
protects expressions that existed before the edit; it does not require a `face`
mapping on characters which legitimately never had one.

The form copies the original character object, preserving names, descriptions,
relationships and other existing fields. Each accepted edit uses the shared
validation/history service, so undo, redo and project save follow the existing
transaction path. Cancel does not change the project.

Focused Qt tests `contentCharacterPortraitRoundtrip` and
`contentCharacterPortraitGuards` passed on 2026-10-13 (2 cases, plus initialization
and cleanup). They use disposable project copies and cover addition, image
replacement, unreferenced deletion, unchanged round-trips, undo/redo, disk
save/reopen, duplicate IDs, blank image choices, referenced deletion/renaming
and cancel. The follow-up `contentCharacterSpeakerPortraitGuard` also passed:
the fixture uses dialogue speakers with default `face` and explicit `smile`
image bindings, with no direct scene reference to either expression. It checks
that removing or renaming either existing key is rejected, while initially
absent keys are left optional. Actual production scene playback is a separate
GUI verification.
