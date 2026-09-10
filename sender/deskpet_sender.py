#!/usr/bin/env python3
"""DeskPet agent sender: planofplan status -> BLE push to DeskPet.

状态采集与归并已委托给 planofplan(/api/agent-status,文档见
planofplan/docs/agent-status-api.md);本脚本只剩两件事:
  1. 轮询 planofplan 的 4 槽快照(短暂失败沿用上次快照,≤30s);
  2. 把槽位打包成 52 字节 GATT 写入,经 BLE 推给 DeskPet Click。
"""
import argparse
import asyncio
import json
import struct
import time
import urllib.request

DEVICE_NAME = "DeskPet Click"
STATUS_CHAR_SHORT = 0xAA01
SUMMARY_CHAR_SHORT = 0xAA02
MAX_SLOTS = 4
HOLD_LAST_GOOD_S = 30  # planofplan 短暂失联时沿用旧快照的时限


def _utf8_field(value, limit):
    """Truncate to a valid UTF-8 prefix and reserve one byte for NUL."""
    raw = str(value).replace("\0", " ").encode("utf-8", "ignore")[:limit]
    return raw.decode("utf-8", "ignore").encode("utf-8")


def build_update(slot, view):
    if view is None:
        return struct.pack("<BBBB", slot, 3, 0, 0) + bytes(48)
    name = _utf8_field(view["name"], 15)
    text = _utf8_field(view["text"], 31)
    src_id = {"codex": 0, "dsh": 1, "opencode": 2}.get(view["source"], 3)
    state = view["state"] if 0 <= view["state"] <= 5 else 0
    progress = view["progress"] if 0 <= view["progress"] <= 100 or view["progress"] == 255 else 255
    return struct.pack("<BBBB", slot, src_id, state, progress) + \
        name.ljust(16, b"\0") + text.ljust(32, b"\0")


def updates_for_slots(slots):
    """Always send all slots so disappeared agents are cleared on-device."""
    return [build_update(slot, slots.get(slot)) for slot in range(MAX_SLOTS)]


def slots_from_api(payload, disabled):
    """planofplan JSON -> {slot: view}。空槽不进 dict -> 触发设备端清槽。"""
    out = {}
    for s in payload.get("slots", []):
        if not s.get("occupied"):
            continue
        if s.get("source") in disabled:
            continue
        out[s["slot"]] = s
    return out


async def fetch_status(base_url, timeout=3.0):
    loop = asyncio.get_running_loop()
    def _get():
        with urllib.request.urlopen(base_url.rstrip("/") + "/api/agent-status", timeout=timeout) as r:
            return json.load(r)
    return await loop.run_in_executor(None, _get)


async def ble_push(slots):
    from bleak import BleakClient, BleakScanner
    dev = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
    if not dev:
        print("BLE: DeskPet Click not found", flush=True)
        return False
    async with BleakClient(dev) as client:
        status_char = summary_char = None
        for svc in client.services:
            for ch in svc.characteristics:
                try:
                    short = int(ch.uuid.split("-")[0], 16)
                except ValueError:
                    continue
                if short == STATUS_CHAR_SHORT:
                    status_char = ch.uuid
                elif short == SUMMARY_CHAR_SHORT:
                    summary_char = ch.uuid
        if status_char is None:
            print("BLE: status char 0xAA01 not found (pair DeskPet first?)", flush=True)
            return False
        for update in updates_for_slots(slots):
            await client.write_gatt_char(status_char, update)
        if summary_char:
            raw = await client.read_gatt_char(summary_char)
            print("BLE: summary", raw.hex(), flush=True)
    return True


def print_table(slots):
    print(f"--- slots {time.strftime('%H:%M:%S')} ---")
    for i in range(MAX_SLOTS):
        v = slots.get(i)
        if v:
            print(f"[{i}] {v['source']:8s} {v['name']:16s} st={v['state']} pr={v['progress']} {v['text']}")
        else:
            print(f"[{i}] -")


async def amain(args):
    base = args.planofplan_url.rstrip("/")
    disabled = {s.strip() for s in args.disable.split(",") if s.strip()}
    if args.discover:
        from bleak import BleakClient, BleakScanner
        dev = await BleakScanner.find_device_by_name(DEVICE_NAME, timeout=10.0)
        if not dev:
            print("not found")
            return 1
        async with BleakClient(dev) as client:
            for svc in client.services:
                print("svc", svc.uuid)
                for ch in svc.characteristics:
                    print("  char", ch.uuid, ch.properties)
        return 0
    slots, good_until = {}, 0
    while True:
        try:
            payload = await fetch_status(base)
            slots = slots_from_api(payload, disabled)
            good_until = time.time() + HOLD_LAST_GOOD_S
        except Exception as e:
            if time.time() < good_until:
                print(f"planofplan hiccup ({e}); holding last snapshot", flush=True)
            else:
                slots = {}
                print(f"planofplan unreachable at {base}: {e}", flush=True)
        if args.dry_run:
            print_table(slots)
            return 0
        print_table(slots)
        try:
            await ble_push(slots)
        except Exception as e:
            print(f"BLE error: {e}", flush=True)
        await asyncio.sleep(args.interval)


def main():
    ap = argparse.ArgumentParser(description="DeskPet BLE sender (planofplan consumer)")
    ap.add_argument("--dry-run", action="store_true", help="打印槽位表,不碰蓝牙")
    ap.add_argument("--discover", action="store_true", help="扫描 DeskPet 的 GATT 服务")
    ap.add_argument("--interval", type=float, default=5.0)
    ap.add_argument("--planofplan-url", default="http://127.0.0.1:9288")
    ap.add_argument("--disable", default="", help="逗号分隔的源名,如 amp,agy")
    args = ap.parse_args()
    return asyncio.run(amain(args))


if __name__ == "__main__":
    sys_exit = main()
    raise SystemExit(sys_exit)
