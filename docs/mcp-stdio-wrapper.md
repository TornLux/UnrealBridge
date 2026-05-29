# MCP stdio wrapper

UnrealBridge includes a small stdio MCP server for agents and clients that
prefer MCP tooling over direct command-line calls:

```bash
python .claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py
```

The wrapper is intentionally thin. It does not start a second Unreal-side
server, does not add an HTTP transport, and does not duplicate bridge logic.
Each MCP tool delegates to `bridge.py`, so UDP discovery, token handling, AST
preflight, audit logging, and the length-prefixed TCP protocol remain owned by
the existing UnrealBridge client and plugin.

## Client configuration

Most stdio MCP clients use the same shape: a command, arguments, and optional
environment variables. Prefer environment variables for secrets instead of
passing tokens through tool arguments.

```json
{
  "mcpServers": {
    "unrealbridge": {
      "command": "python",
      "args": [
        "/absolute/path/to/UnrealBridge/.claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py"
      ],
      "env": {
        "UNREAL_BRIDGE_PROJECT": "optional project name or .uproject path",
        "UNREAL_BRIDGE_ENDPOINT": "optional 127.0.0.1:port",
        "UNREAL_BRIDGE_TOKEN": "optional token"
      }
    }
  }
}
```

OpenClaw stores owned MCP server definitions under `mcp.servers`. The CLI form
can store the same stdio server definition:

```bash
openclaw mcp set unrealbridge '{"command":"python","args":["/absolute/path/to/UnrealBridge/.claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py"],"env":{"UNREAL_BRIDGE_PROJECT":"optional project name or .uproject path"}}'
```

Or in an OpenClaw config file:

```json5
{
  mcp: {
    servers: {
      unrealbridge: {
        command: "python",
        args: [
          "/absolute/path/to/UnrealBridge/.claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py"
        ],
        env: {
          UNREAL_BRIDGE_PROJECT: "optional project name or .uproject path",
          UNREAL_BRIDGE_TOKEN: "optional token"
        }
      }
    }
  }
}
```

Hermes reads MCP client config from `~/.hermes/config.yaml` under
`mcp_servers`:

```yaml
mcp_servers:
  unrealbridge:
    command: "python"
    args:
      - "/absolute/path/to/UnrealBridge/.claude/skills/unreal-bridge/scripts/unrealbridge_mcp_server.py"
    env:
      UNREAL_BRIDGE_PROJECT: "optional project name or .uproject path"
      UNREAL_BRIDGE_TOKEN: "optional token"
    timeout: 120
    connect_timeout: 60
    tools:
      resources: false
      prompts: false
```

## Exposed tools

- `bridge_ping`
- `bridge_list_editors`
- `bridge_preflight`
- `bridge_exec`
- `bridge_exec_file`
- `bridge_suggest`
- `bridge_gamethread_ping`
- `bridge_resume`
- `bridge_wait_compile`
- `bridge_wait_pose_index`
- `bridge_search_assets_page`
- `bridge_list_actors_page`
- `bridge_searchable_name_values_page`
- `bridge_assets_referencing_searchable_name_page`
- `bridge_datatable_row_names_page`
- `bridge_datatable_search_rows_page`
- `bridge_blueprint_call_sites_page`
- `bridge_blueprint_debug_prints_page`

All tools return MCP text content containing JSON. Failed bridge invocations
set `isError: true` and include the bridge exit code, parsed stdout, and stderr
when available.

## Output limits

The wrapper sets `UNREAL_BRIDGE_MAX_OUTPUT_BYTES=262144` for delegated
`bridge.py` calls unless the environment already provides a value. When an
`exec` response has an `output` or `error` field larger than the limit,
`bridge.py` writes the full field to a spill file and returns an inline preview
with a `spills` metadata object:

```json
{
  "success": true,
  "output": "preview...\n\n[UnrealBridge truncated output: wrote full text to ...]",
  "spills": {
    "output": {
      "path": "C:/Project/Saved/UnrealBridge/spills/...",
      "bytes": 1048576,
      "shown_bytes": 262144,
      "truncated": true
    }
  }
}
```

Useful knobs:

- `UNREAL_BRIDGE_MAX_OUTPUT_BYTES` or tool argument `max_output_bytes`: maximum
  UTF-8 bytes kept inline for `bridge.py` `output` / `error` fields. Use `0`
  to disable.
- `UNREAL_BRIDGE_SPILL_DIR` or tool argument `spill_dir`: directory for full
  spill files.
- `UNREAL_BRIDGE_MCP_MAX_CONTENT_BYTES`: final MCP text-content envelope limit
  for the stdio wrapper. Defaults to `393216` bytes. Use `0` to disable.

## Cursor pagination

Use paginated tools for broad queries instead of dumping whole result sets:

