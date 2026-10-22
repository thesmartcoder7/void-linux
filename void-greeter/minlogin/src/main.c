
#define _POSIX_C_SOURCE 200809L

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xft/Xft.h>
#include <X11/keysym.h>
#include <X11/extensions/Xrandr.h>
#include <Imlib2.h>
#include <security/pam_appl.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>

#include "../config.h"

#define FIELD_WIDTH 300
#define FIELD_HEIGHT 40
#define MAX_MESSAGE 128
#define XAUTH_COPY_BUFFER 4096

typedef struct {
    Window window;
    int x;
    int y;
    int width;
    int height;
    int primary;
} MonitorWindow;

/* Greeter state shared by all monitor windows. */
static const char *username = MINLOGIN_USER;
static char password[MAX_PASSWORD];
static size_t password_len;
static char message[MAX_MESSAGE];

static MonitorWindow *monitors;
static int monitor_count;
static int active_monitor;

static Display *dpy;
static GC gc;

static XftFont *font_username;
static XftFont *font_password;
static XftFont *font_status;

static XftColor color_username;
static XftColor color_placeholder;
static XftColor color_password;
static XftColor color_status;

static int color_username_allocated;
static int color_placeholder_allocated;
static int color_password_allocated;
static int color_status_allocated;

static Imlib_Image original_background;
static int preview_mode;

/* ------------------------------------------------------------------------- */
/* PAM conversation                                                          */
/* ------------------------------------------------------------------------- */

static int pam_conversation(
    int num_msg,
    const struct pam_message **msg,
    struct pam_response **resp,
    void *appdata_ptr
)
{
    (void)appdata_ptr;

    if (num_msg <= 0 || !msg || !resp)
        return PAM_CONV_ERR;

    struct pam_response *responses =
        calloc((size_t)num_msg, sizeof(*responses));

    if (!responses)
        return PAM_BUF_ERR;

    for (int i = 0; i < num_msg; i++) {
        if (!msg[i])
            goto fail;

        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
            responses[i].resp = strdup(password);
            if (!responses[i].resp)
                goto fail;
            break;

        case PAM_PROMPT_ECHO_ON:
            responses[i].resp = strdup(username);
            if (!responses[i].resp)
                goto fail;
            break;

        case PAM_ERROR_MSG:
        case PAM_TEXT_INFO:
            responses[i].resp = NULL;
            break;

        default:
            goto fail;
        }
    }

    *resp = responses;
    return PAM_SUCCESS;

fail:
    for (int i = 0; i < num_msg; i++) {
        if (responses[i].resp) {
            memset(responses[i].resp, 0,
                   strlen(responses[i].resp));
            free(responses[i].resp);
        }
    }

    free(responses);
    return PAM_CONV_ERR;
}

/* ------------------------------------------------------------------------- */
/* Xauthority handling                                                       */
/* ------------------------------------------------------------------------- */

/*
 * Copy the X server's authority cookie to a file readable by the session
 * user. The parent directory of the destination should be root-controlled.
 *
 * O_EXCL prevents following or replacing a pre-existing destination. The
 * runit service removes stale files when it starts; this function deliberately
 * does not unlink a destination that it did not create itself.
 */
static int create_user_xauthority(uid_t uid, gid_t gid)
{
    const char *source = MINLOGIN_ROOT_XAUTHORITY;
    const char *destination = MINLOGIN_USER_XAUTHORITY;

    char buffer[XAUTH_COPY_BUFFER];
    int in = -1;
    int out = -1;
    int created = 0;
    int ok = 0;

    in = open(source, O_RDONLY | O_CLOEXEC);
    if (in < 0)
        goto done;

    out = open(
        destination,
        O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC | O_NOFOLLOW,
        0600
    );

    if (out < 0)
        goto done;

    created = 1;

    for (;;) {
        ssize_t n = read(in, buffer, sizeof(buffer));

        if (n == 0)
            break;

        if (n < 0) {
            if (errno == EINTR)
                continue;
            goto done;
        }

        ssize_t offset = 0;

        while (offset < n) {
            ssize_t written = write(
                out,
                buffer + offset,
                (size_t)(n - offset)
            );

            if (written < 0) {
                if (errno == EINTR)
                    continue;
                goto done;
            }

            if (written == 0)
                goto done;

            offset += written;
        }
    }

    if (fchown(out, uid, gid) != 0)
        goto done;

    if (fchmod(out, 0600) != 0)
        goto done;

    if (fsync(out) != 0)
        goto done;

    ok = 1;

done:
    if (in >= 0)
        close(in);

    if (out >= 0 && close(out) != 0)
        ok = 0;

    memset(buffer, 0, sizeof(buffer));

    if (!ok && created)
        unlink(destination);

    return ok;
}

