#!/bin/sh

# This file is expected to be called by net-cgi when customer set guest
# network schedule on Netgear UP APP.
#
# For set 2.4G guest schedule: /sbin/guest_sched.sh set wlg
# The command need to be run is in /tmp/guest_sched
#
# To get 2.4G guest schedule remaining time: /sbin/guest_sched.sh get wlg
# Remaining time would be stored at /tmp/guest_schedule_time

set_sched() {
	band=$2
	if [ "$band" = "wla_2nd" ]; then
		GUEST_RADIO="11h"
	elif [ "$band" = "wla" ]; then
		GUEST_RADIO="11a"
	elif [ "$band" = "wlg" ]; then
		GUEST_RADIO="11g"
	fi
	/sbin/wlan guest_schedule $GUEST_RADIO
}

get_time() {
	atq | while read line
	do
		if [ "$2" = "wla_2nd" ]; then
			GUEST_RADIO="11h"
			GUEST_NAME="wla1_2nd"
		elif [ "$2" = "wla" ]; then
			GUEST_RADIO="11a"
			GUEST_NAME="wla1"
		elif [ "$2" = "wlg" ]; then
			GUEST_RADIO="11g"
			GUEST_NAME="wlg1"
		fi

		sched_num=`echo $line | awk -F ' ' '{print $1}'`
		sched_cmd=`at -c $sched_num | grep "/sbin/wlan turn_off_guest ${GUEST_RADIO}"`
		[ "${sched_cmd}" = "" ] || {
			target_time=`/bin/config get ${GUEST_NAME}_guest_target_time`
			now=`date +%s`
			distance=$((target_time-now))
			echo $distance > /tmp/guest_schedule_time
		}
	done
}

case "$1" in
	get) get_time $@;;
	set) set_sched $@;;
esac
