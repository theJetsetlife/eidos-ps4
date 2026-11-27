#include "input_ps4.h"
#include <stdio.h>

int main(void)
{
    EidosPs4Input pad = {0};
    int x = -1, y = -1;
    pad.connected = pad.touch_down = 1;
    pad.touch_width = 1920;
    pad.touch_height = 942;
    const unsigned samples[][4] = {
        {0, 0, 0, 0}, {960, 471, 128, 96},
        {1919, 941, 255, 191}, {1920, 942, 255, 191},
        {65535, 65535, 255, 191}
    };
    for (unsigned i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i) {
        pad.touch_x = samples[i][0]; pad.touch_y = samples[i][1];
        if (!eidos_ps4_input_touch_position(&pad, &x, &y) ||
            x != (int)samples[i][2] || y != (int)samples[i][3]) return 1;
    }
    pad.connected = 0;
    if (eidos_ps4_input_touch_position(&pad, &x, &y)) return 2;
    pad.connected = 1; pad.touch_down = 0;
    if (eidos_ps4_input_touch_position(&pad, &x, &y)) return 3;
    pad.touch_down = 1; pad.touch_width = 0;
    if (eidos_ps4_input_touch_position(&pad, &x, &y)) return 4;
    pad.touch_width = 1920; pad.touch_height = 0;
    if (eidos_ps4_input_touch_position(&pad, &x, &y)) return 5;
    puts("test_ps4_touch: coordinates, bounds, release and disconnect passed");
    return 0;
}
