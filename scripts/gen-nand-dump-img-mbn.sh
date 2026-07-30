#!/bin/bash

exit_with_message()
{
	echo "Usage: $0 nand-partion.conf nand-dump.img.mbn"
	echo "example: $0 gen-nand-dump-img-mbn.conf nand-dump.img.mbn"
	exit
}

hexswap() {
	echo -n ${1:6:2}${1:4:2}${1:2:2}${1:0:2}
}

partition_conf="$1"
mbn_file="$2"

[ $# = 2 ] || exit_with_message
[ -f "$partition_conf" ] || exit_with_message
rm -f "$mbn_file"

blocksize=0x20000
index=0
cat "$partition_conf" | sed '1d'| while read no name start size datasize subimage; do
	echo "Generating partition $name"
	index=$(($index + 1))
	mbnstart=$(($start / $blocksize))
	mbnend=$(($size / $blocksize + $mbnstart - 1))
	mbndata=$(($datasize / $blocksize))

	mbnindex=$(printf %08x $index)
	mbnstart=$(printf %08x $mbnstart)
	mbnend=$(printf %08x $mbnend)
	mbndata=$(printf %08x $mbndata)

	mbnline="00000000: $(hexswap $mbnstart) $(hexswap $mbnend) $(hexswap $mbndata) ffffff${mbnindex:6:2}"
	echo $mbnline
	echo $mbnline | xxd -r >> "$mbn_file"
done
echo "ending"
echo "00000000: ffffffff ffffffff ffffffff ffffffff"
echo "00000000: ffffffff ffffffff ffffffff ffffffff" | xxd -r >> "$mbn_file"

