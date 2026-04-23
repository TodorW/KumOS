set -e

BOLD='\033[1m'
GREEN='\033[0;32m'
CYAN='\033[0;36m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m'

banner() {
    echo -e "${CYAN}"
    echo "  ██╗  ██╗██╗   ██╗███╗   ███╗ ██████╗ ███████╗"
    echo "  ██║ ██╔╝██║   ██║████╗ ████║██╔═══██╗██╔════╝"
    echo "  █████╔╝ ██║   ██║██╔████╔██║██║   ██║███████╗"
    echo "  ██╔═██╗ ██║   ██║██║╚██╔╝██║██║   ██║╚════██║"
    echo "  ██║  ██╗╚██████╔╝██║ ╚═╝ ██║╚██████╔╝███████║"
    echo "  ╚═╝  ╚═╝ ╚═════╝ ╚═╝     ╚═╝ ╚═════╝ ╚══════╝"
    echo -e "${NC}"
    echo -e "  ${YELLOW}Minimal x86 Operating System — KumLabs 2025${NC}"
    echo ""
}

log()  { echo -e "${GREEN}[✓]${NC} $1"; }
info() { echo -e "${CYAN}[i]${NC} $1"; }
warn() { echo -e "${YELLOW}[!]${NC} $1"; }
err()  { echo -e "${RED}[✗]${NC} $1"; exit 1; }

banner

# ── Detect distro & install deps ──────────────────────────────────────────────
install_deps() {
    info "Detecting package manager..."
    if command -v dnf &>/dev/null; then
        info "Fedora/RHEL detected. Installing dependencies..."
        sudo dnf install -y nasm gcc gcc-c++ make grub2-tools-extra \
            xorriso qemu-system-x86 2>/dev/null || \
        sudo dnf install -y nasm gcc make grub2-tools xorriso qemu-kvm
    elif command -v apt-get &>/dev/null; then
        info "Debian/Ubuntu detected. Installing dependencies..."
        sudo apt-get install -y nasm gcc gcc-multilib make \
            grub-pc-bin grub-common xorriso mtools qemu-system-x86
    elif command -v pacman &>/dev/null; then
        info "Arch detected. Installing dependencies..."
        sudo pacman -Sy --noconfirm nasm gcc make grub xorriso qemu
    else
        warn "Unknown distro. Make sure you have: nasm gcc make grub-mkrescue xorriso qemu-system-x86_64"
    fi
    log "Dependencies ready."
}

# ── Find GCC freestanding includes ────────────────────────────────────────────
find_gcc_inc() {
    local inc
    inc=$(find /usr/lib/gcc -name "stddef.h" 2>/dev/null | head -1)
    if [ -z "$inc" ]; then
        # Try cross-compiler paths
        inc=$(find /usr -name "stddef.h" 2>/dev/null | grep -i "gcc\|freestanding" | head -1)
    fi
    if [ -z "$inc" ]; then
        err "Cannot find gcc freestanding headers (stddef.h). Install gcc-multilib."
    fi
    dirname "$inc"
}

# ── Build ──────────────────────────────────────────────────────────────────────
build() {
    local GCC_INC
    GCC_INC=$(find_gcc_inc)
    info "Using GCC includes: $GCC_INC"

    local CFLAGS="-m32 -ffreestanding -O2 -Wall -fno-stack-protector -fno-builtin -nostdlib -nostdinc -I${GCC_INC} -Isrc"

    log "Assembling boot entry..."
    nasm -f elf32 boot/boot.asm -o boot/boot.o

    log "Compiling kernel sources..."
    for f in src/vga.c src/keyboard.c src/kstring.c src/kmalloc.c src/process.c src/fs.c src/kernel.c; do
        echo "   -> $f"
        gcc $CFLAGS -c "$f" -o "${f%.c}.o"
    done

    log "Linking kernel..."
    ld -m elf_i386 -T linker.ld -o kumos.bin \
        boot/boot.o \
        src/kstring.o src/vga.o src/keyboard.o src/kmalloc.o \
        src/process.o src/fs.o src/kernel.o

    local SIZE
    SIZE=$(du -sh kumos.bin | cut -f1)
    log "Kernel built: kumos.bin (${SIZE})"
}

