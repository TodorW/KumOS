#!/bin/bash

# GDB Setup Script for ZephyrOS

# Make it ext
chmod +x $0

# Check directory (backdirectory) // 
cd - 

# Default configuration options
DEFAULT_PREFIX="/usr/local/cross"
DEFAULT_TARGET="i686-unknown-linux-gnu"  # Change this to your target architecture
GDB_SRC_DIR="/testing_debugging_tools"

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

# Clone and set up GDB
setup_gdb() {
    log "Setting up GDB..."
    if [ ! -d "$GDB_SRC_DIR" ]; then
        git clone https://sourceware.org/git/binutils-gdb.git "$GDB_SRC_DIR"
    fi

    cd "$GDB_SRC_DIR" || error "Failed to change directory to $GDB_SRC_DIR."

    # Configure GDB
    log "Configuring GDB..."
    ./configure --prefix="$DEFAULT_PREFIX" --target="$DEFAULT_TARGET"
    [ $? -ne 0 ] && error "GDB configuration failed!"

    # Build GDB
    log "Building GDB..."
    make -j$(nproc)
    [ $? -ne 0 ] && error "GDB build failed!"

    # Install GDB
    log "Installing GDB..."
    make install
    [ $? -ne 0 ] && error "GDB installation failed!"

    log "GDB installed successfully."
}

# Main Execution
main() {
    log "Starting GDB setup..."

    # Check for necessary tools
    check_tools

    # Setup GDB
    setup_gdb

    log "GDB setup completed successfully."
}

# Run the main function
main "$@"
