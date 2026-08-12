#!/bin/sh

gray_1="#222222"
gray_2="#444444"
gray_3="#bbbbbb"
gray_4="#eeeeee"
gray_5="#595959"
font="JetBrainsMono Nerd Font Mono:size=14"

dmenu_cmd() {
    dmenu -i -p "$1" \
        -fn "$font" \
        -nb "$gray_1" \
        -nf "$gray_3" \
        -sb "$gray_5" \
        -sf "$gray_4"
}

notify() {
    notify-send -t 3000 "Bluetooth" "$1"
}

initialize_bluetooth() {
    bluetoothctl agent on >/dev/null 2>&1
    bluetoothctl default-agent >/dev/null 2>&1
    bluetoothctl power on >/dev/null 2>&1
    sleep 0.5
}

toggle_power() {
    if [ "$(bluetoothctl show 2>/dev/null | awk '/Powered:/ {print $2}')" = "yes" ]; then
        bluetoothctl power off >/dev/null 2>&1 && notify "Bluetooth turned OFF"
    else
        bluetoothctl power on >/dev/null 2>&1 && notify "Bluetooth turned ON"
    fi
}

# ====================== SCAN & CONNECT ======================
scan_connect() {
    initialize_bluetooth
    notify "Scanning for nearby devices..."
    bluetoothctl --timeout 3 scan on >/dev/null 2>&1

    devices=$(bluetoothctl devices 2>/dev/null)
    [ -z "$devices" ] && { notify "No devices found."; exit 1; }

    device_list=$(echo "$devices" | awk '{for(i=3;i<=NF;i++) printf $i" "; print "(" $2 ")"}' | sed 's/  / /g')

    choice=$(echo "$device_list" | dmenu_cmd "Select Device:")

    [ -z "$choice" ] && exit 0

    mac=$(echo "$choice" | grep -oE '\([0-9A-F:]{17}\)' | tr -d '()')
    name=$(echo "$choice" | sed 's/ ([0-9A-F:]\{17\})$//')

    notify "Connecting to $name..."
    bluetoothctl pair "$mac" >/dev/null 2>&1
    bluetoothctl trust "$mac" >/dev/null 2>&1

    if bluetoothctl connect "$mac" >/dev/null 2>&1; then
        notify "Connected to $name"
    else
        notify "Connection failed"
    fi
}

# ====================== MAIN MENU ======================
initialize_bluetooth

bt_status=$(bluetoothctl show 2>/dev/null | awk '/Powered:/ {print $2}' || echo "off")
bt_status_text=$([ "$bt_status" = "yes" ] && echo "On" || echo "Off")

# Get currently connected device + battery
current=$(bluetoothctl devices Connected 2>/dev/null | head -n1)
current_name=$(echo "$current" | cut -d' ' -f3-)
current_mac=$(echo "$current" | awk '{print $2}')

menu="${menu}Status: ${bt_status_text}\n"

if [ -n "$current_name" ]; then
    # Get battery level
    battery=$(bluetoothctl info "$current_mac" 2>/dev/null | awk '/Battery Percentage:/ {print $4}' | tr -d '()')
    if [ -n "$battery" ]; then
        menu="${menu}Current: ${current_name}\n"
        menu="${menu}Current Device Battery: ${battery}%\n"
    else
        menu="${menu}Current: ${current_name}\n"
    fi
else
    menu="${menu}Current: None\n"
fi

menu="${menu}\n>>> Scan for Devices <<<\n"

choice=$(echo -e "$menu" | dmenu_cmd "Bluetooth")

[ -z "$choice" ] && exit 0

# ====================== ACTIONS ======================
case "$choice" in
    "Bluetooth")
        toggle_power
        ;;

    ">>> Scan for Devices <<<")
        scan_connect
        ;;

    "Status: "*)
        toggle_power
        ;;

    "Current: "*)
        # Toggle current device
        if [ -n "$current_mac" ]; then
            if bluetoothctl info "$current_mac" 2>/dev/null | grep -q "Connected: yes"; then
                bluetoothctl disconnect "$current_mac" >/dev/null 2>&1 && notify "Disconnected from ${current_name}"
            else
                bluetoothctl connect "$current_mac" >/dev/null 2>&1 && notify "Connected to ${current_name}"
            fi
        fi
        ;;

    *"○ "*)
        name=$(echo "$choice" | sed 's/^○ //')
        mac=$(bluetoothctl paired-devices 2>/dev/null | grep -F "$name" | awk '{print $2}')
        [ -n "$mac" ] && {
            initialize_bluetooth
            bluetoothctl connect "$mac" >/dev/null 2>&1 && notify "Connected to $name" || notify "Failed to connect"
        }
        ;;
esac
