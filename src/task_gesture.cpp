
#include <Arduino.h>

#include "task_gesture.h"
#include "gesture_inference.h"
#include "task_rgb.h"
#include <cstring>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static QueueHandle_t gestureFrameQueue = nullptr;

// Dùng static để không chiếm 4096 byte stack của AI Task.
static GestureFrame gestureFrame;
static portMUX_TYPE gestureTelemetryMux = portMUX_INITIALIZER_UNLOCKED;
static GestureTelemetry latestGestureTelemetry = {"unknown", 0.0f, 0, 0};
static bool previousPredictionWasActionGesture = false;
static char previousActionGesture[24] = "";

void gesture_get_telemetry(GestureTelemetry &telemetry)
{
    portENTER_CRITICAL(&gestureTelemetryMux);
    telemetry = latestGestureTelemetry;
    portEXIT_CRITICAL(&gestureTelemetryMux);
}

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

                // Confidence threshold ban đầu
                const float CONFIDENCE_THRESHOLD = 0.35f;

                RgbGestureCommand command = RGB_CMD_NO_HAND;

                bool validGesture =
                    prediction.confidence >= CONFIDENCE_THRESHOLD;

                const char *reportedGesture =
                    validGesture ? prediction.label : "unknown";

                // Count a gesture when entering a confident action class.
                // no_hand/unknown rearms the counter for the next gesture.
                if (!validGesture || prediction.class_index == 1)
                {
                    previousPredictionWasActionGesture = false;
                    previousActionGesture[0] = '\0';
                }
                else if (!previousPredictionWasActionGesture ||
                         strcmp(previousActionGesture, prediction.label) != 0)
                {
                    portENTER_CRITICAL(&gestureTelemetryMux);
                    latestGestureTelemetry.gesture_count++;
                    portEXIT_CRITICAL(&gestureTelemetryMux);
                    snprintf(previousActionGesture, sizeof(previousActionGesture),
                             "%s", prediction.label);
                    previousPredictionWasActionGesture = true;
                }

                portENTER_CRITICAL(&gestureTelemetryMux);
                snprintf(latestGestureTelemetry.gesture,
                         sizeof(latestGestureTelemetry.gesture), "%s",
                         reportedGesture);
                latestGestureTelemetry.confidence = prediction.confidence;
                latestGestureTelemetry.inference_ms = prediction.inference_ms;
                portEXIT_CRITICAL(&gestureTelemetryMux);

                if (validGesture)
                {
                    switch (prediction.class_index)
                    {
                    case 0:
                        command = RGB_CMD_FIST;
                        break;

                    case 1:
                        command = RGB_CMD_NO_HAND;
                        break;

                    case 2:
                        command = RGB_CMD_OPEN_PALM;
                        break;

                    case 3:
                        command = RGB_CMD_THUMBS_UP;
                        break;

                    case 4:
                        command = RGB_CMD_VICTORY;
                        break;

                    default:
                        command = RGB_CMD_NO_HAND;
                        break;
                    }
                }

                // Gửi lệnh cho RGB task
                rgb_submit_gesture(command);
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
