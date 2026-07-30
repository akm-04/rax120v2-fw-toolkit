#
# Copyright (c) 2017-2018 Qualcomm Technologies, Inc.
# All Rights Reserved.
# Confidential and Proprietary - Qualcomm Technologies, Inc.
#
#!/bin/sh
#
# Copyright (c) 2014, 2016, The Linux Foundation. All rights reserved.
#
append DRIVERS "qcawificfg80211"

[ -f /etc/ath/wifi.conf ] && . /etc/ath/wifi.conf

qdss_tracing=0
daemon=0

update_qdss_tracing_daemon_variables() {
	[ -f /sys/firmware/devicetree/base/soc_version_major ] && {
		soc_version_major="$(hexdump -n 1 -e '"%1d"' /sys/firmware/devicetree/base/soc_version_major)"
		if [ $soc_version_major = 1 ] && [ -f /ini/internal/QCA8074_i.ini ];then
			qdss_tracing="$(grep enable_qdss_tracing /ini/internal/QCA8074_i.ini  | awk -F '=' '{print $2}')"
			daemon="$(grep enable_daemon_support /ini/internal/QCA8074_i.ini  | awk -F '=' '{print $2}')"
			qdss_tracing=$(($qdss_tracing && $daemon))
		elif [ $soc_version_major = 2 ] && [ -f /ini/internal/QCA8074V2_i.ini ];then
			qdss_tracing="$(grep enable_qdss_tracing /ini/internal/QCA8074V2_i.ini  | awk -F '=' '{print $2}')"
			daemon="$(grep enable_daemon_support /ini/internal/QCA8074V2_i.ini  | awk -F '=' '{print $2}')"
			qdss_tracing=$(($qdss_tracing && $daemon))
		fi
	}
}

#
# Put qcawifi's country (ISO 3166-1 Alpha-2) to country code conversion
# (digits) table to /etc/ath/country_code_mapping_table.txt for all later code
# to access.
#
countrycode_mapping_qcawificfg80211() {
	cp /lib/wifi/qcawifi_countrycode.txt \
	   /etc/ath/country_code_mapping_table.txt
}

#
# Convert country code to country (ISO 3166-1 Alpha-2)
#
# Look up conversion table to convert country code (digits) to country (ISO
# 3166-1 Alpha-2).
#
# input:  $1 - country code (digits).
# output: $2 - country (ISO 3166-1 Alpha-2).
#              If no input country code (digits) is found, then return digits.
#
_countrycode2str_qcacfg80211() {
	local digit_country_code=$1

	local str_country_code=$(
		eval awk \'/ ${digit_country_code}\$/ { print \$1 }\' \
		         /etc/ath/country_code_mapping_table.txt)

	if [ -z "$str_country_code" ]; then
		eval "$2"="$digit_country_code"
	else
		eval "$2"="$str_country_code"
	fi
}

reload_time_qcawificfg80211() {
	local devices="$2"
	local wifi_reload_time=14
	local wifi_reload_vap_time=2
	local module_reload_time=16
	local module_reload=$(uci_get wireless qcawifi module_reload 1)

	test "$module_reload" = "1" && wifi_reload_time=$(( $wifi_reload_time + $module_reload_time ))
	for device in ${devices}; do
		config_get vifs "$device" vifs
		for vif in $vifs; do
			wifi_reload_time=$(( $wifi_reload_time + $wifi_reload_vap_time ))
		done
	done
	eval "export $1=$wifi_reload_time"
}

wlanconfig() {
	[ -n "${DEBUG}" ] && echo wlanconfig "$@"
	/usr/sbin/wlanconfig "$@"
}

iwconfig() {
	[ -n "${DEBUG}" ] && echo iwconfig "$@"
	/usr/sbin/iwconfig "$@"
}

iwpriv() {
	[ -n "${DEBUG}" ] && echo iwpriv "$@"
	/usr/sbin/iwpriv "$@"
}

start_recovery_daemon() {
	[ -n "${DEBUG}" ] && echo starting recovery daemon
	killall acfg_tool
	/usr/sbin/acfg_tool -e -s > /dev/console &
}

find_qcawifi_phy() {
	local device="$1"

	local macaddr="$(config_get "$device" macaddr | tr 'A-Z' 'a-z')"
	config_get phy "$device" phy
	[ -z "$phy" -a -n "$macaddr" ] && {
		cd /sys/class/net
		for phy in $(ls -d wifi* 2>&-); do
			[ "$macaddr" = "$(cat /sys/class/net/${phy}/address)" ] || continue
			config_set "$device" phy "$phy"
			break
		done
		config_get phy "$device" phy
	}
	[ -n "$phy" -a -d "/sys/class/net/$phy" ] || {
		echo "phy for wifi device $1 not found"
		return 1
	}
	[ -z "$macaddr" ] && {
		config_set "$device" macaddr "$(cat /sys/class/net/${phy}/address)"
	}
	return 0
}

enable_qdss_tracing() {
	local board_name

	[ -f /tmp/sysinfo/board_name ] && {
		board_name=$(cat /tmp/sysinfo/board_name)
	}

	case "$board_name" in
	ap-hk*)
		[ ! -f /tmp/qdss_trace_commands ] && {
			echo "q6mem" > /sys/bus/coresight/devices/coresight-tmc-etr/out_mode
			echo 1 > /sys/bus/coresight/devices/coresight-tmc-etr/curr_sink
			echo "0x06021FB0 0xc5acce55" > /sys/bus/coresight/devices/coresight-hwevent/setreg
			echo "0x06130FB0 0xc5acce55" > /sys/bus/coresight/devices/coresight-hwevent/setreg
			echo "0x06021000 0x00000320" > /sys/bus/coresight/devices/coresight-hwevent/setreg
			echo "0x06130000 0x00000340" > /sys/bus/coresight/devices/coresight-hwevent/setreg
			echo 1 > /sys/bus/coresight/devices/coresight-stm/enable
			echo "*****Registers configuration for qdss_tracing completed*******" > /dev/console
		}
		echo -e "qdss_trace_load_config\nqdss_trace_start\nquit\n" > /tmp/qdss_trace_commands
		echo "******Starting cnss_cli********" > /dev/console
		cnsscli < /tmp/qdss_trace_commands > /dev/null
		;;
	*)
		echo "INFO:QDSS_tracing not applicable for $board_name" > /dev/console
		;;
	esac
}

do_cold_boot_calibration() {
	local board_name
	update_qdss_tracing_daemon_variables
	[ -f /tmp/sysinfo/board_name ] && {
		board_name=$(cat /tmp/sysinfo/board_name)
	}

	case "$board_name" in
	ap-hk*)
		[ -f /tmp/cold_boot_done ] && {
			echo "******Not the first boot. Skip coldboot calibration*****"  > /dev/console && return 0
		}

		#soc_version_major is assigned a value globally
		if [ $soc_version_major = 1 ];then
			cold_boot_support="$(grep enable_cold_boot_support \
					/ini/internal/QCA8074_i.ini | awk -F '=' '{print $2}')"
		elif [ $soc_version_major = 2 ];then
			cold_boot_support="$(grep enable_cold_boot_support \
					/ini/internal/QCA8074V2_i.ini | awk -F '=' '{print $2}')"
		fi

		echo $cold_boot_support > /sys/module/cnss2/parameters/cold_boot_support
		#daemon is assigned a value globally
		echo $daemon > /sys/module/cnss2/parameters/daemon_support

		[ $cold_boot_support = 0 ] && {
			echo "******No cold_boot_support*****"  > /dev/console && return 0
		}

		mkdir -p /data/vendor/wifi/
		[ $daemon = 1 ] && {
			echo "*****starting cnssdaemon*****" > /dev/console && /usr/bin/cnssdaemon &
		}
		echo "*********initiating cold boot calibration*************" > /dev/console
		coldbootmode=7
		#Set Cold boot mode to 10 for FTM Mode, 7 otherwise
		is_ftm="$(grep wifi_ftm_mode /proc/cmdline | wc -l)"
		[ $is_ftm = 1   ] && {
			coldbootmode=10
			echo "*****FTM Coldboot mode set to 10*****" > /dev/console
		}
		insmod qca_ol testmode=$coldbootmode
		pid=`pgrep cnssdaemon` && echo -1000 > /proc/$pid/oom_score_adj && echo "*****cnssdaemon pid=$pid*********" > /dev/console
		# insmod and rmmod wifi_3_0 kernel object in addition to qca_ol
		# as wifi_3_0 initiates and shuts down driver.
		insmod wifi_3_0
		rmmod wifi_3_0
		rmmod qca_ol
		sync
		touch /tmp/cold_boot_done
		;;

	*)
		echo "INFO: Cold boot calibration not applicable: $board_name"  > /dev/console
		;;
	esac
}

scan_qcawificfg80211() {
	local device="$1"
	local wds
	local adhoc sta ap monitor lite_monitor ap_monitor ap_smart_monitor mesh ap_lp_iot disabled

	[ ${device%[0-9]} = "wifi" ] && config_set "$device" phy "$device"

	local ifidx=0
	local radioidx=${device#wifi}

	config_get vifs "$device" vifs
	for vif in $vifs; do
		config_get_bool disabled "$vif" disabled 0
		[ $disabled = 0 ] || continue

		local vifname
		local ifname

		[ $ifidx -gt 0 ] && vifname="ath${radioidx}$ifidx" || vifname="ath${radioidx}"

		config_set "$vif" ifname $vifname

		config_get mode "$vif" mode
		case "$mode" in
			adhoc|sta|ap|monitor|lite_monitor|wrap|ap_monitor|ap_smart_monitor|mesh|ap_lp_iot)
				append "$mode" "$vif"
			;;
			wds)
				config_get ssid "$vif" ssid
				[ -z "$ssid" ] && continue

				config_set "$vif" wds 1
				config_set "$vif" mode sta
				mode="sta"
				addr="$ssid"
				${addr:+append "$mode" "$vif"}
			;;
			*) echo "$device($vif): Invalid mode, ignored."; continue;;
		esac

		ifidx=$(($ifidx + 1))
	done

	case "${adhoc:+1}:${sta:+1}:${ap:+1}" in
		# valid mode combinations
		1::) wds="";;
		1::1);;
		:1:1)config_set "$device" nosbeacon 1;; # AP+STA, can't use beacon timers for STA
		:1:);;
		::1);;
		::);;
		*) echo "$device: Invalid mode combination in config"; return 1;;
	esac

	config_set "$device" vifs "${ap:+$ap }${ap_monitor:+$ap_monitor }${mesh:+$mesh }${ap_smart_monitor:+$ap_smart_monitor }${wrap:+$wrap }${sta:+$sta }${adhoc:+$adhoc }${wds:+$wds }${monitor:+$monitor}${lite_monitor:+$lite_monitor }${ap_lp_iot:+$ap_lp_iot}"
}

# The country ID is set at the radio level. When the driver attaches the radio,
# it sets the default country ID to 840 (US STA). This is because the desired
# VAP modes are not known at radio attach time, and STA functionality is the
# common unit of 802.11 operation.
# If the user desires any of the VAPs to be in AP mode, then we set a new
# default of 843 (US AP with TDWR) from this script. Even if any of the other
# VAPs are in non-AP modes like STA or Monitor, the stricter default of 843
# will apply.
# No action is required here if none of the VAPs are in AP mode.
set_default_country() {
	local device="$1"
	local mode

        config_get device_if "$device" device_if "cfg80211tool"
	find_qcawifi_phy "$device" || return 1
	config_get phy "$device" phy

	config_get vifs "$device" vifs
	for vif in $vifs; do
		config_get_bool disabled "$vif" disabled 0
		[ $disabled = 0 ] || continue

		config_get mode "$vif" mode
		case "$mode" in
			ap|wrap|ap_monitor|ap_smart_monitor|ap_lp_iot)
				"$device_if" "$phy" setCountryID 843
				return 0;
			;;
		*) ;;
		esac
	done

	return 0
}

config_low_targ_clkspeed() {
        local board_name
        [ -f /tmp/sysinfo/board_name ] && {
                board_name=$(cat /tmp/sysinfo/board_name)
        }

        case "$board_name" in
                ap147 | ap151)
                   echo "true"
                ;;
                *) echo "false"
                ;;
        esac
}

# configure tx queue fc_buf_max
config_tx_fc_buf() {
	local phy="$1"
	local board_name
	[ -f /tmp/sysinfo/board_name ] && {
		board_name=$(cat /tmp/sysinfo/board_name)
	}
	memtotal=$(grep MemTotal /proc/meminfo | awk '{print $2}')

	case "$board_name" in
		ap-dk*)
			if [ $memtotal -le 131072 ]; then
				# 4MB tx queue max buffer size
				"$device_if" "$phy" fc_buf_max 4096
				"$device_if" "$phy" fc_q_max 512
				"$device_if" "$phy" fc_q_min 32
			elif [ $memtotal -le 256000 ]; then
				# 8MB tx queue max buffer size
				"$device_if" "$phy" fc_buf_max 8192
				"$device_if" "$phy" fc_q_max 1024
				"$device_if" "$phy" fc_q_min 64
			fi
				# default value from code memsize > 256MB
		;;

		*)
		;;
	esac
}

#
# Workaround for improving 2.4 GHz Wi-Fi connection in noisy environment as
# per Netgear's requirement.
#
# Should be only used in CSU1.
#
do_improve_wl_con() {
	local device="$1"

	local vifs
	local vif
	local ifname

	config_get vifs "$device" vifs
	for vif in $vifs; do
		config_get ifname "$vif" ifname
		wifitool $ifname setUnitTestCmd 67 5 16 1 -5 55 55
	done
}

improve_wl_con_qcawifi() {
	local device="$1"

	local improve_wl_con

	config_get improve_wl_con "$device" improve_wl_con
	[ "$improve_wl_con" = "1" ] && do_improve_wl_con $device
	return 0
}

update_ini_file()
{
	grep -q $1 /ini/global.ini && sed -i "/$1=/c $1=$2" /ini/global.ini || echo $1=$2 >> /ini/global.ini
	sync
}

update_internal_ini()
{
	grep -q $2 /ini/internal/$1 && sed -i "/$2=/c $2=$3" /ini/internal/$1 || echo $2=$3 >> /ini/internal/$1
	sync
}

update_ini_for_lowmem()
{
	update_internal_ini $1 dp_rxdma_monitor_buf_ring 128
	update_internal_ini $1 dp_rxdma_monitor_dst_ring 128
	update_internal_ini $1 dp_rxdma_monitor_desc_ring 128
	update_internal_ini $1 dp_rxdma_monitor_status_ring 512
	update_internal_ini $1 enable_daemon_support 0
	update_internal_ini $1 enable_cold_boot_support 0
	sync
}

update_ini_nss_info()
{
	[ -f /lib/wifi/wifi_nss_hk_olnum ] && { \
		local hk_ol_num="$(cat /lib/wifi/wifi_nss_hk_olnum)"
		if [ -e /sys/firmware/devicetree/base/MP_256 ]; then
			update_internal_ini QCA8074V2_i.ini dp_nss_comp_ring_size 0x2000
		elif [ -e /sys/firmware/devicetree/base/MP_512 ]; then
			if [ $hk_ol_num -eq 3 ]; then
				update_internal_ini QCA8074V2_i.ini dp_nss_comp_ring_size 0x3000
			else
				update_internal_ini QCA8074V2_i.ini dp_nss_comp_ring_size 0x4000
			fi
		else
			if [ $hk_ol_num -eq 3 ]; then
				update_internal_ini QCA8074V2_i.ini dp_nss_comp_ring_size 0x8000
			else
				update_internal_ini QCA8074V2_i.ini dp_nss_comp_ring_size 0x8000
			fi
		fi
	}
	sync
}


update_ini_for_512MP()
{
	update_internal_ini $1 dp_tx_desc 0x4000
	update_internal_ini $1 dp_rxdma_monitor_buf_ring 128
	update_internal_ini $1 dp_rxdma_monitor_dst_ring 128
	update_internal_ini $1 dp_rxdma_monitor_desc_ring 128
	update_internal_ini $1 dp_rxdma_monitor_status_ring 512
	sync
}

update_ini_for_monitor_buf_ring()
{
	update_internal_ini $1 dp_rxdma_monitor_buf_ring 8192
	update_internal_ini $1 dp_rxdma_monitor_dst_ring 8192
	update_internal_ini $1 dp_rxdma_monitor_desc_ring 8192
	update_internal_ini $1 dp_rxdma_monitor_status_ring 2048
	sync
}

