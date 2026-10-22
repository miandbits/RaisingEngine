# Roster display order

The web/React activity assignment UI now follows `project.roster.members` order,
matching the native player. It previously used assignment object key order, which
could change when the native runtime serialized a save.

`PlayerView.rosterCharacterIds` carries presentation order only. Simulation order,
commands, save schema and random state are unchanged. Older hosts without this
metadata retain their existing assignment order; missing or duplicate metadata
cannot hide an assignment.

Validation: browser regression covers reversed save-key order, reload, target
selection, and older hosts. Actor-story controller tests verify metadata delivery
alongside personal event/choice/save replay checks.
