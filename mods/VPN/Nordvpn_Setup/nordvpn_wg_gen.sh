#!/bin/sh
#
# nordvpn_wg_gen.sh — turn a NordVPN access token + chosen country into a
# saved WireGuard peer, by building a synthetic .conf and handing it to
# wg_update_config.sh (the SAME parser/validator upload_wg.mod uses). This
# script never touches NVRAM directly for the peer fields — it delegates
# that entirely to wg_update_config.sh, so a NordVPN-generated config and a
# manually-uploaded one end up in an identical NVRAM state.
#
# The NordVPN *account token* is persisted in NVRAM, ENCRYPTED, so it
# survives reboots without needing to be re-pasted every time. Key material
# is derived from this device's own serial number (via `artmtd -r sn`) --
# this raises the bar against casual exposure (a `config show` glance, a
# firmware backup, a log line) but is explicitly NOT a defense against
# anyone who already has root shell on this device: the decryption logic
# necessarily lives right here, reachable by that same access level. There
# is no hardware secure element on this SoC to do better than that. A
# tmpfs-only session cache (/tmp/nordvpn_token) avoids re-deriving the key
# and re-decrypting on every single call within one uptime.
#
# Standalone usage (same telnet-testable pattern as every other backend
# script in /etc/vpn_client/):
#   nordvpn_wg_gen.sh token_status
#   nordvpn_wg_gen.sh set_token <token>
#   nordvpn_wg_gen.sh clear_token
#   nordvpn_wg_gen.sh list_countries
#   nordvpn_wg_gen.sh generate <country_id>
#
# Output:
#   token_status    -> "not_set" | "verified" | "invalid"
#   set_token       -> "OK" or "ERROR: <reason>" (validates against the API
#                      before persisting -- a rejected token never gets
#                      written to NVRAM)
#   clear_token     -> "OK"
#   list_countries  -> one "<id>|<name>" pair per line, or "ERROR: <reason>"
#   generate        -> "OK: ..." (see wg_update_config.sh) or "ERROR: <reason>"
#
# CGI layer (nordvpn_gen.mod) just proxies these verbs and formats the
# result as JSON -- no NordVPN-specific logic lives in the CGI file. It
# NEVER echoes the token or its encrypted form back to the frontend, only
# the token_status tri-state -- the setup page must never show a
# previously-saved key, even in encrypted/masked form.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "${SCRIPT_DIR}/wireguard_client.env"

NORD_API="https://api.nordvpn.com/v1"
NORD_TECH_ID=35            # NordLynx / WireGuard
NORD_TOKEN_FILE="/tmp/nordvpn_token"        # tmpfs session cache only
NORD_KEYMAT_FILE="/tmp/.nordvpn_keymat"     # tmpfs session cache only
NORD_CURL_TIMEOUT=8

NORD_TOKEN_NVRAM_KEY="vpn_client_nordvpn_token_enc"
NORD_VERIFIED_NVRAM_KEY="vpn_client_nordvpn_verified"

CONFIG="/bin/config"        # matches the exact convention used elsewhere
                             # in this codebase (e.g. vpn_auth.mod)

CURL="curl -s -m ${NORD_CURL_TIMEOUT}"

fail()
{
	echo "ERROR: $1" >&2
	exit 1
}

require_jsonfilter()
{
	command -v jsonfilter >/dev/null 2>&1 || fail "jsonfilter not found on this firmware"
}

require_openssl()
{
	command -v openssl >/dev/null 2>&1 || fail "openssl not found on this firmware"
}

# --- key material / encrypt / decrypt --------------------------------------
#
# Determinism is what matters here, not human-readable parsing -- this
# passphrase is never displayed or typed by anyone, only ever fed straight
# into openssl's KDF, so it just needs to be the same string every time on
# this device. Confirmed live output shape for `artmtd -r sn`:
#   sn:6K011852A0226
#   SN: 6K011852A0226
# (two lines, one lowercase-no-space, one uppercase-with-space). The MAC
# read's exact line shape hasn't been confirmed -- rather than guess at
# parsing it and risk different runs producing different key material if
# the guess is wrong, the *entire* raw output of both reads gets folded in.
# Bad/partial parsing here is harmless (still deterministic); an empty
# read from both is the only real failure mode.

