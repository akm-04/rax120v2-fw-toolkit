# OpenWrt-NSS — RAX120v2 Firmware Toolkit

Unpack, edit, and repack NETGEAR RAX120v2 firmware images.

## Layout

```
.
├── rax120-toolkit.sh   # interactive menu — start here
├── scripts/            # unpack.sh, unpack_rootfs.sh, build_rootfs.sh, repack.sh
├── bin/                # vendored mksquashfs4 / unsquashfs4 (preferred over host tools)
├── Stock_FW/            # place original .img files here
└── out/                 # all generated output lands here
```

## Quick start

```bash
./rax120-toolkit.sh
```

Pick **5) Run full pipeline** — it unpacks the stock image, extracts the
rootfs to a tree, pauses so you can edit files, then rebuilds and repacks
into `RAX120-CUSTOM.img`.

If you just want to poke around without editing anything, run
**6) Test unpack/repack reliability** — does a full unpack → repack pass
with no edits and reports whether the result is byte-identical to the
original.

## Manual step-by-step

```bash
scripts/unpack.sh         Stock_FW/RAX120-Vx.x.x.x.img out
scripts/unpack_rootfs.sh  out/03_rootfs_payload.bin    out/extracted_rootfs
# ... edit files under out/extracted_rootfs/ ...
scripts/build_rootfs.sh   <label>          # SRC_DIR_OVERRIDE=out/extracted_rootfs if not default
scripts/repack.sh         out out/rootfs_latest.squashfs RAX120-CUSTOM.img
```

## If the tools in `bin/` don't work

`bin/mksquashfs4` / `bin/unsquashfs4` are prebuilt from NETGEAR's GPL source
and are what everything above assumes you're using. If they don't run on
your machine (missing libs, wrong arch, etc.), rebuild them:

```sh
make tools
```

If that errors out, it's almost certainly a toolchain/library mismatch —
the GPL source expects an Ubuntu 14.04-era build environment. Spin one up
with distrobox and build inside it instead:

```sh
distrobox create --name ubuntu14 --image ubuntu:14.04
distrobox enter ubuntu14
# inside the container:
sudo apt-get update
sudo apt-get install -y build-essential linux-headers-generic \
    libacl1-dev liblzo2-dev uuid-dev zlib1g-dev liblzma-dev libselinux1-dev
make tools
```

See `tools/README.md` for where the GPL source itself comes from (official
NETGEAR links are dead — Internet Archive mirror is linked there) and which
patches are applied.

## Important notes

- **Use the vendored tools in `bin/`**, not host `mksquashfs`/`unsquashfs`.
  Host builds are confirmed to encode this device's xz compressor-options
  block differently — `unsquashfs -s` on the stock rootfs fails with
  `error reading stored compressor options` under host tools.
- The rootfs partition (mtd26) is **~93.87 MB** (`0x5de0000`) — `build_rootfs.sh`
  checks your rebuilt image against this and refuses to let an oversized
  build slide by silently.
- Device nodes in the rootfs need a matching `fakeroot` state file
  (`<dir>.fakeroot.state`) to survive the extract → repack round-trip —
  `unpack_rootfs.sh` and `build_rootfs.sh` handle this automatically as
  long as you don't rename or move the extracted tree.
- **Always verify on-device** (sha256 read-back from mtd26) before rebooting
  a flashed image.

## Key env vars (set before running `rax120-toolkit.sh`, or edit its config block)

| Var | Purpose |
|---|---|
| `BIN_DIR_OVERRIDE` | point at a different vendored-tools dir |
| `ROOTFS_EXTRACT_DIR` | where step 2 extracts to (default `out/extracted_rootfs`) |
| `TARGET_ROOTFS_DIR` | pack from a different tree instead (skips extraction) |
| `SPOOF_REPACK_DATE` | `1` = reuse original image's build timestamp on repack |
| `OUTPUT_IMG_NAME` | final repacked image filename |
| `STOCK_FW` | dir to auto-scan for stock `.img` files |
