# SimpleWC (Simple Wayland Compositor)

It is a simple wlroots-based compositor (window manager) written to fit my specific needs, and a way 
to teach myself the basics of window management. 

## Description

 - Minimal stacking window manager for Wayland using wlroots (currently based on wlroots v0.18)
 - Not meant to be tiny or fast, but aims for simplicity in design and coding
 - Written in C
 - Built upon [tinywl], inspirations from [dwl] and [labwc]
 - Features:
   - No frills (menu, titlebar, icons, pixmap themes, etc...)
   - Text config file (default $HOME/.config/simplewc/configrc)
   - Simple tiling (manual left-right tiling or one-shot auto-tile like DWL/DWM)
   - simplewc-msg: IPC messenger using dwl ipc protocol(dwl-ipc-unstable-v2.xml, adopted from [dwlmsg])

[tinywl]: https://gitlab.freedesktop.org/wlroots/wlroots/-/tree/master/tinywl
[dwl]: https://codeberg.org/dwl/dwl
[labwc]: https://github.com/labwc/labwc
[dwlmsg]: https://codeberg.org/notchoc/dwlmsg


### Screenshots

v0.1

<a href="https://i.redd.it/b7wqm1au7adc1.png" target="_blank"><img src="https://i.redd.it/b7wqm1au7adc1.png" width="450" /></a>


### Usage

    > simplewc [--config file][--start cmd][--debug|--info][--version][--help]

    > simplewc-msg --set [tag .+-^][client tag_n]
                  (--get|--watch) (tagcount|output|tag|client)
                   --action (quit|lock|reconfig)

### Build

    > meson setup [-Dxwayland=enabled|disabled] build
    > ninja -C build || exit 1
    > sudo ninja -C build install

 - Build dependencies:
   - wlroots
   - libxkbcommon (usually a dependency of wlroots)
   - libinput (usually a dependency of wlroots)
   - xwayland (optional)
   - libxcb (optional - for xwayland)

### Configuration

Default configuration file read from `$HOME/.config/simplewc/configrc`, and allows users to customize:

 - window management behaviour: ( # of tags, tile gaps, movement steps etc )
 - window border width and colour
 - lock command
 - autostart script (e.g. set background, invoke status-bar, launch idle-inhibitor)
 - keyboard layout and options 
 - keybinds
 - mouse binds 


## Version Log

Please use [Github Issues Tracker][ghit] to report bugs and issues.

  - 0.3 (Work in progress)
    - Goal: Better support for multihead and fullscreen; Continue to improve the compositor
    - Uses wlroots v0.18
    - Improved support for multihead
    - Uses the same tagset across all outputs
    - Added fullscreen support
    - Added support for wlr_relative_pointer_manager_v1
  - 0.2 (2024-08-29) ([download][v02])
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
[v02]: https://github.com/kcirick/simplewc/archive/refs/tags/v0.2.tar.gz
[ghit]: https://github.com/kcirick/simplewc/issues

