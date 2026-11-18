#include "global.h"

#include "led_blinky.h"
#include "neo_blinky.h"
#include "temp_humi_monitor.h"
#include "mainserver.h"
#include "task_mqtt_gateway.h"

// include task
#include "task_check_info.h"
#include "task_toogle_boot.h"
#include "task_wifi.h"
#include "task_webserver.h"

void setup()
{
  Serial.begin(115200);

  xBinarySemaphoreInternet = xSemaphoreCreateBinary();
  if (xBinarySemaphoreInternet == nullptr)
  {
    Serial.println("ERROR: Cannot create Internet semaphore!");
  }

  check_info_File(0);

  xTaskCreate(led_blinky, "Task LED Blink", 2048, NULL, 2, NULL);
  xTaskCreate(task_neopixel, "Task NEO Pixel", 4096, NULL, 2, NULL);
  xTaskCreate(temp_humi_monitor, "Task TEMP HUMI Monitor", 4096, NULL, 2, NULL);
  xTaskCreate(main_server_task, "Task Main Server", 8192, NULL, 2, NULL);
  xTaskCreate(Task_Toogle_BOOT, "Task_Toogle_BOOT", 4096, NULL, 2, NULL);
  xTaskCreate(mqtt_gateway_task, "MQTT Gateway Task", 6144, NULL, 2, NULL);
}

void loop()
{
  if (check_info_File(1))
  {
    // Wi-Fi reconnect khong duoc dung/tat AsyncWebServer.
    // Ca hai WebServer van phuc vu tren cong 80 va 8080, ke ca khi dang o AP mode.
    if (Wifi_reconnect())
    {
      // CORE_IOT_reconnect();
    }
  }
  Webserver_reconnect();
  delay(10); // yield CPU cho cac FreeRTOS task va xu ly OTA/WebSocket
}