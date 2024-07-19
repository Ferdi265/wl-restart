# `wl-restart` - restart your compositor when it crashes

`wl-restart` is a simple tool that restarts your compositor when it crashes. It
also creates and destroys the Wayland socket for the compositor, so that
clients that support seamless compositor reconnects (most KDE programs, e.g.
`konsole`) don't die with it when your compositor crashes.

`wl-restart` is inspired by KDE's `kwin_wayland_wrapper` and reuses
`wl-socket.c` from its implementation.

## Compositor Support

Your compositor must support Wayland socket handover. The socket is passed to
the compositor with the options `--socket NAME --wayland-fd FD`.

**Supported Compositors:**

- Kwin (but you should probably use `kwin_wayland_wrapper` instead, which is default in Plasma 6)
- Sway (via [`feature-socket-handover`](https://github.com/ferdi265/sway/tree/feature-socket-handover) branch)
- Hyprland (via [`feature-socket-handover`](https://github.com/ferdi265/hyprland/tree/feature-socket-handover) branch)

Other compositors do not support Wayland socket handover as of now. If there are
any compositors with support for socket handover missing from this list, please
open an issue for it on GitHub.

## Client and Toolkit Support

The socket handover implemented by `wl-restart` and compatible compositors
allows Wayland clients to survive a crash or restart of the compositor without
losing any state. However, this needs explicit support in the client or toolkit
used.

Currently, only **Qt6** supports surviving a compositor restart. The
environment variable `QT_WAYLAND_RECONNECT=1` needs to be set in order for Qt
apps to stay alive when the compositor crashes.

A more complete list of toolkits and their state supporting this can be found at
in the [KDE wiki](https://invent.kde.org/plasma/kwin/-/wikis/Restarting).

## Features

- Restarts your compositor when it crashes
- Don't lose your session (for programs that support seamless Wayland reconnect)
- Configurable max number of crashes before giving up
- Sets `WL_RESTART_COUNT` to the current restart counter on compositor restart.

## Usage

```
usage: wl-restart [[options] --] <compositor args>

compositor restart helper. restarts your compositor when it
crashes and keeps the wayland socket alive.

options:
  -h,   --help             show this help
  -n N, --max-restarts N   restart a maximum of N times (default 10)
```

For example, run this in your TTY instead of normally starting your compositor:

- `$ wl-restart sway`
- `$ wl-restart -n 20 hyprland`

## Installation

`wl-restart` can be installed from the ArchLinux AUR with the package
[wl-restart-git](https://aur.archlinux.org/packages/wl-restart-git).

## Dependencies

- `CMake`
- `scdoc` (for man pages)

## Building

- Run `cmake -B build`
- Run `cmake --build build`

## Files

- `src/wl-restart.c`: the main program
- `src/wl-socket.c`: the socket creation library from `kwin_wayland_wrapper`

## License

This project is licensed under the GNU GPL version 3.0 or later (SPDX
[GPL-3.0-or-later](https://spdx.org/licenses/GPL-3.0-or-later.html)). The full
license text can also be found in the [LICENSE](/LICENSE) file.
