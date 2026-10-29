# Current reminders

Current reminders let a designer show a small, ordered set of existing screen text using the player's current state. They can describe a current commitment, date, resources or relationship without adding a command, changing the save format or creating a transcript.

In **Project settings → Reading**, choose **Choose reminder text…**:

1. Select an existing source screen. Ordinary and modal screens are both supported.
2. Add the text elements to **Reminder order**, then use **Up** and **Down** to order them. Select 1–64 distinct elements.
3. Optionally enable **Show automatically after loading a save**.
4. Apply the selection, then apply Project settings. This is one editor edit with Undo/Redo; save the project to persist it.

The source text, screen, components and buttons are preserved. The player can open **Current reminders** through the reading menu or settings. A button can also use **Open current reminders**. Source-screen buttons are never copied into the reminder panel.

Supported text includes fixed text and the existing calendar, location, user-field, attribute-value, relationship-value and item-count bindings. Relationship text can use the current `{value}` and `{label}`; a format containing `{delta}` is excluded because it describes a transient change. Stable component text, including nested components, is selectable. Conditions, visibility and translations still apply to the current state. Canvas clipping does not remove a selected reminder. Text that is currently hidden or empty is omitted.

Dialogue, ending text, result/choice feedback, selected-row data, collection-generated rows and other transient bindings are not eligible. A reminder is designer-authored current information, not reading history or an automatic story summary. Choose wording and conditions that do not reveal information the player has not learned.

Missing or unsupported selected text stays visible in the chooser so it can be removed or replaced. Changing the source screen does not silently discard the existing selection. Shared validation points back to the Reading settings for repair. To disable reminders, first change any connected reminder-button actions, then clear **Enable current reminders**. Disabling removes only the feature configuration, and Undo restores it.

Automatic display follows a successful save restore when explicitly enabled. A failed restore does not trigger it. If all selected reminders are currently hidden or empty, loading returns directly to the game without an extra Continue step. Players can still open Current reminders manually to see its empty-state explanation. The panel uses the restored/current state; it does not preserve an earlier branch's text or record a new campaign command. The original reading history remains a separate session-only feature. Runtime acceptance separately checks preview, gallery and lifecycle behavior; this editor page does not establish human comprehension or physical-device accessibility.
