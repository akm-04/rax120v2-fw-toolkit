# shellcheck shell=bash
# _lib_toolpath.sh -- shared vendored-vs-host tool resolution.
# Source this AFTER defining ok()/warn()/fail() (see build_rootfs.sh /
# unpack_rootfs.sh for the shared color-helper definitions this expects).
#
# resolve_tool <out_var> <bin_dir_name> <host_command_name>
#
# Preference order:
#   1. $RAX120_BIN_DIR/<bin_dir_name>, if RAX120_BIN_DIR is exported (set by
#      rax120-toolkit.sh) and that file exists and is executable.
#   2. <host_command_name> on PATH, with a specific warning -- host
#      squashfs-tools builds are CONFIRMED (see project notes / unsquashfs -s
#      output: "error reading stored compressor options from filesystem!")
#      to encode this device's xz compressor-options block differently than
#      the vendor build does. A host-tools fallback run should not be
#      trusted for flashing.
#   3. Neither found -> fail with a clear message.

resolve_tool() {
    local __var="$1" __bin_name="$2" __host_name="$3"
    local __resolved=""

    if [ -n "${RAX120_BIN_DIR:-}" ] && [ -x "$RAX120_BIN_DIR/$__bin_name" ]; then
        __resolved="$RAX120_BIN_DIR/$__bin_name"
        ok "Using vendored $__bin_name -> $__resolved"
    else
        if [ -n "${RAX120_BIN_DIR:-}" ]; then
            warn "RAX120_BIN_DIR is set ($RAX120_BIN_DIR) but $__bin_name isn't there."
        else
            warn "RAX120_BIN_DIR not exported (running standalone, outside rax120-toolkit.sh?)."
        fi
        if command -v "$__host_name" >/dev/null 2>&1; then
            __resolved="$(command -v "$__host_name")"
            warn "Falling back to host-installed '$__host_name' ($__resolved)."
            warn "This is CONFIRMED to encode this device's xz compressor-options"
            warn "block differently from the vendor build (see: unsquashfs -s giving"
            warn "'error reading stored compressor options' on the stock rootfs)."
            warn "Do not trust output from this run for flashing -- get the vendored"
            warn "$__bin_name into \$RAX120_BIN_DIR first."
        else
            fail "$__host_name not found on PATH either -- nothing usable for this step."
        fi
    fi

    printf -v "$__var" '%s' "$__resolved"
}
