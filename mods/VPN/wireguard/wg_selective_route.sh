#!/bin/sh
#
# wg_selective_route.sh — route only specified bridge-port interfaces
# through a VPN tunnel, leaving all other br0 clients on the normal WAN
# default route. Tunnel-device-agnostic (takes it as an argument), so
# it's used by both wg_ctrl.sh (pointed at wg0) and ovpn_ctrl.sh
# (pointed at tun90) for their guest-scoped mode.
#
# Also handles DNS for scoped clients: DNAT's their port 53 traffic to
# a fixed resolver rather than trying to make dnsmasq hand out different
# servers to different clients. dnsmasq on this router doesn't serve DHCP
# at all (confirmed live: udhcpd does that job), and the four VAPs are
# bridged onto one L2 segment/subnet anyway, so DHCP-tag-based per-SSID
# DNS was a dead end regardless. DNAT reuses the exact same bridge-
# netfilter + physdev-match machinery already proven for the routing
# mark above -- no new subsystem. Confirmed live on RAX120v2 with a real
# DNS leak test (multiple servers/locations -> exactly 2, both matching
# the tunnel's exit location) before this was ever scripted; note this
# is inserted at PREROUTING position 1 specifically to take priority
# over this router's own pre-existing parental-control-style DNS
# hijack/REDIRECT rules already present in that chain.
#
# Also fully blocks IPv6 for scoped interfaces, not just DNS. This
# router auto-establishes a 6to4 tunnel (visible as sit1) even on an
# IPv4-only ISP, giving clients real internet-routable IPv6 despite
# neither tunnel protocol carrying IPv6 at all. A DNS-only fix left a
# bigger hole open than it closed: Happy-Eyeballs-style dual A/AAAA
# lookups meant the IPv4 DNS query got caught by the DNAT above while
# the IPv6 leg sailed straight out via 6to4 to a real, un-tunneled
# resolver (confirmed live: an extra DNS server appeared in a leak test,
# geolocating to the 6to4 relay's location, not the tunnel's exit) --
# and if DNS can leak that way, so can actual IPv6-reachable payload
# traffic, not just lookups. Since neither tunnel here ever carries
# IPv6, there's no legitimate case where a scoped client should have
# any IPv6 path at all, so this drops it entirely rather than
# narrowly targeting port 53.
#
# Validated by hand over telnet on RAX120v2 against wg0 + a real guest
# client before this was ever scripted.
#
# Mechanism: bridge-netfilter + physdev match to mark packets at the
# point they enter br0 from a specific wifi VAP, then fwmark-based
# policy routing sends only marked traffic to the tunnel via a separate
# routing table.
#
# Usage:
#   wg_selective_route.sh start <tunnel_iface> <dns_server> <if1> [if2] [if3] ...
#   wg_selective_route.sh stop  <tunnel_iface> <dns_server> <if1> [if2] [if3] ...
#   wg_selective_route.sh status <tunnel_iface>
#
# <dns_server> is a plain IP (e.g. 1.1.1.1) -- callers are responsible
# for resolving NVK_SCOPE_DNS/its default before calling this (see
# resolve_scope_dns() in vpn_common.env), and for persisting whatever
# value they used so `stop` can be called with the SAME value later
# (see wg_ctrl.sh/ovpn_ctrl.sh's applied_scope_dns file) -- iptables -D
# requires an exact match on --to-destination, so calling stop with a
# different value than was used at start will silently fail to remove
# the old rule.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [ -f "${SCRIPT_DIR}/vpn_common.env" ]; then
	. "${SCRIPT_DIR}/vpn_common.env"
else
	# standalone fallback (e.g. run from somewhere vpn_common.env wasn't copied) --
	# same values, just not the single source of truth in that case
	vpn_fwmark="0x10"
	vpn_rt_table="100"
fi

usage()
{
	printf "%s\n" \
		"Usage: ${0##*/} start|stop tun_iface dns_server if1 [if2] [if3] ..." \
		"       ${0##*/} status tun_iface" \
		>&2
	exit 1
}

require_physdev()
{
	if [ "$(cat /proc/sys/net/bridge/bridge-nf-call-iptables 2>/dev/null)" != "1" ]; then
		echo "bridge-nf-call-iptables is not 1 -- physdev match will not see bridged traffic. Aborting." >&2
		exit 1
	fi
	iptables -m physdev -h >/dev/null 2>&1
	if [ "$?" -ne 0 ]; then
		echo "physdev match module unavailable. Aborting." >&2
		exit 1
	fi
}

# ip6tables is a soft dependency -- if this firmware genuinely has no
# IPv6 netfilter support at all, there's nothing to leak through it in
# the first place, so this degrades to a no-op with a log line rather
# than aborting the whole connect over it.
have_ip6tables()
{
	command -v ip6tables >/dev/null 2>&1
}

