#!/usr/bin/env python3
"""Generate the English app's word pictures (main/apps/english_assets/images/img_<word>.bin).

Each picture is a Noto Color Emoji image (Apache License 2.0,
https://github.com/googlefonts/noto-emoji) scaled to SIZE x SIZE and stored as
raw LVGL RGB565A8 pixels: SIZE*SIZE little-endian RGB565 values, then SIZE*SIZE
alpha bytes. The firmware shows it straight from the memory-mapped assets
partition, so it costs no RAM. main/CMakeLists.txt packs the files into the
assets partition with the word clips.

Every word in a Picture::kImage topic of main/apps/english_words.cc needs an
entry in EMOJI below; the script stops if one is missing.

Needs network access (to download the emoji) and Pillow.

    python3 scripts/gen_english_images.py            # only missing pictures
    python3 scripts/gen_english_images.py --force    # all pictures again
"""

import argparse
import io
import re
import sys
import urllib.request
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
WORDS = ROOT / "main/apps/english_words.cc"
OUT = ROOT / "main/apps/english_assets/images"
SIZE = 88  # Must match kImageSize in main/apps/english_app.cc
MAX_NAME = 31  # Asset names are char[32] in the assets table
URL = "https://raw.githubusercontent.com/googlefonts/noto-emoji/main/2D/png/128/emoji_u{}.png"

# English word -> emoji code point(s), as in the Noto file names.
EMOJI = {
    # Things
    "house": "1f3e0", "phone": "1f4f1", "camera": "1f4f7", "star": "2b50",
    "heart": "2764", "key": "1f511", "lock": "1f512", "watch": "231a",
    "headphones": "1f3a7", "glasses": "1f453", "letter": "2709", "robot": "1f916",
    "alarm clock": "23f0", "calendar": "1f4c5", "bell": "1f514",
    # Animals
    "cat": "1f431", "dog": "1f436", "bird": "1f426", "fish": "1f41f", "duck": "1f986",
    "chicken": "1f414", "pig": "1f437", "cow": "1f42e", "horse": "1f434",
    "monkey": "1f435", "elephant": "1f418", "tiger": "1f42f", "rabbit": "1f430",
    "bear": "1f43b",
    # Fruits
    "apple": "1f34e", "banana": "1f34c", "orange": "1f34a", "mango": "1f96d",
    "grapes": "1f347", "watermelon": "1f349", "lemon": "1f34b", "pineapple": "1f34d",
    "coconut": "1f965", "strawberry": "1f353", "peach": "1f351",
    # Body
    "face": "1f642", "tongue": "1f445", "eye": "1f441", "ear": "1f442", "nose": "1f443",
    "mouth": "1f444", "teeth": "1f9b7", "hand": "270b", "foot": "1f9b6", "arm": "1f4aa",
    "leg": "1f9b5",
    # Food
    "bread": "1f35e", "rice": "1f35a", "egg": "1f95a", "milk": "1f95b", "cake": "1f370",
    "candy": "1f36c", "ice cream": "1f366", "pizza": "1f355", "noodles": "1f35c",
    "cheese": "1f9c0", "cookie": "1f36a", "juice": "1f9c3", "water": "1f4a7",
    # Vegetables
    "carrot": "1f955", "corn": "1f33d", "potato": "1f954", "tomato": "1f345",
    "cucumber": "1f952", "onion": "1f9c5", "garlic": "1f9c4", "chili": "1f336",
    "mushroom": "1f344", "broccoli": "1f966", "eggplant": "1f346",
    # Vehicles
    "car": "1f697", "bus": "1f68c", "bicycle": "1f6b2", "motorbike": "1f6f5",
    "train": "1f686", "airplane": "2708", "boat": "26f5", "ship": "1f6a2",
    "truck": "1f69a", "taxi": "1f695", "rocket": "1f680", "helicopter": "1f681",
    # Nature
    "sun": "2600", "moon": "1f319", "cloud": "2601", "rain": "1f327", "snow": "2744",
    "rainbow": "1f308", "tree": "1f333", "flower": "1f338", "leaf": "1f343",
    "mountain": "26f0", "sea": "1f30a", "fire": "1f525",
    # Clothes
    "shirt": "1f455", "dress": "1f457", "pants": "1f456", "hat": "1f452", "cap": "1f9e2",
    "shoes": "1f45f", "boots": "1f462", "socks": "1f9e6", "coat": "1f9e5",
    "scarf": "1f9e3", "gloves": "1f9e4", "bag": "1f45c",
    # Family
    "father": "1f468", "mother": "1f469", "baby": "1f476", "boy": "1f466",
    "girl": "1f467", "grandfather": "1f474", "grandmother": "1f475", "family": "1f46a",
    # School
    "book": "1f4d6", "notebook": "1f4d2", "pencil": "270f", "pen": "1f58a",
    "crayon": "1f58d", "ruler": "1f4cf", "scissors": "2702", "backpack": "1f392",
    "paint": "1f3a8", "computer": "1f4bb", "teacher": "1f469_200d_1f3eb", "school": "1f3eb",
    # Toys
    "ball": "26bd", "kite": "1fa81", "teddy bear": "1f9f8", "balloon": "1f388",
    "gift": "1f381", "drum": "1f941", "guitar": "1f3b8", "puzzle": "1f9e9", "doll": "1fa86",
}


def image_name(word):
    """Must match ImageName() in main/apps/english_app.cc."""
    return "img_" + word.replace(" ", "_") + ".bin"


def image_words():
    """English words of the topics pictured with kImage, in file order."""
    text = WORDS.read_text(encoding="utf-8")
    arrays = dict(re.findall(r"constexpr Word (k\w+)\[\] = \{(.*?)\n\};", text, re.S))
    topics = re.findall(r"Picture::kImage, (k\w+),", text)
    words = []
    for topic in topics:
        words += re.findall(r'\{"([a-z ]+)",\s*"', arrays[topic])
    return list(dict.fromkeys(words))


def to_rgb565a8(image):
    image = image.convert("RGBA")
    color = bytearray()
    alpha = bytearray()
    for r, g, b, a in image.getdata():
        value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        color += value.to_bytes(2, "little")
        alpha.append(a)
    return bytes(color + alpha)


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--force", action="store_true", help="regenerate existing pictures")
    args = parser.parse_args()

    words = image_words()
    if not words:
        sys.exit(f"no Picture::kImage topics found in {WORDS}")
    missing = [w for w in words if w not in EMOJI]
    if missing:
        sys.exit(f"no emoji for: {', '.join(missing)} (add them to EMOJI)")

    OUT.mkdir(parents=True, exist_ok=True)
    wanted = set()
    for word in words:
        name = image_name(word)
        if len(name) > MAX_NAME:
            sys.exit(f"picture name too long for the assets table: {name}")
        wanted.add(name)
        target = OUT / name
        if target.exists() and not args.force:
            continue
        with urllib.request.urlopen(URL.format(EMOJI[word]), timeout=30) as response:
            png = response.read()
        image = Image.open(io.BytesIO(png)).resize((SIZE, SIZE), Image.LANCZOS)
        target.write_bytes(to_rgb565a8(image))
        print(f"{name}: emoji U+{EMOJI[word].upper()}")

    for path in OUT.glob("img_*.bin"):
        if path.name not in wanted:
            path.unlink()
            print(f"removed {path.name}")
    total = sum(p.stat().st_size for p in OUT.glob("img_*.bin"))
    print(f"{len(wanted)} pictures, {total // 1024} KB in {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
