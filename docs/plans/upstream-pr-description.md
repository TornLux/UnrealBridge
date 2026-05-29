## Summary

This PR adds a thin stdio MCP wrapper for UnrealBridge. The wrapper delegates to
the existing `bridge.py` workflow, so the established discovery, preflight,
audit logging, token handling, and editor execution path remain the source of
truth.

It also includes a small set of reliability and review-supporting updates:

- safer `bridge.py preflight --json` output, with `errors` and `warnings`
  reported separately;
- cancellation of queued editor exec work after a client-side timeout;
- output spill protection for oversized `exec` results;
- cursor-paginated MCP tools for broad asset, actor, SearchableName /
  GameplayTag, DataTable, and Blueprint audit queries;
- JSON-RPC compatibility coverage for common MCP client probes and error paths;
- `bridge.py mcp-config` snippets for common MCP client config shapes, with
  no-editor tests covering the emitted JSON, TOML, and YAML;
- no-editor smoke tests and scoped workflow guardrails for future changes.

## Design

- MCP tools delegate to `bridge.py` rather than duplicating UnrealBridge logic.
- Secrets stay in the MCP server environment and are not exposed as tool
  arguments.
- The exposed MCP surface stays intentionally small: broad browsing queries get
  paginated tools, while detail reads and write operations continue to use
  `bridge_exec` or the existing UnrealBridge APIs.
- Client probe behavior is covered by fixture-driven no-editor tests so future
  schema changes are easy to review. The generated client config snippets are
  shape-tested without launching those external clients.

## Validation

```powershell
python tools\smoke_mcp_all.py
python tools\check_mcp_followup_scope.py --mode combined --base origin/main
git diff --check origin/main..HEAD
RunUAT.bat BuildPlugin -Plugin=C:\tmp\TornLux-UnrealBridge-mcp-pr-clean\Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeFinal57Loose_112427 -TargetPlatforms=Win64
RunUAT.bat BuildPlugin -Plugin=C:\tmp\TornLux-UnrealBridge-mcp-pr-clean\Plugin\UnrealBridge\UnrealBridge.uplugin -Package=C:\tmp\UnrealBridgeFinal57Strict_112624 -TargetPlatforms=Win64 -StrictIncludes
```

Live validation reports and the final readiness record are under
`docs/reports/`.
