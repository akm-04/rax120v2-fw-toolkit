#!/bin/sh
echo "Content-type: text/plain"
echo ""

. /etc/vpn_client/vpn_common.env
. /etc/vpn_client/wireguard_client.env
. /etc/vpn_client/ovpn_client_custom.env

PROTO=$(/bin/config get ${NVK_ACTIVE_PROTOCOL})
case "$PROTO" in
	openvpn) LOGFILE="$ovpn_client_log_file" ;;
	wireguard|*) LOGFILE="$wg_client_log_file" ;;
esac

ACTION=$(echo "$QUERY_STRING" | grep -o 'action=[^&]*' | cut -d= -f2-)

case "$ACTION" in
	view)
		if [ -s "$LOGFILE" ]; then
			tail -n 200 "$LOGFILE"
		else
			echo "(no log output yet)"
		fi
		;;
	clear)
		: > "$LOGFILE" 2>/dev/null
		echo "(log cleared)"
		;;
	debug)
		DOCTOR_OUT=$(/etc/vpn_client/vpnctl doctor 2>&1)
		TS=$(date '+%Y-%m-%d %H:%M:%S')
		for LOG in "$wg_client_log_file" "$ovpn_client_log_file"; do
			{
				echo ""
				echo "=== vpnctl doctor @ ${TS} ==="
				echo "$DOCTOR_OUT"
				echo "=== end vpnctl doctor ==="
			} >> "$LOG" 2>/dev/null
		done
		echo "Diagnostic output appended to the log below."
		;;
	*)
		echo "ERROR: invalid action"
		;;
esac
