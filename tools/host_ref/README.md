# Graphics reference (ANGLE GLES 2.0 on macOS)

Ground truth for NanoOS GL2 rendering of OpenLara: the iPod build of the engine
(same `_OS_NANO7` / `_GAPI_GLES2` defines, same LOW detail settings, same level) rendered
through ANGLE's GLES 2.0 + EGL, 240x432, fixed 33 ms frame step.

```sh
ANGLE_DIR=/path/to/dir/with/libEGL.dylib+libGLESv2.dylib tools/host_ref/build.sh
cd /dir/containing/DATA && /path/to/tools/host_ref/build/host_ref OUT_DIR 6   # writes OUT_DIR/ref0..5.ppm
tools/host_ref/compare.py OUT_DIR/ref4.ppm nanoos_frame.(png|bin) side_by_side.png
```

ANGLE ships with Chromium-based apps (CEF in Steam, Adobe, Electron apps). Copy the two
dylibs; do not run them in place. Level data (`DATA/LEVEL2.PHD`) is proprietary: keep it
out of git.

The Nano side must use the same timing: build the bench with `-DLARA_BENCH -DLARA_FIXED_DT`.
Bench frame f lands in queue buffer f % 3 (q0 = frame 3, q1 = frame 4, q2 = frame 5 after
6 frames). In NanoEmu, `--capture q1:0x09a80000:414720` grabs frame 4.
