#!/bin/sh
#
# ovpn_ctrl.sh — connect / disconnect / status / reconnect for the custom
# OpenVPN client tunnel. Same interface shape as wg_ctrl.sh so vpn_ctrl.sh
# (the dispatcher) can treat both protocols identically.
#
# Deliberately does NOT rely on the uploaded .ovpn's own `redirect-gateway`
# to make the tunnel "global" -- that would silently rewrite the main
# routing table, which is exactly what we avoided for WireGuard's global
# mode (a dead tunnel could otherwise strand the router's own WAN
# reachability). `--pull-filter ignore "redirect-gateway"` strips it, and
# the same mark+table mechanism wg_ctrl.sh uses is applied here instead --
# so "Global" means the identical thing regardless of which protocol the
# dropdown has selected.
#
# Usage:
#   ovpn_ctrl.sh connect
#   ovpn_ctrl.sh disconnect
#   ovpn_ctrl.sh status
#   ovpn_ctrl.sh reconnect

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "${SCRIPT_DIR}/ovpn_client_custom.env"
# also needed so disconnect can clean up the OTHER protocol's state too --
# see do_disconnect below
. "${SCRIPT_DIR}/wireguard_client.env"
CONFIG=/bin/config

write_state_file()
{
	# @1 state @2 tunnel_ip @3 error
	mkdir -p "$ovpn_client_data_dir"
	cat <<-EOF > "$ovpn_client_stat_file"
		{
			"state" : "$1",
			"tunnel_ip" : "$2",
			"error" : "$3"
		}
	EOF
}

log()
{
	mkdir -p "$ovpn_client_data_dir" 2>/dev/null
	echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$ovpn_client_log_file"
	logger -- "[OpenVPN-custom] $1" 2>/dev/null
}

# --- routing + DNS (called by OpenVPN's --up hook) ------------------------
#
# Both routing and DNS are applied from inside OpenVPN's own --up hook now,
# not from a separate poller. Reasoning:
#
# 1. foreign_option_* (where a pushed "dhcp-option DNS" lives -- see
#    resolve_scope_dns()'s comment in vpn_common.env for why OpenVPN
#    providers like NordVPN/ProtonVPN NEVER put this in the static .ovpn
#    text) is ONLY ever populated as an env var for the duration of this
#    hook. There is no way to recover it afterward, so anything driven by
#    a poller watching for the tun device to appear structurally cannot
#    see it -- that was the previous design's actual bug, independent of
#    the resolv.conf question below.
# 2. This hook fires exactly once, exactly when negotiation (DNS push
#    included) has completed and the interface is up -- i.e. exactly the
#    condition the old poller was trying to approximate from outside.
#
# Neither protocol EVER touches /tmp/resolv.conf or restarts dnsmasq for
# DNS anymore (see vpn_common.env's NVK_SCOPE_DNS comment for the two live
# failures -- an IPv6-only leak, and a total outage when a provider pushed
# nothing -- that the old resolv.conf-swap design caused). This block is
# the OpenVPN-side twin of wg_ctrl.sh's do_connect routing block: identical
# DNAT+mark/route mechanism, identical wg_selective_route.sh call for
# scoped mode, differing only in where PROVIDER_DNS comes from (parsed
# from foreign_option_* here vs. the uploaded config's static DNS field
# for WireGuard).
#
# script_type ("up"/"down") arrives as $2 since $1 is consumed by this
# script's own dispatch case at the bottom; OpenVPN's own positional args
# (tun_dev, tun_mtu, link_mtu, ifconfig_local, ifconfig_remote, [init|
# restart]) follow after that -- unused here, same as before.

