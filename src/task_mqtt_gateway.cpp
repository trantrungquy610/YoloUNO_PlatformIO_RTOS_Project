
#include "task_mqtt_gateway.h"

#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#include "global.h"

// ========================================
// MQTT GATEWAY CONFIGURATION
// ========================================

// Thay bằng IPv4 của máy chạy Docker Gateway
const char *MQTT_SERVER = "192.168.101.124";

constexpr uint16_t MQTT_PORT = 1883;

// Tên thiết bị trên CoreIoT
const char *DEVICE_ID = "esp32_01";

// MQTT topics
String telemetryTopic =
    "yolouno/" + String(DEVICE_ID) + "/telemetry";

String statusTopic =
    "yolouno/" + String(DEVICE_ID) + "/status";

String commandTopic =
    "yolouno/" + String(DEVICE_ID) + "/command";

// ========================================
// MQTT CLIENT
// ========================================

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ========================================
// HANDLE COMMAND FROM GATEWAY
// ========================================

void mqttCallback(char *topic, byte *payload,
                  unsigned int length)
{
    Serial.print("[MQTT] Received topic: ");
    Serial.println(topic);

    char message[256];

    if (length >= sizeof(message))
    {
        Serial.println("[MQTT] Message too large!");
        return;
    }

    memcpy(message, payload, length);
    message[length] = '\0';

    Serial.print("[MQTT] Payload: ");
    Serial.println(message);

    StaticJsonDocument<256> doc;

    DeserializationError error =
        deserializeJson(doc, message);

    if (error)
    {
        Serial.println("[MQTT] JSON parse failed!");
        return;
    }

    const char *method = doc["method"] | "";

    if (strcmp(method, "setLedSwitchValue") == 0)
    {
        bool state = doc["params"] | false;

        Serial.print("[MQTT] LED command: ");
        Serial.println(state ? "ON" : "OFF");

        // TODO:
        // Kết nối với logic điều khiển LED
        // của firmware sau khi xác định GPIO LED.
    }
    else
    {
        Serial.print("[MQTT] Unknown method: ");
        Serial.println(method);
    }
}

// ========================================
// MQTT CONNECTION
// ========================================

bool mqttConnect()
{
    if (mqttClient.connected())
        return true;

    if (WiFi.status() != WL_CONNECTED)
        return false;

    String clientId = "YOLO-UNO-";
    clientId += WiFi.macAddress();

    Serial.println("[MQTT] Connecting to Gateway...");

    // Mosquitto hiện tại cho phép anonymous
    if (mqttClient.connect(clientId.c_str()))
    {
        Serial.println("[MQTT] Connected!");

        mqttClient.subscribe(commandTopic.c_str(), 1);

        Serial.print("[MQTT] Subscribed: ");
        Serial.println(commandTopic);

        // Publish trạng thái online
        mqttClient.publish(
            statusTopic.c_str(),
            "{\"online\":true}",
            true);

        return true;
    }

    Serial.print("[MQTT] Connection failed, state = ");
    Serial.println(mqttClient.state());

    return false;
}

// ========================================
// PUBLISH SENSOR DATA
// ========================================

void publishSensorData()
{
    float temperature;
    float humidity;

    // Đọc các biến dùng chung an toàn
    portENTER_CRITICAL(&temperatureMux);
    temperature = glob_temperature;
    portEXIT_CRITICAL(&temperatureMux);

    portENTER_CRITICAL(&humidityMux);
    humidity = glob_humidity;
    portEXIT_CRITICAL(&humidityMux);

    if (!glob_temperature_valid || glob_sensor_error)
    {
        Serial.println("[MQTT] Sensor data invalid!");
        return;
    }

    StaticJsonDocument<128> doc;

    doc["temperature"] = temperature;
    doc["humidity"] = humidity;

    char payload[128];

    serializeJson(doc, payload, sizeof(payload));

    bool success = mqttClient.publish(
        telemetryTopic.c_str(),
        payload);

    if (success)
    {
        Serial.print("[MQTT] Published: ");
        Serial.println(payload);
    }
    else
    {
        Serial.println("[MQTT] Publish failed!");
    }
}

// ========================================
// FREERTOS TASK
// ========================================

void mqtt_gateway_task(void *pvParameters)
{
    mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
    mqttClient.setCallback(mqttCallback);

    mqttClient.setBufferSize(512);
    mqttClient.setKeepAlive(30);

    unsigned long lastPublish = 0;
    unsigned long lastReconnect = 0;

    const unsigned long publishInterval = 10000;
    const unsigned long reconnectInterval = 5000;

    while (true)
    {
        // Chỉ kết nối khi ESP32 ở STA mode
        if (WiFi.status() != WL_CONNECTED)
        {
            if (mqttClient.connected())
                mqttClient.disconnect();

            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Reconnect MQTT nếu bị mất kết nối
        if (!mqttClient.connected())
        {
            if (millis() - lastReconnect >= reconnectInterval)
            {
                lastReconnect = millis();
                mqttConnect();
            }
        }
        else
        {
            mqttClient.loop();

            if (millis() - lastPublish >= publishInterval)
            {
                lastPublish = millis();
                publishSensorData();
            }
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
