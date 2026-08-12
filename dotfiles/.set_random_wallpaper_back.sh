#!/bin/bash

# get random wallpaper
rand_image=$(find ~/Pictures -type f  -name "*.jpg" -o -name "*.png" | shuf -n 1)

# echo $rand_image

# set background
feh --no-fehbg --bg-scale "$rand_image"

# activate pywal environment
source $HOME/pywal-env/bin/activate

# set the colors using wal
wal -i $rand_image 2>&1 -q

# remove the urgent command for colors section
sed -i 'N;$!P;D' $HOME/.cache/wal/colors-wal-dwm.h

# recomplile dwm
cd $HOME/.config/suckless/dwm && doas make clean install>/dev/null
