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
    int count;      /* .nds files found, before any dialog */
} UsbPicker;

static int is_nds(const char *name, int kind)
{
    const char *ext = strrchr(name, '.');
    return kind == BW_PATH_FILE && ext != NULL && strlen(ext) == 4 &&
           (ext[1] == 'n' || ext[1] == 'N') && (ext[2] == 'd' || ext[2] == 'D') &&
           (ext[3] == 's' || ext[3] == 'S');
}

static int count_entry(void *ctx, const char *name, int kind)
{
    if (is_nds(name, kind)) ((UsbPicker *)ctx)->count++;
    return 0;
}

static int pick_entry(void *ctx, const char *name, int kind)
{
    UsbPicker *p = ctx;
    const char *ext = strrchr(name, '.');
    char prompt[512];
    SceMsgDialogParam params;
    SceMsgDialogUserMessageParam message = {0};
    SceMsgDialogResult result = {0};
    int rc;

    (void)ext;
    if (!is_nds(name, kind))
        return 0;
    snprintf(prompt, sizeof(prompt), "Install this ROM?\n\n%s\n\nX: Yes, install it\nO: No%s",
             name, p->count > 1 ? " (show the next file)" : " (quit)");
    sceMsgDialogParamInitialize(&params);
    params.mode = SCE_MSG_DIALOG_MODE_USER_MSG;
    params.userId = p->user;
    params.userMsgParam = &message;
    message.msg = prompt;
    /* one ROM: a single button, so nothing can land on No by mistake */
    message.buttonType = SCE_MSG_DIALOG_BUTTON_TYPE_OK;   /* X = OK = Yes, O = back = No */
    rc = sceMsgDialogInitialize();
    if (rc < 0) {
        snprintf(p->out, p->size, "sceMsgDialogInitialize failed (0x%08x).", (unsigned)rc);
        p->status = BW_PICK_UNAVAILABLE;
        return 1;
    }
    rc = sceMsgDialogOpen(&params);
    if (rc >= 0) {
        /* the dialog is another process: its status is NONE until it starts */
        int waited_ms = 0, seen = 0, last = -1;
        while (waited_ms < 300000) {
            int status = sceMsgDialogUpdateStatus();
            if (status != last) {
                fprintf(stderr, "PS4: ROM dialog status %d at %d ms\n", status, waited_ms);
                last = status;
            }
            if (status == SCE_COMMON_DIALOG_STATUS_FINISHED) break;
            if (status == SCE_COMMON_DIALOG_STATUS_INITIALIZED ||
                status == SCE_COMMON_DIALOG_STATUS_RUNNING) seen = 1;
            else if (seen || waited_ms >= 10000) break;
            sceKernelUsleep(16000);
            waited_ms += 16;
        }
        rc = sceMsgDialogGetResult(&result);
        fprintf(stderr, "PS4: ROM dialog result rc=0x%08x mode=%d result=%d buttonId=%d\n",
                (unsigned)rc, (int)result.mode, (int)result.result, (int)result.buttonId);
    } else {
        fprintf(stderr, "PS4: sceMsgDialogOpen for the ROM failed (0x%08x)\n", (unsigned)rc);
    }
    sceMsgDialogTerminate();
    if (rc < 0) {
        snprintf(p->out, p->size, "USB ROM selection failed (0x%08x).", (unsigned)rc);
        p->status = BW_PICK_UNAVAILABLE;
        return 1;
    }
    p->status = BW_PICK_CANCELLED;
    /* X answers with the OK button; O closes the dialog with no button,
     * which is No: the next file, and after the last one the picker ends
     * with nothing chosen (BW_PICK_CANCELLED) */
    if (result.buttonId != SCE_MSG_DIALOG_BUTTON_ID_OK)
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
    /* every USB slot the console has, then a folder on internal storage that
 * can be filled over FTP (the console has no USB drive in some setups) */
    static const char *const roots[] = {
        "/mnt/usb0", "/mnt/usb1", "/mnt/usb2", "/mnt/usb3",
        "/mnt/usb4", "/mnt/usb5", "/mnt/usb6", "/mnt/usb7",
        "/data/eidos/rom"};
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
    snprintf(out, n, "No Pokemon .nds file found. Copy your ROM to /data/eidos/rom/ on the console with a file transfer (FTP) client. A USB drive is searched too, but applications often cannot see one.");
    (void)bw_mkdirs_utf8("/data/eidos/rom");    /* a place to put the ROM over FTP */
    rc = sceUserServiceGetInitialUser(&picker.user);
    if (rc < 0) goto failed;
    loaded_here = sceSysmoduleIsLoaded(SCE_SYSMODULE_MESSAGE_DIALOG) != 0;
    if (loaded_here && (rc = sceSysmoduleLoadModule(SCE_SYSMODULE_MESSAGE_DIALOG)) < 0)
        goto failed;
    rc = sceCommonDialogInitialize();
    if (rc == SCE_COMMON_DIALOG_ERROR_ALREADY_INITIALIZED) rc = 0;
    if (rc >= 0) {
        for (i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
            if (bw_path_kind_utf8(roots[i]) == BW_PATH_DIR)
                (void)bw_list_dir_utf8(roots[i], count_entry, &picker);
        }
        fprintf(stderr, "PS4: %d ROM file(s) found\n", picker.count);
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
