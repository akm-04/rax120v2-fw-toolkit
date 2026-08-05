#!/usr/bin/env bash
#
# rax120-toolkit.sh
#
# Interactive wrapper for the RAX120v2 firmware unpack/repack toolkit.
# Assumes unpack.sh, unpack_rootfs.sh, build_rootfs.sh, repack.sh all live
# in ./scripts/ relative to wherever this wrapper is launched from.
#
set -uo pipefail

SCRIPTS_DIR="scripts"

# RAX120_PROJECT_ROOT: exported so apply_mods.sh (and anything else that
# wants it -- see PROJECT_ROOT resolution in apply_mods.sh, which prefers
# this over deriving its own project root from its file location) can key
# off the SAME root this wrapper is using. Same "wherever this wrapper is
# launched from" root as everything else below (SCRIPTS_DIR, BIN_DIR,
# MODS_DIR, ...) -- not derived from this file's own location, since the
# header comment above already documents cwd-at-launch as the project
# root by design.
export RAX120_PROJECT_ROOT
RAX120_PROJECT_ROOT="$(pwd)"

BIN_DIR="${BIN_DIR_OVERRIDE:-bin}"
if [ -d "$BIN_DIR" ]; then
    export RAX120_BIN_DIR
    RAX120_BIN_DIR="$(cd "$BIN_DIR" && pwd)"
else
    RAX120_BIN_DIR=""
fi

# Export mkdniimg override if the binary exists in the expected location
if [ -x "$RAX120_BIN_DIR/firmware-utils/mkdniimg" ]; then
    export MKDNIIMG_OVERRIDE="$RAX120_BIN_DIR/firmware-utils/mkdniimg"
fi

# ============================================================================
# Required external tools — checked once at startup,
# ============================================================================
# sudo apt-get install -y fakeroot u-boot-tools patch binwalk
REQUIRED_TOOLS=(
    mkimage
    dumpimage
    dd
    sha256sum
    cmp
    fakeroot
)

# ============================================================================
# Directory and Paths setup.
# ============================================================================

# WORK_DIR: where unpack.sh stores header/FIT/rootfs slices + info.txt
WORK_DIR="tmp"
# Exported so unpack_rootfs.sh / build_rootfs.sh (via resolve_fakeroot_state
# in _lib_toolpath.sh) 
export RAX120_WORK_DIR="$WORK_DIR"

# Exported so build_rootfs.sh writes its built squashfs images into $WORK_DIR too
export OUT_DIR_OVERRIDE="$WORK_DIR"

# TEST_WORK_DIR: separate scratch dir used ONLY by "Test unpack/repack"
TEST_WORK_DIR="$WORK_DIR/test_run"

# ROOTFS_EXTRACT_DIR: default destination for unpack_rootfs.sh's extraction
ROOTFS_EXTRACT_DIR="extracted_rootfs"

# TARGET_ROOTFS_DIR: if set, this directory is used as the rootfs source for
# packing INSTEAD of ROOTFS_EXTRACT_DIR — e.g. point this at a separately
# maintained modified-rootfs tree.
# rather than the toolkit's own extraction output.
#   Empty (""): ROOTFS_EXTRACT_DIR and "the rootfs tree" are treated as the
#               same thing — full pipeline runs unpack -> extract -> pause
#               for edits -> pack -> repack, in full.
#   Set:        full pipeline SKIPS extraction entirely and packs straight
#               from this dir. Running "Unpack rootfs" (action 2) manually
#               while this is set will ask for confirmation first, since its
#               output would go to ROOTFS_EXTRACT_DIR and NOT be used.
TARGET_ROOTFS_DIR=""

# MODS_DIR: directory of NNN-description.sh / NNN[-description].patch mods
# (see scripts/apply_mods.sh's header comment for the naming convention)
# applied on top of the rootfs tree by action_apply_mods / the full
# pipeline's mods step. A missing or empty MODS_DIR is a no-op, not an
# error -- projects that don't use mods yet are unaffected.
MODS_DIR="mods"

