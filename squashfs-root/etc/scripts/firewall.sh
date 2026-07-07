#! /bin/sh

###############################################################################
### THIS SCRIPT IS A QUICK ENTRY TO MANAGE NET-WALL RULEs
###
###     Consider some times we need to add|delete|change firewall rules on
###     urgent, and it's not easy to modify net-wall source code directly,
###     so add this quick entry to manage firewall rules.
###
###     Each time when `net-wall start|restart` command executes, this script
###     will be called with parameter "start", and of course, `net-wall stop`
###     will call this script with parameter "stop".
###
### NOTE: THIS SCRIPT IS *JUST* A QUICK ENTRY, PLEASE MANAGE FIREWALL RULES
### IN NET-WALL SOURCE CODE AS FAR AS POSSIBLE. AND PLEASE MOVE SOME CHANGES
### IN THIS FILE INTO NET-WALL SOURCE CODE IN THE FUTURE TO KEEP THIS FILE
### IS CONCISE TO REDUCE AFFECTS OF NET-WALL'S PERFORMANCE.
###############################################################################

IPTB=/usr/sbin/iptables
CONFIG=${CONFIG:-/bin/config}

LIBDIR=/etc/scripts/firewall

NETWALL_RESULT=/tmp/netwall-result

RETRY_FOLDER=/tmp/cache/netwall
RETRYING=/tmp/cache/netwall/retrying
RETRYCNT=/tmp/cache/netwall/retrycnt
RETRYCMD=/tmp/cache/netwall/retrycmd

get_configs()
{
	:
}

firewall_start()
{
	# start extra firewall rules
	ls ${LIBDIR}/*.rule | while read rule
	do
		$SHELL $rule start
	done
}

firewall_stop()
{
	# stop extra firewall rules
	ls ${LIBDIR}/*.rule | while read rule
	do
		$SHELL $rule stop
	done
}

firewall6_start()
{
        # start extra firewall rules
        ls ${LIBDIR}/*.rule6 | while read rule
        do
                $SHELL $rule start
        done
}

firewall6_stop()
{
        # stop extra firewall rules
        ls ${LIBDIR}/*.rule6 | while read rule
        do
                $SHELL $rule stop
        done
}

firewall_status_check()
{
        local result=$(cat $NETWALL_RESULT)

        if [ "$result" = "fail" ]; then
               [ -d "$RETRY_FOLDER" ] || mkdir -p $RETRY_FOLDER
                echo "$1" > $RETRYCMD
                [ ! -f "$RETRYING" ] && /etc/scripts/firewall-retry.sh &
                return 1
        else
                [ -f "$RETRYCMD" ] && rm -rf $RETRYCMD
                [ -f "$RETRYCNT" ] && rm -rf $RETRYCNT
                return 0
        fi
}

firewall_v6_status_check()
{
        local result=$(cat ${NETWALL_RESULT}-v6)

        if [ "$result" = "fail" ]; then
               [ -d "$RETRY_FOLDER" ] || mkdir -p $RETRY_FOLDER
                echo "$1" > ${RETRYCMD}-v6
                [ ! -f "${RETRYING}-v6" ] && /etc/scripts/firewall-retry.sh "v6" &
                return 1
        else
                [ -f "${RETRYCMD}-v6" ] && rm -rf ${RETRYCMD}-v6
                [ -f "${RETRYCNT}-v6" ] && rm -rf ${RETRYCNT}-v6
                return 0
        fi
}

get_configs
case $1 in
	"start"|"START")
               if firewall_status_check "start" ; then #success
                       firewall_start
               fi
               ;;
	"stop"|"STOP")
               if firewall_status_check "stop" ; then
                       firewall_stop
               fi
               ;;
	"v6-start"|"V6-START")
                if firewall_v6_status_check "start" ; then
                        firewall6_start
                fi
                ;;
        "v6-stop"|"V6-STOP")
                if firewall_v6_status_check "stop" ; then
                        firewall6_stop
                fi
                ;;
        "check")
                firewall_status_check $2
                ;;
        "v6-check")
                firewall_v6_status_check $2
                ;;
	*)
		printf "Usage: ${0##*/} start|stop\n";;
esac
