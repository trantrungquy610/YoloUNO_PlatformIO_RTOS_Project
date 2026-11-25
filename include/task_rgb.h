
#pragma once

#include <stdint.h>

enum RgbGestureCommand : uint8_t
{
    RGB_CMD_NO_HAND = 0,
    RGB_CMD_OPEN_PALM,
    RGB_CMD_FIST,
    RGB_CMD_THUMBS_UP,
    RGB_CMD_VICTORY
};

bool task_rgb_init();

bool rgb_submit_gesture(RgbGestureCommand command);

void task_rgb(void *pvParameters);
