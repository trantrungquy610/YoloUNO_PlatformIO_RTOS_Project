
#include <Arduino.h>

#include "task_gesture.h"
#include "gesture_inference.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static QueueHandle_t gestureFrameQueue = nullptr;

// Dùng static để không chiếm 4096 byte stack của AI Task.
static GestureFrame gestureFrame;

// =====================================================
// Initialize Gesture Queue
// =====================================================
bool gesture_task_init()
{
    if (gestureFrameQueue != nullptr)
    {
        return true;
    }

    gestureFrameQueue = xQueueCreate(
        1,
        sizeof(GestureFrame));

    if (gestureFrameQueue == nullptr)
    {
        Serial.println("[GESTURE] Cannot create frame queue");
        return false;
    }

    Serial.println("[GESTURE] Frame queue created");
    return true;
}

// =====================================================
// Gesture AI Task
// =====================================================
void gesture_ai_task(void *pvParameters)
{
    (void)pvParameters;

    Serial.println("[GESTURE] AI task started");

    if (!gesture_ai_init())
    {
        Serial.println("[GESTURE] TFLM initialization FAILED");
        vTaskDelete(nullptr);
        return;
    }

    Serial.println("[GESTURE] TFLM ready. Waiting for frames...");

    GesturePrediction prediction;

    while (true)
    {
        if (xQueueReceive(
                gestureFrameQueue,
                &gestureFrame,
                portMAX_DELAY) == pdTRUE)
        {
            if (gesture_ai_predict(
                    gestureFrame.pixels,
                    sizeof(gestureFrame.pixels),
                    prediction))
            {
                Serial.printf(
                    "[GESTURE] Result: %s | confidence: %.3f\n",
                    prediction.label,
                    prediction.confidence);
            }
            else
            {
                Serial.println("[GESTURE] Prediction failed");
            }
        }
    }
}

// =====================================================
// Submit Gesture Frame
// =====================================================
bool gesture_submit_frame(
    const uint8_t *pixels,
    size_t length)
{
    if (gestureFrameQueue == nullptr ||
        pixels == nullptr ||
        length != GESTURE_FRAME_BYTES)
    {
        return false;
    }

    // Chỉ giữ frame mới nhất.
    return xQueueOverwrite(
               gestureFrameQueue,
               pixels) == pdPASS;
}
