# Pokémon sprite credits

The 25 battle sprites are cropped from the community contributed idle sheets in
[PMDCollab/SpriteCollab](https://github.com/PMDCollab/SpriteCollab), pinned to
commit [`53074f14d6947daf17af7e21db709497169a0e76`](https://github.com/PMDCollab/SpriteCollab/tree/53074f14d6947daf17af7e21db709497169a0e76).
The repository's submission policy licenses custom graphics under
[Creative Commons Attribution-NonCommercial 4.0](https://creativecommons.org/licenses/by-nc/4.0/).
Its full license text is included in
[`SPRITECOLLAB-LICENSE.md`](SPRITECOLLAB-LICENSE.md).

The build reads each species' `AnimData.xml` and uses the recorded frame width,
height, duration count and direction row. It trims transparent padding, fits
each complete sprite inside a 56×56 area with nearest-neighbor sampling, and
anchors it in a 64×64 tile. The static preview uses the first Idle frame. The
animation atlas stores four sampled frames each from Idle, Walk and an attack
animation, in that order for every species. Tiles are assembled in numeric
roster order before conversion to little-endian RGB565; this avoids
lexicographic filename sorting from assigning a different Pokémon's art to a
name and avoids clipping species with larger source frames. The artist,
species, and exact source path for each tile are recorded in
[`roster-sprites.tsv`](roster-sprites.tsv). The source URL
for a row is `https://github.com/PMDCollab/SpriteCollab/blob/53074f14d6947daf17af7e21db709497169a0e76/<source-path>`.

`tools/build_sprite_atlas.sh` rebuilds both atlases from that pinned SpriteCollab
revision. Set `SPRITECOLLAB_SOURCE_DIR` to a folder containing the downloaded
PNG and metadata files to build offline.

These Pokémon fan sprites remain subject to the rights of their respective
creators and the Pokémon rights holders. This is a non-commercial fan project,
not an official or endorsed Pokémon product. Pokémon names and characters are
owned by Nintendo, Creatures Inc., GAME FREAK inc., and The Pokémon Company.
