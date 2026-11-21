#include "task_webserver.h"
#include "task_gesture.h"
#include <cstring>
#include <stdlib.h>
#include <string.h>

// Async server duoc tach sang cong 8080 de khong tranh chap voi MainServer (80).
AsyncWebServer asyncServer(8080);
AsyncWebSocket ws("/ws");

bool webserver_isrunning = false;

void Webserver_sendata(String data)
{
    if (ws.count() > 0)
    {
        ws.textAll(data); // Gửi đến tất cả client đang kết nối
        Serial.println("📤 Đã gửi dữ liệu qua WebSocket: " + data);
    }
    else
    {
        Serial.println("⚠️ Không có client WebSocket nào đang kết nối!");
    }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        AwsFrameInfo *info = (AwsFrameInfo *)arg;

        if (info->opcode == WS_TEXT)
        {
            String message;
            message += String((char *)data).substring(0, len);
            // parseJson(message, true);
            handleWebSocketMessage(message);
        }
    }
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

void connnectWSV()
{
    ws.onEvent(onEvent);
    asyncServer.addHandler(&ws);
    asyncServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
                   { request->send(LittleFS, "/index.html", "text/html"); });
    asyncServer.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
                   { request->send(LittleFS, "/script.js", "application/javascript"); });
    asyncServer.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest *request)
                   { request->send(LittleFS, "/styles.css", "text/css"); });
    asyncServer.on("/health", HTTP_GET, [](AsyncWebServerRequest *request)
                   { request->send(200, "text/plain", "AsyncWebServer is working!"); });
    ElegantOTA.begin(&asyncServer);
    registerGestureEndpoint();
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
    // AsyncWebServer/ElegantOTA cung chi khoi dong sau khi Wi-Fi co IP.
    if (!isNetworkReady)
    {
        return;
    }

    if (!webserver_isrunning)
    {
        connnectWSV();
    }
    ElegantOTA.loop();
}
