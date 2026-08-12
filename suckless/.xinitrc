# laptop configurations
./.laptop_config & 

# launch dbus
eval $(dbus-launch --sh-syntax --exit-with-session)

# set background image
feh --bg-scale ~/Pictures/debian_background.jpg &

# terminal transparency
if ! ps aux | grep -v grep | grep picom > /dev/null; then
	nohup picom > /dev/null 2>&1 &
fi

# start slstatus ( quick fix )
slstatus &

# home wifi auto login
nmcli device wifi connect Smart-code_5G password "#thesmartcoder" &

# start dwm
while type dwm >/dev/null; do dwm && continue || break ; done

exec dwm