# SPOOF_REPACK_DATE: 
#   1 = reuse the ORIGINAL firmware's build timestamp when repaacking.
#   0 = repack.sh stamps the real current time instead (mkimage's default).
SPOOF_REPACK_DATE=1

# OUTPUT_IMG_NAME: filename for the final repacked firmware image.
OUTPUT_IMG_NAME="RAX120-CUSTOM.img"

# STOCK_FW: path to a stock firmware .img file, OR a directory containing
# one or more .img files. If set, skips prompting for the firmware path in
# "Unpack stock firmware", the full pipeline, and test mode. If a directory
# has more than one .img, you'll be asked to pick which one at that point.
# Leave empty ("") to always be prompted for the path manually.
STOCK_FW="Stock_FW"

# Resolved at runtime: whichever of the two rootfs-source vars is in effect.
resolve_rootfs_src() {
    if [ -n "$TARGET_ROOTFS_DIR" ]; then
        echo "$TARGET_ROOTFS_DIR"
    else
        echo "$ROOTFS_EXTRACT_DIR"
    fi
}

# ---- colours ----------------------------------------------------------
RED='\033[1;31m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
BLUE='\033[1;34m'
CYAN='\033[1;36m'
MAGENTA='\033[1;35m'
DIM='\033[2m'
BOLD='\033[1m'
NC='\033[0m'

log_section() {
    echo -e "${BLUE}======================================${NC}"
    echo -e "${GREEN}$1${NC}"
    echo -e "${BLUE}======================================${NC}"
    echo
}

info()  { echo -e "${CYAN}==>${NC} ${1}"; }
ok()    { echo -e "${GREEN}✓${NC} ${1}"; }
warn()  { echo -e "${YELLOW}!${NC} ${1}"; }
err()   { echo -e "${RED}✗ ${1}${NC}"; }

print_config() {
    echo -e "${DIM}scripts/ dir:        $SCRIPTS_DIR${NC}"
    if [ -n "$RAX120_BIN_DIR" ]; then
        echo -e "${DIM}bin/ dir:            $RAX120_BIN_DIR (vendored tools prebuilt)${NC}"
    else
        echo -e "${YELLOW}bin/ dir:            not found (looked for: $BIN_DIR) — squashfs steps will fall back to host tools${NC}"
    fi

    if [ -n "${MKDNIIMG_OVERRIDE:-}" ]; then
        echo -e "${DIM}mkdniimg:            Found ($MKDNIIMG_OVERRIDE)${NC}"
    else
        echo -e "${YELLOW}mkdniimg:            Not found! Repack step will fail unless it is compiled.${NC}"
    fi

    echo -e "${DIM}WORK_DIR:            $WORK_DIR${NC}"
    echo -e "${DIM}TEST_WORK_DIR:       $TEST_WORK_DIR${NC}"
    echo -e "${DIM}ROOTFS_EXTRACT_DIR:  $ROOTFS_EXTRACT_DIR${NC}"
    if [ -n "$TARGET_ROOTFS_DIR" ]; then
        echo -e "${YELLOW}TARGET_ROOTFS_DIR:   $TARGET_ROOTFS_DIR  (overrides extraction dir; full pipeline skips extraction)${NC}"
    else
        echo -e "${DIM}TARGET_ROOTFS_DIR:   (unset — same as ROOTFS_EXTRACT_DIR)${NC}"
    fi
    if [ "$SPOOF_REPACK_DATE" -eq 1 ]; then
        echo -e "${DIM}SPOOF_REPACK_DATE:   1 (repack reuses original build timestamp)${NC}"
    else
        echo -e "${DIM}SPOOF_REPACK_DATE:   0 (repack stamps current time)${NC}"
    fi
    if [ -d "$MODS_DIR" ]; then
        echo -e "${DIM}MODS_DIR:            $MODS_DIR${NC}"
    else
        echo -e "${DIM}MODS_DIR:            $MODS_DIR (doesn't exist — mods step is a no-op)${NC}"
    fi
    echo -e "${DIM}OUTPUT_IMG_NAME:     $OUTPUT_IMG_NAME${NC}"
    if [ -n "$STOCK_FW" ]; then
        echo -e "${YELLOW}STOCK_FW:            $STOCK_FW  (skips firmware path prompt)${NC}"
    else
        echo -e "${DIM}STOCK_FW:            (unset — will prompt for firmware path)${NC}"
    fi
}

