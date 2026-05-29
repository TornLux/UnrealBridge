# MCP stdio wrapper roadmap

This roadmap tracks follow-up work for the UnrealBridge MCP stdio wrapper.

## Current TODO

- [x] MCP stdio no-editor smoke passes.
- [x] `bridge.py preflight --json` reports `errors` and `warnings` as separate
  arrays.
- [x] Live MCP stdio calls can reach UnrealBridge discovery, ping, and exec.
- [x] Sync the exec timeout cancellation change into the `TEST_57` project
  plugin source.
- [x] Verify the synced timeout cancellation source with UE 5.7 `BuildPlugin`
  using a temporary package output.
- [x] Live validation: rebuild, restart Unreal Editor, and confirm a timed-out
  queued exec does not run later.
- [x] Add output cap / spill-to-file protection for `bridge.py exec` and MCP
  text content.
- [x] Add cursor pagination infrastructure for MCP asset search and actor list
  tools.
- [x] Clean up UE 5.7 StrictIncludes failures and declare the `StructUtils`
  plugin dependency explicitly.
- [x] Add a data-driven no-editor MCP stdio fixture for common client probes.
- [x] Prepare an upstream-friendly commit / PR split plan.
- [x] Prepare a reusable upstream PR description draft.
- [x] Move additional high-cardinality pagination work into
  `codex/mcp-pagination-followups` for staging.
- [x] Fold the pagination follow-up work into `TornLux/UnrealBridge#2` as the
  final combined PR scope.
- [x] Keep `codex/mcp-pagination-followups` pushed to the fork as a backup
  branch pointing at the same final commit.
- [x] Add a registry-vs-doc checker for the MCP exposed tool lists.
- [x] Add an all-in-one no-editor MCP smoke runner for review and CI handoff.
- [x] Add a GitHub Actions workflow for the no-editor MCP smoke runner.

## P0 - Current PR scope

- Add a client-neutral stdio MCP wrapper that delegates to `bridge.py`.
- Keep the Unreal-side service unchanged: no HTTP server, no second plugin, no
  duplicated command transport.
- Keep token handling in the server environment instead of MCP tool arguments.
- Add no-editor smoke coverage for initialize, `tools/list`, `bridge_preflight`,
  `ping`, resources, prompts, and JSON-RPC batches.
- Fix `bridge.py preflight --json` so warnings and errors are reported as
  separate arrays.
- Ensure queued exec requests that time out before GameThread execution are
  cancelled instead of being executed later.
- Keep oversized `exec` output out of MCP client context by spilling full text
  to a file and returning an inline preview plus the spill path.
- Provide cursor-paginated MCP tools for broad asset, actor, SearchableName /
  GameplayTag, DataTable, and Blueprint audit queries.

## P1 - Upstream hygiene candidates

- Use `docs/plans/upstream-pr-split-plan.md` as the current review and submit
  order for the preflight fix, MCP wrapper, timeout cancellation, output spill,
  pagination, and StrictIncludes changes.
- Use `docs/plans/upstream-pr-description.md` as the current PR body draft.
- Keep UE 5.7 `RunUAT BuildPlugin -StrictIncludes` green when adding new
  bridge-side code.
- Keep the no-editor MCP fixture current when new client probes or compatibility
  expectations are discovered.
- Keep `python tools/check_mcp_tool_docs.py` green when adding or renaming MCP
  tools.
- Keep `python tools/test_check_mcp_tool_docs.py` green so missing, extra, and
  reordered tool-list docs stay covered by negative tests.
- Prefer `python tools/smoke_mcp_all.py` for no-editor MCP validation before
  pushing follow-up changes.
- Keep `.github/workflows/mcp-no-editor.yml` limited to no-editor checks so it
  remains usable without an installed Unreal Engine, and keep its path filters
  scoped to MCP wrapper files and MCP-specific smoke tooling.
- Keep `python tools/check_mcp_workflow.py` green when changing the no-editor
  workflow trigger scope.
- Keep `python tools/test_check_mcp_workflow.py` green so missing, extra, and
  overly broad workflow path filters stay covered by negative tests.
- Keep the folded high-cardinality pagination coverage documented in
  `docs/plans/mcp-pagination-followups.md`.

## P2 - Low priority compatibility validation

- Run the same BuildPlugin validation on UE 5.8 final and record the result.
- Re-check MCP behavior against new OpenClaw and Hermes client releases if their
  config schemas or probe methods change.
- Consider protocol-version expansion only after a client needs it; keep the
  wrapper conservative until then.
