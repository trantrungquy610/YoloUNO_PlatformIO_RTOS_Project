#include "task_handler.h"
#include "task_webserver.h"
#include "task_check_info.h"

// Reject known-in-use/reserved pins. GPIO must also be explicitly configured.
// Board pinout and relay polarity MUST be verified before changing -1.
int webRelayGpio()
{
    const int gpio = WEB_RELAY_GPIO;
    if (gpio < 1 || gpio > 48 || gpio == 6 || gpio == 8 ||
        gpio == 9 || gpio == 10 || gpio == 11 || gpio == 12 ||
        gpio == 19 || gpio == 20 || gpio == 45 || gpio == 46 ||
        gpio == 48)
        return -1;
    return gpio;
}

bool webRelayState()
{
    const int gpio = webRelayGpio();
    return gpio >= 0 && digitalRead(gpio) == HIGH;
}

static void replyJson(AsyncWebSocketClient *client, JsonDocument &doc)
{
    if (!client) return;
    String response;
    serializeJson(doc, response);
    client->text(response); // Only the requesting client receives its ACK.
}

static bool validPort(const String &port)
{
    if (port.isEmpty() || port.length() > 5) return false;
    for (size_t i = 0; i < port.length(); ++i)
        if (port[i] < '0' || port[i] > '9') return false;
    const long value = port.toInt();
    return value >= 1 && value <= 65535;
}

void handleWebSocketMessage(AsyncWebSocketClient *client, const String &message)
{
    if (!client) return;
    StaticJsonDocument<1024> doc;
    const DeserializationError error = deserializeJson(doc, message);
    if (error || !doc.is<JsonObject>())
    {
        client->text("{\"page\":\"error\",\"message\":\"JSON khong hop le\"}");
        return;
    }

    const String page = doc["page"].as<String>();
    const uint32_t requestId = doc["request_id"] | 0UL;
    if (page == "device")
    {
        StaticJsonDocument<256> response;
        response["page"] = "device_result";
        response["request_id"] = requestId;
        const int gpio = doc["value"]["gpio"] | -1;
        const String status = doc["value"]["status"].as<String>();
        if (webRelayGpio() < 0 || gpio != webRelayGpio())
        {
            response["status"] = "error";
            response["message"] = "GPIO relay chua duoc cau hinh hoac khong duoc phep";
        }
        else if (status != "ON" && status != "OFF")
        {
            response["status"] = "error";
            response["message"] = "Status phai la ON hoac OFF";
        }
        else
        {
            digitalWrite(gpio, status == "ON" ? HIGH : LOW);
            response["status"] = "ok";
            response["gpio"] = gpio;
            response["state"] = webRelayState() ? "ON" : "OFF";
            // GPIO level confirmation is NOT physical load feedback.
        }
        replyJson(client, response);
        return;
    }

    if (page == "setting")
    {
        StaticJsonDocument<256> response;
        response["page"] = "setting_saved";
        response["request_id"] = requestId;
        const String ssid = doc["value"]["ssid"].as<String>();
        const String password = doc["value"]["password"].as<String>();
        const String token = doc["value"]["token"].as<String>();
        const String server = doc["value"]["server"].as<String>();
        const String port = doc["value"]["port"].as<String>();

        if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63 ||
            token.isEmpty() || token.length() > 256 || server.isEmpty() ||
            server.length() > 253 || !validPort(port))
        {
            response["status"] = "error";
            response["message"] = "SSID, password, token, server hoac port khong hop le";
        }
        else if (!Save_info_File_NoRestart(ssid, password, token, server, port))
        {
            response["status"] = "error";
            response["message"] = "Khong ghi duoc cau hinh LittleFS";
        }
        else
        {
            response["status"] = "ok";
            response["message"] = "Da ghi cau hinh; sap khoi dong lai";
        }
        replyJson(client, response);
        if (response["status"] == "ok")
            Webserver_scheduleRestart(2500); // Send ACK before restarting.
        return;
    }
    client->text("{\"page\":\"error\",\"message\":\"Lenh khong duoc ho tro\"}");
}
