#!/bin/bash
DISK_IMAGE="./iso/zdisk.img"
USER_BIN="./build/i386/user/bin"
RESOURCES_DIR="./resources"

export MTOOLS_SKIP_CHECK=1

# Recreate the disk image fresh
dd if=/dev/zero of="$DISK_IMAGE" bs=512 count=131072
mkfs.fat -F 32 -s 8 -S 512 "$DISK_IMAGE"

# Create directories
mmd -i "$DISK_IMAGE" ::BIN
mmd -i "$DISK_IMAGE" ::HOME
mmd -i "$DISK_IMAGE" ::RES

# Copy binaries with explicit uppercase short names
for f in "$USER_BIN"/*; do
    name=$(basename "$f" | tr '[:lower:]' '[:upper:]')
    echo "Copying $f -> ::BIN/$name"
    mcopy -i "$DISK_IMAGE" "$f" "::BIN/$name"
done

# Copy resources
for f in "$RESOURCES_DIR"/*; do
    name=$(basename "$f" | tr '[:lower:]' '[:upper:]')
    echo "Copying $f -> ::RES/$name"
    mcopy -i "$DISK_IMAGE" "$f" "::RES/$name"
done

# Doom looks for its IWAD in the current directory first, so mirror doom.wad into /HOME.
if [ -f "$RESOURCES_DIR/doom.wad" ]; then
    echo "Copying $RESOURCES_DIR/doom.wad -> ::HOME/DOOM.WAD"
    mcopy -i "$DISK_IMAGE" "$RESOURCES_DIR/doom.wad" "::HOME/DOOM.WAD"
fi

# Copy home files if any
if [ -d "./home" ]; then
    for f in ./home/*; do
        name=$(basename "$f" | tr '[:lower:]' '[:upper:]')
        mcopy -i "$DISK_IMAGE" "$f" "::HOME/$name"
    done
fi

echo "Disk image created successfully: $DISK_IMAGE"
echo "--- Disk contents ---"
mdir -i "$DISK_IMAGE" ::BIN
mdir -i "$DISK_IMAGE" ::HOME
