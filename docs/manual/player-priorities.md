# Player priorities and sampled balance

Open **Auto play · Route map** from the editor toolbar (or the **Authoring tools** menu), then select **Player priorities** to test a saved set of immediate-result preferences. This is an optional analysis strategy. It does not change game rules, resources, saves, or the existing Random, Focus, Conserve, and Explore strategies.

The Player priorities strategy currently supports single-character schedules. It does not support roster projects or target search. Existing strategies and their roster behavior are unchanged.

## Save a profile

1. Open the Analysis pane and choose **New priorities…**.
2. Give the profile a name. Under **Immediate-result preferences**, choose Increase or Decrease for a global attribute and a weight from 1 to 10. Leave other rows at No preference.
3. Under **Keep within preferred limits**, optionally set a minimum, maximum, or both for resources and conditions. Money, stamina, and stress can use preferred limits; a stat cannot. Limits must fit the attribute's declared bounds.
4. Save, select the profile, and choose **Player priorities**. Each list permits up to 32 entries; at least one preference or preferred limit is required.

The strategy compares accepted immediate candidate results. It first prefers smaller violations of the preferred limits, normalized by each attribute's declared range, then larger weighted directional gains. Ties use the deterministic analysis seed. These are preferences, not hard requirements: no candidate may meet every limit. There is no future prediction, optimality guarantee, or assessment of whether a person will enjoy the route.

Profiles are authoring files at `playtests/player-priorities.json`. They are included in project backups and recovery, but excluded from compiled game content and its fingerprint. Saving a profile does not dirty game documents or rewrite campaign saves. Profile saves check an optimistic revision; refresh before retrying a conflict. If an attribute is removed or no longer supports its saved limit, the profile is marked for review. Edit it and explicitly clear the unavailable row. The editor does not silently drop preferences.

Profiles belong to a game identity. Changing `gameId` makes an existing profile file unavailable rather than automatically rebinding it. Preserve the file and restore the original identity, or move it aside before deliberately creating profiles for the new identity.

## Run and inspect

Set the number of runs, seed, worker count, per-run command cap, and whole-job time limit. Long campaigns usually need a larger command cap than the default. A capped or timed-out attempt is not a finished game.

Player priorities records sampled resource and condition metrics. For an existing strategy, enable **Record resource and condition samples** to add those observations without changing its decisions. An optionally selected profile supplies preferred limits for observation only when using those strategies.

Open **Balance · sampled resources** to inspect:

- Recorded runs against requested runs, completion/outcome counts, and the separate job stop reason.
- Per-run limiting causes, when recorded: command cap, time limit, cancellation, or target-search state limit. Their denominator is the number of observed runs.
- The minimum and maximum observed value of each resource or condition.
- Runs observed outside each configured preferred limit, or touching an attribute's declared minimum or maximum. A run counts once for each condition even if it happens repeatedly.
- Ending counts against recorded runs. **View matching runs** opens the existing run list; **Replay below / Replay above** opens the exact archived command checkpoint, including command 0 for the initial state.

Samples include the initial state and every accepted command actually chosen for the recorded trace, including intermediate commands in a chosen bundle. Rejected or merely evaluated candidates do not contribute. These are observations at command boundaries, not continuous-time measurements or percentages of time. Hidden effects that occur and disappear inside one command cannot be inferred from these samples.

Labels, bounds, the resolved profile, and its provenance are stored with the job snapshot. Editing a current profile or renaming a current attribute does not relabel an old report. Existing replay reconstructs the archived run using its original content and commands.

Old jobs without observations display **Not recorded**, not zero. A missing preferred limit displays **Not configured**. No recorded runs means there is no evidence of safety or balance. Sampling cannot prove an unseen route impossible or replace designer review and human playtesting.