# ── Create ISO ─────────────────────────────────────────────────────────────────
make_iso() {
    log "Building bootable ISO..."
    mkdir -p iso/boot/grub
    cp kumos.bin iso/boot/kumos.bin
    cp grub.cfg  iso/boot/grub/grub.cfg

    if command -v grub2-mkrescue &>/dev/null; then
        grub2-mkrescue -o kumos.iso iso/ 2>/dev/null
    elif command -v grub-mkrescue &>/dev/null; then
        grub-mkrescue -o kumos.iso iso/ 2>/dev/null
    else
        err "grub-mkrescue not found. Install grub2-tools."
    fi

    local SIZE
    SIZE=$(du -sh kumos.iso | cut -f1)
    log "ISO built: kumos.iso (${SIZE})"
}

# ── Run in QEMU ───────────────────────────────────────────────────────────────
run_qemu() {
    if ! command -v qemu-system-x86_64 &>/dev/null; then
        warn "qemu-system-x86_64 not found. Install qemu-system-x86 or qemu-kvm."
        info "To boot manually: dd if=kumos.iso of=/dev/sdX (USB) or burn to CD"
        return
    fi

    info "Launching KumOS in QEMU..."
    echo -e "${YELLOW}  Controls: Ctrl+Alt+G = release mouse | Ctrl+Alt+Q = quit${NC}"
    echo ""

    # Try KVM acceleration first (much faster), fall back to TCG
    if [ -r /dev/kvm ]; then
        qemu-system-x86_64 \
            -enable-kvm \
            -cdrom kumos.iso \
            -m 128M \
            -vga std \
            -name "KumOS" \
            -no-reboot
    else
        warn "KVM not available (no /dev/kvm access). Running in TCG mode (slower)."
        qemu-system-x86_64 \
            -cdrom kumos.iso \
            -m 128M \
            -vga std \
            -name "KumOS" \
            -no-reboot
    fi
}

# ── Write to USB ──────────────────────────────────────────────────────────────
write_usb() {
    local dev="$1"
    if [ -z "$dev" ]; then
        echo ""
        echo -e "${YELLOW}Available block devices:${NC}"
        lsblk -d -o NAME,SIZE,TYPE,TRAN | grep -v "loop\|rom"
        echo ""
        read -rp "Enter USB device (e.g. /dev/sdb): " dev
    fi
    if [ ! -b "$dev" ]; then
        err "Device $dev not found or not a block device."
    fi
    echo -e "${RED}WARNING: This will ERASE $dev. Are you sure? (yes/no)${NC}"
    read -r confirm
    if [ "$confirm" != "yes" ]; then
        warn "Aborted."
        return
    fi
    log "Writing KumOS to $dev ..."
    sudo dd if=kumos.iso of="$dev" bs=4M status=progress oflag=sync
    log "Done! You can now boot from $dev"
}

# ── Clean ─────────────────────────────────────────────────────────────────────
clean() {
    rm -f boot/boot.o src/*.o kumos.bin kumos.iso
    rm -f iso/boot/kumos.bin
    log "Cleaned build artifacts."
}

# ── Main ──────────────────────────────────────────────────────────────────────
case "${1:-all}" in
    deps)    install_deps ;;
    build)   build ;;
    iso)     build && make_iso ;;
    run)     build && make_iso && run_qemu ;;
    usb)     write_usb "$2" ;;
    clean)   clean ;;
    all|"")
        build
        make_iso
        echo ""
        echo -e "${GREEN}╔══════════════════════════════════════════╗${NC}"
        echo -e "${GREEN}║        KumOS built successfully!         ║${NC}"
        echo -e "${GREEN}╚══════════════════════════════════════════╝${NC}"
        echo ""
        echo -e "  ${BOLD}Run in QEMU:${NC}    ./build.sh run"
        echo -e "  ${BOLD}Write to USB:${NC}   ./build.sh usb /dev/sdX"
        echo -e "  ${BOLD}ISO file:${NC}       $(pwd)/kumos.iso"
        echo ""
        ;;
    *)
        echo "Usage: $0 {deps|build|iso|run|usb [device]|clean|all}"
        exit 1
        ;;
esac
