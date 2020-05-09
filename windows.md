# TS3D on Windows

TS3D was designed to run on Unix platforms. However, using PDCurses and MinGW, I
have gotten it to work on the Windows console. Note that it still must be built
on Unix using MinGW and that the tests cannot run on Windows.

## Build Requirements

MinGW is required as well as the source repository of PDCurses. The repository
can be found [here](https://github.com/wmcbrine/PDCurses).

## Building

Run this:

```
CC=... PDCURSES_DIR=... ./make-windows
```

`CC` should be set to the C compiler to use. The default is
`x86_64-w64-mingw32-gcc` if `CC` is left unspecified.

`PDCURSES_DIR` should be set to the root directory of the PDCurses repository.

You can specify other parameters, e.g. `CFLAGS`, alongside the two above. These
will work as normal.

The script will create a statically-linked executable `ts3d.exe` which you can
run on Windows. If `$PDCURSES_DIR/wincon/pdcurses.a` does not exist, the script
will build it as well. If `pdcurses.a` is already built, make sure it is also
built for Windows, not the native platform.

The statically linked `ts3d.exe` compiled with `CFLAGS='-O3 -flto'` is about
740k in size.

## Installation

The default data path for TS3D on Windows is `%AppData%\ts3d` rather than
`$HOME/.ts3d`. You will have to manually install the game data to
`%AppData%\ts3d\data`. Then the executable should work. Launch it from the
command prompt.
