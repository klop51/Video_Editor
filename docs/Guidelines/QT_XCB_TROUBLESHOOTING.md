**Symptoms & fixes**
- `OpenGL functionality tests failed` → add `mesa-common-dev libgl1-mesa-dev`.
- `xkbcommon_x11 = 0` → add `libxkbcommon-dev libxkbcommon-x11-dev`.
- `Feature "xcb" / "xcb_xlib" breaks condition` → add missing XCB dev libs:
  - `libx11-xcb-dev libxcb-xinput-dev libxcb-xkb-dev libxcb-randr0-dev`
  - `libxcb-xinerama0-dev libxcb-render0-dev libxcb-render-util0-dev`
  - `libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev`
  - `libxcb-shape0-dev libxcb-sync-dev libxcb-xfixes0-dev`
  - `libxcb-shm0-dev libxcb-util-dev`

**Verify**
```
pkg-config --modversion xcb xcb-randr xcb-xkb xcb-xinput xkbcommon xkbcommon-x11 gl