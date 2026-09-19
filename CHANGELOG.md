# Changelog

## 1.2.0

- Removed `refreshRate`; use SilentPatch for that.
- Merged `[autoLimitFps]` into `[framerate]`. An existing INI is migrated.
- Added `README.txt` to the release archive.

## 1.1.0

- Changed `enableLogging` to `log`, off by default.
- Changed the INI to be created from the file compiled into the plugin.
- Added version information to the plugin file.
- Removed `README.txt` from the release archive.

## 1.0.0

- 61 frame-rate fixes across the camera, the player, vehicles, weapons,
  particles, the HUD, the world and the menu, each with its own switch.
- Optional frame limiter, refresh-rate override and automatic limits for
  specific game cases, all off by default.
- Wins over other frame-rate plugins at shared patch sites.
- Log of every fix that installed or was skipped.
