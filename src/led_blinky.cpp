#include "led_blinky.h"
#include "global.h"

void led_blinky(void *pvParameters)
{
  pinMode(LED_GPIO, OUTPUT);

  bool ledState = false;
  bool previousErrorState = false;
  TickType_t lastToggle = xTaskGetTickCount();

  digitalWrite(LED_GPIO, LOW);

  while (1)
  {
    bool sensorError;
    float humidity;

    // Doc trang thai loi
    portENTER_CRITICAL(&temperatureMux);
    sensorError = glob_sensor_error;
    portEXIT_CRITICAL(&temperatureMux);

    if (sensorError)
    {
      // Loi cam bien: LED sang lien tuc
      ledState = true;
      digitalWrite(LED_GPIO, HIGH);
      previousErrorState = true;
      vTaskDelay(pdMS_TO_TICKS(50));
      continue;
    }

    // Khi het loi, bat dau lai chu ky nhap nhay tu trang thai tat
    if (previousErrorState)
    {
      ledState = false;
      digitalWrite(LED_GPIO, LOW);
      previousErrorState = false;
      lastToggle = xTaskGetTickCount();
    }

    // Doc do am
    portENTER_CRITICAL(&humidityMux);
    humidity = glob_humidity;
    portEXIT_CRITICAL(&humidityMux);

    uint32_t blinkPeriodMs;
    if (humidity < 40.0f)
    {
      blinkPeriodMs = 2000;
    }
    else if (humidity <= 60.0f)
    {
      blinkPeriodMs = 1000;
    }
    else
    {
      blinkPeriodMs = 500;
    }

    // Moi nua chu ky thi dao trang thai LED
    TickType_t toggleInterval = pdMS_TO_TICKS(blinkPeriodMs / 2);
    TickType_t now = xTaskGetTickCount();
    if ((now - lastToggle) >= toggleInterval)
    {
      ledState = !ledState;
      digitalWrite(LED_GPIO, ledState ? HIGH : LOW);
      lastToggle = now;
    }

    // Kiem tra trang thai loi nhanh, doc lai do am o vong tiep theo
    vTaskDelay(pdMS_TO_TICKS(20));
  }
}
