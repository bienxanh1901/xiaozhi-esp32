"""Build and run the host test for the English app's words and quiz (main/apps/english_words.cc)."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
# Only the glyph macros are used; the header has no other dependencies.
ICONS = ROOT / "managed_components/78__xiaozhi-fonts/include"


@unittest.skipUnless(shutil.which("g++"), "g++ is not installed")
@unittest.skipUnless((ICONS / "material_symbols.h").exists(), "managed components are not installed")
class EnglishWordsTest(unittest.TestCase):
    def test_words_and_quiz(self):
        with tempfile.TemporaryDirectory() as temp:
            binary = Path(temp) / "english_words_test"
            subprocess.run(
                ["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                 f"-I{ROOT / 'main/apps'}", f"-I{ICONS}", str(ROOT / "main/apps/english_words.cc"),
                 str(ROOT / "scripts/tests/english_words_test.cc"), "-o", str(binary)],
                check=True, capture_output=True, text=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
