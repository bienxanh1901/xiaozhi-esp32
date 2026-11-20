#!/usr/bin/env python3
"""Generate the English app's word pictures (main/apps/english_assets/images/img_<word>.bin).

Each picture is a Noto Color Emoji image (Apache License 2.0,
https://github.com/googlefonts/noto-emoji) scaled to SIZE x SIZE and stored as
raw LVGL RGB565A8 pixels: SIZE*SIZE little-endian RGB565 values, then SIZE*SIZE
alpha bytes. The firmware shows it straight from the memory-mapped assets
partition, so it costs no RAM. main/CMakeLists.txt packs the files into the
assets partition with the word clips.

Every word in a Picture::kImage topic of main/apps/english_words.cc needs an
entry in EMOJI or DRAWN below; the script stops if one is missing. DRAWN
pictures are drawn here instead, where the emoji is a poor fit (the triangle
emoji is small).

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

from PIL import Image, ImageDraw

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
    # Jobs
    "doctor": "1f9d1_200d_2695", "farmer": "1f9d1_200d_1f33e", "cook": "1f9d1_200d_1f373",
    "police": "1f46e", "firefighter": "1f9d1_200d_1f692", "pilot": "1f9d1_200d_2708",
    "singer": "1f9d1_200d_1f3a4", "artist": "1f9d1_200d_1f3a8",
    "astronaut": "1f9d1_200d_1f680", "builder": "1f477",
    # Feelings
    "happy": "1f600", "sad": "1f622", "angry": "1f620", "scared": "1f628", "tired": "1f62b",
    "sleepy": "1f634", "surprised": "1f62e", "sick": "1f912", "cold": "1f976", "hot": "1f975",
    "love": "1f970", "silly": "1f61c",
    # Home
    "bed": "1f6cf", "chair": "1fa91", "door": "1f6aa", "window": "1fa9f",
    "light bulb": "1f4a1", "toilet": "1f6bd", "bathtub": "1f6c1", "sofa": "1f6cb",
    "mirror": "1fa9e", "clock": "1f570", "television": "1f4fa", "broom": "1f9f9",
    "soap": "1f9fc", "toothbrush": "1faa5",
    # Sports
    "soccer": "26bd", "basketball": "1f3c0", "tennis": "1f3be", "swimming": "1f3ca",
    "running": "1f3c3", "badminton": "1f3f8", "volleyball": "1f3d0", "baseball": "26be",
    "cycling": "1f6b4", "skating": "26f8", "boxing": "1f94a", "golf": "26f3",
    # Sea
    "whale": "1f433", "dolphin": "1f42c", "shark": "1f988", "octopus": "1f419",
    "crab": "1f980", "turtle": "1f422", "jellyfish": "1fabc", "shrimp": "1f990",
    "seal": "1f9ad", "penguin": "1f427",
    # Insects
    "butterfly": "1f98b", "bee": "1f41d", "ant": "1f41c", "ladybug": "1f41e",
    "spider": "1f577", "snail": "1f40c", "worm": "1fab1", "mosquito": "1f99f",
    "fly": "1fab0", "beetle": "1fab2", "caterpillar": "1f41b",
    # Weather
    "wind": "1f32c", "storm": "26c8", "lightning": "26a1", "fog": "1f32b",
    "umbrella": "2614",
    # Shapes
    "circle": "1f535", "square": "1f7e5", "triangle": "1f53a", "diamond": "1f537",
    # Actions
    "eat": "1f60b", "drink": "1f964", "sleep": "1f634", "read": "1f4d6",
    "write": "270d", "sing": "1f3a4", "dance": "1f483", "laugh": "1f602",
    "cry": "1f62d", "wave": "1f44b",
    # Instruments
    "piano": "1f3b9", "violin": "1f3bb", "trumpet": "1f3ba", "saxophone": "1f3b7",
    "accordion": "1fa97", "banjo": "1fa95",
    # Birds
    "owl": "1f989", "eagle": "1f985", "parrot": "1f99c", "swan": "1f9a2",
    "flamingo": "1f9a9", "peacock": "1f99a",
    # Places
    "hospital": "1f3e5", "shop": "1f3ea", "park": "1f3de", "beach": "1f3d6",
    "castle": "1f3f0",
    # Winter
    "snowman": "26c4",
    # Zoo
    "lion": "1f981", "giraffe": "1f992", "zebra": "1f993", "panda": "1f43c",
    "kangaroo": "1f998", "crocodile": "1f40a", "snake": "1f40d",
    # Space
    "earth": "1f30f", "planet": "1fa90", "alien": "1f47d", "telescope": "1f52d",
}


def draw_triangle():
    # Drawn large and shrunk, for smooth edges; red like the square.
    big = SIZE * 4
    image = Image.new("RGBA", (big, big), (0, 0, 0, 0))
    margin = big // 16
    ImageDraw.Draw(image).polygon(
        [(big // 2, margin), (big - margin, big - margin * 2), (margin, big - margin * 2)],
        fill=(0xF4, 0x43, 0x36, 255))
    return image.resize((SIZE, SIZE), Image.LANCZOS)


# English word -> function drawing its picture, for words without a good emoji.
DRAWN = {"triangle": draw_triangle}


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
    missing = [w for w in words if w not in EMOJI and w not in DRAWN]
    if missing:
        sys.exit(f"no emoji for: {', '.join(missing)} (add them to EMOJI or DRAWN)")

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
        if word in DRAWN:
            target.write_bytes(to_rgb565a8(DRAWN[word]()))
            print(f"{name}: drawn")
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
