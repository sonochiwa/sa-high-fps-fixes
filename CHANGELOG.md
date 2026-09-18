# Changelog

## 1.1.0

- Changed the `enableLogging` setting to `log`, off by default. Configuration
  and patch errors still force the log on. An existing INI keeps working;
  rename the key to turn routine logging back on.
- Changed the INI to be created from the canonical file compiled into the
  plugin, byte for byte.
- Added version information to the plugin file.
- Removed `README.txt` from the release archive; the repository README is the
  documentation.

## 1.0.0

- Added 61 frame-rate fixes across the camera, the player, vehicles, weapons,
  particles, the HUD, the world and the menu, each with its own switch in
  `HighFpsFixes.ini` and each verifying the original instructions before
  patching.
- Added an optional frame limiter, refresh-rate override and per-situation
  automatic limits, all off by default.
- Added claiming of sites already held by other frame-rate plugins, with a
  guard that puts the patch back when it is overwritten later in startup.
- Added a log listing every fix that installed and every one that was
  skipped with the reason.
