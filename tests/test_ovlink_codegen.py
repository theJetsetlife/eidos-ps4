#!/usr/bin/env python3
"""Focused ABI regressions for the overlay trampoline generator."""

import importlib.util
import pathlib
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


if __name__ == "__main__":
    unittest.main()
