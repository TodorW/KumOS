#!/bin/bash

# Toolchain Setup Script for ZephyrOS

# Make it ext
chmod +x $0

# Check directory (backdirectory) // 
cd - 

# Default configuration options
DEFAULT_PREFIX="/usr/local/cross"
DEFAULT_TARGET="i686-unknown-linux-gnu"  # Change this to your target architecture
BINUTILS_SRC_DIR="/cross_compiler_toolchain"
GCC_SRC_DIR="/cross_compiler_toolchain"

# Logging Functions
log() {
    echo "[INFO] $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

error() {
    echo "[ERROR] $(date '+%Y-%m-%d %H:%M:%S') - $1" >&2
    exit 1
}

# Check for necessary tools
check_tools() {
    command -v git >/dev/null 2>&1 || error "git is required but not installed."
    command -v make >/dev/null 2>&1 || error "make is required but not installed."
    command -v gcc >/dev/null 2>&1 || error "gcc is required but not installed."
}

# Clone and set up binutils
setup_binutils() {
    log "Setting up binutils..."
    if [ ! -d "$BINUTILS_SRC_DIR" ]; then
        git clone https://sourceware.org/git/binutils-gdb.git "$BINUTILS_SRC_DIR"
    fi

    cd "$BINUTILS_SRC_DIR" || error "Failed to change directory to $BINUTILS_SRC_DIR."
    
    # Call binutils configuration script
    ./binutils_configure.sh
}

# Clone and set up GCC
setup_gcc() {
    log "Setting up GCC..."
    if [ ! -d "$GCC_SRC_DIR" ]; then
        git clone https://gcc.gnu.org/git/gcc.git "$GCC_SRC_DIR"
    fi

    cd "$GCC_SRC_DIR" || error "Failed to change directory to $GCC_SRC_DIR."
    
    # Call GCC configuration script
    ./gcc_configure.sh
}

# Main Execution
main() {
    log "Starting toolchain setup..."

    # Check for necessary tools
    check_tools

    # Setup binutils and GCC
    setup_binutils
    setup_gcc

    log "Toolchain setup completed successfully."
}

# Run the main function
main "$@"