# ---- environment check — runs once at startup, exits immediately if incomplete
check_env() {
    local missing=()
    for tool in "${REQUIRED_TOOLS[@]}"; do
        command -v "$tool" >/dev/null 2>&1 || missing+=("$tool")
    done

    if [ "${#missing[@]}" -gt 0 ]; then
        err "Missing required tool(s) on PATH: ${missing[*]}"
        echo -e "${DIM}All of: ${REQUIRED_TOOLS[*]}${NC}"
        echo -e "${DIM}must be resolvable before running this toolkit. Install via your normal${NC}"
        echo -e "${DIM}package manager (e.g. u-boot-tools for mkimage/dumpimage).${NC}"
        exit 1
    fi
    ok "Environment OK — all required tools found: ${REQUIRED_TOOLS[*]}"
}

# ---- ctrl-c handling ----------------------------------------------------
INTERRUPTED=0
on_interrupt() {
    INTERRUPTED=1
    echo
    echo -e "${YELLOW}^C${NC}"
}
trap on_interrupt INT

# ---- helpers ------------------------------------------------------------
prompt() {   # prompt <varname> <question> [default]   — returns 130 on Ctrl+C
    local __var="$1" __q="$2" __default="${3:-}"
    local __ans
    INTERRUPTED=0
    if [ -n "$__default" ]; then
        read -r -p "$(echo -e "${CYAN}${__q}${NC} [${__default}]: ")" __ans
        __ans="${__ans:-$__default}"
    else
        read -r -p "$(echo -e "${CYAN}${__q}${NC}: ")" __ans
    fi
    if [ "$INTERRUPTED" -eq 1 ]; then
        return 130
    fi
    printf -v "$__var" '%s' "$__ans"
    return 0
}

require_script() {   # require_script <name> -> prints path or returns 1
    local name="$1" path="$SCRIPTS_DIR/$1"
    if [ ! -f "$path" ]; then
        err "$path not found. Expected in ./$SCRIPTS_DIR/"
        return 1
    fi
    if [ ! -x "$path" ]; then
        warn "$path is not executable — fixing (chmod +x)"
        chmod +x "$path"
    fi
    echo "$path"
    return 0
}

pause() {
    echo
    read -r -p "$(echo -e "${DIM}Press Enter to return to the menu...${NC}")" _
}

# resolve_stock_fw: sets STOCK_FW_RESOLVED, returns 0 on success.
# Returns 1 if STOCK_FW is unset (caller should prompt manually instead).
resolve_stock_fw() {
    STOCK_FW_RESOLVED=""
    [ -z "$STOCK_FW" ] && return 1

    if [ -f "$STOCK_FW" ]; then
        STOCK_FW_RESOLVED="$STOCK_FW"
        return 0
    fi

    if [ -d "$STOCK_FW" ]; then
        local matches=()
        while IFS= read -r -d '' f; do
            matches+=("$f")
        done < <(find "$STOCK_FW" -maxdepth 1 -iname "*.img" -print0 | sort -z)

        if [ "${#matches[@]}" -eq 0 ]; then
            err "STOCK_FW dir '$STOCK_FW' contains no *.img files"
            return 1
        elif [ "${#matches[@]}" -eq 1 ]; then
            STOCK_FW_RESOLVED="${matches[0]}"
            return 0
        else
            echo -e "${CYAN}Multiple firmware images found in $STOCK_FW:${NC}"
            local i=1
            for f in "${matches[@]}"; do
                echo "  ${i}) $(basename "$f")"
                i=$((i+1))
            done
            echo -e "${DIM}(leave blank and press Enter to type/paste a custom path instead)${NC}"
            local choice
            prompt choice "Select firmware (number)" || return 130
            local idx=$((choice - 1))
            if [ "$idx" -ge 0 ] && [ "$idx" -lt "${#matches[@]}" ] 2>/dev/null; then
                STOCK_FW_RESOLVED="${matches[$idx]}"
                return 0
            else
                err "Invalid selection: $choice"
                return 1
            fi
        fi
    fi

    err "STOCK_FW ('$STOCK_FW') is neither a file nor a directory"
    return 1
}

