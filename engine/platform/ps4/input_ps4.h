#ifndef EIDOS_PLATFORM_PS4_INPUT_H
#define EIDOS_PLATFORM_PS4_INPUT_H

#include <stdint.h>
#include <user_service.h>

#include "platform.h"
#include "presentation_ps4.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct EidosPs4Input {
    int32_t handle;
    SceUserServiceUserId user_id;
    uint32_t buttons;
    uint16_t touch_x;
    uint16_t touch_y;
    uint16_t touch_width;
    uint16_t touch_height;
    uint16_t cursor_x;
    uint16_t cursor_y;
    uint8_t left_x;
    uint8_t left_y;
    uint8_t right_x;
    uint8_t right_y;
    uint8_t left_trigger;
    uint8_t right_trigger;
    uint8_t initialized;
    uint8_t connected;
    uint8_t touch_available;
    uint8_t touch_down;
    uint8_t cursor_touch;
    uint8_t cursor_visible_frames;
    uint8_t retry_frames;
    uint8_t stick_dead_zone;
    uint8_t presentation_mode;
    uint8_t presentation_button_prev;
} EidosPs4Input;

/* Present mode changes only on the touchpad button's rising edge. */
static inline void eidos_ps4_input_presentation_button(EidosPs4Input *input,
                                                       uint8_t pressed)
{
    if (pressed && !input->presentation_button_prev) {
        input->presentation_mode =
            (uint8_t)((input->presentation_mode + 1u) %
                      (EIDOS_PS4_PRESENT_BOTH + 1u));
    }
    input->presentation_button_prev = pressed;
}

/* One coordinate conversion for gameplay and the visible stylus marker.
 * Pad resolution is a range size; clamp out-of-range device samples. */
static inline int eidos_ps4_input_touch_position(const EidosPs4Input *input,
                                                int *x, int *y)
{
    if (!input->connected || !input->touch_down ||
        input->touch_width == 0 || input->touch_height == 0) {
        return 0;
    }
    *x = (int)((uint32_t)input->touch_x * 256u / input->touch_width);
    *y = (int)((uint32_t)input->touch_y * 192u / input->touch_height);
    if (*x > 255) *x = 255;
    if (*y > 191) *y = 191;
    return 1;
}

/* The caller owns UserService and supplies the user who launched the app. */
int eidos_ps4_input_open(EidosPs4Input *input, SceUserServiceUserId user_id);
/* Distance from the neutral 128 stick position needed to synthesize a DS
 * keypad direction. Values are clamped to [1, 127]. The default is 48. */
void eidos_ps4_input_set_dead_zone(EidosPs4Input *input, uint8_t dead_zone);
int eidos_ps4_input_poll(EidosPs4Input *input, uint32_t *buttons);
void eidos_ps4_input_close(EidosPs4Input *input);

#ifdef __cplusplus
}
#endif

#endif