load_qcawificfg80211() {
	local umac_args
	local qdf_args
	local ol_args
	local cfg_low_targ_clkspeed
	local qca_da_needed=0
	local device
	local board_name
	local def_pktlog_support=1
	local hk_ol_num=0

	echo -n "/ini" > /sys/module/firmware_class/parameters/path
	is_wal=`grep waltest_mode /proc/cmdline | wc -l`
	[ $is_wal = 1 ] && return

	if [ ! -d "/cfg/default" ]; then
		mkdir -p /cfg/default
		cp -rf /ini/* /cfg/default/
	fi

	cp /cfg/default/* /ini/

	local enable_cfg80211=`uci show qcacfg80211.config.enable |grep "qcacfg80211.config.enable='0'"`
	[ -n "$enable_cfg80211" ] && echo "qcawificfg80211 configuration is disable" > /dev/console && return 1;

	# Making sure all the radio types are 'qcawificfg80211'
        local check_qcawifi=`cat /etc/config/wireless | grep -Eo "\s*option\s*type\s*['\\s]*qcawifi['\\s]*$"`
        [ -n "$check_qcawifi" ] && sed -ie "s/\\soption\\stype.*$/\\toption type 'qcawificfg80211'/" /etc/config/wireless

	lock /var/run/wifilock
	[ -f /tmp/sysinfo/board_name ] && {
		board_name=$(cat /tmp/sysinfo/board_name)
	}
	memtotal=$(grep MemTotal /proc/meminfo | awk '{print $2}')

	case "$board_name" in
		ap-dk01.1-c1 | ap-dk01.1-c2 | ap-dk04.1-c1 | ap-dk04.1-c2 | ap-dk04.1-c3)
			if [ $memtotal -le 131072 ]; then
				echo 1 > /proc/net/skb_recycler/max_skbs
				echo 1 > /proc/net/skb_recycler/max_spare_skbs
				update_ini_file low_mem_system 1
			fi
		;;
		ap152 | ap147 | ap151 | ap135 | ap137)
			if [ $memtotal -le 66560 ]; then
				def_pktlog_support=0
			fi
		;;
	esac

	update_ini_file cfg80211_config "1"
	config_get_bool testmode qcawifi testmode
	[ -n "$testmode" ] && append ol_args "testmode=$testmode"

	config_get vow_config qcawifi vow_config
	[ -n "$vow_config" ] && update_ini_file vow_config "$vow_config"

	config_get carrier_vow_config qcawifi carrier_vow_config
	[ -n "$carrier_vow_config" ] && update_ini_file carrier_vow_config "$carrier_vow_config"

	config_get fw_vow_stats_enable qcawifi fw_vow_stats_enable
	[ -n "$fw_vow_stats_enable" ] && update_ini_file fw_vow_stats_enable "$fw_vow_stats_enable"

	config_get ol_bk_min_free qcawifi ol_bk_min_free
	[ -n "$ol_bk_min_free" ] && update_ini_file OL_ACBKMinfree "$ol_bk_min_free"

	config_get ol_be_min_free qcawifi ol_be_min_free
	[ -n "$ol_be_min_free" ] && update_ini_file OL_ACBEMinfree "$ol_be_min_free"

	config_get ol_vi_min_free qcawifi ol_vi_min_free
	[ -n "$ol_vi_min_free" ] && update_ini_file OL_ACVIMinfree "$ol_vi_min_free"

	config_get ol_vo_min_free qcawifi ol_vo_min_free
	[ -n "$ol_vo_min_free" ] && update_ini_file OL_ACVOMinfree "$ol_vo_min_free"

	config_get_bool ar900b_emu qcawifi ar900b_emu
	[ -n "$ar900b_emu" ] && append ol_args "ar900b_emu=$ar900b_emu"

	config_get frac qcawifi frac
	[ -n "$frac" ] && append ol_args "frac=$frac"

	config_get intval qcawifi intval
	[ -n "$intval" ] && append ol_args "intval=$intval"

	config_get atf_mode qcawifi atf_mode
	[ -n "$atf_mode" ] && append umac_args "atf_mode=$atf_mode"

        config_get atf_msdu_desc qcawifi atf_msdu_desc
        [ -n "$atf_msdu_desc" ] && append umac_args "atf_msdu_desc=$atf_msdu_desc"

        config_get atf_peers qcawifi atf_peers
        [ -n "$atf_peers" ] && append umac_args "atf_peers=$atf_peers"

        config_get debugtxbf_ulofdma qcawifi debugtxbf_ulofdma
        [ "$debugtxbf_ulofdma" = "Y" ] && echo Y > /sys/module/cnss2/parameters/qca6290_support
        [ "$debugtxbf_ulofdma" = "N" ] && echo N > /sys/module/cnss2/parameters/qca6290_support

        config_get atf_max_vdevs qcawifi atf_max_vdevs
        [ -n "$atf_max_vdevs" ] && append umac_args "atf_max_vdevs=$atf_max_vdevs"

	config_get fw_dump_options qcawifi fw_dump_options
	[ -n "$fw_dump_options" ] && update_ini_file fw_dump_options "$fw_dump_options"

	config_get enableuartprint qcawifi enableuartprint
	[ -n "$enableuartprint" ] && update_ini_file enableuartprint "$enableuartprint"

	config_get ar900b_20_targ_clk qcawifi ar900b_20_targ_clk
	[ -n "$ar900b_20_targ_clk" ] && append ol_args "ar900b_20_targ_clk=$ar900b_20_targ_clk"

	config_get qca9888_20_targ_clk qcawifi qca9888_20_targ_clk
	[ -n "$qca9888_20_targ_clk" ] && append ol_args "qca9888_20_targ_clk=$qca9888_20_targ_clk"

        cfg_low_targ_clkspeed=$(config_low_targ_clkspeed)
        [ -z "$qca9888_20_targ_clk" ] && [ $cfg_low_targ_clkspeed = "true" ] && append ol_args "qca9888_20_targ_clk=300000000"

	config_get max_descs qcawifi max_descs
	[ -n "$max_descs" ] && update_ini_file max_descs "$max_descs"

	config_get max_peers qcawifi max_peers
	[ -n "$max_peers" ] && update_ini_file max_peers "$max_peers"

	config_get cce_disable qcawifi cce_disable
	[ -n "$cce_disable" ] && update_ini_file cce_disable "$cce_disable"

	config_get qwrap_enable qcawifi qwrap_enable 0
	[ -n "$qwrap_enable" ] && update_ini_file qwrap_enable "$qwrap_enable"

	config_get otp_mod_param qcawifi otp_mod_param
	[ -n "$otp_mod_param" ] && update_ini_file otp_mod_param "$otp_mod_param"

	config_get max_active_peers qcawifi max_active_peers
	[ -n "$max_active_peers" ] && update_ini_file max_active_peers "$max_active_peers"

	config_get enable_smart_antenna qcawifi enable_smart_antenna
	[ -n "$enable_smart_antenna" ] && update_ini_file enable_smart_antenna "$enable_smart_antenna"

	config_get sa_validate_sw qcawifi sa_validate_sw
	[ -n "$sa_validate_sw" ] && update_ini_file sa_validate_sw "$sa_validate_sw"

	if [ -e /sys/firmware/devicetree/base/MP_512 ]; then
		config_get enable_monitor_mode qcawifi enable_monitor_mode
		if [ -n "$enable_monitor_mode" ]; then
			update_ini_for_monitor_buf_ring QCA8074_i.ini
			update_ini_for_monitor_buf_ring QCA8074V2_i.ini
		fi
	fi

	config_get nss_wifi_olcfg qcawifi nss_wifi_olcfg
	if [ -n "$nss_wifi_olcfg" ]; then
		update_ini_file nss_wifi_olcfg "$nss_wifi_olcfg"
		config_get nss_wifi_nxthop_cfg qcawifi nss_wifi_nxthop_cfg
		if [ -n "$nss_wifi_nxthop_cfg" ]; then
		    update_ini_file nss_wifi_nxthop_cfg "$nss_wifi_nxthop_cfg"
		fi
	elif [ -f /lib/wifi/wifi_nss_olcfg ]; then
		nss_wifi_olcfg="$(cat /lib/wifi/wifi_nss_olcfg)"

		if [ $nss_wifi_olcfg != 0 ]; then
			if [ -f /lib/wifi/wifi_nss_override ] && [ $(cat /lib/wifi/wifi_nss_override) = 1 ]; then
				echo "NSS offload disabled due to unsupported config" >&2
				update_ini_file nss_wifi_olcfg 0
			else
				update_ini_file nss_wifi_olcfg "$nss_wifi_olcfg"
				update_ini_file rx_hash 0
			fi
		else
			update_ini_file nss_wifi_olcfg 0
		fi
	fi

	config_get max_clients qcawifi max_clients
	[ -n "$max_clients" ] && update_ini_file max_clients "$max_clients"

	config_get enable_rdk_stats qcawifi enable_rdk_stats
	[ -n "$enable_rdk_stats" ] && update_ini_file enable_rdk_stats "$enable_rdk_stats"

	config_get max_vaps qcawifi max_vaps
	[ -n "$max_vaps" ] && update_ini_file max_vaps "$max_vaps"

	config_get enable_smart_antenna_da qcawifi enable_smart_antenna_da
	[ -n "$enable_smart_antenna_da" ] && update_ini_file enable_smart_antenna_da "$enable_smart_antenna_da"

	config_get prealloc_disabled qcawifi prealloc_disabled
	[ -n "$prealloc_disabled" ] && append qdf_args "prealloc_disabled=$prealloc_disabled"

	if [ -n "$nss_wifi_olcfg" ] && [ "$nss_wifi_olcfg" != "0" ]; then
	local mp_256="$(ls /proc/device-tree/ | grep -rw "MP_256")"
	local mp_512="$(ls /proc/device-tree/ | grep -rw "MP_512")"
	sysctl dev.nss.n2hcfg.n2h_high_water_core0 >/dev/null 2>/dev/null

	#update the ini nss info
	update_ini_nss_info

	#If this is a first time load, then remove the one radio up file
	if [ ! -d /sys/module/qca_ol ] && [ -f /tmp/wifi_nss_up_one_radio ]; then
		rm /tmp/wifi_nss_up_one_radio
	fi

	if [ "$mp_256" == "MP_256" ]; then
		#total pbuf size is 160 bytes,allocate memory for 8712 pbufs
		sysctl -w dev.nss.n2hcfg.extra_pbuf_core0=1400000 >/dev/null 2>/dev/null
		sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=20432 >/dev/null 2>/dev/null
		sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=0 >/dev/null 2>/dev/null
	elif [ "$mp_512" == "MP_512" ]; then
		[ -d /sys/module/qca_ol ] || { \
			hk_ol_num="$(cat /lib/wifi/wifi_nss_hk_olnum)"
			if [ $hk_ol_num -eq 3 ]; then
				#total pbuf size is 160 bytes,allocate memory for 28120 pbufs
				sysctl -w dev.nss.n2hcfg.extra_pbuf_core0=4500000 >/dev/null 2>/dev/null
				sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=39840 >/dev/null 2>/dev/null
				sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=8192 >/dev/null 2>/dev/null
			else
				#total pbuf size is 160 bytes,allocate memory for 18904 pbufs
				sysctl -w dev.nss.n2hcfg.extra_pbuf_core0=3100000 >/dev/null 2>/dev/null
				sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=30624 >/dev/null 2>/dev/null
				sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=8192 >/dev/null 2>/dev/null
			fi
		}
	else
	case "$board_name" in
	ap-hk09*)
			local soc_version_major
			[ -f /sys/firmware/devicetree/base/soc_version_major ] && {
				soc_version_major="$(hexdump -n 1 -e '"%1d"' /sys/firmware/devicetree/base/soc_version_major)"
			}
			if [ $soc_version_major = 2 ];then
				[ -d /sys/module/qca_ol ] || { \
					#total pbuf size is 160 bytes,allocate memory for 55672 pbufs
					sysctl -w dev.nss.n2hcfg.extra_pbuf_core0=9000000 >/dev/null 2>/dev/null
					sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=67392 >/dev/null 2>/dev/null
					#initially after init 4k buf for 5G and 4k for 2G will be allocated, later range will be configured
					sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=40960 >/dev/null 2>/dev/null
				}
			else
				#total pbuf size is 160 bytes,allocate memory for 57184 pbufs
				sysctl -w dev.nss.n2hcfg.extra_pbuf_core0=9200000 >/dev/null 2>/dev/null
				sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=68904 >/dev/null 2>/dev/null
				sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=32768 >/dev/null 2>/dev/null
			fi
	;;
	ap-ac01)
		#total pbuf size is 160 bytes,allocate memory for 14712 pbufs
		sysctl -w dev.nss.n2hcfg.extra_pbuf_core0=2400000 >/dev/null 2>/dev/null
		sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=26432 >/dev/null 2>/dev/null
		sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=0 >/dev/null 2>/dev/null

	;;
	ap-ac02)
		#total pbuf size is 160 bytes,allocate memory for 18808 pbufs
		sysctl -w dev.nss.n2hcfg.extra_pbuf_core0=3100000 >/dev/null 2>/dev/null
		sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=30528 >/dev/null 2>/dev/null
		sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=4096 >/dev/null 2>/dev/null
	;;
	ap-hk* | ap-oak*)
		hk_ol_num="$(cat /lib/wifi/wifi_nss_hk_olnum)"
		[ -d /sys/module/qca_ol ] || { \
			if [ $hk_ol_num -eq 3 ]; then
				#total pbuf size is 160 bytes,allocate memory for 77176 pbufs
				sysctl -w dev.nss.n2hcfg.extra_pbuf_core0=13000000 >/dev/null 2>/dev/null
				sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=88896 >/dev/null 2>/dev/null
				#initially after init 4k buf for 5G and 4k for 2G will be allocated, then range will be configured
				sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=53248 >/dev/null 2>/dev/null
			else
				#total pbuf size is 160 bytes,allocate memory for 55672 pbufs
				sysctl -w dev.nss.n2hcfg.extra_pbuf_core0=9000000 >/dev/null 2>/dev/null
				sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=67392 >/dev/null 2>/dev/null
				#initially after init 4k buf for 5G and 4k for 2G will be allocated, then range will be configured
				sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=40960 >/dev/null 2>/dev/null
			fi
		}
	;;
	*)
		#total pbuf size is 160 bytes,allocate memory for 48456 pbufs
		sysctl -w dev.nss.n2hcfg.extra_pbuf_core0=7800000 >/dev/null 2>/dev/null
		sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=60176 >/dev/null 2>/dev/null
		sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=35840 >/dev/null 2>/dev/null
	;;
	esac
	fi
	fi

	config_get lteu_support qcawifi lteu_support
	[ -n "$lteu_support" ] && update_ini_file lteu_support "$lteu_support"

        config_get tgt_sched_params qcawifi tgt_sched_params
        [ -n "$tgt_sched_params" ] && update_ini_file tgt_sched_params "$tgt_sched_params"

	config_get enable_mesh_support qcawifi enable_mesh_support
	[ -n "$enable_mesh_support" ] && update_ini_file mesh_support "$enable_mesh_support"

        config_get enable_eapol_minrate qcawifi enable_eapol_minrate
	[ -n "$enable_eapol_minrate" ] && update_ini_file eapol_minrate_set "$enable_eapol_minrate"

        config_get set_eapol_minrate_ac qcawifi set_eapol_minrate_ac
	[ -n "$set_eapol_minrate_ac" ] && update_ini_file eapol_minrate_ac_set "$set_eapol_minrate_ac"

	if [ -n "$enable_mesh_support" ]; then
		config_get enable_mesh_peer_cap_update qcawifi enable_mesh_peer_cap_update
		[ -n "$enable_mesh_peer_cap_update" ] && append umac_args "enable_mesh_peer_cap_update=$enable_mesh_peer_cap_update"
	fi

	config_get enable_pktlog_support qcawifi enable_pktlog_support $def_pktlog_support
	[ -n "$enable_pktlog_support" ] && append umac_args "enable_pktlog_support=$enable_pktlog_support"

	config_get g_unicast_deauth_on_stop qcawifi g_unicast_deauth_on_stop $g_unicast_deauth_on_stop
	[ -n "$g_unicast_deauth_on_stop" ] && append umac_args "g_unicast_deauth_on_stop=$g_unicast_deauth_on_stop"

	config_get beacon_offload_disable qcawifi beacon_offload_disable
	[ -n "$beacon_offload_disable" ] && update_ini_file beacon_offload_disable "$beacon_offload_disable"

	config_get spectral_disable qcawifi spectral_disable
	[ -n "$spectral_disable" ] && update_ini_file spectral_disable "$spectral_disable"

        config_get twt_enable qcawifi twt_enable
        [ -n "$twt_enable" ] && update_ini_file twt_enable "$twt_enable"

        config_get mbss_ie_enable qcawifi mbss_ie_enable
        [ -n "$mbss_ie_enable" ] && update_ini_file mbss_ie_enable "$mbss_ie_enable"

	for mod in $(cat /lib/wifi/qca-wifi-modules); do
		case ${mod} in
			umac) [ -d /sys/module/${mod} ] || { \

				insmod ${mod} ${umac_args} || { \
					lock -u /var/run/wifilock
					unload_qcawificfg80211
					return 1
				}
			};;

			qdf) [ -d /sys/module/${mod} ] || { \
				insmod ${mod} ${qdf_args} || { \
					lock -u /var/run/wifilock
					unload_qcawificfg80211
					return 1
				}
			};;

			qca_ol) [ -d /sys/module/${mod} ] || { \
				do_cold_boot_calibration
				insmod ${mod} ${ol_args} || { \
					lock -u /var/run/wifilock
					unload_qcawificfg80211
					return 1
				}
			};;

			qca_da|ath_dev|hst_tx99|ath_rate_atheros|ath_hal) [ -f /tmp/no_qca_da ] || { \
				[ -d /sys/module/${mod} ] || { \
					insmod ${mod} || { \
						lock -u /var/run/wifilock
						unload_qcawificfg80211
						return 1
					}
				}
			};;

			ath_pktlog) [ $enable_pktlog_support -eq 0 ] || { \
				[ -d /sys/module/${mod} ] || { \
					insmod ${mod} || { \
						lock -u /var/run/wifilock
						unload_qcawificfg80211
						return 1
					}
				}
			};;

			*) [ -d /sys/module/${mod} ] || { \
				insmod ${mod} || { \
					lock -u /var/run/wifilock
					unload_qcawificfg80211
					return 1
				}
			};;

		esac
	done

       # Remove DA modules, if no DA chipset found
	for device in $(ls -d /sys/class/net/wifi* 2>&-); do
		[[ -f $device/is_offload ]] || {
			qca_da_needed=1
			break
		}
	done

	if [ $qca_da_needed -eq 0 ]; then
		if [ ! -f /tmp/no_qca_da ]; then
			echo "No Direct-Attach chipsets found." >/dev/console
			rmmod qca_da > /dev/null 2> /dev/null
			rmmod ath_dev > /dev/null 2> /dev/null
			rmmod hst_tx99 > /dev/null 2> /dev/null
			rmmod ath_rate_atheros > /dev/null 2> /dev/null
			rmmod ath_hal > /dev/null 2> /dev/null
			cat "1" > /tmp/no_qca_da
		fi
	fi

	if [ -f "/lib/update_smp_affinity.sh" ]; then
		. /lib/update_smp_affinity.sh
		config_foreach enable_smp_affinity_wifi wifi-device
	fi

	lock -u /var/run/wifilock
}

_DFS_HT160_workaround_qcacfg80211() {
	NOL_list=`cat "${WIFI_DFSNOL_FILE}_${1}_workaround" | cut -d '=' -f 2 | cut -b 1-4`

	for channel_frequence in $NOL_list
	do
		NOL_chan="$(((${channel_frequence}-5000)/5))"
		[ "$NOL_chan" -le 65 ] && band2=1
		[ "$NOL_chan" -le 128 -a "$NOL_chan" -ge 100 ] && band3=1
	done
	[ "x$band2" = "x1" -a "x$band3" = "x1" ] && htmode=HT80
	rm "${WIFI_DFSNOL_FILE}_${1}_workaround"
}

# $1: device
_import_dfsnol_status_qcacfg80211() {
	local wifi_topology_file=$(grep WIFI_TOPOLOGY_FILE /etc/ath/wifi.conf | awk -F= '{print $2}')
	local _prefix=$(awk -v input_wifidev="$1" -v output_rule=prefix -f /etc/search-wifi-interfaces.awk "$wifi_topology_file" | cut -c 1-3)

	# This function should only work on 5 GHz devices
	[ "$_prefix" != "wla" ] && return

	radartool -i "$1" setnol "${WIFI_DFSNOL_FILE}_${1}" >> "${WIFI_DFSNOL_FILE}_${1}_workaround"
	rm "${WIFI_DFSNOL_FILE}_${1}"
}

# $1: device
_backup_dfsnol_status_qcacfg80211() {
	local wifi_topology_file=$(grep WIFI_TOPOLOGY_FILE /etc/ath/wifi.conf | awk -F= '{print $2}')
	local _prefix=$(awk -v input_wifidev="$1" -v output_rule=prefix -f /etc/search-wifi-interfaces.awk "$wifi_topology_file" | cut -c 1-3)

	# This function should only work on 5 GHz devices
	[ "$_prefix" != "wla" ] && return

	if [ -e "${WIFI_DFSNOL_FILE}_${1}" ]; then
		echo "DFS: ${WIFI_DFSNOL_FILE}_${1} file exist, cannot backup DFS info."
	else
		radartool -i "$1" getnol "${WIFI_DFSNOL_FILE}_${1}"
	fi
}

unload_qcawificfg80211() {
	# "config_load wireless" will re-export parameter which will affect original parameter.
	# ex. Device parameter will be changed to last wifiX. At Orbi project, device will be changed to wifi2.
	config_load wireless
	config_foreach disable_qcawifi wifi-device

	eval "type qwrap_teardown" >/dev/null 2>&1 && qwrap_teardown

	rm -f /var/run/iface_mgr.conf
	[ -r /var/run/iface_mgr.pid ] && kill "$(cat "/var/run/iface_mgr.pid")"
	rm -f /var/run/iface_mgr.pid
	killall iface-mgr

	eval "type lowi_teardown" >/dev/null 2>&1 && lowi_teardown
	sleep 3
	killall cfr_test_app
	lock /var/run/wifilock
	for mod in $(cat /lib/wifi/qca-wifi-modules | sed '1!G;h;$!d'); do
        case ${mod} in
            mem_manager) continue;
            esac
		[ -d /sys/module/${mod} ] && rmmod ${mod}
	done
	lock -u /var/run/wifilock
}

disable_recover_qcawificfg80211() {
	disable_qcawificfg80211 $@ 1
}

enable_recover_qcawificfg80211() {
	enable_qcawificfg80211 $@ 1
}

#
# _disable_qcawificfg80211 <DEVICE> <RECOVER>
# _disable_qcawificfg80211 <DEVICE> dni <RECOVER>
#
_disable_qcawificfg80211() {
	local device="$1"
	local parent
	local retval=0

	#
	# $dni = "dni": Adopt DNI-specific behaviors
	# $dni = "": keep QCA's behaviors
	#
	local dni
	if [ "$2" = "dni" ]; then
		dni="$2"

		# Change $@ back to "<DEVICE> <RECOVER>"
		shift 2
		eval "set '$device' \"$@\""
	fi
	local recover="$2"

	# Does this "$device" contain only backhaul ifaces? 1 = yes, 0 = no
	local backhaul_only=1

	echo "$DRIVERS disable radio $1" >/dev/console

	find_qcawifi_phy "$device" >/dev/null || return 1

	config_get ODM "$device" ODM
	[ "$ODM" = "dni" ] && _backup_dfsnol_status_qcacfg80211 "$device"

	# If qrfs is disabled in enable_qcawifi(),need to enable it
	if [ -f /var/qrfs_disabled_by_wifi ] && [ $(cat /var/qrfs_disabled_by_wifi) == 1 ]; then
		echo "1" > /proc/qrfs/enable
		echo "0" > /var/qrfs_disabled_by_wifi
	fi

	# disable_qcawifi also gets called for disabled radio during wifi up. Don't
	# remove the files if it gets from disabled radio.
	config_get disabled "$device" disabled
	if [ -f /tmp/wifi_nss_up_one_radio ] && [ "$disabled" = "0" ]; then
		rm /tmp/wifi_nss_up_one_radio
	fi
	config_get phy "$device" phy

	set_wifi_down "$device"

	_clear_wifi_ebtables_qcacfg80211

	include /lib/network
	cd /sys/class/net
	for dev in *; do
		[ -f /sys/class/net/${dev}/parent ] && { \
			local parent=$(cat /sys/class/net/${dev}/parent)
			[ -n "$parent" -a "$parent" = "$device" ] && { \
				[ -f "/var/run/hostapd-${dev}.lock" ] && { \
					wpa_cli -g /var/run/hostapd/global raw REMOVE ${dev}
					rm /var/run/hostapd-${dev}.lock
				}
				[ -f "/var/run/wpa_supplicant-${dev}.lock" ] && { \
					wpa_cli -g /var/run/wpa_supplicantglobal  interface_remove  ${dev}
					rm /var/run/wpa_supplicant-${dev}.lock
				}
				[ -f "/var/run/wapid-${dev}.conf" ] && { \
					kill "$(cat "/var/run/wifi-${dev}.pid")"
				}
				ifconfig "$dev" down
				unbridge "$dev"
				if [ -z "$recover" ] || [ "$recover" -eq "0" ]; then
				    iw "$dev" del
				fi
			}
			[ -f /var/run/hostapd_cred_${device}.bin ] && { \
				rm /var/run/hostapd_cred_${device}.bin
			}
		}
	done

	config_get vifs "$device" vifs
	for vif in $vifs; do
		config_get backhaul "$vif" backhaul 0
		[ -n "$backhaul" -a "$backhaul" = 1 ] && continue
		backhaul_only=0
		break
	done

	# don't count wifi status for backhaul vap only radio
	[ "$backhaul_only" = "0" ] && {
		# delete wlan uptime file
		band_type=`grep "^[ga].*_device" /etc/ath/wifi.conf | grep $phy | cut -c 1`
		if [ "$band_type" = "g" ]; then
			rm /tmp/WLAN_uptime
			echo "OFF" > /tmp/WLAN_2G_status
		elif [ "$band_type" = "a" ]; then
			rm /tmp/WLAN_uptime_5G
			echo "OFF" > /tmp/WLAN_5G_status
		fi
	}

	return 0
}

destroy_vap() {
	local ifname="$1"
	ifconfig $ifname down
	wlanconfig $ifname destroy
}

#
# disable_qcawificfg80211 <DEVICE> <RECOVER>
# disable_qcawificfg80211 <DEVICE> dni <RECOVER>
#
disable_qcawificfg80211() {
        local device="$1"
        lock /var/run/wifilock
        _disable_qcawificfg80211 "$device" $2 $3
        lock -u /var/run/wifilock
}

_set_boarddata_qcacfg80211() {
	local country_code="$1"

	local country

	REGULATORY_DOMAIN=US
	IPQ4019_BDF_DIR=/lib/firmware/IPQ4019/hw.1
	QCA9984_BDF_DIR=/lib/firmware/QCA9984/hw.1
	QCA9888_BDF_DIR=/lib/firmware/QCA9888/hw.2
	IPQ8074_BDF_DIR=/lib/firmware/IPQ8074

	_countrycode2str_qcacfg80211 "$country_code" country
	case "$country" in
		MY|458|CN|156|TH|764)
			REGULATORY_DOMAIN=CN
			;;
		*)
			REGULATORY_DOMAIN="$country"
			;;
	esac

	if [ -d $IPQ4019_BDF_DIR/$REGULATORY_DOMAIN ]; then
		/bin/cp -f $IPQ4019_BDF_DIR/$REGULATORY_DOMAIN/* $IPQ4019_BDF_DIR/
	elif [ -d $IPQ4019_BDF_DIR/US ]; then
		/bin/cp -f $IPQ4019_BDF_DIR/US/* $IPQ4019_BDF_DIR/
	fi

	if [ -d $QCA9984_BDF_DIR/$REGULATORY_DOMAIN ]; then
		/bin/cp -f $QCA9984_BDF_DIR/$REGULATORY_DOMAIN/* $QCA9984_BDF_DIR/
	elif [ -d $QCA9984_BDF_DIR/US ]; then
		/bin/cp -f $QCA9984_BDF_DIR/US/* $QCA9984_BDF_DIR/
	fi

	if [ -d $QCA9888_BDF_DIR/$REGULATORY_DOMAIN ]; then
		/bin/cp -f $QCA9888_BDF_DIR/$REGULATORY_DOMAIN/* $QCA9888_BDF_DIR/
	elif [ -d $QCA9888_BDF_DIR/US ]; then
		/bin/cp -f $QCA9888_BDF_DIR/US/* $QCA9888_BDF_DIR/
	fi

	if [ -d $IPQ8074_BDF_DIR/$REGULATORY_DOMAIN ]; then
		/bin/cp -f $IPQ8074_BDF_DIR/$REGULATORY_DOMAIN/* $IPQ8074_BDF_DIR/
	elif [ -d $IPQ8074_BDF_DIR/US ]; then
		/bin/cp -f $IPQ8074_BDF_DIR/US/* $IPQ8074_BDF_DIR/
	fi
}

#
# enable_qcawificfg80211 <DEVICE> <RECOVER>
# enable_qcawificfg80211 <DEVICE> dni <RECOVER>
#
enable_qcawificfg80211() {
	local device="$1"
	local count=0
	echo "$DRIVERS: enable radio $1" >/dev/console
	local num_radio_instamode=0

	#
	# $dni = "dni": Adopt DNI-specific behaviors
	# $dni = "": keep QCA's behaviors
	#
	local dni
	if [ "$2" = "dni" ]; then
		dni="$2"

		# Change $@ back to "<DEVICE> <RECOVER>"
		shift 2
		eval "set '$device' \"$@\""
	fi
	local recover="$2"
	local hk_ol_num=0
	local hwcaps
	local board_name
	[ -f /tmp/sysinfo/board_name ] && {
		board_name=$(cat /tmp/sysinfo/board_name)
	}

	# Does this "$device" contain only backhaul ifaces? 1 = yes, 0 = no
	local backhaul_only=1

	local wifi_topology_file=`cat /etc/ath/wifi.conf | grep WIFI_TOPOLOGY_FILE | awk -F= '{print $2}'`

	config_get country "$device" country
	_set_boarddata_qcacfg80211 "$country"

	#
	# Bug 92172: free memory caches so that Wi-Fi can get enough memory to
	# start while USB disk file is being transferred.
	#
	echo 3 > /proc/sys/vm/drop_caches
	sleep 2

	load_qcawificfg80211

	find_qcawifi_phy "$device" || return 1


	if [ ! -f /lib/wifi/wifi_nss_override ]; then
		if [ -f /lib/wifi/wifi_nss_olcfg ] && [ $(cat /lib/wifi/wifi_nss_olcfg) != 0 ]; then
			touch /lib/wifi/wifi_nss_override
			echo 0 > /lib/wifi/wifi_nss_override
		fi
	fi

	if [ -f /lib/wifi/wifi_nss_override ]; then
		cd /sys/class/net
		for all_device in $(ls -d wifi* 2>&-); do
			config_get_bool disabled "$all_device" disabled 0
			[ $disabled = 0 ] || continue
			config_get vifs "$all_device" vifs

			for vif in $vifs; do
				config_get mode "$vif" mode
				if [ $mode = "sta" ]; then
					num_radio_instamode=$(($num_radio_instamode + 1))
					break
				fi
			done
			if [ $num_radio_instamode = "0" ]; then
				break
			fi
		done

		nss_override="$(cat /lib/wifi/wifi_nss_override)"
		if [ $num_radio_instamode = "3" ]; then
			config_get nss_wifi_olcfg qcawifi nss_wifi_olcfg
			if [ -n "$nss_wifi_olcfg" ] && [ $nss_wifi_olcfg != 0 ]; then
				echo " Invalid Configuration: 3 stations in offload not supported"
				return 1
			fi
			if [ $nss_override = "0" ]; then
				echo 1 > /lib/wifi/wifi_nss_override
				unload_qcawificfg80211
				device=$1
				load_qcawificfg80211
			fi
		else
			if [ $nss_override != "0" ]; then
				echo 0 > /lib/wifi/wifi_nss_override
				unload_qcawificfg80211
				device=$1
				load_qcawificfg80211
			fi
		fi
	fi

	lock /var/run/wifilock

	config_get phy "$device" phy
	config_get device_if "$device" device_if "cfg80211tool"

	config_get interCACChan "$device" interCACChan
	[ -n "$interCACChan" ] && "$device_if" "$phy" interCACChan "$interCACChan"

	if [ -z "$country" ]; then
		if ! set_default_country $device; then
			lock -u /var/run/wifilock
			return 1
		fi
	else
		# If the country parameter is a number (either hex or decimal), we
		# assume it's a regulatory domain - i.e. we use "$device_if" setCountryID.
		# Else we assume it's a country code - i.e. we use "$device_if" setCountry.
		case "$country" in
			[0-9]*)
				"$device_if" "$phy" setCountryID "$country"
			;;
			*)
				[ -n "$country" ] && "$device_if" "$phy" setCountry "$country"
			;;
		esac
	fi

	config_get preCACEn "$device" preCACEn
	[ -n "$preCACEn" ] && "$device_if" "$phy" preCACEn "$preCACEn"

	config_get bsta_fixed_idmask "$device" bsta_fixed_idmask 255
	[ -n "$bsta_fixed_idmask" ] && "$device_if" "$phy" bsta_fixed_idmask "$bsta_fixed_idmask"

	config_get pCACTimeout "$device" pCACTimeout
	[ -n "$pCACTimeout" ] && "$device_if" "$phy" pCACTimeout "$pCACTimeout"

	config_get channel "$device" channel 0
	config_get vifs "$device" vifs
	config_get txpower "$device" txpower
	config_get tpscale "$device" tpscale
	[ -n "$tpscale" ] && "$device_if" "$phy" tpscale "$tpscale"
	config_get htmode "$device" htmode auto

	#
	# "$WIFI_CONF_ch36_spur_war_default" is expected to be set in
	# /etc/ath/wifi.conf
	#
	config_get edge_channel_deprioritize "$device" edge_channel_deprioritize \
			"${WIFI_CONF_ch36_spur_war_default:-0}"

	[ auto = "$channel" ] && channel=0
	[ AUTO = "$channel" ] && channel=0

	# WAR to not use chan 36 as primary channel, when using higher BW.
	if [ $channel -eq 36 ]; then
		if [ "${edge_channel_deprioritize:-0}" -eq 1 ]; then
			case "$board_name" in
				ap-hk*|ap-ac*|ap-oa*)
				[ HT20 != "$htmode" ] && channel=40 && echo " Primary channel is changed to 40"
				[ HT40+ = "$htmode" ] && htmode=HT40- && echo " Mode changed to HT40MINUS with channel 40"
			;;
				*)
			;;
			esac
		fi
	fi

	config_get_bool antdiv "$device" diversity
	config_get antrx "$device" rxantenna
	config_get anttx "$device" txantenna
	config_get_bool softled "$device" softled
	config_get antenna "$device" antenna
	config_get distance "$device" distance

	[ -n "$antdiv" ] && echo "antdiv option not supported on this driver"
	[ -n "$antrx" ] && echo "antrx option not supported on this driver"
	[ -n "$anttx" ] && echo "anttx option not supported on this driver"
	[ -n "$softled" ] && echo "softled option not supported on this driver"
	[ -n "$antenna" ] && echo "antenna option not supported on this driver"
	[ -n "$distance" ] && echo "distance option not supported on this driver"

	# Advanced QCA wifi per-radio parameters configuration
	config_get txchainmask "$device" txchainmask
	[ -n "$txchainmask" ] && "$device_if" "$phy" txchainmask "$txchainmask"

	config_get rxchainmask "$device" rxchainmask
	[ -n "$rxchainmask" ] && "$device_if" "$phy" rxchainmask "$rxchainmask"

        config_get regdomain "$device" regdomain
        [ -n "$regdomain" ] && "$device_if" "$phy" setRegdomain "$regdomain"

	config_get he_bsscolor "$device" he_bsscolor
	[ -n "$he_bsscolor" ] && "$device_if" "$phy" he_bsscolor ${he_bsscolor}

	config_get arp_protocol_tag "$device" rx_protocol_arp_type_tag
        [ -n "$arp_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 0 $arp_protocol_tag

        config_get dhcpv4_protocol_tag "$device" rx_protocol_dhcpv4_type_tag
        [ -n "$dhcpv4_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 4 $dhcpv4_protocol_tag

        config_get dhcpv6_protocol_tag "$device" rx_protocol_dhcpv6_type_tag
        [ -n "$dhcpv6_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 5 $dhcpv6_protocol_tag

        config_get dns_tcpv4_protocol_tag "$device" rx_protocol_dns_tcpv4_type_tag
        [ -n "$dns_tcpv4_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 6 $dns_tcpv4_protocol_tag

        config_get dns_tcpv6_protocol_tag "$device" rx_protocol_dns_tcpv6_type_tag
        [ -n "$dns_tcpv6_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 7 $dns_tcpv6_protocol_tag

        config_get dns_udpv4_protocol_tag "$device" rx_protocol_dns_udpv4_type_tag
        [ -n "$dns_udpv4_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 8 $dns_udpv4_protocol_tag

        config_get dns_udpv6_protocol_tag "$device" rx_protocol_dns_udpv6_type_tag
        [ -n "$dns_tcpv6_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 9 $dns_udpv6_protocol_tag

        config_get icmpv4_protocol_tag "$device" rx_protocol_icmpv4_type_tag
        [ -n "$icmpv4_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 10 $icmpv4_protocol_tag

        config_get icmpv6_protocol_tag "$device" rx_protocol_icmpv6_type_tag
        [ -n "$icmpv6_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 11 $icmpv6_protocol_tag

        config_get tcpv4_protocol_tag "$device" rx_protocol_tcpv4_type_tag
        [ -n "$tcpv4_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 12 $tcpv4_protocol_tag

        config_get tcpv6_protocol_tag "$device" rx_protocol_tcpv6_type_tag
        [ -n "$tcpv6_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 13 $tcpv6_protocol_tag

        config_get udpv4_protocol_tag "$device" rx_protocol_udpv4_type_tag
        [ -n "$udpv4_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 14 $udpv4_protocol_tag

        config_get udpv6_protocol_tag "$device" rx_protocol_udpv6_type_tag
        [ -n "$udpv6_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 15 $udpv6_protocol_tag

        config_get ipv4_protocol_tag "$device" rx_protocol_ipv4_type_tag
        [ -n "$ipv4_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 16 $ipv4_protocol_tag

        config_get ipv6_protocol_tag "$device" rx_protocol_ipv6_type_tag
        [ -n "$ipv6_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 17 $ipv6_protocol_tag

        config_get eap_protocol_tag "$device" rx_protocol_eap_type_tag
        [ -n "$eap_protocol_tag" ] && "$device_if" "$phy" set_rxProtocolTag 0 18 $eap_protocol_tag

	config_get AMPDU "$device" AMPDU
	[ -n "$AMPDU" ] && "$device_if" "$phy" AMPDU "$AMPDU"

	config_get ampdudensity "$device" ampdudensity
	[ -n "$ampdudensity" ] && "$device_if" "$phy" ampdudensity "$ampdudensity"

	config_get_bool AMSDU "$device" AMSDU
	[ -n "$AMSDU" ] && "$device_if" "$phy" AMSDU "$AMSDU"

	config_get AMPDULim "$device" AMPDULim
	[ -n "$AMPDULim" ] && "$device_if" "$phy" AMPDULim "$AMPDULim"

	config_get AMPDUFrames "$device" AMPDUFrames
	[ -n "$AMPDUFrames" ] && "$device_if" "$phy" AMPDUFrames "$AMPDUFrames"

	config_get AMPDURxBsize "$device" AMPDURxBsize
	[ -n "$AMPDURxBsize" ] && "$device_if" "$phy" AMPDURxBsize "$AMPDURxBsize"

	config_get_bool bcnburst "$device" bcnburst 0
	[ -n "$bcnburst" ] && "$device_if" "$phy" set_bcnburst "$bcnburst"

	config_get set_smart_antenna "$device" set_smart_antenna
	[ -n "$set_smart_antenna" ] && "$device_if" "$phy" setSmartAntenna "$set_smart_antenna"

	config_get current_ant "$device" current_ant
	[ -n  "$current_ant" ] && "$device_if" "$phy" current_ant "$current_ant"

	config_get default_ant "$device" default_ant
	[ -n "$default_ant" ] && "$device_if" "$phy" default_ant "$default_ant"

	config_get ant_retrain "$device" ant_retrain
	[ -n "$ant_retrain" ] && "$device_if" "$phy" ant_retrain "$ant_retrain"

	config_get retrain_interval "$device" retrain_interval
	[ -n "$retrain_interval" ] && "$device_if" "$phy" retrain_interval "$retrain_interval"

	config_get retrain_drop "$device" retrain_drop
	[ -n "$retrain_drop" ] && "$device_if" "$phy" retrain_drop "$retrain_drop"

	config_get ant_train "$device" ant_train
	[ -n "$ant_train" ] && "$device_if" "$phy" ant_train "$ant_train"

	config_get ant_trainmode "$device" ant_trainmode
	[ -n "$ant_trainmode" ] && "$device_if" "$phy" ant_trainmode "$ant_trainmode"

	config_get ant_traintype "$device" ant_traintype
	[ -n "$ant_traintype" ] && "$device_if" "$phy" ant_traintype "$ant_traintype"

	config_get ant_pktlen "$device" ant_pktlen
	[ -n "$ant_pktlen" ] && "$device_if" "$phy" ant_pktlen "$ant_pktlen"

	config_get ant_numpkts "$device" ant_numpkts
	[ -n "$ant_numpkts" ] && "$device_if" "$phy" ant_numpkts "$ant_numpkts"

	config_get ant_numitr "$device" ant_numitr
	[ -n "$ant_numitr" ] && "$device_if" "$phy" ant_numitr "$ant_numitr"

	config_get ant_train_thres "$device" ant_train_thres
	[ -n "$ant_train_thres" ] && "$device_if" "$phy" train_threshold "$ant_train_thres"

	config_get ant_train_min_thres "$device" ant_train_min_thres
	[ -n "$ant_train_min_thres" ] && "$device_if" "$phy" train_threshold "$ant_train_min_thres"

	config_get ant_traffic_timer "$device" ant_traffic_timer
	[ -n "$ant_traffic_timer" ] && "$device_if" "$phy" traffic_timer "$ant_traffic_timer"

	config_get dcs_enable "$device" dcs_enable
	[ -n "$dcs_enable" ] && "$device_if" "$phy" dcs_enable "$dcs_enable"

	config_get dcs_coch_int "$device" dcs_coch_int
	[ -n "$dcs_coch_int" ] && "$device_if" "$phy" set_dcs_coch_int "$dcs_coch_int"

	config_get dcs_errth "$device" dcs_errth
	[ -n "$dcs_errth" ] && "$device_if" "$phy" set_dcs_errth "$dcs_errth"

	config_get dcs_phyerrth "$device" dcs_phyerrth
	[ -n "$dcs_phyerrth" ] && "$device_if" "$phy" set_dcs_phyerrth "$dcs_phyerrth"

	config_get dcs_usermaxc "$device" dcs_usermaxc
	[ -n "$dcs_usermaxc" ] && "$device_if" "$phy" set_dcs_usermaxc "$dcs_usermaxc"

	config_get dcs_debug "$device" dcs_debug
	[ -n "$dcs_debug" ] && "$device_if" "$phy" set_dcs_debug "$dcs_debug"

	config_get set_ch_144 "$device" set_ch_144
	[ -n "$set_ch_144" ] && "$device_if" "$phy" setCH144 "$set_ch_144"
	[ -z "$set_ch_144" ] && {
		# Australia
		if [ "$country" = "36" -o "$country" = "702" -o "$country" = "344" -o "$country" = "704" ]; then
			"$device_if" "$phy" setCH144 0
		fi
	}

	config_get eppovrd_ch_144 "$device" eppovrd_ch_144
	[ -n "$eppovrd_ch_144" ] && "$device_if" "$phy" setCH144EppOvrd "$eppovrd_ch_144"

	config_get_bool ani_enable "$device" ani_enable
	[ -n "$ani_enable" ] && "$device_if" "$phy" ani_enable "$ani_enable"

	config_get_bool acs_bkscanen "$device" acs_bkscanen
	[ -n "$acs_bkscanen" ] && "$device_if" "$phy" acs_bkscanen "$acs_bkscanen"

	config_get acs_scanintvl "$device" acs_scanintvl
	[ -n "$acs_scanintvl" ] && "$device_if" "$phy" acs_scanintvl "$acs_scanintvl"

	config_get acs_rssivar "$device" acs_rssivar
	[ -n "$acs_rssivar" ] && "$device_if" "$phy" acs_rssivar "$acs_rssivar"

	config_get acs_chloadvar "$device" acs_chloadvar
	[ -n "$acs_chloadvar" ] && "$device_if" "$phy" acs_chloadvar "$acs_chloadvar"

	config_get acs_lmtobss "$device" acs_lmtobss
	[ -n "$acs_lmtobss" ] && "$device_if" "$phy" acs_lmtobss "$acs_lmtobss"

	config_get acs_ctrlflags "$device" acs_ctrlflags
	[ -n "$acs_ctrlflags" ] && "$device_if" "$phy" acs_ctrlflags "$acs_ctrlflags"

	config_get acs_dbgtrace "$device" acs_dbgtrace
	[ -n "$acs_dbgtrace" ] && "$device_if" "$phy" acs_dbgtrace "$acs_dbgtrace"

	config_get acs_samech_th "$device" acs_samech_th
	[ -n "$acs_samech_th" ] && "$device_if" "$phy" acs_samech_th "$acs_samech_th"

	config_get_bool dscp_ovride "$device" dscp_ovride
	[ -n "$dscp_ovride" ] && "$device_if" "$phy" set_dscp_ovride "$dscp_ovride"

	config_get reset_dscp_map "$device" reset_dscp_map
	[ -n "$reset_dscp_map" ] && "$device_if" "$phy" reset_dscp_map "$reset_dscp_map"

	config_get dscp_tid_map "$device" dscp_tid_map
	[ -n "$dscp_tid_map" ] && "$device_if" "$phy" set_dscp_tid_map $dscp_tid_map

        #Default enable IGMP overide & TID=6
	"$device_if" "$phy" sIgmpDscpOvrid 1
	"$device_if" "$phy" sIgmpDscpTidMap 6

	config_get_bool igmp_dscp_ovride "$device" igmp_dscp_ovride
	[ -n "$igmp_dscp_ovride" ] && "$device_if" "$phy" sIgmpDscpOvrid "$igmp_dscp_ovride"

	config_get igmp_dscp_tid_map "$device" igmp_dscp_tid_map
	[ -n "$igmp_dscp_tid_map" ] && "$device_if" "$phy" sIgmpDscpTidMap "$igmp_dscp_tid_map"

	config_get_bool hmmc_dscp_ovride "$device" hmmc_dscp_ovride
	[ -n "$hmmc_dscp_ovride" ] && "$device_if" "$phy" sHmmcDscpOvrid "$hmmc_dscp_ovride"

	config_get hmmc_dscp_tid_map "$device" hmmc_dscp_tid_map
	[ -n "$hmmc_dscp_tid_map" ] && "$device_if" "$phy" sHmmcDscpTidMap "$hmmc_dscp_tid_map"

	config_get_bool blk_report_fld "$device" blk_report_fld
	[ -n "$blk_report_fld" ] && "$device_if" "$phy" setBlkReportFld "$blk_report_fld"

	config_get_bool drop_sta_query "$device" drop_sta_query
	[ -n "$drop_sta_query" ] && "$device_if" "$phy" setDropSTAQuery "$drop_sta_query"

	config_get_bool burst "$device" burst
	[ -n "$burst" ] && "$device_if" "$phy" burst "$burst"

	config_get burst_dur "$device" burst_dur
	[ -n "$burst_dur" ] && "$device_if" "$phy" burst_dur "$burst_dur"

	config_get TXPowLim2G "$device" TXPowLim2G
	[ -n "$TXPowLim2G" ] && "$device_if" "$phy" TXPowLim2G "$TXPowLim2G"

	config_get TXPowLim5G "$device" TXPowLim5G
	[ -n "$TXPowLim5G" ] && "$device_if" "$phy" TXPowLim5G "$TXPowLim5G"

	config_get cck_tx_enable "$device" cck_tx_enable
	[ -n "$cck_tx_enable" ] && "$device_if" "$phy" cck_tx_enable "$cck_tx_enable"

	case "$board_name" in
		ap-hk*|ap-ac*|ap-oak*)
		echo "Enable ol_stats by default for Lithium platforms"
		"$device_if" "$phy" enable_ol_stats 1
	;;
		*) echo "ol_stats is disabled for non-Lithium platforms"
	;;
	esac

	config_get_bool enable_ol_stats "$device" enable_ol_stats
	[ -n "$enable_ol_stats" ] && "$device_if" "$phy" enable_ol_stats "$enable_ol_stats"

	config_get emiwar80p80 "$device" emiwar80p80
	[ -n "$emiwar80p80" ] && "$device_if" "$phy" emiwar80p80 "$emiwar80p80"

	config_get_bool rst_tso_stats "$device" rst_tso_stats
	[ -n "$rst_tso_stats" ] && "$device_if" "$phy" rst_tso_stats "$rst_tso_stats"

	config_get_bool rst_lro_stats "$device" rst_lro_stats
	[ -n "$rst_lro_stats" ] && "$device_if" "$phy" rst_lro_stats "$rst_lro_stats"

	config_get_bool rst_sg_stats "$device" rst_sg_stats
	[ -n "$rst_sg_stats" ] && "$device_if" "$phy" rst_sg_stats "$rst_sg_stats"

	config_get_bool set_fw_recovery "$device" set_fw_recovery
	[ -n "$set_fw_recovery" ] && "$device_if" "$phy" set_fw_recovery "$set_fw_recovery"

	config_get_bool allowpromisc "$device" allowpromisc
	[ -n "$allowpromisc" ] && "$device_if" "$phy" allowpromisc "$allowpromisc"

	config_get set_sa_param "$device" set_sa_param
	[ -n "$set_sa_param" ] && "$device_if" "$phy" set_sa_param $set_sa_param

	config_get_bool aldstats "$device" aldstats
	[ -n "$aldstats" ] && "$device_if" "$phy" aldstats "$aldstats"

	config_get macaddr "$device" macaddr
	[ -n "$macaddr" ] && "$device_if" "$phy" setHwaddr "$macaddr"

	config_get promisc "$device" promisc
	[ -n "$promisc" ] && "$device_if" "$phy" promisc $promisc

	config_get mode0 "$device" mode0
	[ -n "$mode0" ] && "$device_if" "$phy" fc_buf_min 2501

	config_get mode1 "$device" mode1
	[ -n "$mode1" ] && "$device_if" "$phy" fc_buf_min 0

	config_get nf_baseline "$device" nf_baseline
	[ -n "$nf_baseline" ] && "$device_if" "$phy" nf_baseline $nf_baseline

	handle_aggr_burst() {
		local value="$1"
		[ -n "$value" ] && "$device_if" "$phy" aggr_burst $value
	}

	config_list_foreach "$device" aggr_burst handle_aggr_burst

	config_get_bool block_interbss "$device" block_interbss
	[ -n "$block_interbss" ] && "$device_if" "$phy" block_interbss "$block_interbss"

	config_get set_pmf "$device" set_pmf
	[ -n "$set_pmf" ] && "$device_if" "$phy" set_pmf "${set_pmf}"

	config_get txbf_snd_int "$device" txbf_snd_int 100
	[ -n "$txbf_snd_int" ] && "$device_if" "$phy" txbf_snd_int "$txbf_snd_int"

	config_get mcast_echo "$device" mcast_echo
	[ -n "$mcast_echo" ] && "$device_if" "$phy" mcast_echo "${mcast_echo}"

	config_get obss_rssi_th "$device" obss_rssi_th 35
	[ -n "$obss_rssi_th" ] && "$device_if" "$phy" obss_rssi_th "${obss_rssi_th}"

	config_get obss_rxrssi_th "$device" obss_rxrssi_th 35
	[ -n "$obss_rxrssi_th" ] && "$device_if" "$phy" obss_rxrssi_th "${obss_rxrssi_th}"

        config_get acs_txpwr_opt "$device" acs_txpwr_opt
        [ -n "$acs_txpwr_opt" ] && "$device_if" "$phy" acs_tcpwr_opt "${acs_txpwr_opt}"

        config_get set_mu_ppdu_dur "$device" set_mu_ppdu_dur
        [ -n "$set_mu_ppdu_dur" ] && "$device_if" "$phy" set_mu_ppdu_dur "$set_mu_ppdu_dur"

	config_get obss_long_slot "$device" obss_long_slot
	[ -n "$obss_long_slot" ] && "$device_if" "$phy" obss_long_slot "${obss_long_slot}"

	config_get staDFSEn "$device" staDFSEn
	[ -n "$staDFSEn" ] && "$device_if" "$phy" staDFSEn "${staDFSEn}"

        config_get dbdc_enable "$device" dbdc_enable
        [ -n "$dbdc_enable" ] && "$device_if" "$phy" dbdc_enable "${dbdc_enable}"

        config_get client_mcast "$device" client_mcast
        [ -n "$client_mcast" ] && "$device_if" "$phy" client_mcast "${client_mcast}"

        config_get pas_scanen "$device" pas_scanen
        [ -n "$pas_scanen" ] && "$device_if" "$phy" pas_scanen "${pas_scanen}"

        config_get delay_stavapup "$device" delay_stavapup
        [ -n "$delay_stavapup" ] && "$device_if" "$phy" delay_stavapup "${delay_stavapup}"

	config_get cca_threshold "$device" cca_threshold
	[ -n "$cca_threshold" ] && "$device_if" "$phy" cca_threshold "${cca_threshold}"

        config_get tid_override_queue_map "$device" tid_override_queue_map
        [ -n "$tid_override_queue_map" ] && "$device_if" "$phy" queue_map "${tid_override_queue_map}"

        config_get channel_block_mode "$device" channel_block_mode
        [ -n "$channel_block_mode" ] && "$device_if" "$phy" acs_bmode "${channel_block_mode}"

        config_get no_vlan "$device" no_vlan
        [ -n "$no_vlan" ] && "$device_if" "$phy" no_vlan "${no_vlan}"

	config_get ul_hyst "$device" ul_hyst
	[ -n "$ul_hyst" ] && "$device_if" "$phy" ul_hyst "${ul_hyst}"

        config_get discon_time qcawifi discon_time 10
        [ -n "$discon_time" ] && "$device_if" "$phy" discon_time "${discon_time}"

        config_get reconfig_time qcawifi reconfig_time 60
        [ -n "$reconfig_time" ] && "$device_if" "$phy" reconfig_time "${reconfig_time}"

        config_get alwaysprimary qcawifi alwaysprimary
        [ -n "$alwaysprimary" ] && "$device_if" "$phy" alwaysprimary "${alwaysprimary}"

        config_get samessid_disable qcawifi samessid_disable
        [ -n "$samessid_disable" ] && "$device_if" "$phy" samessid_disable "${samessid_disable}"

	config_get nss_wifi_olcfg qcawifi nss_wifi_olcfg
	if [ -z "$nss_wifi_olcfg" ]; then
		if [ -f /lib/wifi/wifi_nss_olcfg ]; then
			nss_wifi_olcfg="$(cat /lib/wifi/wifi_nss_olcfg)"
		fi
	fi

	if [ -n "$nss_wifi_olcfg" ] && [ "$nss_wifi_olcfg" != "0" ]; then
		local mp_256="$(ls /proc/device-tree/ | grep -rw "MP_256")"
		local mp_512="$(ls /proc/device-tree/ | grep -rw "MP_512")"
		config_get hwmode "$device" hwmode auto
		hk_ol_num="$(cat /lib/wifi/wifi_nss_hk_olnum)"
		#For 256 memory profile the range is preset in fw
		if [ "$mp_256" == "MP_256" ]; then
			:
		elif [ "$mp_512" == "MP_512" ]; then
			if [ $hk_ol_num -eq 3 ]; then
				if [ ! -f /tmp/wifi_nss_up_one_radio ]; then
					touch /tmp/wifi_nss_up_one_radio
					sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=39840 >/dev/null 2>/dev/null
					sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=8192 >/dev/null 2>/dev/null
				fi
				case "$hwmode" in
				*axa | *ac)
					"$device_if" "$phy" fc_buf0_max 8192
					"$device_if" "$phy" fc_buf1_max 8192
					"$device_if" "$phy" fc_buf2_max 8192
					"$device_if" "$phy" fc_buf3_max 8192
					;;
				*)
					"$device_if" "$phy" fc_buf0_max 4096
					"$device_if" "$phy" fc_buf1_max 4096
					"$device_if" "$phy" fc_buf2_max 4096
					"$device_if" "$phy" fc_buf3_max 4096
					;;
				esac
			else
				if [ ! -f /tmp/wifi_nss_up_one_radio ]; then
					touch /tmp/wifi_nss_up_one_radio
					sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=30624 >/dev/null 2>/dev/null
					sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=8192 >/dev/null 2>/dev/null
				fi
				case "$hwmode" in
				*axa | *ac)
					"$device_if" "$phy" fc_buf0_max 8192
					"$device_if" "$phy" fc_buf1_max 8192
					"$device_if" "$phy" fc_buf2_max 8192
					"$device_if" "$phy" fc_buf3_max 8192
					;;
				*)
					"$device_if" "$phy" fc_buf0_max 8192
					"$device_if" "$phy" fc_buf1_max 8192
					"$device_if" "$phy" fc_buf2_max 8192
					"$device_if" "$phy" fc_buf3_max 8192
					;;
				esac
			fi
		else
		case "$board_name" in
		ap-ac01)
			;;
		ap-ac02)
			case "$hwmode" in
			*axa | *ac)
				"$device_if" "$phy" fc_buf0_max 4096
				"$device_if" "$phy" fc_buf1_max 8192
				"$device_if" "$phy" fc_buf2_max 8192
				"$device_if" "$phy" fc_buf3_max 8192
				;;
			*)
				"$device_if" "$phy" fc_buf0_max 4096
				"$device_if" "$phy" fc_buf1_max 4096
				"$device_if" "$phy" fc_buf2_max 4096
				"$device_if" "$phy" fc_buf3_max 4096
				;;
			esac
			;;
		ap-hk09*)
			local soc_version_major
			[ -f /sys/firmware/devicetree/base/soc_version_major ] && {
				soc_version_major="$(hexdump -n 1 -e '"%1d"' /sys/firmware/devicetree/base/soc_version_major)"
			}

			if [ $soc_version_major = 2 ];then
				if [ ! -f /tmp/wifi_nss_up_one_radio ]; then
					touch /tmp/wifi_nss_up_one_radio
					#reset the high water mark for NSS if range 0 value > 4096
					sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=67392 >/dev/null 2>/dev/null
					#initially after init 4k buf for 5G and 4k for 2G will be allocated, later range will be configured
					sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=40960 >/dev/null 2>/dev/null
				fi
				case "$hwmode" in
				*axa | *ac)
					"$device_if" "$phy" fc_buf0_max 32768
					"$device_if" "$phy" fc_buf1_max 32768
					"$device_if" "$phy" fc_buf2_max 32768
					"$device_if" "$phy" fc_buf3_max 32768
				;;
				*)
					"$device_if" "$phy" fc_buf0_max 16384
					"$device_if" "$phy" fc_buf1_max 16384
					"$device_if" "$phy" fc_buf2_max 16384
					"$device_if" "$phy" fc_buf3_max 32768
				;;
				esac
			else
				case "$hwmode" in
				*ac)
					#we distinguish the legacy chipset based on the hwcaps
					hwcaps=$(cat /sys/class/net/${phy}/hwcaps)
					if [ "$hwcaps" == "802.11an/ac" ]; then
						"$device_if" "$phy" fc_buf0_max 8192
						"$device_if" "$phy" fc_buf1_max 12288
						"$device_if" "$phy" fc_buf2_max 16384
					else
						"$device_if" "$phy" fc_buf0_max 4096
						"$device_if" "$phy" fc_buf1_max 8192
						"$device_if" "$phy" fc_buf2_max 12288
					fi
					"$device_if" "$phy" fc_buf3_max 16384
					;;
				*)
					"$device_if" "$phy" fc_buf0_max 4096
					"$device_if" "$phy" fc_buf1_max 8192
					"$device_if" "$phy" fc_buf2_max 12288
					"$device_if" "$phy" fc_buf3_max 16384
					;;
				esac
			fi
			;;
		ap-hk* | ap-oak*)
			if [ $hk_ol_num -eq 3 ]; then
				if [ ! -f /tmp/wifi_nss_up_one_radio ]; then
					touch /tmp/wifi_nss_up_one_radio
					sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=88896 >/dev/null 2>/dev/null
					sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=53248 >/dev/null 2>/dev/null
				fi
				case "$hwmode" in
				*axa | *ac)
					"$device_if" "$phy" fc_buf0_max 24576
					"$device_if" "$phy" fc_buf1_max 24576
					"$device_if" "$phy" fc_buf2_max 24576
					"$device_if" "$phy" fc_buf3_max 24576
					;;
				*)
					"$device_if" "$phy" fc_buf0_max 16384
					"$device_if" "$phy" fc_buf1_max 16384
					"$device_if" "$phy" fc_buf2_max 16384
					"$device_if" "$phy" fc_buf3_max 24576
					;;
				esac
			else
				local soc_version_major
				[ -f /sys/firmware/devicetree/base/soc_version_major ] && {
					soc_version_major="$(hexdump -n 1 -e '"%1d"' /sys/firmware/devicetree/base/soc_version_major)"
				}

				if [ ! -f /tmp/wifi_nss_up_one_radio ]; then
					touch /tmp/wifi_nss_up_one_radio
					#reset the high water mark for NSS if range 0 value > 4096
					sysctl -w dev.nss.n2hcfg.n2h_high_water_core0=67392 >/dev/null 2>/dev/null
					#initially after init 4k buf for 5G and 4k for 2G will be allocated, later range will be configured
					sysctl -w dev.nss.n2hcfg.n2h_wifi_pool_buf=40960 >/dev/null 2>/dev/null
				fi
				case "$hwmode" in
				*axa | *ac)
					if [ $soc_version_major = 2 ];then
						"$device_if" "$phy" fc_buf0_max 32768
						"$device_if" "$phy" fc_buf1_max 32768
						"$device_if" "$phy" fc_buf2_max 32768
						"$device_if" "$phy" fc_buf3_max 32768
					else
						"$device_if" "$phy" fc_buf0_max 8192
						"$device_if" "$phy" fc_buf1_max 8192
						"$device_if" "$phy" fc_buf2_max 12288
						"$device_if" "$phy" fc_buf3_max 32768
					fi
				;;
				*)
					if [ $soc_version_major = 2 ];then
						"$device_if" "$phy" fc_buf0_max 16384
						"$device_if" "$phy" fc_buf1_max 16384
						"$device_if" "$phy" fc_buf2_max 16384
						"$device_if" "$phy" fc_buf3_max 32768
					else
						"$device_if" "$phy" fc_buf0_max 4096
						"$device_if" "$phy" fc_buf1_max 8192
						"$device_if" "$phy" fc_buf2_max 12288
						"$device_if" "$phy" fc_buf3_max 16384
					fi
				;;
				esac
			fi
			;;
		*)
			case "$hwmode" in
			*ng)
				"$device_if" "$phy" fc_buf0_max 5120
				"$device_if" "$phy" fc_buf1_max 8192
				"$device_if" "$phy" fc_buf2_max 12288
				"$device_if" "$phy" fc_buf3_max 16384
				;;
			*ac)
				"$device_if" "$phy" fc_buf0_max 8192
				"$device_if" "$phy" fc_buf1_max 16384
				"$device_if" "$phy" fc_buf2_max 24576
				"$device_if" "$phy" fc_buf3_max 32768
				;;
			*)
				"$device_if" "$phy" fc_buf0_max 5120
				"$device_if" "$phy" fc_buf1_max 8192
				"$device_if" "$phy" fc_buf2_max 12288
				"$device_if" "$phy" fc_buf3_max 16384
				;;
			esac
			;;
		esac
		fi
	else
		local mp_512="$(ls /proc/device-tree/ | grep -rw "MP_512")"
		config_get hwmode "$device" hwmode auto
		if [ "$mp_512" == "MP_512" ]; then
			case "$hwmode" in
				*axa | *ac)
					# For 128 clients
					"$device_if" "$phy" fc_buf_max 8192
					;;
				esac
		fi
	fi

	if [ $nss_wifi_olcfg == 0 ]; then
		sysctl -w dev.nss.n2hcfg.n2h_queue_limit_core0=2048 >/dev/null 2>/dev/null
		sysctl -w dev.nss.n2hcfg.n2h_queue_limit_core1=2048 >/dev/null 2>/dev/null
	else
		sysctl -w dev.nss.n2hcfg.n2h_queue_limit_core0=256 >/dev/null 2>/dev/null
		sysctl -w dev.nss.n2hcfg.n2h_queue_limit_core1=256 >/dev/null 2>/dev/null
	fi

	config_tx_fc_buf "$phy"

	# Enable RPS and disable qrfs, if rxchainmask is 15 for some platforms
	disable_qrfs_wifi=0
	enable_rps_wifi=0
	if [ $("$device_if" "$phy" get_rxchainmask | awk -F ':' '{ print $2 }') -gt 3 ]; then
		disable_qrfs_wifi=1
		enable_rps_wifi=1
	fi

	for vif in $vifs; do
		local start_hostapd=
		config_get mode "$vif" mode
		config_get enc "$vif" encryption "none"

		case "$enc" in
			wep*|mixed*|psk*|wpa*|8021x)
				start_hostapd=1
				config_get key "$vif" key
			;;
		esac

		case "$mode" in
			ap|wrap)
				if [ -n "$start_hostapd" ] && [ $count -lt 2 ] && eval "type hostapd_config_multi_cred" 2>/dev/null >/dev/null; then
					hostapd_config_multi_cred "$vif"
					count=$(($count + 1))
				fi
	  			;;
                esac
	done

	echo "number of vifs: $vifs" >/dev/console
	ifconfig "$phy" up

	config_get ODM "$device" ODM
	[ "$ODM" = "dni" ] && _import_dfsnol_status_qcacfg80211 "$device"
	[ "$ODM" = "dni" ] && [ "x$htmode" = "xHT160" ] && _DFS_HT160_workaround_qcacfg80211 "$device"

	for vif in $vifs; do
		local start_hostapd= vif_txpower= nosbeacon= wlanaddr=""
		local wlanmode
		config_get ifname "$vif" ifname
		config_get enc "$vif" encryption "none"
		config_get eap_type "$vif" eap_type
		config_get mode "$vif" mode
		wlanmode=$mode
		pmode=$mode

		if [ -f /sys/class/net/$device/ciphercaps ]
		then
			case "$enc" in
				*gcmp*)
					echo "enc:GCMP" >&2
					cat /sys/class/net/$device/ciphercaps | grep -i "gcmp"
					if [ $? -ne 0 ]
					then
						echo "enc:GCMP is Not Supported on Radio" >&2
						continue
					fi
					;;
				*ccmp-256*)
					echo "enc:CCMP-256" >&2
					cat /sys/class/net/$device/ciphercaps | grep -i "ccmp-256"
					if [ $? -ne 0 ]
					then
						echo "enc:CCMP-256 is Not Supported on Radio" >&2
						continue
					fi
					;;
			esac
		fi

		[ "$wlanmode" = "ap" ] && wlanmode="__ap"
		[ "$wlanmode" = "sta" ] && wlanmode="managed"
		[ "$wlanmode" = "lite_monitor" ] && wlanmode="monitor"
		[ "$wlanmode" = "ap_monitor" ] && wlanmode="__ap"
		[ "$wlanmode" = "ap_smart_monitor" ] && wlanmode="__ap"
		[ "$wlanmode" = "ap_lp_iot" ] && wlanmode="__ap"
		[ "$wlanmode" = "mesh" ] && wlanmode="__ap"
		[ "$wlanmode" = "wrap" ] && wlanmode="__ap"

		[ "$pmode" = "ap_monitor" ] && pmode="specialvap"
		[ "$pmode" = "ap_smart_monitor" ] && pmode="smart_monitor"
		[ "$pmode" = "ap_lp_iot" ] && pmode="lp_iot_mode"

		case "$mode" in
			sta)
				config_get_bool nosbeacon "$device" nosbeacon
				config_get qwrap_enable "$device" qwrap_enable 0
				[ $qwrap_enable -gt 0 ] && wlanaddr="00:00:00:00:00:00"
				;;
			adhoc)
				config_get_bool nosbeacon "$vif" sw_merge 1
				;;
		esac

		[ "$nosbeacon" = 1 ] || nosbeacon=""
		if [ -z "$recover" ] || [ "$recover" -eq "0" ]; then
		    wlanconfig "$ifname" create wlandev "$phy" wlanmode "$pmode" ${wlanaddr:+wlanaddr "$wlanaddr"} ${nosbeacon:+nosbeacon} -cfg80211

		    echo "cfg80211: ifname: $ifname mode: $wlanmode cfgphy: $(cat /sys/class/net/$phy/phy80211/name)" >&2
		    iw phy "$(cat /sys/class/net/$phy/phy80211/name)" interface add $ifname type $wlanmode
		    [ $? -ne 0 ] && {
			echo "enable_qcawifi($device): Failed to set up $mode vif $ifname" >&2
			continue
		    }
		    config_set "$vif" ifname "$ifname"
		fi
		[ $qwrap_enable -gt 0 ] && iw "$ifname" set 4addr on >/dev/null 2>&1

		config_get hwmode "$device" hwmode auto
		pureg=0

		case "$hwmode:$htmode" in
			*ac:HT20) "$device_if" "$ifname" mode 11ACVHT20;;
			*ac:HT40+) "$device_if" "$ifname" mode 11ACVHT40PLUS;;
			*ac:HT40-) "$device_if" "$ifname" mode 11ACVHT40MINUS;;
			*ac:HT40) "$device_if" "$ifname" mode 11ACVHT40;;
			*axg:HT20) "$device_if" "$ifname" mode 11GHE20;;
			*axg:HT40-) "$device_if" "$ifname" mode 11GHE40MINUS;;
			*axg:HT40+) "$device_if" "$ifname" mode 11GHE40PLUS;;
			*axg:HT40) "$device_if" "$ifname" mode 11GHE40;;
			*axg:*) "$device_if" "$ifname" mode 11GHE20;;
			*axa:HT20) "$device_if" "$ifname" mode 11AHE20;;
			*axa:HT40+) "$device_if" "$ifname" mode 11AHE40PLUS;;
			*axa:HT40-) "$device_if" "$ifname" mode 11AHE40MINUS;;
			*axa:HT40) "$device_if" "$ifname" mode 11AHE40;;
			*axa:HT80) "$device_if" "$ifname" mode 11AHE80;;
			*axa:HT160) "$device_if" "$ifname" mode 11AHE160;;
			*axa:HT80_80) "$device_if" "$ifname" mode 11AHE80_80;;
			*axa:*) temphwmode=11AHE80
				if [ -f /sys/class/net/$device/5g_maxchwidth ]; then
					maxchwidth="$(cat /sys/class/net/$device/5g_maxchwidth)"
					[ -n "$maxchwidth" ] && temphwmode=11AHE$maxchwidth
				fi
				if [ "$mode" == "sta" ]; then
					cat /sys/class/net/$device/hwmodes | grep  "11AXA_HE80_80"
					if [ $? -eq 0 ]; then
						temphwmode=11AHE80_80
					fi
				fi;"$device_if" "$ifname" mode "$temphwmode";;

			*bg:*) "$device_if" "$ifname" mode 11G
				pureg=0
			;;
			*b:*) "$device_if" "$ifname" mode 11B
				;;
		esac

		case "$hwmode:$htmode" in
		# The parsing stops at the first match so we need to make sure
		# these are in the right orders (most generic at the end)
			*ng:HT20) hwmode=11NGHT20;;
			*ng:HT40-) hwmode=11NGHT40MINUS;;
			*ng:HT40+) hwmode=11NGHT40PLUS;;
			*ng:HT40) hwmode=11NGHT40;;
			*ng:*) hwmode=11NGHT20;;
			*na:HT20) hwmode=11NAHT20;;
			*na:HT40-) hwmode=11NAHT40MINUS;;
			*na:HT40+) hwmode=11NAHT40PLUS;;
			*na:HT40) hwmode=11NAHT40;;
			*na:*) hwmode=11NAHT40;;
			*ac:HT20) hwmode=11ACVHT20;;
			*ac:HT40+) hwmode=11ACVHT40PLUS;;
			*ac:HT40-) hwmode=11ACVHT40MINUS;;
			*ac:HT40) hwmode=11ACVHT40;;
			*ac:HT80) hwmode=11ACVHT80;;
			*ac:HT160) hwmode=11ACVHT160;;
			*ac:HT80_80) hwmode=11ACVHT80_80;;
                        *ac:*) hwmode=11ACVHT80
			       if [ -f /sys/class/net/$device/5g_maxchwidth ]; then
			           maxchwidth="$(cat /sys/class/net/$device/5g_maxchwidth)"
				   [ -n "$maxchwidth" ] && hwmode=11ACVHT$maxchwidth
			       fi
                               if [ "$mode" == "sta" ]; then
                                   cat /sys/class/net/$device/hwmodes | grep  "11AC_VHT80_80"
				   if [ $? -eq 0 ]; then
			               hwmode=11ACVHT80_80
				   fi
			       fi;;
			*axg:HT20) hwmode=11GHE20;;
			*axg:HT40-) hwmode=11GHE40MINUS;;
			*axg:HT40+) hwmode=11GHE40PLUS;;
			*axg:HT40) hwmode=11GHE40;;
			*axg:*) hwmode=11GHE20;;
			*axa:HT20) hwmode=11AHE20;;
			*axa:HT40+) hwmode=11AHE40PLUS;;
			*axa:HT40-) hwmode=11AHE40MINUS;;
			*axa:HT40) hwmode=11AHE40;;
			*axa:HT80) hwmode=11AHE80;;
			*axa:HT160) hwmode=11AHE160;;
			*axa:HT80_80) hwmode=11AHE80_80;;
			*axa:*) hwmode=11AHE80
				if [ -f /sys/class/net/$device/5g_maxchwidth ]; then
					maxchwidth="$(cat /sys/class/net/$device/5g_maxchwidth)"
					[ -n "$maxchwidth" ] && hwmode=11AHE$maxchwidth
				fi
				if [ "$mode" == "sta" ]; then
					cat /sys/class/net/$device/hwmodes | grep  "11AXA_HE80_80"
					if [ $? -eq 0 ]; then
						hwmode=11AHE80_80
					fi
				fi;;
			*b:*) hwmode=11B;;
			*bg:*) hwmode=11G;;
			*g:*) hwmode=11G; pureg=1;;
			*a:*) hwmode=11A;;
			*) hwmode=auto;;
		esac

		if [ "$pmode" = "specialvap" ] || [ "$pmode" = "smart_monitor" ] || [ "$pmode" = "monitor" ]|| [ "$pmode" = "lite_monitor" ] || [ "$pmode" = "lp_iot_mode" ]; then
			echo "HWMODE: $hwmode" > /dev/console
			"$device_if" "$ifname" mode "$hwmode"
			"$device_if" "$ifname" channel "$channel"

			[ $pureg -gt 0 ] && "$device_if" "$ifname" pureg "$pureg"

			config_get ssid "$vif" ssid
			[ -n "$ssid" ] && {
				cfg80211tool "$ifname" ssid "$ssid"
			}
		fi #end of propritery modes

		[ "sta" = "$mode" ] && "$device_if" "$ifname" mode "$hwmode"
		[ 0 = "$channel" ] && "$device_if" "$ifname" mode "$hwmode"
		[ "$htmode" = "HT80_80" ] && "$device_if" "$ifname" mode "$hwmode"

		config_get_bool map "$vif" map 0
		[ $map -gt 0 ] && "$device_if" "$ifname" map "$map"

		config_get MapBSSType "$vif" MapBSSType
		[ $MapBSSType -gt 0 ] && "$device_if" "$ifname" MapBSSType "$MapBSSType"

		config_get cfreq2 "$vif" cfreq2
		[ -n "$cfreq2" -a "$htmode" = "HT80_80" ] && "$device_if" "$ifname" cfreq2 "$cfreq2"

		#set channel; store in cfg80211 vap structures, use them while setting channels
		"$device_if" "$ifname" channel "$channel"

		[ $pureg -gt 0 ] && cfg80211tool "$ifname" pureg "$pureg" #set pureg

		config_get puren "$vif" puren
		[ -n "$puren" ] && "$device_if" "$ifname" puren "$puren"

		band_type=`grep "^[ga].*_device" /etc/ath/wifi.conf | grep $phy | cut -c 1`
		if [ -f /tmp/radardetect.pid ] && [ "$band_type" == "a" ]; then
			/usr/sbin/radardetect_cli -b "$phy"
		fi

		config_get_bool hidden "$vif" hidden 0
		"$device_if" "$ifname" hide_ssid "$hidden"

                config_get_bool dynamicbeacon "$vif" dynamicbeacon 0
                [ $hidden = 1 ] && "$device_if" "$ifname" dynamicbeacon "$dynamicbeacon"

                config_get db_rssi_thr "$vif" db_rssi_thr
                [ -n "$db_rssi_thr" ] && "$device_if" "$ifname" db_rssi_thr "$db_rssi_thr"

                config_get db_timeout "$vif" db_timeout
                [ -n "$db_timeout" ] && "$device_if" "$ifname" db_timeout "$db_timeout"

                config_get nrshareflag "$vif" nrshareflag
                [ -n "$nrshareflag" ] && "$device_if" "$ifname" nrshareflag "$nrshareflag"

		config_get shortgi "$vif" shortgi
		[ -n "$shortgi" ] && "$device_if" "$ifname" shortgi "${shortgi}"

		config_get_bool disablecoext "$vif" disablecoext
		[ -n "$disablecoext" ] && "$device_if" "$ifname" disablecoext "${disablecoext}"
		[ "$disablecoext" -eq "0" ] && "$device_if" $ifname extbusythres 30
		[ "$disablecoext" -eq "1" ] && "$device_if" $ifname extbusythres 100

		config_get chwidth "$vif" chwidth
		[ -n "$chwidth" ] && "$device_if" "$ifname" chwidth "${chwidth}"

		config_get wds "$vif" wds
		case "$wds" in
			1|on|enabled)	wds=1
				iw "$ifname" set 4addr on >/dev/null 2>&1
				;;
			*)	wds=0
				;;
		esac
		"$device_if" "$ifname" wds "$wds"

		config_get bridge "$vif" bridge
		hyd_enabled=$(uci -q get hyd.config.Enable)
		[ "$ODM" = "dni" -a "x$hyd_enabled" != "x1" ] && [ "$wds" -eq 1 ] && brctl stp "$bridge" on

		config_get vlan_pri "$vif" vlan_pri
		if [ "$vlan_pri" = "" ]; then
			"$device_if" "$ifname" dni_vlan_pri -1
		else
			"$device_if" "$ifname" dni_vlan_pri "$vlan_pri"
		fi

		config_get ext_nss "$device" ext_nss
		case "$ext_nss" in
			1|on|enabled) "$device_if" "$phy" ext_nss 1 >/dev/null 2>&1
				;;
			0|on|enabled) "$device_if" "$phy" ext_nss 0 >/dev/null 2>&1
				;;
			*) ;;
		esac

		config_get ext_nss_sup "$vif" ext_nss_sup
		case "$ext_nss_sup" in
			1|on|enabled) "$device_if" "$ifname" ext_nss_sup 1 >/dev/null 2>&1
				;;
			0|on|enabled) "$device_if" "$ifname" ext_nss_sup 0 >/dev/null 2>&1
				;;
			*) ;;
		esac

		config_get  backhaul "$vif" backhaul 0
                "$device_if" "$ifname" backhaul "$backhaul" >/dev/null 2>&1

		config_get TxBFCTL "$vif" TxBFCTL
		[ -n "$TxBFCTL" ] && "$device_if" "$ifname" TxBFCTL "$TxBFCTL"

		config_get bintval "$vif" bintval
		[ -n "$bintval" ] && "$device_if" "$ifname" bintval "$bintval"

		config_get_bool countryie "$vif" countryie
		[ -n "$countryie" ] && "$device_if" "$ifname" countryie "$countryie"

		config_get ppdu_duration "$device" ppdu_duration
		[ -n "$ppdu_duration" ] && "$device_if" "$phy" ppdu_duration "${ppdu_duration}"

		config_get he_ul_ppdu_dur "$device" he_ul_ppdu_dur
		[ -n "$he_ul_ppdu_dur" ] && "$device_if" "$phy" he_ul_ppdu_dur "${he_ul_ppdu_dur}"

		config_get own_ie_override "$vif" own_ie_override
                [ -n "$own_ie_override" ] && cfg80211tool "$ifname" rsn_override 1

		config_get_bool sae "$vif" sae
		config_get_bool owe "$vif" owe
		config_get_bool dpp "$vif" dpp
		config_get pkex_code "$vif" pkex_code
		config_get suite_b "$vif" suite_b 0

		if [ $suite_b -eq 192 ]
		then
			cat /sys/class/net/$device/ciphercaps | grep -i "gcmp-256"
			if [ $? -ne 0 ]
			then
				echo "enc:GCMP-256 is Not Supported on Radio" > /dev/console
				destroy_vap $ifname
				continue
			fi
		elif [ $suite_b -ne 0 ]
		then
			echo "$suite_b bit security level is not supported for SUITE-B" > /dev/console
			destroy_vap $ifname
			continue
		fi

		if [ "${dpp}" -eq 1 ]
		then
			cfg80211tool "$ifname" set_dpp_mode 1
			config_get dpp_type "$vif" dpp_type "qrcode"
			if [ "$dpp_type" != "qrcode" -a "$dpp_type" != "pkex" ]
			then
				echo "Invalid DPP type" > /dev/console
				destroy_vap $ifname
				continue
			elif [ "$dpp_type" == "pkex" ]
			then
				if [ -z "$pkex_code" ]
				then
					echo "pkex_code should not be NULL" > /dev/console
					destroy_vap $ifname
				fi
			fi
		fi

		case "$enc" in
			none)
				# We start hostapd in open mode also
				start_hostapd=1
			;;
			wpa*|8021x)
				start_hostapd=1
			;;
			mixed*|wep*|psk*)
				start_hostapd=1
				config_get key "$vif" key

				if [ -z "$key" ]
				then
					echo "Key is NULL" > /dev/console
					destroy_vap $ifname
					continue
				fi
				case "$enc" in
					*tkip*|wep*)
						if [ $sae -eq 1 ] || [ $owe -eq 1 ]
						then
							echo "With SAE/OWE enabled, tkip/wep enc is not supported" > /dev/console
							destroy_vap $ifname
							continue
						fi
					;;
				esac
			;;
			tkip*)
				if [ $sae -eq 1 ] || [ $owe -eq 1 ]
				then
					echo "With SAE/OWE enabled, tkip enc is not supported" > /dev/console
					destroy_vap $ifname
					continue
				fi
			;;
			wapi*)
				start_wapid=1
				config_get key "$vif" key
			;;
			#Needed ccmp*|gcmp* check for SAE OWE auth types
			ccmp*|gcmp*)
				flag=0
				start_hostapd=1
				config_get key "$vif" key
				config_get sae_password "$vif" sae_password
				if [ $sae -eq 1 ]
				then
					if [ -z "$sae_password" ] && [ -z "$key" ]
					then
						echo "key/sae_password are NULL" > /dev/console
						destroy_vap $ifname
						continue
					fi
				fi
				if [ $owe -eq 1 ]
				then
					if [ "$mode" = "ap" ]
					then
						check_owe_groups() {
							local owe_groups=$(echo $1 | tr "," " ")
							for owe_group_value in $owe_groups
							do
								if [ $owe_group_value -ne 19 ] && [ $owe_group_value -ne 20 ] && [ $owe_group_value -ne 21 ]
								then
									echo "Invalid owe_group: $owe_group_value" > /dev/console
									destroy_vap $ifname
									flag=1
									break
								fi
							done
						}
						config_list_foreach "$vif" owe_groups check_owe_groups
					elif [ "$mode" = "sta" ]
                                        then
                                                config_get owe_group "$vif" owe_group
                                                if [ $owe_group && ${#owe_group} -ne 2 ]
                                                then
                                                        echo "Invalid owe_group: $owe_group" > /dev/console
                                                        destroy_vap $ifname
                                                        flag=1
                                                        break;
                                                fi
                                        fi

					if [ $flag -eq 1 ]
					then
						continue
					fi
				fi
			;;
			sae*|dpp|psk2)
				start_hostapd=1
			;;
		esac

		case "$mode" in
			sta|adhoc)
				config_get addr "$vif" bssid
				[ -z "$addr" ] || {
#TODO
					iwconfig "$ifname" ap "$addr"
				}
			;;
		esac

		config_get_bool uapsd "$vif" uapsd 1
		"$device_if" "$ifname" uapsd "$uapsd"

		config_get powersave "$vif" powersave
		[ -n "$powersave" ] && "$device_if" "$ifname" powersave "${powersave}"

		config_get_bool ant_ps_on "$vif" ant_ps_on
		[ -n "$ant_ps_on" ] && "$device_if" "$ifname" ant_ps_on "${ant_ps_on}"

		config_get ps_timeout "$vif" ps_timeout
		[ -n "$ps_timeout" ] && "$device_if" "$ifname" ps_timeout "${ps_timeout}"

		config_get mcastenhance "$vif" mcastenhance
		[ -n "$mcastenhance" ] && "$device_if" "$ifname" mcastenhance "${mcastenhance}"

		config_get disable11nmcs "$vif" disable11nmcs
		[ -n "$disable11nmcs" ] && "$device_if" "$ifname" disable11nmcs "${disable11nmcs}"

		config_get conf_11acmcs "$vif" conf_11acmcs
		[ -n "$conf_11acmcs" ] && "$device_if" "$ifname" conf_11acmcs "${conf_11acmcs}"

		config_get metimer "$vif" metimer
		[ -n "$metimer" ] && "$device_if" "$ifname" metimer "${metimer}"

		config_get metimeout "$vif" metimeout
		[ -n "$metimeout" ] && "$device_if" "$ifname" metimeout "${metimeout}"

		config_get_bool medropmcast "$vif" medropmcast
		[ -n "$medropmcast" ] && "$device_if" "$ifname" medropmcast "${medropmcast}"

		config_get me_adddeny "$vif" me_adddeny
		[ -n "$me_adddeny" ] && "$device_if" "$ifname" me_adddeny ${me_adddeny}

		#support independent repeater mode
		config_get vap_ind "$vif" vap_ind
		[ -n "$vap_ind" ] && "$device_if" "$ifname" vap_ind "${vap_ind}"

		#support extender ap & STA
		config_get extap "$vif" extap
		[ -n "$extap" ] && "$device_if" "$ifname" extap "${extap}"
		[ -n "$extap" ] && iw "$ifname" set 4addr on >/dev/null 2>&1

		config_get scanband "$vif" scanband
		[ -n "$scanband" ] && "$device_if" "$ifname" scanband "${scanband}"

		config_get periodicScan "$vif" periodicScan
		[ -n "$periodicScan" ] && "$device_if" "$ifname" periodicScan "${periodicScan}"

		config_get cb "$vif" cb
		if [ "$cb" = "qca" ]; then
			"$device_if" "$ifname" extap 1
			"$device_if" "$ifname" scanband 1
			"$device_if" "$ifname" periodicScan 180000
		elif [ "$cb" = "dni" ]; then
			"$device_if" "$ifname" dni_cb 1
			"$device_if" "$ifname" periodicScan 180000
		fi

		config_get_bool short_preamble "$vif" short_preamble
		[ -n "$short_preamble" ] && "$device_if" "$ifname" shpreamble "${short_preamble}"

		config_get cwmin "$vif" cwmin
		[ -n "$cwmin" ] && "$device_if" "$ifname" cwmin ${cwmin}

		config_get cwmax "$vif" cwmax
		[ -n "$cwmax" ] && "$device_if" "$ifname" cwmax ${cwmax}

		config_get aifs "$vif" aifs
		[ -n "$aifs" ] && "$device_if" "$ifname" aifs ${aifs}

		config_get txoplimit "$vif" txoplimit
		[ -n "$txoplimit" ] && "$device_if" "$ifname" txoplimit ${txoplimit}

		config_get noackpolicy "$vif" noackpolicy
		[ -n "$noackpolicy" ] && "$device_if" "$ifname" noackpolicy ${noackpolicy}

		config_get_bool wmm "$vif" wmm
		[ -n "$wmm" ] && "$device_if" "$ifname" wmm "$wmm"

		band_type=`grep "^[ga].*_device" /etc/ath/wifi.conf | grep $phy | cut -c 1`
		if [ "$band_type" = "a" ]; then
			config_get_bool no_wradar "$vif" no_wradar 1
			[ -n "$no_wradar" ] && "$device_if" "$ifname" no_wradar "$no_wradar"
		fi

		config_get_bool doth "$vif" doth
		[ -n "$doth" ] && "$device_if" "$ifname" doth "$doth"

		config_get doth_chanswitch "$vif" doth_chanswitch
		[ -n "$doth_chanswitch" ] && "$device_if" "$ifname" doth_chanswitch ${doth_chanswitch}

		config_get quiet "$vif" quiet
		[ -n "$quiet" ] && "$device_if" "$ifname" quiet "$quiet"

		config_get mfptest "$vif" mfptest
		[ -n "$mfptest" ] && "$device_if" "$ifname" mfptest "$mfptest"

		config_get dtim_period "$vif" dtim_period
		[ -n "$dtim_period" ] && "$device_if" "$ifname" dtim_period "$dtim_period"

		config_get noedgech "$vif" noedgech
		[ -n "$noedgech" ] && "$device_if" "$ifname" noedgech "$noedgech"

		config_get ps_on_time "$vif" ps_on_time
		[ -n "$ps_on_time" ] && "$device_if" "$ifname" ps_on_time "$ps_on_time"

		config_get inact "$vif" inact
		[ -n "$inact" ] && "$device_if" "$ifname" inact "$inact"

		config_get wnm "$vif" wnm
		[ -n "$wnm" ] && "$device_if" "$ifname" wnm "$wnm"

		config_get ampdu "$vif" ampdu
		[ -n "$ampdu" ] && "$device_if" "$ifname" ampdu "$ampdu"

		config_get amsdu "$vif" amsdu
		[ -n "$amsdu" ] && "$device_if" "$ifname" amsdu "$amsdu"

		config_get maxampdu "$vif" maxampdu
		[ -n "$maxampdu" ] && "$device_if" "$ifname" maxampdu "$maxampdu"

		config_get vhtmaxampdu "$vif" vhtmaxampdu
		[ -n "$vhtmaxampdu" ] && "$device_if" "$ifname" vhtmaxampdu "$vhtmaxampdu"

		config_get setaddbaoper "$vif" setaddbaoper
		[ -n "$setaddbaoper" ] && "$device_if" "$ifname" setaddbaoper "$setaddbaoper"

		config_get addbaresp "$vif" addbaresp
		[ -n "$addbaresp" ] && "$device_if" "$ifname" $addbaresp

		config_get addba "$vif" addba
		[ -n "$addba" ] && "$device_if" "$ifname" addba $addba

		config_get delba "$vif" delba
		[ -n "$delba" ] && "$device_if" "$ifname" delba $delba

		config_get_bool stafwd "$vif" stafwd 0
		[ -n "$stafwd" ] && "$device_if" "$ifname" stafwd "$stafwd"

		config_get maclist "$vif" maclist
		[ -n "$maclist" ] && {
			# flush MAC list
			"$device_if" "$ifname" maccmd 3
			for mac in $maclist; do
				"$device_if" "$ifname" addmac "$mac"
			done
		}

		config_get macfilter "$vif" macfilter
		case "$macfilter" in
			allow)
				"$device_if" "$ifname" maccmd 1
			;;
			deny)
				"$device_if" "$ifname" maccmd 2
			;;
			*)
				# default deny policy if mac list exists
				[ -n "$maclist" ] && "$device_if" "$ifname" maccmd 2
			;;
		esac

		config_get nss "$vif" nss
		[ -n "$nss" ] && "$device_if" "$ifname" nss "$nss"

		config_get vht_mcsmap "$vif" vht_mcsmap
		[ -n "$vht_mcsmap" ] && "$device_if" "$ifname" vht_mcsmap "$vht_mcsmap"

		config_get he_mcs "$vif" he_mcs
		[ -n "$he_mcs" ] && "$device_if" "$ifname" he_mcs "$he_mcs"

		config_get chwidth "$vif" chwidth
		[ -n "$chwidth" ] && "$device_if" "$ifname" chwidth "$chwidth"

		config_get chbwmode "$vif" chbwmode
		[ -n "$chbwmode" ] && "$device_if" "$ifname" chbwmode "$chbwmode"

		config_get ldpc "$vif" ldpc
		[ -n "$ldpc" ] && "$device_if" "$ifname" ldpc "$ldpc"

		config_get rx_stbc "$vif" rx_stbc
		[ -n "$rx_stbc" ] && "$device_if" "$ifname" rx_stbc "$rx_stbc"

		config_get tx_stbc "$vif" tx_stbc
		[ -n "$tx_stbc" ] && "$device_if" "$ifname" tx_stbc "$tx_stbc"

		config_get cca_thresh "$vif" cca_thresh
		[ -n "$cca_thresh" ] && "$device_if" "$ifname" cca_thresh "$cca_thresh"

		config_get set11NRetries "$vif" set11NRetries
		[ -n "$set11NRetries" ] && "$device_if" "$ifname" set11NRetries "$set11NRetries"

		config_get chanbw "$vif" chanbw
		[ -n "$chanbw" ] && "$device_if" "$ifname" chanbw "$chanbw"

		config_get maxsta "$vif" maxsta
		[ -n "$maxsta" ] && "$device_if" "$ifname" maxsta "$maxsta"

		config_get sko_max_xretries "$vif" sko_max_xretries
		[ -n "$sko_max_xretries" ] && "$device_if" "$ifname" sko "$sko_max_xretries"

		config_get extprotmode "$vif" extprotmode
		[ -n "$extprotmode" ] && "$device_if" "$ifname" extprotmode "$extprotmode"

		config_get extprotspac "$vif" extprotspac
		[ -n "$extprotspac" ] && "$device_if" "$ifname" extprotspac "$extprotspac"

		config_get_bool cwmenable "$vif" cwmenable
		[ -n "$cwmenable" ] && "$device_if" "$ifname" cwmenable "$cwmenable"

		config_get_bool protmode "$vif" protmode
		[ -n "$protmode" ] && "$device_if" "$ifname" protmode "$protmode"

		config_get enablertscts "$vif" enablertscts
		[ -n "$enablertscts" ] && "$device_if" "$ifname" enablertscts "$enablertscts"

		config_get txcorrection "$vif" txcorrection
		[ -n "$txcorrection" ] && "$device_if" "$ifname" txcorrection "$txcorrection"

		config_get rxcorrection "$vif" rxcorrection
		[ -n "$rxcorrection" ] && "$device_if" "$ifname" rxcorrection "$rxcorrection"

		config_get vsp_enable "$vif" vsp_enable
		[ -n "$vsp_enable" ] && "$device_if" "$ifname" vsp_enable "$vsp_enable"

                config_get qdf_cv_lvl "$vif" qdf_cv_lvl
                [ -n "$qdf_cv_lvl" ] && "$device_if" "$ifname" qdf_cv_lvl "$qdf_cv_lvl"

		config_get mode "$vif" mode
		if [ $mode = "sta" ]; then
			config_get ssid "$vif" ssid
				[ -n "$ssid" ] && {
					cfg80211tool "$ifname" ssid  "$ssid"
				}
		fi

		config_get txqueuelen "$vif" txqueuelen
		[ -n "$txqueuelen" ] && ifconfig "$ifname" txqueuelen "$txqueuelen"

                net_cfg="$(find_net_config "$vif")"

                config_get mtu $net_cfg mtu

                [ -n "$mtu" ] && {
                        config_set "$vif" mtu $mtu
                        ifconfig "$ifname" mtu $mtu
		}

		config_get tdls "$vif" tdls
		[ -n "$tdls" ] && "$device_if" "$ifname" tdls "$tdls"

		config_get set_tdls_rmac "$vif" set_tdls_rmac
		[ -n "$set_tdls_rmac" ] && "$device_if" "$ifname" set_tdls_rmac "$set_tdls_rmac"

		config_get tdls_qosnull "$vif" tdls_qosnull
		[ -n "$tdls_qosnull" ] && "$device_if" "$ifname" tdls_qosnull "$tdls_qosnull"

		config_get tdls_uapsd "$vif" tdls_uapsd
		[ -n "$tdls_uapsd" ] && "$device_if" "$ifname" tdls_uapsd "$tdls_uapsd"

		config_get tdls_set_rcpi "$vif" tdls_set_rcpi
		[ -n "$tdls_set_rcpi" ] && "$device_if" "$ifname" set_rcpi "$tdls_set_rcpi"

		config_get tdls_set_rcpi_hi "$vif" tdls_set_rcpi_hi
		[ -n "$tdls_set_rcpi_hi" ] && "$device_if" "$ifname" set_rcpihi "$tdls_set_rcpi_hi"

		config_get tdls_set_rcpi_lo "$vif" tdls_set_rcpi_lo
		[ -n "$tdls_set_rcpi_lo" ] && "$device_if" "$ifname" set_rcpilo "$tdls_set_rcpi_lo"

		config_get tdls_set_rcpi_margin "$vif" tdls_set_rcpi_margin
		[ -n "$tdls_set_rcpi_margin" ] && "$device_if" "$ifname" set_rcpimargin "$tdls_set_rcpi_margin"

		config_get tdls_dtoken "$vif" tdls_dtoken
		[ -n "$tdls_dtoken" ] && "$device_if" "$ifname" tdls_dtoken "$tdls_dtoken"

		config_get do_tdls_dc_req "$vif" do_tdls_dc_req
		[ -n "$do_tdls_dc_req" ] && "$device_if" "$ifname" do_tdls_dc_req "$do_tdls_dc_req"

		config_get tdls_auto "$vif" tdls_auto
		[ -n "$tdls_auto" ] && "$device_if" "$ifname" tdls_auto "$tdls_auto"

		config_get tdls_off_timeout "$vif" tdls_off_timeout
		[ -n "$tdls_off_timeout" ] && "$device_if" "$ifname" off_timeout "$tdls_off_timeout"

		config_get tdls_tdb_timeout "$vif" tdls_tdb_timeout
		[ -n "$tdls_tdb_timeout" ] && "$device_if" "$ifname" tdb_timeout "$tdls_tdb_timeout"

		config_get tdls_weak_timeout "$vif" tdls_weak_timeout
		[ -n "$tdls_weak_timeout" ] && "$device_if" "$ifname" weak_timeout "$tdls_weak_timeout"

		config_get tdls_margin "$vif" tdls_margin
		[ -n "$tdls_margin" ] && "$device_if" "$ifname" tdls_margin "$tdls_margin"

		config_get tdls_rssi_ub "$vif" tdls_rssi_ub
		[ -n "$tdls_rssi_ub" ] && "$device_if" "$ifname" tdls_rssi_ub "$tdls_rssi_ub"

		config_get tdls_rssi_lb "$vif" tdls_rssi_lb
		[ -n "$tdls_rssi_lb" ] && "$device_if" "$ifname" tdls_rssi_lb "$tdls_rssi_lb"

		config_get tdls_path_sel "$vif" tdls_path_sel
		[ -n "$tdls_path_sel" ] && "$device_if" "$ifname" tdls_pathSel "$tdls_path_sel"

		config_get tdls_rssi_offset "$vif" tdls_rssi_offset
		[ -n "$tdls_rssi_offset" ] && "$device_if" "$ifname" tdls_rssi_o "$tdls_rssi_offset"

		config_get tdls_path_sel_period "$vif" tdls_path_sel_period
		[ -n "$tdls_path_sel_period" ] && "$device_if" "$ifname" tdls_pathSel_p "$tdls_path_sel_period"

		config_get tdlsmacaddr1 "$vif" tdlsmacaddr1
		[ -n "$tdlsmacaddr1" ] && "$device_if" "$ifname" tdlsmacaddr1 "$tdlsmacaddr1"

		config_get tdlsmacaddr2 "$vif" tdlsmacaddr2
		[ -n "$tdlsmacaddr2" ] && "$device_if" "$ifname" tdlsmacaddr2 "$tdlsmacaddr2"

		config_get tdlsaction "$vif" tdlsaction
		[ -n "$tdlsaction" ] && "$device_if" "$ifname" tdlsaction "$tdlsaction"

		config_get tdlsoffchan "$vif" tdlsoffchan
		[ -n "$tdlsoffchan" ] && "$device_if" "$ifname" tdlsoffchan "$tdlsoffchan"

		config_get tdlsswitchtime "$vif" tdlsswitchtime
		[ -n "$tdlsswitchtime" ] && "$device_if" "$ifname" tdlsswitchtime "$tdlsswitchtime"

		config_get tdlstimeout "$vif" tdlstimeout
		[ -n "$tdlstimeout" ] && "$device_if" "$ifname" tdlstimeout "$tdlstimeout"

		config_get tdlsecchnoffst "$vif" tdlsecchnoffst
		[ -n "$tdlsecchnoffst" ] && "$device_if" "$ifname" tdlsecchnoffst "$tdlsecchnoffst"

		config_get tdlsoffchnmode "$vif" tdlsoffchnmode
		[ -n "$tdlsoffchnmode" ] && "$device_if" "$ifname" tdlsoffchnmode "$tdlsoffchnmode"

		config_get_bool blockdfschan "$vif" blockdfschan
		[ -n "$blockdfschan" ] && "$device_if" "$ifname" blockdfschan "$blockdfschan"

		config_get dbgLVL "$vif" dbgLVL
		[ -n "$dbgLVL" ] && "$device_if" "$ifname" dbgLVL "$dbgLVL"

		config_get dbgLVL_high "$vif" dbgLVL_high
		[ -n "$dbgLVL_high" ] && "$device_if" "$ifname" dbgLVL_high "$dbgLVL_high"

		config_get acsmindwell "$vif" acsmindwell
		[ -n "$acsmindwell" ] && "$device_if" "$ifname" acsmindwell "$acsmindwell"

		config_get acsmaxdwell "$vif" acsmaxdwell
		[ -n "$acsmaxdwell" ] && "$device_if" "$ifname" acsmaxdwell "$acsmaxdwell"

		config_get acsreport "$vif" acsreport
		[ -n "$acsreport" ] && "$device_if" "$ifname" acsreport "$acsreport"

		config_get ch_hop_en "$vif" ch_hop_en
		[ -n "$ch_hop_en" ] && "$device_if" "$ifname" ch_hop_en "$ch_hop_en"

		config_get ch_long_dur "$vif" ch_long_dur
		[ -n "$ch_long_dur" ] && "$device_if" "$ifname" ch_long_dur "$ch_long_dur"

		config_get ch_nhop_dur "$vif" ch_nhop_dur
		[ -n "$ch_nhop_dur" ] && "$device_if" "$ifname" ch_nhop_dur "$ch_nhop_dur"

		config_get ch_cntwn_dur "$vif" ch_cntwn_dur
		[ -n "$ch_cntwn_dur" ] && "$device_if" "$ifname" ch_cntwn_dur "$ch_cntwn_dur"

		config_get ch_noise_th "$vif" ch_noise_th
		[ -n "$ch_noise_th" ] && "$device_if" "$ifname" ch_noise_th "$ch_noise_th"

		config_get ch_cnt_th "$vif" ch_cnt_th
		[ -n "$ch_cnt_th" ] && "$device_if" "$ifname" ch_cnt_th "$ch_cnt_th"

		config_get_bool scanchevent "$vif" scanchevent
		[ -n "$scanchevent" ] && "$device_if" "$ifname" scanchevent "$scanchevent"

		config_get_bool send_add_ies "$vif" send_add_ies
		[ -n "$send_add_ies" ] && "$device_if" "$ifname" send_add_ies "$send_add_ies"

		config_get_bool ext_ifu_acs "$vif" ext_ifu_acs
		[ -n "$ext_ifu_acs" ] && "$device_if" "$ifname" ext_ifu_acs "$ext_ifu_acs"

		config_get_bool enable_rtt "$vif" enable_rtt
		[ -n "$enable_rtt" ] && "$device_if" "$ifname" enable_rtt "$enable_rtt"

		config_get_bool enable_lci "$vif" enable_lci
		[ -n "$enable_lci" ] && "$device_if" "$ifname" enable_lci "$enable_lci"

		config_get_bool enable_lcr "$vif" enable_lcr
		[ -n "$enable_lcr" ] && "$device_if" "$ifname" enable_lcr "$enable_lcr"

		config_get_bool rrm "$vif" rrm
		[ -n "$rrm" ] && "$device_if" "$ifname" rrm "$rrm"

		config_get_bool rrmslwin "$vif" rrmslwin
		[ -n "$rrmslwin" ] && "$device_if" "$ifname" rrmslwin "$rrmslwin"

		config_get_bool rrmstats "$vif" rrmsstats
		[ -n "$rrmstats" ] && "$device_if" "$ifname" rrmstats "$rrmstats"

		config_get rrmdbg "$vif" rrmdbg
		[ -n "$rrmdbg" ] && "$device_if" "$ifname" rrmdbg "$rrmdbg"

		config_get acparams "$vif" acparams
		[ -n "$acparams" ] && "$device_if" "$ifname" acparams $acparams

		config_get setwmmparams "$vif" setwmmparams
		[ -n "$setwmmparams" ] && "$device_if" "$ifname" setwmmparams $setwmmparams

		config_get_bool qbssload "$vif" qbssload
		[ -n "$qbssload" ] && "$device_if" "$ifname" qbssload "$qbssload"

		config_get_bool proxyarp "$vif" proxyarp
		[ -n "$proxyarp" ] && "$device_if" "$ifname" proxyarp "$proxyarp"

		config_get_bool dgaf_disable "$vif" dgaf_disable
		[ -n "$dgaf_disable" ] && "$device_if" "$ifname" dgaf_disable "$dgaf_disable"

		config_get setibssdfsparam "$vif" setibssdfsparam
		[ -n "$setibssdfsparam" ] && "$device_if" "$ifname" setibssdfsparam "$setibssdfsparam"

		config_get startibssrssimon "$vif" startibssrssimon
		[ -n "$startibssrssimon" ] && "$device_if" "$ifname" startibssrssimon "$startibssrssimon"

		config_get setibssrssihyst "$vif" setibssrssihyst
		[ -n "$setibssrssihyst" ] && "$device_if" "$ifname" setibssrssihyst "$setibssrssihyst"

		config_get noIBSSCreate "$vif" noIBSSCreate
		[ -n "$noIBSSCreate" ] && "$device_if" "$ifname" noIBSSCreate "$noIBSSCreate"

		config_get setibssrssiclass "$vif" setibssrssiclass
		[ -n "$setibssrssiclass" ] && "$device_if" "$ifname" setibssrssiclass $setibssrssiclass

		config_get offchan_tx_test "$vif" offchan_tx_test
		[ -n "$offchan_tx_test" ] && "$device_if" "$ifname" offchan_tx_test $offchan_tx_test

		handle_vow_dbg_cfg() {
			local value="$1"
			[ -n "$value" ] && "$device_if" "$ifname" vow_dbg_cfg $value
		}

		config_list_foreach "$vif" vow_dbg_cfg handle_vow_dbg_cfg

		config_get_bool vow_dbg "$vif" vow_dbg
		[ -n "$vow_dbg" ] && "$device_if" "$ifname" vow_dbg "$vow_dbg"
#TODO
		handle_set_max_rate() {
			local value="$1"
			[ -n "$value" ] && wlanconfig "$ifname" set_max_rate $value -cfg80211
		}
		config_list_foreach "$vif" set_max_rate handle_set_max_rate

		# "implicitbf" is set in "wifi-device" section
		config_get_bool implicitbf "$device" implicitbf
		[ -n "$implicitbf" ] && "$device_if" "$ifname" implicitbf "${implicitbf}"

		#
		# "implicitbf" is set in "wifi-iface" section. If set, This will
		# overwrite "implicitbf" in "wifi-device" section.
		#
		config_get_bool implicitbf "$vif" implicitbf
		[ -n "$implicitbf" ] && "$device_if" "$ifname" implicitbf "${implicitbf}"

		# SU TxBF (Single User Tx-Beamforming)
		config_get_bool bf "$device" bf

		# MU-MIMO (Multi-User MIMO)
		config_get_bool mu_mimo "$device" mu_mimo
		if [ "$bf" = 0 ]; then
			mu_mimo=0
		fi

		config_get_bool vhtsubfee "$vif" vhtsubfee "$bf"
		[ -n "$vhtsubfee" ] && "$device_if" "$ifname" vhtsubfee "${vhtsubfee}"

		config_get_bool vhtmubfee "$vif" vhtmubfee "$mu_mimo"
		[ -n "$vhtmubfee" ] && "$device_if" "$ifname" vhtmubfee "${vhtmubfee}"

		config_get_bool vhtsubfer "$vif" vhtsubfer "$bf"
		[ -n "$vhtsubfer" ] && "$device_if" "$ifname" vhtsubfer "${vhtsubfer}"

		config_get_bool vhtmubfer "$vif" vhtmubfer "$mu_mimo"
		[ -n "$vhtmubfer" ] && "$device_if" "$ifname" vhtmubfer "${vhtmubfer}"

		config_get vhtstscap "$vif" vhtstscap
		[ -n "$vhtstscap" ] && "$device_if" "$ifname" vhtstscap "${vhtstscap}"

		config_get vhtsounddim "$vif" vhtsounddim
		[ -n "$vhtsounddim" ] && "$device_if" "$ifname" vhtsounddim "${vhtsounddim}"

		config_get enable_11v_dms "$vif" enable_11v_dms
		[ -n "$enable_11v_dms" ] && "$device_if" "$ifname" enable_11v_dms "${enable_11v_dms}"

		config_get he_subfee "$vif" he_subfee "$bf"
		[ -n "$he_subfee" ] && "$device_if" "$ifname" he_subfee "${he_subfee}"

		config_get he_subfer "$vif" he_subfer "$bf"
		[ -n "$he_subfer" ] && "$device_if" "$ifname" he_subfer "${he_subfer}"

		config_get he_mubfee "$vif" he_mubfee "$mu_mimo"
		[ -n "$he_mubfee" ] && "$device_if" "$ifname" he_mubfee "${he_mubfee}"

		config_get he_mubfer "$vif" he_mubfer "$mu_mimo"
		[ -n "$he_mubfer" ] && "$device_if" "$ifname" he_mubfer "${he_mubfer}"

		config_get he_dlofdma "$vif" he_dlofdma
		[ -n "$he_dlofdma" ] && "$device_if" "$ifname" he_dl_ofdma "${he_dlofdma}"

		config_get he_ulofdma "$vif" he_ulofdma
		[ -n "$he_ulofdma" ] && "$device_if" "$ifname" he_ul_ofdma "${he_ulofdma}"

		config_get he_ulmumimo "$vif" he_ulmumimo
		[ -n "$he_ulmumimo" ] && "$device_if" "$ifname" he_ul_mimo "${he_ulmumimo}"

		config_get he_dcm "$vif" he_dcm
		[ -n "$he_dcm" ] && "$device_if" "$ifname" he_dcm "${he_dcm}"

		config_get he_extrange "$vif" he_extrange
		[ -n "$he_extrange" ] && "$device_if" "$ifname" he_extrange "${he_extrange}"

		config_get he_ltf "$vif" he_ltf
		[ -n "$he_ltf" ] && "$device_if" "$ifname" he_ltf "${he_ltf}"

		config_get he_txmcsmap "$vif" he_txmcsmap
		[ -n "$he_txmcsmap" ] && "$device_if" "$ifname" he_txmcsmap "${he_txmcsmap}"

		config_get he_rxmcsmap "$vif" he_rxmcsmap
		[ -n "$he_rxmcsmap" ] && "$device_if" "$ifname" he_rxmcsmap "${he_rxmcsmap}"

		config_get ba_bufsize "$vif" ba_bufsize
		[ -n "$ba_bufsize" ] && "$device_if" "$ifname" ba_bufsize "${ba_bufsize}"

		config_get encap_type "$vif" encap_type
		[ -n "$encap_type" ] && "$device_if" "$ifname" encap_type "${encap_type}"

		config_get decap_type "$vif" decap_type
		[ -n "$decap_type" ] && "$device_if" "$ifname" decap_type "${decap_type}"

		config_get_bool rawsim_txagr "$vif" rawsim_txagr
		[ -n "$rawsim_txagr" ] && "$device_if" "$ifname" rawsim_txagr "${rawsim_txagr}"

		config_get clr_rawsim_stats "$vif" clr_rawsim_stats
		[ -n "$clr_rawsim_stats" ] && "$device_if" "$ifname" clr_rawsim_stats "${clr_rawsim_stats}"

		config_get_bool rawsim_debug "$vif" rawsim_debug
		[ -n "$rawsim_debug" ] && "$device_if" "$ifname" rawsim_debug "${rawsim_debug}"

		# specify vendor IE
		if [ "$backhaul" -eq 1 ]; then
			[ -n "$vie_oui" ] && [ -n "$vie_bh_flag" ] && wlanconfig "$ifname" vendorie update len 11 oui "$vie_oui" pcap_data "$vie_bh_flag" ftype_map ff
		else
			[ -n "$vie_oui" ] && [ -n "$vie_fh_flag" ] && wlanconfig "$ifname" vendorie update len 11 oui "$vie_oui" pcap_data "$vie_fh_flag" ftype_map ff
		fi

		config_get_bool vap_only "$vif" vap_only 0
		if [ "$vap_only" = "1" ]; then
			start_hostapd=
		elif [ "$start_hostapd" = "" ]; then
			ifconfig "$ifname" up
		fi
		brctl addif "$bridge" "$ifname"
		#enable hairpin mode
		brctl hairpin "$bridge" "$ifname" on

		config_get set_monrxfilter "$vif" set_monrxfilter
		[ -n "$set_monrxfilter" ] && "$device_if" "$ifname" set_monrxfilter "${set_monrxfilter}"

		config_get neighbourfilter "$vif" neighbourfilter
		[ -n "$neighbourfilter" ] && "$device_if" "$ifname" neighbourfilter "${neighbourfilter}"

		config_get athnewind "$vif" athnewind
		[ -n "$athnewind" ] && "$device_if" "$ifname" athnewind "$athnewind"

		config_get osen "$vif" osen
		[ -n "$osen" ] && "$device_if" "$ifname" osen "$osen"

		if [ $osen -ne 0 ]; then
			"$device_if" "$ifname" proxyarp 1
		fi

		config_get re_scalingfactor "$vif" re_scalingfactor
		[ -n "$re_scalingfactor" ] && "$device_if" "$ifname" set_whc_sfactor "$re_scalingfactor"

		config_get ul_hyst "$vif" ul_hyst
		[ -n "$ul_hyst" ] && "$device_if" "$ifname" ul_hyst "${ul_hyst}"

		config_get son_event_bcast qcawifi son_event_bcast
		[ -n "$son_event_bcast" ] && "$device_if" "$ifname" son_event_bcast "${son_event_bcast}"

		config_get root_distance "$vif" root_distance
		[ -n "$root_distance" ] && "$device_if" "$ifname" set_whc_dist "$root_distance"

		config_get caprssi "$vif" caprssi
		[ -n "$caprssi" ] && "$device_if" "$ifname" caprssi "${caprssi}"

		config_get_bool ap_isolation_enabled $device ap_isolation_enabled 0
		config_get_bool isolate "$vif" isolate 0

		if [ $ap_isolation_enabled -ne 0 ]; then
			[ "$mode" = "wrap" ] && isolate=1
			"$device_if" "$phy" isolation "$ap_isolation_enabled"
		fi

                config_get_bool ctsprt_dtmbcn "$vif" ctsprt_dtmbcn
                [ -n "$ctsprt_dtmbcn" ] && "$device_if" "$ifname" ctsprt_dtmbcn "${ctsprt_dtmbcn}"

		config_get assocwar160  "$vif" assocwar160
		[ -n "$assocwar160" ] && "$device_if" "$ifname" assocwar160 "$assocwar160"
		# Force assocwar160 enabled when HT80_80 or HT160 is selected
		[ "$htmode" = "HT80_80" -o "$htmode" = "HT160" ] && "$device_if" "$ifname" assocwar160 1

		config_get rawdwepind "$vif" rawdwepind
		[ -n "$rawdwepind" ] && "$device_if" "$ifname" rawdwepind "$rawdwepind"

		config_get revsig160  "$vif" revsig160
		[ -n "$revsig160" ] && "$device_if" "$ifname" revsig160 "$revsig160"

		config_get channel_block_list "$vif" channel_block_list
		[ -n "$channel_block_list" ] && wifitool "$ifname" block_acs_channel "$channel_block_list"

		config_get rept_spl  "$vif" rept_spl
		[ -n "$rept_spl" ] && "$device_if" "$ifname" rept_spl "$rept_spl"

		config_get cactimeout  "$vif" cactimeout
		[ -n "$cactimeout" ] && "$device_if" "$ifname" set_cactimeout "$cactimeout"

		config_get mark_subchan  "$vif" mark_subchan
		[ -n "$mark_subchan" ] && "$device_if" "$ifname" mark_subchan "$mark_subchan"

		config_get meshdbg "$vif" meshdbg
		[ -n "$meshdbg" ] && "$device_if" "$ifname" meshdbg "$meshdbg"

		config_get rmode_pktsim "$vif" rmode_pktsim
		[ -n "$rmode_pktsim" ] && "$device_if" "$ifname" rmode_pktsim "$rmode_pktsim"

                config_get global_wds qcawifi global_wds

                if [ $global_wds -ne 0 ]; then
                     "$device_if" "$ifname" athnewind 1
                fi

                config_get pref_uplink "$device" pref_uplink
                [ -n "$pref_uplink" ] && "$device_if" "$phy" pref_uplink "${pref_uplink}"

                config_get fast_lane "$device" fast_lane
                [ -n "$fast_lane" ] && "$device_if" "$phy" fast_lane "${fast_lane}"

                if [ $fast_lane -ne 0 ]; then
                        "$device_if" "$ifname" athnewind 1
                fi

		if [ "$ODM" != "dni" ]; then
			local net_cfg bridge
			net_cfg="$(find_net_config "$vif")"
			[ -z "$net_cfg" -o "$isolate" = 1 -a "$mode" = "wrap" ] || {
				[ -f /sys/class/net/${ifname}/parent ] && { \
					bridge="$(bridge_interface "$net_cfg")"
					config_set "$vif" bridge "$bridge"
				}
			}
		fi

		case "$mode" in
			ap|wrap|ap_monitor|ap_smart_monitor|mesh|ap_lp_iot)


				"$device_if" "$ifname" ap_bridge "$((isolate^1))"

				config_get_bool l2tif "$vif" l2tif
				[ -n "$l2tif" ] && "$device_if" "$ifname" l2tif "$l2tif"

				if [ -n "$start_wapid" ]; then
					wapid_setup_vif "$vif" || {
						echo "enable_qcawifi($device): Failed to set up wapid for interface $ifname" >&2
						ifconfig "$ifname" down
						iw "$ifname" del
						continue
					}
				fi

				if [ "$mode" == "ap_lp_iot" ]; then
					default_dtim_period=41
				else
					default_dtim_period=1
				fi
				config_get dtim_period "$vif" dtim_period
				if [ -z "$dtim_period" ]; then
					config_set "$vif" dtim_period $default_dtim_period
				fi

				if [ -n "$start_hostapd" ] && eval "type hostapd_setup_vif" 2>/dev/null >/dev/null; then
					hostapd_setup_vif "$vif" nl80211 no_nconfig || {
						echo "enable_qcawifi($device): Failed to set up hostapd for interface $ifname" >&2
						# make sure this wifi interface won't accidentally stay open without encryption
						ifconfig "$ifname" down
						iw "$ifname" del
						continue
					}
				fi
			;;
			wds|sta)
				if eval "type wpa_supplicant_setup_vif" 2>/dev/null >/dev/null; then
					wpa_supplicant_setup_vif "$vif" nl80211 || {
						echo "enable_qcawifi($device): Failed to set up wpa_supplicant for interface $ifname" >&2
						ifconfig "$ifname" down
						iw "$ifname" del
						continue
					}
				fi
			;;
			adhoc)
				if eval "type wpa_supplicant_setup_vif" 2>/dev/null >/dev/null; then
					wpa_supplicant_setup_vif "$vif" nl80211 || {
						echo "enable_qcawifi($device): Failed to set up wpa"
						ifconfig "$ifname" down
						iw "$ifname" del
						continue
					}
				fi
		esac

		[ -z "$bridge" -o "$isolate" = 1 -a "$mode" = "wrap" ] || {
                        [ -f /sys/class/net/${ifname}/parent ] && { \
				start_net "$ifname" "$net_cfg"
                        }
		}

		set_wifi_up "$vif" "$ifname"
		# configure below options once AP is up , as wireless mode is available now
		config_get frag "$vif" frag
		[ -n "$frag" ] && iw phy "$(cat /sys/class/net/$phy/phy80211/name)"  set frag "${frag%%.*}"

		config_get rts "$vif" rts
		[ -n "$rts" ] && iw phy "$(cat /sys/class/net/$phy/phy80211/name)" set rts "${rts%%.*}"

		config_get set11NRates "$vif" set11NRates
		[ -n "$set11NRates" ] && "$device_if" "$ifname" set11NRates "$set11NRates"

		config_get setwmmparams "$vif" setwmmparams
		[ -n "$setwmmparams" ] && "$device_if" "$ifname" setwmmparams $setwmmparams

		# 256 QAM capability needs to be parsed first, since
		# vhtmcs enables/disable rate indices 8, 9 for 2G
		# only if vht_11ng is set or not
		config_get_bool vht_11ng "$vif" vht_11ng
		[ -n "$vht_11ng" ] && {
			"$device_if" "$ifname" vht_11ng "$vht_11ng"
			"$device_if" "$ifname" 11ngvhtintop "$vht_11ng"
		}

		config_get vhtmcs "$vif" vhtmcs
		[ -n "$vhtmcs" ] && "$device_if" "$ifname" vhtmcs "$vhtmcs"

		config_get dis_legacy "$vif" dis_legacy
		[ -n "$dis_legacy" ] && "$device_if" "$ifname" dis_legacy "$dis_legacy"

		config_get mbo "$vif" mbo
		[ -n "$mbo" ] && "$device_if" "$ifname" mbo "$mbo"

		if [ $mode = "sta" ]; then
			config_get enable_ft "$vif" ieee80211r
			[ -n "$enable_ft" ] && "$device_if" "$ifname" ft "$enable_ft"
		fi

		config_get enable_fils "$vif" ieee80211ai
		config_get fils_discovery_period  "$vif" fils_fd_period 0
		[ -n "$enable_fils" ] && "$device_if" "$ifname" enable_fils "$enable_fils" "$fils_discovery_period"

		config_get bpr_enable  "$vif" bpr_enable
		[ -n "$bpr_enable" ] && "$device_if" "$ifname" set_bpr_enable "$bpr_enable"

		config_get oce "$vif" oce
		[ -n "$oce" ] && "$device_if" "$ifname" oce "$oce"
		[ "$oce" -gt 0 ] && {
			case "$hwmode" in
				11B*|11G*|11NG*)
					"$device_if" "$ifname" set_bcn_rate 5500
					"$device_if" "$ifname" prb_rate 5500
					;;
				*)
					;;
			esac

			[ -z "$enable_fils" ] && {
				config_get fils_discovery_period  "$vif" fils_fd_period 20
				"$device_if" "$ifname" enable_fils 1 "$fils_discovery_period"
			}
		}

		config_get set_bcn_rate "$vif" set_bcn_rate
		[ -n "$set_bcn_rate" ] && "$device_if" "$ifname" set_bcn_rate "$set_bcn_rate"

		config_get mcast_rate "$vif" mcast_rate
		[ -n "$mcast_rate" ] && "$device_if" "$ifname" mcast_rate "${mcast_rate%%.*}"

		#support nawds
		config_get nawds_mode "$vif" nawds_mode
		[ -n "$nawds_mode" ] && wlanconfig "$ifname" nawds mode "${nawds_mode}" -cfg80211

		handle_nawds() {
			local value="$1"
			[ -n "$value" ] && wlanconfig "$ifname" nawds add-repeater $value -cfg80211
		}
		config_list_foreach "$vif" nawds_add_repeater handle_nawds

		handle_hmwds() {
			local value="$1"
			[ -n "$value" ] && wlanconfig "$ifname" hmwds add_addr $value -cfg80211
		}
		config_list_foreach "$vif" hmwds_add_addr handle_hmwds

		config_get nawds_override "$vif" nawds_override
		[ -n "$nawds_override" ] && wlanconfig "$ifname" nawds override "${nawds_override}" -cfg80211

		config_get nawds_defcaps "$vif" nawds_defcaps
		[ -n "$nawds_defcaps" ] && wlanconfig "$ifname" nawds defcaps "${nawds_defcaps}" -cfg80211

		handle_hmmc_add() {
			local value="$1"
			[ -n "$value" ] && wlanconfig "$ifname" hmmc add $value -cfg80211
		}
		config_list_foreach "$vif" hmmc_add handle_hmmc_add

		# TXPower settings only work if device is up already
		# while atheros hardware theoretically is capable of per-vif (even per-packet) txpower
		# adjustment it does not work with the current atheros hal/madwifi driver

		config_get vif_txpower "$vif" txpower
		# use vif_txpower (from wifi-iface) instead of txpower (from wifi-device) if
		# the latter doesn't exist
		txpower="${txpower:-$vif_txpower}"
		[ -z "$txpower" ] || iwconfig "$ifname" txpower "${txpower%%.*}"

		config_get rps "$vif" rps
		if [ $rps == 1 ]; then
			enable_rps_wifi=1
		elif [ $rps == 0 ]; then
			enable_rps_wifi=0
		fi

		if [ $enable_rps_wifi == 1 ] && [ -f "/lib/update_system_params.sh" ]; then
			. /lib/update_system_params.sh
			enable_rps $ifname
		fi

		config_get dyn_bw_rts "$vif" dyn_bw_rts
		[ -n "$dyn_bw_rts" ] && "$device_if" "$ifname" dyn_bw_rts "$dyn_bw_rts"

		if [ "$backhaul" -eq 1 ]
		then
			echo " $mode $ifname $phy" >> /var/run/son.conf
		else
			echo " nbh_$mode $ifname $phy" >> /var/run/son.conf
		fi

	done

	config_get wifi_debug_sh $device wifi_debug_sh
	[ -n "$wifi_debug_sh" -a -e "$wifi_debug_sh" ] && sh "$wifi_debug_sh"

        config_get primaryradio "$device" primaryradio
        [ -n "$primaryradio" ] && "$device_if" "$phy" primaryradio "${primaryradio}"

        config_get CSwOpts "$device" CSwOpts
        [ -n "$CSwOpts" ] && "$device_if" "$phy" CSwOpts "${CSwOpts}"

	#
	# Extended NSS signaling
	#
	# "ext_nss_default" should be defined in /etc/ath/wifi.conf.
	#
	[ -n "$ext_nss_default" -a "$htmode" = "HT160" ] && "$device_if" "$phy" ext_nss "$ext_nss_default"

        config_get improve_wl_con "$device" improve_wl_con
        [ "$improve_wl_con" = "1" ] && do_improve_wl_con $device

	if [ $disable_qrfs_wifi == 1 ] && [ -f "/lib/update_system_params.sh" ]; then
		. /lib/update_system_params.sh
		disable_qrfs
	fi

	# reset vifs
	config_get vifs "$device" vifs
	for vif in $vifs; do
		config_get backhaul "$vif" backhaul 0
		[ -n "$backhaul" -a "$backhaul" = 1 ] && continue
		backhaul_only=0
		config_get ifname "$vif" ifname
		isup=`ifconfig $ifname | grep UP`
		[ -n "$isup" ] && break
	done

	# don't count wifi status for backhaul vap only radio
	[ "$backhaul_only" = "0" ] && {
		# update wlan uptime file
		band_type=`grep "^[ga].*_device" /etc/ath/wifi.conf | grep $phy | cut -c 1`
		if [ "$isup" != "" -a "$band_type" = "g" ]; then
			cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime
			echo "ON" > /tmp/WLAN_2G_status
		elif [ "$isup" != "" -a "$band_type" = "a" ]; then
			cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime_5G
			echo "ON" > /tmp/WLAN_5G_status
		fi
	}

	if [ "$DEVICE_TYPE" = "extender" ]; then
		band_type=`grep "^[ga].*_device" /etc/ath/wifi.conf | grep $phy | cut -c 1`
		ifname=`awk -v input_opmode=STA -v input_wifidev=$phy -v output_rule=ifname -f /etc/search-wifi-interfaces.awk $wifi_topology_file`
		if [ "$band_type" = "g" ] && [ -n "$ifname" ]; then
			killall led-extender
			/sbin/led-extender -i ${ifname} &
			cat /sys/class/net/${ifname}/address > /tmp/mac_addr_${ifname}
		elif [ "$band_type" = "a" ] && [ -n "$ifname" ]; then
			killall led-extender-5G
			/sbin/led-extender-5G -i ${ifname} &
			cat /sys/class/net/${ifname}/address > /tmp/mac_addr_${ifname}
		fi
	else
		[ ! -f /tmp/link_status ] && echo 0 > /tmp/link_status
		[ ! -f /tmp/link_status_5g ] && echo 0 > /tmp/link_status_5g
	fi

	#
	# WARNING: "config_load wireless" may mess up variable "device".
	#
	# Please avoid adding code segments which use "device" after this
	# segment of code.
	#
	if [ -f "/lib/update_smp_affinity.sh" ]; then
		config_load wireless
		. /lib/update_smp_affinity.sh
		config_foreach enable_smp_affinity_wifi wifi-device
	fi

	lock -u /var/run/wifilock
}

setup_wps_enhc_device() {
	local device=$1
	local wps_enhc_cfg=

	append wps_enhc_cfg "RADIO" "$N"
	config_get_bool wps_pbc_try_sta_always "$device" wps_pbc_try_sta_always 0
	config_get_bool wps_pbc_skip_ap_if_sta_disconnected "$device" wps_pbc_skip_ap_if_sta_disconnected 0
	config_get_bool wps_pbc_overwrite_ap_settings "$device" wps_pbc_overwrite_ap_settings 0
	config_get wps_pbc_overwrite_ssid_band_suffix "$device" wps_pbc_overwrite_ssid_band_suffix
	[ $wps_pbc_try_sta_always -ne 0 ] && \
			append wps_enhc_cfg "$device:try_sta_always" "$N"
	[ $wps_pbc_skip_ap_if_sta_disconnected -ne 0 ] && \
			append wps_enhc_cfg "$device:skip_ap_if_sta_disconnected" "$N"
	[ $wps_pbc_overwrite_ap_settings -ne 0 ] && \
			append wps_enhc_cfg "$device:overwrite_ap_settings" "$N"
	[ -n "$wps_pbc_overwrite_ssid_band_suffix" ] && \
			append wps_enhc_cfg "$device:overwrite_ssid_band_suffix:$wps_pbc_overwrite_ssid_band_suffix" "$N"

	config_get vifs $device vifs

	for vif in $vifs; do
		config_get ifname "$vif" ifname


		config_get_bool wps_pbc_enable "$vif" wps_pbc_enable 0
		config_get wps_pbc_start_time "$vif" wps_pbc_start_time
		config_get wps_pbc_duration "$vif" wps_pbc_duration
		config_get_bool wps_pbc_noclone "$vif" wps_pbc_noclone 0
		config_get_bool disabled "$vif" disabled 0
		if [ $disabled -eq 0 -a $wps_pbc_enable -ne 0 ]; then
			append wps_enhc_cfg "VAP" "$N"
			[ -n "$wps_pbc_start_time" -a -n "$wps_pbc_duration" ] && {
				if [ $wps_pbc_noclone -eq 0 ]; then
					append wps_enhc_cfg "$ifname:$wps_pbc_start_time:$wps_pbc_duration:$device:clone" "$N"
				else
					append wps_enhc_cfg "$ifname:$wps_pbc_start_time:$wps_pbc_duration:$device:noclone" "$N"
				fi
			}
			[ -n "$wps_pbc_start_time" -a -n "$wps_pbc_duration" ] || {
				if [ $wps_pbc_noclone -eq 0 ]; then
					append wps_enhc_cfg "$ifname:-:-:$device:clone" "$N"
				else
					append wps_enhc_cfg "$ifname:-:-:$device:noclone" "$N"
				fi
			}
		fi
	done

	cat >> /var/run/wifi-wps-enhc-extn.conf <<EOF
$wps_enhc_cfg
EOF
}

setup_wps_enhc() {
	local wps_enhc_cfg=

	append wps_enhc_cfg "GLOBAL" "$N"
	config_get_bool wps_pbc_overwrite_ap_settings_all qcawifi wps_pbc_overwrite_ap_settings_all 0
	[ $wps_pbc_overwrite_ap_settings_all -ne 0 ] && \
			append wps_enhc_cfg "-:overwrite_ap_settings_all" "$N"
	config_get_bool wps_pbc_overwrite_sta_settings_all qcawifi wps_pbc_overwrite_sta_settings_all 0
	[ $wps_pbc_overwrite_sta_settings_all -ne 0 ] && \
			append wps_enhc_cfg "-:overwrite_sta_settings_all" "$N"
	config_get wps_pbc_overwrite_ssid_suffix qcawifi wps_pbc_overwrite_ssid_suffix
	[ -n "$wps_pbc_overwrite_ssid_suffix" ] && \
			append wps_enhc_cfg "-:overwrite_ssid_suffix:$wps_pbc_overwrite_ssid_suffix" "$N"

	cat >> /var/run/wifi-wps-enhc-extn.conf <<EOF
$wps_enhc_cfg
EOF

	config_load wireless
	config_foreach setup_wps_enhc_device wifi-device
}

qcawifi_start_hostapd_cli() {
	local device=$1
	local ifidx=0
	local radioidx=${device#wifi}

	config_get vifs $device vifs

	for vif in $vifs; do
		local config_methods vifname

		config_get vifname "$vif" ifname

		if [ -n $vifname ]; then
			[ $ifidx -gt 0 ] && vifname="ath${radioidx}$ifidx" || vifname="ath${radioidx}"
		fi

		config_get_bool wps_pbc "$vif" wps_pbc 0
		config_get config_methods "$vif" wps_config
		[ "$wps_pbc" -gt 0 ] && append config_methods push_button

		if [ -n "$config_methods" ]; then
			pid=/var/run/hostapd_cli-$vifname.pid
			hostapd_cli -i $vifname -P $pid -a /lib/wifi/wps-hostapd-update-uci -p /var/run/hostapd-$device -B
		fi

		ifidx=$(($ifidx + 1))
	done
}

pre_qcawificfg80211() {
	local action=${1}

	config_load wireless

	lock /var/run/wifilock
	case "${action}" in
		disable)
			config_get_bool wps_vap_tie_dbdc qcawifi wps_vap_tie_dbdc 0

			if [ $wps_vap_tie_dbdc -ne 0 ]; then
				kill "$(cat "/var/run/hostapd.pid")"
				[ -f "/tmp/hostapd_conf_filename" ] &&
					rm /tmp/hostapd_conf_filename
			fi

			eval "type qwrap_teardown" >/dev/null 2>&1 && qwrap_teardown
			eval "type icm_teardown" >/dev/null 2>&1 && icm_teardown
			eval "type wpc_teardown" >/dev/null 2>&1 && wpc_teardown
			eval "type lowi_teardown" >/dev/null 2>&1 && lowi_teardown
			[ ! -f /etc/init.d/lbd ] || /etc/init.d/lbd stop
			[ ! -f /etc/init.d/hyd ] || /etc/init.d/hyd stop
			[ ! -f /etc/init.d/ssid_steering ] || /etc/init.d/ssid_steering stop
			[ ! -f /etc/init.d/mcsd ] || /etc/init.d/mcsd stop
			[ ! -f /etc/init.d/wsplcd ] || /etc/init.d/wsplcd stop

                       rm -f /var/run/wifi-wps-enhc-extn.conf
                       [ -r /var/run/wifi-wps-enhc-extn.pid ] && kill "$(cat "/var/run/wifi-wps-enhc-extn.pid")"

			rm -f /var/run/iface_mgr.conf
			[ -r /var/run/iface_mgr.pid ] && kill "$(cat "/var/run/iface_mgr.pid")"
                        rm -f /var/run/iface_mgr.pid
			killall iface-mgr

                        if [ -f  "/var/run/son.conf" ]; then
                                rm /var/run/son.conf
                        fi
			;;

		enable)
			local icm_enable

			global_qca_hostapd_restart
			global_qca_wpa_supplicant_restart

			config_get_bool icm_enable icm enable 0
			[ ${icm_enable} -gt 0 ] && \
					eval "type icm_setup" >/dev/null 2>&1 && {
				icm_setup cfg80211
			}
			;;
	esac
	lock -u /var/run/wifilock

	start_recovery_daemon
}

post_qcawificfg80211() {
	local action=${1}
	config_get type "$device" type
	[ "$type" != "qcawificfg80211" ] && return

	lock /var/run/wifilock
	case "${action}" in
		enable)
			local icm_enable qwrap_enable lowi_enable

			# Run a single hostapd instance for all the radio's
			# Enables WPS VAP TIE feature

			config_get_bool wps_vap_tie_dbdc qcawifi wps_vap_tie_dbdc 0

			if [ $wps_vap_tie_dbdc -ne 0 ]; then
				hostapd_conf_file=$(cat "/tmp/hostapd_conf_filename")
				hostapd -P /var/run/hostapd.pid $hostapd_conf_file -B
				config_foreach qcawifi_start_hostapd_cli wifi-device
			fi


			config_get_bool wpc_enable wpc enable 0
			[ ${wpc_enable} -gt 0 ] && \
					eval "type wpc_setup" >/dev/null 2>&1 && {
				wpc_setup
			}

			config_get_bool lowi_enable lowi enable 0
			[ ${lowi_enable} -gt 0 ] && \
				eval "type lowi_setup" >/dev/null 2>&1 && {
				lowi_setup
			}

			eval "type qwrap_setup" >/dev/null 2>&1 && qwrap_setup && _disable_qcawificfg80211

			# These init scripts are assumed to check whether the feature is
			# actually enabled and do nothing if it is not.
			[ ! -f /etc/init.d/lbd ] || /etc/init.d/lbd start
			[ ! -f /etc/init.d/hyfi-bridging ] || /etc/init.d/hyfi-bridging start
			[ ! -f /etc/init.d/ssid_steering ] || /etc/init.d/ssid_steering start
			[ ! -f /etc/init.d/wsplcd ] || /etc/init.d/wsplcd restart

			config_get_bool wps_pbc_extender_enhance qcawifi wps_pbc_extender_enhance 0
			[ ${wps_pbc_extender_enhance} -ne 0 ] && {
				rm -f /var/run/wifi-wps-enhc-extn.conf
				setup_wps_enhc
			}
                        if [ -f  "/var/run/son.conf" ]; then
                                rm /var/run/son.conf
                        fi

			config_load wireless
			config_foreach son_get_config_qcawificfg80211 wifi-device

                        rm -f /etc/ath/iface_mgr.conf
                        rm -f /var/run/iface_mgr.pid
                        iface_mgr_setup
			update_qdss_tracing_daemon_variables
			[ $qdss_tracing = 1 ] && {
				enable_qdss_tracing
			}

			shift
			for device in $@; do
				config_get sys_bridge $device sys_bridge
				[ -n "$sys_bridge" ] && {
					lan_ipaddr=$(ifconfig $sys_bridge | grep "inet addr" | awk '{print $2}' | awk -F ':' '{print $2}')
					netmask=$(ifconfig $sys_bridge | grep "inet addr" | awk '{print $4}' | awk -F ':' '{print $2}')
					_wl_lan_restricted_access_qcacfg80211 $lan_ipaddr $netmask $@
					break
				}
			done

			if [ "$DEVICE_TYPE" = "extender" ] && [ -f /sbin/ap-led ]; then
				pidlist=`pidof ap-led`
				for pid in $pidlist; do
					kill -9 $pid
				done
				/sbin/ap-led &
			fi
		;;
	esac

	atf_configcfg80211
	lock -u /var/run/wifilock
}

post_updateconf_qcawificfg80211()
{
	# 5b8397efc2 commmit has move original feature to post_lock_qcawifi function.
	# Still retain this function for future expansion

	# Add this to avoid script error
	return
}

wifitoggle_qcawificfg80211()
{
	local device="$1"
	local hw_btn_state="$2"
	local gui_radio_state="$3"

	find_qcawifi_phy "$device" || return 1

	band_type=`grep "^[ga].*_device" /etc/ath/wifi.conf | grep $phy | cut -c1`
	config_get vifs "$device" vifs
	for vif in $vifs; do
		config_get ifname "$vif" ifname
		if [ "$hw_btn_state" = "on" ]; then
			if [ "$gui_radio_state" = "on" ] ||
			   [ "$band_type" = "g" -a "$gui_radio_state" = "g_on" ] ||
			   [ "$band_type" = "a" -a "$gui_radio_state" = "a_on" ]; then
				config_get mode "$vif" mode
				if [ "$mode" = "ap" ]; then
					wpa_cli -g /var/run/hostapd/global raw REMOVE $ifname
					test -f /var/run/hostapd_cli-${ifname}.pid && kill $(cat /var/run/hostapd_cli-${ifname}.pid)
				fi
				ifconfig "$ifname" down
			fi
		elif [ "$hw_btn_state" = "off" ]; then
			if [ "$gui_radio_state" = "on" ] ||
			   [ "$band_type" = "g" -a "$gui_radio_state" = "g_on" ] ||
			   [ "$band_type" = "a" -a "$gui_radio_state" = "a_on" ]; then
				isup=`ifconfig $ifname | grep UP`
				[ -n "$isup" ] && continue
				ifconfig "$ifname" up
				config_get enc "$vif" encryption "none"
				case "$enc" in
				none)
					# If we're in open mode and want to use WPS, we
					# must start hostapd_cli
					config_get_bool wps_pbc "$vif" wps_pbc 0
					config_get config_methods "$vif" wps_config
					[ "$wps_pbc" -gt 0 ] && append config_methods push_button
					if [ -n "$config_methods" ]; then
						hostapd_setup_vif "$vif" nl80211 no_nconfig
					fi
					;;
				mixed*|psk*|wpa*)
					config_get mode "$vif" mode
					if [ "$mode" = "ap" ]; then
						hostapd_setup_vif "$vif" nl80211 no_nconfig
					else
						wpa_supplicant_setup_vif "$vif" nl80211
					fi
					;;
				esac
			fi
		fi
	done

	config_get ifname "$vif" ifname
	isup=`ifconfig $ifname | grep UP`

	# update wlan uptime file
	if [ "$isup" != "" -a "$band_type" = "g" ]; then
		cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime
		echo "ON" > /tmp/WLAN_2G_status
	elif [ "$isup" != "" -a "$band_type" = "a" ]; then
		cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime_5G
		echo "ON" > /tmp/WLAN_5G_status
	elif [ "$isup" = "" -a "$band_type" = "g" ]; then
		rm /tmp/WLAN_uptime
		echo "OFF" > /tmp/WLAN_2G_status
	elif [ "$isup" = "" -a "$band_type" = "a" ]; then
		rm /tmp/WLAN_uptime_5G
		echo "OFF" > /tmp/WLAN_5G_status
	fi
}

post_lock_qcawificfg80211()
{
	echo "QCA post_lock_qcawificfg80211"
	local wsplcd_enabled=$(uci get wsplcd.config.HyFiSecurity)
	local hyd_enabled=$(uci get hyd.config.Enable)
	local lbd_enabled=$(uci get lbd.config.Enable)

	if [ "x$wsplcd_enabled" = "x1" ]; then
		echo "Restart wsplcd after wlan up" > /dev/console
		/etc/init.d/wsplcd restart
	fi

	if [ "x$hyd_enabled" = "x1" ]; then
		echo "Restart hyd after wlan up" > /dev/console
		/sbin/wifison.sh updateconf lbd
		/etc/init.d/hyd restart
	fi

	if [ "x$lbd_enabled" = "x1" ]; then
		echo "Restart lbd after wlan up"
		/sbin/wifison.sh updateconf lbd
		/sbin/wifison.sh restart lbd
	fi
}

wifischedule_qcawificfg80211()
{
	local device="$1"
	local hw_btn_state="$2"
	local band="$3"
	local newstate="$4"
	local wifi_topology_file=`cat /etc/ath/wifi.conf | grep WIFI_TOPOLOGY_FILE | awk -F= '{print $2}'`
	local _optype _opmode

	# Does this "$device" contain only backhaul ifaces? 1 = yes, 0 = no
	local backhaul_only=1

	find_qcawifi_phy "$device" || return 1

	config_get hwmode "$device" hwmode
	config_get vifs "$device" vifs
	for vif in $vifs; do
		config_get ifname "$vif" ifname
		_optype=`awk -v input_ifname=$ifname -v output_rule=optype -f /etc/search-wifi-interfaces.awk $wifi_topology_file`
		_opmode=`awk -v input_ifname=$ifname -v output_rule=opmode -f /etc/search-wifi-interfaces.awk $wifi_topology_file`
		# We don't schedule backhaul nor station interfaces
		[ "$_optype" = "BACKHAUL" -o "$_opmode" = "STA" ] && continue
		if [ "$newstate" = "on" -a "$hw_btn_state" = "on" ]; then
			isup=`ifconfig $ifname | grep UP`
			[ -n "$isup" ] && continue
			ifconfig "$ifname" up
			sleep 1
			config_get enc "$vif" encryption "none"
			case "$enc" in
			none)
				# If we're in open mode and want to use WPS, we
				# must start hostapd_cli
				config_get_bool wps_pbc "$vif" wps_pbc 0
				config_get config_methods "$vif" wps_config
				[ "$wps_pbc" -gt 0 ] && append config_methods push_button
				if [ -n "$config_methods" ]; then
					hostapd_setup_vif "$vif" nl80211 no_nconfig
				fi
				;;
			mixed*|psk*|wpa*)
				hostapd_setup_vif "$vif" nl80211 no_nconfig
				;;
			esac

			sleep 1
			i=1
			while [ 1 ];do
				if [ "x$(iwconfig ${ifname} 2>&- |grep 'Access Point: ' |grep 'Not-Associated')" != "x" ]; then
					if [ $i -lt 30 ]; then
						echo "waiting $i : $ifname bring up 1"
					else
						echo "waiting $i : $ifname bring up 2"
						ifconfig ${ifname} down
						sleep 1
						ifconfig ${ifname} up
						sleep 10
						break
					fi
					echo "waiting $i : $ifname bring up 3"
					i=$((i+1))
					sleep 1
				else
					echo "interface up, break"
					break
				fi
			done
		
			if [ "`grep -c "^[ga].*_device" /etc/ath/wifi.conf`" = "1" ]; then
				logger "[Wireless signal schedule] The wireless signal is ON,"
			else
				case "$hwmode" in
				*b|*g|*ng) logger "[Wireless signal schedule] The wireless 2.4GHz signal is ON,";;
				*a|*na|*ac) logger "[Wireless signal schedule] The wireless 5GHz signal is ON," ;;
				esac
			fi
		else
			if [ -f /tmp/schedule_flag ];then
				i=1
				while [ 1 ];do
					if [ "x$(iwconfig ${ifname} 2>&- |grep 'Access Point: ' |grep 'Not-Associated')" = "x"  -o $i -gt 30 ] ;then
						sleep 1
						break;
					fi
					i=$((i+1))
					sleep 1
				done
			fi
			test -f /var/run/hostapd_cli-${ifname}.pid && kill $(cat /var/run/hostapd_cli-${ifname}.pid)
			ifconfig "$ifname" down
			if [ "`grep -c "^[ga].*_device" /etc/ath/wifi.conf`" = "1" ]; then
				logger "[Wireless signal schedule] The wireless signal is OFF,"
			else
				case "$hwmode" in
				*b|*g|*ng) logger "[Wireless signal schedule] The wireless 2.4GHz signal is OFF,";;
				*a|*na|*ac) logger "[Wireless signal schedule] The wireless 5GHz signal is OFF," ;;
				esac
			fi
		fi
	done

	# reset vifs
	config_get vifs "$device" vifs
	for vif in $vifs; do
		config_get backhaul "$vif" backhaul 0
		[ -n "$backhaul" -a "$backhaul" = 1 ] && continue
		backhaul_only=0
		config_get ifname "$vif" ifname
		isup=`ifconfig $ifname | grep UP`
		[ -n "$isup" ] && break
	done

	# don't count wifi status for backhaul vap only radio
	[ "$backhaul_only" = "1" ] && return

	band_type=`grep "^[ga].*_device" /etc/ath/wifi.conf | grep $phy | cut -c 1`
	if [ "$isup" != "" -a "$band_type" = "g" ]; then
		cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime
		echo "ON" > /tmp/WLAN_2G_status
	elif [ "$isup" != "" -a "$band_type" = "a" ]; then
		cat /proc/uptime | sed 's/ .*//' > /tmp/WLAN_uptime_5G
		echo "ON" > /tmp/WLAN_5G_status
	elif [ "$isup" = "" -a "$band_type" = "g" ]; then
		rm /tmp/WLAN_uptime
		echo "OFF" > /tmp/WLAN_2G_status
	elif [ "$isup" = "" -a "$band_type" = "a" ]; then
		rm /tmp/WLAN_uptime_5G
		echo "OFF" > /tmp/WLAN_5G_status
	fi
}

