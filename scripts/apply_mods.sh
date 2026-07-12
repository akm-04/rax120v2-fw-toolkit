#!/usr/bin/env bash
#
# apply_mods.sh <rootfs_dir> [mods_dir]
#
# Discovers and applies everything in mods_dir/ against <rootfs_dir>, in
# order. Ported from tp-link-ax55-fw-hacks/apply-mods.sh -- adapted to take
# an explicit rootfs dir argument instead of hardcoding ./squashfs-root, so
# it works with whatever resolve_rootfs_src() (TARGET_ROOTFS_DIR /
# ROOTFS_EXTRACT_DIR) currently resolves to in rax120-toolkit.sh.
#
# Naming convention inside mods/:
#   NNN-description.sh         -- run via `bash`, in sort -V order
#   NNN[-description].patch    -- applied via tools/apply_patches.sh
#                                  against <rootfs_dir>, as a single batch
#
# Exactly 3 leading digits required. Anything that doesn't match (wrong
# digit count, missing digits, or a prefix like 'd001-telnet.sh') is
# WARNED ABOUT AND SKIPPED, not applied and not treated as an error --
# that's the deliberate mechanism for disabling a specific mod/patch
# without deleting it.
#
# Patch support uses tools/apply_patches.sh (already present in this repo --
# same script as tp-link-ax55-fw-hacks/vendor/apply_patches.sh, just living
# under tools/ here instead of vendor/). It handles the actual `patch -p1`
# application, including dry-run-first and "already applied" detection --
# apply_mods.sh only discovers/orders/validates mods, it doesn't duplicate
# any of that patch-application logic itself.
#
# Patches are applied first (as one apply_patches.sh call), then scripts
# run in order -- not interleaved by number across the two types.
#
# A missing/empty mods_dir is a no-op, not an error -- this lets the full
# pipeline call this script unconditionally without every project needing
# a mods/ dir to exist.
#
set -euo pipefail

C_RESET='\033[0m'
C_GREEN='\033[1;32m'
C_YELLOW='\033[1;33m'
C_RED='\033[1;31m'
C_CYAN='\033[1;36m'
C_DIM='\033[2m'

info()  { echo -e "${C_CYAN}==>${C_RESET} ${1}"; }
ok()    { echo -e "${C_GREEN}✓${C_RESET} ${1}"; }
warn()  { echo -e "${C_YELLOW}!${C_RESET} ${1}"; }
fail()  { echo -e "${C_RED}✗ ${1}${C_RESET}"; exit 1; }

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]:-$0}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

ROOTFS_DIR="${1:?Usage: apply_mods.sh <rootfs_dir> [mods_dir]}"
MODS_DIR="${2:-$PROJECT_ROOT/mods}"
APPLY_PATCHES="$PROJECT_ROOT/tools/apply_patches.sh"

# Resolve to an absolute path before we cd below, so a relative <rootfs_dir>
# (e.g. "squashfs-root" passed from the caller's cwd) still points at the
# right place after PROJECT_ROOT becomes cwd.
[ -d "$ROOTFS_DIR" ] || fail "$ROOTFS_DIR not found. Run an unpack step first."
ROOTFS_DIR="$(cd "$ROOTFS_DIR" && pwd)"

if [ ! -d "$MODS_DIR" ]; then
    info "$MODS_DIR not found -- nothing to apply, skipping."
    exit 0
fi

HAVE_APPLY_PATCHES=0
[ -f "$APPLY_PATCHES" ] && HAVE_APPLY_PATCHES=1

SCRIPT_RE='^[0-9]{3}-.+\.sh$'
PATCH_RE='^[0-9]{3}(-.*)?\.patch$'

valid_scripts=()
valid_patches=()

shopt -s nullglob
for f in "$MODS_DIR"/*; do
    [ -f "$f" ] || continue
    name="$(basename "$f")"
    case "$name" in
        *.sh)
            if [[ "$name" =~ $SCRIPT_RE ]]; then
                valid_scripts+=("$f")
            else
                warn "skipping $name -- doesn't match NNN-description.sh (disabled or misnamed?)"
            fi
            ;;
        *.patch)
            if [[ ! "$name" =~ $PATCH_RE ]]; then
                warn "skipping $name -- doesn't match NNN[-description].patch (disabled or misnamed?)"
            elif [ "$HAVE_APPLY_PATCHES" -eq 0 ]; then
                warn "skipping $name -- $APPLY_PATCHES not found"
            else
                valid_patches+=("$f")
            fi
            ;;
        *)
            warn "skipping $name -- not a .sh or .patch file"
            ;;
    esac
done
shopt -u nullglob

if [ ${#valid_scripts[@]} -eq 0 ] && [ ${#valid_patches[@]} -eq 0 ]; then
    info "No valid mods found in $MODS_DIR -- nothing to do."
    exit 0
fi

if [ ${#valid_patches[@]} -gt 0 ]; then
    info "Applying ${#valid_patches[@]} patch(es)"
    TMP_PATCH_DIR="$(mktemp -d)"
    trap 'rm -rf "$TMP_PATCH_DIR"' EXIT
    for p in "${valid_patches[@]}"; do
        ln -s "$p" "$TMP_PATCH_DIR/$(basename "$p")"
    done
    bash "$APPLY_PATCHES" "$ROOTFS_DIR" "$TMP_PATCH_DIR" || fail "apply_patches.sh failed"
    ok "Patches applied"
fi

if [ ${#valid_scripts[@]} -gt 0 ]; then
    mapfile -t sorted_scripts < <(printf '%s\n' "${valid_scripts[@]}" | sort -V)
    info "Running ${#sorted_scripts[@]} mod script(s) against $ROOTFS_DIR"
    # cd to PROJECT_ROOT for compatibility with mod scripts ported straight
    # from tp-link-ax55-fw-hacks that assume cwd == repo root and reference
    # ./squashfs-root/... directly. ROOTFS_DIR is also passed as $1 and
    # exported, so any mod script updated to use it instead will work
    # correctly even when the rootfs tree isn't literally ./squashfs-root
    # (e.g. ROOTFS_EXTRACT_DIR mode instead of TARGET_ROOTFS_DIR).
    cd "$PROJECT_ROOT"
    for s in "${sorted_scripts[@]}"; do
        echo -e "${C_DIM}--- $(basename "$s") ---${C_RESET}"
        ROOTFS_DIR="$ROOTFS_DIR" bash "$s" "$ROOTFS_DIR" || fail "$(basename "$s") exited non-zero"
    done
    ok "Scripts applied"
fi

ok "All mods applied to $ROOTFS_DIR"
