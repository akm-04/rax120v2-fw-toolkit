#!/usr/bin/env bash
#
# unpack.sh <firmware.img> [output_dir]
#
# v3 — fixes two issues found via round-trip testing:
#   1. Alignment now computed from fit_end (128 + totalsize), not totalsize
#      alone — avoids a latent off-by-one-BLOCKSIZE bug near boundary edges.
#   2. Rootfs payload is sliced to the EXACT length declared in the original
#      uImage header (not "everything to EOF"), catching trailing-byte
#      anomalies instead of silently absorbing them.
#
# Adds check_fw(): a preflight validator that checks every invariant BEFORE
# any dd slicing happens, and aborts loudly on mismatch rather than
# producing a subtly-wrong unpack you only discover via cmp -l later.
#
set -euo pipefail

# ---- colours (matches build_rootfs.sh / repack.sh) -------------------------
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
err()   { echo -e "${C_RED}✗ ${1}${C_RESET}" >&2; }
fail()  { echo -e "${C_RED}✗ ${1}${C_RESET}" >&2; exit 1; }

echo -e "${C_BOLD}${C_MAGENTA}"
echo "  _   _ _   _ ____   _    ____ _  __"
echo " | | | | \\ | |  _ \\ / \\  / ___| |/ /"
echo " | | | |  \\| | |_) / _ \\| |   | ' / "
echo " | |_| | |\\  |  __/ ___ \\ |___| . \\ "
echo "  \\___/|_| \\_|_| /_/   \\_\\____|_|\\_\\"
echo -e "${C_RESET}"
echo -e "${C_DIM}RAX120v2 firmware unpack — chop point: FIT/kernel vs uImage-wrapped rootfs${C_RESET}"
echo

IMG="${1:?Usage: unpack.sh <firmware.img> [output_dir]}"
OUT="${2:-tmp}"
BLOCKSIZE=$((128 * 1024))   # confirmed from ipq807x.mk netgear_rax120v2 device profile

[ -f "$IMG" ] || fail "$IMG not found"

# ---- helpers ---------------------------------------------------------------
read_be32() {   # read_be32 <file> <offset>  -> decimal value
    dd if="$1" bs=1 skip="$2" count=4 status=none | od -An -tu4 --endian=big | tr -d ' '
}
read_hex() {    # read_hex <file> <offset> <count>
    dd if="$1" bs=1 skip="$2" count="$3" status=none | od -An -tx1 | tr -d ' \n'
}
align_up() { local v=$1 b=$2; echo $(( ( (v + b - 1) / b ) * b )); }

