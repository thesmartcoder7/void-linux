#!/bin/bash

TOUCHPAD=$(xinput list | \
    grep -i "touchpad" | \
    grep -o 'id=[0-9]*' | \
    cut -d= -f2)

# Exit if no touchpad found
[ -z "$TOUCHPAD" ] && {
    notify-send "Touchpad" "No touchpad detected"
    exit 1
}

xinput set-prop "$TOUCHPAD" "libinput Tapping Enabled" 1
xinput set-prop "$TOUCHPAD" "libinput Natural Scrolling Enabled" 1

