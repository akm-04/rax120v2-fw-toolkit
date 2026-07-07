#! /bin/sh

# [DNIRAX60-2372] Projects without procd, call bd script periodically.
# Run "/opt/bitdefender/bin/bd start" once every 30 minutes
# to check armor related daemon crash or not.

debug_file=/tmp/dal_ash_status.log

for retry_num in `seq 1 15`
do
	if [ "$(pidof dal_ash)" != "" ]; then
		echo "dal_ash is up" > /dev/console
		echo "[`date '+%Y/%m/%d %T'`]dal_ash up now" >> $debug_file
	else
		echo "dal_ash is not up, delay 2s" > /dev/console
		echo "[`date '+%Y/%m/%d %T'`]dal_ash is not up now" >> $debug_file
		sleep 2
	fi
done      

while :; do
	sleep 30
	if [ -f /tmp/check_bdagent ];then
		if [ "$(pidof dal_ash)" = "" ]; then
			#logger to dni armor debug log
			echo "[`date '+%Y/%m/%d %T'`]dal_ash not running" >> $debug_file

			if [ "$(pidof bdbrokerd)" != "" ]; then
				echo "[`date '+%Y/%m/%d %T'`]start dal_ash in check_dal_ash_status.sh" >> $debug_file
				/usr/bin/dal_ash 2>/dev/null &
			fi
		fi
	fi
done
