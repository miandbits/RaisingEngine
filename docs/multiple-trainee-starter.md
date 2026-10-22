# Starting a multiple-trainee raising game

Choose **Multiple trainees · personal skills and shared funds** from the project launcher or **File → New project**. Both use the existing public `examples/genre-suite/roster` sample and create an independent copy in an empty folder.

The starter has two trainees, separate skill/stamina/stress, shared funds, per-character activity assignment, and a short complete month. Edit participants through **Design → Trainees / activity assignment**. Add existing characters by name, or create a new trainee. Excluding a character from the roster preserves the character document.

Screen cards and ending conditions remain authored content. Adding a trainee does not implicitly add a fixed card or rewrite an ending. For dynamic screens, use data lists/components; for a fixed composition, add a card and connect its personal data explicitly.

Verification for this change:

- Copy-service tests: valid independent snapshot, personal/shared scope, existing destination preservation.
- Qt `rosterTemplateRoundtrip`: launcher creation, opening in editor, creation from the editor wizard.
- Same Qt test against a locally packaged engine directory, using its bundled service and sample.
- Package creation includes the starter from the same template list used by the service. No installer or public release was published for this change.