wifistainfo_qcawificfg80211()
{
	local device="$1"
	local _optype
	local _opmode
	local wifi_topology_file=`cat /etc/ath/wifi.conf | grep WIFI_TOPOLOGY_FILE | awk -F= '{print $2}'`

	find_qcawifi_phy "$device" || return 1

	tmpfile=/tmp/sta_info.$$

	config_get vifs "$device" vifs
	for vif in $vifs; do
		config_get ifname "$vif" ifname
		_optype=`awk -v input_ifname=$ifname -v output_rule=optype -f /etc/search-wifi-interfaces.awk $wifi_topology_file`
		_opmode=`awk -v input_ifname=$ifname -v output_rule=opmode -f /etc/search-wifi-interfaces.awk $wifi_topology_file`
		if [ "$_optype" != "NORMAL" -a "$_optype" != "GUEST" ]; then
			continue
		elif [ "$_opmode" != "AP" ]; then
			continue
		fi

		wlanconfig "$ifname" list sta >> $tmpfile
		band_type=`grep "^[ga].*_device" /etc/ath/wifi.conf | grep $phy  | cut -c 1`
		if [ "$band_type" = "g" ]; then
			if [ "$_optype" = "NORMAL" ]; then
				echo "###2.4G###"
			elif [ "$_optype" = "GUEST" ]; then
				echo "###2.4G Guest###"
			fi
		else
			if [ "$_optype" = "NORMAL" ]; then
				echo "###5G###"
			elif [ "$_optype" = "GUEST" ]; then
				echo "###5G Guest###"
			fi
		fi
		[ -f /usr/lib/stainfo.awk ] && awk -f /usr/lib/stainfo.awk $tmpfile
		rm -f $tmpfile
		echo ""
	done
}

