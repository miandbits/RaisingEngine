# Authoring feedback and manifest errors

The display-data search now includes actual choice results, reading status, player
notices, activity progress, the draft schedule and save summaries. These use the
same labels as the inspector binding selector. Numeric gauges still accept only
numeric bindings. Choice results are reconstructed from successful commands and
do not apply their effects a second time.

When the project manifest fails structural validation, the validator reports its
format errors and explicitly defers cross-document semantic checks. It continues
structural checks for the other files. Previously, removing an invalid manifest
from the validated documents made it look like a legacy project with no roster,
which could produce many misleading character-support errors. Once the manifest
is repaired, normal reference, range and rule checks resume. Errors appear before
warnings; locations and deterministic ordering within each severity remain intact.

Verification includes the searchable choice-result binding through the Qt dialog
and save/reopen, invalid-manifest/sibling validation, and restoration of genuine
character-state errors after repairing the manifest.
