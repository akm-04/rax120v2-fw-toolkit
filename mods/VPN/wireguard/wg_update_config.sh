#!/bin/sh
#
# wg_update_config.sh — parse a WireGuard client .conf and persist its
# fields to NVRAM via /bin/config, one key per field.
#
# Pure backend logic, callable standalone for telnet testing:
#   wg_update_config.sh /tmp/custom.conf
#
# upload_wg.mod does the multipart extraction and hands the resulting
# file to this script -- no parsing logic duplicated in the CGI. Note
# this script re-checks size itself (WG_CONF_MAX_BYTES) rather than
# trusting the CGI already did -- it can be invoked directly, bypassing
# the CGI's own guard entirely, so it can't assume that guard ran.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "${SCRIPT_DIR}/wireguard_client.env"
CONFIG=/bin/config

log()
{
	mkdir -p "$wg_client_data_dir" 2>/dev/null
	echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$wg_client_log_file"
}

usage()
{
	echo "Usage: ${0##*/} <path-to-wg-conf>" >&2
	exit 1
}

fail()
{
	echo "ERROR: $1" >&2
	log "config save failed: $1"
	exit 1
}

[ -n "$1" ] || usage
[ -f "$1" ] || fail "file not found: $1"

# --- size guard, before anything else touches the file ------------------
filesize=$(wc -c < "$1" 2>/dev/null | tr -d ' ')
[ -n "$filesize" ] || fail "could not determine file size"
[ "$filesize" -gt 0 ] || fail "file is empty"
if [ "$filesize" -gt "$WG_CONF_MAX_BYTES" ]; then
	fail "config is ${filesize} bytes, exceeds ${WG_CONF_MAX_BYTES} byte limit -- refusing to parse (not a valid WireGuard config, or something unexpected was uploaded)"
fi

# normalize line endings so parsing doesn't choke on CRLF uploads
clean_conf="${wg_client_data_dir}/upload_clean.tmp"
mkdir -p "$wg_client_data_dir"
tr -d '\r' < "$1" > "$clean_conf"

# @1 section name (Interface|Peer), @2 key name
get_field()
{
	awk -v section="$1" -v key="$2" '
		/^\[/ { insec = ($0 ~ ("\\[" section "\\]")) ; next }
		insec && $0 ~ ("^[ \t]*" key "[ \t]*=") {
			sub(/^[^=]*=[ \t]*/, "")
			sub(/[ \t]*(#.*)?$/, "")
			print
			exit
		}
	' "$clean_conf"
}

PRIVATEKEY=$(get_field Interface PrivateKey)
ADDRESS=$(get_field Interface Address)
DNS=$(get_field Interface DNS)
PEER_PUBKEY=$(get_field Peer PublicKey)
ENDPOINT=$(get_field Peer Endpoint)
ALLOWEDIPS=$(get_field Peer AllowedIPs)
KEEPALIVE=$(get_field Peer PersistentKeepalive)

rm -f "$clean_conf"

# --- validation -------------------------------------------------------
# WireGuard keys are 32 raw bytes, base64-encoded -> 44 chars, always
# ending in '='. This is a format sanity check, not a cryptographic one.
is_valid_key()
{
	echo "$1" | grep -Eq '^[A-Za-z0-9+/]{43}=$'
}

[ -n "$PRIVATEKEY" ]  || fail "missing PrivateKey in [Interface]"
is_valid_key "$PRIVATEKEY" || fail "PrivateKey is not a valid base64 WireGuard key"

[ -n "$PEER_PUBKEY" ] || fail "missing PublicKey in [Peer]"
is_valid_key "$PEER_PUBKEY" || fail "Peer PublicKey is not a valid base64 WireGuard key"

[ -n "$ENDPOINT" ] || fail "missing Endpoint in [Peer]"
echo "$ENDPOINT" | grep -Eq '^[^:]+:[0-9]+$' || fail "Endpoint must be host:port (got: $ENDPOINT)"

# Address is technically optional -- some providers assign it out-of-band
# rather than in the config file itself (confirmed by the actual test
# config used throughout this project, which has never had one). If
# present, normalize to CIDR; if absent, leave empty and let wg_ctrl.sh
# skip the `ip address add` step rather than guessing a value.
if [ -n "$ADDRESS" ]; then
	case "$ADDRESS" in
		*/*) : ;;
		*) ADDRESS="${ADDRESS}/32" ;;
	esac
fi

ALLOWEDIPS="${ALLOWEDIPS:-0.0.0.0/0}"
KEEPALIVE="${KEEPALIVE:-25}"
case "$KEEPALIVE" in
	''|*[!0-9]*) fail "PersistentKeepalive must be numeric (got: $KEEPALIVE)" ;;
esac

# --- persist ------------------------------------------------------------
$CONFIG set ${NVK_PRIVATEKEY}="$PRIVATEKEY"
$CONFIG set ${NVK_ADDRESS}="$ADDRESS"
$CONFIG set ${NVK_DNS}="$DNS"
$CONFIG set ${NVK_PEER_PUBKEY}="$PEER_PUBKEY"
$CONFIG set ${NVK_ENDPOINT}="$ENDPOINT"
$CONFIG set ${NVK_ALLOWEDIPS}="$ALLOWEDIPS"
$CONFIG set ${NVK_KEEPALIVE}="$KEEPALIVE"
$CONFIG set ${NVK_CONFIGURED}="1"
$CONFIG commit

echo "OK: WireGuard config parsed and saved to NVRAM"
echo "    Address=${ADDRESS} Endpoint=${ENDPOINT} AllowedIPs=${ALLOWEDIPS} Keepalive=${KEEPALIVE}"
log "config saved: endpoint=${ENDPOINT} address=${ADDRESS:-none}"