do_start()
{
	tun_dev="$1"; shift
	dns_server="$1"; shift
	[ $# -ge 1 ] || usage
	[ -n "$dns_server" ] || usage

	require_physdev
	IP6="0"
	have_ip6tables && IP6="1"

	for ifc in "$@"; do
		iptables -t mangle -I PREROUTING -m physdev --physdev-in "$ifc" -j MARK --set-mark "$vpn_fwmark"

		# position 1 specifically: this router already has its own DNS
		# hijack/REDIRECT rules earlier in this same chain (parental
		# control-style transparent DNS interception) -- inserting ahead
		# of those, not just appending, is what makes this actually take
		# effect rather than losing to a rule that already claimed the
		# packet first.
		iptables -t nat -I PREROUTING 1 -m physdev --physdev-in "$ifc" -p udp --dport 53 -j DNAT --to-destination "$dns_server"
		iptables -t nat -I PREROUTING 1 -m physdev --physdev-in "$ifc" -p tcp --dport 53 -j DNAT --to-destination "$dns_server"

		if [ "$IP6" = "1" ]; then
			ip6tables -t mangle -I PREROUTING 1 -m physdev --physdev-in "$ifc" -j DROP
		fi
	done

	ip rule show | grep -q "fwmark $vpn_fwmark lookup $vpn_rt_table" \
		|| ip rule add fwmark "$vpn_fwmark" lookup "$vpn_rt_table"

	ip route replace default dev "$tun_dev" table "$vpn_rt_table"

	iptables -C FORWARD -o "$tun_dev" -j ACCEPT 2>/dev/null \
		|| iptables -I FORWARD -o "$tun_dev" -j ACCEPT
	iptables -C FORWARD -i "$tun_dev" -j ACCEPT 2>/dev/null \
		|| iptables -I FORWARD -i "$tun_dev" -j ACCEPT

	iptables -t nat -C POSTROUTING -m mark --mark "$vpn_fwmark" -o "$tun_dev" -j MASQUERADE 2>/dev/null \
		|| iptables -t nat -I POSTROUTING -m mark --mark "$vpn_fwmark" -o "$tun_dev" -j MASQUERADE

	if [ "$IP6" = "1" ]; then
		echo "Routing ${*} -> ${tun_dev} via table ${vpn_rt_table} (mark ${vpn_fwmark}), DNS -> ${dns_server}, IPv6 blocked"
	else
		echo "Routing ${*} -> ${tun_dev} via table ${vpn_rt_table} (mark ${vpn_fwmark}), DNS -> ${dns_server}, IPv6 unavailable (ip6tables not found -- nothing to block)"
	fi
}

do_stop()
{
	tun_dev="$1"; shift
	dns_server="$1"; shift
	[ $# -ge 1 ] || usage

	IP6="0"
	have_ip6tables && IP6="1"

	for ifc in "$@"; do
		iptables -t mangle -D PREROUTING -m physdev --physdev-in "$ifc" -j MARK --set-mark "$vpn_fwmark" 2>/dev/null

		if [ -n "$dns_server" ]; then
			iptables -t nat -D PREROUTING -m physdev --physdev-in "$ifc" -p udp --dport 53 -j DNAT --to-destination "$dns_server" 2>/dev/null
			iptables -t nat -D PREROUTING -m physdev --physdev-in "$ifc" -p tcp --dport 53 -j DNAT --to-destination "$dns_server" 2>/dev/null
		fi

		if [ "$IP6" = "1" ]; then
			ip6tables -t mangle -D PREROUTING -m physdev --physdev-in "$ifc" -j DROP 2>/dev/null
		fi
	done

	ip rule del fwmark "$vpn_fwmark" lookup "$vpn_rt_table" 2>/dev/null
	ip route flush table "$vpn_rt_table" 2>/dev/null

	iptables -D FORWARD -o "$tun_dev" -j ACCEPT 2>/dev/null
	iptables -D FORWARD -i "$tun_dev" -j ACCEPT 2>/dev/null
	iptables -t nat -D POSTROUTING -m mark --mark "$vpn_fwmark" -o "$tun_dev" -j MASQUERADE 2>/dev/null

	echo "Stopped selective routing for ${*} -> ${tun_dev}"
}

do_status()
{
	tun_dev="$1"
	echo "--- mangle PREROUTING (mark ${vpn_fwmark}) ---"
	iptables -t mangle -L PREROUTING -v -n | grep -E "Chain|mark|physdev"
	echo "--- nat PREROUTING (DNS DNAT, port 53) ---"
	iptables -t nat -L PREROUTING -v -n | grep -E "Chain|dpt:53|physdev"
	echo "--- ip6tables mangle PREROUTING (IPv6 block) ---"
	if have_ip6tables; then
		ip6tables -t mangle -L PREROUTING -v -n | grep -E "Chain|DROP|physdev"
	else
		echo "(ip6tables not available on this firmware)"
	fi
	echo "--- ip rule ---"
	ip rule show | grep "$vpn_fwmark"
	echo "--- table ${vpn_rt_table} ---"
	ip route show table "$vpn_rt_table"
	echo "--- forward/nat for ${tun_dev} ---"
	iptables -L FORWARD -v -n | grep "$tun_dev"
	iptables -t nat -L POSTROUTING -v -n | grep "$tun_dev"
}

case "$1" in
	start)
		shift
		[ $# -ge 3 ] || usage
		do_start "$@"
		;;
	stop)
		shift
		[ $# -ge 3 ] || usage
		do_stop "$@"
		;;
	status)
		shift
		[ $# -ge 1 ] || usage
		do_status "$@"
		;;
	*)
		usage
		;;
esac
