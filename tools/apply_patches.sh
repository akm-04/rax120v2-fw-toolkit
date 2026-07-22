#!/usr/bin/env bash

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

# Resolve to absolute BEFORE the `cd "$SRC_DIR"` below -- otherwise a
# relative PATCH_DIR (very natural to pass, e.g. `apply_patches.sh
# squashfs-root mods` from repo root) silently breaks: the patch file
# paths found here would still be relative to the ORIGINAL cwd, but every
# `patch -p1 -i "$patchfile"` call below runs from inside $SRC_DIR, so
# they'd resolve to the wrong place and every patch would report
# "FAILED (dry-run)" even when the patch itself is completely valid.
PATCH_DIR="$(cd "$PATCH_DIR" && pwd)"

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
        FAILED=$((FAILED + 1))
        continue
    fi

    if patch -p1 -i "$patchfile" > /dev/null 2>&1; then
        echo "OK"
    else
        echo "FAILED"
        FAILED=$((FAILED + 1))
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
