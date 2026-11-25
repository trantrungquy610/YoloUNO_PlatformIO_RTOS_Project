#include "global.h"
float glob_temperature = 0;
float glob_humidity = 0;

portMUX_TYPE humidityMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE temperatureMux = portMUX_INITIALIZER_UNLOCKED;
bool glob_temperature_valid = false;
bool glob_sensor_error = false;

String WIFI_SSID;
String WIFI_PASS;
String CORE_IOT_TOKEN;
String CORE_IOT_SERVER;
String CORE_IOT_PORT;

boolean isWifiConnected = false;
volatile bool isNetworkReady = false;
volatile bool wifiConfigInProgress = false;
SemaphoreHandle_t xBinarySemaphoreInternet = nullptr;
