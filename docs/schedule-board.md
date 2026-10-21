# Schedule board and presentation saves

The Windows native player supports `schedule-board-v1`. Projects using this
feature require a rebuilt player. Export refuses older Windows players and web
players that do not advertise it; this change does not add web support.

In the Qt editor:

1. Add a **Schedule** element and open **Widget settings**.
2. Choose **Activity cards, plan and forecast**.
3. Optionally select attributes where lower values are better, such as fatigue.
4. Add a button with **Confirm month plan and start**.
5. A text element can bind to **Draft schedule summary** or **Activity progress**.
6. For a Result element, choose **Reveal changes with playback** to hide
   unrevealed zero deltas and show changes alongside an animation.
7. Open **Choose activity icons…** to select an imported image for each
   activity by name. The board displays it on the activity card and assigned
   period. Selecting **None** removes an assignment.

The board selects a period, then assigns an activity. Selecting another activity
replaces the selected period. Clear affects only that period. Navigation does not
commit the draft or advance time. The confirm button requires all periods to be
assigned. The existing continuous-schedule setting controls subsequent progress;
events and narrative choices retain their normal stops.

## Data

- `scheduleLayout: "board"` opts in; existing layouts keep their behavior.
- `decreasingAttributeIds` identifies attributes whose reductions are favorable.
- `attributeIds` limits the forecast attributes shown.
- `activityIcons: [{ activityId, imageAssetId }]` optionally gives each activity
  an icon. Missing assignments remain text-only. IDs must refer to existing
  activities and image assets; duplicate activity assignments are rejected.
  This requires native `schedule-board-icons-v1`; web export rejects it until
  that renderer supports the same feature.
- `action: "commitPlan"` confirms and starts the schedule.
- `binding: "schedule-summary"` displays the draft. `valueFormat: "count"`
  displays a short assigned-period count.
- `binding: "activity-progress"` displays the current period and activity.
- `resultLayout: "changes"` opts in to progressive result labels. It requires
  the project's `resultPlayback` configuration to animate the changes.

The forecast runs the core on a **copy** of the current state in activities-only
mode, in schedule order. It includes core clamping, costs and period boundaries,
but excludes narrative choices and the final month's settlement. Activities with
success checks, expeditions, or intermediate decisions do not receive a
definitive forecast. It is a planning aid, not a proof of future event outcomes.
The player state, revision, RNG and command log are not changed by this preview.

`playerPresentation.scheduleDraft` stores the month, selected period,
assigned-period bitmask and activity IDs. The native loader validates the draft
before committing the replayed save. Older saves without this optional field
continue to load. A malformed draft cannot partially replace the simulation.

The shared save codec also accepts and validates native `saveRecord` metadata
(date, location, screenshot basename and sequence), so migration tools can read
native multi-slot saves. Metadata never changes simulation results. Migration
still requires an explicit presentation reset; a caller may retain presentation
only after validating it against the target content. This does not add a web
schedule-board or multi-slot UI.

Opening an authored modal pauses automatic schedule progress, result playback and
live animation playback. Closing it resumes the existing pause/speed state.

## Verification

```powershell
$env:RAISING_NATIVE_PLAYER_RUNTIME = 'dist/native-player-uiux'
node --test tests/native-player/schedule-board.test.mjs
```

The test covers partial drafts, navigation, clear/restore, preview isolation,
small-window operation, modal pause, mid-result restoration, legacy saves and
invalid-draft rejection. `calendarWidgetAuthoring` in the Qt UI suite covers the
editor's board and result-layout settings and their saved project data.
