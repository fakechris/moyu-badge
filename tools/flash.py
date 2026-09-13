#!/usr/bin/env python3
"""Cross-platform firmware flash and utility tool for FoloToy AI Passport (ESP32-C3).

Works natively on Windows, macOS, and Linux.
Usage:
    python tools/flash.py               # flash build/deskpet-game.bin -> 0x10000
    python tools/flash.py settime       # sync device wall clock to now
    python tools/flash.py log [secs]    # monitor serial boot log
    python tools/flash.py backup [out]  # backup full 8MB flash
    python tools/flash.py restore <file> [-y]  # restore 8MB backup from 0x0
    python tools/flash.py idf           # full idf.py flash
"""

import argparse
import datetime
import hashlib
import os
import re
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
APP = ROOT / "build" / "deskpet-game.bin"
BUILD_TIME_H = ROOT / "build" / "build_time.h"
CHIP = "esp32c3"
FACTORY_ADDR = 0x10000
FACTORY_MAX = 0x300000   # 3 MB
FULL_SIZE = 0x800000     # 8 MB
DEFAULT_BAUD = 460800
def ensure_esp_python() -> None:
    """If current python lacks serial or esptool, re-exec using the ESP-IDF virtualenv python."""
    try:
        import serial
        import esptool
        return
    except ImportError:
        pass

    home = Path.home()
    # Support macOS/Linux (bin/python) and Windows (Scripts/python.exe)
    cand_pythons = sorted(list((home / ".espressif" / "python_env").glob("*/bin/python*")) + \
                          list((home / ".espressif" / "python_env").glob("*/Scripts/python*.exe")),
                          reverse=True)
    for py in cand_pythons:
        if py.is_file() and os.access(py, os.X_OK):
            try:
                res = subprocess.run([str(py), "-c", "import serial, esptool; print('OK')"],
                                     capture_output=True, text=True)
                if "OK" in res.stdout:
                    os.execv(str(py), [str(py)] + sys.argv)
            except Exception:
                pass

ensure_esp_python()


def log(msg: str) -> None:
    print(f"== {msg}")


def die(msg: str) -> None:
    print(f"ERROR: {msg}", file=sys.stderr)
    sys.exit(1)


def pick_esptool() -> list[str]:
    """Find a usable esptool invocation command."""
    # 1. Custom env var
    if "ESPTOOL_PY" in os.environ:
        return os.environ["ESPTOOL_PY"].split()
    # 2. Current python environment
    try:
        res = subprocess.run([sys.executable, "-m", "esptool", "version"],
                             capture_output=True, text=True)
        if res.returncode == 0:
            return [sys.executable, "-m", "esptool"]
    except Exception:
        pass
    # 3. Check PATH for esptool.py
    import shutil
    if shutil.which("esptool.py"):
        return ["esptool.py"]
    if shutil.which("esptool"):
        return ["esptool"]
    # 4. Check ~/.espressif python envs on macOS/Linux/Windows
    home = Path.home()
    cand_pythons = list((home / ".espressif" / "python_env").glob("*/bin/python*")) + \
                   list((home / ".espressif" / "python_env").glob("*/Scripts/python*.exe"))
    for py in cand_pythons:
        if py.is_file() and os.access(py, os.X_OK):
            try:
                res = subprocess.run([str(py), "-m", "esptool", "version"],
                                     capture_output=True, text=True)
                if res.returncode == 0:
                    return [str(py), "-m", "esptool"]
            except Exception:
                pass
    die("esptool not found. Please activate your ESP-IDF environment or install esptool:\n"
        "  pip install esptool")


def detect_port(explicit_port: str | None = None) -> str:
    """Find the native ESP32-C3 USB-Serial-JTAG device (VID 0x303a, PID 0x1001)."""
    if explicit_port:
        return explicit_port
    if "PORT" in os.environ and os.environ["PORT"].strip():
        return os.environ["PORT"].strip()

    try:
        import serial.tools.list_ports
        ports = list(serial.tools.list_ports.comports())
    except ImportError:
        die("pyserial is required for port auto-detection. Install it via: pip install pyserial")

    # Match by USB VID:PID
    matched = [p.device for p in ports if p.vid == 0x303A and p.pid == 0x1001]
    if not matched:
        # Fallback to description / hwid keywords
        for p in ports:
            desc = f"{p.description or ''} {p.hwid or ''}".lower()
            if "usb-serial-jtag" in desc or ("espressif" in desc and "cdc" in desc):
                matched.append(p.device)

    if len(matched) == 1:
        return matched[0]
    if len(matched) > 1:
        die(f"multiple Espressif devices detected: {matched}. Specify PORT explicitly.")

    available = [f"{p.device} ({p.description})" for p in ports]
    die("no Espressif USB-Serial-JTAG (0x303a:0x1001) found on the bus.\n"
        "  - plug the data cable, power on via its power button\n"
        f"  Available system ports: {available if available else '<none>'}")


