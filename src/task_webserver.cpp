#include "task_webserver.h"
#include "global.h"
#include <WiFi.h>
#include <math.h>
#include "task_gesture.h"
#include <cstring>
#include <stdlib.h>
#include <string.h>

// Keep branch's synchronous Wi-Fi onboarding server at port 80 unchanged.
// This local dashboard runs separately at port 8080 and starts only when AP/STA has an IP.
AsyncWebServer asyncServer(8080);
AsyncWebSocket ws("/ws");
static bool webserver_isrunning = false;
static uint32_t scheduledRestartAt = 0;

void Webserver_scheduleRestart(uint32_t delayMs)
{
    scheduledRestartAt = millis() + delayMs;
}

void Webserver_sendata(String data)
{
    // Reserved for intentional broadcasts of non-sensitive telemetry.
    if (ws.count() > 0) ws.textAll(data);
}

static void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
                    AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("[WEB] WebSocket connected #%u\n", client->id());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("[WEB] WebSocket disconnected #%u\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        const AwsFrameInfo *info = static_cast<AwsFrameInfo *>(arg);
        if (!info || !info->final || info->index != 0 || info->len != len ||
            info->opcode != WS_TEXT || len == 0 || len > 1024)
        {
            client->text("{\"page\":\"error\",\"message\":\"Frame WebSocket khong hop le\"}");
            return;
        }
        // Frame bytes are not null-terminated: construct exactly len bytes.
        String message;
        if (!message.reserve(len + 1))
        {
            client->text("{\"page\":\"error\",\"message\":\"Thieu bo nho\"}");
            return;
        }
        for (size_t i = 0; i < len; ++i)
            message += static_cast<char>(data[i]);
        handleWebSocketMessage(client, message);
    }
}

static void sendJson(AsyncWebServerRequest *request, JsonDocument &doc)
{
    String result;
    serializeJson(doc, result);
    AsyncWebServerResponse *response =
        request->beginResponse(200, "application/json", result);
    response->addHeader("Cache-Control", "no-store");
    response->addHeader("X-Content-Type-Options", "nosniff");
    request->send(response);
}

// =====================================================
// Gesture HTTP Frame Receiver
// =====================================================

struct GestureHttpFrame
{
    uint8_t pixels[GESTURE_FRAME_BYTES];
    size_t received = 0;
    bool invalid = false;
};

static void registerGestureEndpoint()
{
    asyncServer.on(
        "/api/gesture/frame",
        HTTP_POST,

        // Được gọi sau khi nhận xong HTTP body
        [](AsyncWebServerRequest *request)
        {
            GestureHttpFrame *frame =
                static_cast<GestureHttpFrame *>(request->_tempObject);

            request->_tempObject = nullptr;

            if (frame == nullptr)
            {
                request->send(
                    400,
                    "text/plain",
                    "Missing image frame");
                return;
            }

            bool valid =
                !frame->invalid &&
                frame->received == GESTURE_FRAME_BYTES &&
                request->contentType() == "application/octet-stream";

            if (!valid)
            {
                free(frame);
                request->send(
                    400,
                    "text/plain",
                    "Invalid frame. Expected 4096 bytes of application/octet-stream");
                return;
            }

            bool queued = gesture_submit_frame(
                frame->pixels,
                GESTURE_FRAME_BYTES);

            free(frame);

            if (!queued)
            {
                request->send(
                    503,
                    "text/plain",
                    "Gesture queue unavailable");
                return;
            }

            request->send(
                202,
                "text/plain",
                "Gesture frame queued");
        },

        nullptr,

        // Nhận từng phần HTTP body
        [](AsyncWebServerRequest *request,
           uint8_t *data,
           size_t len,
           size_t index,
           size_t total)
        {
            GestureHttpFrame *frame =
                static_cast<GestureHttpFrame *>(request->_tempObject);

            if (index == 0 && frame == nullptr)
            {
                frame = static_cast<GestureHttpFrame *>(
                    calloc(1, sizeof(GestureHttpFrame)));

                if (frame == nullptr)
                {
                    Serial.println("[GESTURE HTTP] Memory allocation failed");
                    return;
                }

                request->_tempObject = frame;
            }

            if (frame == nullptr)
                return;

            if (total != GESTURE_FRAME_BYTES ||
                index != frame->received ||
                len > GESTURE_FRAME_BYTES - frame->received)
            {
                frame->invalid = true;
                return;
            }

            memcpy(
                frame->pixels + index,
                data,
                len);

            frame->received += len;
        });
}

