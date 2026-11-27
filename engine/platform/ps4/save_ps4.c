#include "save_ps4.h"

#include <save_data.h>
#include <stdio.h>
#include <string.h>

/* Private application save directory, independent of a commercial title ID.
 * SaveData associates it with the running application's assigned identity.
 */
#define EIDOS_PS4_SAVE_DIRECTORY "eidos_ps4_saves"
#define EIDOS_PS4_DATA_DIRECTORY "eidos_ps4_game_data"
/* Eight cartridge-sized slots plus atomic replacement and port metadata. */
#define EIDOS_PS4_SAVE_BLOCKS 256u

static SceSaveDataMountPoint g_mount_point;
static SceSaveDataMountPoint g_data_mount_point;
static int g_save_data_initialized;
static int g_save_data_mounted;
static int g_data_mounted;

static int mount_directory(SceUserServiceUserId user_id, const char *name,
                           SceSaveDataBlocks blocks,
                           SceSaveDataMountPoint *mount_point, int *mounted,
                           char *path, size_t path_size, const char **operation)
{
    SceSaveDataDirName dir_name;
    SceSaveDataMount2 mount;
    SceSaveDataMountResult result;
    size_t name_length;
    int rc;

    *operation = "SaveData mount arguments";
    if (user_id < 0 || name == NULL || mount_point == NULL || mounted == NULL ||
        path == NULL || path_size < SCE_SAVE_DATA_MOUNT_POINT_DATA_MAXSIZE ||
        !g_save_data_initialized || *mounted) {
        return -1;
    }
    path[0] = '\0';
    name_length = strlen(name);
    if (name_length == 0u || name_length >= sizeof(dir_name.data)) {
        return -1;
    }
    memset(&dir_name, 0, sizeof(dir_name));
    memcpy(dir_name.data, name, name_length + 1u);
    memset(&mount, 0, sizeof(mount));
    mount.userId = user_id;
    mount.dirName = &dir_name;
    mount.blocks = blocks;
    mount.mountMode = SCE_SAVE_DATA_MOUNT_MODE_CREATE2 |
                      SCE_SAVE_DATA_MOUNT_MODE_RDWR;
    memset(&result, 0, sizeof(result));

    *operation = (mount_point == &g_mount_point) ? "sceSaveDataMount2 (saves)" :
                                                 "sceSaveDataMount2 (game data)";
    rc = sceSaveDataMount2(&mount, &result);
    if (rc < 0) {
        return rc;
    }
    *mount_point = result.mountPoint;
    *mounted = 1;
    *operation = "SaveData mount path";
    if (memchr(mount_point->data, '\0', sizeof(mount_point->data)) == NULL) {
        return -1;
    }
    {
        const int written = snprintf(path, path_size, "%s", mount_point->data);
        if (written < 0 || (size_t)written >= path_size) {
            return -1;
        }
    }
    return 0;
}

int eidos_ps4_save_mount(SceUserServiceUserId user_id, char *path,
                         size_t path_size, const char **operation)
{
    int rc;

    *operation = "SaveData mount arguments";
    if (user_id < 0 || path == NULL || path_size <
            SCE_SAVE_DATA_MOUNT_POINT_DATA_MAXSIZE ||
        g_save_data_initialized || g_save_data_mounted || g_data_mounted) {
        return -1;
    }

    *operation = "sceSaveDataInitialize3";
    rc = sceSaveDataInitialize3(NULL);
    if (rc < 0) {
        return rc;
    }
    g_save_data_initialized = 1;
    rc = mount_directory(user_id, EIDOS_PS4_SAVE_DIRECTORY,
                         EIDOS_PS4_SAVE_BLOCKS, &g_mount_point,
                         &g_save_data_mounted, path, path_size, operation);
    if (rc < 0) {
        (void)eidos_ps4_save_unmount();
        return rc;
    }
    return 0;
}

int eidos_ps4_data_mount(SceUserServiceUserId user_id, char *path,
                         size_t path_size, const char **operation)
{
    return mount_directory(user_id, EIDOS_PS4_DATA_DIRECTORY,
                           SCE_SAVE_DATA_BLOCKS_MAX,
                           &g_data_mount_point, &g_data_mounted,
                           path, path_size, operation);
}

int eidos_ps4_save_unmount(void)
{
    int rc;

    if (g_data_mounted) {
        rc = sceSaveDataUmount(&g_data_mount_point);
        if (rc < 0) {
            return rc;
        }
        g_data_mounted = 0;
        memset(&g_data_mount_point, 0, sizeof(g_data_mount_point));
    }
    if (g_save_data_mounted) {
        rc = sceSaveDataUmount(&g_mount_point);
        if (rc < 0) {
            return rc;
        }
        g_save_data_mounted = 0;
        memset(&g_mount_point, 0, sizeof(g_mount_point));
    }
    if (g_save_data_initialized) {
        rc = sceSaveDataTerminate();
        if (rc < 0) {
            return rc;
        }
        g_save_data_initialized = 0;
    }
    return 0;
}
