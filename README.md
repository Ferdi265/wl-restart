# `wl-restart` - restart your compositor when it crashes

`wl-restart` is a simple tool that restarts your compositor when it crashes. It
also creates and destroys the Wayland socket for the compositor, so that
clients that support seamless compositor reconnects (most KDE programs, e.g.
`konsole`) don't die with it when your compositor crashes.

`wl-restart` is inspired by KDE's `kwin_wayland_wrapper` and reuses
`wl-socket.c` from its implementation.

## Compositor Requirements

Your compositor must support Wayland socket handover. The socket is passed to
the compositor with the options `--socket NAME --wayland-fd FD`.

**Supported Compositors:**

- Kwin (but you should probably use `kwin_wayland_wrapper` instead, which is default in Plasma 6)
- Sway (only with my `feature-compositor-restart` branch)
- Hyprland (only with my `feature-compositor-restart` branch)
- your compositor?

## Features

- Restarts your compositor when it crashes
- Don't lose your session (for programs that support seamless Wayland reconnect)
- Configurable max number of crashes before giving up

## Usage

```
usage: wl-restart [[options] --] <compositor args>

compositor restart helper. restarts your compositor when it
crashes and keeps the wayland socket alive.

options:
  -h,   --help             show this help
  -n N, --max-restarts N   restart a maximum of N times (default 10)
```

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