wifiradio_qcawificfg80211()
{
	local device=$1

	config_get vifs "$device" vifs
	config_get device_if "$device" device_if "cfg80211tool"

	shift
	while [ "$#" -gt "0" ]; do
		case $1 in
		-s|--status)
			for vif in $vifs; do
				config_get ifname "$vif" ifname
				isup=`ifconfig $ifname | grep UP`
			done
			[ -n "$isup" ] && echo "ON" || echo "OFF"
			shift
			;;
		-c|--channel)
			for vif in $vifs; do
				config_get ifname "$vif" ifname
				isap=`iwconfig $ifname | grep Master`
				[ -z "$isap" ] && continue
				p_chan=`iwlist $ifname chan | grep Current | awk '{printf "%d\n", substr($5,1,length($5))}'`
				cur_mode=`"$device_if" $ifname get_mode | cut -d: -f2`
				is_20=`echo $cur_mode | grep '20'`
				is_plus=`echo $cur_mode | grep 'PLUS'`
				is_minus=`echo $cur_mode | grep 'MINUS'`
				is_80=`echo $cur_mode | grep '80'`

				# when enable wla_ht160, need change mode to HT160 for EU and HT80_80 for NA.
				# EU use channel "36+40+44+48+52+56+60+64" or "100+104+108+112+116+120+124+128"
				# NA use channel "36+40+44+48+149+153+157+161"
				is_80_80=`echo $cur_mode | grep '80_80'`
				is_160=`echo $cur_mode | grep '160'`

				if [ -n "$is_20" ]; then
					chan=$p_chan
				elif [ -n "$is_plus" ]; then
					s_chan=$(($p_chan + 4));
					chan="${p_chan}(P) + ${s_chan}(S)"
				elif [ -n "$is_minus" ]; then
					s_chan=$(($p_chan - 4));
					chan="${p_chan}(P) + ${s_chan}(S)"
				elif [ -z "$is_80_80" -a -n "$is_80" ]; then
					case "${p_chan}" in
					36) chan="36(P) + 40 + 44 + 48" ;;
					40) chan="36 + 40(P) + 44 + 48" ;;
					44) chan="36 + 40 + 44(P) + 48" ;;
					48) chan="36 + 40 + 44 + 48(P)" ;;
					52) chan="52(P) + 56 + 60 + 64" ;;
					56) chan="52 + 56(P) + 60 + 64" ;;
					60) chan="52 + 56 + 60(P) + 64" ;;
					64) chan="52 + 56 + 60 + 64(P)" ;;
					100) chan="100(P) + 104 + 108 + 112" ;;
					104) chan="100 + 104(P) + 108 + 112" ;;
					108) chan="100 + 104 + 108(P) + 112" ;;
					112) chan="100 + 104 + 108 + 112(P)" ;;
					116) chan="116(P) + 120 + 124 + 128" ;;
					120) chan="116 + 120(P) + 124 + 128" ;;
					124) chan="116 + 120 + 124(P) + 128" ;;
					128) chan="116 + 120 + 124 + 128(P)" ;;
					132) chan="132(P) + 136 + 140 + 144";;
					136) chan="132 + 136(P) + 140 + 144";;
					140) chan="132 + 136 + 140(P) + 144";;
					144) chan="132 + 136 + 140 + 144(P)";;
					149) chan="149(P) + 153 + 157 + 161";;
					153) chan="149 + 153(P) + 157 + 161";;
					157) chan="149 + 153 + 157(P) + 161";;
					161) chan="149 + 153 + 157 + 161(P)";;
					esac
				elif [ -n "$is_80_80" ]; then
					case "${p_chan}" in
					36) chan="36(P) + 40 + 44 + 48 + 149 + 153 + 157 + 161" ;;
					40) chan="36 + 40(P) + 44 + 48 + 149 + 153 + 157 + 161" ;;
					44) chan="36 + 40 + 44(P) + 48 + 149 + 153 + 157 + 161" ;;
					48) chan="36 + 40 + 44 + 48(P) + 149 + 153 + 157 + 161" ;;
					149) chan="36 + 40 + 44 + 48 + 149(P) + 153 + 157 + 161";;
					153) chan="36 + 40 + 44 + 48 + 149 + 153(P) + 157 + 161";;
					157) chan="36 + 40 + 44 + 48 + 149 + 153 + 157(P) + 161";;
					161) chan="36 + 40 + 44 + 48 + 149 + 153 + 157 + 161(P)";;
					esac
				elif [ -n "$is_160" ]; then
					case "${p_chan}" in
					36) chan="36(P) + 40 + 44 + 48 + 52 + 56 + 60 + 64" ;;
					40) chan="36 + 40(P) + 44 + 48 + 52 + 56 + 60 + 64" ;;
					44) chan="36 + 40 + 44(P) + 48 + 52 + 56 + 60 + 64" ;;
					48) chan="36 + 40 + 44 + 48(P) + 52 + 56 + 60 + 64" ;;
					52) chan="36 + 40 + 44 + 48 + 52(P) + 56 + 60 + 64" ;;
					56) chan="36 + 40 + 44 + 48 + 52 + 56(P) + 60 + 64" ;;
					60) chan="36 + 40 + 44 + 48 + 52 + 56 + 60(P) + 64" ;;
					64) chan="36 + 40 + 44 + 48 + 52 + 56 + 60 + 64(P)" ;;
					100) chan="100(P) + 104 + 108 + 112 + 116 + 120 + 124 + 128" ;;
					104) chan="100 + 104(P) + 108 + 112 + 116 + 120 + 124 + 128" ;;
					108) chan="100 + 104 + 108(P) + 112 + 116 + 120 + 124 + 128" ;;
					112) chan="100 + 104 + 108 + 112(P) + 116 + 120 + 124 + 128" ;;
					116) chan="100 + 104 + 108 + 112 + 116(P) + 120 + 124 + 128" ;;
					120) chan="100 + 104 + 108 + 112 + 116 + 120(P) + 124 + 128" ;;
					124) chan="100 + 104 + 108 + 112 + 116 + 120 + 124(P) + 128" ;;
					128) chan="100 + 104 + 108 + 112 + 116 + 120 + 124 + 128(P)" ;;
					esac
				else
					chan=$p_chan
				fi
				echo "$chan"
				break;
			done
			shift
			;;
		--coext)
			for vif in $vifs; do
				config_get ifname "$vif" ifname
				isap=`iwconfig $ifname | grep Master`
				[ -z "$isap" ] && continue
				if [ "$2" = "on" ]; then
					"$device_if" $ifname disablecoext 0
					"$device_if" $ifname extbusythres 30
				else
					"$device_if" $ifname disablecoext 1
					"$device_if" $ifname extbusythres 100
				fi
			done
			shift 2
			;;
		*)
			shift
			;;
		esac
	done
}

