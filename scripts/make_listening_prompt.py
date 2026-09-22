#!/usr/bin/env python3
"""Install a spoken "you can talk now" cue into a locale.

By default the device plays a short tone when it starts listening. A tone does
not tell a child it is their turn. Drop a recording in here and the firmware
plays that instead; with no such asset it keeps using the tone.

The cue plays only when listening starts from an explicit action, meaning the
wake word or a button press, not after every reply, so it does not become
noise. Keep it under about two seconds.

Where the audio comes from
--------------------------
Record it yourself (a parent's own voice works well on a child's device), or
use any text-to-speech you have. Something like:

    Vietnamese  "Mây nghe nè!"  /  "Con nói đi"
    English     "I'm listening" /  "Your turn"

Usage
-----
    ./scripts/make_listening_prompt.py recording.wav
    ./scripts/make_listening_prompt.py prompt.mp3 --locale en-US
    ./scripts/make_listening_prompt.py --remove --locale vi-VN

Then rebuild and flash: idf.py build flash
"""

import argparse
import pathlib
import shutil
import subprocess
import sys

# Matches the other bundled locale sounds: Opus in Ogg, mono, 24 kHz, 60 ms.
SAMPLE_RATE = 24000
BITRATE = "32k"
FRAME_DURATION = 60
ASSET_NAME = "listening.ogg"
MAX_SECONDS = 3.0

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent
LOCALES_DIR = REPO_ROOT / "main" / "assets" / "locales"


def active_locale() -> str:
    """Locale selected in sdkconfig, falling back to en-US."""
    sdkconfig = REPO_ROOT / "sdkconfig"
    if sdkconfig.is_file():
        for line in sdkconfig.read_text(encoding="utf-8", errors="ignore").splitlines():
            if line.startswith("CONFIG_LANGUAGE_") and line.rstrip().endswith("=y"):
                code = line[len("CONFIG_LANGUAGE_"):-len("=y")]
                parts = code.split("_")
                if len(parts) == 2:
                    candidate = f"{parts[0].lower()}-{parts[1].upper()}"
                    if (LOCALES_DIR / candidate).is_dir():
                        return candidate
    return "en-US"


def duration_of(path: pathlib.Path) -> float | None:
    if shutil.which("ffprobe") is None:
        return None
    result = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration",
         "-of", "default=nw=1:nk=1", str(path)],
        capture_output=True, text=True)
    try:
        return float(result.stdout.strip())
    except ValueError:
        return None


def touch_language_json(locale_dir: pathlib.Path) -> None:
    """Force lang_config.h to be regenerated.

    The generator's inputs are language.json and the set of sound files, so
    adding a file re-runs it but *removing* one does not: the build would keep
    a stale header declaring a symbol that is no longer embedded, and fail to
    link. Touching language.json makes the header regenerate either way.
    """
    target = locale_dir / "language.json"
    if target.is_file():
        target.touch()


def main() -> int:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("source", nargs="?", type=pathlib.Path,
                        help="recording or TTS output to install")
    parser.add_argument("--locale", default=None,
                        help="locale directory (default: the one selected in sdkconfig)")
    parser.add_argument("--remove", action="store_true",
                        help="remove the cue and go back to the tone")
    args = parser.parse_args()

    locale = args.locale or active_locale()
    locale_dir = LOCALES_DIR / locale
    if not locale_dir.is_dir():
        available = ", ".join(sorted(p.name for p in LOCALES_DIR.iterdir() if p.is_dir()))
        print(f"No such locale: {locale}\nAvailable: {available}", file=sys.stderr)
        return 1
    dest = locale_dir / ASSET_NAME

    if args.remove:
        if dest.is_file():
            dest.unlink()
            touch_language_json(locale_dir)
            print(f"Removed {dest.relative_to(REPO_ROOT)}; the tone will be used again.")
        else:
            print(f"Nothing to remove: {dest.relative_to(REPO_ROOT)} does not exist.")
        print("Rebuild for this to take effect:")
        print("  idf.py reconfigure && idf.py build")
        return 0

    if args.source is None:
        parser.error("give an audio file to install, or pass --remove")
    if not args.source.is_file():
        print(f"No such file: {args.source}", file=sys.stderr)
        return 1
    if shutil.which("ffmpeg") is None:
        print("ffmpeg not found. Install it with: sudo apt install ffmpeg", file=sys.stderr)
        return 1

    result = subprocess.run(
        ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
         "-i", str(args.source),
         "-vn", "-map_metadata", "-1",
         "-af", "loudnorm=I=-16:TP=-1.5:LRA=11",  # even loudness with the other cues
         "-c:a", "libopus", "-b:a", BITRATE,
         "-ac", "1", "-ar", str(SAMPLE_RATE),
         "-frame_duration", str(FRAME_DURATION),
         "-f", "ogg", str(dest)],
        capture_output=True, text=True)
    if result.returncode != 0:
        print(f"ffmpeg failed:\n{result.stderr.strip()}", file=sys.stderr)
        dest.unlink(missing_ok=True)
        return 1

    seconds = duration_of(dest)
    size_kb = dest.stat().st_size / 1024
    length = f"{seconds:.1f}s, " if seconds is not None else ""
    print(f"Installed {dest.relative_to(REPO_ROOT)}  ({length}{size_kb:.0f} KB)")
    if seconds is not None and seconds > MAX_SECONDS:
        print(f"  Warning: {seconds:.1f}s is long for a cue that plays before every "
              f"conversation. Aim for under {MAX_SECONDS:.0f}s.")
    touch_language_json(locale_dir)
    print("\nRebuild and flash for the device to use it:")
    print("  idf.py reconfigure && idf.py build flash")
    return 0


if __name__ == "__main__":
    sys.exit(main())
