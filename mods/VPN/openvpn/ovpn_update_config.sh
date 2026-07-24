#!/bin/sh
#
# ovpn_update_config.sh — validate an uploaded OpenVPN client .ovpn file
# and persist it to NVRAM as a base64 blob.
#
# Unlike wg_update_config.sh, this does NOT parse individual directives
# into separate NVRAM fields -- .ovpn's schema is open-ended (arbitrary
# directives, inline <ca>/<cert>/<key>/<tls-auth> PEM blocks that can
# legitimately run to several KB each), so there's no small fixed set of
# fields to extract the way WireGuard's four-field schema allows. This
# mirrors the OEM's own approach of storing the whole validated file, but
# adds the size + sanity guards the OEM's upload_ovpn.mod never had.
#
# Callable standalone for telnet testing:
#   ovpn_update_config.sh /tmp/custom.ovpn

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "${SCRIPT_DIR}/ovpn_client_custom.env"
CONFIG=/bin/config

log()
{
	mkdir -p "$ovpn_client_data_dir" 2>/dev/null
	echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$ovpn_client_log_file"
}

usage()
{
	echo "Usage: ${0##*/} <path-to-ovpn-conf>" >&2
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

# --- size guard -----------------------------------------------------
filesize=$(wc -c < "$1" 2>/dev/null | tr -d ' ')
[ -n "$filesize" ] || fail "could not determine file size"
[ "$filesize" -gt 0 ] || fail "file is empty"
if [ "$filesize" -gt "$OVPN_CONF_MAX_BYTES" ]; then
	fail "config is ${filesize} bytes, exceeds ${OVPN_CONF_MAX_BYTES} byte limit -- refusing (not a valid .ovpn file, or something unexpected was uploaded)"
fi

# --- binary/format sanity checks -------------------------------------
# a real .ovpn is plain text. Rather than searching for NUL specifically
# (which needs a bash/ksh-only $'\x00' construct -- not safe under this
# firmware's ash/BusyBox #!/bin/sh), flag any non-printable, non-whitespace
# byte at all. Broader than a NUL-only check and still POSIX (BusyBox grep
# supports [:print:]/[:space:]), so it also catches small-but-binary
# uploads that happen not to contain a literal NUL.
if LC_ALL=C grep -q '[^[:print:][:space:]]' "$1" 2>/dev/null; then
	fail "file contains non-text data -- not a valid .ovpn config"
fi

# a client .ovpn must at minimum declare it's a client and where to
# connect. Absence of either is a strong signal this isn't really an
# OpenVPN client config, whatever else it might be.
grep -Eq '^[[:space:]]*client[[:space:]]*(#.*)?$' "$1" \
	|| fail "missing 'client' directive -- not a client-mode .ovpn config"
grep -Eq '^[[:space:]]*remote[[:space:]]+[^[:space:]]+' "$1" \
	|| fail "missing 'remote' directive -- no server to connect to"

# --- persist ------------------------------------------------------------
mkdir -p "$ovpn_client_data_dir"
BLOB=$(base64 "$1" | tr -d '\n')

$CONFIG set ${NVK_OVPN_BLOB}="$BLOB"
$CONFIG set ${NVK_OVPN_CONFIGURED}="1"
$CONFIG commit

echo "OK: OpenVPN config validated (${filesize} bytes) and saved to NVRAM"
log "config saved: ${filesize} bytes"
