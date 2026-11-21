
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#include "task_rgb.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// RGB module
#define RGB_PIN 8
#define NUM_RGB 4

// Tắt LED nếu không nhận diện được cử chỉ trong 3 giây
#define NO_HAND_TIMEOUT_MS 3000

static Adafruit_NeoPixel rgb(
    NUM_RGB,
    RGB_PIN,
    NEO_GRB + NEO_KHZ800);

static QueueHandle_t rgbCommandQueue = nullptr;

// =====================================================
// Initialize RGB command queue
// =====================================================

bool task_rgb_init()
{
    if (rgbCommandQueue != nullptr)
        return true;

    rgbCommandQueue = xQueueCreate(
        1,
        sizeof(RgbGestureCommand));

    if (rgbCommandQueue == nullptr)
    {
        Serial.println("[RGB] Cannot create command queue");
        return false;
    }

    Serial.println("[RGB] Command queue created");
    return true;
}

// =====================================================
// Submit command from Gesture AI
// =====================================================

bool rgb_submit_gesture(RgbGestureCommand command)
{
    if (rgbCommandQueue == nullptr)
        return false;

    return xQueueOverwrite(
               rgbCommandQueue,
               &command) == pdPASS;
}

// =====================================================
// RGB Task
// =====================================================

void task_rgb(void *pvParameters)
{
    (void)pvParameters;

    rgb.begin();
    rgb.setBrightness(50);
    rgb.clear();
    rgb.show();

    Serial.println("[RGB] Gesture control ready");

    RgbGestureCommand command;

    bool noHandTiming = false;
    bool ledIsOff = true;

    uint32_t noHandStart = 0;

    while (true)
    {
        if (xQueueReceive(
                rgbCommandQueue,
                &command,
                pdMS_TO_TICKS(100)) == pdTRUE)
        {
            switch (command)
            {
            case RGB_CMD_OPEN_PALM:
                rgb.fill(rgb.Color(0, 50, 0));
                rgb.show();

                Serial.println("[RGB] OPEN_PALM -> GREEN");

                noHandTiming = false;
                ledIsOff = false;
                break;

            case RGB_CMD_FIST:
                rgb.fill(rgb.Color(50, 0, 0));
                rgb.show();

                Serial.println("[RGB] FIST -> RED");

                noHandTiming = false;
                ledIsOff = false;
                break;

            case RGB_CMD_THUMBS_UP:
                rgb.fill(rgb.Color(0, 0, 50));
                rgb.show();

                Serial.println("[RGB] THUMBS_UP -> BLUE");

                noHandTiming = false;
                ledIsOff = false;
                break;

            case RGB_CMD_VICTORY:
                rgb.fill(rgb.Color(50, 50, 0));
                rgb.show();

                Serial.println("[RGB] VICTORY -> YELLOW");

                noHandTiming = false;
                ledIsOff = false;
                break;

            case RGB_CMD_NO_HAND:
                if (!noHandTiming)
                {
                    noHandStart = millis();
                    noHandTiming = true;

                    Serial.println("[RGB] No hand detected, waiting...");
                }
                break;
            }
        }

        // Turn off RGB if no hand persists for 3 seconds
        if (noHandTiming &&
            !ledIsOff &&
            millis() - noHandStart >= NO_HAND_TIMEOUT_MS)
        {
            rgb.clear();
            rgb.show();

            Serial.println("[RGB] No hand timeout -> OFF");

            ledIsOff = true;
        }
    }
}