nordvpn_key_material()
{
	if [ -s "$NORD_KEYMAT_FILE" ]; then
		cat "$NORD_KEYMAT_FILE"
		return 0
	fi

	SN_RAW=$(artmtd -r sn 2>/dev/null)
	MAC_RAW=$(artmtd -r mac 2>/dev/null)

	if [ -z "$SN_RAW" ] && [ -z "$MAC_RAW" ]; then
		return 1
	fi

	KM="nordvpn-wg-gen:${SN_RAW}:${MAC_RAW}"
	umask 077
	printf '%s' "$KM" > "$NORD_KEYMAT_FILE"
	printf '%s' "$KM"
}

nordvpn_encrypt()
{
	# $1 = plaintext -> stdout: base64 ciphertext, single line
	KM=$(nordvpn_key_material) || return 1
	printf '%s' "$1" | openssl enc -aes-256-cbc -md sha256 -a -A -salt -pass "pass:${KM}" 2>/dev/null
}

nordvpn_decrypt()
{
	# $1 = base64 ciphertext -> stdout: plaintext
	KM=$(nordvpn_key_material) || return 1
	printf '%s' "$1" | openssl enc -aes-256-cbc -md sha256 -a -A -d -salt -pass "pass:${KM}" 2>/dev/null
}

# Resolves the active token transparently: tmpfs session cache first (fast
# path, avoids re-deriving key material + re-decrypting on every call
# within one uptime), else decrypt from NVRAM and repopulate the cache.
# Returns 1 (empty stdout) if nothing is set or decryption fails.

nordvpn_get_token()
{
	if [ -s "$NORD_TOKEN_FILE" ]; then
		cat "$NORD_TOKEN_FILE"
		return 0
	fi

	ENC=$($CONFIG get "$NORD_TOKEN_NVRAM_KEY" 2>/dev/null)
	[ -n "$ENC" ] || return 1

	TOKEN=$(nordvpn_decrypt "$ENC") || return 1
	[ -n "$TOKEN" ] || return 1

	umask 077
	echo "$TOKEN" > "$NORD_TOKEN_FILE"
	printf '%s' "$TOKEN"
}

# --- token_status / set_token / clear_token --------------------------------
# The CGI layer (and thus the setup page) NEVER receives the token or its
# encrypted form -- only this tri-state, so a re-opened setup page can show
# "Verified" without ever displaying (or being able to display) what was
# actually pasted.

do_token_status()
{
	ENC=$($CONFIG get "$NORD_TOKEN_NVRAM_KEY" 2>/dev/null)
	if [ -z "$ENC" ]; then
		echo "not_set"
		return 0
	fi
	VERIFIED=$($CONFIG get "$NORD_VERIFIED_NVRAM_KEY" 2>/dev/null)
	if [ "$VERIFIED" = "1" ]; then
		echo "verified"
	else
		echo "invalid"
	fi
}

# Three-stage gate before anything ever reaches NVRAM:
#   1. Cheap local shape check (length + charset) -- catches "pasted the
#      wrong thing entirely" instantly, no network call spent on it, and
#      hard-caps how large a string can ever reach curl/openssl/NVRAM.
#   2. Real API validation -- authoritative check that it's an actual
#      working token, not just correctly-shaped.
#   3. Only after both pass does anything get written to NVRAM at all.

