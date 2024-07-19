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
support the *--socket* and *--wayland-fd* options. Currently that is only Kwin,
but patches exist for sway and hyprland. For details, see *COMPOSITOR SUPPORT*
below.

Currently there aren't many clients and toolkits that support surviving a
compositor restart, with *Qt6* and the KDE apps being a notable exception. For
details, see *CLIENT AND TOOLKIT SUPPORT* below.

# OPTIONS

*-h, --help*
	Show help message and exit.

*-n N, --max-restarts N*
	Restart the compositor a maximum of *N* times. (default is 10)

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

*sway*
	Upstream Sway does not support socket handover, but socket handover is
	implemented in the following branch and works well with *wl-restart*:

	https://github.com/ferdi265/sway/tree/feature-socket-handover

*hyprland*
	Upstream Hyprland does not support socket handover, but socket handover is
	implemented in the following branch and works well with *wl-restart*:

	https://github.com/ferdi265/hyprland/tree/feature-socket-handover

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
