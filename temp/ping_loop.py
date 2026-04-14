"""Continuously ping the UnrealBridge server and log each attempt.

Usage:
    python temp/ping_loop.py                 # default 0.5s interval
    python temp/ping_loop.py --interval 0.1  # faster
    python temp/ping_loop.py --count 200     # stop after N pings
"""
import argparse
import socket
import struct
import json
import time
import uuid
from datetime import datetime

HOST = "127.0.0.1"
PORT = 9876


def ping_once(timeout: float) -> tuple[bool, str]:
    payload = json.dumps({"id": str(uuid.uuid4()), "command": "ping"}).encode("utf-8")
    header = struct.pack(">I", len(payload))
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.settimeout(timeout)
            s.connect((HOST, PORT))
            s.sendall(header + payload)
            hdr = _recv_all(s, 4)
            n = struct.unpack(">I", hdr)[0]
            body = _recv_all(s, n)
            resp = json.loads(body.decode("utf-8"))
            ready = resp.get("ready")
            if ready is True:
                return True, "ready"
            if ready is False:
                return True, "initializing"
            return True, "ok"
    except ConnectionRefusedError:
        return False, "refused (editor not listening)"
    except socket.timeout:
        return False, f"timeout after {timeout}s"
    except OSError as e:
        # 10053 = WSAECONNABORTED, 10054 = WSAECONNRESET
        return False, f"OSError[{getattr(e, 'winerror', e.errno)}] {e.strerror or e}"
    except Exception as e:
        return False, f"{type(e).__name__}: {e}"


def _recv_all(sock, n: int) -> bytes:
    chunks = []
    got = 0
    while got < n:
        c = sock.recv(n - got)
        if not c:
            raise ConnectionError("connection closed by server mid-read")
        chunks.append(c)
        got += len(c)
    return b"".join(chunks)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--interval", type=float, default=0.5, help="seconds between pings")
    ap.add_argument("--timeout", type=float, default=3.0, help="per-ping timeout")
    ap.add_argument("--count", type=int, default=0, help="stop after N pings (0 = forever)")
    args = ap.parse_args()

    print(f"Pinging {HOST}:{PORT} every {args.interval}s (timeout={args.timeout}s). Ctrl+C to stop.")
    print(f"{'time':>12s}  {'#':>5s}  {'ms':>6s}  result")
    print("-" * 60)

    ok = fail = seq = 0
    try:
        while args.count == 0 or seq < args.count:
            seq += 1
            t0 = time.time()
            success, msg = ping_once(args.timeout)
            elapsed_ms = (time.time() - t0) * 1000.0
            ts = datetime.now().strftime("%H:%M:%S.%f")[:-3]
            tag = "OK " if success else "ERR"
            print(f"{ts}  {seq:5d}  {elapsed_ms:6.1f}  {tag}  {msg}", flush=True)
            if success:
                ok += 1
            else:
                fail += 1
            if args.interval > 0:
                time.sleep(args.interval)
    except KeyboardInterrupt:
        pass
    total = ok + fail
    rate = (ok / total * 100.0) if total else 0.0
    print("-" * 60)
    print(f"Summary: {ok}/{total} ok ({rate:.1f}%), {fail} failures")


if __name__ == "__main__":
    main()
