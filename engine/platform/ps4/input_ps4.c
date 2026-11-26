#include "input_ps4.h"

#include <pad.h>
#include <string.h>

#define EIDOS_PS4_PAD_RETRY_FRAMES 60u
#define EIDOS_PS4_DEFAULT_STICK_DEAD_ZONE 48u
#define EIDOS_PS4_CURSOR_VISIBLE_FRAMES 120u

static uint32_t map_buttons(uint32_t source)
{
    uint32_t result = 0;

    if (source & SCE_PAD_BUTTON_CROSS)    result |= EIDOS_PAD_A;
    if (source & SCE_PAD_BUTTON_CIRCLE)   result |= EIDOS_PAD_B;
    /* L3 is application-visible and gives Select a reliable mapping without
     * depending on the reserved Share/Create or PS system buttons. */
    if (source & SCE_PAD_BUTTON_L3)       result |= EIDOS_PAD_SELECT;
    if (source & SCE_PAD_BUTTON_OPTIONS)  result |= EIDOS_PAD_START;
    if (source & SCE_PAD_BUTTON_UP)       result |= EIDOS_PAD_UP;
    if (source & SCE_PAD_BUTTON_RIGHT)    result |= EIDOS_PAD_RIGHT;
    if (source & SCE_PAD_BUTTON_DOWN)     result |= EIDOS_PAD_DOWN;
    if (source & SCE_PAD_BUTTON_LEFT)     result |= EIDOS_PAD_LEFT;
    if (source & SCE_PAD_BUTTON_L1)       result |= EIDOS_PAD_L;
    if (source & SCE_PAD_BUTTON_R1)       result |= EIDOS_PAD_R;
    if (source & SCE_PAD_BUTTON_TRIANGLE) result |= EIDOS_PAD_X;
    if (source & SCE_PAD_BUTTON_SQUARE)   result |= EIDOS_PAD_Y;
    return result;
}

static uint32_t map_stick_directions(uint8_t x, uint8_t y, uint8_t dead_zone)
{
    uint32_t result = 0;
    const int x_delta = (int)x - 128;
    const int y_delta = (int)y - 128;
    if (x_delta <= -(int)dead_zone) result |= EIDOS_PAD_LEFT;
    if (x_delta >=  (int)dead_zone) result |= EIDOS_PAD_RIGHT;
    if (y_delta <= -(int)dead_zone) result |= EIDOS_PAD_UP;
    if (y_delta >=  (int)dead_zone) result |= EIDOS_PAD_DOWN;
    return result;
}

static uint16_t cursor_step(uint16_t position, int delta, uint16_t maximum)
{
    int next = (int)position + delta / 24;
    if (next < 0) next = 0;
    if (next > (int)maximum) next = maximum;
    return (uint16_t)next;
}

static int open_pad(EidosPs4Input *input)
{
    input->handle = scePadOpen(input->user_id, SCE_PAD_PORT_TYPE_STANDARD, 0, 0);
    if (input->handle < 0) {
        return input->handle;
    }

    {
        ScePadControllerInformation info;
        memset(&info, 0, sizeof(info));
        if (scePadGetControllerInformation(input->handle, &info) >= 0 &&
            info.touchPadInfo.resolution.x != 0 &&
            info.touchPadInfo.resolution.y != 0) {
            input->touch_width = info.touchPadInfo.resolution.x;
            input->touch_height = info.touchPadInfo.resolution.y;
            input->touch_available = 1;
        }
    }
    return 0;
}

int eidos_ps4_input_open(EidosPs4Input *input, SceUserServiceUserId user_id)
{
    int result;

    if (input == 0 || user_id < 0 || user_id == SCE_USER_SERVICE_USER_ID_SYSTEM ||
        user_id == SCE_USER_SERVICE_USER_ID_EVERYONE) {
        return -1;
    }

    memset(input, 0, sizeof(*input));
    input->handle = -1;
    input->user_id = user_id;
    input->stick_dead_zone = EIDOS_PS4_DEFAULT_STICK_DEAD_ZONE;
    input->cursor_x = 128u;
    input->cursor_y = 96u;

    result = scePadInit();
    if (result < 0) {
        return result;
    }

    input->initialized = 1;
    result = open_pad(input);
    if (result < 0) {
        /* Keep libpad initialized so poll() can retry this user-owned port;
         * a controller can be absent when the application starts. */
        input->retry_frames = EIDOS_PS4_PAD_RETRY_FRAMES;
        return result;
    }
    return 0;
}

