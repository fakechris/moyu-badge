#!/usr/bin/env bash
# Flash deskpet-game firmware onto FoloToy AI Passport (ESP32-C3, 8MB flash).
#
# Method proven on this device across two sessions:
#   - 2026-09-04: hardware check + full 8MB ROM backup
#     (my-ai-passport/FLASH_GUIDE.md); backup + SHA256 live in
#     ../my-ai-passport/rom-backup/
#   - 2026-09-05: first firmware flash, `idf.py flash` at mainline 01ebd8f —
#     wrote bootloader/partition-table/factory, hash verified, cardid and
#     recovery untouched, clean boot ("DeskPet standalone boot" -> "pet ready")
# Since that first flash the device runs OUR partition table (partitions.csv),
# so routine updates only need the factory partition:
#   - default action writes ONLY factory @ 0x10000 — minimal write, verified
#     by reading the partition back and comparing SHA256
#   - cardid @ 0x356000 (device identity) and recovery @ 0x700000 are NEVER
#     written or erased by this script; whole-flash erase is forbidden
#     (upstream ai-passport/docs/development/engineering/ble-recovery-compatibility.md:62-70)
# macOS has no `timeout`; boot-log capture uses the idf.py monitor stdin
# commands (reset + sleep) instead.
#
# Device prerequisites: data (not charge-only) cable; device powered on via
# its power button and resting on a normal screen (USB drops out during the
# few seconds of deep-sleep); browser flashing pages closed (they hold the
# port exclusively).
#
# Usage:
#   tools/flash.sh                     # flash build/deskpet-game.bin -> 0x10000
#   tools/flash.sh idf                 # full `idf.py flash` (first-flash path)
#   tools/flash.sh log [seconds]       # capture boot log via IDF monitor
#   tools/flash.sh settime             # set device wall clock from this Mac
#   tools/flash.sh backup [out.bin]    # read full 8MB ROM (writes nothing)
#   tools/flash.sh restore FILE [-y]   # write 8MB backup from 0x0 + verify
#
# Env: PORT (serial device), BAUD (default 460800), ESPTOOL_PY (python with
# the esptool module), IDF_EXPORT (default ~/esp/esp-idf-v5.5.3/export.sh).
set -euo pipefail
cd "$(dirname "$0")/.."

APP=build/deskpet-game.bin
CHIP=esp32c3
FACTORY_ADDR=0x10000
FACTORY_MAX=$((0x300000))   # factory partition is 3MB
FULL_SIZE=$((0x800000))     # 8MB
BAUD=${BAUD:-460800}
BACKUP_DIR=../my-ai-passport/rom-backup
MAC_SUFFIX=${MAC_SUFFIX:-4c11ae30b778}

log() { echo "== $*"; }
die() { echo "ERROR: $*" >&2; exit 1; }