/* ------------------------------------------------------------------------- */
/* Session launching                                                         */
/* ------------------------------------------------------------------------- */

/*
 * Authenticate the configured user using PAM.
 *
 * A privileged supervisor opens and closes the PAM session. Its child drops
 * privileges and runs the user's ~/.xinitrc on the already-running X server.
 * The greeter waits for the session to end, then resumes accepting passwords.
 */
static int authenticate_and_launch(void)
{
    struct passwd *pw = getpwnam(username);

    if (!pw || pw->pw_uid == 0) {
        snprintf(message, sizeof(message),
                 "Configured user is invalid");
        return 0;
    }

    /*
     * Copy account data because NSS-owned storage can be reused by later
     * account lookups, including lookups performed by PAM modules.
     */
    struct passwd target = *pw;

    char target_name[256];
    char target_home[PATH_MAX];
    char target_shell[PATH_MAX];

    int name_len = snprintf(
        target_name, sizeof(target_name), "%s", pw->pw_name
    );

    int home_len = snprintf(
        target_home, sizeof(target_home), "%s", pw->pw_dir
    );

    const char *shell =
        (pw->pw_shell && pw->pw_shell[0])
            ? pw->pw_shell
            : "/bin/sh";

    int shell_len = snprintf(
        target_shell, sizeof(target_shell), "%s", shell
    );

    if (name_len < 0 || name_len >= (int)sizeof(target_name) ||
        home_len < 0 || home_len >= (int)sizeof(target_home) ||
        shell_len < 0 || shell_len >= (int)sizeof(target_shell)) {
        snprintf(message, sizeof(message),
                 "Configured account fields are too long");
        return 0;
    }

    target.pw_name = target_name;
    target.pw_dir = target_home;
    target.pw_shell = target_shell;

    struct pam_conv conv = {
        .conv = pam_conversation,
        .appdata_ptr = NULL
    };

    pam_handle_t *pamh = NULL;

    int result = pam_start(
        MINLOGIN_PAM_SERVICE,
        target.pw_name,
        &conv,
        &pamh
    );

    if (result == PAM_SUCCESS)
        result = pam_authenticate(pamh, 0);

    if (result == PAM_SUCCESS)
        result = pam_acct_mgmt(pamh, 0);

    if (result != PAM_SUCCESS) {
        if (pamh)
            pam_end(pamh, result);

        return 0;
    }

    pid_t supervisor = fork();

    if (supervisor < 0) {
        pam_end(pamh, PAM_SYSTEM_ERR);
        snprintf(message, sizeof(message),
                 "Could not start session");
        return 0;
    }

    if (supervisor == 0) {
        /*
         * This process supervises PAM only. Do not retain the greeter's
         * Xlib connection in the session supervisor.
         */
        XCloseDisplay(dpy);

        int child_result = pam_setcred(pamh, PAM_ESTABLISH_CRED);
        int credentials_established = (child_result == PAM_SUCCESS);

        if (child_result == PAM_SUCCESS)
            child_result = pam_open_session(pamh, 0);

        if (child_result != PAM_SUCCESS) {
            if (credentials_established)
                pam_setcred(pamh, PAM_DELETE_CRED);

            pam_end(pamh, child_result);
            _exit(1);
        }

        /*
         * Make the authority file available only after authentication and
         * successful PAM session setup.
         */
        if (!create_user_xauthority(target.pw_uid, target.pw_gid)) {
            pam_close_session(pamh, 0);
            pam_setcred(pamh, PAM_DELETE_CRED);
            pam_end(pamh, PAM_SYSTEM_ERR);
            _exit(1);
        }

        pid_t session = fork();

        if (session == 0) {
            /*
             * Read PAM-provided environment variables before clearing the
             * inherited root environment.
             */
            char **pam_env = pam_getenvlist(pamh);

            if (clearenv() != 0)
                _exit(126);

            if (setenv("HOME", target.pw_dir, 1) != 0 ||
                setenv("USER", target.pw_name, 1) != 0 ||
                setenv("LOGNAME", target.pw_name, 1) != 0 ||
                setenv("SHELL", target.pw_shell, 1) != 0 ||
                setenv("DISPLAY", MINLOGIN_DISPLAY, 1) != 0 ||
                setenv("XAUTHORITY", MINLOGIN_USER_XAUTHORITY, 1) != 0 ||
                setenv("XDG_SESSION_TYPE", "x11", 1) != 0 ||
                setenv("PATH", "/usr/local/bin:/usr/bin:/bin", 1) != 0) {
                _exit(126);
            }

            if (pam_env) {
                for (char **e = pam_env; *e; e++) {
                    char *eq = strchr(*e, '=');

                    if (eq && eq != *e) {
                        size_t key_len = (size_t)(eq - *e);
                        char key[256];

                        if (key_len < sizeof(key)) {
                            memcpy(key, *e, key_len);
                            key[key_len] = '\0';

                            /*
                             * Identity and X connection settings must not
                             * be overridden by PAM environment entries.
                             */
                            if (strcmp(key, "HOME") != 0 &&
                                strcmp(key, "USER") != 0 &&
                                strcmp(key, "LOGNAME") != 0 &&
                                strcmp(key, "SHELL") != 0 &&
                                strcmp(key, "DISPLAY") != 0 &&
                                strcmp(key, "XAUTHORITY") != 0) {
                                setenv(key, eq + 1, 1);
                            }
                        }
                    }

                    free(*e);
                }

                free(pam_env);
            }

            /*
             * Drop supplementary groups, primary group, and user privileges
             * before executing any user-controlled shell configuration.
             */
            if (initgroups(target.pw_name, target.pw_gid) != 0 ||
                setgid(target.pw_gid) != 0 ||
                setuid(target.pw_uid) != 0 ||
                chdir(target.pw_dir) != 0) {
                _exit(126);
            }

            char xinitrc[PATH_MAX];

            int rc = snprintf(
                xinitrc,
                sizeof(xinitrc),
                "%s/.xinitrc",
                target.pw_dir
            );

            if (rc < 0 || rc >= (int)sizeof(xinitrc))
                _exit(126);

            execl("/bin/sh", "sh", xinitrc, (char *)NULL);
            _exit(127);
        }

        int status = 1;

        if (session > 0) {
            while (waitpid(session, &status, 0) < 0) {
                if (errno == EINTR)
                    continue;

                status = 1;
                break;
            }
        }

        /*
         * The session is over. Remove the user's authority file before
         * closing the PAM session and returning to the greeter.
         */
        unlink(MINLOGIN_USER_XAUTHORITY);

        pam_close_session(pamh, 0);
        pam_setcred(pamh, PAM_DELETE_CRED);
        pam_end(pamh, PAM_SUCCESS);

        _exit(
            session > 0 &&
            WIFEXITED(status) &&
            WEXITSTATUS(status) == 0
                ? 0
                : 1
        );
    }

    int status = 1;

    while (waitpid(supervisor, &status, 0) < 0) {
        if (errno == EINTR)
            continue;

        status = 1;
        break;
    }

    /*
     * The parent still owns its copy of the PAM handle. The supervisor
     * manages the PAM session using its own post-fork copy.
     */
    pam_end(pamh, PAM_SUCCESS);

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

/* ------------------------------------------------------------------------- */
/* Monitor rendering                                                         */
/* ------------------------------------------------------------------------- */

static void draw_monitor(int index)
{
    MonitorWindow *m = &monitors[index];

    if (m->width <= 0 || m->height <= 0)
        return;

    int fw = FIELD_WIDTH;

    if (fw > m->width - 40)
        fw = m->width - 40;

    if (fw < 1)
        fw = m->width;

    const int fh = FIELD_HEIGHT;
    const int fx = (m->width - fw) / 2;
    const int fy = (m->height - fh) / 2;

    const int baseline =
        fy + (fh + font_password->ascent -
              font_password->descent) / 2;

    int screen = DefaultScreen(dpy);

    XftDraw *xftdraw = XftDrawCreate(
        dpy,
        m->window,
        DefaultVisual(dpy, screen),
        DefaultColormap(dpy, screen)
    );

    if (!xftdraw)
        return;

    /* Base fill, visible if no wallpaper can be loaded. */
    XSetForeground(dpy, gc, 0x08090b);

    XFillRectangle(
        dpy, m->window, gc, 0, 0,
        (unsigned int)m->width,
        (unsigned int)m->height
    );

    /*
     * Cover-and-crop the wallpaper independently for each monitor.
     * The image is not stretched across the combined desktop.
     */
    if (original_background) {
        imlib_context_set_image(original_background);

        int iw = imlib_image_get_width();
        int ih = imlib_image_get_height();

        if (iw > 0 && ih > 0) {
            int crop_w;
            int crop_h;
            int crop_x;
            int crop_y;

            if ((double)iw / (double)ih >
                (double)m->width / (double)m->height) {
                crop_h = ih;
                crop_w = (int)(
                    (double)ih * m->width / m->height
                );
                crop_x = (iw - crop_w) / 2;
                crop_y = 0;
            } else {
                crop_w = iw;
                crop_h = (int)(
                    (double)iw * m->height / m->width
                );
                crop_x = 0;
                crop_y = (ih - crop_h) / 2;
            }

            if (crop_w > 0 && crop_h > 0) {
                Imlib_Image cropped =
                    imlib_create_cropped_scaled_image(
                        crop_x,
                        crop_y,
                        crop_w,
                        crop_h,
                        m->width,
                        m->height
                    );

                if (cropped) {
                    imlib_context_set_display(dpy);
                    imlib_context_set_visual(
                        DefaultVisual(dpy, screen)
                    );
                    imlib_context_set_colormap(
                        DefaultColormap(dpy, screen)
                    );
                    imlib_context_set_drawable(m->window);
                    imlib_context_set_image(cropped);

                    imlib_render_image_on_drawable(0, 0);

                    imlib_context_set_image(cropped);
                    imlib_free_image();

                    imlib_context_set_image(original_background);
                }
            }
        }
    }

    XGlyphInfo ext;

    /* Username, positioned above the password field. */
    XftTextExtents8(
        dpy,
        font_username,
        (const FcChar8 *)username,
        (int)strlen(username),
        &ext
    );

    XftDrawString8(
        xftdraw,
        &color_username,
        font_username,
        (m->width - (int)ext.xOff) / 2,
        fy - 38,
        (const FcChar8 *)username,
        (int)strlen(username)
    );

    /* Password field and outline. */
    XSetForeground(dpy, gc, 0x17191e);

    XFillRectangle(
        dpy, m->window, gc,
        fx, fy,
        (unsigned int)fw,
        (unsigned int)fh
    );

    XSetForeground(dpy, gc, 0x656a73);

    XDrawRectangle(
        dpy, m->window, gc,
        fx, fy,
        (unsigned int)fw,
        (unsigned int)fh
    );

    if (password_len == 0) {
        const char *placeholder = "password";

        XftTextExtents8(
            dpy,
            font_password,
            (const FcChar8 *)placeholder,
            (int)strlen(placeholder),
            &ext
        );

        XftDrawString8(
            xftdraw,
            &color_placeholder,
            font_password,
            (m->width - (int)ext.xOff) / 2,
            baseline,
            (const FcChar8 *)placeholder,
            (int)strlen(placeholder)
        );

        /* Shared cursor position on each monitor. */
        XSetForeground(dpy, gc, 0xe5e7eb);

        XDrawLine(
            dpy, m->window, gc,
            m->width / 2,
            baseline - font_password->ascent,
            m->width / 2,
            baseline + font_password->descent
        );
    } else {
        char masked[MAX_PASSWORD];

        size_t n = password_len;

        if (n >= sizeof(masked))
            n = sizeof(masked) - 1;

        memset(masked, '*', n);
        masked[n] = '\0';

        XftTextExtents8(
            dpy,
            font_password,
            (const FcChar8 *)masked,
            (int)n,
            &ext
        );

        int tx = (m->width - (int)ext.xOff) / 2;

        XftDrawString8(
            xftdraw,
            &color_password,
            font_password,
            tx,
            baseline,
            (const FcChar8 *)masked,
            (int)n
        );

        XSetForeground(dpy, gc, 0xe5e7eb);

        XDrawLine(
            dpy, m->window, gc,
            tx + (int)ext.xOff,
            baseline - font_password->ascent,
            tx + (int)ext.xOff,
            baseline + font_password->descent
        );

        memset(masked, 0, sizeof(masked));
    }

    if (message[0]) {
        XftTextExtents8(
            dpy,
            font_status,
            (const FcChar8 *)message,
            (int)strlen(message),
            &ext
        );

        XftDrawString8(
            xftdraw,
            &color_status,
            font_status,
            (m->width - (int)ext.xOff) / 2,
            fy + fh + 30,
            (const FcChar8 *)message,
            (int)strlen(message)
        );
    }

    XftDrawDestroy(xftdraw);
}

static void redraw_all(void)
{
    for (int i = 0; i < monitor_count; i++)
        draw_monitor(i);

    XFlush(dpy);
}

static int find_monitor(Window window)
{
    for (int i = 0; i < monitor_count; i++) {
        if (monitors[i].window == window)
            return i;
    }

    return -1;
}

/* ------------------------------------------------------------------------- */
/* Multi-monitor setup                                                       */
/* ------------------------------------------------------------------------- */

static int create_monitor_windows(void)
{
    Window root = RootWindow(dpy, DefaultScreen(dpy));

    int n = 0;
    XRRMonitorInfo *rr = XRRGetMonitors(dpy, root, True, &n);

    if (!rr || n < 1) {
        if (rr)
            XRRFreeMonitors(rr);

        monitor_count = 1;
        monitors = calloc(1, sizeof(*monitors));

        if (!monitors)
            return 0;

        monitors[0] = (MonitorWindow) {
            .x = 0,
            .y = 0,
            .width = DisplayWidth(dpy, DefaultScreen(dpy)),
            .height = DisplayHeight(dpy, DefaultScreen(dpy)),
            .primary = 1
        };
    } else {
        monitor_count = n;
        monitors = calloc((size_t)n, sizeof(*monitors));

        if (!monitors) {
            XRRFreeMonitors(rr);
            return 0;
        }

        for (int i = 0; i < n; i++) {
            monitors[i].x = rr[i].x;
            monitors[i].y = rr[i].y;
            monitors[i].width = rr[i].width;
            monitors[i].height = rr[i].height;
            monitors[i].primary = rr[i].primary;
        }

        XRRFreeMonitors(rr);
    }

    Window primary_window = None;

    XSetWindowAttributes attrs = {
        .override_redirect = True,
        .event_mask = ExposureMask | KeyPressMask | ButtonPressMask
    };

    for (int i = 0; i < monitor_count; i++) {
        MonitorWindow *m = &monitors[i];

        m->window = XCreateWindow(
            dpy,
            root,
            m->x,
            m->y,
            (unsigned int)m->width,
            (unsigned int)m->height,
            0,
            CopyFromParent,
            InputOutput,
            CopyFromParent,
            CWOverrideRedirect | CWEventMask,
            &attrs
        );

        if (m->window == None)
            return 0;

        XStoreName(dpy, m->window, "minlogin");
        XMapRaised(dpy, m->window);

        if (m->primary) {
            active_monitor = i;
            primary_window = m->window;
        }
    }

    if (primary_window == None) {
        active_monitor = 0;
        primary_window = monitors[0].window;
    }

    XRaiseWindow(dpy, primary_window);

    XSetInputFocus(
        dpy,
        primary_window,
        RevertToParent,
        CurrentTime
    );

    return 1;
}

/* ------------------------------------------------------------------------- */
/* Cleanup                                                                   */
/* ------------------------------------------------------------------------- */

static void cleanup(void)
{
    memset(password, 0, sizeof(password));
    password_len = 0;

    if (original_background) {
        imlib_context_set_image(original_background);
        imlib_free_image();
        original_background = NULL;
    }

    if (dpy) {
        for (int i = 0; i < monitor_count; i++) {
            if (monitors && monitors[i].window)
                XDestroyWindow(dpy, monitors[i].window);
        }

        int screen = DefaultScreen(dpy);
        Visual *visual = DefaultVisual(dpy, screen);
        Colormap cmap = DefaultColormap(dpy, screen);

        if (color_username_allocated)
            XftColorFree(dpy, visual, cmap, &color_username);

        if (color_placeholder_allocated)
            XftColorFree(dpy, visual, cmap, &color_placeholder);

        if (color_password_allocated)
            XftColorFree(dpy, visual, cmap, &color_password);

        if (color_status_allocated)
            XftColorFree(dpy, visual, cmap, &color_status);

        if (font_username)
            XftFontClose(dpy, font_username);

        if (font_password)
            XftFontClose(dpy, font_password);

        if (font_status)
            XftFontClose(dpy, font_status);

        if (gc)
            XFreeGC(dpy, gc);

        XCloseDisplay(dpy);
        dpy = NULL;
    }

    free(monitors);
    monitors = NULL;
    monitor_count = 0;
}

/* ------------------------------------------------------------------------- */
/* Entry point                                                               */
/* ------------------------------------------------------------------------- */

int main(int argc, char **argv)
{
    int exit_code = EXIT_FAILURE;

    if (argc == 2 && strcmp(argv[1], "--preview") == 0) {
        preview_mode = 1;
    } else if (argc != 1) {
        fprintf(stderr, "Usage: %s [--preview]\n", argv[0]);
        return EXIT_FAILURE;
    }

    /*
     * Preview mode runs as the current user and never authenticates or
     * launches a session. Normal mode is intended for the root-owned
     * runit service, not manual use inside an existing X session.
     */
    if (!preview_mode && geteuid() != 0) {
        fprintf(
            stderr,
            "minlogin must be launched by its root-owned runit service\n"
        );
        return EXIT_FAILURE;
    }

    if (!preview_mode) {
        if (!getenv("DISPLAY") &&
            setenv("DISPLAY", MINLOGIN_DISPLAY, 1) != 0) {
            perror("minlogin: setenv DISPLAY");
            return EXIT_FAILURE;
        }

        if (!getenv("XAUTHORITY") &&
            setenv("XAUTHORITY", MINLOGIN_ROOT_XAUTHORITY, 1) != 0) {
            perror("minlogin: setenv XAUTHORITY");
            return EXIT_FAILURE;
        }
    }

    dpy = XOpenDisplay(NULL);

    if (!dpy) {
        fprintf(stderr, "minlogin: cannot open X display\n");
        return EXIT_FAILURE;
    }

    if (!create_monitor_windows()) {
        fprintf(stderr, "minlogin: failed to create monitor windows\n");
        goto done;
    }

    gc = XCreateGC(dpy, monitors[0].window, 0, NULL);

    if (!gc) {
        fprintf(stderr, "minlogin: failed to create graphics context\n");
        goto done;
    }

    int screen = DefaultScreen(dpy);
    Visual *visual = DefaultVisual(dpy, screen);
    Colormap cmap = DefaultColormap(dpy, screen);

    font_username = XftFontOpenName(
        dpy, screen, MINLOGIN_USERNAME_FONT
    );

    font_password = XftFontOpenName(
        dpy, screen, MINLOGIN_PASSWORD_FONT
    );

    font_status = XftFontOpenName(
        dpy, screen, MINLOGIN_STATUS_FONT
    );

    if (!font_username || !font_password || !font_status) {
        fprintf(
            stderr,
            "minlogin: cannot load configured Xft font(s); "
            "check config.h and installed fonts\n"
        );
        goto done;
    }

    color_username_allocated = XftColorAllocName(
        dpy, visual, cmap,
        MINLOGIN_USERNAME_COLOR, &color_username
    );

    color_placeholder_allocated = XftColorAllocName(
        dpy, visual, cmap,
        MINLOGIN_PLACEHOLDER_COLOR, &color_placeholder
    );

    color_password_allocated = XftColorAllocName(
        dpy, visual, cmap,
        MINLOGIN_PASSWORD_COLOR, &color_password
    );

    color_status_allocated = XftColorAllocName(
        dpy, visual, cmap,
        MINLOGIN_STATUS_COLOR, &color_status
    );

    if (!color_username_allocated ||
        !color_placeholder_allocated ||
        !color_password_allocated ||
        !color_status_allocated) {
        fprintf(stderr, "minlogin: failed to allocate Xft colors\n");
        goto done;
    }

    if (access(BACKGROUND_IMAGE, R_OK) == 0)
        original_background = imlib_load_image(BACKGROUND_IMAGE);

    int grab = XGrabKeyboard(
        dpy,
        monitors[active_monitor].window,
        True,
        GrabModeAsync,
        GrabModeAsync,
        CurrentTime
    );

    if (grab != GrabSuccess) {
        fprintf(stderr, "minlogin: could not grab keyboard\n");
        goto done;
    }

    redraw_all();

    for (;;) {
        XEvent event;
        XNextEvent(dpy, &event);

        if (event.type == Expose) {
            int i = find_monitor(event.xexpose.window);

            if (i >= 0)
                draw_monitor(i);

            continue;
        }

        if (event.type == ButtonPress) {
            int i = find_monitor(event.xbutton.window);

            if (i >= 0) {
                active_monitor = i;

                XGrabKeyboard(
                    dpy,
                    monitors[i].window,
                    True,
                    GrabModeAsync,
                    GrabModeAsync,
                    CurrentTime
                );

                XSetInputFocus(
                    dpy,
                    monitors[i].window,
                    RevertToParent,
                    CurrentTime
                );

                redraw_all();
            }

            continue;
        }

        if (event.type != KeyPress)
            continue;

        KeySym key;
        char input[32];

        int count = XLookupString(
            &event.xkey,
            input,
            sizeof(input),
            &key,
            NULL
        );

        if (key == XK_Escape) {
            memset(password, 0, sizeof(password));
            password_len = 0;
            message[0] = '\0';

            if (preview_mode) {
                exit_code = EXIT_SUCCESS;
                break;
            }
        } else if (key == XK_BackSpace || key == XK_Delete) {
            if (password_len > 0) {
                password[--password_len] = '\0';
            }

            message[0] = '\0';
        } else if (key == XK_Return || key == XK_KP_Enter) {
            if (preview_mode) {
                snprintf(
                    message, sizeof(message),
                    "Preview only - no login attempted"
                );
            } else if (password_len == 0) {
                snprintf(
                    message, sizeof(message),
                    "Enter your password"
                );
            } else {
                int ok = authenticate_and_launch();

                memset(password, 0, sizeof(password));
                password_len = 0;

                snprintf(
                    message,
                    sizeof(message),
                    "%s",
                    ok
                        ? "Session ended; enter password to log in again"
                        : "Authentication or session failed"
                );
            }
        } else if (count > 0 &&
                   input[0] >= 32 &&
                   input[0] != 127 &&
                   password_len < sizeof(password) - 1) {
            /*
             * Printable single-byte input only. Unicode/XIM input is not
             * implemented in this version.
             */
            password[password_len++] = input[0];
            password[password_len] = '\0';
            message[0] = '\0';
        }

        redraw_all();
    }

done:
    cleanup();
    return exit_code;
}

