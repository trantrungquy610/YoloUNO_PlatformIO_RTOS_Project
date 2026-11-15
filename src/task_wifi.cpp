#include "task_wifi.h"

bool startAP()
{
    // Danh dau mang chua san sang cho den khi AP co IP hop le.
    isNetworkReady = false;
    isWifiConnected = false;

    // Wi-Fi mode chi duoc quan ly tai module nay.
    if (!WiFi.mode(WIFI_AP))
    {
        Serial.println("ERROR: Cannot switch Wi-Fi to AP mode.");
        return false;
    }

    if (!WiFi.softAP(String(SSID_AP), String(PASS_AP)))
    {
        Serial.println("ERROR: Cannot start Wi-Fi AP.");
        return false;
    }

    // softAP() thanh cong la dieu kien can, nhung van doi interface co IP.
    const IPAddress noIP(0, 0, 0, 0);
    const uint32_t startMs = millis();
    while ((WiFi.getMode() & WIFI_AP) == 0 || WiFi.softAPIP() == noIP)
    {
        if (millis() - startMs >= 5000)
        {
            Serial.println("ERROR: AP started but its network interface is not ready.");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    isNetworkReady = true;
    Serial.print("AP ready. IP: ");
    Serial.println(WiFi.softAPIP());
    return true;
}

static bool connectSTAWithTimeout(uint32_t timeoutMs)
{
    if (WIFI_SSID.isEmpty())
    {
        Serial.println("No saved Wi-Fi SSID; keeping AP mode.");
        return false;
    }

    isNetworkReady = false;
    if (!WiFi.mode(WIFI_STA))
    {
        Serial.println("ERROR: Cannot switch Wi-Fi to STA mode.");
        startAP();
        return false;
    }
    if (WIFI_PASS.isEmpty())
    {
        WiFi.begin(WIFI_SSID.c_str());
    }
    else
    {
        WiFi.begin(WIFI_SSID.c_str(), WIFI_PASS.c_str());
    }

    Serial.print("Connecting to Wi-Fi: ");
    Serial.println(WIFI_SSID);

    const uint32_t startMs = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < timeoutMs)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    if (WiFi.status() == WL_CONNECTED)
    {
        isWifiConnected = true;
        isNetworkReady = (WiFi.localIP() != IPAddress(0, 0, 0, 0));
        Serial.print("STA IP: ");
        Serial.println(WiFi.localIP());

        if (xBinarySemaphoreInternet != nullptr)
        {
            xSemaphoreGive(xBinarySemaphoreInternet);
        }
        return isNetworkReady;
    }

    Serial.println("Wi-Fi connection timed out; restoring AP mode.");
    WiFi.disconnect();
    startAP();
    return false;
}

void startSTA()
{
    (void)connectSTAWithTimeout(15000);
}

bool Wifi_reconnect()
{
    // Trang cai dat dang tu quan ly qua trinh ket noi AP+STA.
    if (wifiConfigInProgress || (WiFi.getMode() & WIFI_AP))
        return false;
    if (WiFi.status() == WL_CONNECTED)
    {
        isWifiConnected = true;
        isNetworkReady = (WiFi.localIP() != IPAddress(0, 0, 0, 0));
        return isNetworkReady;
    }

    if (WIFI_SSID.isEmpty())
    {
        return false;
    }

    // Avoid blocking the Arduino loop with repeated 15-second attempts.
    static bool attemptedOnce = false;
    static uint32_t lastAttemptMs = 0;
    const uint32_t retryIntervalMs = 30000;

    if (attemptedOnce && (millis() - lastAttemptMs) < retryIntervalMs)
    {
        return false;
    }

    attemptedOnce = true;
    lastAttemptMs = millis();
    return connectSTAWithTimeout(15000);
}