static void startServer()
{
    const int gpio = webRelayGpio();
    if (gpio >= 0)
    {
        // Default LOW (active-HIGH relay only); hardware must approve pin/polarity.
        digitalWrite(gpio, LOW);
        pinMode(gpio, OUTPUT);
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
    asyncServer.on("/health", HTTP_GET, [](AsyncWebServerRequest *request) {
        request->send(200, "text/plain", "AsyncWebServer is working!");
    });
    asyncServer.on("/api/sensors", HTTP_GET, [](AsyncWebServerRequest *request) {
        float temperature;
        float humidity;
        bool sensorReady;
        bool sensorError;
        portENTER_CRITICAL(&temperatureMux);
        temperature = glob_temperature;
        sensorReady = glob_temperature_valid;
        sensorError = glob_sensor_error;
        portEXIT_CRITICAL(&temperatureMux);
        portENTER_CRITICAL(&humidityMux);
        humidity = glob_humidity;
        portEXIT_CRITICAL(&humidityMux);
        const bool valid = sensorReady && !sensorError && isfinite(temperature) &&
                           isfinite(humidity) && temperature >= -40 && temperature <= 85 &&
                           humidity >= 0 && humidity <= 100;
        StaticJsonDocument<192> doc;
        doc["valid"] = valid;
        doc["sensor_error"] = sensorError;
        doc["temperature"] = valid ? temperature : 0;
        doc["humidity"] = valid ? humidity : 0;
        doc["uptime_ms"] = millis();
        sendJson(request, doc);
    });
    asyncServer.on("/api/info", HTTP_GET, [](AsyncWebServerRequest *request) {
        StaticJsonDocument<320> doc;
        const wifi_mode_t mode = WiFi.getMode();
        doc["wifi_mode"] = (mode == WIFI_STA) ? "STA" :
                           (mode == WIFI_AP) ? "AP" :
                           (mode == WIFI_AP_STA) ? "AP+STA" : "OFF";
        const bool wifiConnected = WiFi.status() == WL_CONNECTED;
        doc["wifi_connected"] = wifiConnected;
        doc["ip"] = wifiConnected ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
        doc["ap_ip"] = (mode & WIFI_AP) ? WiFi.softAPIP().toString() : "";
        doc["uptime_seconds"] = millis() / 1000;
        doc["core_iot_configured"] = !CORE_IOT_TOKEN.isEmpty() && !CORE_IOT_SERVER.isEmpty();
        doc["relay_gpio"] = webRelayGpio();
        if (webRelayGpio() >= 0) doc["relay_on"] = webRelayState();
        sendJson(request, doc);
    });

    registerGestureEndpoint();

    // OTA requires an explicit security review; do not expose unauthenticated OTA.
#if defined(WEB_ENABLE_OTA) && WEB_ENABLE_OTA
    ElegantOTA.begin(&asyncServer);
#endif
    asyncServer.begin();
    Serial.println("AsyncWebServer started on port 8080");

    webserver_isrunning = true;
    Serial.println("[WEB] AsyncWebServer started on port 8080");
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
    if (!isNetworkReady) return;
    if (!webserver_isrunning) startServer();
    ws.cleanupClients();
#if defined(WEB_ENABLE_OTA) && WEB_ENABLE_OTA
    ElegantOTA.loop();
#endif
}
