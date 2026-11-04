#!/usr/bin/env python3
"""Build numeric-order RGB565 sprite and animation atlases from SpriteCollab sheets."""

from __future__ import annotations

import csv
import io
import os
from pathlib import Path
import sys
import urllib.request
import xml.etree.ElementTree as ET

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "assets" / "roster-sprites.tsv"
ASSET_DIR = ROOT / "assets"
COMMIT = "53074f14d6947daf17af7e21db709497169a0e76"
BASE_URL = f"https://raw.githubusercontent.com/PMDCollab/SpriteCollab/{COMMIT}"
SIDE = 64
FRAMES_PER_ANIMATION = 4
ANIMATION_COUNT = 3
TRANSPARENT = (255, 0, 255, 255)
PREFERRED_ACTIONS = (
    "Attack", "Shoot", "Strike", "QuickStrike", "Swing", "Charge",
    "Double", "Appeal", "RearUp", "Punch", "Beam", "SpAttack",
)


def fetch(url: str) -> bytes:
    request = urllib.request.Request(url, headers={"User-Agent": "PSP-PokemonArena2D atlas builder"})
    with urllib.request.urlopen(request, timeout=45) as response:
        return response.read()


def local_or_remote(slot: int, base_path: str, kind: str, local_dir: Path | None) -> bytes:
    if local_dir:
        local_path = local_dir / f"{slot:02}-{kind}.png"
        if kind == "metadata":
            local_path = local_dir / f"{slot:02}-metadata.xml"
        if local_path.exists():
            return local_path.read_bytes()
    return fetch(f"{BASE_URL}/{base_path}")


def get_anim(root: ET.Element, name: str) -> ET.Element:
    for anim in root.findall(".//Anim"):
        if anim.findtext("Name") == name and anim.findtext("FrameWidth") and anim.findtext("FrameHeight"):
            return anim
    raise ValueError(f"Animation {name!r} has no frame dimensions")


def frame_count(anim: ET.Element) -> int:
    count = len(anim.findall("./Durations/Duration"))
    if count == 0:
        raise ValueError(f"Animation {anim.findtext('Name')} has no frame durations")
    return count


def sheet_animation(root: ET.Element, image: Image.Image, preferred: tuple[str, ...]) -> tuple[str, ET.Element]:
    by_name = {anim.findtext("Name"): anim for anim in root.findall(".//Anim") if anim.findtext("Name")}
    choices = list(preferred) + [
        anim.findtext("Name") for anim in root.findall(".//Anim")
        if anim.findtext("Name") and anim.findtext("CopyOf") is None
    ]
    for name in choices:
        anim = by_name.get(name)
        if anim is None or not anim.findtext("FrameWidth") or not anim.findtext("FrameHeight"):
            continue
        if anim.findtext("CopyOf"):
            continue
        width = int(anim.findtext("FrameWidth"))
        height = int(anim.findtext("FrameHeight"))
        count = frame_count(anim)
        if image.width == width * count and image.height == height * 8:
            return name, anim
    raise ValueError(f"Could not match an animation sheet of size {image.size} to AnimData.xml")


def frame_indices(count: int) -> list[int]:
    return [round(i * max(0, count - 1) / (FRAMES_PER_ANIMATION - 1))
            for i in range(FRAMES_PER_ANIMATION)]


def crop_frame(sheet: Image.Image, anim: ET.Element, index: int, row: int = 2) -> Image.Image:
    fw = int(anim.findtext("FrameWidth"))
    fh = int(anim.findtext("FrameHeight"))
    if row >= sheet.height // fh:
        row = 0
    frame = sheet.crop((index * fw, row * fh, (index + 1) * fw, (row + 1) * fh)).convert("RGBA")
    alpha = frame.getchannel("A")
    bounds = alpha.getbbox()
    if bounds:
        frame = frame.crop(bounds)
    return frame


def pack_frame(sheet: Image.Image, anim: ET.Element, index: int) -> Image.Image:
    frame = crop_frame(sheet, anim, index)
    if frame.width == 0 or frame.height == 0:
        return Image.new("RGBA", (SIDE, SIDE), TRANSPARENT)
    frame.thumbnail((SIDE - 8, SIDE - 8), Image.Resampling.NEAREST)
    tile = Image.new("RGBA", (SIDE, SIDE), TRANSPARENT)
    # Anchor every cel at the same baseline so animation frames do not jitter vertically.
    x = (SIDE - frame.width) // 2
    y = SIDE - 4 - frame.height
    tile.alpha_composite(frame, (x, y))
    return tile


def to_rgb565(tile: Image.Image) -> bytes:
    rgba = tile.tobytes()
    out = bytearray(SIDE * SIDE * 2)
    pos = 0
    for i in range(0, len(rgba), 4):
        r, g, b, a = rgba[i:i + 4]
        if a < 128:
            value = 0xF81F
        else:
            value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        out[pos] = value & 0xFF
        out[pos + 1] = value >> 8
        pos += 2
    return bytes(out)


