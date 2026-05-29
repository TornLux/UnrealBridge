#!/usr/bin/env python3
"""No-editor smoke tests for MCP cursor pagination helpers."""

from __future__ import annotations

import importlib.util
import json
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SERVER = ROOT / ".claude" / "skills" / "unreal-bridge" / "scripts" / "unrealbridge_mcp_server.py"


def _load_server():
    spec = importlib.util.spec_from_file_location("unrealbridge_mcp_for_pagination_smoke", SERVER)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def main() -> int:
    mcp = _load_server()

    params = {"query": "hero", "scope": "ALL_ASSETS"}
    cursor = mcp._encode_cursor("asset_search", params, 25)
    offset, page_size = mcp._page_request(
        "asset_search",
        {"cursor": cursor, "page_size": 10},
        params,
    )
    assert offset == 25
    assert page_size == 10

    offset, page_size = mcp._page_request("asset_search", {"page_size": 9999}, params)
    assert offset == 0
    assert page_size == mcp.MAX_PAGE_SIZE

    try:
        mcp._page_request("asset_search", {"cursor": cursor}, {"query": "villain", "scope": "ALL_ASSETS"})
    except ValueError as exc:
        assert str(exc).startswith("STALE_CURSOR")
    else:
        raise AssertionError("expected stale cursor")

    envelope = mcp._page_envelope("asset_search", params, 25, 10, [{"name": "A"}], True)
    assert envelope["page"]["has_more"] is True
    assert envelope["page"]["next_cursor"]
    decoded = mcp._decode_cursor(envelope["page"]["next_cursor"])
    assert decoded["offset"] == 26

    tool_names = {tool.name for tool in mcp.TOOLS}
    assert {
        "bridge_search_assets_page",
        "bridge_list_actors_page",
        "bridge_searchable_name_values_page",
        "bridge_assets_referencing_searchable_name_page",
        "bridge_datatable_row_names_page",
        "bridge_datatable_search_rows_page",
        "bridge_blueprint_call_sites_page",
        "bridge_blueprint_debug_prints_page",
    }.issubset(tool_names)

    original_run_bridge = mcp._run_bridge
    captured_scripts = []

    def fake_run_bridge(args, command, stdin=None):
        script = stdin or ""
        captured_scripts.append(script)
        if "search_assets(" in script:
            return {
                "exit_code": 0,
                "ok": True,
                "result": {
                    "output": (
                        'log line\n'
                        '{"items":[{"object_path":"/Game/BP_Hero.BP_Hero","package_path":"/Game/BP_Hero",'
                        '"asset_name":"BP_Hero"}],"has_more":false,"include_tokens":[]}'
                    )
                },
            }
        if "list_actors(" in script:
            return {
                "exit_code": 0,
                "ok": True,
                "result": {
                    "output": (
                        'log line\n'
                        '{"items":[{"name":"BP_Hero_C_0","label":"BP_Hero","class_name":"BP_Hero_C",'
                        '"location":{"x":0,"y":0,"z":0},"tags":[],"hidden":false}],"has_more":false}'
                    )
                },
            }
        if "list_searchable_name_values" in script:
            return {
                "exit_code": 0,
                "ok": True,
                "result": {"output": 'log line\n{"items":["Ability.A","Ability.B"],"has_more":true}'},
            }
        if "find_assets_referencing_searchable_name" in script:
            return {
                "exit_code": 0,
                "ok": True,
                "result": {
                    "output": 'log line\n{"items":[{"package_path":"/Game/BP_Hero","asset_name":"BP_Hero"}],"has_more":false}'
                },
            }
        if "get_data_table_row_names" in script:
            return {
                "exit_code": 0,
                "ok": True,
                "result": {"output": 'log line\n{"items":["Sword","Axe"],"has_more":true}'},
            }
        if "search_data_table_rows" in script:
            return {
                "exit_code": 0,
                "ok": True,
                "result": {"output": 'log line\n{"items":["Sword"],"has_more":false}'},
            }
        if "find_function_call_sites_global" in script:
            return {
                "exit_code": 0,
                "ok": True,
                "result": {
                    "output": (
                        'log line\n'
                        '{"items":[{"blueprint_path":"/Game/BP_Hero.BP_Hero","graph_name":"EventGraph",'
                        '"graph_type":"EventGraph","node_guid":"abc","node_title":"Print String","kind":"call"}],'
                        '"has_more":true}'
                    )
                },
            }
        if "find_blueprint_debug_prints" in script:
            return {
                "exit_code": 0,
                "ok": True,
                "result": {
                    "output": (
                        'log line\n'
                        '{"items":[{"blueprint_path":"/Game/BP_Hero.BP_Hero","graph_name":"EventGraph",'
                        '"graph_type":"EventGraph","node_guid":"abc","node_title":"Print String",'
                        '"function_name":"PrintString","string_literal":"debug","has_connected_input":false}],'
                        '"has_more":false}'
                    )
                },
            }
        return original_run_bridge(args, command, stdin)

    mcp._run_bridge = fake_run_bridge
    try:
        searchable = mcp._handle_searchable_name_values_page(
            {"struct_type": "GameplayTag", "filter_prefix": "Ability.", "page_size": 2}
        )
        searchable_payload = json.loads(searchable["content"][0]["text"])
        searchable_result = searchable_payload["result"]
        assert searchable_result["items"] == ["Ability.A", "Ability.B"]
        assert searchable_result["page"]["has_more"] is True
        assert searchable_result["page"]["next_cursor"]

        refs = mcp._handle_assets_referencing_searchable_name_page(
            {"struct_type": "GameplayTag", "value_name": "Ability.A", "package_path_filter": "/Game", "page_size": 1}
        )
        refs_payload = json.loads(refs["content"][0]["text"])
        refs_result = refs_payload["result"]
        assert refs_result["items"][0]["package_path"] == "/Game/BP_Hero"
        assert refs_result["page"]["has_more"] is False
        assert refs_result["page"]["next_cursor"] == ""

        rows = mcp._handle_datatable_row_names_page(
            {"data_table_path": "/Game/Data/DT_Items.DT_Items", "page_size": 2}
        )
        rows_payload = json.loads(rows["content"][0]["text"])
        rows_result = rows_payload["result"]
        assert rows_result["items"] == ["Sword", "Axe"]
        assert rows_result["page"]["has_more"] is True
        assert rows_result["page"]["next_cursor"]

        hits = mcp._handle_datatable_search_rows_page(
            {
                "data_table_path": "/Game/Data/DT_Items.DT_Items",
                "keyword": "sword",
                "column_filter": ["DisplayName"],
                "page_size": 1,
            }
        )
        hits_payload = json.loads(hits["content"][0]["text"])
        hits_result = hits_payload["result"]
        assert hits_result["items"] == ["Sword"]
        assert hits_result["page"]["has_more"] is False

        call_sites = mcp._handle_blueprint_call_sites_page(
            {
                "function_name": "PrintString",
                "owning_class_filter": "KismetSystemLibrary",
                "package_path": "/Game",
                "page_size": 1,
            }
        )
        call_sites_payload = json.loads(call_sites["content"][0]["text"])
        call_sites_result = call_sites_payload["result"]
        assert call_sites_result["items"][0]["blueprint_path"] == "/Game/BP_Hero.BP_Hero"
        assert call_sites_result["items"][0]["kind"] == "call"
        assert call_sites_result["page"]["has_more"] is True

        debug_prints = mcp._handle_blueprint_debug_prints_page(
            {"package_path": "/Game", "page_size": 1}
        )
        debug_prints_payload = json.loads(debug_prints["content"][0]["text"])
        debug_prints_result = debug_prints_payload["result"]
        assert debug_prints_result["items"][0]["function_name"] == "PrintString"
        assert debug_prints_result["items"][0]["string_literal"] == "debug"
        assert debug_prints_result["page"]["has_more"] is False

        def require_script(marker, *needles):
            for script in reversed(captured_scripts):
                if marker in script:
                    for needle in needles:
                        assert needle in script, f"missing {needle!r} in script for {marker}"
                    return
            raise AssertionError(f"missing script for {marker}")

        captured_scripts.clear()

        asset_params = {
            "query": "hero",
            "scope": "ALL_ASSETS",
            "class_filter": "",
            "case_sensitive": False,
            "whole_word": False,
            "min_characters": 1,
            "custom_package_path": "",
        }
        mcp._handle_search_assets_page(
            {"query": "hero", "page_size": 2, "cursor": mcp._encode_cursor("asset_search", asset_params, 2)}
        )
        require_script("search_assets(", "    5,", "paths[2:4]", "len(paths) > 4")

        actor_params = {
            "class_filter": "",
            "tag_filter": "",
            "name_filter": "",
            "selected_only": False,
        }
        mcp._handle_list_actors_page(
            {"page_size": 2, "cursor": mcp._encode_cursor("actor_list", actor_params, 2)}
        )
        require_script("list_actors(", "    5,", "actors[2:4]", "len(actors) > 4")

        searchable_params = {"struct_type": "GameplayTag", "filter_prefix": "Ability."}
        mcp._handle_searchable_name_values_page(
            {
                "struct_type": "GameplayTag",
                "filter_prefix": "Ability.",
                "page_size": 2,
                "cursor": mcp._encode_cursor("searchable_name_values", searchable_params, 2),
            }
        )
        require_script("list_searchable_name_values(", "    5,", "values[2:4]", "len(values) > 4")

        refs_params = {
            "struct_type": "GameplayTag",
            "value_name": "Ability.A",
            "package_path_filter": "/Game",
        }
        mcp._handle_assets_referencing_searchable_name_page(
            {
                "struct_type": "GameplayTag",
                "value_name": "Ability.A",
                "package_path_filter": "/Game",
                "page_size": 2,
                "cursor": mcp._encode_cursor("assets_referencing_searchable_name", refs_params, 2),
            }
        )
        require_script("find_assets_referencing_searchable_name(", "    5,", "paths[2:4]", "len(paths) > 4")

        row_params = {"data_table_path": "/Game/Data/DT_Items.DT_Items"}
        mcp._handle_datatable_row_names_page(
            {
                "data_table_path": "/Game/Data/DT_Items.DT_Items",
                "page_size": 2,
                "cursor": mcp._encode_cursor("datatable_row_names", row_params, 2),
            }
        )
        require_script("get_data_table_row_names(", "names[2:4]", "len(names) > 4")

        hit_params = {
            "data_table_path": "/Game/Data/DT_Items.DT_Items",
            "keyword": "sword",
            "column_filter": ["DisplayName"],
        }
        mcp._handle_datatable_search_rows_page(
            {
                "data_table_path": "/Game/Data/DT_Items.DT_Items",
                "keyword": "sword",
                "column_filter": ["DisplayName"],
                "page_size": 2,
                "cursor": mcp._encode_cursor("datatable_search_rows", hit_params, 2),
            }
        )
        require_script("search_data_table_rows(", "hits[2:4]", "len(hits) > 4")

        call_params = {
            "function_name": "PrintString",
            "owning_class_filter": "KismetSystemLibrary",
            "package_path": "/Game",
        }
        mcp._handle_blueprint_call_sites_page(
            {
                "function_name": "PrintString",
                "owning_class_filter": "KismetSystemLibrary",
                "package_path": "/Game",
                "page_size": 2,
                "cursor": mcp._encode_cursor("blueprint_call_sites", call_params, 2),
            }
        )
        require_script("find_function_call_sites_global(", "    5,", "sites[2:4]", "len(sites) > 4")

        debug_params = {"package_path": "/Game"}
        mcp._handle_blueprint_debug_prints_page(
            {
                "package_path": "/Game",
                "page_size": 2,
                "cursor": mcp._encode_cursor("blueprint_debug_prints", debug_params, 2),
            }
        )
        require_script("find_blueprint_debug_prints(", "    5,", "sites[2:4]", "len(sites) > 4")
    finally:
        mcp._run_bridge = original_run_bridge

    print(json.dumps({"ok": True, "tools": sorted(tool_names)}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
