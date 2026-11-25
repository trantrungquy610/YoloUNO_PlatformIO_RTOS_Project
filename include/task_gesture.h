
#pragma once

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

#define GESTURE_FRAME_BYTES (64 * 64)

struct GestureFrame
{
    uint8_t pixels[GESTURE_FRAME_BYTES];
};

struct GestureTelemetry
{
    char gesture[24];
    float confidence;
    uint32_t inference_ms;
    uint32_t gesture_count;
};

// Khởi tạo Queue trước khi tạo AI Task.
bool gesture_task_init();

// Task được tạo trực tiếp từ main.cpp.
void gesture_ai_task(void *pvParameters);

// Gửi ảnh grayscale 64x64 vào Queue.
bool gesture_submit_frame(const uint8_t *pixels, size_t length);

// Snapshot of the latest prediction for the MQTT telemetry task.
void gesture_get_telemetry(GestureTelemetry &telemetry);
