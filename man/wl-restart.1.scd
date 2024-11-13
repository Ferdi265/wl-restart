wl-restart(1)

# NAME

wl-restart - restart your compositor when it crashes

# SYNOPSIS

*wl-restart* [[-h,-n N] --] <compositor args>

# DESCRIPTION

*wl-restart* is a simple tool that restarts your compositor when it crashes. It
also creates and destroys the Wayland socket for the compositor, so that
clients that support seamless compositor reconnects (most KDE programs, e.g.
*konsole*) don't die with it when your compositor crashes.

*wl-restart* will create a Wayland socket, and then starts the given compositor,
passing along the Wayland socket FD to the compositor with the options
*--socket NAME* and *--wayland-fd FD*. When the compositor crashes, *wl-restart*
will restart the compositor up to 10 times, after which it gives up. Details are
listed in *BEHAVIOUR* below.

Example Usage:

	$ wl-restart sway

	$ wl-restart -n 7 hyprland

This only works for compositors that support Wayland socket handover and that
support the *--socket* and *--wayland-fd* options or any of the other socket
handover mechanisms detailed under *OPTIONS*. Currently that is only Kwin and
Hyprland, but patches exist for sway and labwc. For details, see *COMPOSITOR
SUPPORT* below.

Currently there aren't many clients and toolkits that support surviving a
compositor restart, with *Qt6* and the KDE apps being a notable exception. For
details, see *CLIENT AND TOOLKIT SUPPORT* below.

# OPTIONS

*-h, --help*
	Show help message and exit.

*-n N, --max-restarts N*
	Restart the compositor a maximum of *N* times. (default is 10)

*--kde*
	Pass socket via cli options --socket and --wayland-fd. (default)
	This passes the socket in the same form as KDE's *kwin_wayland_wrapper*.

*--cli*
	Pass socket via cli options --wayland-socket and --wayland-fd.

*--env*
	Pass socket via env vars WAYLAND_SOCKET_NAME and WAYLAND_SOCKET_FD.

*--systemd*
	Pass socket via env vars LISTEN_PID, LISTEN_FDS, and LISTEN_FDNAMES.
	This passes the socket in the same form as the systemd socket activation
	protocol. Further information can be found in *sd_listen_fds*(3).

	The only difference from regular systemd socket activation is that in this
	case *wl-restart* will have already created the Wayland socket lock file,
	which would not be created by regular systemd socket activation as it is
	Wayland-specific.

# BEHAVIOUR

*wl-restart* first finds a free wayland socket name (e.g. *wayland-1*) and
creates it.

Then, it starts the compositor with the given arguments, but with *--socket
wayland-N --wayland-fd FD* appended to the arguments.

If the compositor exits with exit code *0*, *wl-restart* cleans up the socket
and exits as well.

If the compositor exits with exit code *133* (signal *SIGTRAP*), *wl-restart*
resets the restart counter and restarts the compositor.

If the compositor exits with any other exit code, *wl-restart* increments the
restart counter and restarts the compositor if it is smaller than the maximum.
Otherwise, *wl-restart* cleans up the socket and exits as well.

When the compositor is restarted, *wl-restart* sets the environment variable
*WL_RESTART_COUNT* to the restart counter, so other scripts can react to it and
e.g. display a message or restart services that don't restart automatically.

When *wl-restart* receives a *SIGINT* or *SIGTERM* signal, it kills the
compositor with *SIGTERM* if it's currently running, cleans up the socket, and
exits.

When *wl-restart* receives a *SIGHUP* signal, it kills the compositor with
*SIGTERM* if it's currently running, waits for it to die, and restarts it
without incrementing the restart counter.

# COMPOSITOR SUPPORT

*wl-restart* only works for compositors that support Wayland socket handover via
the *--socket* and *--wayland-fd* options.

*kwin_wayland*
	Kwin supports socket handover, but *wl-restart* isn't necessary or
	recommended for it, since automatic restarting for it is already implemented
	by *kwin_wayland_wrapper*, which is automatically used in
	*startplasma_wayland*.

	Kwin uses the *KDE* socket handover mechanism.

*hyprland*
	Hyprland supports socket handover and works well with *wl-restart*.

	Hyprland uses the *KDE* socket handover mechanism.

*sway*
	Upstream Sway does not support socket handover, but socket handover is
	implemented in the following branch and works well with *wl-restart*:

	https://github.com/ferdi265/sway/tree/feature-socket-handover

	This branch uses the *KDE* socket handover mechanism.
	
	https://github.com/ferdi265/sway/tree/feature-socket-handover-alternative

	This alternative branch uses the *env var* socket handover mechanism.

*labwc*
	Upstream labwc does not support socket handover, but socket handover is
	implemented in the following branch and works with *wl-restart*:

	https://github.com/ferdi265/labwc/tree/feature-socket-handover

	This branch uses the *env var* socket handover mechanism.

Other compositors do not support Wayland socket handover as of now. If there are
any compositors with support for socket handover missing from this list, please
open an issue for it on GitHub.

# CLIENT AND TOOLKIT SUPPORT

The socket handover implemented by *wl-restart* and compatible compositors
allows Wayland clients to survive a crash or restart of the compositor without
losing any state. However, this needs explicit support in the client or toolkit
used.

*Qt*
	Qt supports surviving a compositor restart in *Qt6*. The environment
	variable *QT_WAYLAND_RECONNECT=1* needs to be set in order for Qt apps to
	stay alive when the compositor crashes.

*GTK*
	GTK does not currently support surviving a compositor restart.
	There is a branch with a working implementation, but it hasn't been merged
	yet.

*SDL*
	SDL does not currently support surviving a compositor restart.
	There is a branch with a working implementation, but it hasn't been merged
	yet.
	
A more complete list of toolkits and their state supporting this can be found at
the following link:

	https://invent.kde.org/plasma/kwin/-/wikis/Restarting

# AUTHORS

Maintained by Ferdinand Bachmann <ferdinand.bachmann@yrlf.at>. More information
on *wl-restart* can be found at <https://github.com/Ferdi265/wl-restart>.
