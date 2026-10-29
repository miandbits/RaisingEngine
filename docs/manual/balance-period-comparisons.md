# Period trends and before/after balancing

In **Automatic play → Balance**, enable **Sample resource/condition observations** before starting a run. Player-priority strategies enable the same observations automatically. Open **Period trends · compare**, choose an attribute by its authored name, and choose a saved **Before report**. The current report is the one selected in the main analysis history. Newly completed jobs become available without restarting the editor.

The blue curve is the current report; the dashed amber curve is the earlier report. Dots show the mean, vertical bars show the observed minimum and maximum, and the table gives exact values and sample counts. Full content fingerprints, snapshot hashes and settings are available on the identity label's tooltip. Short report identities stay visible above the graph. Open **Report details** for the stored seed, strategy, priorities, requested/observed run counts and limits. The page scrolls in shorter editor windows.

## What a point means

A run contributes **one value: its last accepted state in that sampled period**. Initial states count when no command was accepted. Unchosen planning candidates never contribute. Several effects in one period do not give that run extra weight. The existing resource summary still records intermediate lows, highs and band breaches; a recovered resource can therefore have a low summary minimum and a higher period point.

A period is marked complete after the run moves to a later period or finishes the game. A run stopped by a command limit, time limit, cancellation or blockage can have a partial final point. Counts of observed and complete periods, and terminations at that point, are shown separately. Terminations outside the sampled grid remain in the overall termination totals; they are not moved onto a nearby period.

No value is carried forward after a run stops. A later mean may describe fewer runs and a different surviving subset. Read the counts before interpreting a rising or falling line. A missing value is **Not recorded**, not zero. Connected dots summarize sampled observations and do not predict the periods between them.

Each report stores at most 256 fixed-grid period points and the first 64 attributes in stable ID order. Longer games use a larger period stride; omitted attributes and observed-versus-recorded run-period counts are disclosed. The report stores the calendar axis and sampling method so the UI never quietly substitutes the current project's calendar. Period numbers are one-based for display and refer to the engine's observed global period index; if an engine completion state advances beyond the last playable period, that terminal observation remains explicitly numbered.

## A repeatable balancing workflow

1. Save a player-priority profile and choose a seed, run count and budgets. Run an initial sample and keep its report.
2. Change the relevant activity, cost or rule in the editor. A new analysis can use the current unsaved draft; running it does not save or alter that draft.
3. Keep the same seed, profile and budgets for the second analysis. Select the first report as **Before report** and inspect the relevant resource and condition curves.
4. Check observed/complete counts and termination reasons alongside the numeric differences. Replay a recorded run or a summary breach witness to investigate the actual sequence.

Reports use their original immutable content snapshots and labels, including after later project edits. The comparison reads and verifies those snapshots without replaying commands or writing to the game, save slots or reports. It does not migrate saved games.

## Comparison boundaries

**Same analysis settings** means the stored seed, strategy, profile values and run configuration match. Content and snapshot changes are still disclosed: that is the normal before/after balancing case. A renamed profile with identical values is the same policy; a changed weight or band is a changed setup.

**Analysis settings changed** keeps descriptive means visible and lists the changed settings. Different workers, seed, requested runs, focus, priorities or budgets can confound a comparison. Even with the same seed, changed rules can cause different choices and random draws. The displayed difference is not a paired causal estimate.

**Not comparable** suppresses differences when the recorded measurement/engine version, calendar or sampling grid changes, or when a report is an active job or a target search rather than a run cohort. An attribute whose ID, kind or declared bounds changed has no numeric comparison. Matching IDs are assumed to represent the same authored quantity; the engine cannot infer a semantic unit change hidden behind an unchanged ID.

Older reports without period observations remain available for their original analysis and replay. They display **Not recorded** here. Re-run the same saved setup to collect a new series; no historical zeros or curves are invented.

These are bounded samples of the chosen automated strategies. They can reveal resource pressure and progression differences, but do not prove reachability of every ending, optimal play, fairness, enjoyment or commercial readiness. Use them alongside deliberate route tests and human playtests.
