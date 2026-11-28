#!/usr/bin/env python3
"""
test_dsaddr.py -- tools/dsaddr.py, the fixed-address rewrite.

Three parts:

  - unit cases, one per cast form the corpus uses, one per thing the
    rewrite must leave alone (comments, strings, literals that are not
    the operand of a pointer cast, function pointers, constant
    initializers), one per way a name stands for a DS address (a
    file-local constant, a register macro's parameter, a CALLEES
    function's parameter and its callers), and the residue
    classification and report;
  - with a compiler (--cc): the rewritten fixture parses, the
    declaration the prelude spells agrees with sdk/dsmem.h's, and every
    matched file in the current build (--sources) parses after its
    rewrite, with no diagnostic its original did not have;
  - with an aletheia checkout (--aletheia-root): a dry run over every
    matched source, asserting the rewrite is idempotent and keeps every
    line where it was, and printing how many files and casts it touches;
    and the residue report over the same corpus.

    python tests/test_dsaddr.py [--aletheia-root DIR] [--sources FILE]
                                [--cc CC [--cc-id GNU|Clang|MSVC]
                                 [--cflags "FLAGS"]] [unittest args]

ctest runs it with everything the build knows.  Standard library only.
"""

from __future__ import annotations

import argparse
import os
import re
import shlex
import subprocess
import sys
import tempfile
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.dont_write_bytecode = True     # no __pycache__ in the source tree
sys.path.insert(0, os.path.join(ROOT, "tools"))
import dsaddr  # noqa: E402

CFG = argparse.Namespace(aletheia_root=None, sources=None, cc=None,
                         cc_id="GNU", cflags="")


def rw(src: str) -> str:
    """The rewritten body (the prelude stripped), asserting the prelude is
    there exactly when something was rewritten."""
    out, sites, _ = dsaddr.rewrite(src, "x.c")
    if not sites:
        assert out == src
        return out
    head = out.split("\n", 3)
    assert head[0].startswith("/* tools/dsaddr.py"), head[0]
    assert head[1] == dsaddr.PRELUDE_DECL, head[1]
    assert head[2] == '#line 1 "x.c"', head[2]
    return head[3]


BODY = "void t(void)\n{\n%s\n}\n"


def kinds(src: str, wrap: bool = True):
    """The residue kinds of `src`, as statements in a function unless
    `wrap` is false."""
    return sorted(s.kind for s in dsaddr.analyse(BODY % src if wrap else src)[1])


class Forms(unittest.TestCase):
    """Each cast form in the corpus, rewritten."""

    def check(self, src, want):
        # statements, so inside a function (at file scope a cast in an
        # initializer is left alone)
        src, want = BODY % src, BODY % want
        self.assertEqual(rw(src), want)
        # idempotent, and lines stay where they were
        again, sites, _ = dsaddr.rewrite(dsaddr.rewrite(src, "x.c")[0], "x.c")
        self.assertEqual(sites, [])
        self.assertEqual(rw(src).count("\n"), src.count("\n"))

    def test_deref_volatile_typedef(self):
        self.check("*(vu16 *)0x04000208 = 1;",
                   "*((vu16 *)bw_ds_ptr(0x04000208u)) = 1;")

    def test_parenthesised_literal(self):
        self.check("x = *(volatile u32 *)(0x04000000);",
                   "x = *((volatile u32 *)bw_ds_ptr(0x04000000u));")
        self.check("x = *(volatile u32 *)( ( 0x04000000 ) );",
                   "x = *((volatile u32 *)bw_ds_ptr(0x04000000u));")

    def test_ioreg_addr(self):
        # aletheia's sdk_ioreg.h: IOREG_ADDR(a) is the plain number on the DS
        self.check("x = *(volatile u16 *)IOREG_ADDR(0x04000280);",
                   "x = *((volatile u16 *)bw_ds_ptr(0x04000280u));")

    def test_suffixes(self):
        self.check("p = (u8 *)0x05000000u;", "p = ((u8 *)bw_ds_ptr(0x05000000u));")
        self.check("p = (u8 *)0x05000000UL;", "p = ((u8 *)bw_ds_ptr(0x05000000u));")
        self.check("p = (u8 *)0X6000000;", "p = ((u8 *)bw_ds_ptr(0x06000000u));")
        self.check("p = (u8 *)33554432;", "p = ((u8 *)bw_ds_ptr(0x02000000u));")

    def test_qualifiers_struct_and_stars(self):
        self.check("q = (const struct Work *)0x027FFC00;",
                   "q = ((const struct Work *)bw_ds_ptr(0x027FFC00u));")
        self.check("q = (u32 volatile * const *)0x02FFFC80;",
                   "q = ((u32 volatile * const *)bw_ds_ptr(0x02FFFC80u));")
        self.check("s = *(char **)0x02000000;",
                   "s = *((char **)bw_ds_ptr(0x02000000u));")
        self.check("v = *(volatile unsigned short*)0x04000006;",
                   "v = *((volatile unsigned short*)bw_ds_ptr(0x04000006u));")

    def test_regions(self):
        for addr in (0x02000000, 0x03000000, 0x04000000, 0x05000000,
                     0x06000000, 0x07000000, 0x07FFFFFF):
            self.check("p = (u8 *)0x%08X;" % addr,
                       "p = ((u8 *)bw_ds_ptr(0x%08Xu));" % addr)

    def test_inside_a_macro(self):
        self.check("#define REG_IME (*(vu16 *)0x04000208)\n",
                   "#define REG_IME (*((vu16 *)bw_ds_ptr(0x04000208u)))\n")
        self.check("#define R(x) \\\n    (*(vu32 *)0x04000000 = (x))\n",
                   "#define R(x) \\\n    (*((vu32 *)bw_ds_ptr(0x04000000u)) = (x))\n")

    def test_split_over_lines(self):
        self.check("a = *(vu16 *)\n    0x04000208;\nb = 1;\n",
                   "a = *((vu16 *)bw_ds_ptr(0x04000208u))\n;\nb = 1;\n")
        self.check("#define R (*(vu16 *) \\\n  0x04000208)\nint b;\n",
                   "#define R (*((vu16 *)bw_ds_ptr(0x04000208u))\\\n)\nint b;\n")
        self.check("#define R (*(vu16 *) \\\r\n  0x04000208)\r\nint b;\r\n",
                   "#define R (*((vu16 *)bw_ds_ptr(0x04000208u))\\\n)\r\nint b;\r\n")

    def test_address_expression(self):
        self.check("GX_Load((void *)(0x05000000 + offset), n);",
                   "GX_Load(((void *)bw_ds_ptr(0x05000000 + offset)), n);")
        self.check("r = (vu32 *)(0x040000B0 + dmaNo * 12);",
                   "r = ((vu32 *)bw_ds_ptr(0x040000B0 + dmaNo * 12));")

    def test_pointer_typedef_without_star(self):
        self.check("typedef volatile u16 *REGP;\nv = *(REGP)0x04000004;",
                   "typedef volatile u16 *REGP;\nv = *((REGP)bw_ds_ptr(0x04000004u));")

    def test_pointer_to_function_pointer_is_data(self):
        self.check("(*(void (**)(u32))0x020A8144)(x);",
                   "(*((void (**)(u32))bw_ds_ptr(0x020A8144u)))(x);")
        self.check("a = (u8 (*)[4])0x06000000;",
                   "a = ((u8 (*)[4])bw_ds_ptr(0x06000000u));")

    def test_after_keyword_and_in_cast_chain(self):
        self.check("return (u8 *)0x02000000;",
                   "return ((u8 *)bw_ds_ptr(0x02000000u));")
        self.check("n = (u32)(u8 *)0x02000000;",
                   "n = (u32)((u8 *)bw_ds_ptr(0x02000000u));")

    def test_inside_function_body_braces(self):
        src = ("void f(void)\n{\n    if (x) {\n        *(vu16 *)0x04000208 = 0;\n    }\n}\n")
        self.assertIn("bw_ds_ptr(0x04000208u)", rw(src))

    def test_prelude_names_the_original(self):
        out, _, _ = dsaddr.rewrite(BODY % "p = (u8 *)0x02000000;",
                                   "C:\\a\\b \"c\".c")
        self.assertIn('#line 1 "C:/a/b \\"c\\".c"\n', out)


