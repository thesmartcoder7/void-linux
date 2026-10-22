# minlogin — experimental Void Linux/runit X11 login manager

This version upgrades the multi-monitor greeter to authenticate with PAM, open a PAM session, launch the configured user's `~/.xinitrc` as that unprivileged user, wait for the session to end, close the PAM session, and return to the greeter. It starts a dedicated Xorg server on `:0`/VT 7 from a root-owned runit service. It does **not** use `startx`: `.xinitrc` is executed directly against the service-managed X server, because running `startx` would start a second X server.

**Experimental security warning:** this is a small custom login manager and has not been audited or tested on your machine. Do not replace Ly or enable it on your only working login path until you have tested it from a spare TTY and verified you can recover through another TTY/live USB. It assumes one fixed, non-root user, one X screen (`:0`), Xorg, and Void's runit. It is not a multi-seat manager. Do not install the binary setuid; only the runit service should start it as root. The root greeter owns the X server, so treat this as an experimental personal system project, not production-grade security software.

## Dependencies

```sh
sudo xbps-install -S base-devel libX11-devel libXrandr-devel libXft-devel fontconfig-devel freetype-devel imlib2-devel linux-pam-devel xorg-server xauth util-linux
```

`mcookie` is supplied by util-linux on typical Void installations. Ensure `Xorg`, `xauth`, `mcookie`, and `dwm` are installed.

## Configure and build

Edit `config.h` and set `MINLOGIN_USER`, `BACKGROUND_IMAGE`, and paths if needed. Keep `MINLOGIN_USER` in sync with `USER_NAME` in `runit/minlogin/run`.

```sh
make clean
make
sudo make install
```

Create a blurred wallpaper (optional):

```sh
mkdir -p ~/.config/minlogin
magick /path/to/wallpaper.jpg -resize '1920x1080^' -gravity center \
  -extent 1920x1080 -blur 0x18 ~/.config/minlogin/background.jpg
```

The image is independently cover-cropped on each monitor; it is not stretched across the combined desktop.

## PAM policy

Inspect the existing policy first:

```sh
sudo cat /etc/pam.d/login
```

If `/etc/pam.d/login` exists and its include stack is appropriate for a local graphical login, install the example as `/etc/pam.d/minlogin`:

```sh
sudo install -m 0644 pam-minlogin.example /etc/pam.d/minlogin
```

PAM policies vary by Void installation. Do not blindly replace an existing policy; if the login policy uses site-specific modules or different include files, adapt the four stacks to match the working local-login policy. Ensure the account stack permits your user and the session stack performs the system's required setup.

## Configure `.xinitrc`

Your `/home/USER/.xinitrc` should start the session and finish when DWM exits. For example:

```sh
#!/bin/sh
# Optional environment setup goes here.
exec dwm
```

The service sets `DISPLAY=:0` and `XAUTHORITY=/run/minlogin-user.Xauthority` for the user process. The file is generated with the same temporary X cookie used by the root-owned X server and is removed when the service stops.

## Install the runit service (do not enable until tested)

Review `runit/minlogin/run` and edit `USER_NAME` to match `config.h`. Then:

```sh
sudo mkdir -p /etc/sv/minlogin
sudo install -m 0755 runit/minlogin/run /etc/sv/minlogin/run
sudo install -m 0755 runit/minlogin/finish /etc/sv/minlogin/finish
```

Do **not** link `/var/service/minlogin` yet. Keep Ly enabled and use a spare TTY/maintenance window for the initial service test; two display managers must not own the same VT or display. Before a real replacement, stop Ly and any getty assigned to VT 7, then enable minlogin:

```sh
sudo sv down ly
# Check actual service names on this machine before disabling a getty.
# sudo unlink /var/service/ly
sudo ln -s /etc/sv/minlogin /var/service/minlogin
```

To roll back from a TTY:

```sh
sudo unlink /var/service/minlogin
sudo sv down minlogin 2>/dev/null || true
sudo ln -s /etc/sv/ly /var/service/ly
sudo sv up ly
```

Void service names and PAM policy can vary. Keep a working TTY/getty and do not remove Ly's service directory until recovery has been tested.

## Behavior and current limitations

- Multi-monitor greeter windows share one password buffer; the same wallpaper is independently cropped per monitor.
- PAM authentication and account checks precede session creation.
- A supervisor calls `pam_setcred`, `pam_open_session`, waits for the user session, then calls `pam_close_session`, deletes credentials, and ends its PAM handle.
- The user session is launched by `/bin/sh /home/USER/.xinitrc` after dropping supplementary groups/GID/UID and setting a clean environment.
- A session that exits returns to the greeter. If the greeter/server process exits, runit restarts the service.
- No UTF-8/XIM input, multiple users, seat management, power controls, or security audit yet.
- The run script assumes Xorg creates `/tmp/.X11-unix/X0` and VT 7 is available. Confirm those assumptions on your machine before use.

## Visual preview (no installation, no PAM, no login)

Build the project, then from a terminal inside your existing X11 session run:

```sh
make
./minlogin --preview
```

Preview mode runs as your regular user on the current `$DISPLAY`, shows the
multi-monitor greeter, and never calls PAM or launches `.xinitrc`. Press `Esc`
to close the preview. `Enter` only displays a preview-only message. It does not
replace Ly or change any service configuration. If no wallpaper is available at
`BACKGROUND_IMAGE`, the greeter uses a plain dark background.

## Xft font customization

Text is rendered with Xft/Fontconfig. Edit `config.h` and change these settings:

```c
#define MINLOGIN_USERNAME_FONT "JetBrains Mono:size=20"
#define MINLOGIN_PASSWORD_FONT "JetBrains Mono:size=18"
#define MINLOGIN_STATUS_FONT "JetBrains Mono:size=12"
```

The `size` is in points. Use a family installed on the system; check available family names with `fc-list : family | sort -u`. Example: `"monospace:size=18"`. Rebuild with `make clean && make` after editing. Text colors are separately configurable in the `MINLOGIN_*_COLOR` definitions below those font settings.
