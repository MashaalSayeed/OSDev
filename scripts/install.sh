#!/bin/bash
DISK_IMAGE="./iso/zdisk.img"
MOUNT_POINT="/Volumes/VirtDisk"
USER_BIN="./build/i386/user/bin"
RESOURCES_DIR="./resources"

# Attach the disk image and get the device name
DEVICE=$(hdiutil attach -nomount "$DISK_IMAGE" | awk '{print $1}')

echo "DEVICE: $DEVICE"
if [ -z "$DEVICE" ]; then
    echo "Failed to attach disk image."
    exit 1
fi

# Mount the first partition (adjust if needed)
sudo mkdir -p "$MOUNT_POINT"
sudo mount -t msdos -o rw "$DEVICE" "$MOUNT_POINT"

# Copy the file
mkdir -p "$MOUNT_POINT/bin"
# cp "$FILE_TO_COPY/user.bin" "$MOUNT_POINT/bin/shell"
cp -r "$USER_BIN" "$MOUNT_POINT"
cp -r "$RESOURCES_DIR" "$MOUNT_POINT/RES/"

# # Unmount and detach
sync
sudo umount "$MOUNT_POINT"
hdiutil detach "$DEVICE"

echo "File copied successfully to $DISK_IMAGE"
