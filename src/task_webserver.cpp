#include "task_webserver.h"
#include "global.h"
#include <WiFi.h>
#include <math.h>

// WebServer local for the browser UI. MainServer (port 80) remains unchanged.
AsyncWebServer asyncServer(8080);
AsyncWebSocket ws("/ws");
bool webserver_isrunning = false;
static uint32_t scheduledRestartAt = 0;

void Webserver_scheduleRestart(uint32_t delayMs)
{
    scheduledRestartAt = millis() + delayMs;
}

void Webserver_sendata(String data)
{
    if (ws.count() > 0)
        ws.textAll(data);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
             AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
        Serial.printf("WebSocket client #%u connected\n", client->id());
    else if (type == WS_EVT_DISCONNECT)
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = static_cast<AwsFrameInfo *>(arg);
        // Never use String((char*)data): websocket payload is NOT null-terminated.
        // This UI sends single JSON frames; reject oversized/fragmented packets.
        if (info->final && info->index == 0 && info->len == len &&
            info->opcode == WS_TEXT && len > 0 && len <= 1024)
        {
            String message;
            message.reserve(len);
            for (size_t i = 0; i < len; ++i)
                message += static_cast<char>(data[i]);
            handleWebSocketMessage(message);
        }
        else
            client->text("{\"page\":\"error\",\"message\":\"Invalid or oversized WebSocket frame\"}");
    }
}

void connnectWSV()
{
    // GPIO default LOW at startup (only when explicitly configured by hardware owner).
    const int relayGpio = webRelayGpio();
    if (relayGpio >= 0)
    {
        pinMode(relayGpio, OUTPUT);
        digitalWrite(relayGpio, LOW);
    }
    ws.onEvent(onEvent);
    asyncServer.addHandler(&ws);

    asyncServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/index.html", "text/html");
    });
    asyncServer.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/script.js", "application/javascript");
    });
    asyncServer.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(LittleFS, "/styles.css", "text/css");
    });

    // Same-origin endpoint, so the browser never needs cross-port CORS to port 80.
    // Read existing globals only; do not change the sensor task.
    asyncServer.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
        float temperature;
        float humidity;
        bool valid;
        bool sensorError;
        portENTER_CRITICAL(&temperatureMux);
        temperature = glob_temperature;
        valid = glob_temperature_valid;
        sensorError = glob_sensor_error;
        portEXIT_CRITICAL(&temperatureMux);
        portENTER_CRITICAL(&humidityMux);
        humidity = glob_humidity;
        portEXIT_CRITICAL(&humidityMux);
        valid = valid && !sensorError && isfinite(temperature) && isfinite(humidity);

        StaticJsonDocument<192> doc;
        doc["valid"] = valid;
        doc["sensor_error"] = sensorError;
        doc["temperature"] = valid ? temperature : 0;
        doc["humidity"] = valid ? humidity : 0;
        doc["uptime_ms"] = millis();
        String output;
        serializeJson(doc, output);
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", output);
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
    });

    // Only nonsensitive device diagnostics: NEVER return Wi-Fi password or MQTT token.
    asyncServer.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<288> doc;
        const wifi_mode_t mode = WiFi.getMode();
        doc["wifi_mode"] = (mode == WIFI_STA) ? "STA" : (mode == WIFI_AP) ? "AP" : (mode == WIFI_AP_STA) ? "AP+STA" : "OFF";
        doc["wifi_connected"] = (WiFi.status() == WL_CONNECTED);
        doc["ip"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
        doc["uptime_seconds"] = millis() / 1000;
        doc["core_iot_configured"] = !CORE_IOT_TOKEN.isEmpty() && !CORE_IOT_SERVER.isEmpty();
        doc["relay_gpio"] = webRelayGpio();
        if (webRelayGpio() >= 0)
            doc["relay_on"] = webRelayState();
        String output;
        serializeJson(doc, output);
        AsyncWebServerResponse *response = request->beginResponse(200, "application/json", output);
        response->addHeader("Cache-Control", "no-store");
        request->send(response);
    });

    asyncServer.on("/health", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "AsyncWebServer is working!");
    });
    ElegantOTA.begin(&asyncServer);
    asyncServer.begin();
    Serial.println("AsyncWebServer started on port 8080");
    webserver_isrunning = true;
}

void Webserver_stop()
{
    ws.closeAll();
    asyncServer.end();
    webserver_isrunning = false;
}

void Webserver_reconnect()
{
    if (scheduledRestartAt != 0 &&
        static_cast<int32_t>(millis() - scheduledRestartAt) >= 0)
    {
        ESP.restart();
        return;
    }
    if (!isNetworkReady)
        return;
    if (!webserver_isrunning)
        connnectWSV();
    ws.cleanupClients();
    ElegantOTA.loop();
}
