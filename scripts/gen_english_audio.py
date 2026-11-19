#!/usr/bin/env python3
"""Generate the English app's word clips (main/apps/english_assets/audio/en_<word>.ogg).

Reads the English words from main/apps/english_words.cc, speaks each with
edge-tts (Microsoft's online neural voices) and converts it to the Opus/OGG
format the firmware plays (16 kHz mono, 60 ms frames, like scripts/mp3_to_ogg.sh).
The clips are packed into the assets partition by main/CMakeLists.txt.

Needs network access, ffmpeg, and edge-tts (pip install edge-tts).

    python3 scripts/gen_english_audio.py            # only missing clips
    python3 scripts/gen_english_audio.py --force    # all clips again
"""

import argparse
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
WORDS = ROOT / "main/apps/english_words.cc"
OUT = ROOT / "main/apps/english_assets/audio"
# Asset names are at most 31 characters (char[32] in the assets table).
MAX_NAME = 31


def clip_name(word):
    """Must match ClipName() in main/apps/english_app.cc."""
    return "en_" + word.replace(" ", "_") + ".ogg"


def read_words():
    # Entries look like {"red", "màu đỏ", ...}: the first string is English.
    words = re.findall(r'\{"([a-z ]+)",\s*"', WORDS.read_text(encoding="utf-8"))
    return list(dict.fromkeys(words))  # Unique, in order


def main():
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--voice", default="en-US-AnaNeural", help="edge-tts voice (a child's voice by default)")
    parser.add_argument("--rate", default="-10%", help="speaking rate, slower helps learners")
    parser.add_argument("--force", action="store_true", help="regenerate existing clips")
    args = parser.parse_args()

    edge_tts = shutil.which("edge-tts")
    if not edge_tts or not shutil.which("ffmpeg"):
        sys.exit("edge-tts and ffmpeg must be on PATH")

    words = read_words()
    if not words:
        sys.exit(f"no words found in {WORDS}")
    OUT.mkdir(parents=True, exist_ok=True)
    wanted = set()
    with tempfile.TemporaryDirectory() as temp:
        for word in words:
            name = clip_name(word)
            if len(name) > MAX_NAME:
                sys.exit(f"clip name too long for the assets table: {name}")
            wanted.add(name)
            target = OUT / name
            if target.exists() and not args.force:
                continue
            mp3 = Path(temp) / "word.mp3"
            subprocess.run([edge_tts, "--voice", args.voice, f"--rate={args.rate}",
                            "--text", word, "--write-media", str(mp3)], check=True,
                           capture_output=True)
            # Trim the leading silence so the word plays as soon as it is tapped.
            subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(mp3),
                            "-af", "silenceremove=start_periods=1:start_threshold=-45dB",
                            "-c:a", "libopus", "-b:a", "16k", "-ac", "1", "-ar", "16000",
                            "-frame_duration", "60", str(target)], check=True)
            print(f"{name}: {target.stat().st_size} bytes")

    stale = [p for p in OUT.glob("en_*.ogg") if p.name not in wanted]
    for path in stale:
        path.unlink()
        print(f"removed {path.name}")
    total = sum(p.stat().st_size for p in OUT.glob("en_*.ogg"))
    print(f"{len(wanted)} clips, {total // 1024} KB in {OUT.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
