"""Regressions for the current pointer-sized Aletheia source export."""
from pathlib import Path
import sys
import subprocess
import shutil
import tempfile
import unittest
import json
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import armdata
import dsaddr
import wasm32_layout
import gamespell
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools/lp64"))
import layouts
import abi_dump


class AbiEvidenceGates(unittest.TestCase):
    def test_failed_compiler_cannot_yield_layout_evidence(self):
        result = subprocess.CompletedProcess([], 1, "", "invalid native declaration")
        with mock.patch.object(abi_dump.subprocess, "run", return_value=result):
            with self.assertRaisesRegex(SystemExit, "layout compiler failed"):
                abi_dump.layout_dump(".", "bad.c", ["cc", "-c", "bad.c"])

    def test_compdb_ignores_phony_dependency_rows(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "commands.json"
            path.write_text(json.dumps([
                {"file": "unit.c", "command": "", "directory": directory},
                {"file": "unit.c", "command": "cc -c unit.c", "directory": directory}]))
            self.assertEqual(abi_dump.compile_cmd(str(path), "unit.c")[2][0], "cc")

    def test_missing_requested_layout_is_not_success(self):
        with mock.patch.object(sys, "argv", ["abi_dump.py", "commands.json", "unit.c", "Missing"]), \
             mock.patch.object(abi_dump, "compile_cmd", return_value=(".", "unit.c", ["cc"])), \
             mock.patch.object(abi_dump, "layout_dump", return_value=""):
            with self.assertRaisesRegex(SystemExit, "did not emit requested layouts"):
                abi_dump.main()

    def test_alignment_is_part_of_layout_evidence(self):
        dump = "    0 | struct Work\n    0 |   void * value\n      | [sizeof=8, align=8]\n"
        record = abi_dump.fields(dump, "Work")
        self.assertEqual(record["alignment"], 8)
        self.assertEqual(record["size"], 8)

    def test_mirror_gate_checks_extent_alignment_and_all_fields(self):
        script = Path(layouts.__file__).with_name("cmp_mirror.py")
        base = {"size": 16, "alignment": 8, "fields": [[0, "a"], [8, "b"]]}
        variants = [base, dict(base, size=24), dict(base, alignment=4),
                    dict(base, fields=[[0, "a"]]),
                    dict(base, fields=[[0, "a"], [4, "b"]])]
        with tempfile.TemporaryDirectory() as directory:
            p = Path(directory) / "port.json"; g = Path(directory) / "game.json"
            p.write_text(json.dumps({"Record": base}))
            for index, record in enumerate(variants):
                with self.subTest(index=index):
                    g.write_text(json.dumps({"Record": record}))
                    result = subprocess.run([sys.executable, str(script), str(p), "Record",
                                             str(g), "Record"], capture_output=True, text=True)
                    self.assertEqual(result.returncode, 0 if index == 0 else 1, result.stdout)


class PortTools(unittest.TestCase):
    @unittest.skipUnless(shutil.which("clang"), "clang required for scalar layout")
    def test_matched_sdk_scalars_keep_ds_width(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "src/matched/arm9_sdk/checker.c"
            source.parent.mkdir(parents=True)
            source.write_text("typedef unsigned long u32;\n"
                              "typedef long s32;\n"
                              "typedef volatile unsigned long vu32;\n"
                              "typedef unsigned long native_word;\n"
                              "typedef unsigned long long u64;\n"
                              "struct Work { char pad[0x388]; u32 checker[2]; };\n"
                              "typedef struct RTCDate { unsigned long year, month, day; int week; } RTCDate;\n"
                              "typedef char rtc_size[sizeof(RTCDate) == 16 ? 1 : -1];\n"
                              "typedef char word[sizeof(u32) == 4 ? 1 : -1];\n"
                              "typedef char signed_word[sizeof(s32) == 4 ? 1 : -1];\n"
                              "typedef char volatile_word[sizeof(vu32) == 4 ? 1 : -1];\n"
                              "typedef char wide_word[sizeof(u64) == 8 ? 1 : -1];\n"
                              "typedef char native[sizeof(native_word) == sizeof(unsigned long) ? 1 : -1];\n"
                              "typedef char checker[__builtin_offsetof(struct Work, checker) + sizeof(u32) == 0x38C ? 1 : -1];\n")
            output = root / "host.c"
            subprocess.run([sys.executable, str(Path(dsaddr.__file__)), str(source),
                            "-o", str(output)], check=True)
            subprocess.run([shutil.which("clang"), "-fsyntax-only", str(output)], check=True)
            subprocess.run([shutil.which("clang"), "--target=wasm32-unknown-unknown",
                            "-fsyntax-only", str(output)], check=True)

    def test_heap_replacement_preserves_alias_forwarder(self):
        root = Path(gamespell.PORT_ROOT)
        replacement = str(root / "stubs/reimpl_02060A08.c")
        sources = {
            replacement: "void *func_02060A08(void *h, int n, int a) { return alloc(n); }",
            str(root / "sdk/aliases.c"):
                "void *NNSi_FndAllocFromExpHeapHead(void *h, int n, int a) {\n"
                "return func_02060A08(h, n, a);\n}",
        }
        cmake = "set(BW_HEAP_REIMPL ${CMAKE_CURRENT_SOURCE_DIR}/stubs/reimpl_02060A08.c)"
        _, _, definer = gamespell.port_names(sources)
        names = {0x02060A08: "NNSi_FndAllocFromExpHeapHead"}
        self.assertEqual(gamespell.replacement_copies(
            cmake, sources, definer, {}, names, set(), set()), {})
        # Without a forwarder, the replacement must provide the game's name.
        sources.pop(str(root / "sdk/aliases.c"))
        _, _, definer = gamespell.port_names(sources)
        self.assertEqual(gamespell.replacement_copies(
            cmake, sources, definer, {}, names, set(), set()),
            {"func_02060A08": "NNSi_FndAllocFromExpHeapHead"})

    @unittest.skipUnless(shutil.which("clang"), "clang required for packet layout")
    def test_wasm_packet_alignment_is_scoped(self):
        original = ("typedef struct GdsPacketDownload {\n"
                    "unsigned short reserved0, reserved1;\nlong long key;\n"
                    "} GdsPacketDownload;\n"
                    "struct Other { unsigned value; long long key; };\n")
        patched = wasm32_layout.packet_header(original)
        # Packet is 12 bytes with key at byte 4. The following unrelated
        # record keeps the target's eight-byte alignment and size of 16.
        checks = ("typedef char packet_size[sizeof(GdsPacketDownload) == 12 ? 1 : -1];\n"
                  "typedef char packet_key[__builtin_offsetof(GdsPacketDownload, key) == 4 ? 1 : -1];\n"
                  "typedef char other_size[sizeof(struct Other) == 16 ? 1 : -1];\n")
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "packet.c"
            source.write_text(patched + checks)
            subprocess.run([shutil.which("clang"), "--target=wasm32-unknown-unknown",
                            "-fsyntax-only", str(source)], check=True)
        with self.assertRaises(ValueError):
            wasm32_layout.packet_header(original.replace("long long key;", "int key;"))

    def test_layout_compilation_arguments(self):
        self.assertEqual(layouts.flags_of({"arguments": [
            "clang", "-I", "dir with spaces", "-DABI=1", "-std=c99",
            "-c", "input.c", "-o", "output.o"]}),
            ["-I", "dir with spaces", "-DABI=1", "-std=c99"])

    @unittest.skipUnless(shutil.which("clang"), "clang required for ABI comparison")
    def test_layouts_compare_ds_mac_and_wasm(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "fixture.c"
            source.write_text("struct Item { void *pointer; int flag; };\nstruct Item item;\n")
            records = {}
            for target in (layouts.REFERENCE, "arm64-apple-macos11", "wasm32-unknown-unknown"):
                records[target], error = layouts.dump((shutil.which("clang"), target,
                    [], str(source), str(root / (target + ".ll")), str(root)))
                self.assertIsNone(error)
            self.assertEqual(records[layouts.REFERENCE]["struct Item"],
                             records["wasm32-unknown-unknown"]["struct Item"])
            self.assertNotEqual(records[layouts.REFERENCE]["struct Item"],
                                records["arm64-apple-macos11"]["struct Item"])
            source.write_text("this is deliberately invalid C;\n")
            result, error = layouts.dump((shutil.which("clang"), layouts.REFERENCE,
                [], str(source), str(root / "invalid.ll"), str(root)))
            self.assertIsNone(result)
            self.assertIn("error:", error)

    def test_shared_card_header_keeps_wire_layout(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source_dir = root / "src/game/arm9"
            output_dir = root / "host"
            source_dir.mkdir(parents=True)
            header = source_dir / "card_common.h"
            original = ("typedef struct CARDiCommandArg {\n"
                        "    uptr src;\n    uptr dst;\n} CARDiCommandArg;\n"
                        "typedef struct CARDiCommon {\n"
                        "    uptr src;\n    uptr dst;\n} CARDiCommon;\n")
            header.write_text(original)
            source = source_dir / "fixture.c"
            source.write_text('#include "card_common.h"\n')
            output = output_dir / "fixture.c"
            subprocess.run([sys.executable, str(Path(dsaddr.__file__)),
                            str(source), "--cut-asm", "-o", str(output)], check=True)
            rewritten = (output_dir / "card_common.h").read_text()
            command, common = rewritten.split("typedef struct CARDiCommon")
            self.assertIn("u32 src;", command)
            self.assertIn("u32 dst;", command)
            self.assertIn("uptr src;", common)
            self.assertIn((output_dir / "card_common.h").as_posix(), output.read_text())
            self.assertEqual(header.read_text(), original)

    def test_wrapped_register_address(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "ioreg.h").write_text(
                "#define IOREG_ADDR(a) ((uptr)(a))\n"
                "#define REG_SCFG_ROM_ADDR IOREG_ADDR(0x04004000)\n")
            source = '#include "ioreg.h"\nunsigned char f(void) {\n' \
                     'return *(volatile unsigned char *)REG_SCFG_ROM_ADDR;\n}\n'
            path = root / "test.c"
            path.write_text(source)
            result = dsaddr._rewrite_with_defines(source, str(path), str(path))
            self.assertIn("bw_ds_ptr(REG_SCFG_ROM_ADDR)", result)
            self.assertEqual(path.read_text(), source)

    def test_header_declared_data(self):
        source = """/*
 * symbols:
 *   gSharedTable 0x021D6338 data ov115
 */
#include "shared.h"
void *f(void) { return &gSharedTable; }
"""
        parsed = armdata.SourceFile("fixture.c", source, True)
        self.assertIn("gSharedTable", parsed.extern_objects)
        local = source.replace("return &gSharedTable;",
                               "int gSharedTable = 0; return 0;")
        self.assertNotIn("gSharedTable",
                         armdata.SourceFile("fixture.c", local, True).extern_objects)


if __name__ == "__main__":
    unittest.main()
