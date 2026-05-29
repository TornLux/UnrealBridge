#!/usr/bin/env python3
"""
UnrealBridge MCP stdio server.

This is a thin MCP wrapper around the existing bridge.py TCP client. It does
not add another Unreal-side plugin or transport; MCP tools call bridge.py,
which keeps UnrealBridge discovery, token handling, AST preflight, audit
logging, and length-prefixed TCP execution as the single source of truth.
"""

from __future__ import annotations

import json
import os
import subprocess
import sys
import tempfile
import datetime
import base64
import hashlib
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable


SCRIPT_DIR = Path(__file__).resolve().parent
BRIDGE_PY = SCRIPT_DIR / "bridge.py"
SERVER_NAME = "unrealbridge-mcp"
SERVER_VERSION = "0.1.0"
SUPPORTED_PROTOCOLS = ("2024-11-05", "2025-03-26")
DEFAULT_PROTOCOL = SUPPORTED_PROTOCOLS[-1]
DEFAULT_BRIDGE_MAX_OUTPUT_BYTES = 256 * 1024
DEFAULT_MCP_MAX_CONTENT_BYTES = 384 * 1024
DEFAULT_PAGE_SIZE = 50
MAX_PAGE_SIZE = 200

JsonObject = dict[str, Any]
ToolHandler = Callable[[JsonObject], JsonObject]


COMMON_PROPS: JsonObject = {
    "project": {
        "type": "string",
        "description": "Optional Unreal project name or .uproject path used to disambiguate multiple editors.",
    },
    "endpoint": {
        "type": "string",
        "description": "Optional host:port endpoint. Skips UDP discovery.",
    },
    "timeout": {
        "type": "number",
        "description": "Per-request timeout in seconds.",
    },
    "discovery_timeout": {
        "type": "integer",
        "description": "UDP discovery wait window in milliseconds.",
    },
    "discovery_group": {
        "type": "string",
        "description": "UDP discovery group as host:port.",
    },
    "max_output_bytes": {
        "type": "integer",
        "description": "Maximum UTF-8 bytes kept inline for bridge output/error fields. Larger fields spill to disk.",
    },
    "spill_dir": {
        "type": "string",
        "description": "Directory for oversized output spill files.",
    },
    "max_mcp_content_bytes": {
        "type": "integer",
        "description": "Maximum UTF-8 bytes kept inline in the MCP text content envelope. Use 0 to disable.",
    },
}


@dataclass(frozen=True)
class ToolSpec:
    name: str
    description: str
    properties: JsonObject
    handler: ToolHandler
    required: tuple[str, ...] = ()

    def as_mcp_tool(self) -> JsonObject:
        return {
            "name": self.name,
            "description": self.description,
            "inputSchema": {
                "type": "object",
                "properties": self.properties,
                "required": list(self.required),
                "additionalProperties": False,
            },
        }


def _as_text(value: Any) -> str:
    if isinstance(value, str):
        return value
    return json.dumps(value, ensure_ascii=False, indent=2)


def _parse_nonnegative_int(value: str | None, default: int) -> int:
    if value is None or value == "":
        return default
    try:
        return max(int(value), 0)
    except (TypeError, ValueError):
        return default


def _mcp_content_limit(args: JsonObject | None = None) -> int:
    if args and args.get("max_mcp_content_bytes") is not None:
        return max(int(args["max_mcp_content_bytes"]), 0)
    return _parse_nonnegative_int(os.environ.get("UNREAL_BRIDGE_MCP_MAX_CONTENT_BYTES"),
                                  DEFAULT_MCP_MAX_CONTENT_BYTES)


def _mcp_spill_dir(args: JsonObject | None = None) -> str:
    explicit = None
    if args:
        explicit = args.get("spill_dir")
    explicit = explicit or os.environ.get("UNREAL_BRIDGE_MCP_SPILL_DIR") or os.environ.get("UNREAL_BRIDGE_SPILL_DIR")
    if explicit:
        return str(Path(str(explicit)).expanduser().resolve())
    return str(Path(tempfile.gettempdir()) / "UnrealBridge" / "mcp-spills")


def _truncate_utf8(text: str, max_bytes: int) -> str:
    return text.encode("utf-8")[:max_bytes].decode("utf-8", errors="replace")


def _spill_mcp_content(text: str, args: JsonObject | None = None) -> str:
    max_bytes = _mcp_content_limit(args)
    if max_bytes <= 0 or len(text.encode("utf-8")) <= max_bytes:
        return text

    out_dir = Path(_mcp_spill_dir(args))
    out_dir.mkdir(parents=True, exist_ok=True)
    stamp = datetime.datetime.now(datetime.timezone.utc).strftime("%Y%m%dT%H%M%S%fZ")
    path = out_dir / f"{stamp}_{os.getpid()}_mcp_content.txt"
    path.write_text(text, encoding="utf-8", newline="")
    return (
        f"{_truncate_utf8(text, max_bytes)}\n\n"
        f"[UnrealBridge MCP content truncated: wrote full text to {path} "
        f"({len(text.encode('utf-8'))} bytes; showing first {max_bytes} bytes)]"
    )


