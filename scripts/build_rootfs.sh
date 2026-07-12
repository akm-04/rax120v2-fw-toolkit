#!/usr/bin/env bash
#
# build_rootfs.sh
# Repacks ./squashfs-root into a timestamped .squashfs image under ./out/
#
# Usage:
#   ./build_rootfs.sh              # auto-labelled build
#   ./build_rootfs.sh no-armor     # custom label, e.g. rootfs_20260708-142301_no-armor.squashfs
#
set -euo pipefail

# ---- colours ----------------------------------------------------------
C_RESET='\033[0m'
C_BOLD='\033[1m'
C_GREEN='\033[1;32m'
C_YELLOW='\033[1;33m'
C_RED='\033[1;31m'
C_CYAN='\033[1;36m'
C_MAGENTA='\033[1;35m'
C_DIM='\033[2m'

info()  { echo -e "${C_CYAN}==>${C_RESET} ${1}"; }
ok()    { echo -e "${C_GREEN}✓${C_RESET} ${1}"; }
warn()  { echo -e "${C_YELLOW}!${C_RESET} ${1}"; }
fail()  { echo -e "${C_RED}✗ ${1}${C_RESET}"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
# shellcheck source=_lib_toolpath.sh
source "$SCRIPT_DIR/_lib_toolpath.sh"

# ---- config -------------------------------------------------------------
SRC_DIR="${SRC_DIR_OVERRIDE:-squashfs-root}"
OUT_DIR="${OUT_DIR_OVERRIDE:-out}"
LABEL="${1:-}"
TIMESTAMP="$(date +%Y%m%d-%H%M%S)"

# Known-good params, confirmed directly against the stock image's own
# superblock (unsquashfs -s: "Compression xz", "Block size 262144") once a
# format-matching unsquashfs build was used to read it -- not just measured
# via a round-trip that a mismatched build could report misleadingly.
COMP="xz"
BLOCK_SIZE="262144"

# mtd26 "rootfs" partition size (from /proc/mtd: 05de0000) — hard ceiling.
# nandwrite will happily write past a sane size and corrupt whatever
# follows in flash if you let it, so we check before you ever get that far.
PARTITION_SIZE_BYTES=$((0x05de0000))
# Warn a bit before the hard ceiling so there's headroom for bad-block
# remapping / future growth, not just a last-byte panic.
WARN_THRESHOLD_PCT=90

# ---- sanity checks --------------------------------------------------------
echo -e "${C_BOLD}${C_CYAN}"
echo "  ____             _   __        ____  ______ ____  "
echo " |  _ \\ ___   ___ | |_ / _|___   |  _ \\|  ____/ ___| "
echo " | |_) / _ \\ / _ \\| __| |_/ __|  | |_) | |__  \\___ \\ "
echo " |  _ < (_) | (_) | |_|  _\\__ \\  |  _ <|  __|  ___) |"
echo " |_| \\_\\___/ \\___/ \\__|_| |___/  |_| \\_\\_|    |____/ "
echo -e "${C_RESET}"
echo -e "${C_DIM}RAX120v2 squashfs rebuild — mksquashfs wrapper${C_RESET}"
echo

info "Resolving mksquashfs / unsquashfs"
resolve_tool MKSQUASHFS mksquashfs4 mksquashfs
resolve_tool UNSQUASHFS unsquashfs4 unsquashfs

[ -d "$SRC_DIR" ] || fail "Source dir '$SRC_DIR' not found. Run this from the repo root."

mkdir -p "$OUT_DIR"

if [ -n "$LABEL" ]; then
    OUT_NAME="rootfs_${TIMESTAMP}_${LABEL}.squashfs"
else
    OUT_NAME="rootfs_${TIMESTAMP}.squashfs"
fi
OUT_PATH="${OUT_DIR}/${OUT_NAME}"

info "Source:      ${SRC_DIR}/"
info "Output:      ${OUT_PATH}"
info "Compression: ${COMP}, block size ${BLOCK_SIZE}, all-root, no-xattrs"
echo

# --- FAKEROOT STATE (device node fidelity) ----------------------------------
# Uses the same resolve_fakeroot_state logic unpack_rootfs.sh.

resolve_fakeroot_state FAKEROOT_STATE "${SRC_DIR%/}.fakeroot.state"
if [ -f "$FAKEROOT_STATE" ]; then
    ok "Found matching fakeroot state: $FAKEROOT_STATE"
    FAKEROOT_ARGS=(-i "$FAKEROOT_STATE")
else
    warn "No fakeroot state at $FAKEROOT_STATE for this source dir."
    warn "Building without it -- any device nodes in $SRC_DIR/ that only exist"
    warn "as fakeroot-faked state elsewhere will NOT be preserved correctly."
    FAKEROOT_ARGS=()
fi
# -----------------------------------------------------------------------

# ---- build ----------------------------------------------------------------
info "Running mksquashfs..."
if fakeroot "${FAKEROOT_ARGS[@]}" -- "$MKSQUASHFS" "$SRC_DIR" "$OUT_PATH" \
    -comp "$COMP" \
    -b "$BLOCK_SIZE" \
    -noappend \
    -all-root \
    -no-xattrs; then
    ok "Build complete"
else
    fail "mksquashfs failed"
fi
echo

# ---- verify -----------------------------------------------------------------
info "Verifying superblock..."
"$UNSQUASHFS" -s "$OUT_PATH" | sed 's/^/    /' || warn "unsquashfs -s reported an issue reading back the build (unexpected for a freshly-built image — investigate)"
echo

SIZE_BYTES=$(stat -c%s "$OUT_PATH")
SIZE_MB=$(awk "BEGIN {printf \"%.2f\", ${SIZE_BYTES}/1024/1024}")
SHA256=$(sha256sum "$OUT_PATH" | awk '{print $1}')

echo "$SHA256  $OUT_NAME" >> "${OUT_DIR}/checksums.sha256"

# pointer to the most recent build
ln -sf "$OUT_NAME" "${OUT_DIR}/rootfs_latest.squashfs"

echo
ok "Image:    ${OUT_PATH}"
ok "Size:     ${SIZE_BYTES} bytes (${SIZE_MB} MB)"
ok "SHA256:   ${SHA256}"
ok "Latest ->  ${OUT_DIR}/rootfs_latest.squashfs"
echo

# ---- partition size guard --------------------------------------------------
PART_MB=$(awk "BEGIN {printf \"%.2f\", ${PARTITION_SIZE_BYTES}/1024/1024}")
PCT_USED=$(awk "BEGIN {printf \"%.1f\", (${SIZE_BYTES}/${PARTITION_SIZE_BYTES})*100}")

info "mtd26 'rootfs' partition budget check..."
echo "    partition size : ${PARTITION_SIZE_BYTES} bytes (${PART_MB} MB)"
echo "    image size     : ${SIZE_BYTES} bytes (${SIZE_MB} MB)"
echo "    usage          : ${PCT_USED}%"

if [ "$SIZE_BYTES" -gt "$PARTITION_SIZE_BYTES" ]; then
    echo
    echo -e "${C_RED}${C_BOLD}✗✗✗ IMAGE EXCEEDS mtd26 PARTITION SIZE ✗✗✗${C_RESET}"
    echo -e "${C_RED}    Image is $((SIZE_BYTES - PARTITION_SIZE_BYTES)) bytes too large.${C_RESET}"
    echo -e "${C_RED}    DO NOT flash this image — the write will fail once it hits${C_RESET}"
    echo -e "${C_RED}    the mtd26 partition boundary, leaving a partially-written,${C_RESET}"
    echo -e "${C_RED}    corrupt rootfs. Recovery (NMRP) would be required.${C_RESET}"
    echo
    echo "    This build is left in ${OUT_DIR}/ for inspection, but treat it"
    echo "    as unflashable until it's back under budget (trim added files,"
    echo "    check for accidental duplication, re-verify -comp/-b match)."
    exit 1
elif awk "BEGIN {exit !(${PCT_USED} >= ${WARN_THRESHOLD_PCT})}"; then
    warn "Image is at ${PCT_USED}% of partition capacity (threshold: ${WARN_THRESHOLD_PCT}%)."
    warn "Still technically fits, but there's very little headroom left."
else
    ok "Within budget (${PCT_USED}% of partition capacity)."
fi
echo

warn "Reminder: mtd26 'rootfs' partition is 0x5de0000 (~93.87 MB) — plenty of headroom."
warn "Always md5sum/sha256 verify on-device (read-back from mtd26) BEFORE rebooting."
