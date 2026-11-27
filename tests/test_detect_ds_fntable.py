"""Source scanner coverage for DS-bound record accesses."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch


ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location(
    "detect_ds_fntable", ROOT / "tools/lp64/detect_ds_fntable.py")
detector = importlib.util.module_from_spec(spec)
spec.loader.exec_module(detector)
addr_spec = importlib.util.spec_from_file_location("dsaddr", ROOT / "tools/dsaddr.py")
dsaddr = importlib.util.module_from_spec(addr_spec)
addr_spec.loader.exec_module(dsaddr)


class DsRecordAccesses(unittest.TestCase):
    def test_lookup_and_callback_are_both_found(self):
        source = """extern const Range table[8];
int find(int i) {
    const Range *range = &table[i];
    return range->first;
}
void use(int i) { table[i].handler(0); }
"""
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "table.c"
            path.write_text(source)
            rows = detector.scan_record_accesses(str(path), {"table"})
        self.assertEqual([r["line"] for r in rows], [3, 6])
        self.assertEqual({r["symbol"] for r in rows}, {"table"})

    def test_other_names_and_narrow_only_branches_are_ignored(self):
        source = """#if __SIZEOF_POINTER__ == 4
table[0].handler(0);
#endif
other[0].handler(0);
table[1].handler(0);
"""
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "table.c"
            path.write_text(source)
            rows = detector.scan_record_accesses(str(path), {"table"})
        self.assertEqual([r["line"] for r in rows], [5])


class DsRecordRewrite(unittest.TestCase):
    def rewrite(self, source):
        with patch.object(dsaddr, "_ds_bound_names", return_value={
            "arm9": {"gItemMenuUseRanges", "gBagItemEffectTable"}}):
            return dsaddr._ds_record_rewrite(
                source, "/tmp/build/ov070/test.c", "test.c")

    def test_callback_and_scalar_use_wire_stride(self):
        out, count = self.rewrite(
            "gItemMenuUseRanges[i].useHandler(work);\n"
            "x = gBagItemEffectTable[i].itemId;\n")
        self.assertEqual(count, 2)
        self.assertIn("(unsigned long)(i) * 2u + 1u", out)
        self.assertIn("(unsigned long)(i) * 8u + 0u", out)
        self.assertIn("bw_ds_wire_u16", out)
        self.assertIn('#include "ds_record.h"', out)

    def test_narrow_branch_and_comments_untouched(self):
        out, count = self.rewrite(
            "/* gItemMenuUseRanges[i].useHandler(work); */\n"
            "#if __SIZEOF_POINTER__ == 4\n"
            "gItemMenuUseRanges[i].useHandler(work);\n"
            "#endif\n")
        self.assertEqual(count, 0)
        self.assertNotIn("bw_ds_code_word", out)

    def test_unverified_member_fails_generation(self):
        with self.assertRaisesRegex(SystemExit, "no verified wire layout"):
            self.rewrite("x = gItemMenuUseRanges[i].unmapped;\n")


if __name__ == "__main__":
    unittest.main()
