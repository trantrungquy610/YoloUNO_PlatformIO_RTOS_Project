#include <Arduino.h>
#include "task_fan.h"

#define FAN_PIN 6

void task_fan_control(void *pvParameters)
{
    pinMode(FAN_PIN, OUTPUT);

    Serial.println("[FAN] Fan test task started!");

    while (true)
    {
        Serial.println("[FAN] OFF");
        analogWrite(FAN_PIN, 0);
        vTaskDelay(pdMS_TO_TICKS(3000));

        Serial.println("[FAN] 30%");
        analogWrite(FAN_PIN, 77);
        vTaskDelay(pdMS_TO_TICKS(3000));

        Serial.println("[FAN] 60%");
        analogWrite(FAN_PIN, 153);
        vTaskDelay(pdMS_TO_TICKS(3000));

        Serial.println("[FAN] 100%");
        analogWrite(FAN_PIN, 255);
        vTaskDelay(pdMS_TO_TICKS(3000));
    }
}