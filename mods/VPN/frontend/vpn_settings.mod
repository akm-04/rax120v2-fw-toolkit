#!/bin/sh
echo "Content-type: text/html"
echo ""

# Only sourced for NVK_BOOT_DELAY below -- the boot-delay key was made
# modular in vpn_common.env specifically so this is the one place a new
# NVRAM key gets used via its NVK_ name instead of a bare literal.
. /etc/vpn_client/vpn_common.env

read POST_DATA
decode() { printf '%b' "$(echo "$1" | sed 's/+/ /g; s/%\([0-9A-Fa-f][0-9A-Fa-f]\)/\\x\1/g')"; }

PROTOCOL=$(echo "$POST_DATA" | grep -o 'protocol=[^&]*' | cut -d= -f2-)
SCOPE=$(echo "$POST_DATA" | grep -o 'scope_ifaces=[^&]*' | cut -d= -f2-)
AUTO_ENABLE=$(echo "$POST_DATA" | grep -o 'auto_enable=[^&]*' | cut -d= -f2-)
BOOT_DELAY=$(echo "$POST_DATA" | grep -o 'boot_delay_sec=[^&]*' | cut -d= -f2-)
CUSTOM_DNS=$(echo "$POST_DATA" | grep -o 'custom_dns=[^&]*' | cut -d= -f2-)

PROTOCOL_DEC=$(decode "$PROTOCOL")
SCOPE_DEC=$(decode "$SCOPE")
AUTO_ENABLE_DEC=$(decode "$AUTO_ENABLE")
BOOT_DELAY_DEC=$(decode "$BOOT_DELAY")
CUSTOM_DNS_DEC=$(decode "$CUSTOM_DNS")

# Empty is valid -- it means "leave it unset / let custom_vpn.init's
# boot() self-heal to its own default", same as a fresh install. Any
# non-empty value must be a plain non-negative integer; 0 is allowed
# (no delay at all).
case "$BOOT_DELAY_DEC" in
	'') : ;;
	*[!0-9]*)
		echo "ERROR: boot delay must be a whole number of seconds (0 or greater)"
		exit 0
		;;
esac

# Empty is valid -- means no override (see resolve_scope_dns() in
# vpn_common.env: falls back to provider-pushed DNS, then 1.1.1.1). Any
# non-empty value must be a plain dotted-quad IPv4 -- this ends up
# straight in an iptables --to-destination downstream, same "fail here,
# not three scripts later" reasoning as SCOPE_DEC's check below.
if [ -n "$CUSTOM_DNS_DEC" ]; then
	OCTET='(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)'
	if ! echo "$CUSTOM_DNS_DEC" | grep -Eq "^${OCTET}\.${OCTET}\.${OCTET}\.${OCTET}\$"; then
		echo "ERROR: custom DNS must be a valid IPv4 address (e.g. 9.9.9.9), or empty"
		exit 0
	fi
fi

case "$PROTOCOL_DEC" in
	wireguard|openvpn) : ;;
	*)
		echo "ERROR: invalid protocol '${PROTOCOL_DEC}'"
		exit 0
		;;
esac

# scope_ifaces feeds directly into wg_selective_route.sh's iptables/ip
# arguments downstream -- not eval'd, so this isn't shell-injectable, but
# still whitelisted against the real known interfaces so a
# malformed/unexpected value fails loudly here rather than doing
# something confusing three scripts later.
WAN_IFACES=$(get_wan_ifaces)
for IF in $SCOPE_DEC; do
	case " ${VPN_KNOWN_SCOPE_IFACES} " in
		*" ${IF} "*) : ;;
		*)
			echo "ERROR: '${IF}' is not a recognized VPN scope interface"
			exit 0
			;;
	esac
	for W in $WAN_IFACES; do
		if [ "$IF" = "$W" ]; then
			echo "ERROR: '${IF}' is currently your WAN port and can't be scoped"
			exit 0
		fi
	done
done

/bin/config set vpn_client_active_protocol="$PROTOCOL_DEC"

case "$AUTO_ENABLE_DEC" in
	1|0) /bin/config set vpn_client_auto_enable="$AUTO_ENABLE_DEC" ;;
esac

if [ "$PROTOCOL_DEC" = "wireguard" ]; then
	/bin/config set vpn_client_wg_scope_ifaces="$SCOPE_DEC"
else
	/bin/config set vpn_client_custom_ovpn_scope_ifaces="$SCOPE_DEC"
fi

# Empty means "don't touch it" -- leaves any existing value (or absence
# of one) alone so custom_vpn.init's boot() keeps doing the self-healing
# it already does, rather than this endpoint racing it by writing "" and
# forcing a re-derive every save.
if [ -n "$BOOT_DELAY_DEC" ]; then
	/bin/config set ${NVK_BOOT_DELAY}="$BOOT_DELAY_DEC"
fi

# Unlike BOOT_DELAY above, empty here means "explicitly clear the
# override", not "leave whatever was there alone" -- otherwise there'd be
# no way to remove a previously-set custom DNS from this form. Writes the
# "0" sentinel resolve_scope_dns() already treats as equivalent to empty
# (see its comment in vpn_common.env) rather than an empty string, purely
# for consistency with that existing convention.
/bin/config set ${NVK_SCOPE_DNS}="${CUSTOM_DNS_DEC:-0}"

/bin/config commit
echo "OK: settings saved (protocol=${PROTOCOL_DEC}, scope=${SCOPE_DEC:-whole-router}, boot_delay=${BOOT_DELAY_DEC:-default}, custom_dns=${CUSTOM_DNS_DEC:-auto})"