class NativeTypes(unittest.TestCase):
    def test_native_callback_allocation_uses_native_extent(self):
        source = ("void f(void) {\n"
                  "    anArg = (GPRecvBuddyStatusArg *)gsimalloc(0xc);\n"
                  "}\n")
        path = "/upstream/src/game/ov009/gpibuddy.c"
        native = dsaddr.native_game_allocations(source, path)
        self.assertIn("gsimalloc(sizeof(*anArg))", native)
        self.assertNotIn("gsimalloc(0xc)", native)
        self.assertEqual(dsaddr.native_game_allocations(source, "/other.c"), source)

    def test_native_variadic_definitions_use_promoted_last_parameter(self):
        wm = ("WMErrCode WMi_SendCommand(WMApiid id, u16 paramNum, ...);\n"
              "WMErrCode WMi_SendCommand(WMApiid id, u16 paramNum, ...)\n{\n}\n")
        native = dsaddr.native_variadic_abi(wm, "/r/src/game/arm9/wm_system.c")
        self.assertEqual(native.count("int paramNum"), 2)
        battle = ('#include "btl_server_cmd_types.h"\n'
                  "void BTL_SERVER_CMD_PushVariable("
                  "BtlServerCmdQueue *queue, u8 id, ...)\n{\n}\n")
        native = dsaddr.native_variadic_abi(
            battle, "/r/src/game/ov093/btl_server_cmd.c")
        self.assertIn("queue, int id, ...", native)
        self.assertIn("#define BTL_SERVER_CMD_PushVariable ov093_", native)

    def test_native_variadic_callers_shadow_ds_header_declaration(self):
        wm = '#include "wm_system.h"\nvoid f(void) { WMi_SendCommand(1, 0); }\n'
        native = dsaddr.native_variadic_abi(wm, "/r/src/game/arm9/wm_standard.c")
        self.assertIn("#define WMi_SendCommand WMi_SendCommand_DS_ABI", native)
        self.assertIn("int paramNum, ...", native)
        battle = '#include "btl_server_cmd.h"\n'
        native = dsaddr.native_variadic_abi(
            battle, "/r/src/game/ov093/btl_server_flow.c")
        self.assertIn("BTL_SERVER_CMD_PushVariable_DS_ABI", native)
        self.assertIn("#define BTL_SERVER_CMD_PushVariable ov093_", native)
        self.assertIn("int id, ...", native)

    def test_native_shared_state_has_defined_zero_defaults(self):
        sound = "    int loopStartUnits;\n    int loopLength;\n"
        native = dsaddr.native_defined_locals(
            sound, "/r/src/game/arm9/nns_snd_waveout.c")
        self.assertIn("loopStartUnits = 0", native)
        self.assertIn("loopLength = 0", native)
        wireless = "            u16 state, aid, myAid, reason;\n"
        native = dsaddr.native_defined_locals(
            wireless, "/r/src/game/arm9/wm_system.c")
        self.assertIn("u16 state = 0", native)
        battle = "    int saved;\n    int ret;\n"
        native = dsaddr.native_defined_locals(
            battle, "/r/src/game/ov093/btl_server_flow.c")
        self.assertIn("int saved = 0", native)
        self.assertEqual(dsaddr.native_defined_locals(sound, "/other.c"), sound)

    def test_native_bg_screen_dimensions_define_invalid_code(self):
        source = ("void BG_SYS_GetScreenSizeInTiles(u32 mode, u8 *a, u8 *b)\n"
                  "{\n    switch (mode) {\n"
                  "    case 5:\n        *a = 0x80;\n        *b = 0x80;\n"
                  "        break;\n    }\n}\n")
        native = dsaddr.native_bg_screen_dimensions(
            source, "/r/src/game/arm9/bg_sys.c")
        self.assertIn("default:\n        *a = 0;\n        *b = 0;", native)
        self.assertEqual(dsaddr.native_bg_screen_dimensions(source, "/other.c"), source)

    def test_native_bg_screen_dimensions_execute(self):
        if not CFG.aletheia_root:
            self.skipTest("requires the Aletheia source checkout")
        source_path = os.path.join(CFG.aletheia_root, "src/game/arm9/bg_sys.c")
        with open(source_path, encoding="utf-8") as stream:
            source = dsaddr.native_bg_screen_dimensions(stream.read(), source_path)
        function = re.search(
            r"void BG_SYS_GetScreenSizeInTiles\(u32 mode, u8 \*a, u8 \*b\)\s*"
            r"\{.*?\n\}", source, re.DOTALL)
        self.assertIsNotNone(function)
        harness = ("typedef unsigned int u32; typedef unsigned char u8;\n"
                   + function.group(0) + "\n"
                   "int main(void) {\n"
                   "  const u8 expected[6][2] = {{16,16},{32,32},{32,64},"
                   "{64,32},{64,64},{128,128}};\n"
                   "  for (u32 i = 0; i < 6; ++i) {\n"
                   "    u8 a = 255, b = 255; BG_SYS_GetScreenSizeInTiles(i, &a, &b);\n"
                   "    if (a != expected[i][0] || b != expected[i][1]) return 1;\n"
                   "  }\n"
                   "  u8 a = 255, b = 255; BG_SYS_GetScreenSizeInTiles(6, &a, &b);\n"
                   "  if (a || b) return 2;\n"
                   "  a = b = 255; BG_SYS_GetScreenSizeInTiles(255, &a, &b);\n"
                   "  return a || b ? 3 : 0;\n"
                   "}\n")
        with tempfile.TemporaryDirectory() as tmp:
            source_file = os.path.join(tmp, "bg_dimensions.c")
            binary_file = os.path.join(tmp, "bg_dimensions")
            with open(source_file, "w", encoding="utf-8") as stream:
                stream.write(harness)
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                            source_file, "-o", binary_file], check=True)
            subprocess.run([binary_file], check=True)

    def test_native_namein_mode_conversion_guards_invalid_pair(self):
        source = ("    } else if (newmode == NAMEIN_GRAPHIC_MODE_SYMBOL) {\n"
                  "        from = NAMEIN_GRAPHIC_GridValue;\n"
                  "        to = NAMEIN_GRAPHIC_SymbolGetValue;\n"
                  "        width = 11;\n"
                  "        height = 4;\n"
                  "    }\n"
                  "    result = from(p, 0);\n")
        native = dsaddr.native_namein_mode_conversion(
            source, "/r/src/game/ov194/namein_graphic_field.c")
        self.assertIn("    } else {\n        return;\n    }", native)
        self.assertEqual(
            dsaddr.native_namein_mode_conversion(source, "/other.c"), source)

    def test_native_namein_mode_conversion_executes(self):
        if not CFG.aletheia_root:
            self.skipTest("requires the Aletheia source checkout")
        source_path = os.path.join(
            CFG.aletheia_root, "src/game/ov194/namein_graphic_field.c")
        with open(source_path, encoding="utf-8") as stream:
            source = dsaddr.native_namein_mode_conversion(
                stream.read(), source_path)
        function = re.search(
            r"void NAMEIN_GRAPHIC_ConvertCellMode\(NameinGraphicPair \*p, "
            r"int oldmode, int newmode\)\s*\{.*?\n\}", source, re.DOTALL)
        self.assertIsNotNone(function)
        harness = (
            "typedef struct { int x, y; } NameinGraphicPair;\n"
            "typedef int (*Conv)(NameinGraphicPair *, NameinGraphicPair *);\n"
            "#define NAMEIN_GRAPHIC_MODE_SYMBOL 3\n"
            "int NAMEIN_GRAPHIC_SymbolGetValue(NameinGraphicPair *p, "
            "NameinGraphicPair *out) { (void)out; return p->x + p->y * 14; }\n"
            "int NAMEIN_GRAPHIC_GridValue(NameinGraphicPair *p, "
            "NameinGraphicPair *out) { (void)out; return p->x + p->y * 12; }\n"
            + function.group(0) + "\n"
            "int main(void) {\n"
            "  NameinGraphicPair p = {2, 1};\n"
            "  NAMEIN_GRAPHIC_ConvertCellMode(&p, 3, 0);\n"
            "  if (p.x != 4 || p.y != 1) return 1;\n"
            "  p.x = 3; p.y = 1;\n"
            "  NAMEIN_GRAPHIC_ConvertCellMode(&p, 0, 3);\n"
            "  if (p.x != 1 || p.y != 1) return 2;\n"
            "  p.x = 7; p.y = 2;\n"
            "  NAMEIN_GRAPHIC_ConvertCellMode(&p, 0, 1);\n"
            "  return p.x == 7 && p.y == 2 ? 0 : 3;\n"
            "}\n")
        with tempfile.TemporaryDirectory() as tmp:
            source_file = os.path.join(tmp, "namein_mode.c")
            binary_file = os.path.join(tmp, "namein_mode")
            with open(source_file, "w", encoding="utf-8") as stream:
                stream.write(harness)
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                            source_file, "-o", binary_file], check=True)
            subprocess.run([binary_file], check=True)

    def test_native_field_player_attributes_use_absent_tile_fallback(self):
        source = (
            "u32 FIELD_PLAYER_GetNowAttribute(FieldPlayer *player)\n"
            "{\n    u32 attr;\n    return attr;\n}\n"
            "u32 FIELD_PLAYER_GetFrontAttribute(FieldPlayer *player, u32 dir)\n"
            "{\n    u32 attr;\n    return attr + dir;\n}\n")
        native = dsaddr.native_field_player_attributes(
            source, "/r/src/game/ov021/field_player.c")
        self.assertEqual(native.count("u32 attr = 0xFFFFFFFFu;"), 2)
        self.assertEqual(
            dsaddr.native_field_player_attributes(source, "/other.c"), source)

    def test_native_field_player_attributes_execute(self):
        if not CFG.aletheia_root:
            self.skipTest("requires the Aletheia source checkout")
        source_path = os.path.join(
            CFG.aletheia_root, "src/game/ov021/field_player.c")
        with open(source_path, encoding="utf-8") as stream:
            source = dsaddr.native_field_player_attributes(
                stream.read(), source_path)

        def function_text(signature):
            start = source.index(signature)
            opening = source.index("{", start)
            depth = 0
            for pos in range(opening, len(source)):
                if source[pos] == "{":
                    depth += 1
                elif source[pos] == "}":
                    depth -= 1
                    if depth == 0:
                        return source[start:pos + 1]
            self.fail("unterminated function " + signature)

        now = function_text(
            "u32 FIELD_PLAYER_GetNowAttribute(FieldPlayer *player)")
        front = function_text(
            "u32 FIELD_PLAYER_GetFrontAttribute(FieldPlayer *player, u32 dir)")
        harness = (
            "typedef unsigned int u32; typedef unsigned char u8;\n"
            "typedef struct VecFx32 { int x, y, z; } VecFx32;\n"
            "typedef VecFx32 FieldPlayerLocationPos;\n"
            "typedef struct { int a, b; } FieldPlayerRailLocation;\n"
            "typedef struct { int a, b; } RailLocation;\n"
            "typedef struct FieldG3dMapper { int unused; } FieldG3dMapper;\n"
            "typedef struct FieldNogridMapper { int unused; } FieldNogridMapper;\n"
            "typedef struct Mmdl { int unused; } Mmdl;\n"
            "typedef struct FieldPlayer { void *fieldMap; } FieldPlayer;\n"
            "static u32 move_kind;\n"
            "u32 FIELDMAP_GetMoveKind(void *map) { (void)map; return move_kind; }\n"
            "FieldG3dMapper *FIELDMAP_GetG3dMapper(void *map) { (void)map; return 0; }\n"
            "FieldNogridMapper *FIELDMAP_GetNogridMapper(void *map) { (void)map; return 0; }\n"
            "void FIELD_PLAYER_GetPos(FieldPlayer *p, VecFx32 *v) { (void)p; *v = (VecFx32){0}; }\n"
            "void FIELD_PLAYER_GetPosInDir(FieldPlayer *p, u32 d, VecFx32 *v) "
            "{ (void)p; (void)d; *v = (VecFx32){0}; }\n"
            "Mmdl *FIELD_PLAYER_GetMmdl(FieldPlayer *p) { (void)p; return 0; }\n"
            "u32 MAP_ATTR_GetAt(FieldG3dMapper *m, const VecFx32 *v) "
            "{ (void)m; (void)v; return 0x11111111u; }\n"
            "void FLDMMDL_RAIL_GetNextLocation(Mmdl *m, RailLocation *r) "
            "{ (void)m; *r = (RailLocation){0}; }\n"
            "void FLDMMDL_RAIL_GetLocationByDir(Mmdl *m, u32 d, RailLocation *r) "
            "{ (void)m; (void)d; *r = (RailLocation){0}; }\n"
            "u32 FIELD_NOGRID_MAPPER_GetAttribute(FieldNogridMapper *m, "
            "const RailLocation *r) { (void)m; (void)r; return 0x22222222u; }\n"
            + now + "\n" + front + "\n"
            "int main(void) {\n"
            "  FieldPlayer p = {0};\n"
            "  move_kind = 0; if (FIELD_PLAYER_GetNowAttribute(&p) != 0x11111111u) return 1;\n"
            "  if (FIELD_PLAYER_GetFrontAttribute(&p, 2) != 0x11111111u) return 2;\n"
            "  move_kind = 1; if (FIELD_PLAYER_GetNowAttribute(&p) != 0x22222222u) return 3;\n"
            "  if (FIELD_PLAYER_GetFrontAttribute(&p, 2) != 0x22222222u) return 4;\n"
            "  move_kind = 2; if (FIELD_PLAYER_GetNowAttribute(&p) != 0xFFFFFFFFu) return 5;\n"
            "  return FIELD_PLAYER_GetFrontAttribute(&p, 2) == 0xFFFFFFFFu ? 0 : 6;\n"
            "}\n")
        with tempfile.TemporaryDirectory() as tmp:
            source_file = os.path.join(tmp, "field_player_attributes.c")
            binary_file = os.path.join(tmp, "field_player_attributes")
            with open(source_file, "w", encoding="utf-8") as stream:
                stream.write(harness)
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                            source_file, "-o", binary_file], check=True)
            subprocess.run([binary_file], check=True)

    def test_native_field_model_lookup_has_null_not_found_result(self):
        source = (
            "Mmdl *FLDMMDL_AddMmdlHeaderById(MmdlSys *sys, const MmdlHeader *header, int zoneId, int count, void *evWork, u32 objId)\n"
            "{\n    Mmdl *mmdl;\n    return mmdl;\n}\n")
        native = dsaddr.native_field_model_lookup(
            source, "/r/src/game/ov010/fldmmdl.c")
        self.assertIn("Mmdl *mmdl = 0;", native)
        self.assertEqual(
            dsaddr.native_field_model_lookup(source, "/other.c"), source)

    def test_native_field_model_lookup_execute(self):
        if not CFG.aletheia_root:
            self.skipTest("requires the Aletheia source checkout")
        source_path = os.path.join(
            CFG.aletheia_root, "src/game/ov010/fldmmdl.c")
        with open(source_path, encoding="utf-8") as stream:
            source = dsaddr.native_field_model_lookup(
                stream.read(), source_path)
        signature = (
            "Mmdl *FLDMMDL_AddMmdlHeaderById(MmdlSys *sys, "
            "const MmdlHeader *header, int zoneId, int count, "
            "void *evWork, u32 objId)")
        start = source.index(signature + "\n{")
        opening = source.index("{", start)
        depth = 0
        function = None
        for pos in range(opening, len(source)):
            if source[pos] == "{":
                depth += 1
            elif source[pos] == "}":
                depth -= 1
                if depth == 0:
                    function = source[start:pos + 1]
                    break
        self.assertIsNotNone(function)
        harness = (
            "typedef unsigned int u32;\n"
            "typedef struct MmdlSys { int tag; } MmdlSys;\n"
            "typedef struct Mmdl { int tag; } Mmdl;\n"
            "typedef struct MmdlHeader { u32 objId, eventFlag, none; } MmdlHeader;\n"
            "static Mmdl added = {17};\n"
            "static const MmdlHeader *added_header;\n"
            "int FLDMMDL_HeaderIsEventIdNone(const MmdlHeader *h) "
            "{ return h->none; }\n"
            "int FLDMMDL_CheckEventFlagBit(void *work, u32 flag) "
            "{ (void)work; return flag != 0; }\n"
            "Mmdl *FLDMMDL_AddMmdlCore(MmdlSys *sys, const MmdlHeader *h, "
            "int zone, void *work) { (void)sys; (void)zone; (void)work; "
            "added_header = h; return &added; }\n"
            + function + "\n"
            "int main(void) {\n"
            "  MmdlSys sys = {0}; int work = 0;\n"
            "  MmdlHeader headers[] = {{3, 0, 0}, {7, 0, 0}};\n"
            "  if (FLDMMDL_AddMmdlHeaderById(&sys, headers, 2, 2, &work, 7) != &added) return 1;\n"
            "  if (added_header != &headers[1]) return 2;\n"
            "  if (FLDMMDL_AddMmdlHeaderById(&sys, headers, 2, 2, &work, 9) != 0) return 3;\n"
            "  headers[1].none = 1;\n"
            "  if (FLDMMDL_AddMmdlHeaderById(&sys, headers, 2, 2, &work, 7) != 0) return 4;\n"
            "  headers[1].none = 0; headers[1].eventFlag = 1;\n"
            "  return FLDMMDL_AddMmdlHeaderById(&sys, headers, 2, 2, &work, 7) == 0 ? 0 : 5;\n"
            "}\n")
        with tempfile.TemporaryDirectory() as tmp:
            source_file = os.path.join(tmp, "field_model_lookup.c")
            binary_file = os.path.join(tmp, "field_model_lookup")
            with open(source_file, "w", encoding="utf-8") as stream:
                stream.write(harness)
            subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror",
                            source_file, "-o", binary_file], check=True)
            subprocess.run([binary_file], check=True)

    def test_arc_runtime_prefix_tracks_pointer_width(self):
        source = "struct ArcFile {\n    u8 file[0x48];\n    u32 imageBase;\n};\n"
        path = "/upstream/src/matched/arm9/example.c"
        self.assertIn("u8 file[0x48]", dsaddr.native_matched_types(source, path, 4))
        self.assertIn("u8 file[0x80]", dsaddr.native_matched_types(source, path, 8))

    def test_arc_handle_prefix_tracks_pointer_width(self):
        source = "struct ArcHandle {\n    u8 raw[0x48];\n};\n"
        path = "/upstream/src/matched/arm9/example.c"
        self.assertIn("u8 raw[0x48]", dsaddr.native_matched_types(source, path, 4))
        self.assertIn("u8 raw[0x80]", dsaddr.native_matched_types(source, path, 8))

    def test_global_proc_argument_uses_ds_wire_address_on_lp64(self):
        source = """void f(void) {
    PROCSYS_PushGlobal(88, (u32)d_021BD8E8, (u32)&g_02146948.f04);
}
"""
        path = "/upstream/src/matched/arm9/func_0202F9F8.c"
        native = dsaddr.native_matched_types(source, path, 8)
        self.assertIn("bw_ds_wire_addr((uintptr_t)(d_021BD8E8))", native)
        self.assertIn("bw_ds_wire_addr((uintptr_t)(&g_02146948.f04))", native)
        self.assertEqual(dsaddr.native_matched_types(source, path, 4), source)


