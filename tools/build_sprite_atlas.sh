#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ASSET_DIR="$ROOT/assets"
WORK_DIR="${TMPDIR:-/tmp}/pokemonarena2d-sprite-build"
UPSTREAM_COMMIT=53074f14d6947daf17af7e21db709497169a0e76
UPSTREAM=https://raw.githubusercontent.com/PMDCollab/SpriteCollab/$UPSTREAM_COMMIT

mkdir -p "$WORK_DIR/source" "$WORK_DIR/tiles"
tail -n +2 "$ASSET_DIR/roster-sprites.tsv" | while IFS="$(printf '\t')" read -r slot dex pokemon artist license source; do
    [ -n "$slot" ] || continue
    src="$WORK_DIR/source/$slot.png"
    tile="$WORK_DIR/tiles/$slot.png"
    curl -fsSL "$UPSTREAM/$source" -o "$src"
    magick "$src" -crop 40x40+0+0 +repage -filter point -resize 48x48 \
        -background '#ff00ff' -alpha remove -alpha off "$tile"
done

magick montage "$WORK_DIR"/tiles/*.png -tile 5x5 -geometry 48x48+0+0 \
    -background '#ff00ff' "$WORK_DIR/atlas.png"
ffmpeg -y -v error -i "$WORK_DIR/atlas.png" -frames:v 1 -pix_fmt rgb565le \
    -f rawvideo "$ASSET_DIR/pokemon_atlas.rgb565"

test "$(wc -c < "$ASSET_DIR/pokemon_atlas.rgb565")" -eq 115200
