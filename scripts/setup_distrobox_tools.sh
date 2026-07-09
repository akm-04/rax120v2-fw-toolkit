#!/usr/bin/env bash
#
# setup_distrobox_tools.sh [container_name]
#
# Default container name is "ubuntu" — override with: ./setup_distrobox_tools.sh <name>
#
set -uo pipefail

RED='\033[1;31m'; GREEN='\033[1;32m'; YELLOW='\033[1;33m'
CYAN='\033[1;36m'; DIM='\033[2m'; NC='\033[0m'
info() { echo -e "${CYAN}==>${NC} ${1}"; }
ok()   { echo -e "${GREEN}✓${NC} ${1}"; }
warn() { echo -e "${YELLOW}!${NC} ${1}"; }
err()  { echo -e "${RED}✗ ${1}${NC}"; }

CONTAINER_NAME="${1:-ubuntu}"
EXPORT_PATH="${HOME}/.local/bin"
TOOLS_TO_EXPORT=(mkimage dumpimage)

# ---- 1. is distrobox even installed on this system? -----------------------
if ! command -v distrobox >/dev/null 2>&1; then
    warn "distrobox not found on this system — nothing for this script to do."
    warn "If required tools are missing, install them via your normal package manager instead."
    exit 0
fi

# ---- 2. does the named container actually exist? ---------------------------
if ! distrobox list --no-color 2>/dev/null | awk -F'|' 'NR>1{gsub(/^[ \t]+|[ \t]+$/,"",$2); print $2}' | grep -qx "$CONTAINER_NAME"; then
    err "distrobox container '$CONTAINER_NAME' not found."
    echo -e "${DIM}Available containers:${NC}"
    distrobox list --no-color 2>/dev/null | awk -F'|' 'NR>1{gsub(/^[ \t]+|[ \t]+$/,"",$2); print "  - "$2}'
    echo
    echo -e "${DIM}Re-run with: $0 <container_name>${NC}"
    exit 1
fi
ok "Container '$CONTAINER_NAME' found"

mkdir -p "$EXPORT_PATH"

# ---- 3. per-tool: skip if already on host, else locate inside container and export
any_failed=0
for tool in "${TOOLS_TO_EXPORT[@]}"; do
    if command -v "$tool" >/dev/null 2>&1; then
        ok "$tool already resolvable on host ($(command -v "$tool")) — skipping"
        continue
    fi

    container_path=$(distrobox enter "$CONTAINER_NAME" -- which "$tool" 2>/dev/null) || true
    if [ -z "$container_path" ]; then
        err "$tool not found inside '$CONTAINER_NAME' either — install it there first"
        echo -e "${DIM}    (e.g. inside the container: apt install u-boot-tools)${NC}"
        any_failed=1
        continue
    fi

    info "Exporting $tool ($container_path) from '$CONTAINER_NAME' -> $EXPORT_PATH"
    if distrobox enter "$CONTAINER_NAME" -- distrobox-export --bin "$container_path" --export-path "$EXPORT_PATH" >/dev/null 2>&1; then
        ok "$tool exported"
    else
        err "distrobox-export failed for $tool"
        any_failed=1
    fi
done

echo
if echo "$PATH" | tr ':' '\n' | grep -qxF "$EXPORT_PATH"; then
    ok "$EXPORT_PATH is on PATH — exported tools should be usable directly now."
else
    warn "$EXPORT_PATH is NOT on your PATH. Add this to your shell rc file, then restart your shell:"
    echo "    export PATH=\"$EXPORT_PATH:\$PATH\""
fi

[ "$any_failed" -eq 0 ] || exit 1
exit 0
