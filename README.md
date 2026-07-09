# RAX120v2 — rootfs extraction / modification / repack notes

Working notes for pulling, modifying, and reflashing the `rootfs` (mtd26)
partition on a NETGEAR RAX120v2 (IPQ8074A, Chaos Calmer derivative,
kernel 4.4.60). Keep this updated as the source of truth for what's been
changed and why — the router itself has no persistent overlay, so this
repo + the `out/` builds are the only durable record of any modification.

## Device reference

```
mtd24: 06400000  firmware   (kernel + rootfs combined image, GUI upload target)
mtd25: 00620000  kernel     (uImage)
mtd26: 05de0000  rootfs     (squashfs, xz, block size 262144, ~93.87 MB partition)
```

Confirmed on-device facts (do not re-derive these from scratch, they're settled):

- No dm-verity. `/proc/cmdline` has no `root_hash=`/`dm-mod.create=`, `dmesg`
  has no verity references, `/dev/root` mounts squashfs directly (not via a
  `/dev/dm-*` mapper device). `dm_req_crypt` module present is Qualcomm FDE,
  unrelated.
- No persistent overlay. Root is `overlayfs:/tmp/root/root` on tmpfs — every
  live change (killing a daemon, editing a file at runtime) is lost on
  reboot. Anything permanent must go through this repack-and-flash process.
- `vol_armor` (Bitdefender payload, ~52 MB) lives in a **separate UBI
  volume** (mtd38), not inside the squashfs rootfs. Removing the launcher
  from rootfs stops it from *starting*; it doesn't remove the volume itself.

## 1. Pull rootfs off the router

On the router:
```sh
dd if=/dev/mtd26 bs=1M 2>/dev/null | nc <laptop_ip> 9999
```

On the laptop, listening first:
```sh
nc -l -p 9999 > rootfs.squashfs.orig
```

Note: this captures the **full partition**, including trailing `0xFF` pad
out to the 98,435,072-byte (0x5de0000) partition boundary. The real
squashfs image inside it is smaller (~57 MB) — `unsquashfs -s` on the dump
will report the true filesystem size. Never write the padded `.orig` file
back with `nandwrite`; only ever write a freshly built image.

## 2. Extract

Stock `unsquashfs` fails on this image:
```
xz: error reading stored compressor options from filesystem!
```
This is a vendor-SDK `mksquashfs` quirk (non-standard compressor-options
block), not corruption. `binwalk`'s bundled `sasquatch` extractor tolerates
it:

```sh
binwalk -e rootfs.squashfs.orig
# -> rootfs.squashfs.orig.extracted/0/squashfs-root/
```

Move/copy the resulting `squashfs-root/` into the repo root so
`build_rootfs.sh` can find it:
```sh
cp -a rootfs.squashfs.orig.extracted/0/squashfs-root ./squashfs-root
```

## 3. Original image reference values

For sanity-checking any rebuild against the stock image:

```
Compression:      xz
Block size:        262144
Fragments:          252
Inodes:             4585
Xattrs:              not stored
Uid/Gid:              single owner, root (0)
```

33 paths will always show up as "differs"/"missing" in any `diff -rq`
against a live-mounted rebuild — these are **dangling symlinks by design**
(e.g. `/etc/passwd -> /tmp/config/passwd`, `/sbin/apsched -> /sbin/net-util`),
targets populated by init scripts at boot, not present in a static,
unbooted rootfs. Confirmed via `find squashfs-root -xtype l | wc -l` = 33,
matching exactly. Anything beyond that known set of 33 in a diff is a real
regression worth investigating.

## 4. Make edits

Edit files directly under `squashfs-root/`. Prefer the smallest possible
diff — e.g. removing an `/etc/rc.d/S9x...` symlink rather than modifying
the target script under `/etc/init.d/`, so changes are easy to review and
revert individually.

## 5. Repack

```sh
./build_rootfs.sh [optional-label]
```
Produces `out/rootfs_<timestamp>[_label].squashfs`, updates
`out/checksums.sha256`, and symlinks `out/rootfs_latest.squashfs`.

## 6. Local validation (before touching hardware)

```sh
mkdir -p /tmp/mnt_test
sudo mount -t squashfs -o loop out/rootfs_latest.squashfs /tmp/mnt_test
diff -rq squashfs-root /tmp/mnt_test
sudo umount /tmp/mnt_test
```
Expect silence, or exactly the 33 known dangling-symlink lines from §3.

## 7. Transfer and flash

```sh
# laptop
nc -l -p 9999 < out/rootfs_latest.squashfs

# router
nc <laptop_ip> 9999 > /tmp/rootfs.squashfs
sha256sum /tmp/rootfs.squashfs      # compare against out/checksums.sha256

flash_erase /dev/mtd26 0 0
nandwrite -p /dev/mtd26 /tmp/rootfs.squashfs

# verify BEFORE reboot — read back exactly the written byte count
dd if=/dev/mtd26 bs=1 count=$(stat -c%s /tmp/rootfs.squashfs) 2>/dev/null | sha256sum
# only reboot if this matches the transferred file's hash
```

## Rollback

Keep `rootfs.squashfs.orig` (the original padded dump from §1) somewhere
safe outside this repo (it's large; not meant to be committed — see
`.gitignore`). To revert to stock:
```sh
flash_erase /dev/mtd26 0 0
nandwrite -p /dev/mtd26 rootfs.squashfs.orig
```
Confirm NMRP recovery mode works on this unit *before* you need it
(power-on + hold reset until LED behavior changes) — that's the real
fallback if a flash goes wrong, not the `.orig` file alone.

## Do not touch

`DEVCFG`/`DEVCFG_1` (mtd6/7, SMMU config — verified via mtd6 strings dump
to contain `disable_smmu_ac`, tied to the 1.2.9.52 reboot fix), `ART`/`ART.bak`
(per-device RF calibration), `boarddata1/2`, `CDT`/`CDT_1`, and the entire
early boot chain (`SBL1`, `MIBIB`, `BOOTCONFIG*`, `QSEE*`, `RPM*`,
`APPSBLENV`, `APPSBL*`) — none of these are touched by this rootfs
workflow, and none should be, short of deliberate, separately-researched
changes.
