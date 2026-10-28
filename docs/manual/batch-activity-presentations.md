# Create activity presentations in bulk

Open **Design → Activity / daily presentation timelines**, then choose **Create or link activity presentations in bulk…** above the workshop tabs.

1. Choose **Create a new presentation for each activity** or **Link to an existing presentation**.
2. Select the activities. Activities without a presentation are selected initially. Existing links are skipped unless you explicitly enable replacement of their default presentation.
3. For creation, edit the shared clips: duration, token text, image source, and transition. The activity-background option uses that activity's existing `presentation.backgroundAssetId`. Choose an explicit fallback image or **Text only** for activities without an available background. A fixed image can instead be chosen for an individual clip.
4. Click **Review changes**. The table lists every selected activity, what will be created or kept, its resulting presentation, and whether a fallback is used. Editing any input invalidates the review.
5. Click **Apply reviewed changes**. This updates only the open workshop. Inspect or preview the generated timelines, then **Save** the workshop to make one undoable project edit. Canceling either dialog discards that dialog's changes.

Supported text tokens are `{activity}`, `{outcome}`, `{day}`, and `{changes}`. They resolve from the actual committed activity result when played. New links play once per activity slot. The tool does not enable a logical-day calendar, add judgments, or change activity costs, effects, random state, rules, or story content.

Each generated timeline receives a collision-safe ID. Existing timelines are never overwritten. Replacement changes only the selected activities' default timeline reference; their success/failure references and repeat settings remain intact. Unselected activities and unresolved activity references are retained. The same normal workshop validation, save, undo, and reopen behavior applies.

Limits remain 500 timelines, 100 clips per timeline, 1–60,000 ms per clip, and 4,000 text characters per clip. Stage composition and more specialized success/failure presentations can be edited in the regular workshop after creating the shared starting point.

The batch dialog has a scrolling settings/review body and a fixed Apply/Cancel footer. Keyboard focus reveals settings inside the scrolling area. The existing timeline preview remains the place to inspect image placement and token examples before saving.
