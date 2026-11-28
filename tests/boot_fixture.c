/* Synthetic boot process for checking the CMake boot-test harnesses. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int in_test_root(const char *name, const char *root)
{
    const char *value = getenv(name);
    size_t n = strlen(root);
    if (value != NULL && strncmp(value, root, n) == 0 &&
        (value[n] == '/' || value[n] == '\\' || value[n] == '\0')) return 1;
    fprintf(stderr, "unsafe %s: %s\n", name, value != NULL ? value : "(unset)");
    return 0;
}

int main(void)
{
    static const char *names[] = {
        "EIDOS_HOME", "EIDOS_USER_DIR", "BW_SAVE_DIR", "EIDOS_SETTINGS", "EIDOS_MODS_DIR"
    };
    const char *root = getenv("EIDOS_TEST_ALLOWED_ROOT");
    const char *mode = getenv("EIDOS_TEST_BOOT_MODE");
    size_t i;

    if (root == NULL || mode == NULL) return 90;
    for (i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
        if (!in_test_root(names[i], root)) return 91;
    }
    if (strcmp(mode, "refusal") == 0) {
        puts("the boot needs an ILP32 build");
        return 42;
    }
    if (strcmp(mode, "skip_marker_error") == 0) {
        puts("SKIPPED: no game data at a child process diagnostic");
        return 42;
    }
    if (strcmp(mode, "missing") == 0) return 0;
    if (strcmp(mode, "identity") == 0)
        puts("eidos_boot: DS address space identity-mapped");
    else
        puts("eidos_boot: DS address space on movable host blocks");
    puts("eidos_boot: ran after 1 frames, 1 vblank interrupts");
    puts(strcmp(mode, "wrong_overlay") == 0 ?
         "eidos_boot: resident overlays: 12" : "eidos_boot: resident overlays: 21 93");
    if (strcmp(mode, "panic") == 0) puts("bw-port: PANIC in test fixture");
    if (strcmp(mode, "ubsan") == 0) puts("fixture.c:1: runtime error: signed integer overflow");
    return strcmp(mode, "error_after") == 0 ? 42 : 0;
}
