#!/bin/sh
#
# wg_ctrl.sh — connect / disconnect / status / reconnect for the WireGuard
# client tunnel. Called by vpn_ctrl.sh (the protocol dispatcher), never
# directly by the CGI layer -- but fully usable standalone for telnet
# testing, same as every other backend script here.
#
# NVRAM is the single source of truth: the runtime conf at
# $wg_client_conf_file is regenerated from NVRAM on every connect and
# never read back, so it can never drift out of sync.
#
# Usage:
#   wg_ctrl.sh connect
#   wg_ctrl.sh disconnect
#   wg_ctrl.sh status
#   wg_ctrl.sh reconnect     # disconnect + connect, used by the watchdog

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "${SCRIPT_DIR}/wireguard_client.env"
# also needed so disconnect can clean up the OTHER protocol's tunnel-device
# rules too -- see do_disconnect below
. "${SCRIPT_DIR}/ovpn_client_custom.env"
CONFIG=/bin/config

write_state_file()
{
	# @1 state @2 tunnel_ip @3 error
	mkdir -p "$wg_client_data_dir"
	cat <<-EOF > "$wg_client_stat_file"
		{
			"state" : "$1",
			"tunnel_ip" : "$2",
			"error" : "$3"
		}
	EOF
}

log()
{
	mkdir -p "$wg_client_data_dir" 2>/dev/null
	echo "[$(date '+%Y-%m-%d %H:%M:%S')] $1" >> "$wg_client_log_file"
	logger -- "[WireGuard] $1" 2>/dev/null
}

# 'ip link add wg0 type wireguard' fails outright if the kernel module
# isn't loaded yet -- this isn't guaranteed on every call path (e.g. a
# watchdog-forced reconnect after some future firmware update unloads
# modules on suspend/resume, or this script being invoked standalone
# before the boot script has run), so check/load every time rather than
# assuming the init script already handled it. Idempotent: a no-op if
# already loaded.
require_wg_module()
{
	lsmod | grep -q '^wireguard ' && return 0

	insmod "/lib/modules/$(uname -r)/wireguard.ko" 2>>"$wg_client_log_file"
	if ! lsmod | grep -q '^wireguard '; then
		log "failed to insmod wireguard.ko for kernel $(uname -r)"
		return 1
	fi
	return 0
}

