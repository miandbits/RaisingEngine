# User fields: creation-only text contract

Optional project.userFields is an array (0..32) of {id,type:"text",label,initial,minLength,maxLength}. IDs use the existing ID contract and must be unique within userFields. All six properties are required. minLength is 0..256; maxLength is 1..256; minLength <= maxLength. initial is a valid value for that declaration.

Text is preserved exactly: no trimming, normalization, truncation, or coercion. Length counts Unicode scalar values (code points, not UTF-8 bytes or UTF-16 units). Reject invalid UTF-8/unpaired surrogates, C0/C1 controls U+0000..001F and U+007F..009F, and line separators U+2028/U+2029. Spaces are allowed. Empty text is allowed only when minLength=0.

Creation accepts options.userValues?: Record<string,string>. Merge declared initial values with supplied values; reject unknown IDs, wrong types, invalid text, or invalid lengths atomically, with a field-specific error path /userValues/<id>. Missing values use initial. A project with no declarations accepts omitted/empty userValues but rejects unknown keys.

Only projects with at least one declared field generate state.userValues and origin.userValues. State contains the fully resolved values; origin records supplied values (an empty object is valid), so replay applies the same defaults. Field-less projects never acquire either key: unchanged legacy saves must replay to exactly the same state. Unknown save keys and altered origin/state retain existing schema/replay rejection behavior. Content fingerprint checks remain strict; adding declarations does not migrate a save from a different project fingerprint.

Visual elements: kind:"input" requires userFieldId referring to a declaration. Text elements may use binding:"user-field" plus userFieldId and existing valueFormat (default "{value}"). Input values are UI draft data until action:"start" invokes newRun(version,userValues?). Existing calls omitting the second argument remain valid. No simulation command or phase is added. Input is creation-only; after a run exists render it disabled/read-only. Text binding shows state values during a run and drafts/defaults before creation. Render names as escaped text, never markup.

Keep draft changes local without rebuilding the entire native Rml document on every keystroke. Cancel/navigation must not mutate a running session. On create failure retain both the draft and existing session. Load success replaces display values with loaded state and clears obsolete creation drafts. Native, desktop, and web newRun pass the same map to their creation backend and persist origin consistently.

Required verification: Unicode Korean/non-BMP code point lengths; invalid controls/surrogates/types/IDs; atomic rejection; identical TS/native/WASM state; seed+commands+userValues replay; save/load and tamper rejection; absent keys and unchanged save roundtrip for field-less projects; real IME entry/focus; Qt authoring/undo/save and runtime binding.

Out of scope: runtime rename commands, arbitrary string expressions, numeric/select field editors, console software keyboards, or changing original game formulas.