# get_firmware_path: sets IMG, using STOCK_FW if resolvable, else prompting.
get_firmware_path() {
    if resolve_stock_fw; then
        IMG="$STOCK_FW_RESOLVED"
        info "Using STOCK_FW-resolved image: ${BOLD}$IMG${NC}"
        return 0
    fi
    local rc=$?
    [ "$rc" -eq 130 ] && return 130
    prompt IMG "Path to stock firmware.img" || return 130
    return 0
}

# maybe_export_spoof_date <work_dir> <force>
# If SPOOF_REPACK_DATE=1 (or <force>=1) and <work_dir>/info.txt has a
# captured UIMAGE_TIMESTAMP, exports SOURCE_DATE_EPOCH so repack.sh's
# mkimage call reuses the ORIGINAL firmware's build timestamp instead of
# stamping current time. No-op (prints nothing changed) otherwise.
maybe_export_spoof_date() {
    local work_dir="$1" force="${2:-0}"
    unset SOURCE_DATE_EPOCH
    if [ "$SPOOF_REPACK_DATE" -ne 1 ] && [ "$force" -ne 1 ]; then
        return 0
    fi
    local info_file="$work_dir/info.txt"
    if [ ! -f "$info_file" ]; then
        warn "SPOOF_REPACK_DATE is on, but $info_file doesn't exist yet — repack will use current time this run."
        return 0
    fi
    local ts
    ts=$(grep -E '^UIMAGE_TIMESTAMP=' "$info_file" | cut -d= -f2)
    if [ -z "${ts:-}" ] || [ "$ts" = "0" ]; then
        warn "SPOOF_REPACK_DATE is on, but no UIMAGE_TIMESTAMP found in $info_file (older unpack.sh run?) — using current time."
        return 0
    fi
    export SOURCE_DATE_EPOCH="$ts"
    info "SPOOF_REPACK_DATE active: repack will stamp $(date -d "@$ts" 2>/dev/null || echo "epoch $ts") ${DIM}(SOURCE_DATE_EPOCH=$ts)${NC}"
    return 0
}

# ---- individual actions --------------------------------------------------
action_unpack_fw() {
    log_section "Unpack stock firmware"
    local script; script=$(require_script "unpack.sh") || { pause; return; }
    get_firmware_path || { warn "Cancelled."; pause; return; }
    [ -f "$IMG" ] || { err "File not found: $IMG"; pause; return; }
    info "Output dir: ${BOLD}$WORK_DIR${NC} ${DIM}(edit WORK_DIR at the top of this script to change)${NC}"
    "$script" "$IMG" "$WORK_DIR"
    local rc=$?
    [ "$rc" -eq 0 ] && ok "unpack.sh finished" || err "unpack.sh exited with code $rc"
    pause
}

