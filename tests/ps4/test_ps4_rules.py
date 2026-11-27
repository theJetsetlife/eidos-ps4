#!/usr/bin/env python3
"""
test_ps4_rules.py -- source rules each learned on a real PS4 against the
SDK 12.508 reference, so none comes back.

    python tests/ps4/test_ps4_rules.py

Reads the PS4 sources only; needs no SDK and no game data.  Each test names
the SDK page the rule comes from and the symptom the console showed.
"""

import os
import re
import sys
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.dont_write_bytecode = True


def read(rel):
    with open(os.path.join(ROOT, rel), encoding="utf-8") as f:
        return f.read()


def ps4_sources():
    """Every source compiled for the Orbis build, by path."""
    out = []
    for rel in ("engine/platform/ps4", "install", "sdk"):
        base = os.path.join(ROOT, rel)
        for name in sorted(os.listdir(base)):
            if rel != "engine/platform/ps4" and "ps4" not in name \
                    and name not in ("fsutil.c", "save.c"):
                continue
            if name.endswith((".c", ".cpp", ".h")):
                out.append(os.path.join(rel, name))
    out.append("demo/eidos_boot.c")
    return out


class SaveData(unittest.TestCase):
    def test_directory_names_use_only_the_allowed_characters(self):
        # SceSaveDataDirName: "a" to "z", "A" to "Z", "0" to "9", "-", "."
        # and "@" only; "_" is refused.  An underscore name made
        # sceSaveDataMount2 return SCE_SAVE_DATA_ERROR_PARAMETER (0x809f0000).
        src = read("engine/platform/ps4/save_ps4.c")
        names = re.findall(r'#define EIDOS_PS4_(?:SAVE|DATA)_DIRECTORY "([^"]*)"', src)
        self.assertEqual(len(names), 2, names)
        for name in names:
            self.assertRegex(name, r"^[A-Za-z0-9.@-]+$")
            self.assertLess(len(name), 32)
            self.assertFalse(name.startswith("sce_"))

    def test_blocks_are_within_the_documented_range(self):
        src = read("engine/platform/ps4/save_ps4.c")
        m = re.search(r"#define EIDOS_PS4_SAVE_BLOCKS (\d+)u", src)
        self.assertIsNotNone(m)
        self.assertGreaterEqual(int(m.group(1)), 96)      # SCE_SAVE_DATA_BLOCKS_MIN2
        self.assertLessEqual(int(m.group(1)), 32768)      # SCE_SAVE_DATA_BLOCKS_MAX


class Exit(unittest.TestCase):
    def test_no_loadexec_exit(self):
        # sceSystemServiceLoadExec takes only a program under /app0/; it is
        # no way to quit.
        for rel in ps4_sources():
            self.assertNotIn('sceSystemServiceLoadExec("exit"', read(rel), rel)

    def test_every_ps4_exit_of_main_is_status_zero(self):
        # The system reports a non-zero exit status as an application crash
        # (core dump and error screen).  main()'s PS4 paths end in
        # boot_ps4_exit(), which logs the status and exits with 0.
        src = read("demo/eidos_boot.c")
        body = re.search(r"static void boot_ps4_exit\(int status\)\s*\{(.*?)\n\}", src, re.S)
        self.assertIsNotNone(body)
        self.assertIn("exit(0)", body.group(1))
        self.assertIn("eidos_ps4_log_finish(status)", body.group(1))
        # the three PS4 exits of main() (unsupported option, failed setup,
        # the end of the run) leave through it
        self.assertEqual(src.count("boot_ps4_exit("), 5, "definition + 4 uses")
        self.assertNotRegex(src, r"boot_ps4_services_stop\(\);\s*return [0-9]")
        self.assertNotRegex(src, r"boot_ps4_services_stop\(\);\s*return result;")


