# UnrealBridge Library Iteration — Autonomous Task (for /loop)

Each tick: pick the next library in round-robin, consider one valuable addition, (if any) implement + verify + commit, advance state. End with either a clean commit on `main` or a NO-OP with a clear log line. Never leave uncommitted changes.

## Step 1 — Pick the library

Read `scheduled/state.json`. Primary library = `libraries[next_index]`.

Libraries cycle in this order: Anim → Asset → Blueprint → DataTable → Editor → Level → Material → UMG → GameplayAbility → (wrap).

The exact list and length is authoritative in `scheduled/state.json` `libraries`; use `len(libraries)` below anywhere it says "N".

## Step 2 — Read context

1. `CLAUDE.md` — project overview + canonical sync → compile → launch → verify → close workflow
2. `Plugin/UnrealBridge/Source/UnrealBridge/Public/UnrealBridge<LIBRARY>Library.h` — current C++ surface
3. `.claude/skills/unreal-bridge/references/bridge-<lowercase>-api.md` — current docs (create if missing)
4. `.claude/skills/unreal-bridge/SKILL.md` — index table

## Step 3 — Decide what to add

Target per tick (best-effort, not strict): **~5 new C++ `UFUNCTION`s** on the primary library **plus several Python API reference entries** in the matching `bridge-<lowercase>-api.md`. Fewer is fine if the library is already saturated or the remaining gaps are low value; more is fine if cohesive. Prefer a coherent themed batch (e.g. "socket queries", "sequence metadata", "montage section ops") over five unrelated one-offs.

Candidate sources:

- **C++ gaps** — `UFUNCTION`s genuinely useful for Python/Blueprint callers and not already covered. Cross-check sibling Library headers and native UE Python API (e.g. `unreal.EditorAssetLibrary`, `unreal.EditorLevelLibrary`, `unreal.SystemLibrary`) for inspiration. Don't duplicate what UE's built-in `unreal.*` modules already expose well.
- **Doc gaps** — existing `UFUNCTION`s not yet documented, or short native-UE-Python snippets useful to callers.

Always pair new C++ functions with reference-doc entries in the same tick.

If the primary library has fewer than ~2 meaningful additions available: try `(index+1) % N`, then `(index+2) % N`, ..., up to N tries total (N = `len(libraries)`). If all libraries are exhausted of meaningful work, log `NO-OP: nothing to add this iteration` and go to Step 6 (NO-OP path).

## Step 4 — Implement

### Reference-only changes (no C++ touched)

1. Edit reference file (and `SKILL.md` if a new file was added)
2. Skip to Step 5

### C++ changes

Follow CLAUDE.md's pipeline with this retry policy:

1. **Decide loop**: body-only edit → hot reload; new `UFUNCTION`/`UCLASS`/`UPROPERTY`/struct-layout change → rebuild.
2. **Hot reload path**: `python .claude/skills/unreal-bridge/scripts/hot_reload.py` — syncs + triggers Live Coding. On `Status="NoChanges"` or `"Success"` skip to step 7.
3. **Rebuild path**: `python .claude/skills/unreal-bridge/scripts/rebuild_relaunch.py` — quits editor, syncs, runs Build.bat, relaunches, polls ping. Takes 2–5 min.
4. **If build fails**: read the captured compiler output (rebuild_relaunch streams it; hot_reload cannot — Fall through to rebuild path if you only ran hot_reload and got Failure). Edit to fix, rerun. **Max 3 attempts total** (initial + 2 retries).
5. **If still failing after 3 attempts**:
   - `git diff --name-only -- "Plugin/UnrealBridge/Source/**"` to list touched C++ files
   - `git checkout HEAD -- <those files>` to revert C++ only (keep independent reference/SKILL.md edits)
   - Rerun the relevant loop to confirm green. If still broken: `git restore .` everything, log `ROLLBACK: build failed after retries` and go to Step 6 (NO-OP path — do not commit, but DO advance state).
6. Both loops end with the bridge already back up. Confirm with `python .claude/skills/unreal-bridge/scripts/bridge.py ping`.
7. For **each new UFUNCTION**, call it via `bridge.py exec "import unreal; ..."` and check the return / no exception. On failure: treat like a compile error (fix → reload/rebuild → re-verify, up to 3 attempts; if still broken, revert C++ and abort to NO-OP path).
8. Clean shutdown (if needed): `bridge.py exec "import unreal; unreal.SystemLibrary.quit_editor()"` then verify `tasklist //FI "IMAGENAME eq UnrealEditor.exe"` shows no match.

## Step 5 — Commit

**Before staging anything, clean up `temp/`.** Mandatory on every tick that reaches commit:

1. Delete every file inside `temp/` that you created this tick (verification scripts, probe `.py` files, captured logs, intermediate dumps). Leave the `temp/` directory itself in place — don't remove it. Do not touch scratch files outside `temp/`.
2. Run `git status --porcelain` and confirm the pending changes are the intended deliverable (C++ sources, the matching `bridge-<name>-api.md`, and `scheduled/state.json`). Untracked files not from this tick are fine — leave them.
3. Stage only the deliverable paths by name — never `git add -A` / `git add .`.

- Branch must be `main`. Commit directly.
- Only commit when build is green (or for reference-only changes).
- Message: `<LIBRARY>: <one-line summary>`
- Co-author line:
  ```
  Co-Authored-By: Claude Opus 4.6 <noreply@anthropic.com>
  ```
- Do **not** push — commits stay local. The user handles pushing manually.

## Step 6 — Advance state

Update `scheduled/state.json`: set `next_index = (index_used + 1) % len(libraries)`. For NO-OP after full scan, advance by 1 from the originally-picked primary library.

Working tree must be clean by end of tick (`git status --porcelain` empty).

## Step 7 — Log

Write one line to stdout summarizing the tick:

- `COMMIT <LIBRARY>: <summary>` — made a change
- `NO-OP <LIBRARY>: nothing worth adding` — scanned, skipped
- `ROLLBACK <LIBRARY>: <reason>` — reverted, no commit

## Safety rails

- No force push, no history rewrite, no `git reset --hard` on committed history.
- Don't touch `sync_plugin.bat`, `CLAUDE.md`, or anything under `scheduled/` except `state.json`.
- If anything is ambiguous or you can't make a confident addition, prefer NO-OP over guessing.
- Do not spend more than ~20 minutes on a single tick. If approaching that, wrap up with current partial work reverted.