- `bridge_search_assets_page`: paginated wrapper around UnrealBridge asset
  search. Returns asset object path, package path, and asset name.
- `bridge_list_actors_page`: paginated actor briefs for populated levels.
- `bridge_searchable_name_values_page`: paginated SearchableName values, such
  as used `GameplayTag` values in the AssetRegistry.
- `bridge_assets_referencing_searchable_name_page`: paginated assets that
  reference one SearchableName value.
- `bridge_datatable_row_names_page`: paginated DataTable row names.
- `bridge_datatable_search_rows_page`: paginated DataTable row search hits.
- `bridge_blueprint_call_sites_page`: paginated cross-Blueprint function call
  sites for refactor and usage audits.
- `bridge_blueprint_debug_prints_page`: paginated Blueprint debug-print audit
  hits.

Paginated tools accept `page_size` and `cursor`. The first call omits `cursor`; if
`page.has_more` is true, pass `page.next_cursor` to fetch the next page. Cursors
are opaque and tied to the original query parameters. Reusing a cursor with a
different query returns a structured `STALE_CURSOR` error.

## Extension model

Add new bridge commands in one place:

1. Add a handler function that builds the `bridge.py` argument list.
2. Add a `ToolSpec` entry with the MCP schema and handler.
3. Add a smoke assertion if the tool can be tested without a running editor.

Keep the wrapper client-neutral. Do not add client-specific notifications or
alternate transports here unless they are optional and standard MCP clients can
ignore them safely.

## Protocol compatibility notes

The wrapper speaks JSON-RPC over stdio, keeps stdout reserved for MCP messages,
and supports both single JSON-RPC messages and batches after initialization.
It also handles common client probes such as `ping`, `resources/list`,
`resources/templates/list`, `prompts/list`, `logging/setLevel`, and
cancellation notifications such as `notifications/cancelled` and
`$/cancelRequest`. JSON-RPC messages without an `id` are treated as
notifications, even if their `params` shape is invalid: the wrapper does not
send a response and does not execute tool calls for them.

The wrapper does not expose `UNREAL_BRIDGE_TOKEN` as a tool argument. Supply it
through the MCP server environment so the model does not need to see or repeat
the token.

## Smoke test

Run all no-editor MCP checks from the repository root:

```bash
python tools/smoke_mcp_all.py
```

Or run the individual checks:

```bash
python tools/smoke_mcp_stdio.py
python tools/smoke_output_spill.py
python tools/smoke_mcp_pagination.py
python tools/run_mcp_stdio_fixture.py
python tools/check_mcp_tool_docs.py
python tools/test_check_mcp_tool_docs.py
python tools/check_mcp_workflow.py
python tools/test_check_mcp_workflow.py
python tools/test_check_mcp_followup_scope.py
python tools/test_smoke_mcp_all.py
python tools/test_smoke_mcp_pagination.py
python tools/test_smoke_mcp_stdio.py
```

The all-in-one runner also compiles the MCP scripts and smoke tools before
running the checks. The stdio smoke starts the server, initializes MCP, lists
tools, checks tool input-schema shape, and verifies `bridge_preflight` without
contacting Unreal. The output
spill smoke verifies field-level and MCP-envelope spill behavior without
requiring an editor. The fixture runner replays common client probes from
`tools/fixtures/mcp_stdio_common_probes.json`, including `ping`,
`resources/list`, `resources/templates/list`, `prompts/list`,
`logging/setLevel`, cancellation notifications, basic error responses, response
id / `jsonrpc` envelope checks, notification no-response behavior including
invalid-params notifications, required and unknown tool-argument validation,
clean process shutdown, empty stderr, and JSON-RPC batch handling.
The doc checker imports the server tool registry and verifies that the exposed
tool lists in this document and the UnrealBridge skill stay in sync.
The doc checker tests cover missing, extra, and reordered MCP tool entries.
The workflow checker verifies that the GitHub Actions path filters stay scoped
to MCP wrapper files and no-editor smoke tooling.
The workflow checker tests cover missing, extra, and overly broad path filters.
The all-smoke runner tests make sure each executed script has a `py_compile`
entry and that no check names or executed scripts are duplicated.
The pagination helper tests cover bad bridge JSON output, invalid cursors, and
bridge failure propagation for paginated handlers.
The stdio smoke helper tests cover negative `tools/list` schema cases such as
duplicate names, missing descriptions, open-ended properties, and invalid
required fields.
The follow-up scope checker tests cover forbidden `Plugin/` changes and
unexpected files. The actual branch scope check is base-dependent and is only
needed when preparing future stacked follow-up branches, for example:

```bash
python tools/check_mcp_followup_scope.py --base codex/mcp-stdio-wrapper-pr
```

The same no-editor suite is wired into
`.github/workflows/mcp-no-editor.yml` for pull requests and pushes that touch
the MCP wrapper, its docs, or the MCP smoke/checker tooling.
