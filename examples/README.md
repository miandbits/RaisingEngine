# Public sample candidates

These projects are independent of locally installed reference games. No extracted PM2 or Magical Princess media is required. Engine-wide license selection and generated-asset/dependency notice review are still pending; this directory classification is not a license grant.

| Project | Purpose |
|---|---|
| `academy-demo` | Minimal two-month data/runtime example |
| `academy-year` | Twelve-month content example |
| `academy-visual` | Basic visual editing example |
| `academy-journey` | Screens, city, shop, gifts and relationships |
| `academy-adventure` | Exploration and gameplay systems |
| `genre-suite` | Six short genre templates |
| `starlight-three-months` | Three-month integrated pilot with recorded ending routes |

Copy a sample into a new `projects/<your-game>` folder through the editor before modifying it. `projects/` is ignored and belongs to the user. The old local copies remain in place; opening this directory does not migrate or overwrite their saves or editor state.

Focused regression projects live under `tests/fixtures/projects/`. The source manifest and known excluded-asset hashes are in `config/repository-content.json`.