class Dialogs(unittest.TestCase):
    def test_dialog_waits_do_not_end_on_running_alone(self):
        # The dialog is another process: its status is NONE until it starts,
        # then INITIALIZED, RUNNING, FINISHED.  A loop "while status ==
        # RUNNING" ended at once and the app left under its dialog.
        for rel in ("demo/eidos_boot.c", "install/picker_ps4.c"):
            src = read(rel)
            self.assertNotRegex(
                src, r"while \(sceMsgDialogUpdateStatus\(\) == SCE_COMMON_DIALOG_STATUS_RUNNING\)", rel)
            self.assertIn("SCE_COMMON_DIALOG_STATUS_FINISHED", src, rel)

    def test_splash_is_hidden_before_any_dialog(self):
        # sce_sys/pic1.png stays over the application, dialogs included,
        # until sceSystemServiceHideSplashScreen().
        src = read("demo/eidos_boot.c")
        self.assertIn("sceSystemServiceHideSplashScreen()", src)
        self.assertLess(src.index("sceSystemServiceHideSplashScreen()"),
                        src.index("boot_ps4_storage_start();"))

    def test_rom_prompt_is_one_button(self):
        # A Yes/No pair moves a cursor: the confirm button could answer No.
        src = read("install/picker_ps4.c")
        self.assertIn("SCE_MSG_DIALOG_BUTTON_TYPE_OK", src)
        self.assertNotIn("SCE_MSG_DIALOG_BUTTON_TYPE_YESNO", src)


class Audio(unittest.TestCase):
    def test_main_port_is_opened_for_the_system_user(self):
        src = read("engine/platform/ps4/audio_ps4.c")
        first = src.index("sceAudioOutOpen(")
        self.assertIn("SCE_USER_SERVICE_USER_ID_SYSTEM", src[first:first + 120])


class Memory(unittest.TestCase):
    def test_libc_heap_is_not_left_at_its_default_cap(self):
        # The default cap made the ROM import fail with "out of memory".
        src = read("demo/eidos_boot.c")
        self.assertRegex(src, r"size_t sceLibcHeapSize\s*=\s*SCE_LIBC_HEAP_SIZE_EXTENDED_ALLOC_NO_LIMIT")
        self.assertRegex(src, r"unsigned int sceLibcHeapExtendedAlloc\s*=\s*1")


class NotInTheSdk(unittest.TestCase):
    """Calls and headers the official SDK libc does not have (each broke the
    Orbis build or the console run once)."""

    BANNED = [
        (r'#include <signal\.h>', "no signal.h; the SDK has no signal or exception hook"),
        (r"\bsigaction\s*\(", "sigaction is not declared"),
        (r"\bdup2\s*\(", "dup2 is not declared"),
        (r"\bftello\s*\(|\bfseeko\s*\(", "no ftello/fseeko (long is 64 bits)"),
        (r"\bsceKernelDebugOutText\b", "not in the official stub libraries"),
    ]

    def test_ps4_sources_avoid_them(self):
        for rel in ps4_sources():
            src = read(rel)
            if "EIDOS_BOOT_PS4" not in src and "ps4" not in rel:
                continue
            # a comment that names a missing call is not a use of it
            code = re.sub(r"/\*.*?\*/|//[^\n]*", "", src, flags=re.S)
            for pattern, why in self.BANNED:
                self.assertNotRegex(code, pattern, "%s: %s" % (rel, why))


class Packaging(unittest.TestCase):
    def test_title_id_and_content_id_are_fixed(self):
        # The Title ID keys the save data and the update path: it must not
        # change from one package to the next.
        src = read("tools/ps4/make_game_pkg.sh")
        self.assertIn("TITLE_ID=EIDP90002", src)
        self.assertIn("LABEL=EIDOSGAME0000001", src)
        self.assertIn('eidos_ps4_log_start("EIDP90002"', read("demo/eidos_boot.c"))

    def test_verifier_refuses_rom_data_and_extra_binaries(self):
        src = read("tools/ps4/verify_fpkg.sh")
        self.assertIn("looks like ROM data", src)
        self.assertIn("unexpected binaries in the package", src)
        self.assertIn("PARAM_SFO", src)

    def test_release_target_exists(self):
        self.assertIn("eidos_ps4_release_fpkg", read("cmake/boot.cmake"))


if __name__ == "__main__":
    unittest.main()