_get_backhaul_sta_status_qcacfg80211()
{
	local wifi_topology_file=$2
	local ifname
	local bh_connected=0
	for ifname in `awk -v input_optype=BACKHAUL -v input_opmode=STA -v output_rule=ifname -f /etc/search-wifi-interfaces.awk $wifi_topology_file`; do
		local vap_connected

		vap_connected=$(grep up "/sys/devices/virtual/net/$ifname/operstate")
		[ -n "$vap_connected" ] && {
			bh_connected=1
			break;
		}
	done
	if [ "$bh_connected" = "0" ]; then
		eval export -- "${1}=0"
	else
		eval export -- "${1}=1"
	fi
}

_get_optype_opmode_qcacfg80211()
{
	# $1 -> optype
	# $2 -> opmode

	config_get device_if "$device" device_if "cfg80211tool"

	local wifi_topology_file=`cat /etc/ath/wifi.conf | grep WIFI_TOPOLOGY_FILE | awk -F= '{print $2}'`
	[ -z "$wifi_topology_file" ] && {
		eval export -- "${1}=NORMAL"
		eval export -- "${2}=AP"
		return
	}
	local backhaul_sta=`awk -v input_optype=BACKHAUL -v input_opmode=STA -v output_rule=ifname -f /etc/search-wifi-interfaces.awk $wifi_topology_file`

	if [ -z "$backhaul_sta" ]; then
		eval export -- "${2}=AP"
		local backhaul_ap=`awk -v input_optype=BACKHAUL -v input_opmode=AP -v output_rule=ifname -f /etc/search-wifi-interfaces.awk $wifi_topology_file`
		if [ -z "$backhaul_ap" ]; then
			eval export -- "${1}=NORMAL"
		else
			eval export -- "${1}=\"BACKHAUL NORMAL\""
		fi
	else
		_get_backhaul_sta_status_qcacfg80211 _connected $wifi_topology_file
		if [ "$_connected" = "1" ]; then
			for ifname in $backhaul_sta; do
				# find 5g backhaul sta
				band_type=`awk -v input_optype=BACKHAUL -v input_opmode=STA -v input_ifname=$ifname -v output_rule=prefix -f /etc/search-wifi-interfaces.awk $wifi_topology_file | cut -c 3`
				[ "$band_type" = "a" ] && whc_dist=`$device_if $ifname get_whc_dist | cut -d':' -f2`
			done

			if [ "$whc_dist" -eq "1" ]; then
				eval export -- "${1}=\"BACKHAUL NORMAL\""
			else
				eval export -- "${1}=NORMAL"
			fi

			eval export -- "${2}=AP"
		else
			eval export -- "${1}=BACKHAUL"
			eval export -- "${2}=STA"
		fi
	fi
}