def _content(value: Any, is_error: bool = False, args: JsonObject | None = None) -> JsonObject:
    text = _spill_mcp_content(_as_text(value), args)
    result: JsonObject = {"content": [{"type": "text", "text": text}]}
    if is_error:
        result["isError"] = True
    return result


def _bridge_common_args(args: JsonObject) -> list[str]:
    out = ["--json"]
    if args.get("timeout") is not None:
        out += ["--timeout", str(args["timeout"])]
    if args.get("endpoint"):
        out += ["--endpoint", str(args["endpoint"])]
    if args.get("project"):
        out += ["--project", str(args["project"])]
    if args.get("discovery_timeout") is not None:
        out += ["--discovery-timeout", str(args["discovery_timeout"])]
    if args.get("discovery_group"):
        out += ["--discovery-group", str(args["discovery_group"])]
    if args.get("max_output_bytes") is not None:
        out += ["--max-output-bytes", str(args["max_output_bytes"])]
    if args.get("spill_dir"):
        out += ["--spill-dir", str(args["spill_dir"])]
    return out


def _bridge_process_timeout(args: JsonObject) -> float:
    request_timeout = float(args.get("timeout") or 30)
    if args.get("wait_timeout") is not None:
        request_timeout = max(request_timeout, float(args["wait_timeout"]))
    return request_timeout + 10


def _run_bridge(args: JsonObject, command: list[str], stdin: str | None = None) -> JsonObject:
    env = os.environ.copy()
    env.setdefault("PYTHONIOENCODING", "utf-8")
    env.setdefault("UNREAL_BRIDGE_MAX_OUTPUT_BYTES", str(DEFAULT_BRIDGE_MAX_OUTPUT_BYTES))
    proc = subprocess.run(
        [sys.executable, str(BRIDGE_PY), *_bridge_common_args(args), *command],
        input=stdin,
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        env=env,
        timeout=_bridge_process_timeout(args),
    )

    stdout = proc.stdout.strip()
    parsed: Any = None
    if stdout:
        try:
            parsed = json.loads(stdout)
        except json.JSONDecodeError:
            parsed = stdout

    payload: JsonObject = {
        "exit_code": proc.returncode,
        "ok": proc.returncode == 0,
        "result": parsed,
    }
    stderr = proc.stderr.strip()
    if stderr:
        payload["stderr"] = stderr
    return payload


def _bridge_result(args: JsonObject, command: list[str], stdin: str | None = None) -> JsonObject:
    try:
        payload = _run_bridge(args, command, stdin)
    except subprocess.TimeoutExpired as exc:
        return _content({"ok": False, "error": f"bridge.py timed out after {exc.timeout}s"}, True, args)
    except Exception as exc:
        return _content({"ok": False, "error": str(exc)}, True, args)
    return _content(payload, is_error=not payload.get("ok"), args=args)