pick_esptool() {
  local cands=() p
  [ -n "${ESPTOOL_PY:-}" ] && cands+=("$ESPTOOL_PY")
  for p in ~/.espressif/python_env/*/bin/python ~/esp-tools/bin/python; do
    [ -x "$p" ] && cands+=("$p")
  done
  local c
  for c in "${cands[@]}"; do
    if "$c" -m esptool version >/dev/null 2>&1; then
      echo "$c -m esptool"
      return 0
    fi
  done
  if command -v esptool.py >/dev/null 2>&1; then
    echo "esptool.py"
    return 0
  fi
  die "no esptool found. One-time setup:
  python3 -m venv ~/esp-tools && ~/esp-tools/bin/pip install esptool"
}

detect_port() {
  # Zero-write identification using standard USB descriptors only:
  # the deskpet's native ESP32-C3 USB enumerates as Espressif VID 0x303a +
  # USB-Serial-JTAG PID 0x1001 — same for EVERY unit running this ROM, so
  # nothing is hardcoded per device. We require exactly one such device on
  # the bus and exactly one usbmodem port before touching anything; the
  # app-level "ID?" handshake (device reports its own MAC) tells units apart
  # afterwards. Override: PORT=/dev/cu.usbmodemXXX
  if [ -z "${PORT:-}" ]; then
    local prof n_usj ports n_dev
    prof=$(system_profiler SPUSBDataType 2>/dev/null)
    n_usj=$(printf '%s\n' "$prof" | awk -v RS='' \
      '/Vendor ID: 0x303a/ && /Product ID: 0x1001/' | wc -l | tr -d ' ')
    ports=$(ls /dev/cu.usbmodem* 2>/dev/null || true)
    n_dev=$(echo "${ports:-0}" | wc -l | tr -d ' ')
    [ "$n_usj" -ge 1 ] || die "no Espressif USB-Serial-JTAG (0x303a:0x1001) on the bus.
  - plug the data cable, power on via its power button"
    [ "$n_dev" -eq 1 ] || die "$n_usj deskpet-class USB device(s), $n_dev usbmodem port(s) — ambiguous; set PORT explicitly:
  ${ports:-<none>}"
    PORT=$(echo "$ports" | head -1)
  fi
  echo "port: $PORT"
}

require_port() { detect_port; }

ESPTOOL=$(pick_esptool)
run_esptool() { $ESPTOOL --chip $CHIP -p "$PORT" "$@"; }

sha() { shasum -a 256 "$1" | cut -d' ' -f1; }

# Serial console helpers. Every exchange starts with an ID? handshake: the
# deskpet answers "DESKPET-ID <mac>"; a /dev/cu.usbmodem* belonging to some
# other device stays silent, so we never send commands to the wrong port.
deskpet_py() {
  local py="${ESPTOOL_PY%% *}"
  [ -n "$py" ] || py="${ESPTOOL%% *}"   # fallback: the auto-picked esptool python
  echo "$py"
}

deskpet_id() {  # $1 = port; prints the ID line, 0 if it answered
  "$(deskpet_py)" - "$1" <<'PY'
import serial, sys, time
s = serial.Serial(sys.argv[1], 115200, timeout=1)
s.reset_input_buffer()
s.write(b"ID?\n")
deadline, out = time.time() + 2, b""
while time.time() < deadline:
    out += s.read(64)
    if b"DESKPET-ID" in out:
        for ln in out.decode(errors="replace").splitlines():
            if "DESKPET-ID" in ln:
                print(ln.strip())
        sys.exit(0)
sys.exit(1)
PY
}

deskpet_settime() {  # $1 = port (already identified); prints ack line, 0 on ok
  "$(deskpet_py)" - "$1" <<'PY'
import serial, sys, time
s = serial.Serial(sys.argv[1], 115200, timeout=1)
s.reset_input_buffer()
s.write(f"TIME {int(time.time())}\n".encode())
deadline, out = time.time() + 3, b""
while time.time() < deadline:
    out += s.read(64)
    if b"TIME OK" in out:
        print("device clock:", out.decode(errors="replace").strip().splitlines()[-1])
        sys.exit(0)
sys.exit(1)
PY
}

action=${1:-flash}

case "$action" in
idf)
  # First-flash / full path: bootloader + partition table + factory, exactly
  # what already ran successfully on this device at 01ebd8f.
  require_port
  source "${IDF_EXPORT:-$HOME/esp/esp-idf-v5.5.3/export.sh}" >/dev/null 2>&1
  idf.py -p "$PORT" flash 2>&1 | tail -6
  ;;

log)
  # Capture a fresh boot: monitor's `reset` command restarts the target, the
  # background sleep closes the pipe after SECS so this works without a TTY
  # (macOS has no `timeout`).
  require_port
  SECS=${2:-15}
  source "${IDF_EXPORT:-$HOME/esp/esp-idf-v5.5.3/export.sh}" >/dev/null 2>&1
  OUT=${BOOTLOG:-/tmp/deskpet-bootlog.txt}
  (echo reset; sleep "$SECS") | idf.py -p "$PORT" monitor > "$OUT" 2>&1 || true
  echo "boot log: $OUT"
  grep -aE "deskpet|demo_deskpet|app_main|pet ready|wall clock|agent|Guru|panic|abort|assert|E \([0-9]" "$OUT" | head -20 || true
  [ -s "$OUT" ] || die "empty capture — is the device on and the port free?"
  ;;

settime)
  require_port   # descriptor-verified port; nothing below touches other ports
  IDOUT=$(deskpet_id "$PORT" 2>/dev/null) \
    || die "no ID? ack on $PORT — old firmware (reflash) or app not running"
  echo "deskpet unit: $IDOUT"
  for i in 1 2 3 4 5; do
    if OUT=$(deskpet_settime "$PORT" 2>/dev/null); then
      echo "$OUT"
      exit 0
    fi
    sleep 1
  done
  die "deskpet answered ID but no TIME OK — firmware too old? reflash"
  ;;

flash)
  [ -f "$APP" ] || die "$APP not found; run: source ~/esp/esp-idf-v5.5.3/export.sh && idf.py build"
  SIZE=$(stat -f%z "$APP")
  [ "$SIZE" -le "$FACTORY_MAX" ] || die "$APP is $SIZE bytes, over the 3MB factory limit"
  require_port

  log "chip sanity check"
  run_esptool -b "$BAUD" flash_id 2>&1 | grep -E "Chip type|Detected flash|MAC" || true

  log "app: $APP ($SIZE bytes, $(awk -v s="$SIZE" 'BEGIN{printf "%.2f", s/1048576}') MB)"
  echo "   sha256 $(sha "$APP")"
  # The boot wall clock is seeded from build time (no RTC/NTP) — show its age.
  if [ -f build/build_time.h ]; then
    EPOCH=$(grep -oE '[0-9]+' build/build_time.h | head -1)
    if [ -n "$EPOCH" ]; then
      AGE=$(( $(date +%s) - EPOCH ))
      echo "   clock seed: $(TZ=Asia/Shanghai date -r "$EPOCH" '+%F %T') Beijing ($((AGE / 60)) min ago)"
      [ "$AGE" -le 1800 ] || echo "   WARN: build is stale — run idf.py build so boot clock matches wall time"
    fi
  fi

  log "writing factory ONLY at $FACTORY_ADDR (cardid/recovery untouched)"
  run_esptool -b "$BAUD" write_flash "$FACTORY_ADDR" "$APP" 2>&1 | tail -3

  log "read-back verify"
  TMP=$(mktemp /tmp/deskpet-verify.XXXXXX.bin)
  run_esptool -b 921600 read_flash "$FACTORY_ADDR" "$SIZE" "$TMP" >/dev/null 2>&1
  if [ "$(sha "$TMP")" = "$(sha "$APP")" ]; then
    rm -f "$TMP"
    # The device reset itself after writing — set exact wall time over serial.
    for i in 1 2 3 4 5; do
      if OUT=$(deskpet_settime "$PORT" 2>/dev/null); then echo "   $OUT"; break; fi
      [ "$i" = 5 ] && echo "   note: no TIME ack — build seed applies; retry: tools/flash.sh settime"
      sleep 1
    done
    echo "FLASH OK: new firmware is live"
  else
    die "read-back mismatch — restore the full ROM backup (tools/flash.sh restore)"
  fi
  ;;

backup)
  require_port
  OUT=${2:-$BACKUP_DIR/backup-$(date +%Y%m%d)-$MAC_SUFFIX-full-8MB.bin}
  mkdir -p "$(dirname "$OUT")"
  log "reading $FULL_SIZE bytes from 0x0 (writes nothing) -> $OUT"
  run_esptool -b 921600 read_flash 0x0 "$FULL_SIZE" "$OUT" 2>&1 | tail -2
  [ "$(stat -f%z "$OUT")" -eq "$FULL_SIZE" ] || die "backup size wrong"
  echo "backup sha256: $(sha "$OUT")"
  echo "keep this file — it is the only way back to stock"
  ;;

restore)
  BACKUP=${2:?usage: tools/flash.sh restore <backup.bin> [-y]}
  [ -f "$BACKUP" ] || die "no such file: $BACKUP"
  [ "$(stat -f%z "$BACKUP")" -eq "$FULL_SIZE" ] || die "not an 8MB full backup"
  require_port
  if [ "${3:-}" != "-y" ]; then
    read -r -p "Write $BACKUP over the whole 8MB flash at 0x0? type yes: " ok
    [ "$ok" = "yes" ] || die "aborted"
  fi
  log "restoring full ROM from 0x0"
  run_esptool -b "$BAUD" write_flash 0x0 "$BACKUP" 2>&1 | tail -3
  log "verify against backup"
  TMP=$(mktemp /tmp/deskpet-restore.XXXXXX.bin)
  run_esptool -b 921600 read_flash 0x0 "$FULL_SIZE" "$TMP" >/dev/null 2>&1
  if [ "$(sha "$TMP")" = "$(sha "$BACKUP")" ]; then
    rm -f "$TMP"
    echo "RESTORE OK: reboot should land on the stock QR page"
  else
    die "read-back mismatch — do not retry blindly; check cable/port and redo restore"
  fi
  ;;

*)
  die "unknown action '$action'; use flash (default), idf, log, settime, backup, restore"
  ;;
esac
