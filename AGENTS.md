# High FPS Fixes: project rules

These add to the workspace guide; where they differ from it, they say so and
why.

## Source layout

- `src\core` (namespace `hff`): configuration, log, memory, patch records,
  the conflict guard, the timestep helpers, MinHook setup, the install
  summary.
- `src\game` (namespace `hff`): `addresses.h` holds the engine globals,
  struct offsets and instruction patterns several areas share;
  `sites\<subject>.h` holds the patch sites of one subject, each site's
  address, return address and expected bytes together, with the comment that
  explains what the game does there. `frame_hook` gives fixes a call once a
  frame; `frame_steps` holds the shared scaled per-frame steps.
- One directory per area (`camera`, `player`, `vehicles`, `handling`,
  `bikes`, `weapons`, `world`, `hud`, `scripts`, `timers`, `framerate`),
  namespace `hff::<area>`, one `.cpp`/`.h` pair per fix or per small group of
  fixes with one subject. A header declares only the installers and what
  another file calls; patch records, thunks and helpers stay in the
  anonymous namespace of their `.cpp`.
- `HighFpsFixes.cpp` lists the fixes per INI section in the order they are
  installed. Keep that order when adding a fix: the log and the claimed patch
  ranges follow it.

## Addresses

The workspace guide asks for one address header. The sites here run to about
1,800 lines of addresses, bytes and explanations, far over the 300 line limit
for a header, so they are split by subject under `src\game\sites` and
nowhere else. A naked thunk reads game data through absolute operands
(`fld dword ptr ds:[0x00B7CB5C]`), because inline assembly cannot use a
`constexpr` as a memory operand; those operands are the only addresses
written outside `src\game`.

## Validation

`tests\` holds `HighFpsFixesTests.asi`, which drives a test copy of the game
through a scenario at a virtual frame rate and writes its measurements:

```powershell
powershell -ExecutionPolicy Bypass -File tests\run.ps1 -Modes stock,fixed -Fps 30,300
```

The two copies, `hff test stock` and `hff test fixed`, sit side by side in
the folder named by `-Copies` or the `HFF_TEST_COPIES` environment variable,
each with an ASI loader, SilentPatch and Borderless Mode. The runner never
starts the game while another game or a full-screen application holds the
screen. A fix that changes runtime behaviour gets a
scenario where one can measure it, and its result at 300 FPS is compared with
30 FPS before the fix is called done.
