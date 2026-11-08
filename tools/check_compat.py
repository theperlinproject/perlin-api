#!/usr/bin/env python3
import argparse
import json
import sys
from pathlib import Path

root = Path(__file__).resolve().parent.parent
CURRENT = root / "api" / "perlin_api.json"
BASELINES = root / "api" / "baselines"

def by_name(items):
    return {i["name"]: i for i in items}

def field_sig(f):
    return (f["name"], json.dumps(f["type"], sort_keys=True))

def compare(base, cur):
    problems = []
    cur_ops = {o["id"]: o for o in cur["ops"]}
    for o in base["ops"]:
        c = cur_ops.get(o["id"])
        where = f"op {o['name']} (0x{o['id']:04X})"
        if c is None:
            problems.append(f"{where}: removed")
            continue
        if c["name"] != o["name"]:
            problems.append(f"{where}: renamed to {c['name']}")
        for key in ("request", "response", "permissions"):
            if c[key] != o[key]:
                problems.append(f"{where}: {key} changed from {o[key]} to {c[key]}")
        if not set(o["hosts"]) <= set(c["hosts"]):
            problems.append(f"{where}: no longer hosted by {sorted(set(o['hosts']) - set(c['hosts']))}")
    cur_events = {e["id"]: e for e in cur["events"]}
    for e in base["events"]:
        c = cur_events.get(e["id"])
        where = f"event {e['name']} ({e['id']})"
        if c is None:
            problems.append(f"{where}: removed")
            continue
        if c["name"] != e["name"]:
            problems.append(f"{where}: renamed to {c['name']}")
        if c["payload"] != e["payload"]:
            problems.append(f"{where}: payload changed from {e['payload']} to {c['payload']}")
        if not set(e["hosts"]) <= set(c["hosts"]):
            problems.append(f"{where}: no longer raised by {sorted(set(e['hosts']) - set(c['hosts']))}")
    cur_structs = by_name(cur["structs"])
    for s in base["structs"]:
        c = cur_structs.get(s["name"])
        if c is None:
            problems.append(f"struct {s['name']}: removed")
            continue
        old = [field_sig(f) for f in s["fields"]]
        new = [field_sig(f) for f in c["fields"]]
        if old != new:
            problems.append(f"struct {s['name']}: fields changed (a struct is a frozen wire layout: add a new struct or op instead)")
    cur_enums = by_name(cur["enums"])
    for e in base["enums"]:
        c = cur_enums.get(e["name"])
        if c is None:
            problems.append(f"enum {e['name']}: removed")
            continue
        if c["base"] != e["base"] or c["flags"] != e["flags"]:
            problems.append(f"enum {e['name']}: base type or flags-ness changed")
        have = {m["name"]: m["value"] for m in c["members"]}
        for m in e["members"]:
            if m["name"] not in have:
                problems.append(f"enum {e['name']}.{m['name']}: removed")
            elif have[m["name"]] != m["value"]:
                problems.append(f"enum {e['name']}.{m['name']}: value changed from {m['value']} to {have[m['name']]}")
    cur_consts = by_name(cur["consts"])
    for k in base["consts"]:
        c = cur_consts.get(k["name"])
        if c is None:
            problems.append(f"const {k['name']}: removed")
        elif c["value"] != k["value"] or c["type"] != k["type"]:
            problems.append(f"const {k['name']}: changed")
    cur_alias = by_name(cur["aliases"])
    for a in base["aliases"]:
        if a["name"] not in cur_alias:
            problems.append(f"alias {a['name']}: removed")
    return problems

def main():
    ap = argparse.ArgumentParser(description='Fails when the schema breaks a frozen revision')
    ap.add_argument("--update", action="store_true", help="freeze the current schema as this revision's baseline (release step)")
    args = ap.parse_args()
    cur = json.loads(CURRENT.read_text(encoding="utf-8"))
    if args.update:
        BASELINES.mkdir(exist_ok=True)
        out = BASELINES / f"rev{cur['version']}.json"
        out.write_text(json.dumps(cur, indent=1, ensure_ascii=False) + "\n", encoding="utf-8", newline="\n")
        print(f"froze revision {cur['version']}: {len(cur['ops'])} operations, {len(cur['events'])} events -> {out.relative_to(root)}")
        return 0
    files = sorted(BASELINES.glob("rev*.json"), key=lambda p: int(p.stem[3:])) if BASELINES.exists() else []
    if not files:
        print("check_compat: no frozen revision in api/baselines (run with --update to freeze one)", file=sys.stderr)
        return 2
    failed = False
    for f in files:
        base = json.loads(f.read_text(encoding="utf-8"))
        problems = compare(base, cur)
        print(f"check_compat: frozen revision {base['version']} ({len(base['ops'])} ops, {len(base['events'])} events) vs current "
              f"revision {cur['version']} ({len(cur['ops'])} ops, {len(cur['events'])} events): {'BROKEN' if problems else 'ok'}")
        for p in problems:
            print("  BREAKING: " + p)
        failed |= bool(problems)
    return 1 if failed else 0

if __name__ == "__main__":
    sys.exit(main())
