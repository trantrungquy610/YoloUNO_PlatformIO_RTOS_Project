#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "task_fan.h"

#define FAN_PIN 6

static QueueHandle_t fanCommandQueue = nullptr;
static volatile uint8_t fanSpeedPercent = 0;

bool fan_control_init()
{
    if (fanCommandQueue != nullptr)
        return true;

    fanCommandQueue = xQueueCreate(1, sizeof(uint8_t));
    if (fanCommandQueue == nullptr)
    {
        Serial.println("[FAN] Cannot create command queue");
        return false;
    }

    const uint8_t initialSpeed = 0;
    xQueueOverwrite(fanCommandQueue, &initialSpeed);
    return true;
}

bool fan_set_speed_percent(uint8_t percent)
{
    if (fanCommandQueue == nullptr)
        return false;

    if (percent > 100)
        percent = 100;

    if (xQueueOverwrite(fanCommandQueue, &percent) != pdPASS)
        return false;

    fanSpeedPercent = percent;
    return true;
}

uint8_t fan_get_speed_percent()
{
    return fanSpeedPercent;
}

void task_fan_control(void *pvParameters)
{
    (void)pvParameters;
    pinMode(FAN_PIN, OUTPUT);
    analogWrite(FAN_PIN, 0);

    Serial.println("[FAN] Remote fan control ready (initially OFF)");

    while (true)
    {
        uint8_t requestedPercent = 0;
        if (xQueueReceive(fanCommandQueue, &requestedPercent, portMAX_DELAY) == pdPASS)
        {
            const uint8_t pwmValue = (requestedPercent * 255U + 50U) / 100U;
            analogWrite(FAN_PIN, pwmValue);
            Serial.printf("[FAN] Speed %u%% (PWM %u/255)\n",
                          requestedPercent, pwmValue);
        }
    }
}
