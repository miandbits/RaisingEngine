# Colors for generated controls

Create a named style in the visual editor, then choose it in **Appearance → Control color style** on a Schedule, Save slots, or Load slots widget. **Reading tool color style** in Reading layout supplies the default for generated controls in reading mode, including reading menus, dialogs, and the generated reading-options toolbar beside the authored canvas. A widget's own choice takes precedence, including its activity picker and forecast dialog.

Only these named-style fields are used:

| Style field | Generated control use |
| --- | --- |
| Fill | Normal button, select and option background; scroll track |
| Text color | Control labels |
| Border color | Normal control border; scrollbar thumb |
| Hover fill | Pointer hover background |
| Pressed fill | Pressed background |
| Disabled color | Disabled label |

These references are optional. With no reference, existing control colors stay unchanged. A field omitted from the chosen style keeps its existing control default; it does not inherit the visual design's default style. A missing style is reported by validation and export; a runtime that encounters it keeps the existing colors.

Images, surfaces, fonts, font size, padding, borders widths and corner radius in the selected style are ignored by this picker. Apply decorative surfaces and typography to the authored widget separately. Control colors do not change hitboxes, minimum touch targets, wrapping, scroll layout, command timing, save/load confirmations or Cancel-first focus. Authored action buttons retain their own styles. Selected/focused controls keep their structural cues, and existing destructive overwrite styling remains distinct.

Use **Default control colors** to remove the local reference. In reading mode that allows the Reading layout choice to apply. Ordinary authored canvas widgets do not inherit the reading default. The generated reading utility strip does use it; its fill and controls are part of the reading tools. Reading dialogs opened from the canvas use the reading default, since they are generated reading surfaces. Colors are supported by native and Web players through `ui-control-colors-v1`; old runtimes must be rebuilt before export.

Preview both ordinary and reading layouts, including 200% text and all interaction states. Choose readable foreground/background pairs yourself: this feature does not promise contrast for transparent or textured backgrounds. Per-state color overrides do not make image contrast measurable. Generated schedule/save widgets inside reusable component definitions remain subject to the engine's existing component-kind validation limits.

For JSON projects, use `controlStyleId` on the widget or `visual.readableLayout`. The value is an existing `visual.styles[].id`. Component overrides and presentation-variant values preserve the reference where the underlying element kind is supported. A reading-root choice does not recolor arbitrary authored text, panels or action buttons.
