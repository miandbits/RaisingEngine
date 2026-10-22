# Character stories in the web player

Windows and the C++/Wasm web player share personal event completion, owned flags,
exact-choice records and `after-event` continuation. Rebuild both Wasm and the web
player; copying only the capability file into an older export does not upgrade it.

## Author and export

1. Open the two-character project (`tests/fixtures/projects/character-state-demo`
   is a suitable starting point). Enable `character-v1` and assign a shared action
   to both characters. Connect that action to an actor-scoped interaction event.
2. In the choice editor, record a promise with **Set flag → Acting character**.
   Two choices may set the same promise. In the event connection map, select only
   one choice and create a follow-up using that promise.
3. The generated conditions require the same actor's source completion, exact
   choice and promise. The new follow-up uses **After event**; its period window
   must include the current period.
4. Build the current web runtime and export the project as Web from Qt:

   ```sh
   npm run build:headless
   npm run build:web
   ```

   `build:web` requires the [Emscripten SDK](../../native/README.md).
   Its capability manifest includes `actor-story-links-v1`. Export to older web
   or Windows runtimes without that feature is rejected. Other unsupported
   project features continue to block export independently.
5. Serve the exported directory over HTTP(S), not `file://`. Play the first
   character's action and choose the linked option. Finish that follow-up, then
   use the second character and choose the other option. The second character's
   promise is recorded but the first character's exact choice cannot unlock it.

## Save and replay checks

The web **Save** button downloads JSON; **Load** selects that file. Save at the
source choice, during the follow-up, and after both characters finish. Reload
the page, load each file, and continue. Actor identity, return phase, inventory,
relationships and flags must agree with uninterrupted play. Repeat with the
character order reversed. Old saves with no personal flag map retain their shape.
Personal flags use the definition's initial value when absent, not shared values.
Loading validates both the save structure and the replayed command history;
changing a personal flag by hand does not silently change the game state.

Automated core/controller and export regressions:

```sh
node --test --test-concurrency=1 tests/native/actor-story-links.test.mjs tests/native/actor-story-web-controller.test.mjs tests/runtime/actor-story-export.test.mjs
```

The 2026-10-21 browser check used the **same unchanged GUI-authored two-character
project** as Windows. Real Chrome buttons and downloaded/uploaded saves were
tested in both character orders, including reloads at choices and follow-ups.
Both final states matched the actual Windows play reports, and all downloaded
saves passed native command replay. Local screenshots and save evidence are
kept outside the public source distribution.

This covers Chrome on Windows. It does not certify other browsers or mobile and
console platforms. Immediate post-event chains preserve the current actor and
the source event's return context. For calendar-period, completed-activity and
location-entry follow-ups, see [story appointments](story-appointments.md).
