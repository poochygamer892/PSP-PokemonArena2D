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
- Quick Battle begins with two Pikachu training duels, then uses independent
  random 3v3 teams. Vs CPU has Easy, Normal and Hard and uses a saved team.
- Your Teams and the Vs CPU picker select a three-Pokémon party from the
  website's 1,066-entry roster and support per-Pokémon move editing.
- The current web edition also has two-player Online rooms and a three-player
  Free-for-All mode.
- It has 18 Pokémon types and six arenas: Grass, Forest, Seaside, Volcanic, Ice
  and Ruins.

The PSP build is offline. Its compact 25-Pokémon picker, saved teams and move
sets are PSP-side implementations; it does not include the browser's full
roster or freeform move editor.

## Controls and combat reference

Desktop movement uses WASD or arrow keys, while the mouse aims. The five battle
slots are basic attack (left mouse), skill (right mouse), mobility (Space),
utility (Q), and ultimate (E). Each Pokémon has a distinct preset moveset. R
cycles to the next healthy teammate; 1, 2 and 3 select party slots directly.

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

## Current PSP controls and port

- Analog stick or D-pad: move
- Cross: basic attack
- Square: species skill
- Circle: dash
- L + Circle: species utility/heavy move
- L + Square: Protect guard
- Triangle: ultimate
- R: switch to the next healthy teammate
- R + Cross / Square / Circle: select party slot 1 / 2 / 3 directly

The current PSP build has team selection and combat for a 25-Pokémon roster,
named skills and utility moves, 18-type matchups, switch/KO replacement, timed
supplies, a CPU opponent, and Idle/Walk/Attack animation cels from the credited
community sprite sheets. Network rooms, the 1,066-entry browser roster, and its
freeform move editor are not implemented.

REA's Playwright capture was attempted against both the live game URL and the
home page, but Chromium navigation failed before an initial page state could be
captured. The feature and control notes above come from the site's published
guides, not a live runtime trace or pixel comparison.

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
