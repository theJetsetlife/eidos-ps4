"""Exercise the boot-test verdicts without game data or real user files."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


HERE = Path(__file__).resolve().parent
parser = argparse.ArgumentParser()
parser.add_argument("--cmake", required=True)
parser.add_argument("--ctest", required=True)
parser.add_argument("--boot", required=True)
parser.add_argument("--skip-regex", required=True)
args, rest = parser.parse_known_args()


class BootHarness(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="eidos-boot-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name).resolve()
        self.data = self.root / "data"
        (self.data / "a").mkdir(parents=True)
        (self.root / "arm9_dec.bin").touch()
        self.env = os.environ.copy()
        self.env["EIDOS_TEST_ALLOWED_ROOT"] = self.root.as_posix()
        for name in ("EIDOS_HOME", "EIDOS_USER_DIR", "BW_SAVE_DIR", "EIDOS_SETTINGS", "EIDOS_MODS_DIR"):
            self.env[name] = "/inherited-user-data-must-not-be-used"

    def run_boot(self, harness, mode, *options):
        self.env["EIDOS_TEST_BOOT_MODE"] = mode
        return subprocess.run(
            [args.cmake, f"-DBOOT={args.boot}", f"-DDATA={self.data.as_posix()}",
             "-DFRAMES=1", *options, "-P", str(HERE / harness)],
            cwd=self.root, env=self.env, text=True, capture_output=True, timeout=30,
        )

    def test_scenario_requires_clean_exit_and_expected_state(self):
        for mode, expected in (("ok", True), ("error_after", False),
                               ("panic", False), ("missing", False),
                               ("wrong_overlay", False), ("skip_marker_error", False),
                               ("ubsan", False)):
            with self.subTest(mode=mode):
                result = self.run_boot("boot_scenario.cmake", mode, "-DNAME=audit",
                                       "-DRESIDENT_HAS=93",
                                       "-DENV=EIDOS_HOME=/unsafe;EIDOS_USER_DIR=/unsafe")
                self.assertEqual(result.returncode == 0, expected, result.stdout + result.stderr)

    def test_lp64_diagnostic_must_run_to_completion(self):
        for mode, expected in (("ok", True), ("identity", True),
                               ("error_after", False), ("panic", False),
                               ("missing", False), ("refusal", False), ("ubsan", False)):
            with self.subTest(mode=mode):
                result = self.run_boot("boot_run.cmake", mode, "-DLP64=1")
                self.assertEqual(result.returncode == 0, expected, result.stdout + result.stderr)

    def test_expected_stop_requires_a_nonzero_exit(self):
        for mode, expected in (("known_stop_zero", False), ("known_stop_error", True)):
            with self.subTest(mode=mode):
                result = self.run_boot("boot_run.cmake", mode, "-DLP64=0", "-DMIN_FRAMES=1",
                                       "-DKNOWN_STOP=PANIC in boot stub reached")
                self.assertEqual(result.returncode == 0, expected, result.stdout + result.stderr)

    def test_no_data_is_explicitly_skipped(self):
        result = self.run_boot("boot_scenario.cmake", "ok", "-DDATA=/nonexistent/eidos-test-data")
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("SKIPPED: no game data", result.stdout + result.stderr)

    def test_ctest_skip_does_not_mask_a_failed_child(self):
        def ctest(mode, data):
            self.env["EIDOS_TEST_BOOT_MODE"] = mode
            command = [args.cmake, f"-DBOOT={args.boot}", f"-DDATA={data}",
                       "-DNAME=audit", "-DFRAMES=1", "-P", str(HERE / "boot_scenario.cmake")]
            quoted = " ".join(f"[=[{part}]=]" for part in command)
            (self.root / "CTestTestfile.cmake").write_text(
                f"add_test(audit {quoted})\n"
                f"set_tests_properties(audit PROPERTIES SKIP_REGULAR_EXPRESSION [=[{args.skip_regex}]=])\n"
            )
            return subprocess.run([args.ctest, "--test-dir", str(self.root), "--output-on-failure"],
                                  cwd=self.root, env=self.env, text=True, capture_output=True, timeout=30)

        result = ctest("skip_marker_error", self.data.as_posix())
        self.assertNotEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("***Skipped", result.stdout + result.stderr)
        result = ctest("ok", (self.root / "missing-data").as_posix())
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertIn("***Skipped", result.stdout + result.stderr)


if __name__ == "__main__":
    unittest.main(argv=[__file__, *rest])
