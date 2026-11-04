# Pokémon Arena 2D for PSP-1000

An independent native homebrew adaptation for the PSP, based on the general
real-time arena concept of [Poke2D Arena](https://poke2darena.com/). It does not
run the website or its Phaser code. The project was also originally described
as based on [Empress: Pokémon 2D Arena](https://omniscye.itch.io/empress-pokemon-2d-arena);
that Windows/Unity game and the Poke2D Arena website are separate references.
It uses original PSP menus and arenas, plus
community-contributed Pokémon fan sprites credited in
[`assets/SPRITE_CREDITS.md`](assets/SPRITE_CREDITS.md).

The website's own game graphics and files are not included. The sprite atlas
comes from PMD SpriteCollab, not from Poke2D Arena. Sprite contributions are
licensed for non-commercial use with attribution; see the included license and
credits. Pokémon names and characters belong to their respective rights
holders. This is an unofficial fan project.

## Playable modes

- **Quick Battle:** two Pikachu training duels, then random three-member teams
  against the CPU.
- **VS CPU:** choose among three saved teams, edit each three-Pokémon lineup,
  and select the arena and Easy, Normal, or Hard CPU difficulty.
- **Story:** 16 original one-on-one challenges across four chapters, with
  saved progress and a selectable starter.
- **Team Builder:** browse the 25-Pokémon roster with sprite previews, choose a
  party slot, and save three lineups. Each Pokémon has its own named skill,
  utility move, and ultimate. VS CPU lets you pick the saved team, stage, and
  CPU level before entering the editor or starting the match.
- **Settings:** CPU difficulty, default arena, reduced flashes, and Story
  starter.
- **Battle:** six arenas, all 18 type matchups, per-species Idle/Walk/Attack
  sprite animations, type-colored attacks, timed skills, Protect, dash,
  ultimate charge, three-member teams, switching, burn/poison/paralysis/sleep,
  boosts, and timed supplies.

This is an offline PSP build. The web edition's online rooms, three-player
brawl, full roster and custom move editor are not included. The PSP uses 25
independently sourced Pokémon sprites and original PSP attack effects; it does
not bundle or copy the web game's artwork or animation files.

## PSP controls

- Analog stick or D-pad: move
- Hold **L** + D-pad: aim
- **Cross:** basic attack
- **Square:** species skill
- **L + Circle:** species utility/heavy move
- **L + Square:** Protect
- **Circle:** dash
- **Triangle:** ultimate
- **R:** switch to the next available teammate
- **R + Cross / Square / Circle:** select party slot 1 / 2 / 3
- **Start:** pause; **Select:** exit

In menus, use the D-pad, **Cross** to select, and **Circle** to go back. In the
Team Builder, the D-pad browses the visible roster grid, **Cross** assigns the
previewed Pokémon, **L/R** changes the party slot, and **Square** cycles among
saved teams. The VS CPU setup also opens the team editor before battle.

Save data is written to `ms0:/PSP/SAVEDATA/POKE2D/`. The static sprite atlas
and animated sprite atlas are read from
`ms0:/PSP/GAME/PokemonArena2D/assets/`; keep both
`pokemon_atlas.rgb565` and `pokemon_animations.rgb565` in the `assets`
folder beside `EBOOT.PBP` when installing.
The animation atlas uses a reserved 4 MB heap; if allocation fails, the game
falls back to the static sprite atlas.

## Build and install

With PSPDEV / PSPSDK installed, run `make` in this directory. The output is
`EBOOT.PBP`. Copy the complete `PSP/GAME/PokemonArena2D/` folder from the ZIP
to the root of the Memory Stick, then launch it under **Game → Memory Stick**.

`PSP_LARGE_MEMORY=0`; the build does not request the PSP-2000 extended-memory
mode and targets PSP-1000.

To rebuild the atlases, run `sh tools/build_sprite_atlas.sh` (requires Python 3
and Pillow). The script uses the pinned upstream commit listed in
`assets/SPRITE_CREDITS.md`; set `SPRITECOLLAB_SOURCE_DIR` to local source
images to rebuild without network access.

## Reference notes

- Web feature and control observations: [`docs/poke2d-web-reference.md`](docs/poke2d-web-reference.md)
- Earlier Empress IL2CPP package notes: [`docs/reverse-engineering.md`](docs/reverse-engineering.md)
