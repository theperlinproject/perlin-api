#!/usr/bin/env python3
import argparse
import json
import re
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
ap = argparse.ArgumentParser(description="Checks that every op and event in the schema is served by the hosts that are given")
ap.add_argument("--client", type=Path, help="directory with the client's sources; omitted: client ops are not checked")
ap.add_argument("--server", type=Path, help="directory with the server's sources; omitted: server ops are not checked")
args = ap.parse_args()
api = json.loads((root / "api/perlin_api.json").read_text(encoding="utf-8"))

def read(paths):
    return "".join(p.read_text(encoding="utf-8", errors="ignore") for p in paths if p.exists())

client = read(sorted(args.client.rglob("*.cpp"))) if args.client else ""
server = read(sorted(args.server.rglob("*.cpp"))) if args.server else ""
runtime = read(sorted((root / "sdk/perlin").glob("*.hpp")))
hosts = {"client": client, "server": server}
present = {h for h, text in hosts.items() if text}

problems = []
for op in api["ops"]:
    name = op["name"]
    for host in op["hosts"]:
        if host not in present:
            continue
        text = hosts[host]
        if not re.search(r"Op::%s\b" % name, text) and not re.search(r"\bOp%s\(" % name, text):
            problems.append(f"{host}: Op::{name} (0x{op['id']:04X}) is in the schema but the {host} never serves it")

internal = {"Load", "Unload"}
for ev in api["events"]:
    name = ev["name"]
    if name in internal:
        continue
    for host in ev["hosts"]:
        if host not in present:
            continue
        if not re.search(r"Event::%s\b" % name, hosts[host]):
            problems.append(f"{host}: Event::{name} ({ev['id']}) is in the schema but the {host} never raises it")
    if any(t["name"] == "internal" for t in ev["tags"]) and not re.search(r"Event::%s\b" % name, runtime):
        problems.append(f"sdk: internal Event::{name} is never consumed by the runtime")

print(f"check_api: {len(api['ops'])} operations, {len(api['events'])} events, schema v{api['version']}")
for p in problems:
    print("  " + p)
sys.exit(1 if problems else 0)