do_connect()
{
	mkdir -p "$wg_client_data_dir"

	if ! require_wg_module; then
		write_state_file "disconnected" "" "wireguard.ko failed to load"
		echo '{"status":"disconnected","error":"wireguard.ko failed to load"}'
		exit 1
	fi

	if [ "$($CONFIG get ${NVK_CONFIGURED})" != "1" ]; then
		write_state_file "disconnected" "" "No WireGuard config saved yet. Upload one first."
		echo '{"status":"disconnected","error":"No WireGuard config saved yet. Upload one first."}'
		log "connect refused: no config saved yet"
		exit 0
	fi

	log "connect: starting"

	PRIVATEKEY=$($CONFIG get ${NVK_PRIVATEKEY})
	ADDRESS=$($CONFIG get ${NVK_ADDRESS})
	DNS=$($CONFIG get ${NVK_DNS})
	PEER_PUBKEY=$($CONFIG get ${NVK_PEER_PUBKEY})
	ENDPOINT=$($CONFIG get ${NVK_ENDPOINT})
	ALLOWEDIPS=$($CONFIG get ${NVK_ALLOWEDIPS})
	KEEPALIVE=$($CONFIG get ${NVK_KEEPALIVE})
	SCOPE_IFACES=$($CONFIG get ${NVK_SCOPE_IFACES})

	# tear down any previous instance cleanly before re-init
	do_disconnect >/dev/null 2>&1

	write_state_file "connecting" "" ""

	umask 077
	cat > "$wg_client_conf_file" <<-EOF
		[Interface]
		PrivateKey = ${PRIVATEKEY}

		[Peer]
		PublicKey = ${PEER_PUBKEY}
		Endpoint = ${ENDPOINT}
		AllowedIPs = ${ALLOWEDIPS}
		PersistentKeepalive = ${KEEPALIVE}
	EOF

	ip link show "$wg_iface" >/dev/null 2>&1 || ip link add "$wg_iface" type wireguard
	if ! wg setconf "$wg_iface" "$wg_client_conf_file" 2>>"$wg_client_log_file"; then
		write_state_file "disconnected" "" "wg setconf failed, see log"
		echo '{"status":"disconnected","error":"wg setconf failed"}'
		log "setconf failed for ${wg_iface}"
		exit 1
	fi

	[ -n "$ADDRESS" ] && ip address add "$ADDRESS" dev "$wg_iface" 2>/dev/null
	ip link set up dev "$wg_iface"

	# Provider DNS is known statically here (parsed from the uploaded peer
	# config into $DNS at upload time by wg_update_config.sh) -- unlike
	# OpenVPN, no runtime hook is needed to learn it.
	PROVIDER_DNS="$DNS"
	SCOPE_DNS=$(resolve_scope_dns "$PROVIDER_DNS")
	echo "$SCOPE_DNS" > "${wg_client_data_dir}/applied_scope_dns"

	if [ -n "$SCOPE_IFACES" ]; then
		# guest-only (or arbitrary VAP subset): needs bridge-netfilter since
		# those interfaces share br0 with everything else.
		echo 1 > /proc/sys/net/bridge/bridge-nf-call-iptables
		"${SCRIPT_DIR}/wg_selective_route.sh" start "$wg_iface" "$SCOPE_DNS" $SCOPE_IFACES >> "$wg_client_log_file" 2>&1
	else
		# whole-router: same DNAT+IPv6-block mechanism as scoped mode,
		# just matched on -i br0 instead of a specific physdev -- this is
		# the exact same match already used by the mark rule below, so it
		# dynamically covers whatever's actually bridged into LAN right
		# now (all four VAPs AND the wired eth0-4 ports) with zero
		# hardcoded interface list, and structurally can never touch
		# brwan (a separate bridge) regardless of which physical port is
		# configured as WAN or whether WAN aggregation is active.
		#
		# NOT a resolv.conf swap + dnsmasq restart (what this used to be):
		# that caused a live IPv6-only DNS leak here (Happy Eyeballs sent
		# the AAAA lookup out via this router's own auto-established
		# 6to4 tunnel, un-caught since the swap only ever addressed IPv4)
		# and, separately, a total DNS outage on the OpenVPN side when a
		# provider didn't push any DNS option at all. One mechanism,
		# proven by repeated live leak testing, instead of two different
		# ones with different failure modes.
		iptables -t nat -C PREROUTING -i br0 -p udp --dport 53 -j DNAT --to-destination "$SCOPE_DNS" 2>/dev/null \
			|| iptables -t nat -I PREROUTING 1 -i br0 -p udp --dport 53 -j DNAT --to-destination "$SCOPE_DNS"
		iptables -t nat -C PREROUTING -i br0 -p tcp --dport 53 -j DNAT --to-destination "$SCOPE_DNS" 2>/dev/null \
			|| iptables -t nat -I PREROUTING 1 -i br0 -p tcp --dport 53 -j DNAT --to-destination "$SCOPE_DNS"
		if command -v ip6tables >/dev/null 2>&1; then
			ip6tables -t mangle -C PREROUTING -i br0 -j DROP 2>/dev/null \
				|| ip6tables -t mangle -I PREROUTING 1 -i br0 -j DROP
		fi

		# mark everything ingressing br0, route only the mark via table so
		# the main table's default route (WAN) is never touched
		iptables -t mangle -C PREROUTING -i br0 -j MARK --set-mark "$vpn_fwmark" 2>/dev/null \
			|| iptables -t mangle -I PREROUTING -i br0 -j MARK --set-mark "$vpn_fwmark"
		ip rule show | grep -q "fwmark $vpn_fwmark lookup $vpn_rt_table" \
			|| ip rule add fwmark "$vpn_fwmark" lookup "$vpn_rt_table"
		ip route replace default dev "$wg_iface" table "$vpn_rt_table"
		iptables -C FORWARD -o "$wg_iface" -j ACCEPT 2>/dev/null \
			|| iptables -I FORWARD -o "$wg_iface" -j ACCEPT
		iptables -C FORWARD -i "$wg_iface" -j ACCEPT 2>/dev/null \
			|| iptables -I FORWARD -i "$wg_iface" -j ACCEPT
		iptables -t nat -C POSTROUTING -m mark --mark "$vpn_fwmark" -o "$wg_iface" -j MASQUERADE 2>/dev/null \
			|| iptables -t nat -I POSTROUTING -m mark --mark "$vpn_fwmark" -o "$wg_iface" -j MASQUERADE
	fi

	# for vpnctl doctor's display only -- NOT read back by do_disconnect,
	# which unconditionally flushes the full VPN_KNOWN_SCOPE_IFACES set instead
	# (see vpn_common.env) rather than trusting a possibly-stale tracked
	# value
	echo "$SCOPE_IFACES" > "${wg_client_data_dir}/applied_scope"

	log "connect requested, endpoint=${ENDPOINT} scope=${SCOPE_IFACES:-whole-router}"

	# watchdog lifecycle now lives here, not in the init script -- WireGuard
	# is kernel-resident and stateless (no keepalive/reconnect logic of its
	# own beyond blindly sending handshakes), so it genuinely needs external
	# supervision; every WireGuard connect ensures the watchdog is running
	if [ ! -f "$wg_watchdog_pid_file" ] || ! [ -e "/proc/$(cat "$wg_watchdog_pid_file" 2>/dev/null)" ]; then
		"${SCRIPT_DIR}/wg_watchdog.sh" >/dev/null 2>&1 &
	fi

	echo '{"status":"connecting","error":""}'
}

