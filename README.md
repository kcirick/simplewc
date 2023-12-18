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

[tinywl]: https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/tinywl
[dwl]: https://codeberg.org/dwl/dwl
[labwc]: https://github.com/labwc/labwc

Usage:
`> swwm [--config file][--start cmd][--debug][--version][--help]`

## Status
Still a work in progress, and not in a working state yet. Stay tuned...


## Version Log
No versions tagged yet ...

  - 0.1 (Work in progress)
    - Goal: get the base code in working order
