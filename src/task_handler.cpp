#include <task_handler.h>

// GPIO pins used by existing tasks (fan, RGB, RS485, I2C, status LEDs) and
// ESP32-S3 USB/boot pins must not be controlled by an arbitrary web request.
// WEB_RELAY_GPIO is intentionally -1 until the team's hardware owner selects
// a verified, unused output pin and wires a low-voltage relay to it.
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
    int gpio = webRelayGpio();
    return gpio >= 0 && digitalRead(gpio) == HIGH;
}

void handleWebSocketMessage(String message)
{
    StaticJsonDocument<1024> doc;
    if (deserializeJson(doc, message))
    {
        ws.textAll("{\"page\":\"error\",\"message\":\"JSON khong hop le\"}");
        return;
    }
    String page = doc["page"].as<String>();
    unsigned long requestId = doc["request_id"] | 0UL;

    if (page == "device")
    {
        StaticJsonDocument<192> response;
        response["page"] = "device_result";
        response["request_id"] = requestId;
        const int gpio = doc["value"]["gpio"] | -1;
        const String status = doc["value"]["status"].as<String>();
        if (webRelayGpio() < 0 || gpio != webRelayGpio())
        {
            response["status"] = "error";
            response["message"] = "Relay GPIO chua duoc cau hinh hoac khong duoc phep";
        }
        else if (status != "ON" && status != "OFF")
        {
            response["status"] = "error";
            response["message"] = "Status phai la ON hoac OFF";
        }
        else
        {
            pinMode(gpio, OUTPUT);
            digitalWrite(gpio, status == "ON" ? HIGH : LOW);
            response["status"] = "ok";
            response["gpio"] = gpio;
            response["state"] = webRelayState() ? "ON" : "OFF";
            // GPIO write acknowledged; this does NOT prove the physical load switched.
        }
        String output;
        serializeJson(response, output);
        ws.textAll(output);
    }
    else if (page == "setting")
    {
        StaticJsonDocument<220> response;
        response["page"] = "setting_saved";
        response["request_id"] = requestId;
        const String ssid = doc["value"]["ssid"].as<String>();
        const String password = doc["value"]["password"].as<String>();
        const String token = doc["value"]["token"].as<String>();
        const String server = doc["value"]["server"].as<String>();
        const String port = doc["value"]["port"].as<String>();
        const int parsedPort = port.toInt();
        if (ssid.isEmpty() || ssid.length() > 32 || password.length() > 63 ||
            token.isEmpty() || server.isEmpty() || parsedPort <= 0 || parsedPort > 65535)
        {
            response["status"] = "error";
            response["message"] = "SSID, token, server hoac port khong hop le";
        }
        else if (!Save_info_File_NoRestart(ssid, password, token, server, port))
        {
            response["status"] = "error";
            response["message"] = "Luu LittleFS that bai";
        }
        else
        {
            response["status"] = "ok";
            response["message"] = "Da luu vao LittleFS; thiet bi se khoi dong lai";
            // Delayed restart gives the browser time to receive the acknowledgement.
            // Reconnecting Wi-Fi/Core IoT afterwards is not guaranteed by this ACK.
            Webserver_scheduleRestart(2500);
        }
        String output;
        serializeJson(response, output);
        ws.textAll(output);
    }
}
