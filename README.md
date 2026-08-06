# RAX120v2 Firmware Mod Toolkit

Unpack stock firmware, edit, and repack stock firmware to flash via Netgear GUI.

---

## Repo Structure

```
.
├── rax120-toolkit.sh   # interactive menu — start here
├── scripts/            # unpack.sh, unpack_rootfs.sh, build_rootfs.sh, repack.sh
├── bin/                # vendored mksquashfs4 / unsquashfs4 / mkdniimg (preferred over host tools)
├── tools/              # QSDK GPL vendor source these prebuilts are compiled from (see Tools & Dependencies)
├── mods/               # NNN-description.sh / .patch mods, auto-applied on top of the rootfs tree
└── Stock_FW/           # stock firmware images stored here
```

Upon running the unpack/repack scripts, these folders are created as working dir and temp dir:

```
.
├── extracted_rootfs/  # rootfs extraction — make any modification you want here
├── tmp/                # extracted kernel, rootfs, headers, info files etc. needed to repack fw
└── RAX120-CUSTOM.img   # final repacked, modified fw to flash via the Netgear GUI
```

---

## Quick start

Everything is driven through the menu — there's no separate manual/step-by-step path, each script under `scripts/` expects the env vars and working-dir layout the wrapper sets up for it. From repo root, run:

```bash
./rax120-toolkit.sh
```

You'll be greeted with a menu:

```
 1) Unpack stock firmware.img            (unpack.sh)
 2) Unpack rootfs squashfs -> tree        (unpack_rootfs.sh)
 3) Pack rootfs tree -> squashfs           (build_rootfs.sh)
 4) Repack final firmware.img              (repack.sh)
 5) Run full pipeline (1 -> 2 -> apply mods -> edit -> 3 -> 4)
 6) Test unpack/repack reliability (no rootfs edit — validates offsets)
 7) Apply mods only (mods/ -> current rootfs tree)
 q) Quit
```

- **1)** Unpacks a stock `.img` into its component parts (kernel/DTB, header, rootfs payload) under `tmp/`.
- **2)** Extracts the rootfs squashfs payload from step 1 into an editable directory tree (`extracted_rootfs/`).
- **3)** Rebuilds the edited rootfs tree back into a squashfs image.
- **4)** Repacks the kernel/DTB, header, and rebuilt rootfs from step 3 into a flashable `RAX120-CUSTOM.img`.
- **5)** Runs the entire pipeline end-to-end: unpack → extract rootfs → apply mods → pause for manual edits → rebuild → repack. This is what you want for a normal modding run.
- **6)** Round-trips unpack → repack with zero edits and diffs the result against the original, to confirm the pipeline itself is byte-faithful before you trust it with real changes. (Only for testing)
- **7)** Re-applies every enabled script in `mods/` to the current rootfs tree without redoing the unpack/extract steps — useful after tweaking a mod without wanting a full re-run.
- **q)** Quit.

Pick **5) Run full pipeline** for a normal modding run — it unpacks the stock image, extracts the rootfs to a tree, applies any mods, pauses so you can edit files by hand too, then rebuilds and repacks into `RAX120-CUSTOM.img`.

---

## Mods are auto-applied when 5) Run full pipeline is run

Any mod script inside the `mods/` folder gets auto-run during the full pipeline. In short, the full pipeline unpacks the stock fw, extracts the rootfs, applies all mod scripts, then waits for your input to continue.

All mods inside `mods/` follow a strict `NNN-description.sh` naming convention — a file not named like `002-turbo-fan.sh` gets ignored. So if you want to disable a mod, just rename it to break the convention, e.g. `d002-turbo-fan.sh`. See `mods/NNN-template.sh` for more on how each mod script is meant to be written.

---

### Runtime Dependencies required by rax120-toolkit.sh

To simply run the unpacking and repacking scripts using the included pre-built binaries, install the following:
```bash
sudo apt-get install -y fakeroot u-boot-tools patch binwalk
```
(`u-boot-tools` provides `mkimage`/`dumpimage`; `dd`, `sha256sum`, and `cmp` are also required and come from `coreutils`, already present on virtually any Linux install.)

---

## Firmware & GPL Source Archive

