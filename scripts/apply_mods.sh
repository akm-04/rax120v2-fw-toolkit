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
#   NNN-description.sh                -- run via `bash`, in sort -V order
#   NNN[-description].patch           -- AUTO-applied via tools/apply_patches.sh
#                                         as one batch, before any scripts run
#   NNN[-description].manual.patch    -- NEVER auto-applied. Only touched
#                                         when a script calls
#                                         `apply_mod_patch <name>` itself --
#                                         see below.
#
# Exactly 3 leading digits required on all three. Anything that doesn't
# match (wrong digit count, missing digits, or a prefix like
# 'd001-telnet.sh') is WARNED ABOUT AND SKIPPED, not applied and not
# treated as an error -- that's the deliberate mechanism for disabling a
# specific mod/patch without deleting it.
#
# MANUAL PATCHES -- for a patch whose target file only exists, or is only
# in a patchable state, AFTER a script's own prep work (e.g. the script
# clones a binary/script into place, THEN the accompanying .manual.patch
# edits that freshly-cloned file). If that patch were auto-applied in the
# upfront batch like a normal .patch, it would fail outright (target
# doesn't exist yet) and abort apply_mods.sh before any script even runs.
# Renaming it to NNN[-description].manual.patch excludes it from that
# batch entirely; the script decides exactly when to apply it by calling:
#
#   apply_mod_patch <patch-filename-or-path>
#
# from anywhere in its own body -- right after the prep step that makes
# the patch valid, not necessarily at the start or end of the script.
# Multiple manual patches, multiple calls, at whatever points make sense:
#
#   PATCH1=some-file.manual.patch
#   PATCH2=other-file.manual.patch
#   ...clone stuff...
#   apply_mod_patch "$PATCH1"
#   ...more work...
#   apply_mod_patch "$PATCH2"
#
# A bare filename (or a "./name" one) resolves against MODS_DIR, not the
# script's cwd -- since cwd is ROOTFS_DIR while scripts run (see below),
# not the mods/ folder the patch actually lives in. An absolute path is
# used as-is. This function is exported (`export -f`) so it's callable
# from inside any mod script without redefining anything.
#
# apply_mod_patch uses the exact same tools/apply_patches.sh underneath --
# same dry-run-first, same "already applied" detection -- just invoked for
# one patch, on demand, instead of the whole mods/ batch.
#
# After all scripts finish, apply_mods.sh does one more pass over every
# .manual.patch: if a dry-run shows it would STILL apply cleanly (i.e. it
# was never actually applied), that's a WARNING -- likely a forgotten
# apply_mod_patch call in whichever script was supposed to trigger it. Not
# fatal, since a script legitimately choosing to skip a patch under some
# condition is possible, but worth flagging.
#
# Patch support uses tools/apply_patches.sh (already present in this repo --
# same script as tp-link-ax55-fw-hacks/vendor/apply_patches.sh, just living
# under tools/ here instead of vendor/). It handles the actual `patch -p1`
# application, including dry-run-first and "already applied" detection --
# apply_mods.sh only discovers/orders/validates mods, it doesn't duplicate
# any of that patch-application logic itself.
#
# Ordering: unclaimed (plain .patch) mods are applied first as one batch,
# then scripts run in sort -V order, each cwd'd into ROOTFS_DIR itself (so
# a script writes plain relative paths -- usr/sbin/foo -- exactly like it
# was working inside the router's own /, not needing a "${ROOTFS_DIR}/"
# prefix). .manual.patch files are applied only when their script calls
# apply_mod_patch, at whatever point in that script's own sequence it
# chooses.
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

# Resolve to an absolute path before we cd anywhere below, so a relative
# <rootfs_dir> (e.g. "squashfs-root" passed from the caller's cwd) still
# points at the right place after cwd changes.
[ -d "$ROOTFS_DIR" ] || fail "$ROOTFS_DIR not found. Run an unpack step first."
ROOTFS_DIR="$(cd "$ROOTFS_DIR" && pwd)"
[ -d "$MODS_DIR" ] && MODS_DIR="$(cd "$MODS_DIR" && pwd)"

# Exported (not just set) -- apply_mod_patch runs inside a CHILD bash
# process (each mod script), which only inherits exported variables, not
# apply_mods.sh's own local ones.
export ROOTFS_DIR MODS_DIR APPLY_PATCHES

