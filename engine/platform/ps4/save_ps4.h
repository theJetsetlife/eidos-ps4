#ifndef EIDOS_PLATFORM_PS4_SAVE_H
#define EIDOS_PLATFORM_PS4_SAVE_H

#include <stddef.h>
#include <user_service.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Mount this application's per-user SaveData directory and return the
 * SDK-provided mount path. The caller supplies a buffer of at least 16 bytes.
 */
int eidos_ps4_save_mount(SceUserServiceUserId user_id, char *path,
                         size_t path_size);

/* Mount the application's user-imported ROM and extracted game-data area.
 * Its quota is the SDK's documented maximum SaveData allocation (1 GiB). */
int eidos_ps4_data_mount(SceUserServiceUserId user_id, char *path,
                         size_t path_size);

/* Unmount and terminate SaveData. A failure leaves ownership intact for retry.
 */
int eidos_ps4_save_unmount(void);

#ifdef __cplusplus
}
#endif

#endif
