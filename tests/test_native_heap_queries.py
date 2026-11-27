#!/usr/bin/env python3
"""Compile the real native arena query bodies and historical-name forwarders.
No ROM or SDK is required; layouts come from Aletheia, not a test mirror.
"""
import argparse
from pathlib import Path
import re
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser()
parser.add_argument('--source', required=True)
parser.add_argument('--cc', required=True)
parser.add_argument('--build')
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as directory:
    adapted = Path(directory) / 'native.c'
    subprocess.run([sys.executable, '-I', str(root / 'tools/dsaddr.py'),
                    args.source, '-o', str(adapted), '--pointer-width', '8'], check=True)
    source = adapted.read_text()
    manager_source = Path(args.source).with_name('gfl_heap.c')
    subprocess.run([sys.executable, '-I', str(root / 'tools/dsaddr.py'),
                    str(manager_source), '-o', str(adapted), '--pointer-width', '8'], check=True)
    manager = adapted.read_text()


def declaration(kind, name, text=None):
    match = re.search(r'^' + kind + r' ' + name + r'\s*\{.*?^};', source if text is None else text, re.M | re.S)
    if match is None:
        raise RuntimeError('canonical declaration not found: ' + name)
    return match.group(0)


def function(name, text=None):
    match = re.search(r'^(?:int|s32|u32) ' + name + r'\([^;]*?\)\n\{.*?^}', source if text is None else text, re.M | re.S)
    if match is None:
        raise RuntimeError('canonical function not found: ' + name)
    return match.group(0)


fixture = '''#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
typedef int32_t s32; typedef uintptr_t uptr;
'''
fixture += '#include <stddef.h>\n#include "' + str(Path(args.source).parent / 'nns_fnd_types.h') + '"\n'
fixture += '\n'.join(declaration('struct', name) for name in ('MBlock', 'MBlockList', 'ExpHeap', 'Heap', 'HeapBlock'))
fixture += '\n' + function('NNSi_FndGetListTotalSize')
fixture += '\n' + function('NNSi_FndGetListAllocatableSize')
fixture += '\n'.join(declaration('struct', name, manager) for name in ('HeapRecord', 'HeapManager'))
# Keep an old private mirror available only to demonstrate the old generator's
# wrong field access at runtime; the corrected function never uses it.
old_manager = manager_source.read_text()
fixture += '\n' + declaration('struct', 'ExpHeapHead', old_manager)
fixture += '\nstruct HeapManager gHeapManager;\nstatic void *current_heap;\n'
fixture += 'void *GFL_HEAP_GetHeap(u32 id) { return id == 0 ? current_heap : 0; }\n'
fixture += function('GFL_HEAP_GetHeapSize', manager)

fixture += '''
extern int sub_02060E6C(int *arena);
extern s32 sub_02060E94(struct Heap *arena, s32 align);
void bw_panic(const char *name, u32 address, const char *reason) {
    (void)address; fprintf(stderr, "%s: %s\\n", name, reason); abort();
}
int main(void) {
    struct ExpHeap heap = {0};
    struct MBlock b = {0}, a = {0};
    if (sub_02060E6C((int *)&heap) != 0) return 1;
    a.size = 96; b.size = 160; a.next = &b;
    heap.freeList.head = &a; heap.freeList.tail = &b;
    if (sub_02060E6C((int *)&heap) != 256) return 2;
    for (unsigned align = 4; align <= 256; align *= 2) {
        uintptr_t ap = (uintptr_t)&a + sizeof(a), bp = (uintptr_t)&b + sizeof(b);
        unsigned aa = (unsigned)(((ap + align - 1) & ~(uintptr_t)(align - 1)) - ap);
        unsigned ba = (unsigned)(((bp + align - 1) & ~(uintptr_t)(align - 1)) - bp);
        unsigned afree = aa < a.size ? a.size - aa : 0;
        unsigned bfree = ba < b.size ? b.size - ba : 0;
        unsigned expected = afree > bfree ? afree : bfree;
        if ((unsigned)sub_02060E94((struct Heap *)&heap, (s32)align) != expected) return 5;
        if ((unsigned)sub_02060E94((struct Heap *)&heap, -(s32)align) != expected) return 6;
    }
    heap.freeList.head = &b;
    if (sub_02060E6C((int *)&heap) != 160) return 3;
    heap.freeList.head = 0;
    if (sub_02060E6C((int *)&heap) != 0) return 4;
    union { void *align; unsigned char bytes[4096]; } storage = {0};
    NNSiFndHeapHead *owner = (NNSiFndHeapHead *)storage.bytes;
    current_heap = storage.bytes;
    owner->heapEnd = storage.bytes + sizeof(storage.bytes);
    gHeapManager.heapIdCount = 2;
    if (GFL_HEAP_GetHeapSize(0) != sizeof(storage.bytes)) return 7;
    if (GFL_HEAP_GetHeapSize(0x8000) != sizeof(storage.bytes)) return 8;
    if (GFL_HEAP_GetHeapSize(2) != 0 || gHeapManager.lastError != 1) return 9;
    if (GFL_HEAP_GetHeapSize(1) != 0) return 10;
    puts("native free-list query: empty, multiple blocks and list changes pass");
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    directory = Path(directory)
    cfile = directory / 'query.c'
    cfile.write_text(fixture)
    binary = directory / 'query'
    subprocess.run([args.cc, '-std=c99', '-O2', '-I' + str(root / 'sdk'),
                    str(cfile), str(root / 'stubs/stub_02060E6C.c'),
                    str(root / 'stubs/stub_02060E94.c'),
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)

# The native query must remain in the boot closure, not only in this test.
boot = (root / 'cmake/boot.cmake').read_text()
floor = boot.split('if(NOT EIDOS_HEAP_MATCHED)', 1)[1].split('endif()', 1)[0]
for name in ('02060E6C', '02060E94'):
    assert 'src/matched/arm9/func_' + name + '.c' not in floor, 'native query cut from boot'

if args.build:
    build = Path(args.build)
    cuts = (build / 'boot/boot_cuts.txt').read_text()
    for name in ('NNSi_FndGetListTotalSize', 'NNSi_FndGetListAllocatableSize'):
        assert name not in cuts, 'canonical query removed from generated closure: ' + name
    sources = (build / 'boot/shim_sources_gen.txt').read_text()
    for name in ('stub_02060E6C.c', 'stub_02060E94.c'):
        assert name not in sources, 'alias incorrectly marked as query replacement: ' + name