GPL source and full stock firmware backups (official NETGEAR links are frequently down) are mirrored here: [RAX120v2 Firmware + GPL Source Archive](https://archive.org/download/netgear-rax120v2-firmware-gpl-source). See `tools/README.md` for exactly which archive member the vendored `tools/` source was pulled from and which patches are applied on top.

---

<details>
<summary><strong>Tools & Dependencies (only needed if the prebuilt bin/ tools fail to run)</strong></summary>

Every binary in `bin/` (`mksquashfs4`, `unsquashfs4`, `bin/firmware-utils/mkdniimg`) is already shipped prebuilt for x86_64, which covers most computers — **you don't need anything in this section unless running one of them throws an exec error, "cannot execute binary file", permission/format issue, or similar** (typically a `glibc` mismatch on a very new/old distro, or a non-x86_64 arch). If that happens, recompile with `make` from the repo root.

The scripts rely on a combination of standard Linux utilities and these vendored, QSDK/GPL-derived binaries, built from NETGEAR's own GPL source. These are what everything above assumes you're using — **official `mksquashfs`/`unsquashfs` from your distro's `squashfs-tools` package don't support this device's xz-compressed rootfs**, so extraction fails outright (`unsquashfs -s` on the stock rootfs errors out) — you must use the local QSDK tools in `bin/`. `mkdniimg` (Netgear's own image-header/checksum tool) has no host equivalent at all — it's required for `repack.sh` to produce a header the router's GUI upload validator will accept.

### Building the Tools (Ubuntu 14.04 / 18.04)

If the prebuilt binaries in `bin/` throw exec errors as described above, compile them from the vendored GPL source in `tools/`:

```bash
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    linux-headers-generic \
    libacl1-dev \
    liblzo2-dev \
    uuid-dev \
    zlib1g-dev \
    liblzma-dev \
    libselinux1-dev
```

### Then to build, run this from root directory.

`make`

`make` (root Makefile's default target just depends on `tools`, so `make` and `make tools` are identical) builds everything under `tools/`: `lzma`, `lzma-old`, `xz`, `squashfs` (v3), `squashfs4`, and `firmware-utils`. There's no way to build only one of those from the root Makefile — `make clean` is the same story in reverse, clearing binaries for all of them out of `bin/`, not just one subdir.

Within that, `tools/firmware-utils` itself only builds `mkdniimg` — the only binary from that vendor tree this pipeline actually calls (see `tools/firmware-utils/Makefile`). The other ~70 firmware-utils tools are unrelated vendor/device targets; their build rules are commented out (not deleted) in that Makefile, so uncomment one there if you ever need it, rather than pulling in its extra deps (`libcrypto`, `-lz`, etc.) for a device that doesn't use it.

*Note: Because these are older, patched QSDK/GPL-era tools, they are highly distro-specific and require legacy libraries that modern distros no longer ship. It's recommended to spin up distrobox with Ubuntu 14 or 18 and use that **only to compile** — once `make` finishes, `exit` the container and run `rax120-toolkit.sh` and everything else on your host machine as normal; the resulting binaries in `bin/` run fine outside the container:

```sh
distrobox create --name ubuntu14 --image ubuntu:14.04   # or ubuntu:18.04
distrobox enter ubuntu14
sudo apt-get update
sudo apt-get install -y build-essential linux-headers-generic \
    libacl1-dev liblzo2-dev uuid-dev zlib1g-dev liblzma-dev libselinux1-dev
make
exit
```

</details>

---

## Important notes

- **Use the vendored tools in `bin/`**, not host `mksquashfs`/`unsquashfs`.
  Host builds don't support this device's xz-compressed rootfs —
  `unsquashfs -s` on the stock rootfs fails with
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

---

## Key env vars (set before running `rax120-toolkit.sh`, or edit its config block)

| Var | Purpose |
|---|---|
| `BIN_DIR_OVERRIDE` | point at a different vendored-tools dir |
| `ROOTFS_EXTRACT_DIR` | where step 2 extracts to (default `out/extracted_rootfs`) |
| `TARGET_ROOTFS_DIR` | pack from a different tree instead (skips extraction) |
| `SPOOF_REPACK_DATE` | `1` = reuse original image's build timestamp on repack |
| `OUTPUT_IMG_NAME` | final repacked image filename |
| `STOCK_FW` | dir to auto-scan for stock `.img` files |
