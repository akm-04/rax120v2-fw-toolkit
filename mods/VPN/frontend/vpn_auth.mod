#!/bin/sh
echo "Content-type: text/html"
echo ""

read POST_DATA
decode() { printf '%b' "$(echo "$1" | sed 's/+/ /g; s/%\([0-9A-Fa-f][0-9A-Fa-f]\)/\\x\1/g')"; }

USER=$(echo "$POST_DATA" | grep -o 'user=[^&]*' | cut -d= -f2-)
PASS=$(echo "$POST_DATA" | grep -o 'pass=[^&]*' | cut -d= -f2-)

USER_DEC=$(decode "$USER")
PASS_DEC=$(decode "$PASS")

printf '%s\n%s\n' "$USER_DEC" "$PASS_DEC" > /tmp/vpn_auth.txt
chmod 600 /tmp/vpn_auth.txt

/bin/config set vpn_client_custom_user="$USER_DEC"
/bin/config set vpn_client_custom_pass="$PASS_DEC"
/bin/config commit

echo "OK: Credentials saved to NVRAM"
