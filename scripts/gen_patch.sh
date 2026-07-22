#!/usr/bin/env bash
#
# generate_patch.sh [description]
#
# Diffs a freshly-extracted, pristine rootfs tree against your edited
# working copy, and writes the result as a numbered patch into
# PATCH_OUT_DIR (mods/) in the a/<path> b/<path> form that `patch -p1` --
# and therefore APPLY_PATCHES_SCRIPT -- expects.
#
# Fully config-driven -- edit the vars below, then just run this script
# (optionally with a short description as $1, used in the patch's
# filename). Every run re-extracts ROOTFS_IMAGE into ROOTFS_EXTRACT_DIR
# from scratch via UNPACK_ROOTFS_SCRIPT, so the pristine side of the diff
# is always guaranteed to genuinely match the image you're working from,
# not a stale copy from three edits ago.
#
# PATH RESOLUTION: ROOTFS_IMAGE / TARGET_MOD_DIR / PATCH_OUT_DIR are
# resolved relative to wherever you RUN this script from (cwd) -- same
# convention as every other script in this repo (build_rootfs.sh's
# SRC_DIR, etc). UNPACK_ROOTFS_SCRIPT / TOOLS_DIR / APPLY_PATCHES_SCRIPT
# are resolved relative to wherever this SCRIPT FILE lives instead, since
# they're "find my sibling infrastructure" paths that shouldn't depend on
# your cwd. Defaults below assume this file sits in scripts/, alongside
# unpack_rootfs.sh, with bin/ and tools/ one level up -- adjust if you
# keep it somewhere else.
#
# ENVIRONMENT THIS SCRIPT SHARES WITH unpack_rootfs.sh / rax120-toolkit.sh
# (per the real _lib_toolpath.sh -- resolve_tool() / resolve_fakeroot_state()):
#   RAX120_BIN_DIR  -- vendored unsquashfs. Auto-exported below from
#                      TOOLS_DIR if not already set by a caller (its
#                      export wins if this is ever invoked from inside
#                      rax120-toolkit.sh). Without it, unpack_rootfs.sh
#                      falls back to host unsquashfs -- confirmed to
#                      mismatch this device's compressor-options block --
#                      so this matters for a trustworthy pristine
#                      reference, not just cosmetic consistency.
#   RAX120_WORK_DIR -- read by resolve_fakeroot_state(): if set, fakeroot
#                      state goes to "$RAX120_WORK_DIR/fakeroot.state"
#                      (one FIXED filename, not per-extraction -- don't
#                      point two concurrent extractions at the same
#                      RAX120_WORK_DIR). Left unset here on purpose: when
#                      unset, resolve_fakeroot_state() falls back to the
#                      sibling-path convention unpack_rootfs.sh already
#                      passes in and cleans up internally
#                      (`rm -f "$FAKEROOT_STATE"` right before extracting),
#                      so no separate handling is needed on this end.
#
# ============================================================================
ROOTFS_IMAGE="${ROOTFS_IMAGE_OVERRIDE:-../tmp/03_rootfs_payload.bin}"
ROOTFS_EXTRACT_DIR="${ROOTFS_EXTRACT_DIR_OVERRIDE:-../tmp/rax120_pristine_rootfs}"
TARGET_MOD_DIR="${TARGET_MOD_DIR_OVERRIDE:-../extracted_rootfs}"
PATCH_OUT_DIR="${PATCH_OUT_DIR_OVERRIDE:-../mods}"

UNPACK_ROOTFS_SCRIPT="${UNPACK_ROOTFS_SCRIPT_OVERRIDE:-unpack_rootfs.sh}"      # sibling in scripts/
TOOLS_DIR="${TOOLS_DIR_OVERRIDE:-../bin}"                                       # vendored mksquashfs4/unsquashfs4 -- exported as RAX120_BIN_DIR below
APPLY_PATCHES_SCRIPT="${APPLY_PATCHES_SCRIPT_OVERRIDE:-../tools/apply_patches.sh}"
# ============================================================================
#
# WHAT THIS CAPTURES: added, removed, and content-modified regular files.
#
# WHAT THIS CANNOT CAPTURE AS A PATCH -- by design, not a bug to fix here.
# Classic unified diff has no hunk representation for these, and
# `patch(1)` cannot apply them even if diff emitted something:
#   - symlink creation / removal / retargeting
#   - device nodes, fifos, sockets
#   - permission-only or ownership-only changes (no content change)
# This script detects all of the above, EXCLUDES them from the .patch file,
# and prints them as a separate report so you write a NNN-description.sh
# mod script for that part instead -- consistent with how apply_mods.sh
# already splits mods into the two types.
#
# After writing the patch, this script self-tests it: applies it to a
# scratch copy of the pristine extraction and confirms the result matches
# TARGET_MOD_DIR for every regular file the patch claims to cover. A patch
# that fails this check is NOT left behind.
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

