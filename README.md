# Quick Timer

A Pebble watchapp with an iOS-style quick timer, built from the Quick Timer implementation spec (final version, with the Appendix B changes).

## Build and run

```sh
cd quick-timer
pebble package install pebble-clay      # fetches Clay into node_modules
pebble build
pebble install --emulator emery         # also gabbro, basalt, chalk, diorite, flint
pebble logs --emulator emery
pebble emu-app-config                   # opens the settings page for the emulator
pebble install --cloudpebble            # real watch, via the Pebble app's Dev Connect
```

The emulator can't do a Quick Launch. To see the Fast screen there, set `DEBUG_FORCE_LAUNCH` to `1` in `src/c/config.h`, and set it back to `0` before you ship.

If the `wscript` here doesn't match your SDK, keep the one `pebble new-project --javascript` generates instead. Nothing in this project depends on changes to it.

## What changed from the spec body (Appendix B, final)

- **The alert vibrates until it's dismissed.** `ALERT_VIBE_REPEATS` and `ALERT_AUTO_CLOSE_S` are both `0`, which means no limit. If an alert left unattended for hours worries you, set `ALERT_AUTO_CLOSE_S` to 600 to restore a 10-minute auto-close.
- **The Fast slider animates its fill and ticks once per step.** You can tune it in `config.h`:
    - `FAST_ANIM_INTERVAL_MS` and `FAST_ANIM_PERCENT` set the animation speed.
    - `FAST_TICK_MS` sets the tick length (20 ms). Older watches with ERM motors (basalt, chalk, diorite) may need about 35 ms before the tick can be felt.

## Things to check by eye

These came from the spec, and I couldn't check them without a real render:

- **Text nudges** in `ui.c` (`prv_nudge`). These are the Gothic-font vertical offsets.
- **Round layouts.** Gabbro and chalk follow the spec's math, but the Full picker on chalk is tight.
- **Haptic tick strength** on your Pebble Time 2.

## Verified here

- Every C file compiles with `-Wall -Wextra` against a stub `pebble.h` for each platform flavor: emery, gabbro, flint/diorite and chalk. The touch declarations were checked against the PebbleOS source.
- The formatting and preset-sanitizing logic passes the spec's T1 cases plus edge cases.
- The phone-side settings conversion passes T18–T20 with Clay mocked.

The real `pebble build` and the emulator tests (T2 onward) haven't been run.
