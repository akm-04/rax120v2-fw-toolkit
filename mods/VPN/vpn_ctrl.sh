#!/bin/sh
#
# vpn_ctrl.sh — single dispatch point for connect/disconnect/status/reconnect.
# vpn_ctrl.mod (the CGI layer) calls this rather than knowing which
# protocol is active.
#
# Reads vpn_client_active_protocol from NVRAM ("wireguard"|"openvpn");
# unset or unrecognized defaults to wireguard, matching the frontend
# dropdown's stated default.
#
# Usage: vpn_ctrl.sh connect|disconnect|status|reconnect

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
. "${SCRIPT_DIR}/vpn_common.env"
CONFIG=/bin/config

usage()
{
	echo "Usage: ${0##*/} connect|disconnect|status|reconnect" >&2
	exit 1
}

[ -n "$1" ] || usage

case "$1" in
	connect|disconnect|status|reconnect) : ;;
	*) usage ;;
esac

PROTO=$($CONFIG get ${NVK_ACTIVE_PROTOCOL})
case "$PROTO" in
	openvpn)
		exec "${SCRIPT_DIR}/ovpn_ctrl.sh" "$1"
		;;
	wireguard|*)
		exec "${SCRIPT_DIR}/wg_ctrl.sh" "$1"
		;;
esac