# what the tool also looks for in the trainer AI source (the VM parameters)
AI_ANCHORS = ('#include "../arm9/vm.h"\n'
              "static ScriptVm *mk(u32 heapId) { return VM_Create(heapId, &gTrAiVmParams); }\n")


PRINTSYS_CALLBACK = ("struct TextPrinter *PRINTSYS_PrintStreamCallBack(struct Tcbl *tcbl, u32 priority) {\n"
                     "    task = (struct TcblNode *)TCBL_AddTask((struct TcblSystem *)tcbl, PRINTSYS_Task, 0x50, priority);\n}\n")
# the print stream's delete, as printsys.c has it (the rewrite looks for it)
PRINTSYS_DELETE = ("void PRINTSYS_PrintStreamDelete(struct TextPrinter *printer)\n{\n"
                   "    if (printer->task != 0) {\n"
                   "        TCBL_DeleteTask((struct TcblTask *)printer->task);\n"
                   "        printer->task = 0;\n    }\n}\n")


class Lp64PathRules(unittest.TestCase):
    """The rewrites dsaddr.main applies to one named source at 8-byte
    pointers.  Each was a fault found running the game: the source is put
    where the rule looks for it, the tool run as the build runs it."""

    def run_tool(self, relpath, source, width=8):
        with tempfile.TemporaryDirectory() as tmp:
            src = os.path.join(tmp, relpath)
            os.makedirs(os.path.dirname(src))
            with open(src, "w", encoding="utf-8") as f:
                f.write(source)
            out = os.path.join(tmp, "out.c")
            rc = dsaddr.main([src, "-o", out, "--pointer-width", str(width)])
            text = ""
            if os.path.exists(out):
                with open(out, encoding="utf-8") as f:
                    text = f.read()
            return rc, text

    def test_battle_cry_uses_native_pointer_output_storage(self):
        # -O2 trainer battle: GetVoiceOfSlot wrote eight bytes into buf's
        # four-byte DS word and tripped the caller's stack canary.
        src = ("void f(void) {\n    u32 buf;\n    u32 *out;\n"
               "    out = &buf;\n"
               "    BTL_MAIN_GetVoiceOfSlot(main, pos, (struct PerapVoice **)out);\n"
               "    voice = PM_VOICE_Play(work->sprites[idx].identity.species, "
               "form, id, chorus, a, b, chorusParamC, (void **)out);\n}\n")
        rc, out = self.run_tool("src/game/ov094/btlv_mcss.c", src)
        self.assertEqual(rc, 0)
        self.assertIn("struct PerapVoice *buf = 0;", out)
        self.assertIn("BTL_MAIN_GetVoiceOfSlot(main, pos, out)", out)
        self.assertIn("recordedVoice = buf;", out)
        self.assertIn("out != 0 ? &recordedVoice : 0", out)
        self.assertNotIn("(void **)out", out)
        rc, out = self.run_tool("src/game/ov094/btlv_mcss.c", src, width=4)
        self.assertEqual(rc, 0)
        self.assertIn("u32 buf;", out)
        self.assertIn("(void **)out", out)

    def test_matched_heap_reporter_logs_before_legacy_queries(self):
        src = ("typedef unsigned int u32;\n"
               "void GFL_HEAP_ReportOutOfMemory(u32 heapId, u32 size, const char *site, int tag)\n"
               "{\n    void *arena;\n    sub_02060E6C(arena);\n}\n")
        rc, out = self.run_tool("src/matched/arm9/sub_02030958.c", src)
        self.assertEqual(rc, 0)
        self.assertIn("#include <stdio.h>", out)
        self.assertLess(out.index("heap allocation failed:"), out.index("sub_02060E6C(arena)"))
        rc, out = self.run_tool("src/matched/arm9/sub_02030958.c", src, width=4)
        self.assertEqual(rc, 0)
        self.assertNotIn("heap allocation failed:", out)

    def test_ai_script_jumps_are_signed_at_lp64(self):
        # A script jump is a signed 32-bit offset from the cursor.  Added as
        # u32 to a 64-bit pointer, a backward jump lands 4 GiB ahead (the
        # trainer AI faulted in bw_vm_read_u16).
        src = (AI_ANCHORS +
               "void f(ScriptVm *vm) {\n"
               "    u32 jumpOffset; jumpOffset = VM_GetU32(vm);\n"
               "    VM_Jump(vm, (u8 *)vm->pc + jumpOffset);\n"
               "    VM_Jump(vm, (u8 *)(vm->pc + tableOffset));\n"
               "}\n")
        rc, out = self.run_tool("src/game/ov096/tr_ai.c", src)
        self.assertEqual(rc, 0)
        self.assertIn("vm->pc + (int)jumpOffset", out)
        self.assertIn("vm->pc + (int)tableOffset", out)
        self.assertNotIn("vm->pc + jumpOffset", out)

    def test_ai_script_jumps_are_left_at_ds_width(self):
        src = AI_ANCHORS + "void f(ScriptVm *vm) { VM_Jump(vm, (u8 *)vm->pc + jumpOffset); }\n"
        rc, out = self.run_tool("src/game/ov096/tr_ai.c", src, width=4)
        self.assertEqual(rc, 0)
        self.assertIn("vm->pc + jumpOffset", out)
        self.assertNotIn("(int)jumpOffset", out)

    def test_ai_script_without_offsets_is_an_error_not_a_silent_pass(self):
        # a changed upstream source must not quietly lose the rule
        with self.assertRaises(SystemExit):
            self.run_tool("src/game/ov096/tr_ai.c", AI_ANCHORS + "void f(void) {}\n")

    def test_other_sources_keep_their_offsets(self):
        src = "void f(ScriptVm *vm) { VM_Jump(vm, (u8 *)vm->pc + jumpOffset); }\n"
        rc, out = self.run_tool("src/game/ov010/other.c", src)
        self.assertEqual(rc, 0)
        self.assertIn("vm->pc + jumpOffset", out)

    def test_effect_registries_are_read_as_ds_words(self):
        # ov095's field/position/side registries are rows of two DS words
        # (three for the side one: id, getter, layer limit); a native struct
        # row is twice as wide, so the 2nd row's id was read from the 1st
        # row's getter (the ability getter fault at the first battle).
        for kind, words in (("field", 2), ("pos", 2), ("side", 3)):
            table = "gHand%sEffectTable" % kind.title()
            body = ("int f(int i) { return %s[i].effectId + %s[i].getHandlers"
                    % (table, table))
            if kind == "side":
                body += " + %s[i].maxLayers" % table
            body += "; }\n"
            rc, out = self.run_tool("src/game/ov095/hand_%s.c" % kind, body)
            self.assertEqual(rc, 0, kind)
            self.assertIn('#include "battle_registry.h"', out)
            self.assertIn("BW_BTL_KEY(%s, i, %du, effectId)" % (table, words), out)
            self.assertIn("BW_BTL_GETTER(%s, i, %du)" % (table, words), out)
            if kind == "side":
                self.assertIn("bw_btl_registry_word(%s, i * 3u + 2u)" % table, out)

    def test_registry_rewrite_is_not_applied_at_ds_width(self):
        rc, out = self.run_tool("src/game/ov095/hand_field.c",
                                "int f(int i){return gHandFieldEffectTable[i].effectId;}\n", 4)
        self.assertEqual(rc, 0)
        self.assertNotIn("BW_BTL_KEY", out)

    def test_music_switch_set_is_allocated_at_its_native_size(self):
        # 0x3c is the DS size; with 8-byte pointers the set is 0x60 and
        # ISS_SWITCH_SET_Init wrote past the block (ASan; the game's
        # switch-driven music).
        src = ("struct IssSwitchSet *ISS_SWITCH_SET_Alloc(u32 heapId) {\n"
               "    return GFL_HEAP_Alloc(heapId, 0x3c, 0, IssSwitchSetFileName, 0x12a);\n}\n")
        rc, out = self.run_tool("src/game/arm9/iss_switch_set.c", src)
        self.assertEqual(rc, 0)
        self.assertIn("sizeof(struct IssSwitchSet)", out)
        self.assertNotIn("0x3c", out)
        rc, out = self.run_tool("src/game/arm9/iss_switch_set.c", src, width=4)
        self.assertIn("0x3c", out)

    def test_print_stream_task_work_is_the_native_printer_size(self):
        # the TextPrinter was 0x50 bytes of task work; native it is larger,
        # and PRINTSYS_SetupJob wrote past it (the intro's text, the heap
        # error, and the yellow streaks in every text box).
        src = PRINTSYS_CALLBACK + PRINTSYS_DELETE
        rc, out = self.run_tool("src/game/arm9/printsys.c", src)
        self.assertEqual(rc, 0)
        self.assertIn("sizeof(struct TextPrinter)", out)
        rc, out = self.run_tool("src/game/arm9/printsys.c", src, width=4)
        self.assertIn("PRINTSYS_Task, 0x50", out)

    def test_print_stream_delete_does_not_write_after_the_free(self):
        # the printer is the task's own work area: deleting the task frees
        # it, so printer->task is cleared first (ASan: heap-use-after-free in
        # PRINTSYS_PrintStreamDelete; the console's libc heap error).
        src = PRINTSYS_CALLBACK + PRINTSYS_DELETE
        rc, out = self.run_tool("src/game/arm9/printsys.c", src)
        self.assertEqual(rc, 0)
        self.assertLess(out.index("printer->task = 0;"), out.index("TCBL_DeleteTask(bw_task)"))
        rc, out = self.run_tool("src/game/arm9/printsys.c", src, width=4)
        self.assertNotIn("bw_task", out)

    SND_BANK = ("void SND_AssignWaveArc(SNDBankData *bank, int index, SNDWaveArc *waveArc)\n{\n    x;\n}\n"
                "void SND_DestroyBank(SNDBankData *bank)\n{\n    y;\n}\n"
                "void SND_DestroyWaveArc(SNDWaveArc *waveArc)\n{\n    z;\n}\n")

    def test_bank_wave_links_reach_the_sound_driver_through_the_ds_slot(self):
        # The native bank links live in a side table, not in the bank file.
        # The sound driver finds a bank's wave archives only in the file's
        # DS-width slots (+0x18 + 8*i), so each assign and destroy keeps the
        # slot in step.  Without it no bank had a wave archive: every
        # instrument note was silent (the intro music, all of it).
        rc, out = self.run_tool("src/game/arm9/snd_bank.c", self.SND_BANK)
        self.assertEqual(rc, 0)
        self.assertIn("bw_bank_ds_slot(bank, index, bw_ds_addr_of(waveArc))", out)
        self.assertIn("bw_bank_ds_slot(bank, index, 0)", out)
        self.assertIn("bw_bank_ds_slot(bank, i, 0)", out)
        # the slot is the 32-bit word at +0x18 + 8 * index, little endian
        self.assertIn("(unsigned char *)bank + 0x18 + 8u * (unsigned int)index", out)

    def test_bank_wave_links_are_untouched_in_the_four_byte_branch(self):
        # at 4-byte pointers the game's own links ARE the file's slots: the
        # branch the C code takes there never writes the DS slot by hand
        rc, out = self.run_tool("src/game/arm9/snd_bank.c", self.SND_BANK)
        self.assertEqual(rc, 0)
        marker = "if (sizeof(void *) == 4) {"
        for fn in ("void SND_AssignWaveArc(", "void SND_DestroyBank("):
            body = out[out.index(fn):]
            four = body[body.index(marker):body.index("return;", body.index(marker))]
            self.assertNotIn("bw_bank_ds_slot", four, fn)

    def test_font_state_is_native_storage_not_the_ds_object(self):
        # gFontState's DS slot is 16 bytes; with a pointer in the bit reader
        # it is 24, and the extra 8 landed on gGlyphExpandTable[0..3]: colour
        # 8 in every blank pixel group of every glyph (the yellow dashes).
        src = "extern struct FontState gFontState;\nvoid f(void) { gFontState.textColor[0] = 1; }\n"
        rc, out = self.run_tool("src/game/arm9/gf_font.c", src)
        self.assertEqual(rc, 0)
        self.assertIn("static struct FontState gFontState;", out)
        self.assertIn("#undef gFontState", out)

    def test_other_sources_keep_their_font_state(self):
        src = "extern struct FontState gFontState;\n"
        rc, out = self.run_tool("src/game/arm9/other.c", src)
        self.assertEqual(rc, 0)
        self.assertNotIn("static struct FontState", out)

    def test_battle_event_state_is_native_storage_owned_by_one_unit(self):
        # btl_event.c's list and variable stack are two DS objects that
        # native pointers enlarge; they become the unit's own storage.
        src = ("extern struct BtlEventFactor gBtlEventFactorStorage[BTL_EVENT_FACTOR_MAX];\n"
               "extern struct BtlEventFactorList gBtlEventFactorList;\n"
               "extern struct BtlEventFactor *gBtlEventFactorFreePool[BTL_EVENT_FACTOR_MAX];\n"
               "extern struct BtlEventVarStack gBtlEventVarStack;\n")
        rc, out = self.run_tool("src/game/ov093/btl_event.c", src)
        self.assertEqual(rc, 0)
        self.assertIn("static struct BtlEventFactorList gBtlEventFactorList;", out)
        self.assertIn("static struct BtlEventVarStack gBtlEventVarStack;", out)


