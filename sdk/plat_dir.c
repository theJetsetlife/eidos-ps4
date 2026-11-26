/*
 * sdk/plat_dir.c -- path kind and directory listing, for Win32 and for
 * POSIX (sdk/plat_dir.h says why it exists).
 *
 * Win32 uses GetFileAttributesA and FindFirstFileA/FindNextFileA, the ANSI
 * forms, because the rest of the file layer opens files with narrow fopen
 * and both must agree on what a path names.  POSIX hosts use stat and opendir/readdir. Orbis lists
 * directories through its SDK file API.
 */

/* stat and opendir are POSIX, not C99: ask for them under -std=c99. */
#if !defined(_WIN32) && !defined(__ORBIS__) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <string.h>

#include "plat_dir.h"

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

int bw_plat_path_kind(const char *path)
{
    DWORD a;

    if (path == NULL || *path == '\0') {
        return BW_PATH_NONE;
    }
    a = GetFileAttributesA(path);
    if (a == INVALID_FILE_ATTRIBUTES) {
        return BW_PATH_NONE;
    }
    if ((a & FILE_ATTRIBUTE_DIRECTORY) != 0) {
        return BW_PATH_DIR;
    }
    if ((a & FILE_ATTRIBUTE_DEVICE) != 0) {
        return BW_PATH_OTHER;
    }
    return BW_PATH_FILE;
}

int bw_plat_list_dir(const char *path, int (*fn)(void *ctx, const char *name),
                     void *ctx)
{
    char pattern[1200];
    WIN32_FIND_DATAA fd;
    HANDLE h;
    int rc = 0;

    if (path == NULL || strlen(path) + 3 > sizeof(pattern)) {
        return -1;
    }
    sprintf(pattern, "%s/*", path);
    h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE) {
        return -1;
    }
    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
            continue;
        }
        rc = fn(ctx, fd.cFileName);
    } while (rc == 0 && FindNextFileA(h, &fd));
    FindClose(h);
    return rc;
}

#else /* POSIX */

#if defined(__ORBIS__)
#include <_fs.h>
#include <stddef.h>
#include <sys/dirent.h>
#else
#include <dirent.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>

int bw_plat_path_kind(const char *path)
{
    struct stat st;

    if (path == NULL || *path == '\0' || stat(path, &st) != 0) {
        return BW_PATH_NONE;
    }
    if (S_ISDIR(st.st_mode)) {
        return BW_PATH_DIR;
    }
    if (S_ISREG(st.st_mode)) {
        return BW_PATH_FILE;
    }
    return BW_PATH_OTHER;
}

int bw_plat_list_dir(const char *path, int (*fn)(void *ctx, const char *name),
                     void *ctx)
{
#if defined(__ORBIS__)
    union {
        struct dirent alignment;
        char bytes[4096];
    } buffer;
    const size_t header = offsetof(struct dirent, d_name);
    int result = 0;
    int count;
    int fd;

    if (path == NULL || (fd = sceKernelOpen(path, SCE_KERNEL_O_RDONLY |
                                          SCE_KERNEL_O_DIRECTORY, 0)) < 0) {
        return -1;
    }
    while (result == 0 &&
           (count = sceKernelGetdents(fd, buffer.bytes, sizeof(buffer.bytes))) > 0) {
        size_t offset = 0;
        while (result == 0 && offset < (size_t)count) {
            struct dirent entry;
            size_t remaining = (size_t)count - offset;
            const char *name = buffer.bytes + offset + header;

            if (remaining <= header) {
                result = -1;
                break;
            }
            memcpy(&entry, buffer.bytes + offset, header);
            if (entry.d_reclen > remaining || entry.d_reclen <= header ||
                entry.d_namlen >= entry.d_reclen - header ||
                name[entry.d_namlen] != '\0') {
                result = -1;
                break;
            }
            offset += entry.d_reclen;
            if (entry.d_fileno != 0 && strcmp(name, ".") != 0 &&
                strcmp(name, "..") != 0) {
                result = fn(ctx, name);
            }
        }
    }
    sceKernelClose(fd);
    return result == 0 && count < 0 ? -1 : result;
#else
    DIR *d;
    struct dirent *e;
    int rc = 0;

    if (path == NULL || (d = opendir(path)) == NULL) {
        return -1;
    }
    while (rc == 0 && (e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0) {
            continue;
        }
        rc = fn(ctx, e->d_name);
    }
    closedir(d);
    return rc;
#endif
}

#endif
