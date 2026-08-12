# laptop configurations
# ./.laptop_config &
eval $(dbus-launch --sh-syntax --exit-with-session)

# set background image
feh --bg-scale ~/.config/wm-config/wallpapers/arch/arch-dirty-darker.png &

# terminal transparency
if ! ps aux | grep -v grep | grep picom > /dev/null; then
	nohup picom -b --config .config/picom/picom.conf > /dev/null 2>&1 &
fi

# start slstatus ( quick fix )
slstatus &

# start dwm
while type dwm >/dev/null; do dwm && continue || break ; done

exec dwm