class LeftAlone(unittest.TestCase):
    """What the rewrite must not touch."""

    def same(self, src, wrap=True):
        if wrap:
            src = BODY % src
        out, sites, _ = dsaddr.rewrite(src, "x.c")
        self.assertEqual(sites, [])
        self.assertEqual(out, src)

    def test_comments_and_strings(self):
        self.same("/* (vu16 *)0x04000208 */ x = 1;\n")
        self.same("// *(vu16 *)0x04000208 = 1;\nx = 1;\n")
        self.same("// continued \\\n *(vu16 *)0x04000208 = 1;\nx = 1;\n")
        self.same("// continued \\\r\n *(vu16 *)0x04000208 = 1;\r\nx = 1;\r\n")
        self.same('s = "(vu16 *)0x04000208";\n')
        self.same('s = "\\"(vu16 *)0x04000208";\n')
        self.same("c = '\\''; s = \"(u8 *)0x02000000\";\n")

    def test_literals_not_cast_to_a_pointer(self):
        self.same("x = 0x04000000;\n")
        self.same("n = (u32)0x04000000;\n")
        self.same("n = (unsigned long)0x02000000;\n")
        self.same("if (p == 0x02000000) f(0x04000000);\n")
        self.same("#define REG_BASE 0x04000000\n", wrap=False)
        self.same("#if ADDR == 0x02000000\n#endif\n")
        self.same("y = f(a) (0x04000000);\n")
        self.same("y = (a) + 0x04000000;\n")
        self.same("y = sizeof(u8 *) + 0x04000000;\n")
        self.same("y = (u8 *)(base + 0x06000000);\n")

    def test_outside_the_ds_regions(self):
        self.same("p = (u8 *)0x01FF8000;\n")
        self.same("p = (u8 *)0x08000000;\n")
        self.same("p = (u8 *)0x01FFFFFF;\n")
        self.same("p = (void *)0x45585048;\n")
        self.same("p = (u8 *)0x204000000;\n")
        self.same("x = f_0x04000000;\n")

    def test_function_pointer(self):
        self.same("((void (*)(void))0x02005124)();\n")
        self.assertEqual(kinds("((void (*)(void))0x02005124)();\n"), ["funcptr-cast"])

    def test_constant_initializers(self):
        src = "static vu16 *const r = (vu16 *)0x04000208;\n"
        self.same(src, wrap=False)
        self.assertEqual(kinds(src, wrap=False), ["static-init"])
        src = "void *tab[] = { (void *)0x02000000, 0 };\n"
        self.same(src, wrap=False)
        self.assertEqual(kinds(src, wrap=False), ["static-init"])
        src = "void f(void) {\n    static u8 *p = (u8 *)0x02000000;\n}\n"
        self.same(src, wrap=False)
        self.assertEqual(kinds(src, wrap=False), ["static-init"])
        # a non-static local is an ordinary assignment
        src = "void f(void) {\n    u8 *p = (u8 *)0x02000000;\n}\n"
        self.assertIn("bw_ds_ptr", rw(src))
        src = ("void f(void) {\n    static const struct { void *p; } t[] = {\n"
               "        { (void *)0x02000000 },\n    };\n}\n")
        self.same(src, wrap=False)
        self.assertEqual(kinds(src, wrap=False), ["static-init"])
        src = "void f(void) {\n    static int n;\n    n = *(int *)0x02000000;\n}\n"
        self.assertIn("bw_ds_ptr", rw(src))

    def test_residue_kinds(self):
        self.assertEqual(kinds("#define REG_BASE 0x04000000\n"), ["define"])
        self.assertEqual(kinds("x = 0x04000000 + 8;\n"), ["bare"])
        self.assertEqual(kinds("n = (int)0x020A7174;\n"), ["int-cast"])
        self.assertEqual(kinds("x = 0x01000000;\n"), [])


