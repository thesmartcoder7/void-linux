#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "util.h"
#include "components.h"
#include "portable.h"

/*
 * Find the first wireless network interface.
 *
 * Linux exposes wireless interfaces as:
 *
 *     /sys/class/net/<interface>/wireless
 *
 * This is distribution independent and works on:
 *
 *     Arch
 *     Debian
 *     Fedora
 *     Void
 *
 * It also handles interfaces such as:
 *
 *     wlan0
 *     wlp2s0
 *     wlp0s20f3
 *     wlx001122334455
 */
static const char *
find_wifi_interface(void)
{
	static char iface[256];
	DIR *dir;
	struct dirent *entry;
	char path[512];

	iface[0] = '\0';

	dir = opendir("/sys/class/net");
	if (!dir)
		return NULL;

	while ((entry = readdir(dir))) {
		if (!strcmp(entry->d_name, ".") ||
		    !strcmp(entry->d_name, ".."))
			continue;

		snprintf(path, sizeof(path),
		         "/sys/class/net/%s/wireless",
		         entry->d_name);

		if (access(path, F_OK) == 0) {
			snprintf(iface, sizeof(iface),
			         "%s", entry->d_name);
			closedir(dir);
			return iface;
		}
	}

	closedir(dir);
	return NULL;
}


/*
 * Find the first battery exposed by the Linux power_supply
 * subsystem.
 *
 * Examples:
 *
 *     BAT0
 *     BAT1
 *
 * Again, this does not depend on the distribution.
 */
static const char *
find_battery(void)
{
	static char battery[256];
	DIR *dir;
	struct dirent *entry;
	char path[512];

	battery[0] = '\0';

	dir = opendir("/sys/class/power_supply");
	if (!dir)
		return NULL;

	while ((entry = readdir(dir))) {
		if (strncmp(entry->d_name, "BAT", 3) != 0)
			continue;

		snprintf(path, sizeof(path),
		         "/sys/class/power_supply/%s/capacity",
		         entry->d_name);

		if (access(path, R_OK) == 0) {
			snprintf(battery, sizeof(battery),
			         "%s", entry->d_name);
			closedir(dir);
			return battery;
		}
	}

	closedir(dir);
	return NULL;
}


/*
 * Portable Wi-Fi percentage.
 *
 * If Wi-Fi exists:
 *
 *     wifi_perc(interface)
 *
 * If Wi-Fi doesn't exist:
 *
 *     unknown_str
 */
const char *
portable_wifi_perc(const char *unused)
{
	const char *iface;

	(void)unused;

	iface = find_wifi_interface();

	if (!iface)
		return unknown_str;

	return wifi_perc(iface);
}


/*
 * Portable Wi-Fi ESSID.
 */
const char *
portable_wifi_essid(const char *unused)
{
	const char *iface;

	(void)unused;

	iface = find_wifi_interface();

	if (!iface)
		return unknown_str;

	return wifi_essid(iface);
}


/*
 * Portable battery percentage.
 */
const char *
portable_battery_perc(const char *unused)
{
	const char *battery;

	(void)unused;

	battery = find_battery();

	if (!battery)
		return unknown_str;

	return battery_perc(battery);
}