do_disconnect()
{
	# watchdog has no purpose without an active WireGuard connection
	if [ -f "$wg_watchdog_pid_file" ]; then
		kill -TERM "$(cat "$wg_watchdog_pid_file")" 2>/dev/null
		rm -f "$wg_watchdog_pid_file"
	fi

	# resolve which DNS value(s) the DNAT rules were actually installed
	# with -- iptables -D needs an exact --to-destination match, so if
	# NVRAM's scope-DNS setting changed since connect, the current value
	# alone wouldn't match the old rule. Try the persisted (actually-used)
	# value first, and also the current NVRAM value if it differs -- both
	# are free no-ops via wg_selective_route.sh's own 2>/dev/null if they
	# don't match anything.
	APPLIED_DNS=""
	[ -f "${wg_client_data_dir}/applied_scope_dns" ] && APPLIED_DNS=$(cat "${wg_client_data_dir}/applied_scope_dns")
	CURRENT_DNS=$($CONFIG get ${NVK_SCOPE_DNS})
	CURRENT_DNS="${CURRENT_DNS:-1.1.1.1}"

	# unconditionally flush every possible physdev mark rule, not just
	# whatever the tracked scope claims was applied -- cheap (this mark/
	# table is entirely ours), and closes the gap where a garbage/stale
	# scope value or a partial earlier failure would otherwise leave
	# orphaned rules behind indefinitely
	"${SCRIPT_DIR}/wg_selective_route.sh" stop "$wg_iface" "${APPLIED_DNS:-$CURRENT_DNS}" $VPN_KNOWN_SCOPE_IFACES >> "$wg_client_log_file" 2>&1
	if [ -n "$APPLIED_DNS" ] && [ "$APPLIED_DNS" != "$CURRENT_DNS" ]; then
		"${SCRIPT_DIR}/wg_selective_route.sh" stop "$wg_iface" "$CURRENT_DNS" $VPN_KNOWN_SCOPE_IFACES >> "$wg_client_log_file" 2>&1
	fi
	rm -f "${wg_client_data_dir}/applied_scope_dns"

	# also flush the OTHER protocol's tunnel-device rules and process --
	# wireguard and openvpn are mutually exclusive by design, so a wg
	# disconnect should guarantee a clean slate for BOTH, not just its own,
	# in case a prior openvpn session was left in a partial state
	iptables -D FORWARD -o "$ovpn_iface" -j ACCEPT 2>/dev/null
	iptables -D FORWARD -i "$ovpn_iface" -j ACCEPT 2>/dev/null
	iptables -t nat -D POSTROUTING -m mark --mark "$vpn_fwmark" -o "$ovpn_iface" -j MASQUERADE 2>/dev/null
	if [ -s "$ovpn_client_pid_file" ]; then
		kill -s TERM "$(cat "$ovpn_client_pid_file")" 2>/dev/null
		sleep 1
		kill -0 "$(cat "$ovpn_client_pid_file")" 2>/dev/null && kill -9 "$(cat "$ovpn_client_pid_file")" 2>/dev/null
		rm -f "$ovpn_client_pid_file"
	fi
	rm -f "${ovpn_client_data_dir}/applied_scope"
	# safety net: openvpn normally removes its own tun device on a clean
	# SIGTERM exit, but won't if it had to be force-killed -- explicit
	# delete here guarantees no leftover interface either way
	ip link del "$ovpn_iface" 2>/dev/null

	# and the global path, same as before -- also unconditional, also a
	# safe no-op if it was never installed
	iptables -t mangle -D PREROUTING -i br0 -j MARK --set-mark "$vpn_fwmark" 2>/dev/null
	ip rule del fwmark "$vpn_fwmark" lookup "$vpn_rt_table" 2>/dev/null
	ip route flush table "$vpn_rt_table" 2>/dev/null
	iptables -D FORWARD -o "$wg_iface" -j ACCEPT 2>/dev/null
	iptables -D FORWARD -i "$wg_iface" -j ACCEPT 2>/dev/null
	iptables -t nat -D POSTROUTING -m mark --mark "$vpn_fwmark" -o "$wg_iface" -j MASQUERADE 2>/dev/null

	# global-mode DNS DNAT + IPv6 block -- same both-values approach as
	# the physdev cleanup above, since this can equally have been
	# installed with a since-changed NVRAM value
	for D in "$APPLIED_DNS" "$CURRENT_DNS"; do
		[ -n "$D" ] || continue
		iptables -t nat -D PREROUTING -i br0 -p udp --dport 53 -j DNAT --to-destination "$D" 2>/dev/null
		iptables -t nat -D PREROUTING -i br0 -p tcp --dport 53 -j DNAT --to-destination "$D" 2>/dev/null
	done
	if command -v ip6tables >/dev/null 2>&1; then
		ip6tables -t mangle -D PREROUTING -i br0 -j DROP 2>/dev/null
	fi

	rm -f "${wg_client_data_dir}/applied_scope"

	# deleting the interface tears down the tunnel outright -- no separate
	# process to kill, unlike OpenVPN
	ip link del "$wg_iface" 2>/dev/null

	write_state_file "disconnected" "" ""
	log "disconnected"
	echo '{"status":"disconnected","error":""}'
}

