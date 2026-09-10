#!/usr/bin/env python3
"""Protocol regressions for the slimmed-down DeskPet BLE sender.

状态采集/归并已委托 planofplan(那边有 TS 单测);这里只回归:
  1. 52 字节 GATT 包布局(含 CJK UTF-8 截断);
  2. planofplan JSON -> 槽位 dict 的映射(空槽丢弃/禁源过滤);
  3. 空槽清屏语义。
"""
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "sender"))

from deskpet_sender import build_update, slots_from_api, updates_for_slots  # noqa: E402


def test_packet_and_utf8():
    view = {"source": "codex", "name": "中文标题很长", "state": 2,
            "progress": 255, "text": "等待你的确认继续执行"}
    packet = build_update(1, view)
    assert len(packet) == 52
    slot, source, state, progress = struct.unpack("<BBBB", packet[:4])
    assert (slot, source, state, progress) == (1, 0, 2, 255)
    packet[4:20].rstrip(b"\0").decode("utf-8")
    packet[20:52].rstrip(b"\0").decode("utf-8")
    print("sender_packet_utf8 OK")


def test_api_payload_mapping():
    payload = {"slots": [
        {"slot": 0, "occupied": True, "source": "zcode", "name": "zcode",
         "state": 1, "stateName": "working", "progress": 255, "text": "turn live"},
        {"slot": 1, "occupied": True, "source": "amp", "name": "amp",
         "state": 1, "stateName": "working", "progress": 255, "text": "process alive"},
        {"slot": 2, "occupied": False, "source": "", "name": "",
         "state": 0, "stateName": "idle", "progress": 0, "text": ""},
        {"slot": 3, "occupied": True, "source": "claude", "name": "proj",
         "state": 2, "stateName": "needs-you", "progress": 255, "text": "approve"},
    ]}
    slots = slots_from_api(payload, disabled={"amp"})
    assert set(slots) == {0, 3}          # 空槽丢弃 + 禁源过滤
    assert slots[3]["state"] == 2
    assert slots_from_api(payload, set()) == {
        0: payload["slots"][0], 1: payload["slots"][1], 3: payload["slots"][3]}
    print("sender_api_mapping OK")


def test_empty_slots_are_cleared():
    one = {"source": "opencode", "name": "session", "state": 3,
           "progress": 100, "text": "done"}
    packets = updates_for_slots({0: one})
    assert len(packets) == 4 and all(len(p) == 52 for p in packets)
    assert [p[0] for p in packets] == [0, 1, 2, 3]
    assert packets[0][2] == 3
    assert all(p[2] == 0 and p[3] == 0 for p in packets[1:])
    print("sender_slot_clear OK")


if __name__ == "__main__":
    test_packet_and_utf8()
    test_api_payload_mapping()
    test_empty_slots_are_cleared()
    print("ALL SENDER TESTS PASS")