# apply_mod_patch <patch-filename-or-path>
#
# Applies exactly one patch, right now, via the same tools/apply_patches.sh
# the batch step uses -- so behavior (dry-run-first, "already applied"
# detection) is identical whether a patch was auto-batched or manually
# triggered. A relative argument resolves against MODS_DIR (where the
# patch actually lives), not the caller's cwd -- which is ROOTFS_DIR while
# a mod script is running, not mods/. Exported via `export -f` so it's
# callable from inside any mod script with no setup on the script's part.
apply_mod_patch() {
    local arg="${1:?apply_mod_patch: missing patch filename}" patch_path tmp
    case "$arg" in
        /*) patch_path="$arg" ;;
        *)  patch_path="$MODS_DIR/$arg" ;;
    esac
    if [ ! -f "$patch_path" ]; then
        echo "apply_mod_patch: $patch_path not found" >&2
        return 1
    fi
    if [ ! -f "$APPLY_PATCHES" ]; then
        echo "apply_mod_patch: $APPLY_PATCHES not found -- cannot apply $(basename "$patch_path")" >&2
        return 1
    fi
    tmp="$(mktemp -d)"
    ln -s "$patch_path" "$tmp/$(basename "$patch_path")"
    if bash "$APPLY_PATCHES" "$ROOTFS_DIR" "$tmp"; then
        rm -rf "$tmp"
        return 0
    else
        rm -rf "$tmp"
        echo "apply_mod_patch: failed to apply $(basename "$patch_path")" >&2
        return 1
    fi
}
export -f apply_mod_patch

if [ ! -d "$MODS_DIR" ]; then
    info "$MODS_DIR not found -- nothing to apply, skipping."
    exit 0
fi

HAVE_APPLY_PATCHES=0
[ -f "$APPLY_PATCHES" ] && HAVE_APPLY_PATCHES=1

SCRIPT_RE='^[0-9]{3}-.+\.sh$'
PATCH_RE='^[0-9]{3}(-.*)?\.patch$'
MANUAL_PATCH_RE='^[0-9]{3}(-.*)?\.manual\.patch$'

valid_scripts=()
valid_patches=()
manual_patches=()

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
        *.manual.patch)
            # Checked BEFORE the generic *.patch case below -- a
            # .manual.patch file also ends in .patch, so case's
            # first-match-wins ordering is what keeps these out of the
            # auto-applied bucket.
            if [[ "$name" =~ $MANUAL_PATCH_RE ]]; then
                manual_patches+=("$f")
            else
                warn "skipping $name -- doesn't match NNN[-description].manual.patch (disabled or misnamed?)"
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

if [ ${#manual_patches[@]} -gt 0 ]; then
    info "${#manual_patches[@]} manual patch(es) found -- NOT auto-applying (waiting for a script's apply_mod_patch call)"
fi

if [ ${#valid_scripts[@]} -eq 0 ] && [ ${#valid_patches[@]} -eq 0 ] && [ ${#manual_patches[@]} -eq 0 ]; then
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
    # cd into ROOTFS_DIR itself (not PROJECT_ROOT) before running each mod
    # script, so a script can just write plain relative paths -- usr/sbin/foo,
    # etc/init.d/telnet -- exactly like it was working inside the router's
    # own / rather than needing to prefix every path with "${ROOTFS_DIR}/".
    # ROOTFS_DIR is still passed as $1 and exported for any script that
    # wants the absolute path anyway (e.g. for log messages).
    cd "$ROOTFS_DIR"
    for s in "${sorted_scripts[@]}"; do
        echo -e "${C_DIM}--- $(basename "$s") ---${C_RESET}"
        ROOTFS_DIR="$ROOTFS_DIR" bash "$s" "$ROOTFS_DIR" || fail "$(basename "$s") exited non-zero"
    done
    ok "Scripts applied"
fi

ok "All mods applied to $ROOTFS_DIR"

if [ ${#manual_patches[@]} -gt 0 ] && [ "$HAVE_APPLY_PATCHES" -eq 1 ]; then
    # A manual patch that STILL applies cleanly in a forward dry-run was
    # never actually applied -- most likely a forgotten apply_mod_patch
    # call in whichever script was supposed to trigger it. Not fatal: a
    # script conditionally skipping a patch on purpose is legitimate, so
    # this is a warning to check, not an error to abort on.
    unapplied=()
    for p in "${manual_patches[@]}"; do
        if (cd "$ROOTFS_DIR" && patch -p1 --dry-run -i "$p" > /dev/null 2>&1); then
            unapplied+=("$(basename "$p")")
        fi
    done
    if [ "${#unapplied[@]}" -gt 0 ]; then
        echo
        warn "${#unapplied[@]} manual patch(es) were never applied by any script -- check for a missing apply_mod_patch call:"
        for u in "${unapplied[@]}"; do
            echo -e "    ${C_DIM}${u}${C_RESET}"
        done
    fi
fi