wps_qcawificfg80211()
{
	local device=$1
	local hostapd_if=/var/run/hostapd-${device}
	local supplicant_if=var/run/wpa_supplicant-${device}
	local _opmode
	local _optype
	local wifi_topology_file=`cat /etc/ath/wifi.conf | grep WIFI_TOPOLOGY_FILE | awk -F= '{print $2}'`

	config_get device_if "$device" device_if "cfg80211tool"

	_get_optype_opmode_qcacfg80211 _optype _opmode

	shift
	while [ "$#" -gt "0" ]; do
		case $1 in
		-c|--client_pin)
			for optype in $_optype; do
				[ "$optype" = "NORMAL" ] || continue
				for opmode in $_opmode; do
					if [ "$opmode" = "AP" ]; then
						ifname=`awk -v input_optype=$optype -v input_opmode=$opmode -v input_wifidev=$device -v output_rule=ifname -f /etc/search-wifi-interfaces.awk $wifi_topology_file`
						ifname="$(echo -e $ifname | tr -d ' ')"
						[ -n "$ifname" ] || continue
						dir=/var/run/hostapd-$device
						[ -d $dir ] && {
							hostapd_cli -i $ifname -p "$dir" wps_cancel
							sleep 1
							nopbn=`$device_if $ifname get_nopbn | cut -d':' -f2`
							if [ $nopbn != 1 ]; then
								hostapd_cli -i $ifname -p "$dir" wps_pin any $2
							fi
						}
					fi
				done
			done
			shift 2
			;;
		-p|--pbc_start)
			local PIPE_NAME='/var/run/repacd.pipe'
			local repacd_notified=0
			local repacd_enable
			repacd_enable=`/sbin/uci -q get repacd.repacd.Enable`
			for optype in $_optype; do
				for opmode in $_opmode; do
					ifname=`awk -v input_optype=$optype -v input_opmode=$opmode -v input_wifidev=$device -v output_rule=ifname -f /etc/search-wifi-interfaces.awk $wifi_topology_file`
					ifname=`echo -e $ifname | tr -d " "`
					[ -n "$ifname" ] || continue
					if [ "$opmode" = "AP" ]; then
						dir=/var/run/hostapd-$device
						[ -d $dir ] && {
							nopbn=`$device_if $ifname get_nopbn | cut -d':' -f2`
							if [ $nopbn != 1 ]; then
								echo "Activate PBC for $optype:$opmode - $ifname" > /dev/console
								hostapd_cli -i $ifname -p "$dir" wps_cancel
								hostapd_cli -i $ifname -p "$dir" wps_pbc
							fi
						}
					else
						dir=/var/run/wpa_supplicant-$ifname
						[ -d $dir ] && {
							pid=/var/run/wps-hotplug-$ifname.pid
							echo "Activate PBC for $_optype:$_opmode - $ifname" > /dev/console
							wpa_cli -p "$dir" wps_pbc
							[ -f $pid ] || {
								wpa_cli -p"$dir" -a/lib/wifi/wps-supplicant-update-uci -P$pid -B
							}
							if [ -n "$repacd_enable" -a "$repacd_enable" -ne 0 -a -p $PIPE_NAME -a "$repacd_notified" -eq 0 ] ; then
								echo "wps_pbc" > $PIPE_NAME &
								repacd_notified=1
							fi
						}
					fi
				done
			done
			shift
			;;
		-s|--wps_stop)
			for optype in $_optype; do
				for opmode in $_opmode; do
					ifname=`awk -v input_optype=$optype -v input_opmode=$opmode -v input_wifidev=$device -v output_rule=ifname -f /etc/search-wifi-interfaces.awk $wifi_topology_file`
					ifname="$(echo -e $ifname | tr -d ' ')"
					[ -n "$ifname" ] || continue
					if [ "$opmode" = "AP" ]; then
						dir=/var/run/hostapd-$device
						[ -d $dir ] && {
							hostapd_cli -i $ifname -p "$dir" wps_cancel
						}
					else
						dir=/var/run/wpa_supplicant-$ifname
						[ -d $dir ] && {
							pid=/var/run/wps-hotplug-$ifname.pid
							wpa_cli -p "$dir" wps_cancel
						}
					fi
				done
			done
			shift
			;;
		*)
			shift
			;;
		esac
	done
}

