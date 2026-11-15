#include "neo_blinky.h"
#include "global.h"

void task_neopixel(void *pvParameters)
{
    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.clear();
    strip.show();

    bool hasDisplayedNormalColor = false;
    uint32_t previousColor = 0;

    bool previousErrorState = false;
    bool errorLedState = false;
    TickType_t lastErrorToggle = xTaskGetTickCount();
    const TickType_t errorToggleInterval = pdMS_TO_TICKS(500);

    while (true)
    {
        float temperature;
        bool temperatureValid;
        bool sensorError;

        // Doc nhiet do va trang thai loi trong cung vung toi han
        portENTER_CRITICAL(&temperatureMux);
        temperature = glob_temperature;
        temperatureValid = glob_temperature_valid;
        sensorError = glob_sensor_error;
        portEXIT_CRITICAL(&temperatureMux);

        if (!temperatureValid)
        {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        if (sensorError)
        {
            // Khi vua vao trang thai loi, bat dau voi mau vang sang
            if (!previousErrorState)
            {
                errorLedState = true;
                lastErrorToggle = xTaskGetTickCount();
                previousErrorState = true;
                hasDisplayedNormalColor = false;
            }

            // Nhao nhay vang: moi 500 ms doi sang/tat
            TickType_t now = xTaskGetTickCount();
            if ((now - lastErrorToggle) >= errorToggleInterval)
            {
                errorLedState = !errorLedState;
                lastErrorToggle = now;
            }

            strip.setPixelColor(0, errorLedState ? strip.Color(40, 40, 0) : strip.Color(0, 0, 0));
            strip.show();
        }
        else
        {
            // Roi trang thai loi: tinh lai mau binh thuong theo nhiet do
            if (previousErrorState)
            {
                previousErrorState = false;
                hasDisplayedNormalColor = false;
            }

            uint32_t color;
            if (temperature < 20.0f)
            {
                color = strip.Color(0, 0, 35); // Xanh duong
            }
            else if (temperature < 28.0f)
            {
                color = strip.Color(0, 35, 0); // Xanh la
            }
            else
            {
                color = strip.Color(40, 0, 0); // Do
            }

            if (!hasDisplayedNormalColor || color != previousColor)
            {
                strip.setPixelColor(0, color);
                strip.show();

                Serial.printf("[NEOPIXEL] Temperature: %.2f C -> normal RGB\\n", temperature);

                previousColor = color;
                hasDisplayedNormalColor = true;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}
