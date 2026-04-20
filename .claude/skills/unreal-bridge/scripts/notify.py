#!/usr/bin/env python3
"""POST a completion notification to a webhook URL.

Designed to chain after long-running scripts so the user knows when a
cook / build / soak test is done. Auto-detects Slack / Discord URL
shapes and formats the body accordingly; falls back to a generic JSON
payload otherwise.

Reads `UNREALBRIDGE_WEBHOOK_URL` as the default `--url`, so a one-shot
notification is just:

    python rebuild_relaunch.py && python notify.py --status success --body 'rebuild done'

Or chain via exit code (the wrapper detects the previous shell exit):

    python rebuild_relaunch.py
    python notify.py --exit-status $? --body 'rebuild attempt'

stdlib only (`urllib.request`). No external deps.
"""
from __future__ import annotations

import argparse
import datetime
import json
import os
import platform
import socket
import sys
import urllib.error
import urllib.request

STATUS_EMOJI = {
    "success": ":white_check_mark:",
    "failure": ":x:",
    "warning": ":warning:",
    "info":    ":information_source:",
}


def detect_format(url: str) -> str:
    if "hooks.slack.com" in url:
        return "slack"
    if "discord.com/api/webhooks" in url or "discordapp.com/api/webhooks" in url:
        return "discord"
    return "generic"


def build_slack(title: str, body: str, status: str, fields: list[tuple[str, str]], source: str) -> dict:
    emoji = STATUS_EMOJI.get(status, "")
    header = f"{emoji} *{title}*" if title else emoji
    text_parts = [header.strip(), body.strip()] if body else [header.strip()]
    if fields:
        text_parts.append("\n".join(f"• *{k}*: {v}" for k, v in fields))
    text_parts.append(f"_source: {source}_")
    return {"text": "\n\n".join(p for p in text_parts if p)}


def build_discord(title: str, body: str, status: str, fields: list[tuple[str, str]], source: str) -> dict:
    color_map = {"success": 0x2ECC71, "failure": 0xE74C3C, "warning": 0xF39C12, "info": 0x3498DB}
    emoji_map = {"success": "✅", "failure": "❌", "warning": "⚠️", "info": "ℹ️"}
    embed = {
        "title": f"{emoji_map.get(status, '')} {title}".strip() or status,
        "description": body or "",
        "color": color_map.get(status, 0x95A5A6),
        "footer": {"text": source},
        "timestamp": datetime.datetime.utcnow().replace(microsecond=0).isoformat() + "Z",
    }
    if fields:
        embed["fields"] = [{"name": k, "value": v, "inline": True} for k, v in fields]
    return {"embeds": [embed]}


def build_generic(title: str, body: str, status: str, fields: list[tuple[str, str]], source: str) -> dict:
    return {
        "title": title,
        "body": body,
        "status": status,
        "source": source,
        "timestamp_utc": datetime.datetime.utcnow().replace(microsecond=0).isoformat() + "Z",
        "fields": {k: v for k, v in fields},
    }


def post(url: str, payload: dict, timeout: float = 10.0) -> tuple[int, str]:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/json"},
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            body = resp.read().decode("utf-8", errors="replace")
            return resp.status, body
    except urllib.error.HTTPError as e:
        return e.code, e.read().decode("utf-8", errors="replace")


def parse_field(spec: str) -> tuple[str, str]:
    if "=" not in spec:
        raise argparse.ArgumentTypeError(f"--field expects key=value, got: {spec!r}")
    k, v = spec.split("=", 1)
    return k.strip(), v.strip()


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    p.add_argument(
        "--url",
        default=os.environ.get("UNREALBRIDGE_WEBHOOK_URL", ""),
        help="Webhook URL. Defaults to UNREALBRIDGE_WEBHOOK_URL env var.",
    )
    p.add_argument("--title", default="", help="Short headline.")
    p.add_argument("--body", default="", help="Message body. Pass '-' to read stdin.")
    p.add_argument(
        "--status",
        choices=("success", "failure", "warning", "info"),
        default="info",
        help="Status — controls emoji / color when the format supports it.",
    )
    p.add_argument(
        "--exit-status",
        type=int,
        default=None,
        help="Last command's exit code. When set, --status defaults to success (0) / failure (non-zero).",
    )
    p.add_argument(
        "--field",
        action="append",
        default=[],
        type=parse_field,
        help="Extra key=value field (repeatable).",
    )
    p.add_argument(
        "--format",
        choices=("auto", "slack", "discord", "generic", "raw"),
        default="auto",
        help="Force a payload shape. 'raw' sends --raw-body as-is.",
    )
    p.add_argument(
        "--raw-body",
        default=None,
        help="Custom JSON payload — bypasses formatting. Pass '-' to read stdin.",
    )
    p.add_argument("--source", default=f"UnrealBridge@{socket.gethostname()}")
    p.add_argument("--timeout", type=float, default=10.0)
    p.add_argument("--quiet", action="store_true")
    args = p.parse_args()

    if not args.url:
        print("ERROR: --url is required (or set UNREALBRIDGE_WEBHOOK_URL).", file=sys.stderr)
        return 2

    if args.exit_status is not None and args.status == "info":
        args.status = "success" if args.exit_status == 0 else "failure"

    body = args.body
    if body == "-":
        body = sys.stdin.read().rstrip("\n")

    raw_body = args.raw_body
    if raw_body == "-":
        raw_body = sys.stdin.read()

    if args.format == "raw" or raw_body is not None:
        if raw_body is None:
            print("ERROR: --format=raw requires --raw-body.", file=sys.stderr)
            return 2
        try:
            payload = json.loads(raw_body)
        except json.JSONDecodeError as e:
            print(f"ERROR: --raw-body is not valid JSON: {e}", file=sys.stderr)
            return 2
    else:
        fmt = args.format if args.format != "auto" else detect_format(args.url)
        builder = {"slack": build_slack, "discord": build_discord, "generic": build_generic}[fmt]
        payload = builder(args.title, body, args.status, args.field, args.source)

    status, response_body = post(args.url, payload, timeout=args.timeout)
    if 200 <= status < 300:
        if not args.quiet:
            print(f"[notify] HTTP {status} — {len(response_body)}B response")
        return 0
    else:
        print(
            f"[notify] HTTP {status} — webhook rejected payload:\n{response_body[:500]}",
            file=sys.stderr,
        )
        return 1


if __name__ == "__main__":
    sys.exit(main())
