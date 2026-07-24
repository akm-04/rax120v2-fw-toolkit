#!/bin/sh
echo "Content-type: text/html"
echo ""

. /etc/vpn_client/vpn_common.env

if [ -n "$CONTENT_LENGTH" ] && [ "$CONTENT_LENGTH" -gt "$MAX_UPLOAD_BYTES" ] 2>/dev/null; then
	echo "ERROR: upload too large (${CONTENT_LENGTH} bytes, limit ${MAX_UPLOAD_BYTES})"
	exit 0
fi

RAW=/tmp/raw_ovpn_upload.bin
EXTRACTED=/tmp/ovpn_upload_extracted.conf

head -c "$MAX_UPLOAD_BYTES" | tr -d '\r' > "$RAW"

BOUNDARY=$(echo "$CONTENT_TYPE" | sed 's/.*boundary=//;s/[[:space:]].*//')

awk -v b="$BOUNDARY" '
BEGIN { in_file=0; found=0 }
$0 ~ ("^--" b "--")  { exit }
$0 ~ ("^--" b)       { if(found) in_file=0; found=1; next }
found && !in_file && /^$/ { in_file=1; next }
in_file { print }
' "$RAW" > "$EXTRACTED"

rm -f "$RAW"

if [ -s "$EXTRACTED" ]; then
	/etc/vpn_client/ovpn_update_config.sh "$EXTRACTED"
else
	echo "ERROR: upload failed or empty"
fi

rm -f "$EXTRACTED"
