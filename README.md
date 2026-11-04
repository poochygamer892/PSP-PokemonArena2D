# Pokémon Arena 2D for PSP

This project is based on [Empress Pokémon 2D Arena](https://omniscye.itch.io/empress-pokemon-2d-arena).
It is an unofficial PSP-1000 homebrew reimplementation. The supplied Windows
and Android packages are reference material; the game itself is being built
natively for PSP rather than by porting the Unity executables.

The Windows and Android reference packages are Unity IL2CPP builds. They are
research inputs only; no reference-game assets are included in this repository
yet.

## Target

- PSP-1000, 480 × 272 display
- PSPDEV / PSPSDK toolchain
- Native C/C++ homebrew, packaged as `EBOOT.PBP`
- First milestone: one arena, two fighters, movement, attacks, hit detection,
  health, and a match restart flow

## Current status

Static metadata from the reference build identifies real-time combat systems
for input, movement, attacks, dodging, switching Pokémon, projectiles, and
arena collisions. Exact controls, timing, damage rules, visuals, and match flow
still need confirmation from the running game. See
[`docs/reverse-engineering.md`](docs/reverse-engineering.md).

The repository now contains the first native PSP prototype: a placeholder
arena loop with movement, a CPU opponent, projectiles, health, dodge, and a
fighter-swap placeholder. Its button mapping and combat values are provisional
until they are compared with an observed reference match.

## Controls in the prototype

- Analog stick or D-pad: move
- Cross: attack
- Circle: dodge
- Triangle: swap fighter profile
- Start: restart after a knockout
- Select: exit

These are temporary prototype controls, not yet confirmed reference controls.

## Build

With PSPDEV / PSPSDK configured in the environment, run `make` in this
directory. The build target is `EBOOT.PBP`.

## Work plan

1. Record the reference game's controls, arena dimensions, fighter states, and
   match rules from a short observed match.
2. Inventory candidate sprite and audio assets and decide which can be used in
   the homebrew.
3. Implement a PSP input/render loop and one playable arena as a vertical slice.
4. Compare it against the reference, then add moves, fighters, and menus in
   small increments.