action_unpack_rootfs() {
    log_section "Unpack rootfs squashfs into editable tree"
    local script; script=$(require_script "unpack_rootfs.sh") || { pause; return; }
    if [ -n "$TARGET_ROOTFS_DIR" ]; then
        warn "TARGET_ROOTFS_DIR is set to '$TARGET_ROOTFS_DIR' — packing will use that dir, NOT this extraction."
        warn "Extracting anyway will still write to ROOTFS_EXTRACT_DIR ($ROOTFS_EXTRACT_DIR), which"
        warn "would go unused unless you clear TARGET_ROOTFS_DIR."
        prompt _ "Press Enter to continue anyway (or Ctrl+C to return to menu)" "" || { warn "Cancelled."; pause; return; }
    fi
    local src="$WORK_DIR/03_rootfs_payload.bin"
    info "Source: ${BOLD}$src${NC}   Output dir: ${BOLD}$ROOTFS_EXTRACT_DIR${NC}"
    [ -f "$src" ] || { err "$src not found — run 'Unpack stock firmware' first."; pause; return; }
    if [ -e "$ROOTFS_EXTRACT_DIR" ]; then
        warn "$ROOTFS_EXTRACT_DIR already exists — unpack_rootfs.sh refuses to overwrite."
        prompt CONFIRM "Remove it first? (y/N)" "N" || { warn "Cancelled."; pause; return; }
        if [ "$CONFIRM" = "y" ] || [ "$CONFIRM" = "Y" ]; then
            rm -rf "$ROOTFS_EXTRACT_DIR"
            ok "Removed $ROOTFS_EXTRACT_DIR"
        else
            warn "Leaving existing directory in place — aborting this step."
            pause
            return
        fi
    fi
    "$script" "$src" "$ROOTFS_EXTRACT_DIR"
    local rc=$?
    [ "$rc" -eq 0 ] && ok "unpack_rootfs.sh finished" || err "unpack_rootfs.sh exited with code $rc"
    pause
}

action_build_rootfs() {
    log_section "Repack rootfs tree into squashfs"
    local script; script=$(require_script "build_rootfs.sh") || { pause; return; }
    local srcdir; srcdir=$(resolve_rootfs_src)
    info "Source rootfs tree: ${BOLD}$srcdir${NC}"
    [ -n "$TARGET_ROOTFS_DIR" ] && info "${DIM}(using TARGET_ROOTFS_DIR override)${NC}"
    [ -d "$srcdir" ] || { err "Directory not found: $srcdir"; pause; return; }
    ( export SRC_DIR_OVERRIDE="$srcdir"; "$script" "" )
    local rc=$?
    [ "$rc" -eq 0 ] && ok "build_rootfs.sh finished" || err "build_rootfs.sh exited with code $rc"
    pause
}

action_apply_mods() {
    log_section "Apply mods (mods/ -> rootfs tree)"
    local script; script=$(require_script "apply_mods.sh") || { pause; return; }
    local rootfs_src; rootfs_src=$(resolve_rootfs_src)
    info "Rootfs tree: ${BOLD}$rootfs_src${NC}   Mods dir: ${BOLD}$MODS_DIR${NC}"
    [ -n "$TARGET_ROOTFS_DIR" ] && info "${DIM}(using TARGET_ROOTFS_DIR override)${NC}"
    [ -d "$rootfs_src" ] || { err "Directory not found: $rootfs_src -- unpack/extract a rootfs tree first."; pause; return; }
    "$script" "$rootfs_src" "$MODS_DIR"
    local rc=$?
    [ "$rc" -eq 0 ] && ok "apply_mods.sh finished" || err "apply_mods.sh exited with code $rc"
    pause
}

