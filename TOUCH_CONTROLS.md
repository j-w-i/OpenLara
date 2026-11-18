# OpenLara on iPod nano 7: touch controls

Landscape, 432x240, home button on the right. The panel's multitouch controller can report several
fingers (kernel API v27, see Multitouch below); the HUD uses one finger for the stick and others for
buttons at the same time (hold FIRE while running). Toggles and auto-held actions still stand in
for a second thumb when only one finger is seen. All of it is drawn by the game through GLES2
(`engine/src/nanotouch.h`, solid quads in the game's own UI pass, about 100 quads).
Everything is driven from Lara's real state (`stand`, `state`, weapons, floor trigger data,
collision probes), not guessed from the level.

## Layout (px on the 432x240 screen)

```
 [stick zone]            [look][menu]                    [weapon]
 floating stick,                                  [roll/climb]
 left of x=200,                                [walk] [action]  [JUMP]
 below y=44
```

* **Move stick (left)**: lands where the thumb lands. 46 px radius, 15% dead zone, a near-vertical
  push is snapped to straight so running does not drift. The stick only walks, runs, turns and steps back; it never jumps. For a running jump lift the thumb and tap JUMP (sticky stick below).
* **Sticky stick**: tap JUMP/DIVE/SWIM within 0.35 s of lifting the stick and the old direction is
  held for 0.7 s, so lift-and-tap still makes a running jump.
* **Jump** (big, bottom right). **Action** next to it. **Roll**, **Walk** (toggle, lit green),
  **Weapon**, **Look** (hold, drag the thumb to look round) and **Menu** (inventory).
* Physical buttons still work: LEFT = whatever the jump slot is, RIGHT = whatever the action slot is.
* Pressed buttons light up; context buttons get a pulsing amber ring and a small label.

## Contextual buttons

| Lara is... | What changes |
|---|---|
| Standing at a wall or ledge she can climb (checkClimb's own test: step 1 to 7.5 blocks, headroom) | Roll becomes **CLIMB** (action + forward, amber ring) |
| On a switch cell | Action becomes the lever icon **USE** |
| On a key hole / puzzle slot cell | Action becomes the key icon **USE** |
| Next to a pickup | Action becomes **TAKE** |
| Facing a push block | Action becomes **PUSH**; once she has hold, action is auto-held and the label is **LET GO** (stick up pushes, down pulls) |
| In the air after a jump | Action becomes **GRAB** and is auto-held (ledge grab with no second thumb), when her hands are empty |
| Hanging | Jump becomes **PULL UP** (action + forward), action becomes **DROP**; action is auto-held while hanging; stick left/right shimmies |
| An active enemy within 6 m | **Weapon** button pulses amber (**DRAW**) |
| Weapons drawn | Weapon becomes **HOLSTER** (green); Action becomes **FIRE** (red crosshair). Tap FIRE to latch auto-fire (fires only while Lara has a target, so the thumb stays on the stick). Weapon pulses amber if you are on an interaction while armed |
| Underwater | Jump becomes **SWIM** (tap toggles continuous swimming), **UP** / **DOWN** buttons pitch, stick steers |
| On the surface | Jump becomes **DIVE** (forward + jump), Action becomes **CLIMB OUT** |
| Dead / cutscene | HUD hidden |
| Inventory / menus | D-pad on the left (up/down/left/right), **OK** (big) and **BACK** on the right |

## Multitouch (generic SDK feature)

* Kernel `NanoOS/src/touch.c`: the panel report carries `payload[0x10]` = contact count and
  `payload[0x11]` = record stride (as RetailOS and NanoEmu's model decode it), one
  id/state/type/pad/X/Y record per finger. Every record now updates `touch_contacts[]`
  (up to 5, keyed by panel id; state 5 frees it). `touch` (single contact) is unchanged.
  A malformed count/stride falls back to the old first-record decode.
* ABI v27 (`gameabi.h`): `touch_contacts(out, max)` returns the fingers that are down,
  `touch_max_seen()` the most contacts one report has carried.
* SDK (`nano_app.h`): `nano_app_poll_touches(nano_touch_t out[NANO_TOUCH_MAX])`: stable slot per finger
  from press to release, `down/pressed/released/moved`, display coordinates for the current
  orientation, `prev_x/prev_y`. Falls back to one finger on a v26 kernel.
  `nano_app_touch_max_seen()` for diagnostics. `nano_app_poll_input` is unchanged.
* NOT yet verified on hardware with two real fingers (NanoEmu models one contact; the host test of
  the SDK slot logic passes). The game logs `touch maxfingers N` every 60 frames to the flight
  recorder: 2 or more there means the panel really reports both fingers.

## Code map

* `engine/src/nanotouch.h`: state probes (`probe`), button layout per mode (`layout`), touch to
  `Input::joy` mapping (`update`), drawing (`render`). `NT_FORCE_MODE`/`NT_FORCE_*` defines
  preview any context in `tools/host_ref`; `NT_NO_HUD` removes the drawing for A/B frame timing.
* `engine/src/mesh.h`: `addDynQuad`, a solid quad with free corners.
* `engine/src/level.h`: calls `NanoTouch::render()` at the end of the game and inventory UI passes.
* `src/main.cpp`: fills `NanoTouch::Sample` from `nano_app_poll_input` once per frame.

## Frame cost

Walk bench (`-DLARA_BENCH -DLARA_FIXED_DT -DLARA_LANDSCAPE=1 -DLARA_WALK -DLARA_FILTER=1`, stock
clocks, NanoOS at HEAD 15cd27e): HUD compiled out (`-DNT_NO_HUD`) 17.66 ms avg, p50 16.50;
first version 19.05 ms (1.4 ms); current 8-gon version 18.17 ms avg, p50 17.2 (+0.5 ms, 3%).
