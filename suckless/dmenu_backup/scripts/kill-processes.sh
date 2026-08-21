#!/bin/sh

# dmenu-kill - Kill processes using dmenu

gray_1="#222222"
gray_2="#444444"
gray_3="#bbbbbb"
gray_4="#eeeeee"
gray_5="#595959"

font="JetBrainsMono Nerd Font Mono:size=14"

selected=$(ps -u "$USER" -o pid,comm --sort=-%cpu | \
           tail -n +2 | \
           dmenu -i -l 20 -p "Kill process:" -fn "$font" -nb "$gray_1" -nf "$gray_3" -sb "$gray_5" -sf "$gray_4" | \
           awk '{print $1}')

if [ -n "$selected" ]; then
    # Try graceful kill first (SIGTERM)
    if kill -15 "$selected" 2>/dev/null; then
        notify-send -t 5000 "✅ Process killed" "PID $selected terminated gracefully"
    else
        # Force kill if needed (SIGKILL)
        if kill -9 "$selected" 2>/dev/null; then
            notify-send -t 5000 "⚠️ Process force killed" "PID $selected (SIGKILL)"
        else
            notify-send -t 5000 "❌ Failed" "Could not kill PID $selected"
        fi
    fi
fi

