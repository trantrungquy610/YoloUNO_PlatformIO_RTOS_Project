
#pragma once

#include <Arduino.h>

#define GESTURE_INPUT_WIDTH 64
#define GESTURE_INPUT_HEIGHT 64
#define GESTURE_INPUT_BYTES (64 * 64)

struct GesturePrediction
{
    uint8_t class_index;
    float confidence;
    const char *label;
};

bool gesture_ai_init();

bool gesture_ai_predict(
    const uint8_t *pixels,
    size_t length,
    GesturePrediction &result);