apply_routing()
{
	# only the first pushed "dhcp-option DNS" is used, same one-value
	# policy resolve_scope_dns() applies to WireGuard's comma-separated
	# DNS field
	PROVIDER_DNS=""
	i=1
	while [ "$i" -le 100 ]; do
		eval "OPT=\${foreign_option_${i}:-}"
		[ -n "$OPT" ] || break
		case "$OPT" in
			"dhcp-option DNS"*)
				[ -z "$PROVIDER_DNS" ] && PROVIDER_DNS=$(echo "$OPT" | sed -e 's/^dhcp-option DNS //')
				;;
		esac
		i=$((i + 1))
	done

	SCOPE_IFACES=$($CONFIG get ${NVK_OVPN_SCOPE_IFACES})
	SCOPE_DNS=$(resolve_scope_dns "$PROVIDER_DNS")
	echo "$SCOPE_DNS" > "${ovpn_client_data_dir}/applied_scope_dns"

	if [ -n "$SCOPE_IFACES" ]; then
		echo 1 > /proc/sys/net/bridge/bridge-nf-call-iptables
		"${SCRIPT_DIR}/wg_selective_route.sh" start "$ovpn_iface" "$SCOPE_DNS" $SCOPE_IFACES >> "$ovpn_client_log_file" 2>&1
	else
		# whole-router: same DNAT+IPv6-block mechanism as scoped mode,
		# matched on -i br0 (LAN ingress) rather than a specific physdev --
		# see wg_ctrl.sh's do_connect for the identical block and its
		# reasoning
		iptables -t nat -C PREROUTING -i br0 -p udp --dport 53 -j DNAT --to-destination "$SCOPE_DNS" 2>/dev/null \
			|| iptables -t nat -I PREROUTING 1 -i br0 -p udp --dport 53 -j DNAT --to-destination "$SCOPE_DNS"
		iptables -t nat -C PREROUTING -i br0 -p tcp --dport 53 -j DNAT --to-destination "$SCOPE_DNS" 2>/dev/null \
			|| iptables -t nat -I PREROUTING 1 -i br0 -p tcp --dport 53 -j DNAT --to-destination "$SCOPE_DNS"
		if command -v ip6tables >/dev/null 2>&1; then
			ip6tables -t mangle -C PREROUTING -i br0 -j DROP 2>/dev/null \
				|| ip6tables -t mangle -I PREROUTING 1 -i br0 -j DROP
		fi

		iptables -t mangle -C PREROUTING -i br0 -j MARK --set-mark "$vpn_fwmark" 2>/dev/null \
			|| iptables -t mangle -I PREROUTING -i br0 -j MARK --set-mark "$vpn_fwmark"
		ip rule show | grep -q "fwmark $vpn_fwmark lookup $vpn_rt_table" \
			|| ip rule add fwmark "$vpn_fwmark" lookup "$vpn_rt_table"
		ip route replace default dev "$ovpn_iface" table "$vpn_rt_table"
		iptables -C FORWARD -o "$ovpn_iface" -j ACCEPT 2>/dev/null \
			|| iptables -I FORWARD -o "$ovpn_iface" -j ACCEPT
		iptables -C FORWARD -i "$ovpn_iface" -j ACCEPT 2>/dev/null \
			|| iptables -I FORWARD -i "$ovpn_iface" -j ACCEPT
		iptables -t nat -C POSTROUTING -m mark --mark "$vpn_fwmark" -o "$ovpn_iface" -j MASQUERADE 2>/dev/null \
			|| iptables -t nat -I POSTROUTING -m mark --mark "$vpn_fwmark" -o "$ovpn_iface" -j MASQUERADE
	fi

	# for vpnctl doctor's display only -- NOT read back by do_disconnect,
	# which unconditionally flushes the full VPN_KNOWN_SCOPE_IFACES set instead
	# (see vpn_common.env) rather than trusting a possibly-stale tracked
	# value
	echo "$SCOPE_IFACES" > "${ovpn_client_data_dir}/applied_scope"
	log "routing applied via --up hook, scope=${SCOPE_IFACES:-whole-router}, dns=${SCOPE_DNS} (provider pushed: ${PROVIDER_DNS:-none})"
}

# --down hook: deliberately does nothing now. remove_routing() (below,
# called unconditionally from do_disconnect) already tears down every
# rule this connect could have installed, and it runs regardless of
# whether OpenVPN exits cleanly enough to invoke --down at all (a forced
# kill -9 after a hung shutdown won't). Keeping cleanup in exactly one
# place avoids the old design's split-brain, where resolv.conf restoration
# lived here but routing teardown lived in do_disconnect.
dns_down()
{
	log "down hook fired (no-op -- see comment above dns_down)"
}

