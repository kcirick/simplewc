# SWC (Simple Wayland Compositor)

It is a simple wlroots-based compositor (window manager) written to fit my specific needs, and a way 
to teach myself the basics of window management. (formerly SimpleWay).

## Description

 - Minimal stacking window manager for Wayland using wlroots
 - Not meant to be tiny or fast, but aims for simplicity in design and coding
 - Written in C
 - Built upon [tinywl], inspirations from [dwl] and [labwc]
 - Features:
   - No frills (menu, titlebar, icons, pixmap themes, etc...)
   - Text config file (default $HOME/.config/swwm/configrc)
   - Simple tiling (manual left-right tiling or auto-tile like DWL/DWM)
   - swc-msg: IPC messenger using dwl ipc protocol(dwl-ipc-unstable-v2.xml, adopted from [dwlmsg])

[tinywl]: https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/tinywl
[dwl]: https://codeberg.org/dwl/dwl
[labwc]: https://github.com/labwc/labwc
[dwlmsg]: https://codeberg.org/notchoc/dwlmsg

Usage:

`> swc [--config file][--start cmd][--debug][--version][--help]`

`> swc-msg --set [--tag .+-^][--client tag_n]`

`> swc-msg (--get | --watch) [--output][--tag][--client (title | appid)]`


## Status
Please use [Github Issues Tracker][ghit] to report bugs and issues.

Still a work in progress, and not in a working state yet. Stay tuned...

[ghit]: https://github.com/kcirick/swc/issues


## Version Log

  - 0.2 (Work in progress)
  - 0.1 (2023-12-23) ([download][v01])
    - Goal: get the base code in working order

[v01]: https://github.com/kcirick/swc/archive/refs/tags/v0.1.tar.gz
