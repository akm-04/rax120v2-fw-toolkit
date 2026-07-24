#!/bin/sh
echo "Content-type: application/json"
echo ""

ACTION=$(echo "$QUERY_STRING" | grep -o 'action=[^&]*' | cut -d= -f2-)

case "$ACTION" in
	connect|disconnect|status|reconnect)
		/etc/vpn_client/vpn_ctrl.sh "$ACTION"
		;;
	wan_iface)
		. /etc/vpn_client/vpn_common.env
		LIST=""
		for W in $(get_wan_ifaces); do
			[ -z "$LIST" ] && LIST="\"$W\"" || LIST="${LIST},\"$W\""
		done
		echo "{\"wan_ifaces\":[${LIST}]}"
		;;
	*)
		echo '{"status":"error","error":"invalid or missing action"}'
		;;
esac
