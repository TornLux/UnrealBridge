## Summary

This PR adds a thin stdio MCP wrapper for UnrealBridge while keeping the existing `bridge.py` client and Unreal-side TCP bridge as the single source of truth.

It also:

- fixes `bridge.py preflight --json` so `errors` and `warnings` are separate arrays;
- prevents queued exec requests from running after a client-side timeout;
- adds output spill protection for oversized `exec` results;
- adds cursor-paginated MCP tools for broad asset, actor, SearchableName / GameplayTag, DataTable, and Blueprint audit queries;
- adds a shared bridge JSON output parser for paginated handlers;
- adds no-editor MCP guardrails for common stdio client probes, error responses, response envelopes, JSON-RPC notifications, clean shutdown, tool schema shape, pagination helper failures, and tool-list documentation consistency;
- handles explicit JSON-RPC `"id": null` requests as requests rather than accidentally treating them as notifications;
- rejects empty JSON-RPC batches with `-32600 Invalid Request` instead of returning no response;
- replies to notification-like methods if a client sends them with a request id, avoiding hanging clients;
- rejects requests without `jsonrpc: "2.0"` as `-32600 Invalid Request`;
- rejects requests with a missing or non-string `method` as `-32600 Invalid Request`;
- rejects non-object request `params` instead of silently treating arrays as empty objects;
- isolates unexpected tool handler failures so JSON-RPC error responses keep the original request id and batches continue;
- adds an all-in-one no-editor smoke runner and a scoped GitHub Actions workflow for review / CI handoff;
- keeps UE 5.7 `BuildPlugin -StrictIncludes` green by declaring direct dependencies and includes.

## Design notes

- No HTTP server is added.
- No second Unreal-side transport is added.
- MCP tools delegate to `bridge.py`, preserving UDP discovery, token handling, AST preflight, audit logging, and length-prefixed TCP execution.
- `UNREAL_BRIDGE_TOKEN` stays in the MCP server environment and is not exposed as a tool argument.
- The wrapper keeps a small tool surface and adds paginated coarse-grained tools only where broad result sets are common.
- Detail reads and write operations still use `bridge_exec` or existing domain APIs.

## Validation

```powershell
python tools\smoke_mcp_all.py
git diff --check
RunUAT.bat BuildPlugin -Plugin=C:\tmp\TornLux-UnrealBridge-mcp-pr-clean\Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeFinal57Loose_112427 -TargetPlatforms=Win64
RunUAT.bat BuildPlugin -Plugin=C:\tmp\TornLux-UnrealBridge-mcp-pr-clean\Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeFinal57Strict_112624 -TargetPlatforms=Win64 -StrictIncludes
```

Live validation reports are under `docs/reports/`.

The final readiness check is recorded in `docs/reports/2026-05-29_1129_pr-final-readiness.md`.
Follow-up pagination live smoke is recorded in the SearchableName, DataTable,
and Blueprint audit reports under `docs/reports/`.

## Compatibility notes

- The wrapper supports common MCP probes used by multiple clients: `initialize`, `ping`, `tools/list`, `resources/list`, `resources/templates/list`, `prompts/list`, `logging/setLevel`, JSON-RPC notifications, notification-like methods sent with request ids, JSON-RPC error responses, explicit nullable request ids, JSON-RPC version rejection, missing-method rejection, non-object params rejection, valid batches, invalid empty-batch probes, and per-request handler failure isolation.
- Tool schemas reject unknown arguments and validate required arguments before handlers run.
- UE 5.8 final validation is intentionally left out of the current blocking scope.
- `StructUtils` still emits a UE deprecation warning; this PR declares the existing dependency but does not migrate that dependency to a future replacement.