void eidos_ps4_input_set_dead_zone(EidosPs4Input *input, uint8_t dead_zone)
{
    if (input == 0) {
        return;
    }
    if (dead_zone == 0u) {
        dead_zone = 1u;
    }
    input->stick_dead_zone = dead_zone > 127u ? 127u : dead_zone;
}

int eidos_ps4_input_poll(EidosPs4Input *input, uint32_t *buttons)
{
    ScePadData data;
    int result;

    if (input == 0 || buttons == 0 || !input->initialized) {
        return -1;
    }

    /* A disconnected controller must not leave a button held in game state.
     * Keep the user-service/pad library initialized and retry the user-owned
     * port once per second at the DS presentation rate. */
    if (input->handle < 0) {
        input->buttons = 0;
        input->connected = 0;
        input->left_x = input->left_y = 0;
        input->right_x = input->right_y = 0;
        input->left_trigger = input->right_trigger = 0;
        input->touch_down = 0;
        input->cursor_touch = 0;
        input->cursor_visible_frames = 0;
        *buttons = 0;
        if (input->retry_frames > 0) {
            --input->retry_frames;
            return 0;
        }
        if (open_pad(input) < 0) {
            input->retry_frames = EIDOS_PS4_PAD_RETRY_FRAMES;
            return 0;
        }
    }

    memset(&data, 0, sizeof(data));
    result = scePadReadState(input->handle, &data);
    if (result < 0) {
        (void)scePadClose(input->handle);
        input->handle = -1;
        input->connected = 0;
        input->buttons = 0;
        input->left_x = input->left_y = 0;
        input->right_x = input->right_y = 0;
        input->left_trigger = input->right_trigger = 0;
        input->touch_down = 0;
        input->cursor_touch = 0;
        input->cursor_visible_frames = 0;
        input->retry_frames = EIDOS_PS4_PAD_RETRY_FRAMES;
        *buttons = 0;
        return 0;
    }

    input->connected = data.connected ? 1 : 0;
    input->buttons = input->connected ? map_buttons(data.buttons) : 0;
    if (input->connected) {
        input->left_x = data.leftStick.x;
        input->left_y = data.leftStick.y;
        input->right_x = data.rightStick.x;
        input->right_y = data.rightStick.y;
        input->left_trigger = data.analogButtons.l2;
        input->right_trigger = data.analogButtons.r2;
        input->cursor_touch = (data.buttons & SCE_PAD_BUTTON_R3) != 0;
        if (input->cursor_visible_frames > 0) {
            --input->cursor_visible_frames;
        }
        {
            int x_delta = (int)input->right_x - 128;
            int y_delta = (int)input->right_y - 128;
            if (x_delta > -(int)input->stick_dead_zone &&
                x_delta < (int)input->stick_dead_zone) x_delta = 0;
            if (y_delta > -(int)input->stick_dead_zone &&
                y_delta < (int)input->stick_dead_zone) y_delta = 0;
            input->cursor_x = cursor_step(input->cursor_x, x_delta, 255u);
            input->cursor_y = cursor_step(input->cursor_y, y_delta, 191u);
            if (x_delta != 0 || y_delta != 0 || input->cursor_touch) {
                input->cursor_visible_frames = EIDOS_PS4_CURSOR_VISIBLE_FRAMES;
            }
        }
        input->buttons |= map_stick_directions(
            input->left_x, input->left_y, input->stick_dead_zone);
        if (input->left_trigger >= input->stick_dead_zone) {
            input->buttons |= EIDOS_PAD_L;
        }
        if (input->right_trigger >= input->stick_dead_zone) {
            input->buttons |= EIDOS_PAD_R;
        }
        input->touch_down = input->touch_available && data.touchData.touchNum != 0;
        if (input->touch_down) {
            input->touch_x = data.touchData.touch[0].x;
            input->touch_y = data.touchData.touch[0].y;
        }
    } else {
        (void)scePadClose(input->handle);
        input->handle = -1;
        input->left_x = input->left_y = 0;
        input->right_x = input->right_y = 0;
        input->left_trigger = input->right_trigger = 0;
        input->touch_down = 0;
        input->cursor_touch = 0;
        input->cursor_visible_frames = 0;
        input->retry_frames = EIDOS_PS4_PAD_RETRY_FRAMES;
    }
    *buttons = input->buttons;
    return 0;
}

void eidos_ps4_input_close(EidosPs4Input *input)
{
    if (input == 0) {
        return;
    }
    if (input->initialized && input->handle >= 0) {
        (void)scePadClose(input->handle);
    }
    memset(input, 0, sizeof(*input));
    input->handle = -1;
}
