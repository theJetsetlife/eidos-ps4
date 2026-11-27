/* Run log for the PS4 build (log_ps4.h). */

#include "log_ps4.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define LOG_NAME "EIDOS_LOG.txt"
#define LOG_FALLBACK_DIR "/data/eidos"
#define LOG_MAX_BYTES (2 * 1024 * 1024)

static const char *const usb_roots[] = {"/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3",
                                        "/mnt/usb4", "/mnt/usb5", "/mnt/usb6", "/mnt/usb7"};

static char g_path[128];
static int g_fd = -1;
static int g_on_usb;
static int g_started;
static int g_exporting;

static int copy_file(const char *src, const char *dst)
{
    char buf[4096];
    ssize_t r;
    int in = open(src, O_RDONLY), out;

    if (in < 0) return -1;
    out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out < 0) {
        close(in);
        return -1;
    }
    while ((r = read(in, buf, sizeof buf)) > 0) {
        ssize_t off = 0;
        while (off < r) {
            ssize_t w = write(out, buf + off, (size_t)(r - off));
            if (w <= 0) {
                fsync(out);
                close(out);
                close(in);
                return -1;
            }
            off += w;
        }
    }
    fsync(out);
    close(out);
    close(in);
    return 0;
}

static int usb_path(size_t i, const char *name, char *dst, size_t n)
{
    struct stat st;

    if (stat(usb_roots[i], &st) != 0 || !S_ISDIR(st.st_mode)) return -1;
    snprintf(dst, n, "%s/%s", usb_roots[i], name);
    return 0;
}

/* ---- opening ----------------------------------------------------------- */

static int open_log(const char *path)
{
    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0666);
    struct stat st;

    if (fd < 0) return -1;
    if (fstat(fd, &st) == 0 && st.st_size > LOG_MAX_BYTES) {
        close(fd);
        fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_APPEND, 0666);
    }
    return fd;
}

static int try_usb(int *fd_out)
{
    size_t i;
    char path[128];

    for (i = 0; i < sizeof usb_roots / sizeof usb_roots[0]; i++) {
        struct stat st;
        int fd;
        if (stat(usb_roots[i], &st) != 0 || !S_ISDIR(st.st_mode)) continue;
        snprintf(path, sizeof path, "%s/%s", usb_roots[i], LOG_NAME);
        fd = open_log(path);
        if (fd < 0) continue;
        snprintf(g_path, sizeof g_path, "%s", path);
        *fd_out = fd;
        return 0;
    }
    return -1;
}

/* ---- public ------------------------------------------------------------- */

static void at_exit(void)
{
    /* Written on every orderly exit.  A log that ends without this line is a
     * crash or a hang: the Orbis SDK has no signal or exception hook for an
     * application, so the last line before the end is the evidence. */
    if (g_fd >= 0) {
        static const char msg[] = "\n==== clean exit ====\n";
        (void)write(g_fd, msg, sizeof msg - 1);
        fsync(g_fd);
    }
    eidos_ps4_log_export();
}

void eidos_ps4_log_start(const char *title_id, const char *version)
{
    int fd = -1;

    if (g_started) return;
    g_started = 1;
    if (try_usb(&fd) == 0) {
        char prev[128];
        size_t i;

        g_on_usb = 1;
        /* a run that had no drive in left its log on internal storage */
        for (i = 0; i < sizeof usb_roots / sizeof usb_roots[0]; i++) {
            if (usb_path(i, "EIDOS_LOG_PREV.txt", prev, sizeof prev) == 0 &&
                copy_file(LOG_FALLBACK_DIR "/" LOG_NAME, prev) == 0) {
                unlink(LOG_FALLBACK_DIR "/" LOG_NAME);
                break;
            }
        }
    } else {
        mkdir("/data", 0777);
        mkdir(LOG_FALLBACK_DIR, 0777);
        snprintf(g_path, sizeof g_path, "%s/%s", LOG_FALLBACK_DIR, LOG_NAME);
        fd = open_log(g_path);
    }
    if (fd < 0) {
        g_path[0] = 0;
        return;
    }
    g_fd = fd;
    /* the Orbis libc has no dup2: reopen the two streams on the same file in
     * append mode, so their writes and ours interleave in order */
    freopen(g_path, "a", stdout);
    freopen(g_path, "a", stderr);
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    fprintf(stderr, "\n==== Eidos %s %s run log ====\n", title_id ? title_id : "?",
            version ? version : "?");
    fprintf(stderr, "log: %s (%s)\n", g_path,
            g_on_usb ? "USB drive" :
            "internal storage; applications cannot see USB drives on every system, read it with tools/ps4/fetch_log.sh");
    atexit(at_exit);
}

int eidos_ps4_log_export(void)
{
    char dst[128];
    size_t i;

    if (g_fd < 0) return -1;
    if (g_on_usb) return 0;
    if (g_exporting) return -1;
    g_exporting = 1;
    fsync(g_fd);
    for (i = 0; i < sizeof usb_roots / sizeof usb_roots[0]; i++) {
        if (usb_path(i, LOG_NAME, dst, sizeof dst) != 0) continue;
        if (copy_file(g_path, dst) == 0) {
            g_exporting = 0;
            return 0;
        }
    }
    g_exporting = 0;
    return -1;
}

void eidos_ps4_log_finish(int status)
{
    fflush(stdout);
    fflush(stderr);
    if (g_fd >= 0) {
        char line[64];
        int n = snprintf(line, sizeof line, "\n==== exit (status %d) ====\n", status);
        if (n > 0) (void)write(g_fd, line, (size_t)n);
        fsync(g_fd);
    }
    eidos_ps4_log_export();
}

const char *eidos_ps4_log_path(void)
{
    return g_path;
}