def run_esptool(esptool_cmd: list[str], port: str, baud: int, extra_args: list[str]) -> subprocess.CompletedProcess:
    cmd = esptool_cmd + ["--chip", CHIP, "-p", port]
    if baud:
        cmd.extend(["-b", str(baud)])
    cmd.extend(extra_args)
    res = subprocess.run(cmd)
    if res.returncode != 0:
        die(f"esptool command failed with code {res.returncode}")
    return res


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        while chunk := f.read(65536):
            h.update(chunk)
    return h.hexdigest()


def deskpet_id(port: str) -> str | None:
    """Query device ID over serial."""
    import serial
    try:
        with serial.Serial(port, 115200, timeout=1) as s:
            s.reset_input_buffer()
            s.write(b"ID?\n")
            deadline = time.time() + 2
            out = b""
            while time.time() < deadline:
                chunk = s.read(64)
                out += chunk
                if b"DESKPET-ID" in out:
                    for line in out.decode(errors="replace").splitlines():
                        if "DESKPET-ID" in line:
                            return line.strip()
    except Exception:
        pass
    return None


def deskpet_settime(port: str) -> bool:
    """Send current UNIX epoch to device over serial."""
    import serial
    try:
        with serial.Serial(port, 115200, timeout=1) as s:
            s.reset_input_buffer()
            now_epoch = int(time.time())
            s.write(f"TIME {now_epoch}\n".encode())
            deadline = time.time() + 3
            out = b""
            while time.time() < deadline:
                out += s.read(64)
                if b"TIME OK" in out:
                    for line in out.decode(errors="replace").splitlines():
                        if "TIME OK" in line:
                            print(f"device clock: {line.strip()}")
                            return True
    except Exception:
        pass
    return False


def action_flash(args: argparse.Namespace) -> None:
    if not APP.is_file():
        die(f"{APP} not found; please build first: idf.py build")

    app_size = APP.stat().st_size
    if app_size > FACTORY_MAX:
        die(f"{APP} is {app_size} bytes, exceeding factory limit {FACTORY_MAX} (3MB)")

    port = detect_port(args.port)
    print(f"port: {port}")
    esptool_cmd = pick_esptool()

    log("chip sanity check")
    subprocess.run(esptool_cmd + ["--chip", CHIP, "-p", port, "-b", str(args.baud), "flash_id"])

    app_sha = sha256_file(APP)
    log(f"app: {APP} ({app_size} bytes, {app_size / 1048576:.2f} MB)")
    print(f"   sha256 {app_sha}")

    if BUILD_TIME_H.is_file():
        text = BUILD_TIME_H.read_text()
        m = re.search(r"(\d+)", text)
        if m:
            epoch = int(m.group(1))
            age = int(time.time()) - epoch
            beijing_time = datetime.datetime.fromtimestamp(epoch, datetime.timezone(datetime.timedelta(hours=8)))
            print(f"   clock seed: {beijing_time.strftime('%Y-%m-%d %H:%M:%S')} Beijing ({age // 60} min ago)")
            if age > 1800:
                print("   WARN: build is stale — rebuild so boot clock matches current wall time")

    log(f"writing factory ONLY at 0x{FACTORY_ADDR:X} (cardid/recovery untouched)")
    run_esptool(esptool_cmd, port, args.baud, ["write_flash", hex(FACTORY_ADDR), str(APP)])

    log("read-back verify")
    with tempfile.NamedTemporaryFile(delete=False) as tmp_file:
        tmp_path = Path(tmp_file.name)
    try:
        run_esptool(esptool_cmd, port, 921600, ["read_flash", hex(FACTORY_ADDR), str(app_size), str(tmp_path)])
        read_sha = sha256_file(tmp_path)
    finally:
        if tmp_path.exists():
            tmp_path.unlink()

    if read_sha != app_sha:
        die(f"read-back mismatch (wrote {app_sha}, read {read_sha})")

    # Time sync after reboot
    for i in range(5):
        time.sleep(1)
        if deskpet_settime(port):
            break
        if i == 4:
            print("   note: no TIME ack — build seed applies; retry: python tools/flash.py settime")

    print("FLASH OK: new firmware is live")


def action_settime(args: argparse.Namespace) -> None:
    port = detect_port(args.port)
    print(f"port: {port}")
    ident = deskpet_id(port)
    if not ident:
        die(f"no ID? ack on {port} — device running old firmware or not ready")
    print(f"deskpet unit: {ident}")
    for _ in range(5):
        if deskpet_settime(port):
            return
        time.sleep(1)
    die("deskpet answered ID but no TIME OK — reflash firmware")


