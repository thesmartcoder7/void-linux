#!/bin/bash

if ! ps aux | grep -v grep | grep picom > /dev/null; then
	nohup picom > /dev/null 2>&1 &
fi
