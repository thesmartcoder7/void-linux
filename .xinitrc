# laptop configurations
./~/.config/void-linux/laptop-config.sh &

# Ensure XDG_RUNTIME_DIR is set
export XDG_RUNTIME_DIR="/run/user/$(id -u)"

# start a dbus session
eval $( dbus-launch --sh-syntax --exit-with-session )

# start the notification daemon
dunst &

# re-apply udev sound issue
/usr/bin/udevadm trigger --subsystem-match=sound

# start the audio stack
pipewire &

# set background image
feh --bg-scale ~/.config/void-linux/wallpapers/enter-the-void.png  &

# terminal transparency
if ! ps aux | grep -v grep | grep picom > /dev/null; then
	nohup picom -b --config .config/picom/picom.conf > /dev/null 2>&1 &
fi

# start slstatus ( quick fix )
slstatus &

# start dwm
while type dwm >/dev/null; do dwm && continue || break ; done

exec dwm

