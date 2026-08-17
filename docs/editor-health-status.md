# Cached editor health status

`exact_editor_status` is a read-only protocol-v2 command for diagnosing an
editor without entering the Python exec queue or dispatching new GameThread
work. It requires the frozen discovery identity, an object-valued `request`,
and an advertised `exact_editor_status` capability; legacy `editor_status`
wire requests are rejected. An endpoint that lacks this optional capability
remains discoverable and usable for the six base exact commands, while the
`status` CLI fails before opening TCP. The CLI selects and wraps the exact
command automatically:

```bash
python .claude/skills/unreal-bridge/scripts/bridge.py --json status
```

## Ownership and update flow

- The existing server `FTSTicker` callback records the latest Engine tick time.
- `FSlateApplication::OnPreTick` captures the modal attention summary on Slate's
  owning thread. This delegate continues to run in nested Slate modal loops where
  the Engine ticker is paused.
- Main-frame readiness continues to come from the module's existing
  `SetEditorReady` path.
- The cache contains only strings, numbers, and booleans. `SWindow` and `SWidget`
  pointers remain in the existing modal snapshot and never reach a TCP worker.
- A TCP worker takes one locked copy and computes ages from a monotonic clock.
  Tests inject that clock; production uses `FPlatformTime::Seconds`.
- Shutdown removes the Engine and Slate tick callbacks before resetting the
  cache. The endpoint uses the existing exact-identity preconditions and
  response echo; request-cancellation and coordinator behavior are unchanged.

## Response

The command preserves the standard protocol envelope (`success`, `output`,
`error`, `ready`) and the protocol-v2 identity echo (`protocol_version`,
`instance_id`, `pid`, `project_path`). After identity verification, the CLI
rejects a successful response unless the nested object has integer schema
version 1 and all required fields have their documented JSON types and bounded
non-negative values (tick ages also allow the `-1` never-observed sentinel).
This change introduces schema version 1 of the nested `editor_status` object:

| Field | Meaning |
|---|---|
| `schema_version` | Status schema version; currently 1. |
| `editor_ready` | Main-frame startup gate. |
| `slate_tick_sequence` / `engine_tick_sequence` | Independent owning-tick counters. |
| `last_slate_tick_utc` / `last_engine_tick_utc` | UTC time recorded by the owning callback; empty before first observation. |
| `slate_tick_age_ms` / `engine_tick_age_ms` | Monotonic age, or `-1` before first observation. |
| `slate_stale` / `engine_stale` / `stale` | Per-source and aggregate staleness against `stale_after_ms`. |
| `stale_after_ms` | Server staleness threshold; currently 2000 ms. |
| `ui_state` | `initializing`, `normal`, `slate_modal`, `debugging`, or `unavailable`. |
| `attention_id` | Increments when modal window identity appears, changes, or disappears. |
| `attention_required` | The last Slate-owned sample observed an active modal. |
| `active_modal` | Cached `present`, `title`, `first_seen_utc`, upstream `snapshot_id`, and control counts. It intentionally omits body text, values, and widget pointers. |

`ready` retains its existing meaning: the main frame was created and the startup
exec gate is open. It is intentionally separate from `stale`; old clients that
only understand `ready` keep compatible behavior.

## Diagnostic interpretation

- Engine fresh + Slate fresh + no modal: normal ready editor.
- Engine stale + Slate fresh + modal present: nested Slate modal loop needs
  attention. Use fresh `modal-status` before taking any guarded action.
- Engine stale + Slate fresh + no modal: Engine loop is blocked while Slate is
  still pumping; inspect the active work rather than queueing another exec.
- Engine stale + Slate stale: native window, deadlock, shutdown, or a generally
  unresponsive editor. `exact_editor_status` remains a cached observation, not proof
  that either owning thread can currently accept work.

The cached modal `snapshot_id` is diagnostic only. `modal_action` already
re-captures and rejects stale snapshots; callers should use `modal-status` to
review full current semantics before acting.
