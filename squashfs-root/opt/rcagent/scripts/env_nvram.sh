#!/bin/sh
#Script is setting up /opt/usi/usi variables needed to register device in cloud

/opt/usi/usi set readycloud_fetch_url="https://readycloud.netgear.com/device/entry"
/opt/usi/usi set readycloud_hook_url="https://readycloud.netgear.com/device/hook"
/opt/usi/usi set readycloud_upload_url="https://readycloud.netgear.com/directio"

#set up variables for readycloud_control
/opt/usi/usi set rcagent_path="/opt/rcagent"
/opt/usi/usi set readycloud_control_path="/opt/rcagent/scripts"
/opt/usi/usi set remote_path="/opt/remote"
/opt/usi/usi set leafp2p_path="/opt/leafp2p"

/opt/usi/usi set readycloud_use_lantry=1
/opt/usi/usi set rcagent_log_to_console=0
/opt/usi/usi set rcagent_log_level=error
/opt/usi/usi set rcagent_log_to_file=1

/opt/usi/usi commit
