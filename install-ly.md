# Ly Login Manager Installation instructions for Void Linux

## Install Dependencies
### Compile-time
- zig 0.17.x (you must use a release version of zig; check that zig version does not have a -dev* suffix)
- libc
- pam
- xcb (optional, required by default; needed for X11 support)

### Runtime (default config)
- xorg
- xorg-xauth
- brightnessctl

## Support
Every environment that works on other login managers also should work on Ly.
- Unlike most login managers Ly has an xinitrc and shell entry.
- If you installed your favorite environment and you don't see it, that's because Ly doesn't automatically refresh itself. To fix this you should restart Ly service (depends on your init system) or the easy way is to reboot your system.
- If your environment is still missing then check at /usr/share/xsessions or /usr/share/wayland-sessions to see if a .desktop file is present.
- If there isn't a .desktop file then create a new one at /etc/ly/custom-sessions that launches your favorite environment. These .desktop files can be only seen by Ly and if you want them system-wide you also can create at those directories instead.
- If Xorg sessions don't work then check if your distro compiles Ly with Xorg.
- Logs are defined by /etc/ly/config.lua or /etc/ly/config.ini:
- The session log is located at ~/.local/state/ly-session.log by default.
- The system log is located at /var/log/ly.log by default.

## Installation and Building

### Cloning the repository
```bash
$ git clone https://codeberg.org/fairyglade/ly.git && cd ly

```
### Building and Setting up ly as a runit service
```bash
sudo zig build installexe -Dinit_system=runit
sudo unlink /var/service/lightdm
sudo ln -s /etc/sv/ly /var/service/
sudo touch /etc/sv/agetty-tty2/down
sudo unlink /var/service/agetty-tty2

```

These commands configure Ly as the login manager on Void Linux, replacing LightDM - if you have it installed, and preventing certain agetty services from interfering with Ly.

Let's go through each line, including why it matters and what happens if you skip it.

1. Explanation of each command
- ` zig build installexe -Dinit_system=runit `

Purpose: Compile and install Ly with support for runit.

zig build — uses the Zig build system to compile the Ly source code.

installexe — builds and installs the Ly executable according to the project's build configuration.

-Dinit_system=runit — configures Ly for the runit init system, which Void Linux uses by default.

Why it matters: Ly needs to integrate correctly with the system's service-management environment. This option tells the build configuration which init system to target.

If Ly is already installed and this command has been run successfully, you generally don't need to repeat it unless rebuilding or reinstalling Ly.


- ` unlink /var/service/lightdm `

Purpose: Remove LightDM from runit's active service directory.

Void Linux uses runit to manage services. The directory /var/service/ contains symlinks to services that runit should supervise.

/var/service/lightdm — the symlink enabling the LightDM service.

unlink — removes that symlink, not the underlying service directory or LightDM package.

Why it matters: LightDM and Ly are both display/login managers. You generally want only one managing the graphical login session.

Removing this symlink disables LightDM as an enabled runit service, but it does not uninstall LightDM.

If LightDM isn't enabled, the command will report an error because the symlink doesn't exist.


- ` ln -s /etc/sv/ly /var/service/ `

Purpose: Enable Ly as a runit service.

This creates the symlink:

/var/service/ly -> /etc/sv/ly

/etc/sv/ly — the Ly service definition, including the instructions runit uses to start and supervise it.

/var/service/ly — the enabled service entry.

Why it matters: Installing the Ly executable does not automatically mean runit will start it. Enabling the service allows runit to manage Ly.

This assumes /etc/sv/ly already exists and is configured correctly. If the link already exists, the command may fail rather than replace it.


- ` touch /etc/sv/agetty-tty2/down `

Purpose: Tell runit not to run the agetty-tty2 service.

touch — creates the down file if it doesn't exist, or updates its timestamp if it does.

/etc/sv/agetty-tty2/down — a runit service-control marker.

agetty provides a text-based login prompt on a virtual terminal (TTY). For example, it normally lets you log in at a console such as tty2.

In runit, a down file inside a service directory tells the service supervisor that the service should remain stopped by default.

Why it matters: Ly may be configured to use a virtual terminal for the graphical login. Disabling the corresponding agetty service can prevent the console login prompt and Ly from competing for that terminal.

Importantly, creating this file does not necessarily stop an already-running service immediately; you may need to restart or otherwise stop the service.


- ` unlink /var/service/agetty-tty2 `

Purpose: Disable the agetty-tty2 service in runit.

This removes the symlink that enables the service:

/var/service/agetty-tty2 -> /etc/sv/agetty-tty2

Why it matters: The previous command marks the service as down, while this command removes its enabled-service link. Together, they prevent runit from automatically starting that particular agetty service.

The agetty service definition itself remains in /etc/sv/agetty-tty2.

Note that removing the symlink alone is generally enough to disable a runit service. The down file is an additional control, not necessarily a requirement for installing Ly.


## Note:
There is an installation script for ly `install-ly.sh` within this repo. Run it with sudo within ly's downloaded/cloned directory

Cheers!
