#!/bin/bash
set -e

echo "Listing dirs"
ls -l
echo "Listing $EXT_MOUNT"
ls -l $EXT_MOUNT

# Install coverage dependencies
apt update && apt install -y gcovr lcov

# Install Rust
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y
source "$HOME/.cargo/env"

cmake \
    -B build \
    -S "$EXT_MOUNT" \
    -G Ninja \
    -DEVC_ENABLE_CCACHE=1 \
    -DISO15118_2_GENERATE_AND_INSTALL_CERTIFICATES=OFF \
    -DBUILD_TESTING=ON \
    -DEVEREST_ENABLE_RS_SUPPORT=ON

ninja -j$(nproc) -C build && ninja test -C build
