**Purpose:** Single source of truth for CI on Ubuntu runners. Models must sync workflows to match this.

## Workflows (inventory)
- `.github/workflows/tests.yml` → fast lint + unit tests only; avoid heavy toolchains when possible.
- `.github/workflows/build.yml` → full matrix build (vcpkg/Qt/CMake), build artifacts.

## Ubuntu packages (install **before** vcpkg/Qt)

# Core
build-essential pkg-config cmake ninja-build curl zip unzip tar
# Autotools
autoconf automake libtool libtool-bin m4 autoconf-archive libltdl-dev gperf
# X11 / XCB (Qt xcb platform)
libx11-dev libxext-dev libxfixes-dev libxrender-dev libxrandr-dev libxi-dev
libx11-xcb-dev libxcb1-dev libxcb-util-dev libxcb-shm0-dev
libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev
libxcb-render0-dev libxcb-render-util0-dev libxcb-randr0-dev
libxcb-xinerama0-dev libxcb-xfixes0-dev libxcb-sync-dev
libxcb-xkb-dev libxcb-xinput-dev
# xkbcommon + OpenGL
libxkbcommon-dev libxkbcommon-x11-dev
mesa-common-dev libgl1-mesa-dev

## Configure defaults (Linux)
- Use Ninja + Clang unless overridden: `-G Ninja`, env `CC=clang`, `CXX=clang++`.

## Verify step (must follow package install)

autoconf --version
automake --version
libtool --version
libtoolize --version
gperf --version
pkg-config --modversion x11 xcb xcb-randr xcb-xkb xkbcommon xkbcommon-x11 gl || true

## Logging (always-on)
- Tee tool outputs into `ci_logs/*.txt` and upload as artifact.
- On failure, tail `build/**/buildtrees/*/*.log` (vcpkg/Qt).

## History of common failures → minimal fixes
- `libtool: command not found` → add `libtool-bin` (+ verify step)
- `LT_SYS_SYMBOL_USCORE` macro missing (libxcrypt) → add `libtool libtool-bin libltdl-dev gperf`
- `OpenGL functionality tests failed` / `xkbcommon_x11` = 0 → add Mesa + xkbcommon dev headers
- `Feature "xcb" … breaks its condition` → add remaining XCB dev libs (`util`, `shm`, `xinput`, `xkb`, etc.)