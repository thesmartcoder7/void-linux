
#!/usr/bin/env bash
set -Eeuo pipefail

# Configure Ly as the login manager on Void Linux (runit).

if [[ $EUID -eq 0 ]]; then
    SUDO=""
else
    SUDO="sudo"
fi

echo "==> Building and installing Ly with runit support..."
zig build installexe -Dinit_system=runit

echo "==> Disabling LightDM..."
if [[ -L /var/service/lightdm ]]; then
    $SUDO unlink /var/service/lightdm
else
    echo "    LightDM service link not found; skipping."
fi

echo "==> Enabling Ly..."
if [[ ! -d /etc/sv/ly ]]; then
    echo "ERROR: /etc/sv/ly does not exist."
    echo "Make sure Ly's runit service definition is installed."
    exit 1
fi

if [[ ! -e /var/service/ly && ! -L /var/service/ly ]]; then
    $SUDO ln -s /etc/sv/ly /var/service/
else
    echo "    Ly is already enabled; skipping."
fi

echo "==> Disabling agetty on tty2..."
if [[ -d /etc/sv/agetty-tty2 ]]; then
    $SUDO touch /etc/sv/agetty-tty2/down
else
    echo "    agetty-tty2 service definition not found; skipping."
fi

if [[ -L /var/service/agetty-tty2 ]]; then
    $SUDO unlink /var/service/agetty-tty2
else
    echo "    agetty-tty2 service link not found; skipping."
fi

echo
echo "==> Ly configuration steps completed."
echo "Check the service with: sudo sv status ly"
echo "A reboot may be needed for the login-manager change to take effect."
