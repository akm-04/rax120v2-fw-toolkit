#!/bin/sh

#HOWTO:
#
#Purpose:
#	After DUT gets ntp time, some other modules(such as trafficmeter) need take some action.
#	We write this script to gather all the action other modules need to take.
#
#Usage:
#	Add new function:
#		func_xxxxxx ()
#		{
#			echo ======your_code_xxxxxx======= >/dev/console
#		}
#	Invoke func_xxxxxx ()
#		For example, in ntpclient.c
#		system("ntp_updated xxxxxx");
#
#Using Occasion:
#	func_common - right after getting ntp time
#	func_daylight - when daylight saving time enabled and right after getting ntp time
#	

CONFIG=/bin/config
wlan_schedule="wlan schedule"

OPTIONS=`for opt in $(grep '^func_.*()' $0 | cut -d_ -f2- | cut -d' ' -f1); do echo $opt; done`;

func_common ()
{
	#Record first NTP Sync Timestamp, if the Timestamp have existed on pot partition, ntpst will do nothing.
	ntpst set -T $($CONFIG get time_zone) -d $(part_path pot)
	
	# When time updates, and selects "Per Schedule" for "Block Sites" && "Block Services", generate the crond's schedule file again.
	if [ "x$($CONFIG get block_skeyword)" = "x1" ] || [ "x$($CONFIG get blockserv_ctrl)" = "x1" ]; then
		/sbin/cmdsched
		firewall.sh start
	fi

	# Fix Bug 23259, when time updates,it must check whether WIFI should be turned off according WIFI Schedule
	if [ "x$($CONFIG get wladv_schedule_enable)" = "x1" ]; then
		/sbin/cmdsched_wlan_status 11g
		[ "x$($CONFIG get wlg_onoff_sched)" = "x1" ] && \
			$wlan_schedule 11g off || \
			$wlan_schedule 11g on
	fi

	if [ "x$($CONFIG get wladv_schedule_enable_a)" = "x1" ]; then
		/sbin/cmdsched_wlan_status 11a
		[ "x$($CONFIG get wla_onoff_sched)" = "x1" ] && \
			$wlan_schedule 11a off || \
			$wlan_schedule 11a on
	fi

	/sbin/wlan check_guest_schedule 11g
	/sbin/wlan check_guest_schedule 11a
}

func_daylight ()
{
	/sbin/cmd_traffic_meter stop
	/sbin/cmd_traffic_meter start
}

for opt in $OPTIONS; do
	if [ -n "$1" ] && [ "$1" = "$opt" ]; then
		shift
		eval $@
		func_$opt

		exit 0;
	fi
done

echo "Error: Invalid option -$1- of ntp_updated."
