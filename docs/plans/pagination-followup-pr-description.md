## Superseded

This follow-up PR draft has been folded into `TornLux/UnrealBridge#2`.
Use `docs/plans/upstream-pr-description.md` as the active PR body.

## Summary

This follow-up expands the MCP stdio wrapper's paginated tool surface for high-cardinality UnrealBridge queries and adds lightweight no-editor validation guardrails.

It adds paginated MCP tools for:

- SearchableName / GameplayTag value browsing and reverse references;
- DataTable row-name browsing and row search;
- Blueprint global function call-site and debug-print audits.

It also adds:

- a shared bridge JSON output parser for paginated handlers;
- a registry-vs-doc checker for exposed MCP tool lists;
- an all-in-one no-editor MCP smoke runner with runner self-tests;
- pagination helper failure-path tests for bad bridge JSON, invalid cursors,
  and bridge failure propagation;
- stdio client probe coverage for cancellation notifications, basic error
  responses, response envelopes, notification no-response behavior including
  invalid-params notifications, required / unknown tool-argument validation,
  clean shutdown, and tool input-schema shape;
- a lightweight GitHub Actions workflow for the no-editor MCP checks;
- exposed tool-list negative tests for missing, extra, and reordered entries;
- workflow scope checks and negative tests so the no-editor workflow stays
  limited to MCP wrapper files and MCP-specific smoke tooling.

## Historical design notes

- This was originally drafted as a stacked follow-up after `TornLux/UnrealBridge#2`.
- No Unreal-side transport is added.
- No HTTP server is added.
- The MCP wrapper still delegates to `bridge.py`.
- The tool surface remains coarse-grained: only broad list/audit queries get dedicated paginated MCP tools.
- Detail reads and write operations should continue to use `bridge_exec` or existing domain APIs.
- The no-editor workflow intentionally avoids Unreal Engine installation or `RunUAT`.

## Validation

```powershell
python tools\smoke_mcp_all.py
python tools\check_mcp_followup_scope.py --mode combined --base origin/main
git diff --check
```

`smoke_mcp_all.py` runs:

- Python compile checks for the MCP server and smoke tools;
- all-smoke runner self-checks for compile-list coverage and duplicate checks;
- MCP stdio smoke, including tool input-schema shape checks;
- stdio smoke helper tests for malformed tool schemas;
- cursor pagination smoke, including second-page slice and `MaxResults`
  script-shape checks;
- pagination helper failure-path tests for bad bridge JSON, invalid cursors,
  and bridge failure propagation;
- output spill smoke;
- common client probe fixture, including cancellation, basic error probes, and
  response envelope / notification no-response / invalid-params notification /
  required-argument / unknown-argument / clean-shutdown checks;
- exposed-tool documentation consistency check;
- exposed-tool documentation negative tests for missing, extra, and reordered
  entries;
- no-editor workflow path-filter check;
- workflow checker negative tests for missing, extra, and overly broad path
  filters;
- follow-up scope checker negative tests for forbidden `Plugin/` changes and
  unexpected files.

The branch-specific scope check was used while this work was separate from #2.

Live UE 5.7.4 smoke has also been recorded for SearchableName, DataTable, and Blueprint audit pagination in `docs/reports/`.

## Submit timing

Superseded. These changes are now intended to ship in `TornLux/UnrealBridge#2`
rather than a separate follow-up PR.
