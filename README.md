# Pokémon Arena 2D for PSP

This PSP-1000 homebrew targets the browser game [Poke2D Arena](https://poke2darena.com/).
The web edition describes itself as an independent project. This repository
uses a native PSP implementation; it does not run the browser game or Phaser.

The Windows and Android package analyzed earlier is **Empress: Pokémon 2D
Arena**, a separate Unity IL2CPP game. Its metadata is documented in
[`docs/reverse-engineering.md`](docs/reverse-engineering.md), but it is not
ground truth for the Poke2D Arena web edition. Web findings are recorded in
[`docs/poke2d-web-reference.md`](docs/poke2d-web-reference.md).

No assets from either game are included. The Poke2D Arena site says its code,
battle simulation, effects, arenas, interface, story and screenshots should
not be copied or redistributed without written permission; this project uses
independent PSP code and placeholder visuals.

## Target

- PSP-1000, 480 × 272 display
- PSPDEV / PSPSDK toolchain
- No expanded-memory request (`PSP_LARGE_MEMORY=0`)
- Native C/C++ homebrew, packaged as `EBOOT.PBP`
- First target slice: one arena, two three-member teams with one active fighter
  per side, movement, attack, dodge, switching, health and match restart

## Current status

The website describes a 60-tick-per-second real-time battle simulation, three-
Pokemon teams, aimed attacks, cooldown skills, dash, switching and type
matchups. Its guides provide detailed combat timings. See
[`docs/poke2d-web-reference.md`](docs/poke2d-web-reference.md).

The repository contains an early native PSP arena prototype with movement, a
CPU opponent, projectiles and health. It is still a one-fighter placeholder and
does not yet implement the site's three-member team or combat rules.

## Controls in the prototype

- Analog stick or D-pad: move
- Cross: attack
- Circle: dodge
- Triangle: swap fighter profile
- Start: restart after a knockout
- Select: exit

These are temporary controls. The PSP port will need its own mapping for the
web edition's five battle actions and teammate switching.

## Build

With PSPDEV / PSPSDK configured in the environment, run `make` in this
directory. The build target is `EBOOT.PBP`.

## Work plan

1. Replace the placeholder single fighters with three-member teams and active-
   fighter switching.
2. Implement the 60 Hz combat loop, cooldowns, dash, and attack aiming from the
   documented web rules.
3. Add a small roster, one arena and CPU behavior that fit PSP-1000 memory.
4. Use original or explicitly licensed visuals and add further modes only
   after the battle slice is stable.