def _stable_json(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def _params_hash(params: JsonObject) -> str:
    return hashlib.sha256(_stable_json(params).encode("utf-8")).hexdigest()[:16]


def _encode_cursor(kind: str, params: JsonObject, offset: int) -> str:
    payload = {"v": 1, "kind": kind, "params_hash": _params_hash(params), "offset": max(offset, 0)}
    raw = _stable_json(payload).encode("utf-8")
    return base64.urlsafe_b64encode(raw).decode("ascii").rstrip("=")


def _decode_cursor(cursor: str) -> JsonObject:
    padded = cursor + ("=" * (-len(cursor) % 4))
    try:
        payload = json.loads(base64.urlsafe_b64decode(padded.encode("ascii")).decode("utf-8"))
    except Exception as exc:
        raise ValueError(f"INVALID_CURSOR: {exc}") from exc
    if not isinstance(payload, dict) or payload.get("v") != 1:
        raise ValueError("INVALID_CURSOR: unsupported cursor payload")
    return payload


def _page_request(kind: str, args: JsonObject, params: JsonObject) -> tuple[int, int]:
    page_size = int(args.get("page_size") or DEFAULT_PAGE_SIZE)
    page_size = max(1, min(page_size, MAX_PAGE_SIZE))
    cursor = args.get("cursor")
    if not cursor:
        return 0, page_size

    payload = _decode_cursor(str(cursor))
    if payload.get("kind") != kind or payload.get("params_hash") != _params_hash(params):
        raise ValueError("STALE_CURSOR: cursor does not match the current query parameters")
    return max(int(payload.get("offset") or 0), 0), page_size


def _page_envelope(kind: str, params: JsonObject, offset: int, page_size: int, items: list[Any], has_more: bool) -> JsonObject:
    next_cursor = _encode_cursor(kind, params, offset + len(items)) if has_more else ""
    return {
        "ok": True,
        "items": items,
        "page": {
            "page_size": page_size,
            "offset": offset,
            "returned": len(items),
            "has_more": has_more,
            "next_cursor": next_cursor,
        },
    }


def _handle_page_error(args: JsonObject, exc: Exception) -> JsonObject:
    message = str(exc)
    code = message.split(":", 1)[0] if ":" in message else "PAGE_ERROR"
    return _content({"ok": False, "code": code, "error": message}, True, args=args)


def _parse_bridge_json_output(payload: JsonObject) -> JsonObject:
    output = ((payload.get("result") or {}).get("output") or "").strip().splitlines()
    if not output:
        raise ValueError("empty bridge output")
    data = json.loads(output[-1])
    if not isinstance(data, dict):
        raise ValueError("bridge output JSON must be an object")
    return data


def _handle_ping(args: JsonObject) -> JsonObject:
    return _bridge_result(args, ["ping"])


def _handle_list_editors(args: JsonObject) -> JsonObject:
    return _bridge_result(args, ["list-editors"])


def _handle_preflight(args: JsonObject) -> JsonObject:
    code = args.get("code")
    file = args.get("file")
    if bool(code) == bool(file):
        return _content({"ok": False, "error": "provide exactly one of code or file"}, True)
    if code:
        return _bridge_result({}, ["preflight", "-"], stdin=str(code))
    return _bridge_result({}, ["preflight", str(file)])


def _handle_exec(args: JsonObject) -> JsonObject:
    command = ["exec", "--stdin"]
    if args.get("no_preflight"):
        command.insert(0, "--no-preflight")
    return _bridge_result(args, command, stdin=str(args["code"]))


def _handle_exec_file(args: JsonObject) -> JsonObject:
    command = ["exec-file", str(args["file"])]
    if args.get("no_preflight"):
        command.insert(0, "--no-preflight")
    return _bridge_result(args, command)


def _handle_suggest(args: JsonObject) -> JsonObject:
    command = ["suggest"]
    if args.get("pattern"):
        command.append(str(args["pattern"]))
    return _bridge_result({}, command)


def _handle_gamethread_ping(args: JsonObject) -> JsonObject:
    return _bridge_result(args, ["gamethread-ping"])


def _handle_resume(args: JsonObject) -> JsonObject:
    return _bridge_result(args, ["resume"])


def _handle_wait_compile(args: JsonObject) -> JsonObject:
    command = ["wait-compile", str(args["material_path"])]
    if args.get("wait_timeout") is not None:
        command += ["--wait-timeout", str(args["wait_timeout"])]
    if args.get("poll_interval") is not None:
        command += ["--poll-interval", str(args["poll_interval"])]
    if args.get("feature_level"):
        command += ["--feature-level", str(args["feature_level"])]
    if args.get("quality"):
        command += ["--quality", str(args["quality"])]
    return _bridge_result(args, command)


def _handle_wait_pose_index(args: JsonObject) -> JsonObject:
    command = ["wait-pose-index", str(args["database_path"])]
    if args.get("wait_timeout") is not None:
        command += ["--wait-timeout", str(args["wait_timeout"])]
    if args.get("poll_interval") is not None:
        command += ["--poll-interval", str(args["poll_interval"])]
    return _bridge_result(args, command)


def _handle_search_assets_page(args: JsonObject) -> JsonObject:
    params = {
        "query": str(args.get("query") or ""),
        "scope": str(args.get("scope") or "ALL_ASSETS").upper(),
        "class_filter": str(args.get("class_filter") or ""),
        "case_sensitive": bool(args.get("case_sensitive") or False),
        "whole_word": bool(args.get("whole_word") or False),
        "min_characters": int(args.get("min_characters") or 1),
        "custom_package_path": str(args.get("custom_package_path") or ""),
    }
    try:
        offset, page_size = _page_request("asset_search", args, params)
    except Exception as exc:
        return _handle_page_error(args, exc)

    max_results = offset + page_size + 1
    script = f"""
import json
import unreal

scope_name = {params['scope']!r}
scope = getattr(unreal.BridgeAssetSearchScope, scope_name, unreal.BridgeAssetSearchScope.ALL_ASSETS)
paths, tokens = unreal.UnrealBridgeAssetLibrary.search_assets(
    {params['query']!r},
    scope,
    {params['class_filter']!r},
    {params['case_sensitive']!r},
    {params['whole_word']!r},
    {max_results},
    {params['min_characters']!r},
    {params['custom_package_path']!r},
)
items = []
for p in paths[{offset}:{offset + page_size}]:
    text = p.export_text()
    package = text.split('.', 1)[0]
    asset_name = package.rsplit('/', 1)[-1] if '/' in package else package
    items.append({{'object_path': text, 'package_path': package, 'asset_name': asset_name}})
result = {{
    'items': items,
    'has_more': len(paths) > {offset + page_size},
    'include_tokens': list(tokens),
}}
print(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
"""
    payload = _run_bridge(args, ["exec", "--stdin"], stdin=script)
    if not payload.get("ok"):
        return _content(payload, True, args=args)
    try:
        data = _parse_bridge_json_output(payload)
        envelope = _page_envelope("asset_search", params, offset, page_size, data["items"], bool(data["has_more"]))
        envelope["include_tokens"] = data.get("include_tokens", [])
        return _content({"exit_code": payload["exit_code"], "ok": True, "result": envelope}, args=args)
    except Exception as exc:
        return _content({"ok": False, "error": f"bad asset page payload: {exc}", "raw": payload}, True, args=args)


def _handle_list_actors_page(args: JsonObject) -> JsonObject:
    params = {
        "class_filter": str(args.get("class_filter") or ""),
        "tag_filter": str(args.get("tag_filter") or ""),
        "name_filter": str(args.get("name_filter") or ""),
        "selected_only": bool(args.get("selected_only") or False),
    }
    try:
        offset, page_size = _page_request("actor_list", args, params)
    except Exception as exc:
        return _handle_page_error(args, exc)

    max_results = offset + page_size + 1
    script = f"""
import json
import unreal

actors = unreal.UnrealBridgeLevelLibrary.list_actors(
    {params['class_filter']!r},
    {params['tag_filter']!r},
    {params['name_filter']!r},
    {params['selected_only']!r},
    {max_results},
)
items = []
for a in actors[{offset}:{offset + page_size}]:
    loc = a.get_editor_property('location')
    items.append({{
        'name': a.get_editor_property('name'),
        'label': a.get_editor_property('label'),
        'class_name': a.get_editor_property('class_name'),
        'location': {{
            'x': float(loc.get_editor_property('x')),
            'y': float(loc.get_editor_property('y')),
            'z': float(loc.get_editor_property('z')),
        }},
        'tags': list(a.get_editor_property('tags')),
        'hidden': bool(a.get_editor_property('hidden')),
    }})
result = {{'items': items, 'has_more': len(actors) > {offset + page_size}}}
print(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
"""
    payload = _run_bridge(args, ["exec", "--stdin"], stdin=script)
    if not payload.get("ok"):
        return _content(payload, True, args=args)
    try:
        data = _parse_bridge_json_output(payload)
        envelope = _page_envelope("actor_list", params, offset, page_size, data["items"], bool(data["has_more"]))
        return _content({"exit_code": payload["exit_code"], "ok": True, "result": envelope}, args=args)
    except Exception as exc:
        return _content({"ok": False, "error": f"bad actor page payload: {exc}", "raw": payload}, True, args=args)


def _handle_searchable_name_values_page(args: JsonObject) -> JsonObject:
    params = {
        "struct_type": str(args["struct_type"]),
        "filter_prefix": str(args.get("filter_prefix") or ""),
    }
    try:
        offset, page_size = _page_request("searchable_name_values", args, params)
    except Exception as exc:
        return _handle_page_error(args, exc)

    max_results = offset + page_size + 1
    script = f"""
import json
import unreal

values = unreal.UnrealBridgeAssetLibrary.list_searchable_name_values(
    {params['struct_type']!r},
    {params['filter_prefix']!r},
    {max_results},
)
items = [str(v) for v in values[{offset}:{offset + page_size}]]
result = {{'items': items, 'has_more': len(values) > {offset + page_size}}}
print(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
"""
    payload = _run_bridge(args, ["exec", "--stdin"], stdin=script)
    if not payload.get("ok"):
        return _content(payload, True, args=args)
    try:
        data = _parse_bridge_json_output(payload)
        envelope = _page_envelope(
            "searchable_name_values",
            params,
            offset,
            page_size,
            data["items"],
            bool(data["has_more"]),
        )
        return _content({"exit_code": payload["exit_code"], "ok": True, "result": envelope}, args=args)
    except Exception as exc:
        return _content({"ok": False, "error": f"bad searchable-name values payload: {exc}", "raw": payload}, True, args=args)


def _handle_assets_referencing_searchable_name_page(args: JsonObject) -> JsonObject:
    params = {
        "struct_type": str(args["struct_type"]),
        "value_name": str(args["value_name"]),
        "package_path_filter": str(args.get("package_path_filter") or ""),
    }
    try:
        offset, page_size = _page_request("assets_referencing_searchable_name", args, params)
    except Exception as exc:
        return _handle_page_error(args, exc)

    max_results = offset + page_size + 1
    script = f"""
import json
import unreal

paths = unreal.UnrealBridgeAssetLibrary.find_assets_referencing_searchable_name(
    {params['struct_type']!r},
    {params['value_name']!r},
    {params['package_path_filter']!r},
    {max_results},
)
items = []
for p in paths[{offset}:{offset + page_size}]:
    package = str(p)
    asset_name = package.rsplit('/', 1)[-1] if '/' in package else package
    items.append({{'package_path': package, 'asset_name': asset_name}})
result = {{'items': items, 'has_more': len(paths) > {offset + page_size}}}
print(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
"""
    payload = _run_bridge(args, ["exec", "--stdin"], stdin=script)
    if not payload.get("ok"):
        return _content(payload, True, args=args)
    try:
        data = _parse_bridge_json_output(payload)
        envelope = _page_envelope(
            "assets_referencing_searchable_name",
            params,
            offset,
            page_size,
            data["items"],
            bool(data["has_more"]),
        )
        return _content({"exit_code": payload["exit_code"], "ok": True, "result": envelope}, args=args)
    except Exception as exc:
        return _content({"ok": False, "error": f"bad searchable-name refs payload: {exc}", "raw": payload}, True, args=args)


def _handle_datatable_row_names_page(args: JsonObject) -> JsonObject:
    params = {
        "data_table_path": str(args["data_table_path"]),
    }
    try:
        offset, page_size = _page_request("datatable_row_names", args, params)
    except Exception as exc:
        return _handle_page_error(args, exc)

    script = f"""
import json
import unreal

names = unreal.UnrealBridgeDataTableLibrary.get_data_table_row_names({params['data_table_path']!r})
items = [str(n) for n in names[{offset}:{offset + page_size}]]
result = {{'items': items, 'has_more': len(names) > {offset + page_size}}}
print(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
"""
    payload = _run_bridge(args, ["exec", "--stdin"], stdin=script)
    if not payload.get("ok"):
        return _content(payload, True, args=args)
    try:
        data = _parse_bridge_json_output(payload)
        envelope = _page_envelope(
            "datatable_row_names",
            params,
            offset,
            page_size,
            data["items"],
            bool(data["has_more"]),
        )
        return _content({"exit_code": payload["exit_code"], "ok": True, "result": envelope}, args=args)
    except Exception as exc:
        return _content({"ok": False, "error": f"bad DataTable row-names payload: {exc}", "raw": payload}, True, args=args)


def _handle_datatable_search_rows_page(args: JsonObject) -> JsonObject:
    column_filter = args.get("column_filter") or []
    if not isinstance(column_filter, list):
        return _content({"ok": False, "error": "column_filter must be an array of strings"}, True, args=args)
    params = {
        "data_table_path": str(args["data_table_path"]),
        "keyword": str(args["keyword"]),
        "column_filter": [str(v) for v in column_filter],
    }
    try:
        offset, page_size = _page_request("datatable_search_rows", args, params)
    except Exception as exc:
        return _handle_page_error(args, exc)

    script = f"""
import json
import unreal

hits = unreal.UnrealBridgeDataTableLibrary.search_data_table_rows(
    {params['data_table_path']!r},
    {params['keyword']!r},
    {params['column_filter']!r},
)
items = [str(n) for n in hits[{offset}:{offset + page_size}]]
result = {{'items': items, 'has_more': len(hits) > {offset + page_size}}}
print(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
"""
    payload = _run_bridge(args, ["exec", "--stdin"], stdin=script)
    if not payload.get("ok"):
        return _content(payload, True, args=args)
    try:
        data = _parse_bridge_json_output(payload)
        envelope = _page_envelope(
            "datatable_search_rows",
            params,
            offset,
            page_size,
            data["items"],
            bool(data["has_more"]),
        )
        return _content({"exit_code": payload["exit_code"], "ok": True, "result": envelope}, args=args)
    except Exception as exc:
        return _content({"ok": False, "error": f"bad DataTable row-search payload: {exc}", "raw": payload}, True, args=args)


def _handle_blueprint_call_sites_page(args: JsonObject) -> JsonObject:
    params = {
        "function_name": str(args["function_name"]),
        "owning_class_filter": str(args.get("owning_class_filter") or ""),
        "package_path": str(args.get("package_path") or "/Game"),
    }
    try:
        offset, page_size = _page_request("blueprint_call_sites", args, params)
    except Exception as exc:
        return _handle_page_error(args, exc)

    max_results = offset + page_size + 1
    script = f"""
import json
import unreal

sites = unreal.UnrealBridgeBlueprintLibrary.find_function_call_sites_global(
    {params['function_name']!r},
    {params['owning_class_filter']!r},
    {params['package_path']!r},
    {max_results},
)
items = []
for s in sites[{offset}:{offset + page_size}]:
    items.append({{
        'blueprint_path': str(s.blueprint_path),
        'graph_name': str(s.graph_name),
        'graph_type': str(s.graph_type),
        'node_guid': str(s.node_guid),
        'node_title': str(s.node_title),
        'kind': str(s.kind),
    }})
result = {{'items': items, 'has_more': len(sites) > {offset + page_size}}}
print(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
"""
    payload = _run_bridge(args, ["exec", "--stdin"], stdin=script)
    if not payload.get("ok"):
        return _content(payload, True, args=args)
    try:
        data = _parse_bridge_json_output(payload)
        envelope = _page_envelope(
            "blueprint_call_sites",
            params,
            offset,
            page_size,
            data["items"],
            bool(data["has_more"]),
        )
        return _content({"exit_code": payload["exit_code"], "ok": True, "result": envelope}, args=args)
    except Exception as exc:
        return _content({"ok": False, "error": f"bad Blueprint call-site payload: {exc}", "raw": payload}, True, args=args)


def _handle_blueprint_debug_prints_page(args: JsonObject) -> JsonObject:
    params = {
        "package_path": str(args.get("package_path") or "/Game"),
    }
    try:
        offset, page_size = _page_request("blueprint_debug_prints", args, params)
    except Exception as exc:
        return _handle_page_error(args, exc)

    max_results = offset + page_size + 1
    script = f"""
import json
import unreal

sites = unreal.UnrealBridgeBlueprintLibrary.find_blueprint_debug_prints(
    {params['package_path']!r},
    {max_results},
)
items = []
for s in sites[{offset}:{offset + page_size}]:
    items.append({{
        'blueprint_path': str(s.blueprint_path),
        'graph_name': str(s.graph_name),
        'graph_type': str(s.graph_type),
        'node_guid': str(s.node_guid),
        'node_title': str(s.node_title),
        'function_name': str(s.function_name),
        'string_literal': str(s.string_literal),
        'has_connected_input': bool(s.has_connected_input),
    }})
result = {{'items': items, 'has_more': len(sites) > {offset + page_size}}}
print(json.dumps(result, ensure_ascii=False, separators=(',', ':')))
"""
    payload = _run_bridge(args, ["exec", "--stdin"], stdin=script)
    if not payload.get("ok"):
        return _content(payload, True, args=args)
    try:
        data = _parse_bridge_json_output(payload)
        envelope = _page_envelope(
            "blueprint_debug_prints",
            params,
            offset,
            page_size,
            data["items"],
            bool(data["has_more"]),
        )
        return _content({"exit_code": payload["exit_code"], "ok": True, "result": envelope}, args=args)
    except Exception as exc:
        return _content({"ok": False, "error": f"bad Blueprint debug-print payload: {exc}", "raw": payload}, True, args=args)


TOOLS: tuple[ToolSpec, ...] = (
    ToolSpec(
        "bridge_ping",
        "Check whether a running Unreal Editor is reachable through UnrealBridge.",
        dict(COMMON_PROPS),
        _handle_ping,
    ),
    ToolSpec(
        "bridge_list_editors",
        "List every UnrealBridge editor discovered on the host.",
        dict(COMMON_PROPS),
        _handle_list_editors,
    ),
    ToolSpec(
        "bridge_preflight",
        "Run UnrealBridge AST preflight locally without contacting Unreal.",
        {
            "code": {"type": "string", "description": "Python code to lint. Use either code or file."},
            "file": {"type": "string", "description": "Path to a Python file to lint. Use either code or file."},
        },
        _handle_preflight,
    ),
    ToolSpec(
        "bridge_exec",
        "Execute Python in Unreal through bridge.py with AST preflight enabled by default.",
        {
            **COMMON_PROPS,
            "code": {"type": "string", "description": "Python source to execute in Unreal."},
            "no_preflight": {"type": "boolean", "description": "Skip AST preflight. Rarely needed."},
        },
        _handle_exec,
        ("code",),
    ),
    ToolSpec(
        "bridge_exec_file",
        "Execute an existing Python file in Unreal through bridge.py.",
        {
            **COMMON_PROPS,
            "file": {"type": "string", "description": "Path to the Python file to execute."},
            "no_preflight": {"type": "boolean", "description": "Skip AST preflight. Rarely needed."},
        },
        _handle_exec_file,
        ("file",),
    ),
    ToolSpec(
        "bridge_suggest",
        "Suggest UnrealBridge equivalents for raw unreal.* fallback patterns.",
        {
            "pattern": {"type": "string", "description": "Optional substring such as AssetRegistry or GameplayStatics."},
        },
        _handle_suggest,
    ),
    ToolSpec(
        "bridge_gamethread_ping",
        "Probe Unreal GameThread liveness without using the Python exec queue.",
        dict(COMMON_PROPS),
        _handle_gamethread_ping,
    ),
    ToolSpec(
        "bridge_resume",
        "Resume a paused Blueprint breakpoint through UnrealBridge.",
        dict(COMMON_PROPS),
        _handle_resume,
    ),
    ToolSpec(
        "bridge_wait_compile",
        "Poll material or material instance shader-map readiness.",
        {
            **COMMON_PROPS,
            "material_path": {"type": "string", "description": "Material or material instance asset path."},
            "wait_timeout": {"type": "number", "description": "Maximum total seconds to poll."},
            "poll_interval": {"type": "number", "description": "Seconds between polls."},
            "feature_level": {"type": "string", "description": "Optional feature level, e.g. SM5 or SM6."},
            "quality": {"type": "string", "description": "Optional quality level, e.g. High or Epic."},
        },
        _handle_wait_compile,
        ("material_path",),
    ),
    ToolSpec(
        "bridge_wait_pose_index",
        "Poll until a PoseSearchDatabase async index build finishes.",
        {
            **COMMON_PROPS,
            "database_path": {"type": "string", "description": "PoseSearchDatabase asset path."},
            "wait_timeout": {"type": "number", "description": "Maximum total seconds to poll."},
            "poll_interval": {"type": "number", "description": "Seconds between polls."},
        },
        _handle_wait_pose_index,
        ("database_path",),
    ),
    ToolSpec(
        "bridge_search_assets_page",
        "Search assets with cursor pagination. Use for broad asset queries instead of dumping all results.",
        {
            **COMMON_PROPS,
            "query": {"type": "string", "description": "Keyword query, e.g. 'hero !enemy' or 'hero &Type=Blueprint'."},
            "scope": {"type": "string", "description": "ALL_ASSETS, PROJECT, or CUSTOM_PACKAGE_PATH."},
            "class_filter": {"type": "string", "description": "Optional class filter. Full /Script/... path is passed to AssetRegistry."},
            "case_sensitive": {"type": "boolean", "description": "Use case-sensitive matching."},
            "whole_word": {"type": "boolean", "description": "Require whole-word matches."},
            "min_characters": {"type": "integer", "description": "Minimum query token length."},
            "custom_package_path": {"type": "string", "description": "Content root when scope is CUSTOM_PACKAGE_PATH."},
            "page_size": {"type": "integer", "description": f"Items per page, 1-{MAX_PAGE_SIZE}. Default {DEFAULT_PAGE_SIZE}."},
            "cursor": {"type": "string", "description": "Opaque next_cursor returned by the previous page."},
        },
        _handle_search_assets_page,
        ("query",),
    ),
    ToolSpec(
        "bridge_list_actors_page",
        "List actor briefs with cursor pagination. Use for populated levels instead of dumping all actors.",
        {
            **COMMON_PROPS,
            "class_filter": {"type": "string", "description": "Optional class short name or full path."},
            "tag_filter": {"type": "string", "description": "Optional actor tag filter."},
            "name_filter": {"type": "string", "description": "Optional case-insensitive label substring."},
            "selected_only": {"type": "boolean", "description": "Only include selected actors."},
            "page_size": {"type": "integer", "description": f"Items per page, 1-{MAX_PAGE_SIZE}. Default {DEFAULT_PAGE_SIZE}."},
            "cursor": {"type": "string", "description": "Opaque next_cursor returned by the previous page."},
        },
        _handle_list_actors_page,
    ),
    ToolSpec(
        "bridge_searchable_name_values_page",
        "List distinct SearchableName values with cursor pagination, e.g. used GameplayTag values.",
        {
            **COMMON_PROPS,
            "struct_type": {"type": "string", "description": "Short SearchableName struct type, e.g. GameplayTag."},
            "filter_prefix": {"type": "string", "description": "Optional value prefix filter, e.g. Ability."},
            "page_size": {"type": "integer", "description": f"Items per page, 1-{MAX_PAGE_SIZE}. Default {DEFAULT_PAGE_SIZE}."},
            "cursor": {"type": "string", "description": "Opaque next_cursor returned by the previous page."},
        },
        _handle_searchable_name_values_page,
        ("struct_type",),
    ),
    ToolSpec(
        "bridge_assets_referencing_searchable_name_page",
        "Find assets that reference a SearchableName value with cursor pagination.",
        {
            **COMMON_PROPS,
            "struct_type": {"type": "string", "description": "Short SearchableName struct type, e.g. GameplayTag."},
            "value_name": {"type": "string", "description": "SearchableName value, e.g. Combat.Hit."},
            "package_path_filter": {"type": "string", "description": "Optional package path prefix, e.g. /Game."},
            "page_size": {"type": "integer", "description": f"Items per page, 1-{MAX_PAGE_SIZE}. Default {DEFAULT_PAGE_SIZE}."},
            "cursor": {"type": "string", "description": "Opaque next_cursor returned by the previous page."},
        },
        _handle_assets_referencing_searchable_name_page,
        ("struct_type", "value_name"),
    ),
    ToolSpec(
        "bridge_datatable_row_names_page",
        "List DataTable row names with cursor pagination. Use before reading row details.",
        {
            **COMMON_PROPS,
            "data_table_path": {"type": "string", "description": "DataTable asset path."},
            "page_size": {"type": "integer", "description": f"Items per page, 1-{MAX_PAGE_SIZE}. Default {DEFAULT_PAGE_SIZE}."},
            "cursor": {"type": "string", "description": "Opaque next_cursor returned by the previous page."},
        },
        _handle_datatable_row_names_page,
        ("data_table_path",),
    ),
    ToolSpec(
        "bridge_datatable_search_rows_page",
        "Search DataTable rows by keyword with cursor pagination. Returns row names only.",
        {
            **COMMON_PROPS,
            "data_table_path": {"type": "string", "description": "DataTable asset path."},
            "keyword": {"type": "string", "description": "Case-insensitive keyword searched in row names and values."},
            "column_filter": {"type": "array", "items": {"type": "string"}, "description": "Optional columns to search."},
            "page_size": {"type": "integer", "description": f"Items per page, 1-{MAX_PAGE_SIZE}. Default {DEFAULT_PAGE_SIZE}."},
            "cursor": {"type": "string", "description": "Opaque next_cursor returned by the previous page."},
        },
        _handle_datatable_search_rows_page,
        ("data_table_path", "keyword"),
    ),
    ToolSpec(
        "bridge_blueprint_call_sites_page",
        "Find Blueprint call sites for one function with cursor pagination.",
        {
            **COMMON_PROPS,
            "function_name": {"type": "string", "description": "Target function short name, e.g. PrintString."},
            "owning_class_filter": {"type": "string", "description": "Optional owner class, e.g. KismetSystemLibrary."},
            "package_path": {"type": "string", "description": "Content root to scan. Defaults to /Game."},
            "page_size": {"type": "integer", "description": f"Items per page, 1-{MAX_PAGE_SIZE}. Default {DEFAULT_PAGE_SIZE}."},
            "cursor": {"type": "string", "description": "Opaque next_cursor returned by the previous page."},
        },
        _handle_blueprint_call_sites_page,
        ("function_name",),
    ),
    ToolSpec(
        "bridge_blueprint_debug_prints_page",
        "Find Blueprint PrintString / PrintText / PrintWarning sites with cursor pagination.",
        {
            **COMMON_PROPS,
            "package_path": {"type": "string", "description": "Content root to scan. Defaults to /Game."},
            "page_size": {"type": "integer", "description": f"Items per page, 1-{MAX_PAGE_SIZE}. Default {DEFAULT_PAGE_SIZE}."},
            "cursor": {"type": "string", "description": "Opaque next_cursor returned by the previous page."},
        },
        _handle_blueprint_debug_prints_page,
    ),
)
TOOL_BY_NAME = {tool.name: tool for tool in TOOLS}


def _missing_required_args(tool: ToolSpec, args: JsonObject) -> list[str]:
    return [name for name in tool.required if name not in args]


def _unknown_args(tool: ToolSpec, args: JsonObject) -> list[str]:
    allowed = set(tool.properties)
    return sorted(name for name in args if name not in allowed)


def _response(request_id: Any, result: Any) -> JsonObject:
    return {"jsonrpc": "2.0", "id": request_id, "result": result}


def _error(request_id: Any, code: int, message: str) -> JsonObject:
    return {"jsonrpc": "2.0", "id": request_id, "error": {"code": code, "message": message}}


def _select_protocol(params: JsonObject) -> str:
    requested = params.get("protocolVersion")
    if requested in SUPPORTED_PROTOCOLS:
        return str(requested)
    return DEFAULT_PROTOCOL


def _handle_message(message: JsonObject) -> JsonObject | None:
    method = message.get("method")
    is_notification = "id" not in message
    request_id = message.get("id")
    if is_notification:
        return None
    if message.get("jsonrpc") != "2.0":
        return _error(request_id, -32600, "request jsonrpc must be 2.0")
    if not isinstance(method, str):
        return _error(request_id, -32600, "request method must be a string")

    params = message["params"] if "params" in message and message.get("params") is not None else {}
    if not isinstance(params, dict):
        return _error(request_id, -32602, "params must be an object")

    if method == "initialize":
        return _response(
            request_id,
            {
                "protocolVersion": _select_protocol(params),
                "capabilities": {"tools": {}},
                "serverInfo": {"name": SERVER_NAME, "version": SERVER_VERSION},
                "instructions": "Use bridge_ping before mutating Unreal state. Provide UNREAL_BRIDGE_TOKEN through the environment rather than tool arguments.",
            },
        )
    if method in {"notifications/initialized", "notifications/cancelled", "$/cancelRequest"}:
        return _response(request_id, {})
    if method == "ping":
        return _response(request_id, {})
    if method == "tools/list":
        return _response(request_id, {"tools": [tool.as_mcp_tool() for tool in TOOLS]})
    if method == "tools/call":
        name = params.get("name")
        args = params.get("arguments") or {}
        if not isinstance(name, str):
            return _error(request_id, -32602, "tools/call requires params.name")
        if not isinstance(args, dict):
            return _error(request_id, -32602, "tools/call params.arguments must be an object")
        tool = TOOL_BY_NAME.get(name)
        if tool is None:
            return _response(request_id, _content({"ok": False, "error": f"unknown tool: {name}"}, True))
        missing = _missing_required_args(tool, args)
        if missing:
            return _error(request_id, -32602, f"missing required tool arguments for {name}: {', '.join(missing)}")
        unknown = _unknown_args(tool, args)
        if unknown:
            return _error(request_id, -32602, f"unknown tool arguments for {name}: {', '.join(unknown)}")
        return _response(request_id, tool.handler(args))
    if method == "resources/list":
        return _response(request_id, {"resources": []})
    if method == "resources/templates/list":
        return _response(request_id, {"resourceTemplates": []})
    if method == "prompts/list":
        return _response(request_id, {"prompts": []})
    if method == "logging/setLevel":
        return _response(request_id, {})
    return _error(request_id, -32601, f"method not found: {method}")


def _handle_payload(payload: Any) -> JsonObject | list[JsonObject] | None:
    if isinstance(payload, list):
        if not payload:
            return _error(None, -32600, "batch must not be empty")
        replies = []
        for item in payload:
            if not isinstance(item, dict):
                replies.append(_error(None, -32600, "batch item must be an object"))
                continue
            reply = _handle_message(item)
            if reply is not None:
                replies.append(reply)
        return replies or None
    if not isinstance(payload, dict):
        return _error(None, -32600, "message must be an object or batch")
    return _handle_message(payload)


def main() -> int:
    for line in sys.stdin:
        if not line.strip():
            continue
        try:
            payload = json.loads(line)
            reply = _handle_payload(payload)
        except json.JSONDecodeError as exc:
            reply = _error(None, -32700, f"parse error: {exc}")
        except Exception as exc:
            reply = _error(None, -32603, str(exc))
        if reply is not None:
            sys.stdout.write(json.dumps(reply, ensure_ascii=False, separators=(",", ":")) + "\n")
            sys.stdout.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
