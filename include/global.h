#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

extern float glob_temperature;
extern float glob_humidity;

// Bao ve cac bien cam bien dung chung giua cac FreeRTOS task
extern portMUX_TYPE humidityMux;
extern portMUX_TYPE temperatureMux;

extern bool glob_sensor_error;
// Cho biet task cam bien da doc duoc nhiet do hop le hay chua
extern bool glob_temperature_valid;

extern String WIFI_SSID;
extern String WIFI_PASS;
extern String CORE_IOT_TOKEN;
extern String CORE_IOT_SERVER;
extern String CORE_IOT_PORT;

extern boolean isWifiConnected;
// Chi true khi Wi-Fi AP/STA da khoi tao va co IP hop le.
extern volatile bool isNetworkReady;
// Chan ket noi Wi-Fi moi dang duoc xu ly boi trang cai dat.
extern volatile bool wifiConfigInProgress;
extern SemaphoreHandle_t xBinarySemaphoreInternet;
#endif