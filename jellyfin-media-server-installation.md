# Installing Jellyfin Media Server on Void Linux

A practical guide to installing and running [Jellyfin Media Server](https://jellyfin.org/) on Void Linux using the generic Linux release and the runit init system.

This guide covers a manual installation rather than Docker. It is intended for users who want to manage Jellyfin as a native service on Void Linux.

> **Important:** The generic Linux archive's compatibility depends on your system's architecture, C library, and available runtime dependencies. Verify compatibility before installing. The commands below assume an `x86_64` system using glibc.

## Table of Contents

- [1. Prerequisites](#1-prerequisites)
- [2. Install Dependencies](#2-install-dependencies)
- [3. Download Jellyfin](#3-download-jellyfin)
- [4. Extract the Archive](#4-extract-the-archive)
- [5. Create a Service Account](#5-create-a-service-account)
- [6. Configure Directories](#6-configure-directories)
- [7. Configure the runit Service](#7-configure-the-runit-service)
- [8. Enable and Start Jellyfin](#8-enable-and-start-jellyfin)
- [9. Access the Web Interface](#9-access-the-web-interface)
- [10. Configure Media Libraries](#10-configure-media-libraries)
- [11. Troubleshooting](#11-troubleshooting)
- [12. Updating Jellyfin](#12-updating-jellyfin)
- [References](#references)

## 1. Prerequisites

Before beginning, confirm your architecture and C library:

```bash
uname -m
ldd --version 2>&1 | head -n 2
```

For this guide, the expected architecture is `x86_64` and the system should use glibc.

If your system uses musl, verify that the selected Jellyfin release and its dependencies support your environment before proceeding.

You will also need:

- Root or `sudo` privileges.
- An internet connection.
- Sufficient disk space for Jellyfin and its metadata.
- A directory containing your media files.
- The `runit` service manager, which is the default init system on Void Linux.

## 2. Install Dependencies

Synchronize the Void package repositories and install the required utilities:

```bash
sudo xbps-install -S
sudo xbps-install -y curl ffmpeg
```

`curl` is useful for downloading files, while FFmpeg provides media-processing capabilities.

Verify that FFmpeg is available:

```bash
ffmpeg -version
```

Check whether Jellyfin is available in your configured repositories before using the manual installation method:

```bash
xbps-query -Rs jellyfin
```

If an appropriate package is available, consider using it instead of maintaining a manual installation. Otherwise, continue with the generic Linux release.

## 3. Download Jellyfin

Visit the official Jellyfin server download page:

https://jellyfin.org/downloads/server/

Select the **Stable** release and locate the generic Linux archive that matches your architecture and runtime.

For a compatible x86-64 glibc system, this will generally be the `amd64` archive.

Download the archive to your Downloads directory. The exact filename will depend on the current release.

Verify that the archive exists:

```bash
find ~/Downloads -maxdepth 1 -type f -iname '*jellyfin*'
```

Set a shell variable to the exact archive path. Replace the example filename with the file you downloaded:

```bash
ARCHIVE="$HOME/Downloads/jellyfin_VERSION-amd64.tar.gz"
```

Confirm the file exists before continuing:

```bash
test -f "$ARCHIVE" && echo "Archive found" || echo "Archive not found"
```

## 4. Extract the Archive

Create the installation directory:

```bash
sudo mkdir -p /opt/jellyfin
```

Inspect the archive contents before extracting:

```bash
tar -tzf "$ARCHIVE" | head -n 30
```

Extract the archive:

```bash
sudo tar -xzf "$ARCHIVE" -C /opt/jellyfin
```

Locate the Jellyfin executable:

```bash
sudo find /opt/jellyfin -type f -name jellyfin -perm /111
```

Record the full path to the executable. The archive may extract into a version-specific directory, so do not assume the executable is directly inside `/opt/jellyfin`.

For the remaining steps, use the actual path returned by the command.

Check that the executable and its required shared libraries are available:

```bash
file /path/to/jellyfin
ldd /path/to/jellyfin
```

Replace `/path/to/jellyfin` with the executable's real path.

If `ldd` reports missing libraries, resolve those dependencies before starting the server.

## 5. Create a Service Account

Run Jellyfin under its own unprivileged account rather than root.

Create a system account if it does not already exist:

```bash
id jellyfin >/dev/null 2>&1 || \
    sudo useradd -r -s /bin/false jellyfin
```

Confirm that the account exists:

```bash
id jellyfin
```

This account will be used by runit to launch Jellyfin.

## 6. Configure Directories

Create separate directories for Jellyfin's application data, cache, configuration, and logs:

```bash
sudo mkdir -p \
    /var/lib/jellyfin \
    /var/cache/jellyfin \
    /etc/jellyfin \
    /var/log/jellyfin
```

Assign ownership to the Jellyfin account:

```bash
sudo chown -R jellyfin:jellyfin \
    /var/lib/jellyfin \
    /var/cache/jellyfin \
    /etc/jellyfin \
    /var/log/jellyfin
```

Keep the application binaries under administrative ownership. The Jellyfin account needs read and execute access to the application files, not ownership of the installation.

## 7. Configure the runit Service

Void Linux uses runit rather than systemd. A runit service is defined by a directory containing an executable `run` script.

Create the service directory:

```bash
sudo mkdir -p /etc/sv/jellyfin
```

Create the service script:

```bash
sudo nano /etc/sv/jellyfin/run
```

Add the following:

```bash
#!/bin/sh

exec 2>&1

exec chpst -u jellyfin:jellyfin \
    /path/to/jellyfin \
    --datadir /var/lib/jellyfin \
    --cachedir /var/cache/jellyfin \
    --configdir /etc/jellyfin \
    --logdir /var/log/jellyfin \
    --ffmpeg /usr/bin/ffmpeg
```

Replace `/path/to/jellyfin` with the actual executable path discovered in Section 4.

The script uses Jellyfin's command-line options to separate its writable data from its installed program files.

The `chpst` utility is provided by Void's runit infrastructure. It runs Jellyfin as the dedicated service account.

Save the file and make it executable:

```bash
sudo chmod +x /etc/sv/jellyfin/run
```

Check the script for syntax errors:

```bash
sudo sh -n /etc/sv/jellyfin/run
```

No output indicates that the shell syntax check passed.

## 8. Enable and Start Jellyfin

Enable the service by creating a symlink in Void's active service directory:

```bash
sudo ln -s /etc/sv/jellyfin /var/service/jellyfin
```

If the symlink already exists, inspect it rather than creating another one.

Check the service status:

```bash
sudo sv status jellyfin
```

If it is not running, inspect the service logs and check the executable path and directory permissions.

To stop or restart Jellyfin:

```bash
sudo sv down jellyfin
sudo sv up jellyfin
```

The active symlink allows runit to supervise the service and start it automatically during normal system startup.

## 9. Access the Web Interface

Once Jellyfin is running, open the following address on the server:

http://127.0.0.1:8096

From another device on the same network, use the server's LAN IP address:

```text
http://SERVER-IP:8096
```

Replace `SERVER-IP` with the actual IP address of your Void machine.

The first-run wizard will guide you through:

1. Selecting the display language.
2. Creating an administrator account.
3. Adding media libraries.
4. Choosing metadata and library options.
5. Completing the initial setup.

The default HTTP port is `8096`. Do not expose the server directly to the public internet without an appropriate security configuration.

## 10. Configure Media Libraries

Jellyfin must be able to read the directories containing your media.

For example, if your movies are stored in `/media/Movies`, inspect the directory permissions:

```bash
ls -ld /media /media/Movies
namei -l /media/Movies
```

Test whether the service account can access the directory:

```bash
sudo -u jellyfin test -r /media/Movies \
    && echo "Readable" \
    || echo "Access denied"
```

Repeat this check for each library path.

Do not recursively change ownership of an entire media drive just to make Jellyfin work. Prefer appropriate group membership, directory permissions, or ACLs that grant the service account read access without interfering with your normal user account or other media services.

In the Jellyfin web interface, add each accessible directory to the appropriate library type, such as Movies or TV Shows.

## 11. Troubleshooting

### The service will not start

Check the service status:

```bash
sudo sv status jellyfin
```

Check the service log:

```bash
sudo tail -n 100 /var/log/jellyfin/*
```

Also verify that the executable exists and is accessible:

```bash
sudo -u jellyfin test -x /path/to/jellyfin \
    && echo "Executable accessible" \
    || echo "Executable unavailable"
```

Replace the example path with the actual executable path.

### Missing shared libraries

Run:

```bash
ldd /path/to/jellyfin
```

Investigate any missing dependencies reported by the command. Confirm that the downloaded archive matches your architecture and C library.

### The web interface does not load

Check whether Jellyfin is listening on its default port:

```bash
sudo ss -ltnp | grep ':8096'
```

If no listener appears, check the service status and logs.

If Jellyfin works locally but not from another device, check the server's IP address, network connectivity, and firewall configuration.

### Media files are not visible

Test directory access as the Jellyfin user:

```bash
sudo -u jellyfin test -r /path/to/media \
    && echo "Media directory readable" \
    || echo "Media directory not readable"
```

Check parent-directory permissions, mount status, and any group or ACL restrictions.

### FFmpeg cannot be found

Confirm its location:

```bash
command -v ffmpeg
```

If it is not located at `/usr/bin/ffmpeg`, update the `--ffmpeg` argument in the runit service script to the correct path.

## 12. Updating Jellyfin

For a manual installation, updates are not necessarily handled by `xbps`. Check the official download page for a newer stable release.

Before upgrading:

1. Stop the service.
2. Back up Jellyfin's data and configuration directories.
3. Download the new compatible archive.
4. Extract the new release and verify its dependencies.
5. Update the service's executable path if the extracted directory changes.
6. Restart the service and verify its status.

Example backup command:

```bash
sudo tar -czf \
    "$HOME/jellyfin-backup-$(date +%F).tar.gz" \
    /var/lib/jellyfin \
    /etc/jellyfin
```

The backup destination must be writable by the account running `tar`. If necessary, create the archive in a root-writable location and move it into your home directory afterward.

Keep a copy of the previous working installation until the upgrade has been verified.

## References

- [Jellyfin — Official Website](https://jellyfin.org/)
- [Jellyfin — Server Downloads](https://jellyfin.org/downloads/server/)
- [Jellyfin — Official Documentation](https://jellyfin.org/docs/)
- [Void Linux — Handbook](https://docs.voidlinux.org/)
- [Void Linux — Services and runit](https://docs.voidlinux.org/config/services/index.html)

---

**Note:** This is a manual installation procedure, not a Void Linux package recipe. Verify the current Jellyfin archive layout, runtime requirements, and command-line options against the release you download. The runit service must reference the actual executable path on your system.

