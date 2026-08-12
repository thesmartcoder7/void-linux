#!/bin/sh
# dmenu-network — enhanced network manager with toggles + scan + new network support

gray_1="#222222"
gray_2="#444444"
gray_3="#bbbbbb"
gray_4="#eeeeee"
gray_5="#595959"
font="JetBrainsMono Nerd Font Mono:size=14"

# ------------------------------------------------------------
# Helper: show dmenu with our theme
# ------------------------------------------------------------
dmenu_cmd() {
    dmenu -i -p "$1" \
        -fn "$font" \
        -nb "$gray_1" \
        -nf "$gray_3" \
        -sb "$gray_5" \
        -sf "$gray_4"
}

# ------------------------------------------------------------
# Toggle WiFi radio
# ------------------------------------------------------------
toggle_wifi() {
    if [ "$(nmcli radio wifi)" = "enabled" ]; then
        nmcli radio wifi off && notify-send -t 3000 "WiFi Disabled"
    else
        nmcli radio wifi on && notify-send -t 3000 "WiFi Enabled"
    fi
}

# ------------------------------------------------------------
# Toggle Ethernet (on/off)
# ------------------------------------------------------------
toggle_ethernet() {
    eth_dev=$(nmcli -t -f DEVICE,TYPE device | awk -F: '$2=="ethernet" {print $1; exit}')
    [ -z "$eth_dev" ] && { notify-send "No ethernet device found"; exit 1; }

    state=$(nmcli -t -f STATE device | grep "^${eth_dev}:" | cut -d: -f2)

    if [ "$state" = "connected" ]; then
        nmcli device disconnect "$eth_dev" && notify-send -t 3000 "Ethernet Disconnected"
    else
        nmcli device set "$eth_dev" managed yes
        nmcli device connect "$eth_dev" && notify-send -t 3000 "Ethernet Connected" \
            || notify-send -t 3000 "Failed to connect Ethernet"
    fi
}

# ------------------------------------------------------------
# Build main menu
# ------------------------------------------------------------
wifi_state=$(nmcli radio wifi)
eth_dev=$(nmcli -t -f DEVICE,TYPE device | awk -F: '$2=="ethernet" {print $1; exit}')
eth_state=$( [ -n "$eth_dev" ] && nmcli -t -f STATE device | grep "^${eth_dev}:" | cut -d: -f2 || echo "Unavailable" )

menu="WiFi: ${wifi_state}\nEthernet: ${eth_state}\n>>> Scan for WiFi networks <<<\n"

# Saved connections
saved=$(nmcli -t -f NAME,TYPE connection show 2>/dev/null | \
        awk -F: '$2 != "loopback" {print $1 " (" $2 ")"}')

[ -n "$saved" ] && menu="${menu}\n--- Saved Connections ---\n${saved}"

choice=$(echo -e "$menu" | dmenu_cmd "Network:")

[ -z "$choice" ] && exit 0

# ------------------------------------------------------------
# Handle choice
# ------------------------------------------------------------
case "$choice" in
    *"WiFi:"*)
        toggle_wifi
        ;;

    *"Ethernet:"*)
        toggle_ethernet
        ;;

    *">>> Scan for WiFi networks <<<"*)
        # Turn WiFi on if needed
        if [ "$(nmcli radio wifi)" = "disabled" ]; then
            nmcli radio wifi on
            notify-send -t 2000 "WiFi turned on – scanning..."
            sleep 2
        fi

        nmcli device wifi rescan >/dev/null 2>&1

        # Build nice network list (SSID (signal%) [security])
        networks=$(nmcli --terse --fields SSID,SIGNAL,SECURITY device wifi list 2>/dev/null | \
            awk -F: '
                $1 != "" && $1 != "--" {
                    sec = ($3 == "") ? "Open" : $3
                    printf "%s (%s%%) [%s]\n", $1, $2, sec
                }' | sort -u)

        [ -z "$networks" ] && { notify-send "No WiFi networks found"; exit 1; }

        net_choice=$(echo "$networks" | dmenu_cmd "Select WiFi:")

        [ -z "$net_choice" ] && exit 0

        # Extract clean SSID
        ssid=$(echo "$net_choice" | cut -d'(' -f1 | xargs)

        # Already saved connection?
        if nmcli -t -f NAME connection show | grep -q "^${ssid}$"; then
            nmcli connection up "$ssid" && notify-send "Connected to ${ssid}" \
                || notify-send "Failed to connect"
        else
            # New network
            security=$(echo "$net_choice" | grep -o '\[.*\]' | tr -d '[]')
            if [ "$security" = "Open" ] || [ -z "$security" ]; then
                nmcli device wifi connect "$ssid" && notify-send "Connected to ${ssid}" \
                    || notify-send "Failed to connect"
            else
                # Ask for password (visible — limitation of plain dmenu)
                password=$(dmenu_cmd "Password for ${ssid}:" </dev/null)
                if [ -n "$password" ]; then
                    nmcli device wifi connect "$ssid" password "$password" \
                        && notify-send "Connected to ${ssid}" \
                        || notify-send "Failed (wrong password?)"
                else
                    notify-send "Cancelled"
                fi
            fi
        fi
        ;;

    *)
        # Saved connection selected
        name=$(echo "$choice" | cut -d'(' -f1 | xargs)
        nmcli connection up "$name" && notify-send "Connected to $name" \
            || notify-send "Failed to connect"
        ;;
esac

