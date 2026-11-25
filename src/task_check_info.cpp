#include "task_check_info.h"

void Load_info_File()
{
    File file = LittleFS.open("/info.dat", "r");
    if (!file) return;
    DynamicJsonDocument doc(4096);
    const DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error)
    {
        Serial.println("[CONFIG] Cannot parse saved configuration");
        return;
    }
    // Avoid strdup() on nullable keys (heap leak and null pointer hazard).
    WIFI_SSID = doc["WIFI_SSID"].as<String>();
    WIFI_PASS = doc["WIFI_PASS"].as<String>();
    CORE_IOT_TOKEN = doc["CORE_IOT_TOKEN"].as<String>();
    CORE_IOT_SERVER = doc["CORE_IOT_SERVER"].as<String>();
    CORE_IOT_PORT = doc["CORE_IOT_PORT"].as<String>();
}

void Delete_info_File()
{
    if (LittleFS.exists("/info.dat")) LittleFS.remove("/info.dat");
    ESP.restart();
}

bool Save_info_File_NoRestart(String wifi_ssid, String wifi_pass,
                              String token, String server, String port)
{
    DynamicJsonDocument doc(4096);
    doc["WIFI_SSID"] = wifi_ssid;
    doc["WIFI_PASS"] = wifi_pass;
    doc["CORE_IOT_TOKEN"] = token;
    doc["CORE_IOT_SERVER"] = server;
    doc["CORE_IOT_PORT"] = port;

    File file = LittleFS.open("/info.dat", "w");
    if (!file) return false;
    const size_t expected = measureJson(doc);
    const size_t written = serializeJson(doc, file);
    file.flush();
    file.close();
    if (written == 0 || written != expected)
    {
        Serial.println("[CONFIG] Incomplete configuration write");
        return false;
    }

    // Verify the saved JSON is readable before acknowledging to the browser.
    File check = LittleFS.open("/info.dat", "r");
    if (!check) return false;
    DynamicJsonDocument persisted(4096);
    const DeserializationError parseError = deserializeJson(persisted, check);
    check.close();
    if (parseError || persisted["WIFI_SSID"].as<String>() != wifi_ssid ||
        persisted["CORE_IOT_TOKEN"].as<String>() != token)
    {
        Serial.println("[CONFIG] Read-back verification failed");
        return false;
    }

    WIFI_SSID = wifi_ssid;
    WIFI_PASS = wifi_pass;
    CORE_IOT_TOKEN = token;
    CORE_IOT_SERVER = server;
    CORE_IOT_PORT = port;
    Serial.println("[CONFIG] Saved configuration; credentials are not logged");
    return true;
}

void Save_info_File(String wifi_ssid, String wifi_pass,
                    String token, String server, String port)
{
    if (Save_info_File_NoRestart(wifi_ssid, wifi_pass, token, server, port))
        ESP.restart();
    else
        Serial.println("[CONFIG] Save failed, restart cancelled");
}

bool check_info_File(bool check)
{
    if (!check)
    {
        if (!LittleFS.begin(true))
        {
            Serial.println("[CONFIG] LittleFS mount failed");
            return false;
        }
        Load_info_File();
    }
    if (WIFI_SSID.isEmpty())
    {
        if (!check) startAP();
        return false;
    }
    return true;
}
