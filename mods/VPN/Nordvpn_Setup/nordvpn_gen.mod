#!/bin/sh
#
# nordvpn_gen.mod — CGI endpoint for the "Setup NordVPN" page. Pure
# dispatch, same as vpn_ctrl.mod -- all real logic lives in
# nordvpn_wg_gen.sh, callable standalone for telnet testing.
#
# This wrapper captures stdout/stderr separately from the backend script
# rather than falling back to a generic message on failure, since this is
# an interactive wizard that genuinely benefits from surfacing the real
# reason a step failed.
#
# IMPORTANT: token_status is the ONLY token-related read action. The token
# itself, or its encrypted NVRAM form, is NEVER returned to the frontend by
# any action here -- the setup page must not be able to display a
# previously-saved key under any code path.
#
# GET  ?action=token_status
# GET  ?action=list_countries
# GET  ?action=ping_country&country_id=...
# POST action=set_token&token=...
# POST action=clear_token
# POST action=generate&country_id=...

echo "Content-type: application/json"
echo ""

BACKEND="/etc/vpn_client/nordvpn_wg_gen.sh"

json_escape()
{
	# minimal escaping: backslash, double-quote, newline -> literal space
	printf '%s' "$1" | sed 's/\\/\\\\/g; s/"/\\"/g' | tr '\n' ' '
}

run_backend()
{
	OUT_FILE="/tmp/nordvpn_cgi_out.$$"
	ERR_FILE="/tmp/nordvpn_cgi_err.$$"
	"$@" >"$OUT_FILE" 2>"$ERR_FILE"
	RC=$?
	OUT=$(cat "$OUT_FILE")
	ERR=$(cat "$ERR_FILE")
	rm -f "$OUT_FILE" "$ERR_FILE"
	return $RC
}

decode() { printf '%b' "$(echo "$1" | sed 's/+/ /g; s/%\([0-9A-Fa-f][0-9A-Fa-f]\)/\\x\1/g')"; }

case "$REQUEST_METHOD" in
	POST) read -r POST_DATA ;;
	*)    POST_DATA="" ;;
esac

if [ "$REQUEST_METHOD" = "POST" ]; then
	ACTION=$(echo "$POST_DATA" | grep -o 'action=[^&]*' | cut -d= -f2-)
else
	ACTION=$(echo "$QUERY_STRING" | grep -o 'action=[^&]*' | cut -d= -f2-)
fi

emit_error()
{
	printf '{"status":"error","error":"%s"}\n' "$(json_escape "${ERR#ERROR: }")"
}

case "$ACTION" in
	token_status)
		if run_backend "$BACKEND" token_status; then
			printf '{"status":"ok","token_status":"%s"}\n' "$(json_escape "$OUT")"
		else
			emit_error
		fi
		;;

	set_token)
		TOKEN_RAW=$(echo "$POST_DATA" | grep -o 'token=[^&]*' | cut -d= -f2-)
		TOKEN=$(decode "$TOKEN_RAW")
		if run_backend "$BACKEND" set_token "$TOKEN"; then
			echo '{"status":"ok"}'
		else
			emit_error
		fi
		;;

	clear_token)
		if run_backend "$BACKEND" clear_token; then
			echo '{"status":"ok"}'
		else
			emit_error
		fi
		;;

	list_countries)
		if run_backend "$BACKEND" list_countries; then
			printf '{"status":"ok","countries":['
			FIRST=1
			echo "$OUT" | while IFS='|' read -r CID CNAME; do
				[ -n "$CID" ] || continue
				[ "$FIRST" -eq 1 ] || printf ','
				printf '{"id":%s,"name":"%s"}' "$CID" "$(json_escape "$CNAME")"
				FIRST=0
			done
			printf ']}\n'
		else
			emit_error
		fi
		;;

	ping_country)
		CID_RAW=$(echo "$QUERY_STRING" | grep -o 'country_id=[^&]*' | cut -d= -f2-)
		COUNTRY_ID=$(decode "$CID_RAW")
		if run_backend "$BACKEND" ping_country "$COUNTRY_ID"; then
			printf '{"status":"ok","ping_ms":%s}\n' "$OUT"
		else
			emit_error
		fi
		;;

	generate)
		CID_RAW=$(echo "$POST_DATA" | grep -o 'country_id=[^&]*' | cut -d= -f2-)
		COUNTRY_ID=$(decode "$CID_RAW")
		if run_backend "$BACKEND" generate "$COUNTRY_ID"; then
			printf '{"status":"ok","message":"%s"}\n' "$(json_escape "$OUT")"
		else
			emit_error
		fi
		;;

	*)
		echo '{"status":"error","error":"invalid or missing action"}'
		;;
esac
