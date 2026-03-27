#!/usr/bin/env bash
# ─────────────────────────────────────────────────────────
#  KumOS — Fedora launcher & USB writer
#  Usage:
#    ./run.sh          → run in QEMU (recommended first step)
#    ./run.sh usb      → write ISO to a USB drive
#    ./run.sh build    → rebuild the OS from source
# ─────────────────────────────────────────────────────────

set -euo pipefail
ISO="$(dirname "$0")/kumos.iso"
RED='\033[0;31m'; GRN='\033[0;32m'; YEL='\033[1;33m'; NC='\033[0m'

die()  { echo -e "${RED}ERROR: $*${NC}" >&2; exit 1; }
info() { echo -e "${GRN}==>${NC} $*"; }
warn() { echo -e "${YEL}WARN: $*${NC}"; }

check_iso() {
    [[ -f "$ISO" ]] || die "kumos.iso not found next to this script."
}

# ── QEMU run ─────────────────────────────────────────────
cmd_run() {
    check_iso
    info "Checking for QEMU..."
    local qemu=""
    for q in qemu-system-i386 qemu-system-x86_64; do
        if command -v "$q" &>/dev/null; then qemu="$q"; break; fi
    done

    if [[ -z "$qemu" ]]; then
        info "Installing QEMU (requires sudo)..."
        sudo dnf install -y qemu-system-x86 || \
        sudo apt-get install -y qemu-system-x86 || \
        die "Could not install QEMU. Please install it manually: sudo dnf install qemu-system-x86"
        qemu="qemu-system-i386"
    fi

    info "Launching KumOS in QEMU..."
    echo ""
    echo "  Controls:"
    echo "    Ctrl+Alt+G  — release mouse"
    echo "    Ctrl+Alt+F  — toggle fullscreen"
    echo "    Close window or Ctrl+C — exit"
    echo ""

    # Try KVM first (much faster), fall back to software emulation
    if [[ -r /dev/kvm ]]; then
        info "KVM available — using hardware acceleration"
        "$qemu" -cdrom "$ISO" -m 32M -enable-kvm \
            -display sdl,gl=off 2>/dev/null || \
        "$qemu" -cdrom "$ISO" -m 32M -enable-kvm 2>/dev/null || \
        "$qemu" -cdrom "$ISO" -m 32M
    else
        warn "KVM not available — running in software mode (slower)"
        "$qemu" -cdrom "$ISO" -m 32M
    fi
}

# ── USB write ─────────────────────────────────────────────
cmd_usb() {
    check_iso
    echo ""
    warn "This will ERASE the selected USB drive completely!"
    echo ""

    # List removable block devices
    info "Detected removable drives:"
    lsblk -d -o NAME,SIZE,MODEL,TRAN | grep -E "usb|sd" | grep -v "^NAME" || true
    echo ""

    read -rp "Enter device name (e.g. sdb — WITHOUT /dev/): " DEV
    [[ -z "$DEV" ]] && die "No device specified."

    local DEVPATH="/dev/$DEV"
    [[ -b "$DEVPATH" ]] || die "$DEVPATH is not a block device."

    # Safety check — refuse to write to the system disk
    local ROOT_DEV
    ROOT_DEV=$(findmnt -n -o SOURCE / | sed 's/[0-9]*$//')
    [[ "$DEVPATH" == "$ROOT_DEV" ]] && die "Refusing to overwrite the system disk ($ROOT_DEV)!"

    echo ""
    warn "About to write kumos.iso → $DEVPATH"
    read -rp "Type YES to confirm: " CONFIRM
    [[ "$CONFIRM" == "YES" ]] || die "Aborted."

    # Unmount any partitions
    for part in "${DEVPATH}"?*; do
        if mountpoint -q "$part" 2>/dev/null; then
            info "Unmounting $part..."
            sudo umount "$part"
        fi
    done

    info "Writing ISO to $DEVPATH (this may take a minute)..."
    sudo dd if="$ISO" of="$DEVPATH" bs=4M status=progress oflag=sync
    sudo sync
    info "Done! You can now boot from $DEVPATH."
    echo ""
    echo "  On your target machine:"
    echo "  1. Insert the USB drive"
    echo "  2. Enter BIOS/UEFI (usually F2, F12, or Del at power-on)"
    echo "  3. Set USB as first boot device"
    echo "  4. Save and reboot"
}

# ── Build from source ─────────────────────────────────────
cmd_build() {
    local SRC="$(dirname "$0")"
    info "Building KumOS from source in $SRC..."

    # Check tools
    for tool in gcc nasm ld; do
        command -v "$tool" &>/dev/null || die "$tool not found. Install build tools."
    done

    for mkrescue in grub2-mkrescue grub-mkrescue; do
        if command -v "$mkrescue" &>/dev/null; then break; fi
    done

    if ! command -v grub2-mkrescue &>/dev/null && ! command -v grub-mkrescue &>/dev/null; then
        info "Installing build dependencies..."
        sudo dnf install -y gcc nasm grub2-tools-extra xorriso || \
        sudo apt-get install -y gcc nasm grub-pc-bin xorriso
    fi

    cd "$SRC"
    make clean
    make
    info "Build successful: kumos.iso"
}

# ── Dispatch ──────────────────────────────────────────────
echo ""
echo "  ██╗  ██╗██╗   ██╗███╗   ███╗ ██████╗ ███████╗"
echo "  ██║ ██╔╝██║   ██║████╗ ████║██╔═══██╗██╔════╝"
echo "  █████╔╝ ██║   ██║██╔████╔██║██║   ██║███████╗"
echo "  ██╔═██╗ ██║   ██║██║╚██╔╝██║██║   ██║╚════██║"
echo "  ██║  ██╗╚██████╔╝██║ ╚═╝ ██║╚██████╔╝███████║"
echo "  ╚═╝  ╚═╝ ╚═════╝ ╚═╝     ╚═╝ ╚═════╝ ╚══════╝"
echo "  KumOS v1.0 — Minimal C Kernel"
echo ""

case "${1:-run}" in
    run)   cmd_run ;;
    usb)   cmd_usb ;;
    build) cmd_build ;;
    *)
        echo "Usage: $0 [run|usb|build]"
        echo "  run   — boot KumOS in QEMU (default)"
        echo "  usb   — write ISO to a USB drive"
        echo "  build — rebuild from source"
        exit 1
        ;;
esac