def add_preview(preview: Image.Image, tile: Image.Image, index: int) -> None:
    preview.alpha_composite(tile, ((index % 5) * SIDE, (index // 5) * SIDE))


def main() -> None:
    local_value = os.environ.get("SPRITECOLLAB_SOURCE_DIR")
    local_dir = Path(local_value) if local_value else None
    if local_dir and not local_dir.is_dir():
        raise SystemExit(f"SPRITECOLLAB_SOURCE_DIR is not a directory: {local_dir}")

    static_data = bytearray()
    animation_data = bytearray()
    static_preview = Image.new("RGBA", (5 * SIDE, 5 * SIDE), TRANSPARENT)
    anim_preview = Image.new("RGBA", (5 * SIDE, 12 * SIDE), TRANSPARENT)

    with MANIFEST.open(newline="", encoding="utf-8") as manifest:
        rows = list(csv.DictReader(manifest, delimiter="\t"))
    if len(rows) != 25:
        raise SystemExit(f"Expected 25 roster rows, found {len(rows)}")

    for slot, row in enumerate(rows):
        idle_path = row["source"]
        parent = str(Path(idle_path).parent)
        if parent == ".":
            parent = ""
        metadata_path = f"{parent + '/' if parent else ''}AnimData.xml"
        walk_path = f"{parent + '/' if parent else ''}Walk-Anim.png"

        metadata = ET.fromstring(local_or_remote(slot, metadata_path, "metadata", local_dir))
        idle_bytes = local_or_remote(slot, idle_path, "idle", local_dir)
        walk_bytes = local_or_remote(slot, walk_path, "walk", local_dir)
        idle = Image.open(io.BytesIO(idle_bytes)).convert("RGBA")
        walk = Image.open(io.BytesIO(walk_bytes)).convert("RGBA")
        idle_anim = get_anim(metadata, "Idle")
        walk_anim = get_anim(metadata, "Walk")

        if local_dir and (local_dir / f"{slot:02}-action.png").exists():
            action_bytes = (local_dir / f"{slot:02}-action.png").read_bytes()
        else:
            action_bytes = None
            ordered = list(PREFERRED_ACTIONS)
            ordered += [a.findtext("Name") for a in metadata.findall(".//Anim")
                        if a.findtext("Name") and not a.findtext("CopyOf")]
            for name in dict.fromkeys(ordered):
                anim = next((a for a in metadata.findall(".//Anim")
                             if a.findtext("Name") == name and not a.findtext("CopyOf")), None)
                if anim is None or not anim.findtext("FrameWidth") or not anim.findtext("FrameHeight"):
                    continue
                path = f"{parent + '/' if parent else ''}{name}-Anim.png"
                try:
                    candidate = fetch(f"{BASE_URL}/{path}")
                except Exception:
                    continue
                with Image.open(io.BytesIO(candidate)) as image:
                    try:
                        sheet_animation(metadata, image, (name,))
                    except ValueError:
                        continue
                action_bytes = candidate
                break
            if action_bytes is None:
                raise SystemExit(f"No usable attack animation found for {row['pokemon']}")

        action = Image.open(io.BytesIO(action_bytes)).convert("RGBA")
        action_name, action_anim = sheet_animation(metadata, action, PREFERRED_ACTIONS)
        idle_tiles = [pack_frame(idle, idle_anim, i) for i in frame_indices(frame_count(idle_anim))]
        walk_tiles = [pack_frame(walk, walk_anim, i) for i in frame_indices(frame_count(walk_anim))]
        action_tiles = [pack_frame(action, action_anim, i) for i in frame_indices(frame_count(action_anim))]

        static_data.extend(to_rgb565(idle_tiles[0]))
        add_preview(static_preview, idle_tiles[0], slot)
        for group_index, group in enumerate((idle_tiles, walk_tiles, action_tiles)):
            for frame_index, tile in enumerate(group):
                atlas_index = slot * 12 + group_index * 4 + frame_index
                animation_data.extend(to_rgb565(tile))
                add_preview(anim_preview, tile, atlas_index)

        print(f"{slot:02} {row['pokemon']}: Idle {frame_count(idle_anim)}f, "
              f"Walk {frame_count(walk_anim)}f, {action_name} {frame_count(action_anim)}f")

    (ASSET_DIR / "pokemon_atlas.rgb565").write_bytes(static_data)
    (ASSET_DIR / "pokemon_animations.rgb565").write_bytes(animation_data)
    preview_dir = Path(os.environ.get("SPRITE_ATLAS_PREVIEW_DIR", "/tmp/pokemon-arena-atlas-preview"))
    preview_dir.mkdir(parents=True, exist_ok=True)
    static_preview.convert("RGB").resize((5 * SIDE * 2, 5 * SIDE * 2),
                                         Image.Resampling.NEAREST).save(preview_dir / "pokemon-atlas.png")
    anim_preview.convert("RGB").resize((5 * SIDE, 12 * SIDE),
                                       Image.Resampling.NEAREST).save(preview_dir / "pokemon-animations.png")
    print(f"Wrote {len(static_data)} byte static atlas and {len(animation_data)} byte animation atlas")
    print(f"Previews: {preview_dir}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, ET.ParseError) as exc:
        print(f"atlas build failed: {exc}", file=sys.stderr)
        sys.exit(1)