# ---- check_fw: validate every invariant before touching anything ----------
check_fw() {
    local img="$1"
    local errors=0

    info "check_fw: validating $img"

    local total_size; total_size=$(stat -c%s "$img")
    echo -e "    ${C_DIM}file size: $total_size bytes${C_RESET}"

    local magic; magic=$(read_hex "$img" 128 4)
    if [ "$magic" != "d00dfeed" ]; then
        err "FIT magic mismatch at offset 128: got $magic, expected d00dfeed"
        errors=$((errors+1))
    else
        ok "FIT magic OK"
    fi

    local totalsize; totalsize=$(read_be32 "$img" 132)
    local fit_end=$((128 + totalsize))
    if [ "$totalsize" -lt 1000000 ] || [ "$totalsize" -gt 20000000 ]; then
        err "FIT totalsize ($totalsize) outside sane range [1MB, 20MB] — suspicious"
        errors=$((errors+1))
    else
        ok "FIT totalsize sane: $totalsize bytes"
    fi

    local aligned; aligned=$(align_up "$fit_end" "$BLOCKSIZE")
    local hdr_off=$((aligned + 64))
    local data_off=$((hdr_off + 64))

    if [ "$data_off" -ge "$total_size" ]; then
        err "computed rootfs data offset ($data_off) is past EOF ($total_size)"
        errors=$((errors+1))
    fi

    # cross-check against binwalk if available — treat mismatch as fatal, not a warning
    if command -v binwalk >/dev/null 2>&1; then
        local bw_offset
        bw_offset=$(binwalk "$img" 2>/dev/null | awk '/uImage/{print $1; exit}') || true
        if [ -n "${bw_offset:-}" ]; then
            if [ "$bw_offset" = "$hdr_off" ]; then
                ok "binwalk agrees: uImage header at $hdr_off"
            else
                err "binwalk says uImage at $bw_offset, computed $hdr_off — MISMATCH"
                errors=$((errors+1))
            fi
        else
            warn "binwalk found no uImage signature — cannot cross-check (not fatal, just less certain)"
        fi
    else
        warn "binwalk not available — skipping cross-check (install it for stronger validation)"
    fi

    # validate the rootfs uImage header actually decodes
    local hdr_magic; hdr_magic=$(read_hex "$img" "$hdr_off" 4)
    if [ "$hdr_magic" != "27051956" ]; then
        err "rootfs uImage magic mismatch at $hdr_off: got $hdr_magic, expected 27051956"
        errors=$((errors+1))
    else
        ok "rootfs uImage magic OK"
    fi

    local declared_size; declared_size=$(read_be32 "$img" $((hdr_off + 12)))
    local available=$((total_size - data_off))
    echo -e "    ${C_DIM}header declares data size: $declared_size bytes${C_RESET}"
    echo -e "    ${C_DIM}bytes actually available to EOF: $available bytes${C_RESET}"
    if [ "$declared_size" -gt "$available" ]; then
        err "header declares MORE data than exists in the file — truncated image?"
        errors=$((errors+1))
    elif [ "$declared_size" -lt "$available" ]; then
        warn "$((available - declared_size)) trailing byte(s) beyond declared data size —"
        echo -e "      ${C_DIM}will slice payload to the declared length, not EOF. Trailing bytes captured${C_RESET}"
        echo -e "      ${C_DIM}separately for inspection, not silently discarded.${C_RESET}"
    else
        ok "declared size matches available bytes exactly"
    fi

    if [ "$errors" -gt 0 ]; then
        echo
        err "check_fw: $errors check(s) FAILED — refusing to unpack. Fix before proceeding."
        return 1
    fi

    ok "all checks passed"
    # export computed values for the caller
    FW_TOTAL_SIZE=$total_size
    FW_TOTALSIZE=$totalsize
    FW_ALIGNED=$aligned
    FW_HDR_OFFSET=$hdr_off
    FW_DATA_OFFSET=$data_off
    FW_DECLARED_SIZE=$declared_size
    FW_TRAILING_BYTES=$((available - declared_size))
    FW_TIMESTAMP=$(read_be32 "$img" $((hdr_off + 8)))
    return 0
}

# ---- run preflight, abort on any failure -----------------------------------
if ! check_fw "$IMG"; then
    exit 1
fi

echo
info "Preflight passed. Slicing $IMG into $OUT/"
mkdir -p "$OUT"

# Clear any stale artifacts from a previous run into this same dir — otherwise
# e.g. a leftover 04_trailing_bytes.bin from a DIFFERENT firmware version could
# silently get reattached by repack.sh later, belonging to the wrong image.
rm -f "$OUT"/00_netgear_header.bin "$OUT"/01_fit_kernel_dtb.bin \
      "$OUT"/02_rootfs_uimage_header.bin "$OUT"/03_rootfs_payload.bin \
      "$OUT"/04_trailing_bytes.bin "$OUT"/info.txt "$OUT"/rootfs_header_decoded.txt

