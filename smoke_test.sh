#!/bin/bash
set -u

LOG=$(mktemp /tmp/kumos-smoke-XXXXXX.log)
DISK=$(mktemp /tmp/kumos-smoke-disk-XXXXXX.img)
cp disk.img "$DISK"

timeout 20s qemu-system-x86_64 \
    $([ -r /dev/kvm ] && echo "-enable-kvm") \
    -boot order=d -cdrom kumos.iso -drive file="$DISK",format=raw,if=ide -drive file=ext2.img,format=raw,if=ide \
    -m 128M -display none -serial file:"$LOG" -no-reboot >/dev/null 2>&1

rm -f "$DISK"

if grep -q "Shell started" "$LOG"; then
    echo "PASS: kernel booted to shell"
    if grep -qi "panic\|triple fault\|PAGE FAULT.*KERNEL" "$LOG"; then
        echo "WARN: boot log also contains a fault/panic string, check $LOG"
    fi
    rm -f "$LOG"
    exit 0
else
    echo "FAIL: kernel did not reach the shell prompt within 20s"
    echo "--- boot log ($LOG) ---"
    cat "$LOG"
    exit 1
fi