def check_rewrite(test, src, want):
    """`src` rewrites to `want` (bodies, prelude stripped), the rewrite is
    idempotent and keeps every line where it was."""
    test.assertEqual(rw(src), want)
    out = dsaddr.rewrite(src, "x.c")[0]
    again, sites, _ = dsaddr.rewrite(out, "x.c")
    test.assertEqual(sites, [])
    test.assertEqual(again, out)
    test.assertEqual(rw(src).count("\n"), src.count("\n"))


def residue_kinds(src):
    return [s.kind for s in dsaddr.analyse(src)[1]]


class Constants(unittest.TestCase):
    """A file-local #define standing for a DS address, used as a cast
    operand: rewritten at the use site."""

    def test_plain_use(self):
        src = ("#define REG_KEYINPUT 0x04000130\n"
               "void f(void) { k = *(vu16 *)REG_KEYINPUT; }\n")
        check_rewrite(self, src,
                      "#define REG_KEYINPUT 0x04000130\n"
                      "void f(void) { k = *((vu16 *)bw_ds_ptr(REG_KEYINPUT)); }\n")
        self.assertEqual(residue_kinds(src), ["define-resolved"])

    def test_expression_operand_and_parens(self):
        src = ("#define REG_DMA0SAD_ADDR 0x040000B0\n"
               "void f(int n) { p = (vu32 *)(REG_DMA0SAD_ADDR + n * 12);\n"
               "  q = (vu32 *)(REG_DMA0SAD_ADDR); }\n")
        check_rewrite(self, src,
                      "#define REG_DMA0SAD_ADDR 0x040000B0\n"
                      "void f(int n) { p = ((vu32 *)bw_ds_ptr(REG_DMA0SAD_ADDR + n * 12));\n"
                      "  q = ((vu32 *)bw_ds_ptr(REG_DMA0SAD_ADDR)); }\n")

    def test_through_another_constant(self):
        src = ("#define HW_REG_BASE 0x04000000\n"
               "#define REG_IME_ADDR (HW_REG_BASE + 0x208)\n"
               "void f(void) { *(vu16 *)REG_IME_ADDR = 1; }\n")
        check_rewrite(self, src,
                      "#define HW_REG_BASE 0x04000000\n"
                      "#define REG_IME_ADDR (HW_REG_BASE + 0x208)\n"
                      "void f(void) { *((vu16 *)bw_ds_ptr(REG_IME_ADDR)) = 1; }\n")

    def test_in_another_macro_body(self):
        src = ("#define HW_BG_VRAM 0x06000000\n"
               "#define BG(n) ((void *)(HW_BG_VRAM + (n) * 0x800))\n")
        check_rewrite(self, src,
                      "#define HW_BG_VRAM 0x06000000\n"
                      "#define BG(n) (((void *)bw_ds_ptr(HW_BG_VRAM + (n) * 0x800)))\n")

    def test_left_alone(self):
        same = [
            # not a DS value
            "#define OFS 0x208\nvoid f(void) { p = (u8 *)OFS; }\n",
            # used as a number
            "#define BASE 0x04000000\nvoid f(void) { n = BASE + 8; }\n",
            # postfix binds tighter than the cast: the operand is TAB[2]
            "#define TAB 0x02000000\nvoid f(void) { p = (u8 *)TAB[2]; }\n",
            # a function-like macro call is not the constant
            "#define ADDR(x) (0x04000000 + (x))\nvoid f(void) { p = (u8 *)ADDR(2); }\n",
            # defined twice: which one a use sees depends on where it is
            "#define B 0x04000000\n#undef B\n#define B 8\nvoid f(void) { p = (u8 *)B; }\n",
        ]
        for src in same:
            out, sites, _ = dsaddr.rewrite(src, "x.c")
            self.assertEqual(sites, [], src)
            self.assertEqual(out, src)

    def test_constant_initializer(self):
        src = ("#define REG 0x04000000\n"
               "static vu32 *const r = (vu32 *)REG;\n")
        out, sites, residue = dsaddr.rewrite(src, "x.c")
        self.assertEqual((sites, out), ([], src))
        self.assertIn("static-init", [s.kind for s in residue])

    def test_other_uses_keep_it_residue(self):
        src = ("#define REG 0x04000000\n"
               "void f(void) { p = (u8 *)REG; n = REG + 4; }\n")
        self.assertIn("bw_ds_ptr(REG)", rw(src))
        r = [s for s in dsaddr.analyse(src)[1] if s.kind == "define"]
        self.assertEqual(len(r), 1)
        self.assertEqual(r[0].uses, {"cast": 1, "other": 1})