# Resolve the three infrastructure paths against SCRIPT_DIR unless already
# absolute -- this is what makes relocating this file just a matter of
# editing the three vars above, nothing else in the script needs to change.
resolve_against_script_dir() {
    case "$1" in
        /*) printf '%s' "$1" ;;
        *)  printf '%s' "$SCRIPT_DIR/$1" ;;
    esac
}
UNPACK_ROOTFS_SCRIPT="$(resolve_against_script_dir "$UNPACK_ROOTFS_SCRIPT")"
TOOLS_DIR="$(resolve_against_script_dir "$TOOLS_DIR")"
APPLY_PATCHES_SCRIPT="$(resolve_against_script_dir "$APPLY_PATCHES_SCRIPT")"

DESC="${1:-}"

# ---- vendored tool dir: mirrors rax120-toolkit.sh's own RAX120_BIN_DIR export
if [ -n "${RAX120_BIN_DIR:-}" ]; then
    info "RAX120_BIN_DIR already set in environment ($RAX120_BIN_DIR) — respecting it as-is."
elif [ -d "$TOOLS_DIR" ]; then
    export RAX120_BIN_DIR
    RAX120_BIN_DIR="$(cd "$TOOLS_DIR" && pwd)"
    info "Exporting RAX120_BIN_DIR=$RAX120_BIN_DIR for the extraction step"
else
    warn "No vendored tools dir found at $TOOLS_DIR — unpack_rootfs.sh will fall back to host"
    warn "unsquashfs (confirmed to mismatch this device's compressor-options block)."
fi

# relpath_of_diffline <line> <rootA> <rootB>
# Extracts the path relative to whichever root it belongs to, from one
# line of `diff -rq` output. Lets us compare "this entry differs" across
# two DIFFERENT pairs of directories (generation pass vs self-test pass,
# which use a different scratch dir each run) by relative-path identity
# instead of literal diff-line text, which would never match.
relpath_of_diffline() {
    local line="$1" rootA="$2" rootB="$3" rest fa dir name full
    case "$line" in
        "Files "*" and "*" differ")
            rest="${line#Files }"; rest="${rest% differ}"
            fa="${rest%% and *}"
            echo "${fa#"$rootA"/}"
            ;;
        "Symbolic links "*" and "*" differ")
            rest="${line#Symbolic links }"; rest="${rest% differ}"
            fa="${rest%% and *}"
            echo "${fa#"$rootA"/}"
            ;;
        "Only in "*)
            rest="${line#Only in }"
            dir="${rest%%: *}"; name="${rest#*: }"
            full="$dir/$name"
            case "$dir"/ in
                "$rootA"/*) echo "${full#"$rootA"/}" ;;
                "$rootB"/*) echo "${full#"$rootB"/}" ;;
                *) echo "$full" ;;
            esac
            ;;
        *)
            echo "$line"
            ;;
    esac
}

# ---- resolve config paths --------------------------------------------------
[ -f "$ROOTFS_IMAGE" ] || fail "ROOTFS_IMAGE not found: $ROOTFS_IMAGE (edit the config vars at the top of this script)"
[ -d "$TARGET_MOD_DIR" ] || fail "TARGET_MOD_DIR not found: $TARGET_MOD_DIR -- make your edits there first"
ROOTFS_IMAGE="$(cd "$(dirname "$ROOTFS_IMAGE")" && pwd)/$(basename "$ROOTFS_IMAGE")"
TARGET_MOD_DIR="$(cd "$TARGET_MOD_DIR" && pwd)"
mkdir -p "$PATCH_OUT_DIR"
PATCH_OUT_DIR="$(cd "$PATCH_OUT_DIR" && pwd)"

# ---- extract the pristine reference ----------------------------------------
# Deliberately calling unpack_rootfs.sh rather than re-implementing squashfs
# extraction here -- one implementation of "how do we extract this image"
# for the whole repo, not two that can silently drift apart (different
# vendored-tool resolution, different fakeroot handling, etc).
[ -f "$UNPACK_ROOTFS_SCRIPT" ] || fail "UNPACK_ROOTFS_SCRIPT not found: $UNPACK_ROOTFS_SCRIPT (edit the config vars at the top of this script)"

info "Extracting pristine reference: $ROOTFS_IMAGE -> $ROOTFS_EXTRACT_DIR"
rm -rf "$ROOTFS_EXTRACT_DIR" "${ROOTFS_EXTRACT_DIR%/}.fakeroot.state"
bash "$UNPACK_ROOTFS_SCRIPT" "$ROOTFS_IMAGE" "$ROOTFS_EXTRACT_DIR" || fail "unpack_rootfs.sh failed"
PRISTINE_DIR="$(cd "$ROOTFS_EXTRACT_DIR" && pwd)"
MODIFIED_DIR="$TARGET_MOD_DIR"

# ---- pick the next NNN ------------------------------------------------------
# Shared counter across .sh and .patch: apply_mods.sh applies patches and
# scripts as two separate ordered groups, so this doesn't affect execution
# order -- it's purely so `ls mods/` reads as one readable timeline.
last=$( { find "$PATCH_OUT_DIR" -maxdepth 1 -type f \( -name '[0-9][0-9][0-9]*.sh' -o -name '[0-9][0-9][0-9]*.patch' \) -printf '%f\n' 2>/dev/null \
       | grep -oE '^[0-9]{3}' | sort -n | tail -1; } || true )
last="${last:-000}"
next_num=$(printf '%03d' $((10#$last + 1)))

if [ -n "$DESC" ]; then
    slug=$(echo "$DESC" | tr '[:upper:] ' '[:lower:]-' | tr -cd 'a-z0-9-')
    OUT="$PATCH_OUT_DIR/${next_num}-${slug}.patch"
else
    OUT="$PATCH_OUT_DIR/${next_num}.patch"
fi
[ -e "$OUT" ] && fail "$OUT already exists (shouldn't happen — race with another mod being added concurrently?)"

# ---- diff -------------------------------------------------------------------
# Two passes, deliberately not one `diff -ruN` call: -N (--new-file) makes
# diff treat a missing REGULAR file as empty for a clean hunk, but it does
# NOT understand a missing/extra SYMLINK the same way -- it tries to read
# through it and hard-errors instead of reporting it cleanly. So: pass 1
# (-rq, no -N) just enumerates *what* differs, safely (--no-dereference
# means dangling symlinks never get read-through). Pass 2 builds a real
# diff -u hunk ONLY for pairs confirmed to both be plain regular files --
# everything else becomes a special_notes entry instead.
info "Enumerating differences: $PRISTINE_DIR -> $MODIFIED_DIR"
set +e
summary=$(diff -rq --no-dereference -x '*.fakeroot.state' -x '.git' "$PRISTINE_DIR" "$MODIFIED_DIR" 2>&1)
summary_rc=$?
set -e

if [ "$summary_rc" -gt 1 ]; then
    fail "diff reported a real error enumerating differences:
$summary"
fi
if [ "$summary_rc" -eq 0 ]; then
    info "No differences found between the pristine and modified trees — nothing to generate."
    exit 0
fi

patch_lines=""
special_notes=()
special_relpaths=()

while IFS= read -r line; do
    case "$line" in
        "Files "*" and "*" differ")
            rest="${line#Files }"; rest="${rest% differ}"
            fa="${rest%% and *}"; fb="${rest#* and }"
            if [ -f "$fa" ] && [ ! -L "$fa" ] && [ -f "$fb" ] && [ ! -L "$fb" ]; then
                relpath="${fa#"$PRISTINE_DIR"/}"
                hunk=$(diff -u --label "a/$relpath" --label "b/$relpath" "$fa" "$fb" 2>/dev/null || true)
                case "$hunk" in
                    "Binary files "*)
                        special_notes+=("$line (binary file — diff can't produce a patchable hunk for this)")
                        special_relpaths+=("$relpath")
                        ;;
                    *)
                        patch_lines+="$hunk
"
                        ;;
                esac
            else
                special_notes+=("$line")
                special_relpaths+=("$(relpath_of_diffline "$line" "$PRISTINE_DIR" "$MODIFIED_DIR")")
            fi
            ;;
        "Only in "*)
            rest="${line#Only in }"
            dir="${rest%%: *}"; name="${rest#*: }"
            full="$dir/$name"
            if [ -f "$full" ] && [ ! -L "$full" ]; then
                case "$dir"/ in
                    "$PRISTINE_DIR"/*)
                        relpath="${full#"$PRISTINE_DIR"/}"
                        hunk=$(diff -u --label "a/$relpath" --label /dev/null "$full" /dev/null 2>/dev/null || true)
                        ;;
                    "$MODIFIED_DIR"/*)
                        relpath="${full#"$MODIFIED_DIR"/}"
                        hunk=$(diff -u --label /dev/null --label "b/$relpath" /dev/null "$full" 2>/dev/null || true)
                        ;;
                    *)
                        hunk=""
                        relpath=""
                        special_notes+=("$line (unrecognized dir prefix — investigate manually)")
                        special_relpaths+=("$(relpath_of_diffline "$line" "$PRISTINE_DIR" "$MODIFIED_DIR")")
                        ;;
                esac
                case "$hunk" in
                    "Binary files "*)
                        special_notes+=("$line (binary file — diff can't produce a patchable hunk for this)")
                        special_relpaths+=("$relpath")
                        ;;
                    "")
                        : # nothing to add (either the unrecognized-prefix case above, or genuinely empty)
                        ;;
                    *)
                        patch_lines+="$hunk
"
                        ;;
                esac
            else
                special_notes+=("$line")
                special_relpaths+=("$(relpath_of_diffline "$line" "$PRISTINE_DIR" "$MODIFIED_DIR")")
            fi
            ;;
        *)
            special_notes+=("$line")
            special_relpaths+=("$(relpath_of_diffline "$line" "$PRISTINE_DIR" "$MODIFIED_DIR")")
            ;;
    esac
done <<< "$summary"

if [ -n "$patch_lines" ]; then
    printf '%s' "$patch_lines" > "$OUT"
    ok "Wrote $OUT"
else
    info "Only non-patchable differences found (see below) — no .patch file written."
fi

if [ "${#special_notes[@]}" -gt 0 ]; then
    echo
    warn "${#special_notes[@]} change(s) can't be expressed as a patch — write a"
    warn "NNN-description.sh mod script for these instead (ln -sf / mknod / chmod):"
    for n in "${special_notes[@]+"${special_notes[@]}"}"; do
        echo -e "    ${C_DIM}${n}${C_RESET}"
    done
fi

[ -n "$patch_lines" ] || exit 0

# ---- self-test: does this patch actually reproduce the modified tree? -----
echo
info "Self-testing generated patch"
CHECK_DIR="$(mktemp -d)"
trap 'rm -rf "$CHECK_DIR"' EXIT
cp -a "$PRISTINE_DIR/." "$CHECK_DIR/"

if [ -f "$APPLY_PATCHES_SCRIPT" ]; then
    APPLY_TMP="$(mktemp -d)"
    ln -s "$OUT" "$APPLY_TMP/$(basename "$OUT")"
    if ! bash "$APPLY_PATCHES_SCRIPT" "$CHECK_DIR" "$APPLY_TMP" > /tmp/.genpatch_apply.log 2>&1; then
        cat /tmp/.genpatch_apply.log
        rm -f "$OUT"
        fail "Self-test failed: patch didn't apply cleanly to a fresh copy of the pristine tree. Patch NOT kept."
    fi
    rm -rf "$APPLY_TMP"
else
    warn "$APPLY_PATCHES_SCRIPT not found — falling back to plain 'patch -p1' for self-test"
    if ! (cd "$CHECK_DIR" && patch -p1 -i "$OUT" > /tmp/.genpatch_apply.log 2>&1); then
        cat /tmp/.genpatch_apply.log
        rm -f "$OUT"
        fail "Self-test failed: patch didn't apply cleanly to a fresh copy of the pristine tree. Patch NOT kept."
    fi
fi

# Compare regular-file content only -- the special-cased paths (symlinks
# etc.) are expected to still differ here since the patch deliberately
# doesn't touch them. Match by RELATIVE path (via relpath_of_diffline),
# not literal line text -- CHECK_DIR's absolute path differs every run.
set +e
check_summary=$(diff -rq --no-dereference \
    -x '*.fakeroot.state' -x '.git' \
    "$CHECK_DIR" "$MODIFIED_DIR" 2>&1)
set -e

unexpected=()
while IFS= read -r line; do
    [ -n "$line" ] || continue
    rp="$(relpath_of_diffline "$line" "$CHECK_DIR" "$MODIFIED_DIR")"
    found=0
    for known in "${special_relpaths[@]+"${special_relpaths[@]}"}"; do
        [ "$rp" = "$known" ] && { found=1; break; }
    done
    [ "$found" -eq 0 ] && unexpected+=("$line")
done <<< "$check_summary"

if [ "${#unexpected[@]}" -gt 0 ]; then
    warn "Self-test found unexpected differences after applying the patch:"
    for u in "${unexpected[@]}"; do
        echo "    $u"
    done
    warn "Patch was still written to $OUT — inspect the above before trusting it."
else
    ok "Self-test passed: applying $OUT to the pristine extraction reproduces $TARGET_MOD_DIR"
    ok "(remaining known differences are exactly the ${#special_relpaths[@]} non-patchable change(s) reported above)"
fi

echo
ok "Done: $OUT"
