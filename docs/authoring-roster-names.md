# Named roster authoring

The roster editor now offers existing characters by name, as well as creation of a new character. Duplicate names are disambiguated, members already included are omitted, and generated identifiers avoid collisions with all character documents. Reference identifiers remain available as a read-only view.

Only personal attributes appear in member columns. Shared attributes are listed separately with a pointer to the project's initial-state settings. Renaming preserves the rest of the character document; excluding a member preserves its content for reuse. The protagonist cannot be removed here.

## Verification

- `StudioTests::rosterNamedMembers`: existing/new members, duplicate prevention, shared/personal columns, stable identifiers, character preservation, cancel, undo/redo, save and reopen.
- Existing `rosterAuthoringRoundtrip` also passes.
- Korean, English and Japanese translation coverage checked.
- The production dialog was inspected in a running Windows editor with a separate private two-character project. Private content and screenshots are not distributed in this repository.

This improves configuring a group of trainees; it does not introduce a new runtime format or change existing game rules. A separate first-time creator trial is still needed to assess discoverability across the whole authoring workflow.
