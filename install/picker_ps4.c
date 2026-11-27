/* Select a user-provided dump from the root of an application-readable USB
 * volume. ROM verification and extraction remain in the setup library. */
#include "picker.h"
#include "install.h"

#include <stdbool.h>
#include <_kernel.h>
#include <common_dialog.h>
#include <libsysmodule.h>
#include <message_dialog.h>
#include <user_service.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    SceUserServiceUserId user;
    const char *root;
    char *out;
    size_t size;
    int status;
} UsbPicker;

static int pick_entry(void *ctx, const char *name, int kind)
{
    UsbPicker *p = ctx;
    const char *ext = strrchr(name, '.');
    char prompt[512];
    SceMsgDialogParam params;
    SceMsgDialogUserMessageParam message = {0};
    SceMsgDialogResult result = {0};
    int rc;

    if (kind != BW_PATH_FILE || ext == NULL || strlen(ext) != 4 ||
        (ext[1] != 'n' && ext[1] != 'N') || (ext[2] != 'd' && ext[2] != 'D') ||
        (ext[3] != 's' && ext[3] != 'S'))
        return 0;
    snprintf(prompt, sizeof(prompt), "Install this ROM?\n\n%s/%s\n\nChoose No to see the next file.",
             p->root, name);
    sceMsgDialogParamInitialize(&params);
    params.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
    params.userId = p->user;
    params.userMsgParam = &message;
    message.msg = prompt;
    message.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_YESNO;
    rc = sceMsgDialogInitialize();
    if (rc < 0) {
        snprintf(p->out, p->size, "sceMsgDialogInitialize failed (0x%08x).", (unsigned)rc);
        p->status = BW_PICK_UNAVAILABLE;
        return 1;
    }
    rc = sceMsgDialogOpen(&params);
    if (rc >= 0) {
        while (sceMsgDialogUpdateStatus() == SCE_COMMON_DIALOG_STATUS_RUNNING)
            sceKernelUsleep(16000);
        rc = sceMsgDialogGetResult(&result);
    }
    sceMsgDialogTerminate();
    if (rc < 0) {
        snprintf(p->out, p->size, "USB ROM selection failed (0x%08x).", (unsigned)rc);
        p->status = BW_PICK_UNAVAILABLE;
        return 1;
    }
    p->status = BW_PICK_CANCELLED;
    if (result.result != SCE_COMMON_DIALOG_RESULT_OK)
        return 1;
    if (result.buttonId != SCE_MSG_DIALOG_BUTTON_ID_YES)
        return 0;
    rc = snprintf(p->out, p->size, "%s/%s", p->root, name);
    if (rc >= 0 && (size_t)rc < p->size) {
        p->status = BW_PICK_OK;
    } else {
        snprintf(p->out, p->size, "USB ROM path is too long.");
        p->status = BW_PICK_UNAVAILABLE;
    }
    return 1;
}

int bw_pick_rom(void *parent, const char *title, char *out, size_t n)
{
    static const char *const roots[] = {"/mnt/usb0", "/mnt/usb1"};
    UsbPicker picker = {0};
    int loaded_here, rc;
    size_t i;
    (void)parent;
    (void)title;
    if (out == NULL || n == 0)
        return BW_PICK_UNAVAILABLE;
    picker.out = out;
    picker.size = n;
    picker.status = BW_PICK_UNAVAILABLE;
    snprintf(out, n, "No readable .nds file found. Place your ROM in the root of a USB drive.");
    rc = sceUserServiceGetInitialUser(&picker.user);
    if (rc < 0) goto failed;
    loaded_here = sceSysmoduleIsLoaded(SCE_SYSMODULE_MESSAGE_DIALOG) != 0;
    if (loaded_here && (rc = sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG)) < 0)
        goto failed;
    rc = sceCommonDialogInitialize();
    if (rc == SCE_COMMON_DIALOG_ERROR_ALREADY_INITIALIZED) rc = 0;
    if (rc >= 0) {
        for (i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
            picker.root = roots[i];
            if (bw_path_kind_utf8(picker.root) != BW_PATH_DIR)
                continue;
            rc = bw_list_dir_utf8(picker.root, pick_entry, &picker);
            if (rc != 0) break;
        }
    }
    if (loaded_here) sceSysmoduleUnloadModule(SCE_SYSMODULE_MESSAGE_DIALOG);
    if (rc >= 0) return picker.status;
failed:
    snprintf(out, n, "Could not open the USB ROM picker (0x%08x).", (unsigned)rc);
    return BW_PICK_UNAVAILABLE;
}