def action_log(args: argparse.Namespace) -> None:
    port = detect_port(args.port)
    print(f"port: {port}")
    secs = int(args.seconds or 15)
    import serial
    print(f"listening on {port} for {secs} seconds...")
    try:
        with serial.Serial(port, 115200, timeout=1) as s:
            deadline = time.time() + secs
            while time.time() < deadline:
                line = s.readline()
                if line:
                    sys.stdout.write(line.decode(errors="replace"))
                    sys.stdout.flush()
    except Exception as e:
        die(f"serial monitor error: {e}")


def action_backup(args: argparse.Namespace) -> None:
    port = detect_port(args.port)
    print(f"port: {port}")
    esptool_cmd = pick_esptool()
    date_str = datetime.datetime.now().strftime("%Y%m%d")
    out_path = Path(args.output or (ROOT / ".." / "my-ai-passport" / "rom-backup" / f"backup-{date_str}-full-8MB.bin"))
    out_path.parent.mkdir(parents=True, exist_ok=True)
    log(f"reading {FULL_SIZE} bytes from 0x0 -> {out_path}")
    run_esptool(esptool_cmd, port, 921600, ["read_flash", "0x0", hex(FULL_SIZE), str(out_path)])
    if out_path.stat().st_size != FULL_SIZE:
        die("backup size mismatch")
    print(f"backup sha256: {sha256_file(out_path)}")
    print("keep this file — it is the safe way back to stock")


def action_restore(args: argparse.Namespace) -> None:
    backup_file = Path(args.file)
    if not backup_file.is_file():
        die(f"file not found: {backup_file}")
    if backup_file.stat().st_size != FULL_SIZE:
        die(f"file is not an 8MB full ROM backup ({backup_file.stat().st_size} bytes)")
    if not args.yes:
        confirm = input(f"Write {backup_file} over whole 8MB flash at 0x0? type yes: ")
        if confirm.strip() != "yes":
            die("aborted by user")
    port = detect_port(args.port)
    esptool_cmd = pick_esptool()
    log("restoring full ROM from 0x0")
    run_esptool(esptool_cmd, port, args.baud, ["write_flash", "0x0", str(backup_file)])
    log("verify against backup")
    with tempfile.NamedTemporaryFile(delete=False) as tmp_file:
        tmp_path = Path(tmp_file.name)
    try:
        run_esptool(esptool_cmd, port, 921600, ["read_flash", "0x0", hex(FULL_SIZE), str(tmp_path)])
        if sha256_file(tmp_path) == sha256_file(backup_file):
            print("RESTORE OK: reboot should land on the stock page")
        else:
            die("read-back mismatch — do not retry blindly; check cable and connections")
    finally:
        if tmp_path.exists():
            tmp_path.unlink()


def action_idf(args: argparse.Namespace) -> None:
    port = detect_port(args.port)
    cmd = ["idf.py", "-p", port, "flash"]
    subprocess.run(cmd, check=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("-p", "--port", help="Serial port (e.g. COM3 or /dev/cu.usbmodem*)")
    parser.add_argument("-b", "--baud", type=int, default=DEFAULT_BAUD, help=f"Baud rate (default: {DEFAULT_BAUD})")

    subparsers = parser.add_subparsers(dest="action")

    # flash
    subparsers.add_parser("flash", help="Flash factory partition (default)")

    # settime
    subparsers.add_parser("settime", help="Sync clock to device over serial")

    # log
    p_log = subparsers.add_parser("log", help="Monitor serial log")
    p_log.add_argument("seconds", nargs="?", default=15, help="Monitor duration in seconds")

    # backup
    p_backup = subparsers.add_parser("backup", help="Read 8MB ROM backup")
    p_backup.add_argument("output", nargs="?", help="Output file path")

    # restore
    p_restore = subparsers.add_parser("restore", help="Restore 8MB ROM backup")
    p_restore.add_argument("file", help="Path to backup.bin")
    p_restore.add_argument("-y", "--yes", action="store_true", help="Skip prompt")

    # idf
    subparsers.add_parser("idf", help="Run full idf.py flash")

    args = parser.parse_args()
    action = args.action or "flash"

    actions = {
        "flash": action_flash,
        "settime": action_settime,
        "log": action_log,
        "backup": action_backup,
        "restore": action_restore,
        "idf": action_idf,
    }

    if action not in actions:
        die(f"unknown action: {action}")
    actions[action](args)


if __name__ == "__main__":
    main()
