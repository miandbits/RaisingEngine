# Public templates and samples

These projects are independent of locally installed reference games. Engine code is MIT licensed; media provenance and dependency redistribution review remain separate. See [asset boundaries](../docs/development/public-assets.md).

| Project | Purpose |
|---|---|
| `academy-demo` | Minimal two-month data/runtime example |
| `academy-year` | Twelve-month content example |
| `academy-visual` | Basic visual editing example |
| `academy-journey` | Screens, city, shop, gifts and relationships |
| `academy-adventure` | Exploration and gameplay systems |
| `genre-suite` | Six short genre templates |
| `starlight-three-months` | Three-month integrated pilot with recorded ending routes |
| `rule-extension-lab` | Portable designer-authored rule package and sample game |

Copy a sample into a new `projects/<your-game>` folder through the editor before modifying it. `projects/` is ignored and belongs to the user. The old local copies remain in place; opening this directory does not migrate or overwrite their saves or editor state.

Focused experiments, including the wuxia creator trials, live under `tests/fixtures/projects/`. Their original local copies remain preserved. The source manifest and known excluded-asset hashes are in `config/repository-content.json`.
