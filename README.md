# OpenLara for iPod Nano 7 (NanoOS)

Port of Timur "XProger" Gagiev's **OpenLara** (Tomb Raider 1 engine, BSD 2-Clause) to the **iPod Nano 7** (`S5L8740` / PowerVR MBX Lite) built against the canonical **NanoOS** kernel and Application SDK.

## Upstream Attribution & Revision Selection (Phase 1)

- **Upstream Repository:** `https://github.com/XProger/OpenLara`
- **Selected Revision:** `3b268ca` (`master`)
- **License:** BSD 2-Clause (see `LICENSE`)
- **Why `3b268ca` was selected:**
  - **Genuine GLES 2.0 backend (`_GAPI_GLES2`):** `src/gapi/gl.h` includes explicit GLES 2.0 support and low-end embedded shader options (`OPT_VLIGHTPROJ`, `OPT_VLIGHTVEC`, `OPT_SHADOW_ONETAP`) tailored for mobile/embedded GPUs.
  - **Zero C++ STL runtime in core:** The core engine (`src/*.h`) compiles cleanly with freestanding `arm-none-eabi-g++ -fno-exceptions -fno-rtti` and only requires basic C libc/math/stdio primitives (`malloc`, `free`, `fopen`, `fread`, `fseek`, `ftell`, `fclose`, `sinf`/`cosf`/`sqrtf`/`atan2f`, etc.).
  - **Memory-efficient options:** Supports `USE_ATLAS_RGBA16` (16-bit texture atlas), configurable `SHADOW_TEX_SIZE`, and `Core::Settings` quality toggles to fit within the Nano 7's ~22.4 MiB application heap.
  - **Stability fixes:** Includes upstream fixes for camera matrix NaNs (`cea6576`), enemy pathfinding heap overflow (`7a80d8f`), and stream bounds checking (`0a52e5a`).

## Game Assets (Not Included)

Proprietary Tomb Raider game data is **not** included in this repository. Place your original Tomb Raider 1 level files (e.g., `DATA/GYM.PHD` or `DATA/LEVEL1.PHD`) in `/Volumes/iPod/Apps/Data/OpenLara/DATA/`.

## Build

```sh
# Build against NanoOS SDK
make -C /path/to/NanoOS app APP_DIR=/path/to/openlara-nano7

# Or from this directory
make NANOOS_DIR=/path/to/NanoOS
```

Output executable: `build/game.bin` (`NGM1` image linked at `0x0b000000`).