statistic_qcawificfg80211()
{
	local device=$1

	find_qcawifi_phy "$device" || return 1

	ifconfig $1 > /dev/null 2>&1
	if [ "$?" = "0" ]; then
		apstatsfile="/tmp/apstats.$$"
		apstats -r -i $1 > $apstatsfile

		tx_packets="$(cat $apstatsfile | awk 'NR == 2 {print $NF}')"  # Tx Data Packets
		rx_packets="$(cat $apstatsfile | awk 'NR == 4 {print $NF}')"  # Rx Data Packets
		collisions=0
		tx_bytes="$(cat $apstatsfile | awk 'NR == 3 {print $NF}')"    # Tx Data Bytes
		rx_bytes="$(cat $apstatsfile | awk 'NR == 5 {print $NF}')"    # Rx Data Bytes

		rm -f $apstatsfile

		band_type=`grep "^[ga].*_device" /etc/ath/wifi.conf | grep $phy | cut -c 1`
		if [ "$band_type" = "g" ]; then
			echo "###2.4G###"
		else
			echo "###5G###"
		fi
		echo "TX packets:$tx_packets"
		echo "RX packets:$rx_packets"
		echo "collisons:$collisions"
		echo "TX bytes:$tx_bytes"
		echo "RX bytes:$rx_bytes"
		echo ""
	fi
}

kickallclient_qcawificfg80211()
{
	local devices="$1"

	config_get device_if "$device" device_if "cfg80211tool"

	for device in ${devices}; do
		config_get vifs "$device" vifs
		for vif in $vifs; do
			# Skip backhaul and not AP mode VAPs.
			config_get mode "$vif" mode
			[ "$mode" != "ap" ] && continue
			config_get backhaul "$vif" backhaul 0
			[ -n "$backhaul" -a "$backhaul" = 1 ] && continue

			config_get ifname "$vif" ifname
			for cli in `wlanconfig $ifname list sta | tail -n +2 | awk '{print $1}'`; do
				echo "Kick clien [$cli] from $ifname" > /dev/console
				"$device_if" $ifname kickmac $cli
			done
		done
	done
}

connection_qcawificfg80211()
{
	local devices="$1"
	local status="$2"

	for device in ${devices}; do
		config_get vifs "$device" vifs
		for vif in $vifs; do
			# Skip backhaul and not AP mode VAPs.
			config_get mode "$vif" mode
			[ "$mode" != "ap" ] && continue
			config_get backhaul "$vif" backhaul 0
			[ -n "$backhaul" -a "$backhaul" = 1 ] && continue

			config_get ifname "$vif" ifname
			if [ "$status" != "deny" ]; then
				config_get vap_only "$vif" vap_only 0
				if [ "$vap_only" != "1" ]; then
					ifconfig $ifname up #iwpriv $ifname maccmd 0
				else
					echo "radio $device is off,so shoule not enable $ifname interface" > /dev/console
				fi
			else
				ifconfig $ifname down #iwpriv $ifname maccmd 1
			fi
		done
	done
}

atf_configcfg80211() {
	config_load wireless
	config_foreach atf_radio_vap_params_configcfg80211 wifi-device

	config_load wireless
	config_foreach atf_group_configcfg80211 atf-config-group

	config_load wireless
	config_foreach atf_ssid_configcfg80211 atf-config-ssid

	config_load wireless
	config_foreach atf_sta_configcfg80211 atf-config-sta

	config_load wireless
	config_foreach atf_ac_configcfg80211 atf-config-ac

	config_load wireless
	config_foreach atf_tput_configcfg80211 atf-config-tput

	config_load wireless
	config_foreach atf_enablecfg80211 wifi-device
}

