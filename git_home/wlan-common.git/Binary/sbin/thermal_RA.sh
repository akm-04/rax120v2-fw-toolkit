#This deamon can pick up the temperature of each chip
#Per NTGR's request, it will be a raw data to analyze in the future.

#Introduce the variable be used in this script:
#Name			   : extract from wifi.conf, it name the sensor reading temp from where.
#highestTemp		   : to determine the highest temp so far.
#lowestTemp		   : to determine the lowest temp so far.
#otpEnable		   : extract from wifi.conf, it means that
#                            the project supports changing level
#                            when temp achieves the threshold temp or not.
#otpTriggerStatus	   : to record current trigger status. it equal cur_level,
#                            0 means not trigger, 1, 2, 3 means triggered in QCA solution.
#otpTriggerCount	   : to calcualate trigger time,
#                            it will +1 when otpTriggerStatus from 0 to another status.
#otpTriggerLongestDuration : to calculate how long the duration of otpTriggerStatus is triggered.
#cur_temp		   : the current temp from sensor.
#cur_level		   : the current level from sensor.
#heaterEnable		   : extract from wifi.conf, it means that
#                            the project supports heater function
#heaterTriggerStatus	   : to record current heater trigger status. it equal cur_heater_status,
#                            0 means not trigger, 1 means triggered.
#heaterTriggerCount	   : to calcualate trigger time,
#                            it will +1 when heaterTriggerStatus from 1 to 0 status.
#heaterTriggerLongestDuration : to calculate how long the duration of heaterTriggerStatus is triggered.

. /etc/ath/wifi.conf
[ -d "/tmp/cache/wireless" ] || mkdir /tmp/cache/wireless

thermal_qcawifi(){
    for device in ${DEVICES}; do
        check_cac_status $device
        [ "x$debug_mode" = "x1" ] && echo "==== doing ${device} ==="
        thermal_file=/tmp/cache/wireless/thermal_${device}.conf
        local highestTemp=
        local lowestTemp=
        local cur_temp=

        [ -f ${thermal_file} ] && . ${thermal_file}

        read_thermaltool_data_qca $device
        [ "x$debug_mode" = "x1" ] && echo cur_temp=${cur_temp}

        highestTemp=${highestTemp:-$cur_temp}
        lowestTemp=${lowestTemp:-$cur_temp}

        [ -n "$key_row_qca" ] && update_extremum_temp

        eval echo Name="\$VendorChipName_$device" > ${thermal_file}
        echo highestTemp=$highestTemp >> ${thermal_file}
        echo lowestTemp=$lowestTemp >> ${thermal_file}
        [ "x$debug_mode" = "x1" ] && cat ${thermal_file}
    done
}

check_cac_status(){
    cac=0
    ifname_list=`cat /tmp/wifi_topology|awk 'BEGIN {FS=":"} {print $4}'`
    for ifname in $ifname_list;do
        cac_state=`iwpriv $ifname get_cac_state|awk 'BEGIN {FS=":"} {print $2}'`
        cac=$(($cac+$cac_state))
        if [ -n "$(eval echo \$cac_laster_$1)" ];then #e.g. cac_laster_wifi0.
            [ $(eval echo \$cac_laster_$1) -ne 0 -a $cac -eq 0 ] && sleep 2
        fi
        eval cac_laster_$1=$cac
    done
    if [ $cac -ne 0 ];then
        [ "x$debug_mode" = "x1" ] && echo "==== $1 doing  cac ==="
        continue
    fi
}

thermal_onboard_sensor(){
    [ "x$debug_mode" = "x1" ] && echo "==== doing onboard sensor ==="
    thermal_file=/tmp/cache/wireless/thermal_board.conf
    local highestTemp=
    local lowestTemp=
    local cur_temp=

    [ -f ${thermal_file} ] && . ${thermal_file}

    read_thermaltool_data_onboard
    [ "x$debug_mode" = "x1" ] && echo cur_temp=${cur_temp}

    highestTemp=${highestTemp:-$cur_temp}
    lowestTemp=${lowestTemp:-$cur_temp}

    [ -n "$key_row_gpio" ] && update_extremum_temp

    echo Name=$Onboard_sensor_name > ${thermal_file}
    echo highestTemp=$highestTemp >> ${thermal_file}
    echo lowestTemp=$lowestTemp >> ${thermal_file}
    [ "x$debug_mode" = "x1" ] && cat ${thermal_file}
}

