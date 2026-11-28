#!/usr/bin/env python3
"""Focused ABI regressions for the overlay trampoline generator."""

import importlib.util
import json
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("ovlink", ROOT / "tools" / "ovlink.py")
OVLINK = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(OVLINK)


class WordAggregateConversions(unittest.TestCase):
    def test_pointer_typed_trampoline_recovers_ds_word_via_uintptr(self):
        word = OVLINK.WordAggregate("Fixture", "typedef union { unsigned raw; } Fixture;")
        self.assertEqual(
            OVLINK.word_cast(word, "a0"),
            "bw_word_Fixture_of((unsigned int)(uintptr_t)(a0))",
        )


class HostSocketNamespace(unittest.TestCase):
    def test_arm9_caller_reaches_namespaced_overlay_trampoline(self):
        compiler = shutil.which("cc")
        if compiler is None:
            self.skipTest("a C compiler is required for the bootlink fixture")
        with tempfile.TemporaryDirectory(prefix="eidos-bootlink-socket-") as tmp:
            work = pathlib.Path(tmp)
            tree = work / "tree"
            shutil.copytree(ROOT / "tests" / "ovl_synth", tree)
            source = tree / "src/matched/arm9/socket_caller.c"
            source.write_text("""\
/* function: func_02100010
 * address: 0x02100010
 * region: arm9
 * symbols:
 *   socket 0x02200060 thumb ov000
 */
int socket(int a, int b, int c);
int func_02100010(void) { return socket(1, 2, 3); }
""")
            manifest = tree / "layout/matched/arm9/pkt_synth.json"
            data = json.loads(manifest.read_text())
            data["functions"].append({
                "name": "func_02100010", "address": "0x02100010", "kind": "game",
                "region": "arm9", "source": "src/matched/arm9/socket_caller.c",
                "isa": "thumb", "status": "EXACT",
            })
            manifest.write_text(json.dumps(data))
            plan = work / "plan"
            plan.mkdir()
            for name in ("boot_matched.txt", "boot_asm_subst.txt", "boot_asm.txt",
                         "library_matched.txt", "boot_thunks.txt"):
                (plan / name).write_text(f"{source}\n" if name == "boot_matched.txt" else "")
            shim = work / "shim.c"
            shim.write_text("int bw_ovl_game_socket(int a,int b,int c) { return a+b+c; }\n")
            (work / "shims.txt").write_text(f"{shim}\n")
            (work / "port.txt").write_text("")
            output = work / "out"
            subprocess.run(
                [sys.executable, str(ROOT / "tools/bootlink.py"), "generate",
                 "--aletheia-root", str(tree), "--plan-dir", str(plan),
                 "--shim-sources", str(work / "shims.txt"),
                 "--port-sources", str(work / "port.txt"), "--out-dir", str(output)],
                check=True, capture_output=True, text=True,
            )
            header = output / "include/src_matched_arm9_socket_caller.h"
            main = work / "main.c"
            main.write_text("int func_02100010(void);\n"
                            "int socket(int a,int b,int c) { (void)a; (void)b; (void)c; return -1; }\n"
                            "int main(void) { return func_02100010() == 6 ? 0 : 1; }\n")
            obj = work / "caller.o"
            exe = work / "fixture"
            subprocess.run([compiler, "-include", str(header), "-c", str(source), "-o", str(obj)],
                           check=True, capture_output=True, text=True)
            subprocess.run([compiler, str(obj), str(shim), str(main), "-o", str(exe)],
                           check=True, capture_output=True, text=True)
            subprocess.run([str(exe)], check=True, capture_output=True, text=True)


if __name__ == "__main__":
    unittest.main()
