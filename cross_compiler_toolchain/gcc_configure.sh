#!/bin/bash

# Make it ext
chmod +x $0

# Check directory (backdirectory) // 
cd - 

# Custom Configuration Options
DEFAULT_PREFIX="/usr/local/cross"
DEFAULT_TARGET="i686-unknown-linux-gnu"  # Change this to your target architecture
SRC_DIR="/cross_compiler_toolchain"
BUILD_DIR="gcc-build"

# Logging Function
log() {
    echo "[INFO] $(date '+%Y-%m-%d %H:%M:%S') - $1"
}

error() {
    echo "[ERROR] $(date '+%Y-%m-%d %H:%M:%S') - $1" >&2
    exit 1
}

# Feature Detection
detect_features() {
    log "Detecting system features..."
    # Check for necessary tools
    command -v make >/dev/null 2>&1 || error "make is required but not installed."
    command -v gcc >/dev/null 2>&1 || error "gcc is required but not installed."
}

# Interactive Configuration
interactive_config() {
    read -p "Enter installation prefix [$DEFAULT_PREFIX]: " PREFIX
    PREFIX=${PREFIX:-$DEFAULT_PREFIX}

    read -p "Enter target architecture [$DEFAULT_TARGET]: " TARGET
    TARGET=${TARGET:-$DEFAULT_TARGET}
}

# Default Configuration Profiles
load_default_profile() {
    log "Loading default configuration profile..."
    PREFIX=$DEFAULT_PREFIX
    TARGET=$DEFAULT_TARGET
}

# Version Compatibility Checks
check_version() {
    log "Checking for compatible versions..."
    # Insert version checks for dependencies here if needed
}

# Environment Variable Support
set_environment_vars() {
    export PATH="$PREFIX/bin:$PATH"
    log "Environment variables set."
}

# Post-Configuration Actions
post_configuration() {
    log "Running post-configuration actions..."
    # Any additional setup can be done here
}

# Main Script Execution
main() {
    log "Starting GCC configuration..."

    # Detect features
    detect_features

    # Load default profile or interactive config
    if [ "$1" == "--interactive" ]; then
        interactive_config
    else
        load_default_profile
    fi

    # Check version compatibility
    check_version

    # Create build directory
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR" || error "Failed to change directory to $BUILD_DIR."

    # Configure GCC
    log "Configuring GCC for target $TARGET..."
    "$SRC_DIR/configure" --prefix="$PREFIX" --target="$TARGET" --enable-languages=c,c++ --disable-multilib
    [ $? -ne 0 ] && error "Configuration failed!"

    # Build
    log "Building GCC..."
    make -j$(nproc)
    [ $? -ne 0 ] && error "Build failed!"

    # Install
    log "Installing GCC..."
    make install
    [ $? -ne 0 ] && error "Installation failed!"

    # Set environment variables
    set_environment_vars

    # Post-configuration actions
    post_configuration

    log "GCC installed successfully."
}

# Run the main function
main "$@"