read_thermaltool_data_qca(){
    key_row_qca=`thermaltool -i $1 -get|grep sensor`
    cur_temp=`echo $key_row_qca|awk '{print $3}'|sed 's/,//g'`
}

read_thermaltool_data_onboard(){
    key_row_gpio=`cat /proc/pct2075/temp`
    cur_temp=${key_row_gpio%%.*}
}

update_extremum_temp(){
    #update highest and lowest temp
    [ "$cur_temp" -gt "$highestTemp" ] && highestTemp=$cur_temp
    [ "$cur_temp" -lt "$lowestTemp" ] && lowestTemp=$cur_temp
}

update_thermalComponentAnalytics_onboard(){
    #update otpTriggerLongestDuration
    [ "${otpTriggerStatus}" == "1" ] && otpTriggerLongestDuration=$((otpTriggerLongestDuration+$sleep_time))
    #update otpTriggerCount
    [ "${otpTriggerStatus}" == "0" -a "${cur_level}" == "1" ] && otpTriggerCount=$((otpTriggerCount+1))
    #update otpTriggerStatus
    otpTriggerStatus=$cur_level
}

update_thermalComponentAnalytics(){
    #update otpTriggerLongestDuration
    [ "${otpTriggerStatus}" == "1" -o "${otpTriggerStatus}" == "2" -o "${otpTriggerStatus}" == "3" ] && otpTriggerLongestDuration=$((otpTriggerLongestDuration+$sleep_time))
    update_extremum_temp
    #update otpTriggerCount
    [ "${otpTriggerStatus}" == "0" ] && [ "${cur_level}" == "1" -o "${cur_level}" == "2" -o "${cur_level}" == "3" ] && otpTriggerCount=$((otpTriggerCount+1))
    #update otpTriggerStatus
    otpTriggerStatus=$cur_level
}

thermal_heater(){
    [ "x$debug_mode" = "x1" ] && echo "==== doing heater ===="
    thermal_heater_file=/tmp/thermal_heater.conf
    local heaterTriggerStatus=
    local heaterTriggerCount=${heaterTriggerCount:-0}
    local heaterTriggerLongestDuration=${heaterTriggerDuration:-0}
    local cur_heater_status=
    [ -f ${thermal_heater_file} ] && . ${thermal_heater_file}

    read_heater_status
    [ "x$debug_mode" = "x1" ] && echo cur_heater_status=${cur_heater_status}

    update_thermal_heater_ComponentAnalytics

    echo Name="heater1" > ${thermal_heater_file}
    echo heaterEnable=$heaterEnable >> ${thermal_heater_file}
    echo heaterTriggerStatus=$heaterTriggerStatus >> ${thermal_heater_file}
    echo heaterTriggerCount=$heaterTriggerCount >> ${thermal_heater_file}
    echo heaterTriggerLongestDuration=$heaterTriggerLongestDuration >> ${thermal_heater_file}
    [ "x$debug_mode" = "x1" ] && cat ${thermal_heater_file}
}

read_heater_status(){
    echo 44 > /proc/simple_config/gpio
    cur_heater_status=`cat /proc/simple_config/gpio|awk 'BEGIN  {FS="="} {print $2}'`
}

update_thermal_heater_ComponentAnalytics(){
    #update heaterTriggerLongestDuration
    [ $heaterTriggerStatus -ne 0 ] && heaterTriggerLongestDuration=$((heaterTriggerLongestDuration+$sleep_time))
    #update heaterTriggerCount
    [ $heaterTriggerStatus -eq 1 -a $cur_heater_status -eq 0 ] && heaterTriggerCount=$((heaterTriggerCount+1))
    #update heaterTriggerStatus
    heaterTriggerStatus=$cur_heater_status
}

sleep_time=60
driver=$1
DEVICES=`uci show|grep "type"|grep qcawifi|awk 'BEGIN {FS="."} {print $2}'`
debug_mode=


#since thermaltool still get 0-degree after finishing "wlan up" and doesn't doing CAC , moving regular sleep syntax to be previous row in the while loop.
while [ 1 ];
do
    wlan_daemon=`ps|grep 'wlan up'|grep -v 'grep'`
    if [ -z "$wlan_daemon" ];then
        sleep $sleep_time
        thermal_${driver}
        [ "$heaterEnable" = "True" ] && thermal_heater
        [ -n "$Onboard_sensor_name" ] && thermal_onboard_sensor
    fi
done
