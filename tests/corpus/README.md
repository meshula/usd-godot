# usd-godot fidelity corpus

A behavior-level regression corpus **harvested verbatim from
[godot-usd](https://github.com/migueldeicaza)** (MIT, © 2026 Miguel de Icaza) and used as the
fidelity acceptance oracle for usd-godot's USD ↔ Godot translation work.

See `USD_GODOT_INTEGRATION_PLAN.md` (repo `crucible/godot/`) — this is **Phase 0.1**.

## What this is

`godot-usd` is an unmaintained single-author experiment, but its **test suite and sample assets are
executable specifications** of correct USD import/export (meshes, materials, subsets, skel,
blendshapes, curves, lights, variants, composition, and the non-destructive source-preserving save —
down to exact warning strings). We keep the knowledge, not the project: these tests are transplanted
here to drive and measure the fidelity work described in the plan. `godot-usd` itself is archived.

- `samples/` — 44 USD sample assets (`.usda/.usdc/.usdz` + one `.svg` texture source), copied
  verbatim. Godot's regenerable `.import` sidecars were intentionally **not** copied; Godot recreates
  them once usd-godot registers a USD importer.
- `tests/*.gd` — 53 standalone `SceneTree` regression scripts, copied verbatim (the oracle).
- `tests/run_all.gd` — godot-usd's original hard-fail harness, kept verbatim for reference.
- `tests/run_corpus.gd` — **scoreboard** harness (added here). Classifies each test and fails CI only
  on regressions, so the corpus is committable while most features are still unimplemented.
- `run_corpus.sh` — driver: stages the built usd-godot extension into this project and runs the
  scoreboard.

## Running

```bash
# from the usd-godot repo root, with a built extension in ./lib (or ./bin):
GODOT=/path/to/godot ./tests/corpus/run_corpus.sh
```

Most tests **fail today** — that is expected. Each depends on capabilities usd-godot has not yet
transplanted (starting with a `ResourceFormatLoader` that returns a `PackedScene` from a USD path).

## Scoreboard semantics

`run_corpus.gd` maintains a `MUST_PASS` set (empty at the Phase 0 baseline):

| Result | Meaning |
|---|---|
| **PASS** | passed, and in `MUST_PASS` — good |
| **XFAIL** | failed, not in `MUST_PASS` — expected (feature not implemented yet) |
| **UNEXPECTED-PASS** | passed, not in `MUST_PASS` — a feature landed; promote it into `MUST_PASS` |
| **REGRESSION** | failed, but in `MUST_PASS` — broke something → exit 1 |

**Workflow:** as each transplant phase lands a feature, move the now-passing test name(s) into
`MUST_PASS`. The "passing (fidelity): N / 53" line is the metric tracked across the whole migration.

### Pass criterion (why exit code 0 is not enough)

Godot's `--script` path **exits 0 even when a script fails to compile** (e.g. it references a class
usd-godot does not define yet, such as `UsdStageInstance`). So a test counts as passed only if it
exits 0 **and** its output contains no "did-not-actually-run" marker (`Failed to load script`,
`Program crashed`). Without this, ~7 variant tests falsely reported PASS at the Phase-0 baseline
because they never compiled. The honest baseline is **0/53**.
