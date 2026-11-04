# Pokémon Arena 2D for PSP-1000

An independent native homebrew adaptation for the PSP, based on the general
real-time arena concept of [Poke2D Arena](https://poke2darena.com/). It does not
run the website or its Phaser code. It uses original PSP menus and arenas, plus
community-contributed Pokémon fan sprites credited in
[`assets/SPRITE_CREDITS.md`](assets/SPRITE_CREDITS.md).

The website's own game graphics and files are not included. The sprite atlas
comes from PMD SpriteCollab, not from Poke2D Arena. Sprite contributions are
licensed for non-commercial use with attribution; see the included license and
credits. Pokémon names and characters belong to their respective rights
holders. This is an unofficial fan project.

## Playable modes

- **Quick Battle:** random three-member teams against the CPU.
- **VS CPU:** three saved team slots, selectable arena, and Easy, Normal, or
  Hard CPU difficulty.
- **Story:** 16 original one-on-one challenges across four chapters, with
  saved progress and a selectable starter.
- **Team Builder:** edit the three Pokémon in each of three saved teams.
- **Settings:** CPU difficulty, default arena, reduced flashes, and Story
  starter.
- **Battle:** six arenas, 25 Pokémon, all 18 type matchups, basic attack,
  skill, Protect, dash, ultimate, switching, status effects, and timed supplies.

This is an offline PSP build. The web edition's online rooms, three-player
brawl, full 1,066-entry roster, and move editor are not implemented here.

## PSP controls

- Analog stick or D-pad: move
- Hold **L** + D-pad: aim
- **Cross:** basic attack
- **Square:** skill; **L + Square:** Protect
- **Circle:** dash
- **Triangle:** ultimate
- **R:** switch to the next available teammate
- **Start:** pause; **Select:** exit

In menus, use the D-pad, **Cross** to select, and **Circle** to go back. In the
Team Builder, **Up/Down** chooses a Pokémon, **Left/Right** selects a team,
**L/R** changes the party slot, and **Cross** saves it.

Save data is written to `ms0:/PSP/SAVEDATA/POKE2D/`. The sprite atlas is read
from `ms0:/PSP/GAME/PokemonArena2D/assets/pokemon_atlas.rgb565`; keep the
`assets` folder beside `EBOOT.PBP` when installing.

## Build and install

With PSPDEV / PSPSDK installed, run `make` in this directory. The output is
`EBOOT.PBP`. Copy the complete `PSP/GAME/PokemonArena2D/` folder from the ZIP
to the root of the Memory Stick, then launch it under **Game → Memory Stick**.

`PSP_LARGE_MEMORY=0`; the build does not request the PSP-2000 extended-memory
mode and targets PSP-1000.

To rebuild the atlas, run `sh tools/build_sprite_atlas.sh` (requires `curl`,
ImageMagick, and `ffmpeg`). The script uses the pinned upstream commit listed
in `assets/SPRITE_CREDITS.md` and emits the RGB565 asset expected by the game.

## Reference notes

- Web feature and control observations: [`docs/poke2d-web-reference.md`](docs/poke2d-web-reference.md)
- Earlier Empress IL2CPP package notes: [`docs/reverse-engineering.md`](docs/reverse-engineering.md)