do_set_token()
{
	TOKEN="$1"
	[ -n "$TOKEN" ] || fail "no token provided"
	require_jsonfilter
	require_openssl

	# Real NordVPN access tokens are 64 lowercase hex chars. Bounds are
	# deliberately a little loose (32-128) to tolerate a format change
	# without immediately breaking, while still hard-rejecting "pasted a
	# paragraph by mistake" style input before it touches the network,
	# curl's argv, or -- if it somehow got that far -- NVRAM itself.
	TOKEN_LEN=${#TOKEN}
	if [ "$TOKEN_LEN" -lt 32 ] || [ "$TOKEN_LEN" -gt 128 ]; then
		fail "token has an unexpected length (${TOKEN_LEN} chars) -- expected around 64 hex characters, refusing to even attempt verification"
	fi
	case "$TOKEN" in
		*[!0-9a-fA-F]*)
			fail "token contains non-hex characters -- expected a plain hex access token, refusing to even attempt verification"
			;;
	esac

	RESP=$($CURL -u "token:${TOKEN}" "${NORD_API}/users/services/credentials")
	[ -n "$RESP" ] || fail "empty response from NordVPN (network issue or invalid token)"

	PK=$(echo "$RESP" | jsonfilter -e '@.nordlynx_private_key' 2>/dev/null)
	if [ -z "$PK" ]; then
		$CONFIG set "${NORD_VERIFIED_NVRAM_KEY}=0"
		$CONFIG commit
		fail "token rejected or account has no NordLynx key"
	fi

	ENC=$(nordvpn_encrypt "$TOKEN")
	[ -n "$ENC" ] || fail "encryption failed -- check openssl and artmtd are both working (artmtd -r sn)"

	$CONFIG set "${NORD_TOKEN_NVRAM_KEY}=${ENC}"
	$CONFIG set "${NORD_VERIFIED_NVRAM_KEY}=1"
	$CONFIG commit

	# refresh the session cache so this same call doesn't immediately
	# re-decrypt on the very next generate/list_countries
	umask 077
	echo "$TOKEN" > "$NORD_TOKEN_FILE"

	echo "OK"
}

do_clear_token()
{
	# no confirmed `config delete` subcommand anywhere in this codebase --
	# setting empty is the confirmed-safe operation, and do_token_status
	# already treats an empty value as not_set.
	$CONFIG set "${NORD_TOKEN_NVRAM_KEY}="
	$CONFIG set "${NORD_VERIFIED_NVRAM_KEY}=0"
	$CONFIG commit
	rm -f "$NORD_TOKEN_FILE"
	echo "OK"
}

# --- list_countries ---------------------------------------------------------
# No token needed -- this endpoint is public. jsonfilter has no predicate
# filtering (no jq-style `select()`), so pairing id+name per object is done
# by pulling both arrays in full (order-preserved) and zipping with awk,
# rather than one jsonfilter call per array index (which would mean 2x the
# country count in subprocess spawns -- noticeably slower on embedded).

do_list_countries()

{
	require_jsonfilter

	RESP=$($CURL "${NORD_API}/servers/countries")
	[ -n "$RESP" ] || fail "empty response fetching country list"

	IDS_FILE="/tmp/nordvpn_ids.$$"
	NAMES_FILE="/tmp/nordvpn_names.$$"

	echo "$RESP" | jsonfilter -e '@[*].id'   > "$IDS_FILE"   2>/dev/null
	echo "$RESP" | jsonfilter -e '@[*].name' > "$NAMES_FILE" 2>/dev/null

	if [ ! -s "$IDS_FILE" ] || [ ! -s "$NAMES_FILE" ]; then
		rm -f "$IDS_FILE" "$NAMES_FILE"
		fail "could not parse country list (unexpected API response shape)"
	fi

	# paste isn't available on this firmware (BusyBox build without it) --
	# awk zip of the two line-ordered files instead, same result.
	awk 'NR==FNR { ids[FNR] = $0; next } { print ids[FNR] "|" $0 }' "$IDS_FILE" "$NAMES_FILE"

	rm -f "$IDS_FILE" "$NAMES_FILE"
}

# --- ping_country ------------------------------------------------------------
# Lazy, per-row latency check for the country table -- deliberately NOT run
# for all countries upfront (that's ~180 sequential recommendations+ping
# round trips, tens of seconds before the table could even render). Public
# endpoint, no token required, so this works even before a key is set.

do_ping_country()
{
	require_jsonfilter
	COUNTRY_ID="$1"
	case "$COUNTRY_ID" in
		''|*[!0-9]*) fail "country_id must be numeric (got: $COUNTRY_ID)" ;;
	esac

	REC_URL="${NORD_API}/servers/recommendations?filters[country_id]=${COUNTRY_ID}&filters[servers_technologies][pivot][status]=online&filters[servers_technologies][id]=${NORD_TECH_ID}&limit=1"
	REC_RESP=$($CURL -g "$REC_URL")
	[ -n "$REC_RESP" ] || fail "empty response fetching server"

	HOST=$(echo "$REC_RESP" | jsonfilter -e '@[0].hostname' 2>/dev/null)
	[ -n "$HOST" ] || fail "no server found for country_id=${COUNTRY_ID}"

	# BusyBox ping's summary line: "round-trip min/avg/max = a/b/c ms"
	# (no mdev field, unlike GNU ping's "rtt min/avg/max/mdev"). The grep
	# pattern below only captures the first three slash-separated numbers
	# either way, so it's tolerant of both shapes.
	PING_OUT=$(ping -c 3 -W 2 "$HOST" 2>/dev/null)
	AVG=$(echo "$PING_OUT" | grep -o '[0-9.]*/[0-9.]*/[0-9.]*' | head -1 | awk -F'/' '{print $2}')
	[ -n "$AVG" ] || fail "ping failed or host unreachable (${HOST})"

	echo "$AVG"
}

