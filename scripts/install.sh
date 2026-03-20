# #!/bin/bash
# DISK_IMAGE="./iso/zdisk.img"
# MOUNT_POINT="/Volumes/VirtDisk"
# USER_BIN="./build/i386/user/bin"
# RESOURCES_DIR="./resources"

# # Attach the disk image and get the device name
# DEVICE=$(hdiutil attach -nomount "$DISK_IMAGE" | awk '{print $1}')

# echo "DEVICE: $DEVICE"
# if [ -z "$DEVICE" ]; then
#     echo "Failed to attach disk image."
#     exit 1
# fi

# # Mount the first partition (adjust if needed)
# sudo mkdir -p "$MOUNT_POINT"
# sudo mount -t msdos -o rw "$DEVICE" "$MOUNT_POINT"

# # Copy the file
# mkdir -p "$MOUNT_POINT/bin"
# # cp "$FILE_TO_COPY/user.bin" "$MOUNT_POINT/bin/shell"
# cp -r "$USER_BIN" "$MOUNT_POINT"
# cp -r "$RESOURCES_DIR" "$MOUNT_POINT/RES/"

# # # Unmount and detach
# sync
# sudo umount "$MOUNT_POINT"
# hdiutil detach "$DEVICE"

# echo "File copied successfully to $DISK_IMAGE"
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
