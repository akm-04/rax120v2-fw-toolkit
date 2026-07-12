#!/bin/bash

set -euo pipefail

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <source-directory> <patches-directory>"
    exit 1
fi

SRC_DIR="$1"
PATCH_DIR="$2"

if [ ! -d "$SRC_DIR" ]; then
    echo "Error: Source directory does not exist: $SRC_DIR"
    exit 1
fi

if [ ! -d "$PATCH_DIR" ]; then
    echo "Error: Patches directory does not exist: $PATCH_DIR"
    exit 1
fi

mapfile -t PATCHES < <(find "$PATCH_DIR" -maxdepth 1 -name '*.patch' -print | sort -V)

if [ "${#PATCHES[@]}" -eq 0 ]; then
    echo "No .patch files found in: $PATCH_DIR"
    exit 0
fi

echo "Found ${#PATCHES[@]} patch(es) in $PATCH_DIR"
echo "Source directory: $SRC_DIR"
echo ""

cd "$SRC_DIR"
FAILED=0

for patchfile in "${PATCHES[@]}"; do
    patchname=$(basename "$patchfile")
    echo -n "Applying $patchname ... "

    if ! patch -p1 --dry-run -i "$patchfile" > /dev/null 2>&1; then
        if patch -p1 --dry-run --reverse -i "$patchfile" > /dev/null 2>&1; then
            echo "ALREADY APPLIED (skipping)"
            continue
        fi
        echo "FAILED (dry-run)"
        echo "  -> Re-run with: patch -p1 -i '$patchfile' --verbose"
        ((FAILED++))
        continue
    fi

    if patch -p1 -i "$patchfile" > /dev/null 2>&1; then
        echo "OK"
    else
        echo "FAILED"
        ((FAILED++))
    fi
done

echo ""
if [ "$FAILED" -eq 0 ]; then
    echo "All patches applied successfully."
    exit 0
else
    echo "$FAILED patch(es) failed to apply."
    exit 1
fi
