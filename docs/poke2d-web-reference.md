# Poke2D Arena web reference

## Project identity and scope

The current PSP target is the browser edition at
[poke2darena.com](https://poke2darena.com/), called Poke2D Arena on its own
pages. The site describes it as an independent project and says it is not the
Big Kid Nation / Fabled Apps game. Its About page identifies the web game as
TypeScript rendered with Phaser 4, with battle rules in a separate fixed
60-tick-per-second simulation.

This is distinct from the previously inspected **Empress: Pokémon 2D Arena**
Windows ZIP and Android APK. Those packages contain Unity IL2CPP; their metadata
does not describe the TypeScript/Phaser browser edition. Their evidence remains
in [`reverse-engineering.md`](reverse-engineering.md) for historical context
only.

## Web edition features

- Team modes use three Pokémon per side, with one active fighter and two on the
  bench. The surviving teammate takes over when the active Pokémon faints.
- Story Mode is one-on-one and has four chapters with sixteen challenges.
- Quick Battle, Vs CPU and Online use teams. Vs CPU has Easy, Normal and Hard.
- The current web edition also describes two-player Online rooms and a
  three-player Free-for-All mode.
- It has 18 Pokémon types and six arenas: Grass, Forest, Seaside, Volcanic, Ice
  and Ruins.

For a PSP-1000 first release, prioritize an offline Vs CPU battle. Online rooms,
browser persistence, team editing, the full roster and Story Mode progression
are later features.

## Controls and combat reference

Desktop movement uses WASD or arrow keys, while the mouse aims. The five battle
actions are basic attack (left mouse), skill (right mouse), mobility (Space),
utility (Q), and ultimate (E). R cycles to the next healthy teammate; 1, 2 and
3 select party slots directly.

The published guides describe these rules:

- Basic attack repeats while held, with a 0.5-second cooldown. Physical-style
  fighters lunge for about 3.2% of target maximum HP; special-style fighters
  fire a bolt for about 2.5% at a published range of 380 browser pixels.
- Regular moves use individual cooldowns and no energy cost. Examples range
  from 1.8 seconds for fast attacks to 5.5 seconds for Double-Edge and Flare
  Blitz.
- Same-type attack bonus is 1.5×. Type effectiveness is 2×, 0.5× or 0×;
  dual-type multipliers combine. Basic attacks have a half-damage floor against
  immunity.
- Generic dash has a 2.5-second cooldown and 0.3 seconds of invulnerability.
  Manual switching has a 3.5-second cooldown, about 0.8 seconds of send-out
  time, and 0.2 seconds of invulnerability. A fainted fighter is replaced
  after about 0.78 seconds.
- Ultimate starts at 50% charge. Damage dealt and received build charge; a cast
  has a windup that can be avoided by dash or Protect.
- Supplies first appear at 10 seconds, then every 18 seconds. The warning is
  2.5 seconds; a supply disappears after 12 seconds. Energy adds 20 ultimate
  charge, Heal restores 20% HP, and Speed grants 1.3× movement for six seconds.
- Status examples: Protect blocks direct damage for 1.2 seconds; burn lasts 20
  seconds and ticks every four; paralysis slows movement by 50% for three
  seconds; sleep prevents movement and attacks for 2.8 seconds.

The website's battle-rules guide gives more detail on CPU reaction times,
ultimate charge, statuses, supplies and timeout scoring. These browser-pixel
distances and timings should be translated to PSP screen and frame values,
rather than copied as raw browser coordinates.

## PSP mapping proposal

This is a port-side proposal, not a control mapping from the website:

- Analog stick or D-pad: move
- Cross: basic attack
- Square: regular skill
- Circle: dash
- Triangle: ultimate
- L: utility skill
- R: switch to the next healthy teammate

The first combat slice should use one active fighter per side, a three-member
party, basic attack, one regular skill, dash, switching, HP, KO replacement and
a CPU opponent. Use a fixed 60 Hz simulation and keep it separate from drawing.

## Assets and attribution

The site's About page says its original code, battle simulation and rules,
effects, arenas, interface, story script, text and screenshots should not be
copied, re-hosted or redistributed without written permission. It identifies
some third-party assets as CC0 or CC BY 4.0 and points to in-game credits.
This repository does not reuse the website's artwork or files. The PSP build
instead uses separately sourced community Pokémon sprites from PMD SpriteCollab,
selected from their attribution records and converted to a small RGB565 atlas.
See [`assets/SPRITE_CREDITS.md`](../assets/SPRITE_CREDITS.md) and the included
license text. That sprite contribution license is non-commercial and does not
license Pokémon trademarks or rights held by their owners.

## Sources

- [Poke2D Arena homepage](https://poke2darena.com/)
- [Controls and combat guide](https://poke2darena.com/guides/how-to-play-controls/)
- [Battle rules](https://poke2darena.com/guides/battle-rules/)
- [Team building and move editing](https://poke2darena.com/guides/team-building/)
- [Story Mode walkthrough](https://poke2darena.com/guides/story-mode/)
- [About and technical notes](https://poke2darena.com/about/)
- [Web edition updates and project separation](https://poke2darena.com/release-date/)
- [Empress: Pokémon 2D Arena itch.io page](https://omniscye.itch.io/empress-pokemon-2d-arena)