remove_routing()
{
	# resolve which DNS value(s) the DNAT rules were actually installed
	# with -- see wg_ctrl.sh's do_disconnect for the full reasoning
	APPLIED_DNS=""
	[ -f "${ovpn_client_data_dir}/applied_scope_dns" ] && APPLIED_DNS=$(cat "${ovpn_client_data_dir}/applied_scope_dns")
	CURRENT_DNS=$($CONFIG get ${NVK_SCOPE_DNS})
	CURRENT_DNS="${CURRENT_DNS:-1.1.1.1}"

	# unconditional full flush, same reasoning as wg_ctrl.sh's do_disconnect
	"${SCRIPT_DIR}/wg_selective_route.sh" stop "$ovpn_iface" "${APPLIED_DNS:-$CURRENT_DNS}" $VPN_KNOWN_SCOPE_IFACES >> "$ovpn_client_log_file" 2>&1
	if [ -n "$APPLIED_DNS" ] && [ "$APPLIED_DNS" != "$CURRENT_DNS" ]; then
		"${SCRIPT_DIR}/wg_selective_route.sh" stop "$ovpn_iface" "$CURRENT_DNS" $VPN_KNOWN_SCOPE_IFACES >> "$ovpn_client_log_file" 2>&1
	fi
	rm -f "${ovpn_client_data_dir}/applied_scope_dns"

	iptables -t mangle -D PREROUTING -i br0 -j MARK --set-mark "$vpn_fwmark" 2>/dev/null
	ip rule del fwmark "$vpn_fwmark" lookup "$vpn_rt_table" 2>/dev/null
	ip route flush table "$vpn_rt_table" 2>/dev/null
	iptables -D FORWARD -o "$ovpn_iface" -j ACCEPT 2>/dev/null
	iptables -D FORWARD -i "$ovpn_iface" -j ACCEPT 2>/dev/null
	iptables -t nat -D POSTROUTING -m mark --mark "$vpn_fwmark" -o "$ovpn_iface" -j MASQUERADE 2>/dev/null

	# whole-router DNS DNAT + IPv6 block -- same both-values approach as
	# the wg_selective_route.sh calls above, since this can equally have
	# been installed with a since-changed NVRAM value. Safe no-op if
	# apply_routing() never got as far as installing these (e.g. connect
	# failed before the --up hook fired).
	for D in "$APPLIED_DNS" "$CURRENT_DNS"; do
		[ -n "$D" ] || continue
		iptables -t nat -D PREROUTING -i br0 -p udp --dport 53 -j DNAT --to-destination "$D" 2>/dev/null
		iptables -t nat -D PREROUTING -i br0 -p tcp --dport 53 -j DNAT --to-destination "$D" 2>/dev/null
	done
	if command -v ip6tables >/dev/null 2>&1; then
		ip6tables -t mangle -D PREROUTING -i br0 -j DROP 2>/dev/null
	fi

	rm -f "${ovpn_client_data_dir}/applied_scope"
}

do_connect()
{
	mkdir -p "$ovpn_client_data_dir"

	if [ "$($CONFIG get ${NVK_OVPN_CONFIGURED})" != "1" ]; then
		write_state_file "disconnected" "" "No OpenVPN config saved yet. Upload one first."
		echo '{"status":"disconnected","error":"No OpenVPN config saved yet. Upload one first."}'
		log "connect refused: no config saved yet"
		exit 0
	fi

	log "connect: starting"
	write_state_file "connecting" "" ""

	do_disconnect internal >/dev/null 2>&1

	umask 077
	$CONFIG get ${NVK_OVPN_BLOB} | base64 -d > "$ovpn_client_conf_file" 2>/dev/null
	if [ ! -s "$ovpn_client_conf_file" ]; then
		write_state_file "disconnected" "" "stored config could not be decoded"
		echo '{"status":"disconnected","error":"stored config could not be decoded"}'
		log "base64 decode of stored config failed or produced empty file"
		exit 1
	fi

	# regenerate the auth file from NVRAM every connect -- /tmp is wiped
	# on every reboot, but credentials saved via vpn_auth.mod are meant
	# to persist across it. Previously this only checked whether the
	# file happened to already exist, which meant boot-time connects
	# silently ran with no credentials at all (NVRAM had them, nothing
	# ever wrote them back to /tmp after a reboot) until the frontend
	# was used again in the same session.
	OVPN_USER=$($CONFIG get ${NVK_OVPN_USER})
	OVPN_PASS=$($CONFIG get ${NVK_OVPN_PASS})
	if [ -n "$OVPN_USER" ]; then
		umask 077
		printf '%s\n%s\n' "$OVPN_USER" "$OVPN_PASS" > /tmp/vpn_auth.txt
		chmod 600 /tmp/vpn_auth.txt
	fi

	AUTH_ARGS=""
	[ -s /tmp/vpn_auth.txt ] && AUTH_ARGS="--auth-user-pass /tmp/vpn_auth.txt"

	openvpn --config "$ovpn_client_conf_file" \
		--dev "$ovpn_iface" \
		--dev-type tun \
		--verb 4 \
		--log "$ovpn_client_log_file" \
		--writepid "$ovpn_client_pid_file" \
		--pull-filter ignore "redirect-gateway" \
		--script-security 2 \
		--up "${SCRIPT_DIR}/ovpn_ctrl.sh up" \
		--down "${SCRIPT_DIR}/ovpn_ctrl.sh down" \
		$AUTH_ARGS \
		--daemon

	# openvpn --daemon backgrounds immediately; give it a moment to either
	# write the pidfile or fail outright (bad config, auth failure, etc.)
	# before reporting back
	sleep 2

	if [ ! -s "$ovpn_client_pid_file" ] || ! [ -e "/proc/$(cat "$ovpn_client_pid_file" 2>/dev/null)" ]; then
		write_state_file "disconnected" "" "openvpn failed to start, see log"
		echo '{"status":"disconnected","error":"openvpn failed to start"}'
		log "openvpn process did not start, check ${ovpn_client_log_file}"
		exit 1
	fi

	SCOPE_IFACES=$($CONFIG get ${NVK_OVPN_SCOPE_IFACES})
	log "connect requested, scope=${SCOPE_IFACES:-whole-router}"

	# No poller here anymore -- routing+DNS are applied by OpenVPN's own
	# --up hook (apply_routing, see above) the moment negotiation actually
	# completes, which is both earlier and strictly more capable than
	# polling for the tun device to appear ever was (the poller had no way
	# to see foreign_option_* at all). If negotiation fails or times out,
	# --up simply never fires and no routing is applied -- do_status's log
	# parsing already surfaces that as a stuck "connecting" state.

	echo '{"status":"connecting","error":""}'
}

