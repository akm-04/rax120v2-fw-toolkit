#!/usr/bin/env bash
#
# unpack_rootfs.sh <rootfs.squashfs> <output_dir>
#
# Extracts a squashfs rootfs blob (e.g. out/03_rootfs_payload.bin from
# unpack.sh) into a directory of loose files, ready for editing and later
# feeding to build_rootfs.sh to repack.
#
# Uses binwalk's bundled sasquatch extractor, not stock unsquashfs — this
# vendor's mksquashfs writes a non-standard compressor-options block that
# stock unsquashfs rejects ("error reading stored compressor options").
#
set -euo pipefail

C_RESET='\033[0m'; C_BOLD='\033[1m'; C_GREEN='\033[1;32m'
C_YELLOW='\033[1;33m'; C_RED='\033[1;31m'; C_CYAN='\033[1;36m'; C_MAGENTA='\033[1;35m'; C_DIM='\033[2m'
info()  { echo -e "${C_CYAN}==>${C_RESET} ${1}"; }
ok()    { echo -e "${C_GREEN}✓${C_RESET} ${1}"; }
warn()  { echo -e "${C_YELLOW}!${C_RESET} ${1}"; }
fail()  { echo -e "${C_RED}✗ ${1}${C_RESET}"; exit 1; }

echo -e "${C_BOLD}${C_MAGENTA}"
echo " __  __            _   _   _       ____             _   __"
echo "|  \\/  | ___  _   _| \\ | |_| |_    |  _ \\ ___   ___ | |_/ _|___"
echo "| |\\/| |/ _ \\| | | |  \\| | __|     | |_) / _ \\ / _ \\| __| |_/ __|"
echo "| |  | | (_) | |_| | |\\  | |_      |  _ < (_) | (_) | |_|  _\\__ \\\\"
echo "|_|  |_|\\___/ \\__,_|_| \\_|\\__|     |_| \\_\\___/ \\___/ \\__|_| |___/"
echo -e "${C_RESET}"
echo -e "${C_DIM}Rootfs squashfs -> editable tree (via binwalk/sasquatch)${C_RESET}"
echo

SRC="${1:?Usage: unpack_rootfs.sh <rootfs.squashfs> <output_dir>}"
OUT="${2:?Missing output_dir argument}"

[ -f "$SRC" ] || fail "$SRC not found"
command -v binwalk >/dev/null 2>&1 || fail "binwalk not found on PATH"

info "Verifying squashfs magic"
magic=$(dd if="$SRC" bs=1 count=4 2>/dev/null | od -An -tx1 | tr -d ' \n')
[ "$magic" = "68737173" ] || fail "$SRC does not start with squashfs magic ('hsqs'), got: $magic"
ok "squashfs magic confirmed"

info "Superblock info (via unsquashfs -s, informational — extraction itself uses binwalk)"
unsquashfs -s "$SRC" 2>&1 | sed 's/^/    /' || warn "unsquashfs -s reported an error (expected — vendor compressor-options quirk); continuing with binwalk"

[ -e "$OUT" ] && fail "$OUT already exists — refusing to overwrite. Remove it first or pick a new path."

info "Extracting via binwalk (sasquatch)"
WORKDIR=$(mktemp -d)
cp "$SRC" "$WORKDIR/rootfs.squashfs"
( cd "$WORKDIR" && binwalk -e rootfs.squashfs > .binwalk.log 2>&1 ) || { cat "$WORKDIR/.binwalk.log"; fail "binwalk extraction failed"; }

EXTRACTED=$(find "$WORKDIR" -type d -name squashfs-root | head -1)
[ -n "$EXTRACTED" ] || fail "binwalk ran but no squashfs-root directory was produced — check $WORKDIR/.binwalk.log"

mkdir -p "$(dirname "$OUT")"
mv "$EXTRACTED" "$OUT"
rm -rf "$WORKDIR"

ok "Extracted to $OUT"

info "Quick sanity check on extracted tree"
file_count=$(find "$OUT" -type f | wc -l)
link_count=$(find "$OUT" -type l | wc -l)
dangling_count=$(find "$OUT" -xtype l 2>/dev/null | wc -l)
echo -e "    ${C_DIM}regular files: $file_count${C_RESET}"
echo -e "    ${C_DIM}symlinks:       $link_count (of which $dangling_count dangling — expected for overlay-pattern files like /etc/passwd, see repo README)${C_RESET}"

echo
ok "Done. Edit files under $OUT/, then repack with:"
echo -e "    ${C_CYAN}./build_rootfs.sh <label>${C_RESET}          # rebuild squashfs from $OUT/"
echo -e "    ${C_DIM}(point build_rootfs.sh's SRC_DIR at $OUT if it's not already 'squashfs-root')${C_RESET}"