REG16 = "#define REG16(a) (*(volatile u16 *)(a))\n"


class Macros(unittest.TestCase):
    """A function-like macro that casts its parameter: its body is
    rewritten when every invocation passes a DS address."""

    def test_register_macro(self):
        src = REG16 + ("void f(void) { REG16(0x04000050) = 0;\n"
                       "  REG16(0x0400000A) = REG16(0x0400000A) & 0x43; }\n")
        check_rewrite(self, src,
                      "#define REG16(a) (*((volatile u16 *)bw_ds_ptr(a)))\n"
                      "void f(void) { REG16(0x04000050) = 0;\n"
                      "  REG16(0x0400000A) = REG16(0x0400000A) & 0x43; }\n")
        self.assertEqual(residue_kinds(src), ["macro-arg"] * 3)

    def test_argument_through_constant_or_offset(self):
        src = ("#define BASE 0x04000000\n" + REG16 +
               "void f(int i) { REG16(BASE + 8) = 1; REG16((0x04001000 + i)) = 2; }\n")
        self.assertIn("(*((volatile u16 *)bw_ds_ptr(a)))", rw(src))

    def test_spliced_multi_line_body(self):
        src = ("#define LO(a) \\\n"
               "    do { \\\n"
               "        int v = *(vu16 *)(a) & ~0x3f; \\\n"
               "        *(vu16 *)(a) = v; \\\n"
               "    } while (0)\n"
               "void f(void) { LO(0x04000048); LO(0x04001048); }\n")
        check_rewrite(self, src,
                      "#define LO(a) \\\n"
                      "    do { \\\n"
                      "        int v = *((vu16 *)bw_ds_ptr(a)) & ~0x3f; \\\n"
                      "        *((vu16 *)bw_ds_ptr(a)) = v; \\\n"
                      "    } while (0)\n"
                      "void f(void) { LO(0x04000048); LO(0x04001048); }\n")

    def test_second_parameter(self):
        src = ("#define SET(v, a) (*(vu32 *)(a) = (v))\n"
               "void f(void) { SET(1, 0x04000000); }\n")
        self.assertIn("(*((vu32 *)bw_ds_ptr(a)) = (v))", rw(src))

    def test_mixed_invocations_leave_it_alone(self):
        src = REG16 + "void f(u32 x) { REG16(0x04000050) = 0; REG16(x) = 1; }\n"
        out, sites, residue = dsaddr.rewrite(src, "x.c")
        self.assertEqual((sites, out), ([], src))
        self.assertEqual([s.kind for s in residue], ["macro-arg-mixed"])
        self.assertEqual(residue[0].note, "REG16")

    def test_left_alone(self):
        same = [
            # no invocation: nothing shows what the parameter holds
            REG16,
            # a struct-field accessor: the parameter is not the operand
            "#define W(o) (*(u32 *)(p + (o)))\nvoid f(void) { W(0x04000000) = 1; }\n",
            # used without arguments (passed on to another macro)
            REG16 + "#define AP(m) m(0x04000000)\nvoid f(void) { REG16(0x04000000) = AP(REG16); }\n",
            # an invocation in a constant initializer
            REG16 + "static u16 *p = &REG16(0x04000000);\n",
        ]
        for src in same:
            out, sites, _ = dsaddr.rewrite(src, "x.c")
            self.assertEqual(sites, [], src)
            self.assertEqual(out, src)