action_repack_fw() {
    log_section "Repack final firmware image"
    local script; script=$(require_script "repack.sh") || { pause; return; }
    local newrootfs="$WORK_DIR/rootfs_latest.squashfs"

    # Explicit precondition check: repack needs the header + FIT slices that
    # only unpack.sh produces — fail clearly here rather than let repack.sh's
    # own (less specific) missing-file error surface instead.
    if [ ! -d "$WORK_DIR" ] || [ ! -f "$WORK_DIR/00_netgear_header.bin" ] || [ ! -f "$WORK_DIR/01_fit_kernel_dtb.bin" ]; then
        err "$WORK_DIR is missing the netgear header / FIT slices needed to repack."
        err "Run 'Unpack stock firmware' (action 1) first — repack needs those pieces from the ORIGINAL image."
        pause
        return
    fi
    [ -f "$newrootfs" ] || { err "$newrootfs not found — run 'Pack rootfs tree' (action 3) first."; pause; return; }

    info "Unpack dir: ${BOLD}$WORK_DIR${NC}   New rootfs: ${BOLD}$newrootfs${NC}   Output: ${BOLD}$OUTPUT_IMG_NAME${NC}"
    maybe_export_spoof_date "$WORK_DIR"
    "$script" "$WORK_DIR" "$newrootfs" "$OUTPUT_IMG_NAME"
    local rc=$?
    if [ "$rc" -eq 0 ]; then
        ok "repack.sh finished"
        echo -e "${DIM}(repack.sh's own round-trip check above already compares this against the${NC}"
        echo -e "${DIM} original source image recorded in $WORK_DIR/info.txt — see 'Round-trip comparison')${NC}"
    else
        err "repack.sh exited with code $rc"
    fi
    unset SOURCE_DATE_EPOCH
    pause
}

action_full_pipeline() {
    log_section "Full pipeline: unpack fw -> unpack rootfs -> apply mods -> (edit) -> pack -> repack"
    echo -e "${DIM}This runs each step in sequence. mods/ (if present) is applied"
    echo -e "automatically, then it pauses so you can make any further manual edits"
    echo -e "in the rootfs tree before rebuild + repack.${NC}"
    echo
    for s in unpack.sh unpack_rootfs.sh apply_mods.sh build_rootfs.sh repack.sh; do
        require_script "$s" > /dev/null || { pause; return; }
    done

    local rootfs_src; rootfs_src=$(resolve_rootfs_src)
    info "WORK_DIR=$WORK_DIR   ROOTFS_SRC=$rootfs_src   OUTPUT=$OUTPUT_IMG_NAME"
    [ -n "$TARGET_ROOTFS_DIR" ] && warn "TARGET_ROOTFS_DIR set — step 2 (extraction) will be SKIPPED, packing uses $TARGET_ROOTFS_DIR directly."
    echo

    get_firmware_path || { warn "Cancelled."; pause; return; }
    [ -f "$IMG" ] || { err "File not found: $IMG"; pause; return; }

    log_section "Step 1/5: unpack.sh"
    "$SCRIPTS_DIR/unpack.sh" "$IMG" "$WORK_DIR" || { err "unpack.sh failed"; pause; return; }

    if [ -n "$TARGET_ROOTFS_DIR" ]; then
        log_section "Step 2/5: SKIPPED (TARGET_ROOTFS_DIR override in effect)"
        [ -d "$TARGET_ROOTFS_DIR" ] || { err "TARGET_ROOTFS_DIR ($TARGET_ROOTFS_DIR) does not exist"; pause; return; }
    else
        log_section "Step 2/5: unpack_rootfs.sh"
        if [ -e "$ROOTFS_EXTRACT_DIR" ]; then
            warn "$ROOTFS_EXTRACT_DIR already exists."
            prompt CONFIRM "Remove and re-extract? (y/N)" "N" || { warn "Cancelled."; pause; return; }
            { [ "$CONFIRM" = "y" ] || [ "$CONFIRM" = "Y" ]; } && rm -rf "$ROOTFS_EXTRACT_DIR"
        fi
        if [ ! -e "$ROOTFS_EXTRACT_DIR" ]; then
            "$SCRIPTS_DIR/unpack_rootfs.sh" "$WORK_DIR/03_rootfs_payload.bin" "$ROOTFS_EXTRACT_DIR" \
                || { err "unpack_rootfs.sh failed"; pause; return; }
        fi
    fi

    log_section "Step 3/5: apply_mods.sh"
    if [ -d "$MODS_DIR" ]; then
        "$SCRIPTS_DIR/apply_mods.sh" "$rootfs_src" "$MODS_DIR" || { err "apply_mods.sh failed"; pause; return; }
    else
        info "$MODS_DIR not found — skipping (no-op, not an error)."
    fi

    echo
    warn "PAUSED: edit files under $rootfs_src/ now if you want to make further changes."
    prompt _ "Press Enter when ready to continue (rebuild + repack)" "" || { warn "Cancelled — partial state left in place."; pause; return; }

    log_section "Step 4/5: build_rootfs.sh"
    ( export SRC_DIR_OVERRIDE="$rootfs_src"; "$SCRIPTS_DIR/build_rootfs.sh" "" ) \
        || { err "build_rootfs.sh failed"; pause; return; }

    log_section "Step 5/5: repack.sh"
    maybe_export_spoof_date "$WORK_DIR"
    "$SCRIPTS_DIR/repack.sh" "$WORK_DIR" "$WORK_DIR/rootfs_latest.squashfs" "$OUTPUT_IMG_NAME" \
        || { err "repack.sh failed"; unset SOURCE_DATE_EPOCH; pause; return; }
    unset SOURCE_DATE_EPOCH

    ok "Full pipeline complete: $OUTPUT_IMG_NAME"
    echo -e "${DIM}(repack.sh's own round-trip check above already compared this against the${NC}"
    echo -e "${DIM} original source image — see 'Round-trip comparison' in the output)${NC}"
    pause
}

