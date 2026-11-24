/* Orbis setup paths are rooted explicitly in this application's SaveData
 * mount. Do not infer a Linux home directory or consult a process environment. */

#include <stdio.h>
#include <string.h>

#include "setup.h"

int bw_os_app_home(char *out, size_t n)
{
    (void)out;
    (void)n;
    return -1; /* The PS4 front end supplies its mounted SaveData root. */
}

int bw_os_exe_dir(char *out, size_t n)
{
    (void)out;
    (void)n;
    return -1; /* App content is read-only; portable mode is not applicable. */
}

const char *bw_os_getenv(const char *name, char *buf, size_t n)
{
    (void)name;
    (void)buf;
    (void)n;
    return NULL;
}

int bw_os_abspath(const char *path, char *out, size_t n)
{
    int written;

    if (path == NULL || out == NULL || n == 0) {
        return -1;
    }
    if (path[0] != '/') {
        return -1; /* SaveData paths and supported external paths are absolute. */
    }
    written = snprintf(out, n, "%s", path);
    return written >= 0 && (size_t)written < n ? 0 : -1;
}

char **bw_os_utf8_argv(int *argc, char **argv)
{
    (void)argc;
    return argv;
}