CALLEE = ("void func_0207D658(u32 addr, int plane, int brightness)\n{\n"
          "    *(vu16 *)addr = (u16)(plane | 0x80);\n"
          "    *(vu16 *)(addr + 4) = (u16)brightness;\n}\n")


class Callees(unittest.TestCase):
    """Functions in CALLEES translate their address parameter; callers
    hand them the DS address."""

    def test_every_entry_documented(self):
        for name, ent in dsaddr.CALLEES.items():
            self.assertTrue(ent["params"], name)
            self.assertEqual(dsaddr.CALLEE_NAMES[name], name)
            for a in ent["aliases"]:
                self.assertEqual(dsaddr.CALLEE_NAMES[a], name)
        with open(os.path.join(ROOT, "tools", "dsaddr.py")) as f:
            tool = f.read()
        for name in dsaddr.CALLEES:
            # each entry is preceded by a comment giving the evidence
            self.assertRegex(tool, r"#[^\n]*\n(?:\s*#[^\n]*\n)*\s*\"%s\":" % name)

    def test_callee_body(self):
        check_rewrite(self, CALLEE, CALLEE
                      .replace("*(vu16 *)addr", "*((vu16 *)bw_ds_ptr(addr))")
                      .replace("*(vu16 *)(addr + 4)", "*((vu16 *)bw_ds_ptr(addr + 4))"))

    def test_only_listed_functions_and_parameters(self):
        same = [
            CALLEE.replace("func_0207D658", "func_0207D659"),
            CALLEE.replace("addr", "base"),
            # a prototype is not the definition
            "void func_0207D658(u32 addr, int p, int b);\n"
            "void g(u32 addr) { *(vu16 *)addr = 0; }\n",
        ]
        for src in same:
            out, sites, _ = dsaddr.rewrite(src, "x.c")
            self.assertEqual(sites, [], src)
            self.assertEqual(out, src)

    def test_callers(self):
        src = ("void f(void) {\n"
               "    func_0207D658(0x04000050, 1, 2);\n"
               "    G2x_SetBlendBrightness_((void *)0x04001050, 1, 2);\n"
               "    func_0207D658(0x04000050 + 0x1000, 1, 2);\n"
               "    other((void *)0x04000050, 1, 2);\n"
               "    func_0207D658((u32)p, (int)(u8 *)0x02000000, 2);\n}\n")
        out, sites, residue = dsaddr.rewrite(src, "x.c")
        # the pointer cast standing as the address argument stays a DS
        # address; one passed elsewhere is rewritten
        self.assertIn("G2x_SetBlendBrightness_((void *)0x04001050, 1, 2);", out)
        self.assertIn("other(((void *)bw_ds_ptr(0x04000050u)), 1, 2);", out)
        self.assertIn("(int)((u8 *)bw_ds_ptr(0x02000000u))", out)
        self.assertEqual([s.kind for s in residue], ["callee-arg"] * 3)
        self.assertEqual({s.note for s in residue}, {"func_0207D658"})


class Residue(unittest.TestCase):
    """What the analysis records about a literal it leaves alone, and the
    report category it gets."""

    def one(self, body, wrap=True):
        src = BODY % body if wrap else body
        r = dsaddr.analyse(src)[1]
        self.assertEqual(len(r), 1, r)
        s = r[0]
        line = src.split("\n")[s.line - 1].strip()
        return s, dsaddr.classify(s, line, src[s.start:s.end], {}, "x.c")

    def test_operators_and_calls(self):
        s, cat = self.one("x = (v & 0x07000000) >> 24;")
        self.assertEqual((s.prev_op, s.next_op, cat), ("&", ")", "mask"))
        s, cat = self.one("if (a >= 0x02000000 && a < 8) {}")
        self.assertEqual((s.prev_op, cat), (">=", "range"))
        s, cat = self.one("if ((u32)ptr < 0x02400000) {}")
        self.assertEqual(cat, "range-host")
        s, cat = self.one("g(1, h(0x021F55B4, 2));")
        self.assertEqual((s.call, cat), (("h", 0, True), "call-image"))
        s, cat = self.one("fill(0, 0x06800000, 0x20000);")
        self.assertEqual((s.call, cat), (("fill", 1, True), "call-unknown"))
        s, cat = self.one("PXI_SendWordByFifo(4, 0x03002000, 0);")
        self.assertEqual(cat, "pxi")
        s, cat = self.one("e->c = 0x021BA521;")
        self.assertEqual(cat, "held-image")
        s, cat = self.one("u32 ptr = 0x06400000;")
        self.assertEqual(cat, "held-hw")
        s, cat = self.one("n = v / 100000000;")
        self.assertEqual(cat, "decimal")

    def test_callee_parameter_types(self):
        src = BODY % "set(0x0400006C, 0); seti(0x0400006C, 0);"
        res = dsaddr.analyse(src)[1]
        idx = {"set": ("a.c", [True, False]), "seti": ("b.c", [False, False])}
        cats = [dsaddr.classify(s, "", "0x0400006C", idx, "x.c") for s in res]
        self.assertEqual(cats, ["pointer-param", "call-int"])

    def test_asm_body(self):
        src = ("asm void f(void)\n{\n    ldr r0, =0x027FFC00\n"
               "    dcd 0x02000000\n    bx lr\n}\n")
        res = dsaddr.analyse(src)[1]
        self.assertEqual([s.asm for s in res], [True, True])
        self.assertEqual({dsaddr.classify(s, "", "0x0", {}) for s in res}, {"asm"})

    def test_every_kind_has_a_category(self):
        for k in ("macro-arg", "callee-arg", "define-resolved", "directive",
                  "static-init", "macro-arg-mixed"):
            self.assertIn(k, dsaddr.CATEGORIES)


class Report(unittest.TestCase):

    def test_generated_and_stable(self):
        with tempfile.TemporaryDirectory() as d:
            os.makedirs(os.path.join(d, "arm9"))
            files = {
                "arm9/a.c": REG16 + "void a(void) { REG16(0x04000050) = 0; x = (v & 0x07000000); }\n",
                "arm9/b.c": "void b(void) { func_0207D658(0x04000050, 1, 2); e->c = 0x021BA521; }\n",
                "arm9/c.c": "void c(void) { func_0207D658(p, 1, 2); }\n",
            }
            for name, text in files.items():
                with open(os.path.join(d, name), "w", newline="") as f:
                    f.write(text)
            one = dsaddr.build_report(d)
            self.assertEqual(one, dsaddr.build_report(d))
            for want in ("## Register macro arguments (resolved, 1)",
                         "## Bit patterns (harmless, 1)",
                         "## Address arguments of translating callees (resolved, 1)",
                         "## Game code or data addresses held as integers (port, 1)",
                         "| func_0207D658 | arm9/c.c:1 | `p` | check |",
                         "| func_0207D658 | arm9/b.c:1 | `0x04000050` | yes |",
                         "| `REG16(a)` | arm9/a.c:1 |"):
                self.assertIn(want, one)
            self.assertNotIn(d, one)
            for c in one:
                self.assertLess(ord(c), 128)     # plain ASCII: no em dashes


