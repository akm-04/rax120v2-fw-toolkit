All patches imported from Netgear RAX120v2 GPL source (`RAX120v2v1-V1.2.2.24_gpl_src.tar.bz2/RAX120-V1.2.2.24_gpl_src/tools/`):
Note: I had to download gpl from internet archieve, official links were not working.
Link: https://archive.org/download/netgear-rax120v2-firmware-gpl-source

### Build Dependencies (Ubuntu 14.04)
It's suggested to use distrobox and spin up ubuntu-14
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
### To build, run this from root directory.
make tools

Note: `make tools` only compiles `mkdniimg` out of `tools/firmware-utils` — it's the
only binary from that vendor tree the unpack/repack pipeline actually calls
(see `tools/firmware-utils/Makefile`). The rest of firmware-utils' tools
are unrelated vendor/device targets, intentionally left uncompiled so you
don't need their extra deps just for this device.
