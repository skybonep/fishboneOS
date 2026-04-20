#!/bin/bash
set -euo pipefail

ISO_IMAGE=${1:-build/dist/fishbone.iso}
LOG_PATH=${2:-build/test.log}
TIMEOUT=${TIMEOUT:-5}

if [ ! -f "$ISO_IMAGE" ]; then
    echo "ERROR: ISO image not found: $ISO_IMAGE" >&2
    exit 2
fi

mkdir -p "$(dirname "$LOG_PATH")"
rm -f "$LOG_PATH"

echo "Running fishboneOS tests against $ISO_IMAGE"
echo "Writing serial log to $LOG_PATH"

timeout "${TIMEOUT}s" qemu-system-i386 \
    -cdrom "$ISO_IMAGE" \
    -serial file:"$LOG_PATH" \
    -display none \
    -no-reboot 2>/dev/null || true

# Give QEMU a moment to flush the serial output file.
sleep 0.2

echo "=== Captured serial markers ==="
grep -E 'fishboneOS booting|\[TEST_' "$LOG_PATH" || true
echo "=== End captured markers ==="

PASS_COUNT=$(grep -c '^\[TEST_PASS\]' "$LOG_PATH" || true)
FAIL_COUNT=$(grep -c '^\[TEST_FAIL\]' "$LOG_PATH" || true)
BOOT_COUNT=$(grep -c 'fishboneOS booting' "$LOG_PATH" || true)

if [ "$BOOT_COUNT" -eq 0 ]; then
    echo "ERROR: Expected boot message 'fishboneOS booting' not found in serial output." >&2
    exit 1
fi

if [ "$FAIL_COUNT" -gt 0 ]; then
    echo "ERROR: $FAIL_COUNT test failure(s) found." >&2
    exit 1
fi

echo "OK: boot message found, $PASS_COUNT pass(es), $FAIL_COUNT failure(s)."
exit 0
