# SimpleWC (Simple Wayland Compositor)

It is a simple wlroots-based compositor (window manager) written to fit my specific needs, and a way 
to teach myself the basics of window management. (formerly SimpleWay).

## Description

 - Minimal stacking window manager for Wayland using wlroots (currently based on wlroots v0.17)
 - Not meant to be tiny or fast, but aims for simplicity in design and coding
 - Written in C
 - Built upon [tinywl], inspirations from [dwl] and [labwc]
 - Features:
   - No frills (menu, titlebar, icons, pixmap themes, etc...)
   - Text config file (default $HOME/.config/simplewc/configrc)
   - Simple tiling (manual left-right tiling or auto-tile like DWL/DWM)
   - simplewc-msg: IPC messenger using dwl ipc protocol(dwl-ipc-unstable-v2.xml, adopted from [dwlmsg])

[tinywl]: https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/tinywl
[dwl]: https://codeberg.org/dwl/dwl
[labwc]: https://github.com/labwc/labwc
[dwlmsg]: https://codeberg.org/notchoc/dwlmsg

Usage:

`> simplewc [--config file][--start cmd][--debug][--version][--help]`

`> simplewc-msg --set [--tag .+-^][--client tag_n]`

`> simplewc-msg (--get | --watch) [--output][--tag][--client (title | appid)]`

`> simplewc-msg --action (quit | reconfig | lock)`


## Status
Please use [Github Issues Tracker][ghit] to report bugs and issues.

Still a work in progress, and not in a working state yet. Stay tuned...

[ghit]: https://github.com/kcirick/simplewc/issues


## Screenshots

v0.1

<a href="https://i.redd.it/mqvdzk97038c1.jpeg" target="_blank"><img src="https://i.redd.it/mqvdzk97038c1.jpeg" width="350" /></a>

## Version Log

  - 0.2 (Work in progress)
    - Goal: add in additional features
    - Uses wlroots v0.17
    - Added visibility toggle (a.k.a. iconify) and maximize functionality
    - Added support for wlr_idle_notifier_v1, wlr_idle_inhibit_v1, and wlr_session_lock_manager_v1 (e.g. swaylock)
    - Added support for wlr_output_power_management_v1 (e.g. wlopm)
    - Added support for wlr_gamma_control_manager_v1
    - Added feature to send action via simplewc-msg
    - Added support for dragging icons
    - Improve client cycling function
    - See ChangeLog for fixed bugs
  - 0.1 (2023-12-23) ([download][v01])
    - Goal: get the base code in working order
    - Uses wlroots v0.17

[v01]: https://github.com/kcirick/simplewc/archive/refs/tags/v0.1.tar.gz