# ---------------------------------------------------------------------------
# With a compiler
# ---------------------------------------------------------------------------

FIXTURE = r"""
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef volatile unsigned short vu16;
typedef volatile unsigned int vu32;
typedef volatile u16 *REGP;
struct Work { u32 a, b; };
#define REG_IME (*(vu16 *)0x04000208)
#define REG_DISPCNT (*(volatile u32 *) \
    (0x04000000))
#define REG16(a) (*(vu16 *)(a))
#define HW_BG_VRAM 0x06000000
static u32 static_base = 0x04000000;   /* not a cast: left alone */
static vu16 *const static_reg = (vu16 *)0x04000208;   /* constant: left alone */
u32 fixture(u32 offset, int dmaNo);
void func_0207D658(u32 addr, int plane, int brightness);
void func_0207D658(u32 addr, int plane, int brightness)
{
    *(vu16 *)addr = (u16)(plane | 0x80);
    *(vu16 *)(addr + 4) = (u16)brightness;
}
u32 fixture(u32 offset, int dmaNo)
{
    u32 r = 0;
    const struct Work *w = (const struct Work *)0x027FFC00;
    u8 (*rows)[4] = (u8 (*)[4])0x06000000;
    void (**slot)(u32) = (void (**)(u32))0x020A8144;
    REG_IME = 1;
    REG_DISPCNT |= 0x10000;
    r += *(vu16 *)0x04000006;
    r += *(volatile unsigned short*)(0x04000130);
    r += *(REGP)0x04000004;
    r += w->a + rows[1][2] + (slot != 0);
    r += *(vu32 *)(0x040000B0 + dmaNo * 12);
    r += ((u8 *)0x05000000u)[offset];
    r += *(u8 *)(0x07000000 + offset);
    r += (const void *)(u8 *)0x02000000 != 0;
    r += static_base + *static_reg;
    REG16(0x04000050) = 1;
    r += *(u8 *)(HW_BG_VRAM + offset);
    func_0207D658(0x04000050, 1, 2);
    return r; /* (vu16 *)0x04000208 in a comment */
}
"""


def compile_only(path, include_dirs=()):
    """Syntax-check `path`; returns (ok, diagnostics)."""
    flags = shlex.split(CFG.cflags or "")
    if CFG.cc_id == "MSVC":
        cmd = [CFG.cc, "/nologo", "/Zs", "/W3"] + flags
        cmd += ["/I" + d for d in include_dirs] + [path]
    else:
        cmd = [CFG.cc, "-std=c99", "-fsyntax-only", "-Wall", "-Wextra",
               "-Wno-unused-parameter", "-Wno-unused-variable",
               "-Wno-missing-prototypes", "-fsigned-char"] + flags
        cmd += ["-I" + d for d in include_dirs] + [path]
    p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                       universal_newlines=True, errors="replace")
    out = p.stdout
    if CFG.cc_id == "MSVC":
        # cl echoes the source name; that is not a diagnostic
        out = "\n".join(l for l in out.splitlines()
                        if l.strip() and l.strip() != os.path.basename(path))
    return p.returncode == 0, out.strip()


def _diag_lines(text):
    """Diagnostics with the file name dropped (the copy and the original
    name the same file and line through #line)."""
    out = []
    for l in text.splitlines():
        m = re.search(r":(\d+)(?::\d+)?: ?(warning|error)\b.*$", l)
        if m:
            out.append(m.group(0))
    return sorted(out)


class Compiles(unittest.TestCase):

    def setUp(self):
        if not CFG.cc:
            self.skipTest("no --cc given")
        self.tmp = tempfile.TemporaryDirectory()

    def tearDown(self):
        if hasattr(self, "tmp"):
            self.tmp.cleanup()

    def write(self, name, text):
        p = os.path.join(self.tmp.name, name)
        with open(p, "w", newline="") as f:
            f.write(text)
        return p

    def test_fixture_parses_warning_free(self):
        orig = self.write("fixture.c", FIXTURE)
        out, sites, residue = dsaddr.rewrite(FIXTURE, orig)
        self.assertEqual(len(sites), 16)
        self.assertEqual(sorted(s.kind for s in residue),
                         ["bare", "callee-arg", "define-resolved",
                          "macro-arg", "static-init"])
        p = self.write("fixture_rw.c", out)
        ok, diag = compile_only(p)
        self.assertTrue(ok, diag)
        self.assertEqual(diag, "")

    def test_prelude_agrees_with_dsmem_h(self):
        # A conflicting redeclaration is a hard error in C, so this is the
        # proof that `unsigned int` is u32 on this compiler and target.
        p = self.write("agree.c", dsaddr.PRELUDE_DECL + "\n"
                       '#include "dsmem.h"\n'
                       "void *agree(void);\n"
                       "void *agree(void) { return bw_ds_ptr(0x04000208u); }\n")
        ok, diag = compile_only(p, [os.path.join(ROOT, "sdk")])
        self.assertTrue(ok, diag)
        self.assertEqual(diag, "")

    def test_build_sources_parse_after_rewrite(self):
        if not CFG.sources:
            self.skipTest("no --sources given")
        with open(CFG.sources) as f:
            srcs = [l.strip() for l in f if l.strip()]
        self.assertTrue(srcs)
        changed = []
        for i, src in enumerate(srcs):
            text = dsaddr._read(src)
            out, sites, _ = dsaddr.rewrite(text, src)
            if not sites:
                self.assertEqual(out, text)
                continue
            changed.append((src, len(sites)))
            p = self.write("b%d.c" % i, out)
            inc = [os.path.dirname(src)]
            ok0, d0 = compile_only(src, inc)
            ok1, d1 = compile_only(p, inc)
            self.assertTrue(ok1 or not ok0, "%s: %s" % (src, d1))
            # nothing new; a rewrite may remove a diagnostic (an integer
            # cast to a pointer of another size becomes a call)
            new = sorted(set(_diag_lines(d1)) - set(_diag_lines(d0)))
            self.assertEqual(new, [], src)
        print("\n  build sources: %d, rewritten: %d%s" % (
            len(srcs), len(changed),
            "".join("\n    %s (%d casts)" % c for c in changed)),
            file=sys.stderr)


class Corpus(unittest.TestCase):

    def test_every_matched_file(self):
        if not CFG.aletheia_root:
            self.skipTest("no --aletheia-root given")
        root = os.path.join(CFG.aletheia_root, "src", "matched")
        self.assertTrue(os.path.isdir(root), root)
        files = casts = 0
        changed = 0
        for d, _, names in os.walk(root):
            for nm in names:
                if not nm.endswith(".c"):
                    continue
                path = os.path.join(d, nm)
                src = dsaddr._read(path)
                out, sites, _ = dsaddr.rewrite(src, path)
                files += 1
                if sites:
                    changed += 1
                    casts += len(sites)
                    # the prelude adds three lines and nothing else moves
                    self.assertEqual(out.count("\n"), src.count("\n") + 3, path)
                    again, more, _ = dsaddr.rewrite(out, path)
                    self.assertEqual(more, [], path)
                    self.assertEqual(again, out, path)
                else:
                    self.assertEqual(out, src, path)
        print("\n  corpus: %d files, %d rewritten, %d casts" % (files, changed, casts),
              file=sys.stderr)
        self.assertGreater(casts, 2000)

    def test_report(self):
        # every residue site gets a category (an unknown one would raise),
        # and the register macros, constants and callees each rewrite some
        if not CFG.aletheia_root:
            self.skipTest("no --aletheia-root given")
        text = dsaddr.build_report(os.path.join(CFG.aletheia_root, "src", "matched"))
        # (Since aletheia's wave 20 no src/matched file reaches a DS address
        # through a callee, so that count may be absent.)
        m = re.search(r"(\d+) through a define, (\d+) through a macro", text)
        self.assertTrue(m, text[:2000])
        self.assertTrue(all(int(g) > 0 for g in m.groups()), m.group(0))
        print("\n  report: %s" % m.group(0), file=sys.stderr)


def main():
    ap = argparse.ArgumentParser(add_help=False)
    ap.add_argument("--aletheia-root")
    ap.add_argument("--sources")
    ap.add_argument("--cc")
    ap.add_argument("--cc-id", default="GNU")
    ap.add_argument("--cflags", default="")
    a, rest = ap.parse_known_args()
    CFG.__dict__.update(vars(a))
    unittest.main(argv=[sys.argv[0]] + rest, verbosity=2)


if __name__ == "__main__":
    main()
