#ifndef MINLOGIN_CONFIG_H
#define MINLOGIN_CONFIG_H

/* Set this to the existing, non-root account allowed to log in. */
#define MINLOGIN_USER "samuel"
#define BACKGROUND_IMAGE "/home/samuel/.config/void-linux/wallpapers/void-space.png"
#define MINLOGIN_PAM_SERVICE "minlogin"
#define MINLOGIN_DISPLAY ":0"
#define MINLOGIN_ROOT_XAUTHORITY "/run/minlogin/Xauthority"
#define MINLOGIN_USER_XAUTHORITY "/run/minlogin-user.Xauthority"

/* Xft font patterns: family and pixel size can be changed independently. */
#define MINLOGIN_USERNAME_FONT "JetBrainsMono Nerd Font:size=12"
#define MINLOGIN_PASSWORD_FONT "JetBrainsMono Nerd Font:size=12"
#define MINLOGIN_STATUS_FONT "JetBrainsMono Nerd Font:size=12"

/* Xft colors, as #RRGGBB values. */
#define MINLOGIN_USERNAME_COLOR "#e5e7eb"
#define MINLOGIN_PLACEHOLDER_COLOR "#777b85"
#define MINLOGIN_PASSWORD_COLOR "#e5e7eb"
#define MINLOGIN_STATUS_COLOR "#f87171"

#define MAX_PASSWORD 256

#endif
