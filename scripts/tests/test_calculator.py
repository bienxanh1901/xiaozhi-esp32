"""Build and run the host test for the calculator (main/apps/calculator.cc)."""

import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]


@unittest.skipUnless(shutil.which("g++"), "g++ is not installed")
class CalculatorTest(unittest.TestCase):
    def test_calculator(self):
        with tempfile.TemporaryDirectory() as temp:
            binary = Path(temp) / "calculator_test"
            subprocess.run(
                ["g++", "-std=c++20", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                 f"-I{ROOT / 'main/apps'}", str(ROOT / "main/apps/calculator.cc"),
                 str(ROOT / "scripts/tests/calculator_test.cc"), "-o", str(binary)],
                check=True, capture_output=True, text=True)
            result = subprocess.run([str(binary)], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main()