action_test_unpack_repack() {
    log_section "Test unpack/repack reliability (no rootfs edit)"
    echo -e "${DIM}Unpacks a firmware image, then immediately repacks it using the SAME"
    echo -e "unmodified rootfs payload straight from unpack.sh's output — skipping"
    echo -e "unpack_rootfs.sh and build_rootfs.sh entirely. This validates that the"
    echo -e "unpack/repack offset math is correct for THIS firmware version, since"
    echo -e "offsets can differ between versions (different kernel/DTB sizes shift"
    echo -e "the FIT-end alignment boundary). Always forces SPOOF_REPACK_DATE-style${NC}"
    echo -e "${DIM}timestamp reuse internally, since a byte-perfect result is the whole point.${NC}"
    echo

    local uscript rscript
    uscript=$(require_script "unpack.sh") || { pause; return; }
    rscript=$(require_script "repack.sh") || { pause; return; }

    get_firmware_path || { warn "Cancelled."; pause; return; }
    [ -f "$IMG" ] || { err "File not found: $IMG"; pause; return; }

    if [ -e "$TEST_WORK_DIR" ]; then
        warn "$TEST_WORK_DIR already exists from a previous test run."
        prompt CONFIRM "Remove and re-run? (y/N)" "N" || { warn "Cancelled."; pause; return; }
        { [ "$CONFIRM" = "y" ] || [ "$CONFIRM" = "Y" ]; } || { warn "Aborting test."; pause; return; }
        rm -rf "$TEST_WORK_DIR"
    fi

    log_section "Test step 1/2: unpack.sh -> $TEST_WORK_DIR"
    "$uscript" "$IMG" "$TEST_WORK_DIR" || { err "unpack.sh failed"; pause; return; }

    [ -f "$TEST_WORK_DIR/info.txt" ] || { err "info.txt missing after unpack — cannot continue test."; pause; return; }
    # shellcheck disable=SC1090
    source "$TEST_WORK_DIR/info.txt"

    log_section "Test step 2/2: repack.sh (unmodified rootfs pass-through)"
    maybe_export_spoof_date "$TEST_WORK_DIR" 1
    local test_out="$TEST_WORK_DIR/test_repack.img"
    "$rscript" "$TEST_WORK_DIR" "$TEST_WORK_DIR/03_rootfs_payload.bin" "$test_out"
    local rrc=$?
    unset SOURCE_DATE_EPOCH
    [ "$rrc" -eq 0 ] || { err "repack.sh exited with code $rrc — test failed before comparison."; pause; return; }

    # Independent, unrestricted diff for the summary (repack.sh's own check
    # is head-limited to 5 lines; this one isn't).
    local diff_count=0
    if [ -f "$test_out" ]; then
        diff_count=$(cmp -l "$IMG" "$test_out" 2>/dev/null | wc -l) || true
    fi

    log_section "TEST SUMMARY"
    echo -e "${BOLD}Firmware:${NC}          $IMG"
    echo -e "${BOLD}FIT totalsize:${NC}     ${FIT_TOTALSIZE:-?}"
    echo -e "${BOLD}Aligned boundary:${NC}  ${ALIGNED_BOUNDARY:-?}"
    echo -e "${BOLD}Rootfs header @:${NC}   ${ROOTFS_UIMAGE_HDR_OFFSET:-?}"
    echo -e "${BOLD}Rootfs data @:${NC}     ${ROOTFS_PAYLOAD_OFFSET:-?}"
    echo -e "${BOLD}Rootfs payload size:${NC} ${ROOTFS_PAYLOAD_SIZE:-?}"
    echo -e "${BOLD}Trailing bytes:${NC}    ${ROOTFS_TRAILING_BYTES:-?}"
    echo -e "${BOLD}Repacked image:${NC}    $test_out"
    echo -e "${BOLD}Byte diffs vs original:${NC} $diff_count"
    echo
    if [ "$diff_count" -eq 0 ]; then
        echo -e "${GREEN}${BOLD}VERDICT: ✓ RELIABLE${NC} — unpack/repack round-trip is byte-perfect for this firmware version."
    else
        echo -e "${RED}${BOLD}VERDICT: ✗ NOT RELIABLE${NC} — $diff_count byte(s) differ on an unmodified pass-through."
        echo -e "${DIM}Investigate before trusting this firmware version's computed offsets. First few diffs:${NC}"
        cmp -l "$IMG" "$test_out" 2>/dev/null | head -10 | while read -r off b1 b2; do
            echo "    file offset $off: $b1 -> $b2 (octal)"
        done || true
    fi
    pause
}