do_status()
{
	if ! ip link show "$wg_iface" >/dev/null 2>&1; then
		write_state_file "disconnected" "" ""
		echo '{"status":"disconnected","tunnel_ip":"","error":""}'
		return
	fi

	TUN_IP=$(ip -4 addr show dev "$wg_iface" 2>/dev/null | awk '/inet /{print $2}' | cut -d/ -f1)

	# 'wg show dump' gives interface + first peer in two tab-separated
	# lines: iface = privkey pubkey listen-port fwmark; peer = pubkey
	# psk endpoint allowed-ips latest-handshake rx-bytes tx-bytes keepalive
	DUMP=$(wg show "$wg_iface" dump 2>/dev/null)
	IFACE_LINE=$(echo "$DUMP" | sed -n '1p')
	PEER_LINE=$(echo "$DUMP" | sed -n '2p')
	LISTEN_PORT=$(echo "$IFACE_LINE" | awk -F'\t' '{print $3}')
	ENDPOINT=$(echo "$PEER_LINE" | awk -F'\t' '{print $3}')
	HS=$(echo "$PEER_LINE" | awk -F'\t' '{print $5}')
	RX=$(echo "$PEER_LINE" | awk -F'\t' '{print $6}')
	TX=$(echo "$PEER_LINE" | awk -F'\t' '{print $7}')
	[ -n "$RX" ] || RX=0
	[ -n "$TX" ] || TX=0
	now=$(date +%s)

	if [ -n "$HS" ] && [ "$HS" -gt 0 ] 2>/dev/null; then
		age=$((now - HS))
		if [ "$age" -lt "$wg_stale_threshold" ]; then
			write_state_file "connected" "$TUN_IP" ""
			printf '{"status":"connected","tunnel_ip":"%s","handshake_age":%d,"endpoint":"%s","listen_port":"%s","rx_bytes":%s,"tx_bytes":%s,"error":""}\n' \
				"$TUN_IP" "$age" "$ENDPOINT" "$LISTEN_PORT" "$RX" "$TX"
		else
			write_state_file "stale" "$TUN_IP" "No handshake in ${age}s"
			printf '{"status":"stale","tunnel_ip":"%s","handshake_age":%d,"endpoint":"%s","listen_port":"%s","rx_bytes":%s,"tx_bytes":%s,"error":""}\n' \
				"$TUN_IP" "$age" "$ENDPOINT" "$LISTEN_PORT" "$RX" "$TX"
		fi
	else
		write_state_file "connecting" "$TUN_IP" ""
		printf '{"status":"connecting","tunnel_ip":"%s","endpoint":"%s","listen_port":"%s","error":""}\n' "$TUN_IP" "$ENDPOINT" "$LISTEN_PORT"
	fi
}

case "$1" in
	connect)    do_connect ;;
	disconnect) do_disconnect ;;
	status)     do_status ;;
	reconnect)  do_disconnect >/dev/null 2>&1; sleep 1; do_connect ;;
	*)
		echo "Usage: ${0##*/} connect|disconnect|status|reconnect" >&2
		exit 1
		;;
esac