dd if="$IMG" of="$OUT/00_netgear_header.bin"       bs=1 skip=0                    count=128                   status=none
dd if="$IMG" of="$OUT/01_fit_kernel_dtb.bin"       bs=1 skip=128                  count=$((FW_HDR_OFFSET-128)) status=none
dd if="$IMG" of="$OUT/02_rootfs_uimage_header.bin" bs=1 skip=$FW_HDR_OFFSET       count=64                    status=none
dd if="$IMG" of="$OUT/03_rootfs_payload.bin"       bs=1 skip=$FW_DATA_OFFSET      count=$FW_DECLARED_SIZE    status=none
ok "Sliced into 4 parts"

if [ "$FW_TRAILING_BYTES" -gt 0 ]; then
    trailing_off=$((FW_DATA_OFFSET + FW_DECLARED_SIZE))
    dd if="$IMG" of="$OUT/04_trailing_bytes.bin" bs=1 skip=$trailing_off count=$FW_TRAILING_BYTES status=none
    warn "Captured $FW_TRAILING_BYTES trailing byte(s) separately -> $OUT/04_trailing_bytes.bin"
    echo -e "    ${C_DIM}(inspect with: hexdump -C $OUT/04_trailing_bytes.bin)${C_RESET}"
fi

echo
info "Decoding rootfs uImage header via mkimage -l"
cat "$OUT/02_rootfs_uimage_header.bin" "$OUT/03_rootfs_payload.bin" > "$OUT/.tmp_full_uimage.bin"
mkimage -l "$OUT/.tmp_full_uimage.bin" > "$OUT/rootfs_header_decoded.txt" 2>&1 || true
rm -f "$OUT/.tmp_full_uimage.bin"
sed 's/^/    /' "$OUT/rootfs_header_decoded.txt"

echo
info "Diagnostic: rootfs payload compression check"
payload_magic=$(read_hex "$OUT/03_rootfs_payload.bin" 0 4)
echo -e "    ${C_DIM}First 4 bytes of payload: $payload_magic${C_RESET}"
if [ "$payload_magic" = "68737173" ]; then
    ok "matches squashfs magic ('hsqs'). Payload is RAW at this outer layer."
else
    warn "does NOT match squashfs magic. May genuinely be lzma-compressed — investigate before repacking."
fi

cat > "$OUT/info.txt" << EOF
# Captured from: $IMG
# Generated: $(date -u +%Y-%m-%dT%H:%M:%SZ)

SOURCE_IMG="$IMG"
SOURCE_IMG_SIZE=$FW_TOTAL_SIZE
SOURCE_IMG_SHA256=$(sha256sum "$IMG" | awk '{print $1}')

BLOCKSIZE=$BLOCKSIZE
FIT_TOTALSIZE=$FW_TOTALSIZE
ALIGNED_BOUNDARY=$FW_ALIGNED
ROOTFS_UIMAGE_HDR_OFFSET=$FW_HDR_OFFSET
ROOTFS_PAYLOAD_OFFSET=$FW_DATA_OFFSET
ROOTFS_PAYLOAD_SIZE=$FW_DECLARED_SIZE
ROOTFS_TRAILING_BYTES=$FW_TRAILING_BYTES

# uImage header params for the rootfs wrapper (confirmed via mkimage -l — see rootfs_header_decoded.txt)
UIMAGE_NAME="Linux-4.4.60"
UIMAGE_ARCH=arm64
UIMAGE_OS=linux
UIMAGE_TYPE=kernel
UIMAGE_COMP=lzma
UIMAGE_LOAD=0x40908000
UIMAGE_ENTRY=0x40908000

# Raw Unix epoch from the ORIGINAL header's timestamp field (offset 8-11).
# repack.sh doesn't need to know about this directly — mkimage natively
# honors SOURCE_DATE_EPOCH if the caller exports it before invoking repack.sh,
# which is exactly what the wrapper's SPOOF_REPACK_DATE option does.
UIMAGE_TIMESTAMP=$FW_TIMESTAMP
EOF

echo
ok "Done. Contents of $OUT/:"
ls -la "$OUT"