# --- generate ---------------------------------------------------------------

do_generate()
{
	require_jsonfilter
	COUNTRY_ID="$1"
	case "$COUNTRY_ID" in
		''|*[!0-9]*) fail "country_id must be numeric (got: $COUNTRY_ID)" ;;
	esac

	TOKEN=$(nordvpn_get_token) || fail "no token set -- run set_token first"
	[ -n "$TOKEN" ] || fail "no token set -- run set_token first"

	# 1. best server for this country, WireGuard-capable, online only
	#    (same filter shape as TP-Link's own firmware, confirmed by decompile)
	REC_URL="${NORD_API}/servers/recommendations?filters[country_id]=${COUNTRY_ID}&filters[servers_technologies][pivot][status]=online&filters[servers_technologies][id]=${NORD_TECH_ID}&limit=1"
	REC_RESP=$($CURL -g "$REC_URL")
	[ -n "$REC_RESP" ] || fail "empty response fetching recommended server"

	SERVER_ID=$(echo "$REC_RESP" | jsonfilter -e '@[0].id' 2>/dev/null)
	SERVER_NAME=$(echo "$REC_RESP" | jsonfilter -e '@[0].name' 2>/dev/null)
	[ -n "$SERVER_ID" ] || fail "no WireGuard servers available for country_id=${COUNTRY_ID}"

	# 2. this endpoint hands back a COMPLETE, ready-to-use wg-quick style
	#    config -- [Interface] with the account's private key already
	#    filled in, [Peer] with that specific server's public key/endpoint.
	#    No manual reconstruction needed; confirmed live against the real
	#    API (this used to be guesswork -- it's not anymore).
	CONF_URL="${NORD_API}/servers/${SERVER_ID}/technologies/${NORD_TECH_ID}/configurations"
	CONF_RESP=$($CURL -u "token:${TOKEN}" "$CONF_URL")
	[ -n "$CONF_RESP" ] || fail "empty response fetching peer configuration"

	# sanity check before handing off to the parser -- catches an expired
	# token or an API error page landing here instead of a real conf
	echo "$CONF_RESP" | grep -q '^\[Interface\]' || fail "unexpected response from configurations endpoint (not a WireGuard config -- token may have expired)"

	mkdir -p "$wg_client_data_dir"
	CANDIDATE="${wg_client_data_dir}/nordvpn_candidate.conf"
	umask 077
	echo "$CONF_RESP" > "$CANDIDATE"

	# Hand off to the existing validated parser -- same call upload_wg.mod
	# makes. wg_update_config.sh writes "OK: ..." to stdout on success,
	# "ERROR: ..." to stderr only on failure -- merge with 2>&1 or the
	# failure reason is silently lost.
	RESULT=$("${SCRIPT_DIR}/wg_update_config.sh" "$CANDIDATE" 2>&1)
	STATUS=$?
	rm -f "$CANDIDATE"

	if [ "$STATUS" -eq 0 ]; then
		echo "OK: Server=${SERVER_NAME:-$SERVER_ID} ${RESULT}"
	else
		echo "ERROR: wg_update_config.sh rejected the generated config: ${RESULT}"
		exit 1
	fi
}

case "$1" in
	token_status)   do_token_status ;;
	set_token)      shift; do_set_token "$1" ;;
	clear_token)    do_clear_token ;;
	list_countries) do_list_countries ;;
	ping_country)   shift; do_ping_country "$1" ;;
	generate)       shift; do_generate "$1" ;;
	*)
		echo "Usage: ${0##*/} token_status | set_token <token> | clear_token | list_countries | ping_country <country_id> | generate <country_id>" >&2
		exit 1
		;;
esac
