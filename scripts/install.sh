#!/bin/bash
DISK_IMAGE="./iso/zdisk.img"
MOUNT_POINT="/Volumes/VirtDisk"
FILE_TO_COPY="./build/i386/user/user.bin"

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
cp "$FILE_TO_COPY" "$MOUNT_POINT/user.bin"

# # Unmount and detach
sync
sudo umount "$MOUNT_POINT"
hdiutil detach "$DEVICE"

echo "File copied successfully to $DISK_IMAGE"