atf_radio_vap_params_configcfg80211() {
	local device="$1"
	local atf_sched_dur
	local atfstrictsched
	local atfobsssched
	local atfobssscale
	local atfgrouppolicy

	config_get device_if "$device" device_if "cfg80211tool"
	config_get atf_sched_dur "$device" atf_sched_dur
	[ -n "$atf_sched_dur" ] && $device_if "$device" "atf_sched_dur" "$atf_sched_dur"

	config_get atfstrictsched "$device" atfstrictsched
	[ -n "$atfstrictsched" ] && $device_if "$device" "atfstrictsched" "$atfstrictsched"

	config_get atfobsssched "$device" atfobsssched
	[ -n "$atfobsssched" ] && $device_if "$device" "atfobsssched" "$atfobsssched"

	config_get atfobssscale "$device" atfobssscale
	[ -n "$atfobssscale" ] && $device_if "$device" "atfobssscale" "$atfobssscale"

	config_get atfgrouppolicy "$device" atfgrouppolicy
	[ -n "$atfgrouppolicy" ] && $device_if "$device" "atfgrouppolicy" "$atfgrouppolicy"

	config_get disabled $device disabled 0
	if [ $disabled -eq 0 ]; then
		config_get vifs "$device" vifs

		local ifidx=0
		local radioidx=${device#wifi}
		for vif in $vifs; do
			local vifname
			[ $ifidx -gt 0 ] && vifname="ath${radioidx}$ifidx" || vifname="ath${radioidx}"

			config_get atf_shr_buf "$vif" atf_shr_buf
			[ -n "$atf_shr_buf" ] && $device_if "$vifname" "atf_shr_buf" "$atf_shr_buf"

			config_get atf_max_buf "$vif" atf_max_buf
			[ -n "$atf_max_buf" ] && $device_if "$vifname" "atf_max_buf" "$atf_max_buf"

			config_get atf_min_buf "$vif" atf_min_buf
			[ -n "$atf_min_buf" ] && $device_if "$vifname" "atf_min_buf" "$atf_min_buf"

			config_get commitatf "$vif" commitatf
			[ -n "$commitatf" ] && $device_if "$vifname" "commitatf" "$commitatf"

			config_get atfmaxclient "$vif" atfmaxclient
			[ -n "$atfmaxclient" ] && $device_if "$vifname" "atfmaxclient" "$atfmaxclient"

			config_get atfssidgroup "$vif" atfssidgroup
			[ -n "$atfssidgroup" ] && $device_if "$vifname" "atfssidgroup" "$atfssidgroup"

			config_get atf_tput_at "$vif" atf_tput_at
			[ -n "$atf_tput_at" ] && $device_if "$vifname" "atf_tput_at" "$atf_tput_at"

			config_get atfssidsched "$vif" atfssidsched
			[ -n "$atfssidsched" ] && $device_if "$vifname" "atfssidsched" "$atfssidsched"

			ifidx=$(($ifidx + 1))
		done
	fi

}

atf_group_configcfg80211() {
	local cmd
	local group
	local ssid
	local airtime
	local device

	config_get device "$1" device
	radioidx=${device#wifi}

	config_get cmd "$1" command
	config_get group "$1" group
	config_get ssid "$1" ssid
	config_get airtime "$1" airtime

	if [ -z "$cmd" ] || [ -z "$group" ] ; then
		echo "Invalid ATF GROUP Configuration"
		return 1
	fi

	if [ "$cmd" == "addgroup" ] && [ -n "$ssid" ] && [ -n "$airtime" ]; then
		for word in $ssid; do
			wlanconfig ath$radioidx addatfgroup $group $word -cfg80211
		done
		wlanconfig ath$radioidx configatgroup $group $airtime -cfg80211
	fi

	if [ "$cmd" == "delgroup" ]; then
		wlanconfig ath$radioidx delatfgroup $group -cfg80211
	fi
}

atf_ssid_configcfg80211() {
	local cmd
	local ssid
	local airtime
	local device

	config_get device "$1" device
	radioidx=${device#wifi}

	config_get cmd "$1" command
	config_get ssid "$1" ssid
	config_get airtime "$1" airtime

	if [ -z "$cmd" ] || [ -z "$ssid" ] ; then
		echo "Invalid ATF SSID Configuration"
		return 1
	fi

	if [ "$cmd" == "addssid" ] && [ -n "$airtime" ]; then
		wlanconfig ath$radioidx $cmd $ssid $airtime -cfg80211
	fi

	if [ "$cmd" == "delssid" ]; then
		wlanconfig ath$radioidx $cmd $ssid -cfg80211
	fi
}

atf_sta_configcfg80211() {
	local cmd
	local ssid
	local airtime
	local device
	local mac

	config_get device "$1" device
	radioidx=${device#wifi}

	config_get cmd "$1" command
	config_get airtime "$1" airtime
	config_get ssid "$1" ssid
	config_get mac "$1" macaddr
	mac="${mac//:}"

	if [ -z "$cmd" ] || [ -z "$mac" ] ; then
		echo "Invalid ATF STA Configuration"
		return 1
	fi

	if [ "$cmd" == "addsta" ] && [ -n "$airtime" ]; then
		wlanconfig ath$radioidx $cmd $mac $airtime $ssid -cfg80211
	fi

	if [ "$cmd" == "delsta" ]; then
		wlanconfig ath$radioidx $cmd $mac -cfg80211
	fi
}

atf_ac_configcfg80211() {
	local cmd
	local ssid
	local device
	local ac
	local airtime

	config_get device "$1" device
	radioidx=${device#wifi}

	config_get cmd "$1" command
	config_get ac "$1" ac
	config_get airtime "$1" airtime
	config_get ssid "$1" ssid

	if [ -z "$cmd" ] || [ -z "$ssid" ] || [ -z "$ac" ] ; then
		echo "Invalid ATF AC Configuration"
		return 1
	fi

	if [ "$cmd" == "atfaddac" ] && [ -n "$airtime" ]; then
		wlanconfig ath$radioidx $cmd $ssid $ac:$airtime -cfg80211
	fi

	if [ "$cmd" == "atfdelac" ]; then
		wlanconfig ath$radioidx $cmd $ssid $ac -cfg80211
	fi
}

atf_tput_configcfg80211() {
	local cmd
	local tput
	local max_airtime
	local device
	local mac

	config_get device_if "$device" device_if "cfg80211tool"
	config_get device "$1" device
	radioidx=${device#wifi}

	config_get cmd "$1" command
	config_get tput "$1" throughput
	config_get max_airtime "$1" max_airtime
	config_get mac "$1" macaddr
	mac="${mac//:}"

	if [ -z "$cmd" ] || [ -z "$mac" ] || [ -z "$tput" ] ; then
		echo "Invalid ATF Throughput Configuration"
		return 1
	fi

	if [ "$cmd" == "addtputsta" ]; then
		$device_if ath$radioidx commitatf 0
		wlanconfig ath$radioidx addtputsta $mac $tput $max_airtime -cfg80211
	fi

	if [ "$cmd" == "deltputsta" ]; then
		$device_if ath$radioidx commitatf 0
		wlanconfig ath$radioidx deltputsta $mac -cfg80211
	fi
}

atf_enablecfg80211() {
	local device="$1"

	config_get device_if "$device" device_if "cfg80211tool"
	config_get disabled $device disabled 0
	if [ $disabled -eq 0 ]; then
		config_get vifs "$device" vifs
		echo "device: $device vifs: $vifs"

		local ifidx=0
		local radioidx=${device#wifi}
		for vif in $vifs; do
			local vifname
			[ $ifidx -gt 0 ] && vifname="ath${radioidx}$ifidx" || vifname="ath${radioidx}"

			config_get commitatf "$vif" commitatf
			[ -n "$commitatf" ] && $device_if "$vifname" "commitatf" "$commitatf"

			ifidx=$(($ifidx + 1))
		done
	fi
}

check_qcawifi_device() {
	[ ${1%[0-9]} = "wifi" ] && config_set "$1" phy "$1"
	config_get phy "$1" phy
	[ -z "$phy" ] && {
		find_qcawifi_phy "$1" >/dev/null || return 1
		config_get phy "$1" phy
	}
	[ "$phy" = "$dev" ] && found=1
}

ftm_qcawificfg80211() {
	local board_name
	[ -f /tmp/sysinfo/board_name ] && {
		board_name=$(cat /tmp/sysinfo/board_name)
	}
	echo -n "/ini" > /sys/module/firmware_class/parameters/path
	case "$board_name" in
	ap-hk*|ap-dk*|ap-ac*|ap-oa*)
		echo "Entering FTM mode operation" > /dev/console
	;;
	*)
		echo "FTM mode operation not applicable. Returning" > /dev/console
		return
	;;
	esac

	rm -rf /etc/config/wireless

	update_ini_file cfg80211_config "1"
	for mod in $(cat /lib/wifi/qca-wifi-modules); do

		case ${mod} in
			umac) [ -d /sys/module/${mod} ] || { \
				insmod ${mod} || { \
					lock -u /var/run/wifilock
					unload_qcawifi
					error=1
				}
			};;

			qdf) [ -d /sys/module/${mod} ] || { \
				insmod ${mod} || { \
					lock -u /var/run/wifilock
					unload_qcawifi
					error=2
				}
			};;

			qca_ol) [ -d /sys/module/${mod} ] || { \
				do_cold_boot_calibration
				insmod ${mod} testmode=1 || { \
					lock -u /var/run/wifilock
					unload_qcawifi
					error=3
				}
			};;

			qca_da|ath_dev|hst_tx99|ath_rate_atheros|ath_hal)
			;;

			smart_antenna|ath_pktlog)
			;;

			*) [ -d /sys/module/${mod} ] || { \
				insmod ${mod} || { \
					lock -u /var/run/wifilock
					unload_qcawifi
					error=4
				}
			};;

		esac
	done

	case "$board_name" in
	ap-hk*|ap-ac*|ap-oa*)
		rm -rf /etc/modules.d
		mv /etc/modules.d.bk /etc/modules.d
	;;
	*)
	;;
	esac

	sync
	[ $error != 0 ] && echo "FTM error: $error" > /dev/console && return 1
	dmesg -n8
	ftm -n &
	#dmesg got disabled earlier in boot-ftm file
	#enable dmesg back
	echo "FTM mode interface is ready now" > /dev/kmsg
}
detect_qcawificfg80211() {
	local ODM=$1
	local noinsert=$2

	local enable_cfg80211=`uci show qcacfg80211.config.enable |grep "qcacfg80211.config.enable='0'"`
	[ -n "$enable_cfg80211" ] && echo "qcawificfg80211 configuration is disable" > /dev/console && return 1;

	local country

	is_ftm=`grep wifi_ftm_mode /proc/cmdline | wc -l`
	[ $is_ftm = 1 ] && ftm_qcawificfg80211 &&  return

	is_wal=`grep waltest_mode /proc/cmdline | wc -l`
	[ $is_wal = 1 ] && return

	#
	# Workaround for WiFi target FW recovery failure: use the same set of
	# WiFi BDFs in both the first "wlan detect" and the first "wlan up"
	# after boot so that WiFi Tx-power is correct while the WiFi target FW
	# recovery failure issue is mitigated.
	#
	if [ ! -f /tmp/first_wifi_detect_done ]; then
		wlan updateconf

		country="$(uci get wireless.wifi0.country)"
		_set_boarddata_qcacfg80211 "$country"

		rm /etc/config/wireless
	fi

	if [ -e /sys/firmware/devicetree/base/MP_256 ]; then
		update_ini_for_lowmem QCA8074_i.ini
		update_ini_for_lowmem QCA8074V2_i.ini
	fi

	if [ -e /sys/firmware/devicetree/base/MP_512 ]; then
		update_ini_for_512MP QCA8074_i.ini
		update_ini_for_512MP QCA8074V2_i.ini
	fi

	config_present=0
	devidx=0
	socidx=0
	olcfg_ng=0
	olcfg_ac=0
	olcfg_axa=0
	olcfg_axg=0
	nss_olcfg=0
	nss_ol_num=0
	reload=0
	hw_mode_detect=0
	avoid_load=0
	prefer_hw_mode_id="$(grep hw_mode_id \
			/ini/internal/global_i.ini | awk -F '=' '{print $2}')"

	[ $prefer_hw_mode_id -gt 6 ] && prefer_hw_mode_id=6

	if [ $prefer_hw_mode_id == 6 ]; then
		hw_mode_detect=1
	fi
	[ "$ODM" = "dni" -a "$noinsert" = "1" ] || {
		sleep 3

		if [ -e /sys/firmware/devicetree/base/MP_256 ]; then
			for mod in $(cat /lib/wifi/qca-wifi-modules); do
				case ${mod} in
					umac) [ -d /sys/module/${mod} ] && { \
						avoid_load=1
					};;
				esac
			done
		fi

		load_qcawificfg80211
	}
	config_load wireless
	local board_name

	[ -f /tmp/sysinfo/board_name ] && {
		board_name=$(cat /tmp/sysinfo/board_name)
	}

	while :; do
		config_get type "wifi$devidx" type
		[ -n "$type" ] || break
		devidx=$(($devidx + 1))
	done
	cd /sys/class/net
	for soc in $(ls -d soc* 2>&-); do
		if [ -f ${soc}/hw_modes ]; then
			hw_modes=$(cat ${soc}/hw_modes)
			case "${hw_modes}" in
				*DBS_SBS:*)
					prefer_hw_mode_id=4;;
				*DBS:*)
					prefer_hw_mode_id=1;;
				*DBS_OR_SBS:*)
					prefer_hw_mode_id=5;;
				*SINGLE:*)
					prefer_hw_mode_id=0;;
				*SBS_PASSIVE:*)
					prefer_hw_mode_id=2;;
				*SBS:*)
					prefer_hw_mode_id=3;;
			esac
		fi
	done
	if [ $hw_mode_detect == 1 ]; then
		update_internal_ini global_i.ini hw_mode_id "$prefer_hw_mode_id"
	fi

	[ -d wifi0 ] || return
	for dev in $(ls -d wifi* 2>&-); do
		found=0
		config_foreach check_qcawifi_device wifi-device
		if [ "$found" -gt 0 ]; then
			config_present=1
		       	continue
		fi

		hwcaps=$(cat ${dev}/hwcaps)
		case "${hwcaps}" in
			*11an) mode_11=na;;
			*11an/ac) mode_11=ac;;
			*11an/ac/ax) mode_11=axa;;
			*11abgn/ac) mode_11=ac;;
			*11abgn/ac/ax) mode_11=axa;;
			*11abgn) mode_11=ng;;
			*11bgn) mode_11=ng;;
			*11bgn/ax) mode_11=axg;;
		esac
		if [ -f /sys/class/net/${dev}/nssoffload ] && [ $(cat /sys/class/net/${dev}/nssoffload) == "capable" ]; then
			case "${mode_11}" in
				ng)
					if [ $olcfg_ng == 0 ]; then
						olcfg_ng=1
						nss_olcfg=$(($nss_olcfg|$((1<<$devidx))))
						nss_ol_num=$(($nss_ol_num + 1))
					fi
				;;
				na|ac)
					if [ $olcfg_ac == 0 ]; then
						olcfg_ac=1
						nss_olcfg=$(($nss_olcfg|$((1<<$devidx))))
						nss_ol_num=$(($nss_ol_num + 1))
					fi
				;;
                                axa)
                                        if [ $olcfg_axa -le 1 ]; then
                                                olcfg_axa=$(($olcfg_axa + 1))
                                                nss_olcfg=$(($nss_olcfg|$((1<<$devidx))))
                                                nss_ol_num=$(($nss_ol_num + 1))
                                        fi
                                ;;
				axg)
                                        if [ $olcfg_axg == 0 ]; then
                                                olcfg_axg=1
                                                nss_olcfg=$(($nss_olcfg|$((1<<$devidx))))
                                                nss_ol_num=$(($nss_ol_num + 1))
                                        fi
                                ;;

			esac
		reload=1
		fi
		cat <<EOF
config wifi-device  wifi$devidx
	option type	qcawificfg80211
	option channel	auto
	option macaddr	$(cat /sys/class/net/${dev}/address)
	option hwmode	11${mode_11}
	# REMOVE THIS LINE TO ENABLE WIFI:
	option disabled 1

config wifi-iface
	option device	wifi$devidx
	option network	lan
	option mode	ap
	option ssid	OpenWrt
	option encryption none

EOF
	devidx=$(($devidx + 1))
	done

	#config_present 1 indicates that /etc/config/wireless is already having some configuration.
	# In that case we shall not update the olcfg files
	if [ $config_present == 0 ]; then
		case "$board_name" in
		ap-dk01.1-c1 | ap-dk01.1-c2 | ap-dk04.1-c1 | ap-dk04.1-c2 | ap-dk04.1-c3 | ap152 | ap147 | ap151 | ap135 | ap137)
			;;
		ap-hk*)
			if [ -f /etc/rc.d/*qca-nss-ecm ]; then
				echo $nss_olcfg >/lib/wifi/wifi_nss_olcfg
				echo $nss_ol_num >/lib/wifi/wifi_nss_olnum
				echo "$(($olcfg_axa + $olcfg_axg))" > /lib/wifi/wifi_nss_hk_olnum
			else
				echo 0 >/lib/wifi/wifi_nss_olcfg
				echo $nss_ol_num >/lib/wifi/wifi_nss_olnum
				echo "$(($olcfg_axa + $olcfg_axg))" > /lib/wifi/wifi_nss_hk_olnum
			fi
			;;
		*)
			echo $nss_olcfg >/lib/wifi/wifi_nss_olcfg
			echo $nss_ol_num >/lib/wifi/wifi_nss_olnum
			;;
		esac
	fi
	sync


	if [ $reload == 1 ] ; then
		if [ $avoid_load == 1 ]; then
			wifi_updown "disable" "$2" > /dev/null
			ubus call network reload
			wifi_updown "enable" "$2" > /dev/null
		else
			unload_qcawificfg80211 > /dev/null
			load_qcawificfg80211 > /dev/null
		fi
	fi

	start_recovery_daemon

	# Workaround for WiFi target FW recovery failure
	if [ ! -f /tmp/first_wifi_detect_done ]; then
		touch /tmp/first_wifi_detect_done
	fi
}

# Handle traps here
trap_qcawifi() {
	# Release any locks taken
	lock -u /var/run/wifilock
}

_calculate_subnet_qcacfg80211()
{
	local lan_ipaddr=$1
	local netmask=$2
	local mask=0
	local ip=0
	local CIDR=0

	for var in 1 2 3 4
	do
		cnt=$(echo $netmask | awk -F '.' '{print $'$var'}')
		ipt=$(echo $lan_ipaddr | awk -F '.' '{print $'$var'}')
		if [ "$cnt" -gt 255 -o "$cnt" -lt 0 -o "$ipt" -gt 255 -o "$ipt" -lt 0 ]; then
			return
		fi
		mask=$(( mask + (cnt << (4-var)*8) ))
		ip=$(( ip + (ipt << (4-var)*8) ))
	done

	for var in $(seq 31 -1 0)
	do
		cnt=$((mask & (1 << var) ))
		[ $cnt -eq 0 ] && break
		CIDR=$((CIDR + 1))
	done

	ip=$(( ip & (0xffffffff << (32 - CIDR))))
	ip1=$((ip >> 24 & 0xff))
	ip2=$((ip >> 16 & 0xff))
	ip3=$((ip >> 8 & 0xff))
	ip4=$((ip & 0xff))
	subnet=$ip1.$ip2.$ip3.$ip4/$CIDR

	eval export -- "${3}=$subnet"
}

_base_lan_restricted_access_qcacfg80211()
{
	local devices=$@
	local inited=0
	local ETH_P_ARP=0x0806
	local ETH_P_RARP=0x8035
	local ETH_P_IP=0x0800
	local ETH_P_IPv6=0x86dd
	local IPPROTO_UDP=17
	local IPPROTO_ICMPv6=58
	local DHCPS_DHCPC=67:68
	local DHCP6S_DHCP6C=546:547
	local PORT_DNS=53
	local subnet

	for device in ${devices}; do
		config_get vifs "$device" vifs
		for vif in $vifs; do
			config_get_bool lan_restricted "$vif" lan_restricted
			if [ "$lan_restricted" = "1" -a "$inited" = "0" ]; then
				config_get bridge "$vif" bridge
				lan_ipaddr=$(ifconfig $bridge | grep "inet addr" | awk '{print $2}' | awk -F ':' '{print $2}')
				netmask=$(ifconfig $bridge | grep "inet addr" | awk '{print $4}'| awk -F ':' '{print $2}')
				_calculate_subnet_qcacfg80211 $lan_ipaddr $netmask subnet

				ebtables -P FORWARD ACCEPT
				ebtables -A FORWARD -p "$ETH_P_ARP" -j ACCEPT
				ebtables -A FORWARD -p "$ETH_P_RARP" -j ACCEPT
				ebtables -A FORWARD -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT
				ebtables -P INPUT ACCEPT
				ebtables -A INPUT -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT
				ebtables -A INPUT -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$PORT_DNS" -j ACCEPT
				ebtables -A INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_ICMPv6" --ip6-icmp-type ! echo-request -j ACCEPT
				ebtables -A INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$DHCP6S_DHCP6C" -j ACCEPT
				ebtables -A INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$PORT_DNS" -j ACCEPT
				ebtables -N GUEST
				ebtables -P GUEST DROP
				ebtables -A GUEST -p "$ETH_P_ARP" -j ACCEPT
				ebtables -A GUEST -p "$ETH_P_RARP" -j ACCEPT
				ebtables -A GUEST -p $ETH_P_IPv6 --ip6-proto "$IPPROTO_UDP" --ip6-dport "$PORT_DNS" -j ACCEPT
				ebtables -A GUEST -p $ETH_P_IPv6 --ip6-proto "$IPPROTO_UDP" --ip6-dport "$DHCP6S_DHCP6C" -j ACCEPT
				ebtables -A GUEST -p $ETH_P_IP --ip-proto "$IPPROTO_UDP" --ip-dport "$PORT_DNS" -j ACCEPT
				ebtables -A GUEST -p $ETH_P_IP --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT

				inited=1
			fi

			if [ "$lan_restricted" = "1" ]; then
				config_get ifname "$vif" ifname
				config_get bridge "$vif" bridge
				lan_ipaddr=$(ifconfig $bridge | grep "inet addr" | awk '{print $2}' | awk -F ':' '{print $2}')
				lan_ipv6addr=$(ifconfig $bridge | grep Scope:Link | awk '{print $3}' | awk -F '/' '{print $1}')
				ipv6subnet=$(ifconfig $bridge | grep "inet6 addr" | grep "Link" | awk '{print $3}')
				ebtables -A INPUT -i "$ifname" -p "$ETH_P_IP" --ip-dst "$subnet" -j GUEST
				ebtables -A INPUT -i "$ifname" -p "$ETH_P_IPv6" --ip6-dst "$lan_ipv6addr" -j DROP
				ebtables -A INPUT -i "$ifname" -p "$ETH_P_IP" --ip-dst "$lan_ipaddr" -j DROP
				ebtables -A FORWARD -i "$ifname" -p "$ETH_P_IP" --ip-dst "$subnet" -j GUEST
				ebtables -A FORWARD -i "$ifname" -p "$ETH_P_IPv6" --ip6-dst "$ipv6subnet" -j GUEST
			fi
		done
	done
}

_wl_lan_restricted_access_qcacfg80211()
{
	local product_type

	if ! eval "type ebtables" 2>>/dev/null >/dev/null; then
		echo "Please install tool ebtables first"
		return
	fi

	_clear_wifi_ebtables_qcacfg80211

	[ -f "/tmp/orbi_type" ] && product_type=`/bin/cat /tmp/orbi_type`

	if [ "$product_type" = "Satellite" ]; then
		_satellite_lan_restricted_access_qcacfg80211 $@
	elif [ "$product_type" = "Base" ]; then
		shift 2
		_base_lan_restricted_access_qcacfg80211 $@
	else
		shift 2
		_router_lan_restricted_access_qcacfg80211 $@
	fi
}

_router_lan_restricted_access_qcacfg80211()
{
	local devices=$@
	local inited=0
	local ETH_P_ARP=0x0806
	local ETH_P_RARP=0x8035
	local ETH_P_IP=0x0800
	local ETH_P_IPv6=0x86dd
	local IPPROTO_UDP=17
	local IPPROTO_ICMPv6=58
	local DHCPS_DHCPC=67:68
	local DHCP6S_DHCP6C=546:547
	local PORT_DNS=53

	for device in ${devices}; do
		config_get vifs "$device" vifs
		for vif in $vifs; do
			config_get_bool lan_restricted "$vif" lan_restricted
			if [ "$lan_restricted" = "1" -a "$inited" = "0" ]; then
				ebtables -P FORWARD ACCEPT
				ebtables -A FORWARD -p "$ETH_P_ARP" -j ACCEPT
				ebtables -A FORWARD -p "$ETH_P_RARP" -j ACCEPT
				ebtables -A FORWARD -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT
				ebtables -P INPUT ACCEPT
				ebtables -A INPUT -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT
				ebtables -A INPUT -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$PORT_DNS" -j ACCEPT
				ebtables -A INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_ICMPv6" --ip6-icmp-type ! echo-request -j ACCEPT
				ebtables -A INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$DHCP6S_DHCP6C" -j ACCEPT
				ebtables -A INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$PORT_DNS" -j ACCEPT

				inited=1
			fi

			if [ "$lan_restricted" = "1" ]; then
				config_get ifname "$vif" ifname
				config_get bridge "$vif" bridge
				lan_ipaddr=$(ifconfig $bridge | grep "inet addr" | awk '{print $2}' | awk -F ':' '{print $2}')
				lan_ipv6addr=$(ifconfig $bridge | grep Scope:Link | awk '{print $3}' | awk -F '/' '{print $1}')
				ebtables -A FORWARD -i "$ifname" -j DROP
				ebtables -A FORWARD -o "$ifname" -j DROP
				ebtables -A INPUT -i "$ifname" -p "$ETH_P_IP" --ip-dst "$lan_ipaddr" -j DROP
				ebtables -A INPUT -i "$ifname" -p "$ETH_P_IPv6" --ip6-dst "$lan_ipv6addr" -j DROP
			fi
		done
	done
}

_clear_wifi_ebtables_qcacfg80211()
{
	local ETH_P_ARP=0x0806
	local ETH_P_RARP=0x8035
	local ETH_P_IP=0x0800
	local ETH_P_IPv6=0x86dd
	local IPPROTO_UDP=17
	local IPPROTO_ICMPv6=58
	local DHCPS_DHCPC=67:68
	local DHCP6S_DHCP6C=546:547
	local PORT_DNS=53

	if ! eval "type ebtables" 2>>/dev/null >/dev/null; then
		echo "Please install tool ebtables first"
		return
	fi
	ebtables -D FORWARD -p "$ETH_P_ARP" -j ACCEPT 2>/dev/null >/dev/null
	ebtables -D FORWARD -p "$ETH_P_RARP" -j ACCEPT 2>/dev/null >/dev/null
	ebtables -D FORWARD -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT 2>/dev/null >/dev/null
	ebtables -D INPUT -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT 2>/dev/null >/dev/null
	ebtables -D INPUT -p "$ETH_P_IP" --ip-proto "$IPPROTO_UDP" --ip-dport "$PORT_DNS" -j ACCEPT 2>/dev/null >/dev/null
	ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_ICMPv6" --ip6-icmp-type ! echo-request -j ACCEPT 2>/dev/null >/dev/null
	ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$DHCP6S_DHCP6C" -j ACCEPT 2>/dev/null >/dev/null
	ebtables -D INPUT -p "$ETH_P_IPv6" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$PORT_DNS" -j ACCEPT 2>/dev/null >/dev/null
	ebtables -L | grep "ath" > /tmp/wifi_rules
	while read loop
	do
		ebtables -D INPUT $loop 2>/dev/null >/dev/null
		ebtables -D FORWARD $loop 2>/dev/null >/dev/null
	done < /tmp/wifi_rules
	rm /tmp/wifi_rules
	ebtables -X GUEST
}

lan_restricted_access_qcawificfg80211()
{
	_wl_lan_restricted_access_qcacfg80211 $@
}

_satellite_lan_restricted_access_qcacfg80211()
{
	local lan_ipaddr=$1
	local netmask=$2
	shift 2
	local devices=$@
	local inited=0
	local ETH_P_IP=0x0800
	local ETH_P_IPv6=0x86dd
	local IPPROTO_UDP=17
	local DHCPS_DHCPC=67:68
	local DHCP6S_DHCP6C=546:547
	local PORT_DNS=53
	local subnet

	for device in ${devices}; do
		config_get vifs "$device" vifs
		for vif in $vifs; do
			config_get_bool lan_restricted "$vif" lan_restricted
			if [ "$lan_restricted" = "1" -a "$inited" = "0" ]; then
				_calculate_subnet_qcacfg80211 $lan_ipaddr $netmask subnet

				ebtables -P FORWARD ACCEPT

				inited=1
			fi

			if [ "$lan_restricted" = "1" ]; then
				config_get ifname "$vif" ifname
				config_get bridge "$vif" bridge
				ipv6subnet=$(ifconfig $bridge | grep "inet6 addr" | grep "Link" | awk '{print $3}')
				ebtables -A INPUT -i "$ifname" -p $ETH_P_IP --ip-proto tcp --ip-dport 22 -j DROP
				ebtables -A INPUT -i "$ifname" -p $ETH_P_IP --ip-proto tcp --ip-dport 23 -j DROP
				ebtables -A INPUT -i "$ifname" -p $ETH_P_IP --ip-proto tcp --ip-dport 80 -j DROP
				ebtables -A INPUT -i "$ifname" -p $ETH_P_IP --ip-proto icmp -j DROP
				ebtables -A FORWARD -p $ETH_P_IPv6 -i "$ifname" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$PORT_DNS" -j ACCEPT
				ebtables -A FORWARD -p $ETH_P_IPv6 -i "$ifname" --ip6-proto "$IPPROTO_UDP" --ip6-dport "$DHCP6S_DHCP6C" -j ACCEPT
				ebtables -A FORWARD -p $ETH_P_IP -i "$ifname" --ip-proto "$IPPROTO_UDP" --ip-dport "$PORT_DNS" -j ACCEPT
				ebtables -A FORWARD -p $ETH_P_IP -i "$ifname" --ip-proto "$IPPROTO_UDP" --ip-dport "$DHCPS_DHCPC" -j ACCEPT
				ebtables -A FORWARD -p $ETH_P_IP -i "$ifname" --ip-dst "$subnet" -j DROP
				ebtables -A FORWARD -p $ETH_P_IPv6 -i "$ifname" --ip6-dst "$ipv6subnet" -j DROP
			fi
		done
	done
}

clear_lan_restricted_access_qcawificfg80211()
{
	_clear_wifi_ebtables_qcacfg80211
}

son_get_config_qcawificfg80211()
{
    config_load wireless
    local device="$1"
    config_get disabled $device disabled 0
    if [ $disabled -eq 0 ]; then
    config_get vifs $device vifs
    for vif in $vifs; do
        config_get_bool disabled $vif disabled 0
        [ $disabled = 0 ] || continue
        config_get backhaul "$vif" backhaul 0
        config_get mode $vif mode
        config_get ifname $vif ifname
        local macaddr="$(config_get "$device" macaddr)"
        if [ $backhaul -eq 1 ]; then
            echo " $mode $ifname $device $macaddr"  >> /var/run/son.conf
        else
            echo " nbh_$mode $ifname $device $macaddr"  >> /var/run/son.conf
        fi
    done
    fi
}
