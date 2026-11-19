
#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// Module RGB kết nối cổng D9-D10
#define RGB_PIN 8
#define NUM_RGB 4

Adafruit_NeoPixel rgb(
    NUM_RGB,
    RGB_PIN,
    NEO_GRB + NEO_KHZ800);

void task_rgb(void *pvParameters)
{
    rgb.begin();
    rgb.setBrightness(50); // Giảm độ sáng
    rgb.clear();
    rgb.show();

    while (true)
    {
        // Đỏ
        rgb.fill(rgb.Color(50, 0, 0));
        rgb.show();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Xanh lá
        rgb.fill(rgb.Color(0, 50, 0));
        rgb.show();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Xanh dương
        rgb.fill(rgb.Color(0, 0, 50));
        rgb.show();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Vàng
        rgb.fill(rgb.Color(50, 50, 0));
        rgb.show();
        vTaskDelay(pdMS_TO_TICKS(1000));

        // Tắt LED
        rgb.clear();
        rgb.show();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
