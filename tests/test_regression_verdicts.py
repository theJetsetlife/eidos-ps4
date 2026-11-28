"""Check that test prerequisites and failures produce honest verdicts."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


parser = argparse.ArgumentParser()
parser.add_argument("--armdata", required=True)
parser.add_argument("--bmap", required=True)
parser.add_argument("--ui", required=True)
parser.add_argument("--platform")
parser.add_argument("--ctest", required=True)
parser.add_argument("--build", required=True)
args, rest = parser.parse_known_args()


class RegressionVerdicts(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(prefix="eidos-verdict-")
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.env = os.environ.copy()
        self.env.update({
            "EIDOS_HOME": str(self.root),
            "EIDOS_USER_DIR": str(self.root),
            "BW_SAVE_DIR": str(self.root / "save"),
            "EIDOS_SETTINGS": str(self.root / "settings.ini"),
            "EIDOS_MODS_DIR": str(self.root / "mods"),
            "UI_KIT_SHEET": str(self.root / "sheet.png"),
        })

    def run_test(self, executable, *argv, env=None):
        return subprocess.run([executable, *map(str, argv)], cwd=self.root,
                              env=self.env if env is None else env,
                              capture_output=True, text=True, timeout=60)

    def registered_tests(self):
        result = subprocess.run([args.ctest, "--test-dir", args.build, "--show-only=json-v1"],
                                capture_output=True, text=True, timeout=60, check=True)
        return {item["name"]: item for item in json.loads(result.stdout)["tests"]}

    def test_missing_game_data_skips_only_asset_checks(self):
        missing = self.root / "empty" / "data"
        missing.mkdir(parents=True)
        for executable, extra in ((args.armdata, ()), (args.bmap, ()),
                                  (args.ui, ("--assets-only",))):
            with self.subTest(executable=executable):
                result = self.run_test(executable, missing, *extra)
                self.assertEqual(result.returncode, 77, result.stdout + result.stderr)
        result = self.run_test(args.ui, missing, "--synthetic-only")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertRegex(result.stdout, r"test_ui_kit: [1-9][0-9]* checks, 0 failures")

    def test_corrupt_arm9_is_a_failure(self):
        root = self.root / "bad-arm9"
        data = root / "data"
        data.mkdir(parents=True)
        (root / "arm9_dec.bin").write_bytes(b"\0")
        result = self.run_test(args.armdata, data)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("cannot read the required ARM9 range", result.stderr)

    def test_relative_root_checks_the_actual_arm9_path(self):
        (self.root / "arm9_dec.bin").write_bytes(b"\0")
        result = self.run_test(args.armdata, "missing-data")
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("cannot read the required ARM9 range", result.stderr)

    def test_corrupt_and_missing_font_in_an_existing_tree_fail(self):
        root = self.root / "game-data"
        archive = root / "a" / "0" / "2" / "3"
        archive.parent.mkdir(parents=True)
        for contents in (None, b"not a NARC archive"):
            if contents is not None:
                archive.write_bytes(contents)
            result = self.run_test(args.ui, root, "--assets-only")
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("FAIL large game font loads", result.stdout)

    def test_missing_map_archive_in_an_existing_tree_fails(self):
        root = self.root / "incomplete-map-data"
        (root / "a").mkdir(parents=True)
        result = self.run_test(args.bmap, root)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("FAIL map archive exists", result.stdout)

    if args.platform:
        def test_sdl_initialization_failure_is_not_a_pass(self):
            env = self.env.copy()
            env.update(SDL_VIDEODRIVER="eidos-invalid-driver", SDL_AUDIODRIVER="dummy")
            result = self.run_test(args.platform, env=env)
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
            self.assertIn("FAIL stacked: SDL initialization", result.stdout)

    def test_failed_python_package_generator_is_not_a_skip(self):
        executable = self.registered_tests()["test_mod_bw"]["command"][0]
        shim = self.root / "bin"
        shim.mkdir()
        if os.name == "nt":
            fake_python = shim / "python.cmd"
            fake_python.write_text('@if "%1"=="-c" (exit /b 0) else (exit /b 42)\n')
        else:
            fake_python = shim / "python"
            fake_python.write_text('#!/bin/sh\nif [ "$1" = "-c" ]; then exit 0; fi\nexit 42\n')
            fake_python.chmod(0o755)
        env = self.env.copy()
        env["PATH"] = str(shim) + os.pathsep + env.get("PATH", "")
        result = self.run_test(executable, env=env)
        self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertIn("FAIL python package generator", result.stdout)

    def test_ctest_registers_missing_data_as_skipped(self):
        tests = self.registered_tests()
        for name in ("test_armdata_real", "test_bmap", "test_ui_kit_assets",
                     "test_ui_kit_write_failure"):
            properties = {p["name"]: p["value"] for p in tests[name].get("properties", [])}
            self.assertEqual(properties.get("SKIP_RETURN_CODE"), 77, name)


if __name__ == "__main__":
    unittest.main(argv=[__file__, *rest])
