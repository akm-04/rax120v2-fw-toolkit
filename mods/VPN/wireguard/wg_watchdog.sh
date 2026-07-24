#!/bin/sh
#
# wg_watchdog.sh — background monitor for the WireGuard client tunnel.
#
# No netifd/proto_* (not present on this firmware), no UCI config_load
# (this firmware uses /bin/config). WireGuard has no up/down script hooks
# the way OpenVPN does, so this loop is the only place tunnel death gets
# detected at all.
#
# Two-tier response to staleness:
#   - past wg_stale_threshold:       just reflect "stale" in the status
#                                     file. WireGuard's own kernel-side
#                                     retry + PersistentKeepalive often
#                                     recovers on its own without a full
#                                     re-init.
#   - past wg_hard_reconnect_after:  give up waiting, force a full
#                                     teardown + reconnect via wg_ctrl.sh.
#
# Started by wg_ctrl.sh as part of a successful WireGuard connect, killed
# by either protocol's disconnect.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "${SCRIPT_DIR}/wireguard_client.env"

log()
{
	mkdir -p "$wg_client_data_dir" 2>/dev/null
	echo "[$(date '+%Y-%m-%d %H:%M:%S')] [watchdog] $1" >> "$wg_client_log_file"
	logger -- "[WireGuard][watchdog] $1" 2>/dev/null
}

echo $$ > "$wg_watchdog_pid_file"
trap 'rm -f "$wg_watchdog_pid_file"; exit 0' TERM INT

stale_since=0   # 0 = not currently stale; otherwise epoch time staleness began

while true; do
	sleep "$wg_watchdog_interval"

	if ! ip link show "$wg_iface" >/dev/null 2>&1; then
		log "${wg_iface} missing, attempting connect"
		"${SCRIPT_DIR}/wg_ctrl.sh" connect >/dev/null 2>&1
		stale_since=0
		continue
	fi

	HS=$(wg show "$wg_iface" latest-handshakes 2>/dev/null | awk '{print $2}')
	now=$(date +%s)

	if [ -z "$HS" ] || [ "$HS" -eq 0 ] 2>/dev/null; then
		stale_since=0
		continue
	fi

	age=$((now - HS))

	if [ "$age" -lt "$wg_stale_threshold" ]; then
		stale_since=0
		continue
	fi

	if [ "$stale_since" -eq 0 ]; then
		stale_since="$now"
		log "handshake stale (${age}s), watching before forcing reconnect"
		continue
	fi

	stale_for=$((now - stale_since))
	if [ "$stale_for" -ge "$wg_hard_reconnect_after" ]; then
		log "stale for ${stale_for}s, forcing reconnect"
		"${SCRIPT_DIR}/wg_ctrl.sh" reconnect >/dev/null 2>&1
		stale_since=0
	fi
done