do_disconnect()
{
	remove_routing

	# also flush the OTHER protocol's tunnel-device rules and interface --
	# same mutual-exclusion reasoning as wg_ctrl.sh's do_disconnect
	iptables -D FORWARD -o "$wg_iface" -j ACCEPT 2>/dev/null
	iptables -D FORWARD -i "$wg_iface" -j ACCEPT 2>/dev/null
	iptables -t nat -D POSTROUTING -m mark --mark "$vpn_fwmark" -o "$wg_iface" -j MASQUERADE 2>/dev/null
	ip link del "$wg_iface" 2>/dev/null
	rm -f "${wg_client_data_dir}/applied_scope"
	if [ -f "$wg_watchdog_pid_file" ]; then
		kill -TERM "$(cat "$wg_watchdog_pid_file")" 2>/dev/null
		rm -f "$wg_watchdog_pid_file"
	fi

	if [ -s "$ovpn_client_pid_file" ]; then
		kill -s TERM "$(cat "$ovpn_client_pid_file")" 2>/dev/null
		sleep 1
		kill -0 "$(cat "$ovpn_client_pid_file")" 2>/dev/null && kill -9 "$(cat "$ovpn_client_pid_file")" 2>/dev/null
	fi
	rm -f "$ovpn_client_pid_file"

	# Called with "internal" when used as a pre-clean step from within
	# do_connect (mutual-exclusion teardown before establishing a new
	# tunnel) -- in that case the caller owns the state file/log from
	# here on and this shouldn't stomp "connecting" back to
	# "disconnected" a moment after setting it. A real, user-initiated
	# disconnect (no argument) still reports normally.
	if [ "${1:-}" != "internal" ]; then
		write_state_file "disconnected" "" ""
		log "disconnected"
		echo '{"status":"disconnected","error":""}'
	fi
}

do_status()
{
	if [ ! -s "$ovpn_client_pid_file" ] || ! [ -e "/proc/$(cat "$ovpn_client_pid_file" 2>/dev/null)" ]; then
		write_state_file "disconnected" "" ""
		echo '{"status":"disconnected","tunnel_ip":"","error":""}'
		return
	fi

	TUN_IP=$(ip -4 addr show dev "$ovpn_iface" 2>/dev/null | awk '/inet /{print $2}' | cut -d/ -f1)

	# OpenVPN doesn't expose a structured query for this the way
	# `wg show dump` does -- scraping the most recent negotiation line
	# from the log is the only source available without adding a
	# --status file (a bigger change, not done here). No byte counters
	# for the same reason -- that data only exists in a --status file
	# this setup doesn't currently generate.
	LAST_PEER_LINE=$(grep "Peer Connection Initiated" "$ovpn_client_log_file" 2>/dev/null | tail -1)
	ENDPOINT=$(echo "$LAST_PEER_LINE" | sed -n 's/.*\[AF_INET\]\([^ ]*\).*/\1/p')
	SERVER_NAME=$(echo "$LAST_PEER_LINE" | grep -oE '\[[^]]+\] Peer Connection Initiated' | sed -e 's/^\[//' -e 's/\] Peer Connection Initiated//')

	if [ -n "$TUN_IP" ] && grep -q "Initialization Sequence Completed" "$ovpn_client_log_file" 2>/dev/null; then
		write_state_file "connected" "$TUN_IP" ""
		printf '{"status":"connected","tunnel_ip":"%s","endpoint":"%s","server_name":"%s","error":""}\n' "$TUN_IP" "$ENDPOINT" "$SERVER_NAME"
	else
		write_state_file "connecting" "$TUN_IP" ""
		printf '{"status":"connecting","tunnel_ip":"%s","endpoint":"%s","server_name":"%s","error":""}\n' "$TUN_IP" "$ENDPOINT" "$SERVER_NAME"
	fi
}

case "$1" in
	connect)    do_connect ;;
	disconnect) do_disconnect ;;
	status)     do_status ;;
	reconnect)  do_disconnect >/dev/null 2>&1; sleep 1; do_connect ;;
	up)         apply_routing ;;
	down)       dns_down ;;
	*)
		echo "Usage: ${0##*/} connect|disconnect|status|reconnect" >&2
		exit 1
		;;
esac