show_menu() {
    echo
    echo -e "${DIM}##################################################################${NC}"
    echo -e "${BOLD}${MAGENTA} Configured Directories and Paths:"
    echo -e "${NC}"
    print_config
    echo -e "${DIM}##################################################################${NC}"
    echo
    echo -e "${GREEN} 1)${NC} Unpack stock firmware.img            (unpack.sh)"
    echo -e "${GREEN} 2)${NC} Unpack rootfs squashfs -> tree        (unpack_rootfs.sh)"
    echo -e "${GREEN} 3)${NC} Pack rootfs tree -> squashfs           (build_rootfs.sh)"
    echo -e "${GREEN} 4)${NC} Repack final firmware.img              (repack.sh)"
    echo -e "${GREEN} 5)${NC} Run full pipeline (1 -> 2 -> apply mods -> edit -> 3 -> 4)"
    echo -e "${MAGENTA} 6)${NC} Test unpack/repack reliability ${DIM}(no rootfs edit — validates offsets)${NC}"
    echo -e "${GREEN} 7)${NC} Apply mods only ${DIM}(mods/ -> current rootfs tree)${NC}"
    echo -e "${GREEN} q)${NC} Quit"
    echo
}

# ---- startup ---------------------------------------------------------------
echo -e "${BOLD}${MAGENTA}RAX120v2 toolkit — Checking required tools:${NC}"
echo
check_env

# ---- main loop ------------------------------------------------------------
while true; do
    show_menu
    prompt CHOICE "Choose an option" || { echo -e "${YELLOW}^C at menu — use 'q' to quit.${NC}"; continue; }
    case "$CHOICE" in
        1) action_unpack_fw ;;
        2) action_unpack_rootfs ;;
        3) action_build_rootfs ;;
        4) action_repack_fw ;;
        5) action_full_pipeline ;;
        6) action_test_unpack_repack ;;
        7) action_apply_mods ;;
        q|Q)
            echo -e "${CYAN}Bye.${NC}"
            exit 0
            ;;
        *)
            warn "Unrecognized option: $CHOICE"
            sleep 1
            ;;
    esac
done
