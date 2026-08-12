#!/bin/sh
# dmenu-mount (using udisksctl for user mounts)
    
gray_1="#222222"
gray_2="#444444"
gray_3="#bbbbbb"
gray_4="#eeeeee"
gray_5="#595959"

font="JetBrainsMono Nerd Font Mono:size=14"

action=$(echo -e "Mount\nUnmount\nPoweroff" | dmenu -i -p "Drive action:" -fn "$font" -nb "$gray_1" -nf "$gray_3" -sb "$gray_5" -sf "$gray_4")

case $action in
    Mount)
        dev=$(lsblk -rno NAME,SIZE,TYPE,MOUNTPOINT | awk '$3=="part" && $4=="" {print $1 " (" $2 ")"}' | dmenu -i -l 10 -p "Mount:" -fn "$font" -nb "$gray_1" -nf "$gray_3" -sb "$gray_5" -sf "$gray_4")
        [ -n "$dev" ] && udisksctl mount -b "/dev/${dev%% *}"
        ;;
    Unmount)
        dev=$(lsblk -rno NAME,MOUNTPOINT | awk '$2!="" {print $1 " (" $2 ")"}' | dmenu -i -p "Unmount:" -fn "$font" -nb "$gray_1" -nf "$gray_3" -sb "$gray_5" -sf "$gray_4")
        [ -n "$dev" ] && udisksctl unmount -b "/dev/${dev%% *}"
        ;;
    Poweroff)
        dev=$(lsblk -rno NAME,SIZE,TYPE,MOUNTPOINT | awk '$3=="disk" {print $1 " (" $2 ")"}' | dmenu -i -p "Poweroff:" -fn "$font" -nb "$gray_1" -nf "$gray_3" -sb "$gray_5" -sf "$gray_4" )
        [ -n "$dev" ] && udisksctl power-off -b "/dev/${dev%% *}"
        ;;
esac

