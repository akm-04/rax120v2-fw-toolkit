#!/usr/bin/env bash
#
# unpack_rootfs.sh <rootfs.squashfs> <output_dir>
#
# Extracts a squashfs rootfs blob (e.g. out/03_rootfs_payload.bin from
# unpack.sh) into a directory of loose files, ready for editing and later
# feeding to build_rootfs.sh to repack.
#
# Extraction uses unsquashfs (vendored in bin/ if RAX120_BIN_DIR is set by
# rax120-toolkit.sh, else falls back to host PATH -- see _lib_toolpath.sh).
# NOT binwalk/sasquatch: this device's stock rootfs reads cleanly with a
# format-matching unsquashfs build (confirmed via `unsquashfs -s` -- no
# "error reading stored compressor options"), which makes it both simpler
# and more trustworthy than a heuristic extractor. 
#
# Wrapped in `fakeroot -s <state>` because unsquashfs can't create real
# device nodes without root 
# The saved state is picked up by build_rootfs.sh's matching
# `fakeroot -i` on repack so device nodes survive the round trip instead of
# silently downgrading to empty regular files.
#
set -euo pipefail

C_RESET='\033[0m'; C_BOLD='\033[1m'; C_GREEN='\033[1;32m'
C_YELLOW='\033[1;33m'; C_RED='\033[1;31m'; C_CYAN='\033[1;36m'; C_MAGENTA='\033[1;35m'; C_DIM='\033[2m'
info()  { echo -e "${C_CYAN}==>${C_RESET} ${1}"; }
ok()    { echo -e "${C_GREEN}✓${C_RESET} ${1}"; }
warn()  { echo -e "${C_YELLOW}!${C_RESET} ${1}"; }
fail()  { echo -e "${C_RED}✗ ${1}${C_RESET}"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
# shellcheck source=_lib_toolpath.sh
source "$SCRIPT_DIR/_lib_toolpath.sh"

echo -e "${C_BOLD}${C_MAGENTA}"
echo " __  __            _   _   _       ____             _   __"
echo "|  \\/  | ___  _   _| \\ | |_| |_    |  _ \\ ___   ___ | |_/ _|___"
echo "| |\\/| |/ _ \\| | | |  \\| | __|     | |_) / _ \\ / _ \\| __| |_/ __|"
echo "| |  | | (_) | |_| | |\\  | |_      |  _ < (_) | (_) | |_|  _\\__ \\\\"
echo "|_|  |_|\\___/ \\__,_|_| \\_|\\__|     |_| \\_\\___/ \\___/ \\__|_| |___/"
echo -e "${C_RESET}"
echo -e "${C_DIM}Rootfs squashfs -> editable tree (via unsquashfs)${C_RESET}"
echo

SRC="${1:?Usage: unpack_rootfs.sh <rootfs.squashfs> <output_dir>}"
OUT="${2:?Missing output_dir argument}"

[ -f "$SRC" ] || fail "$SRC not found"

info "Resolving unsquashfs"
resolve_tool UNSQUASHFS unsquashfs4 unsquashfs

info "Verifying squashfs magic"
magic=$(dd if="$SRC" bs=1 count=4 2>/dev/null | od -An -tx1 | tr -d ' \n')
[ "$magic" = "68737173" ] || fail "$SRC does not start with squashfs magic ('hsqs'), got: $magic"
ok "squashfs magic confirmed"

info "Superblock info"
if ! "$UNSQUASHFS" -s "$SRC" 2>&1 | sed 's/^/    /'; then
    fail "$UNSQUASHFS couldn't read the superblock. If this is the host fallback" \
         "(see warnings above), that mismatch is the known issue -- get the" \
         "vendored unsquashfs into \$RAX120_BIN_DIR and re-run."
fi

if command -v binwalk >/dev/null 2>&1; then
    info "binwalk signature cross-check (informational only, not used for extraction)"
    binwalk "$SRC" 2>&1 | sed 's/^/    /' || warn "binwalk scan failed -- not fatal, purely informational"
else
    warn "binwalk not on PATH -- skipping cross-check (not required, extraction doesn't use it)"
fi

[ -e "$OUT" ] && fail "$OUT already exists — refusing to overwrite. Remove it first or pick a new path."

# Sibling state file, not nested inside $OUT -- keeps it out of the tree
# build_rootfs.sh will later mksquashfs (a stray file inside $OUT would get
# packed into the rootfs image itself).
FAKEROOT_STATE="${OUT%/}.fakeroot.state"
rm -f "$FAKEROOT_STATE"

info "Extracting via $UNSQUASHFS (fakeroot -s, saving state to $FAKEROOT_STATE)"
UNSQUASHFS_LOG="$(mktemp)"
fakeroot -s "$FAKEROOT_STATE" -- "$UNSQUASHFS" -d "$OUT" "$SRC" 2>&1 | tee "$UNSQUASHFS_LOG"
ok "Extracted to $OUT"

info "Quick sanity check on extracted tree"
file_count=$(find "$OUT" -type f | wc -l)
link_count=$(find "$OUT" -type l | wc -l)
dangling_count=$(find "$OUT" -xtype l 2>/dev/null | wc -l)
# NOTE: deliberately NOT `find -type c/-type b` here. Verified empirically:
# find's -type test uses the real on-disk d_type from getdents() as a fast
# path, which fakeroot cannot intercept (unlike stat()/lstat(), which it
# does) -- so find reports the true underlying type (a 0-byte regular file)
# even when run under a matching `fakeroot -i`, silently giving a false "0
# device nodes" reading no matter what. unsquashfs's own summary line
# (captured above, from inside the one session with accurate visibility) is
# the reliable source for this count.
dev_count=$(grep -oE 'created [0-9]+ devices?' "$UNSQUASHFS_LOG" | grep -oE '[0-9]+' | head -1)
dev_count="${dev_count:-0}"
rm -f "$UNSQUASHFS_LOG"
echo -e "    ${C_DIM}regular files: $file_count${C_RESET}"
echo -e "    ${C_DIM}symlinks:       $link_count (of which $dangling_count dangling — expected for overlay-pattern files like /etc/passwd, see repo README)${C_RESET}"
echo -e "    ${C_DIM}device nodes:   $dev_count (per unsquashfs's own summary above -- on disk these look like empty${C_RESET}"
echo -e "    ${C_DIM}                regular files outside a fakeroot session; that's fakeroot's faked state, not${C_RESET}"
echo -e "    ${C_DIM}                corruption. build_rootfs.sh's matching 'fakeroot -i $FAKEROOT_STATE' is what${C_RESET}"
echo -e "    ${C_DIM}                restores them correctly on repack.)${C_RESET}"
if [ "$dev_count" -eq 0 ]; then
    warn "0 device nodes found. Either this rootfs genuinely has none (fine), or" 
    warn "something upstream silently dropped them -- worth a second look if you" 
    warn "expected e.g. /dev/console to be present."
fi

echo
ok "Done. Edit files under $OUT/, then repack with:"
echo -e "    ${C_CYAN}./build_rootfs.sh <label>${C_RESET}          # rebuild squashfs from $OUT/"
echo -e "    ${C_DIM}(point build_rootfs.sh's SRC_DIR at $OUT if it's not already 'squashfs-root')${C_RESET}"
