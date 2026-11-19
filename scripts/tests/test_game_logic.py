"""Build and run the host test for the games' rules (main/apps/game_logic.cc)."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("g++"), "g++ is not installed")
class GameLogicTest(unittest.TestCase):
    def test_rules(self):
        with tempfile.TemporaryDirectory() as temp:
            binary = Path(temp) / "game_logic_test"
            subprocess.run(
                ["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                 f"-I{ROOT / 'main/apps'}", str(ROOT / "main/apps/game_logic.cc"),
                 str(ROOT / "scripts/tests/game_logic_test.cc"), "-o", str(binary)],
                check=True, capture_output=True, text=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
