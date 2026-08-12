#!/usr/bin/bash

sudo xbps-install -S pipewire pipewire-devel alsa-pipewire wireplumber elogind dbus-elogind

sudo ln -s /etc/sv/elogind /var/service

sudo mkdir -p /etc/alsa/conf.d

