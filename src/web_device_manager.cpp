#include "web_device_manager.h"

#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "task_handler.h"

namespace
{
constexpr char DEVICE_FILE[] = "/web_devices.json";
constexpr size_t MAX_WEB_DEVICES = 8;
constexpr size_t MAX_NAME_LENGTH = 32;
constexpr uint8_t DEFAULT_RGB_PIXELS = 4;
constexpr uint8_t MAX_RGB_PIXELS = 64;

enum class DeviceType : uint8_t
{
    Relay,
    Fan,
    Rgb
};

struct WebDevice
{
    uint32_t id = 0;
    String name;
    DeviceType type = DeviceType::Relay;
    uint8_t gpio = 0;
    uint8_t pixelCount = 0;
    bool activeLow = false;
    bool relayOn = false;
    uint8_t fanPercent = 0;
    uint32_t color = 0;
    Adafruit_NeoPixel *pixels = nullptr;
};

WebDevice devices[MAX_WEB_DEVICES];
size_t deviceCount = 0;
uint32_t nextDeviceId = 1;
SemaphoreHandle_t devicesMutex = nullptr;

bool lockDevices()
{
    return devicesMutex != nullptr && xSemaphoreTake(devicesMutex, portMAX_DELAY) == pdTRUE;
}

void unlockDevices()
{
    if (devicesMutex != nullptr)
        xSemaphoreGive(devicesMutex);
}

const char *typeName(DeviceType type)
{
    switch (type)
    {
    case DeviceType::Relay: return "relay";
    case DeviceType::Fan: return "fan";
    case DeviceType::Rgb: return "rgb";
    }
    return "";
}

bool parseType(const String &value, DeviceType &type)
{
    if (value == "relay") type = DeviceType::Relay;
    else if (value == "fan") type = DeviceType::Fan;
    else if (value == "rgb") type = DeviceType::Rgb;
    else return false;
    return true;
}

bool gpioIsReserved(int gpio)
{
    // Pins used by this firmware or reserved by the ESP32-S3/Yolo UNO board.
    if (gpio < 1 || gpio > 48 || gpio == 0 || gpio == 3 ||
        gpio == 6 || gpio == 8 || gpio == 9 || gpio == 10 ||
        gpio == 11 || gpio == 12 || gpio == 19 || gpio == 20 ||
        gpio == 26 || gpio == 27 || gpio == 28 || gpio == 29 ||
        gpio == 30 || gpio == 31 || gpio == 32 || gpio == 33 ||
        gpio == 34 || gpio == 35 || gpio == 36 || gpio == 37 ||
        gpio == 41 || gpio == 43 || gpio == 44 || gpio == 45 ||
        gpio == 46 || gpio == 48)
        return true;

    if (WEB_RELAY_GPIO > 0 && gpio == WEB_RELAY_GPIO)
        return true;
    return false;
}

bool gpioInUse(int gpio, int ignoredIndex = -1)
{
    for (size_t i = 0; i < deviceCount; ++i)
        if (static_cast<int>(i) != ignoredIndex && devices[i].gpio == gpio)
            return true;
    return false;
}

void turnOff(WebDevice &device)
{
    if (device.type == DeviceType::Relay)
    {
        digitalWrite(device.gpio, device.activeLow ? HIGH : LOW);
        device.relayOn = false;
    }
    else if (device.type == DeviceType::Fan)
    {
        analogWrite(device.gpio, 0);
        device.fanPercent = 0;
    }
    else if (device.pixels != nullptr)
    {
        device.pixels->clear();
        device.pixels->show();
        device.color = 0;
    }
}

void releaseDevice(WebDevice &device)
{
    turnOff(device);
    if (device.type == DeviceType::Rgb && device.pixels != nullptr)
    {
        delete device.pixels;
        device.pixels = nullptr;
    }
    pinMode(device.gpio, INPUT);
}

bool startDevice(WebDevice &device)
{
    if (device.type == DeviceType::Relay)
    {
        pinMode(device.gpio, OUTPUT);
        digitalWrite(device.gpio, device.activeLow ? HIGH : LOW);
        device.relayOn = false;
        return true;
    }
    if (device.type == DeviceType::Fan)
    {
        pinMode(device.gpio, OUTPUT);
        analogWrite(device.gpio, 0);
        device.fanPercent = 0;
        return true;
    }

    // The Arduino-ESP32 toolchain used here does not provide std::nothrow
    // operator new. Use the library's normal constructor supported by this core.
    device.pixels = new Adafruit_NeoPixel(
        device.pixelCount, device.gpio, NEO_GRB + NEO_KHZ800);
    if (device.pixels == nullptr)
        return false;
    device.pixels->begin();
    device.pixels->setBrightness(50);
    device.pixels->clear();
    device.pixels->show();
    device.color = 0;
    return true;
}

bool writeConfig(int skipIndex = -1)
{
    DynamicJsonDocument doc(2048);
    JsonArray entries = doc.createNestedArray("devices");
    for (size_t i = 0; i < deviceCount; ++i)
    {
        if (static_cast<int>(i) == skipIndex)
            continue;
        const WebDevice &device = devices[i];
        JsonObject entry = entries.createNestedObject();
        entry["id"] = device.id;
        entry["name"] = device.name;
        entry["type"] = typeName(device.type);
        entry["gpio"] = device.gpio;
        if (device.type == DeviceType::Rgb)
            entry["pixel_count"] = device.pixelCount;
        if (device.type == DeviceType::Relay)
            entry["active_low"] = device.activeLow;
    }

    File file = LittleFS.open(DEVICE_FILE, "w");
    if (!file)
        return false;
    const size_t expected = measureJson(doc);
    const size_t written = serializeJson(doc, file);
    file.flush();
    file.close();
    if (written == 0 || written != expected)
        return false;

    File check = LittleFS.open(DEVICE_FILE, "r");
    if (!check)
        return false;
    DynamicJsonDocument verify(2048);
    const DeserializationError error = deserializeJson(verify, check);
    check.close();
    return !error && verify["devices"].is<JsonArray>();
}

int findDevice(uint32_t id)
{
    for (size_t i = 0; i < deviceCount; ++i)
        if (devices[i].id == id)
            return static_cast<int>(i);
    return -1;
}

void appendDeviceJson(JsonArray array, const WebDevice &device)
{
    JsonObject entry = array.createNestedObject();
    entry["id"] = device.id;
    entry["name"] = device.name;
    entry["type"] = typeName(device.type);
    entry["gpio"] = device.gpio;
    if (device.type == DeviceType::Relay)
    {
        entry["on"] = device.relayOn;
        entry["active_low"] = device.activeLow;
    }
    else if (device.type == DeviceType::Fan)
        entry["percent"] = device.fanPercent;
    else
    {
        entry["pixel_count"] = device.pixelCount;
        char color[8];
        snprintf(color, sizeof(color), "#%06lX", static_cast<unsigned long>(device.color & 0xFFFFFF));
        entry["color"] = color;
    }
}
}

bool webDevicesInit()
{
    if (devicesMutex == nullptr)
    {
        devicesMutex = xSemaphoreCreateMutex();
        if (devicesMutex == nullptr)
        {
            Serial.println("[WEB-DEVICES] Cannot create device registry mutex");
            return false;
        }
    }

    deviceCount = 0;
    nextDeviceId = 1;
    File file = LittleFS.open(DEVICE_FILE, "r");
    if (!file)
    {
        Serial.println("[WEB-DEVICES] No saved devices");
        return true;
    }

    DynamicJsonDocument doc(2048);
    const DeserializationError error = deserializeJson(doc, file);
    file.close();
    if (error || !doc["devices"].is<JsonArray>())
    {
        Serial.println("[WEB-DEVICES] Saved device list is invalid");
        return false;
    }

    for (JsonObjectConst entry : doc["devices"].as<JsonArrayConst>())
    {
        if (deviceCount >= MAX_WEB_DEVICES)
            break;
        DeviceType type;
        const String name = entry["name"].as<String>();
        const String typeText = entry["type"].as<String>();
        const int gpio = entry["gpio"] | -1;
        if (name.isEmpty() || name.length() > MAX_NAME_LENGTH ||
            !parseType(typeText, type) || gpioIsReserved(gpio) || gpioInUse(gpio))
            continue;

        WebDevice &device = devices[deviceCount];
        device.id = entry["id"] | nextDeviceId;
        device.name = name;
        device.type = type;
        device.gpio = static_cast<uint8_t>(gpio);
        device.pixelCount = (type == DeviceType::Rgb)
                                ? static_cast<uint8_t>(entry["pixel_count"] | DEFAULT_RGB_PIXELS)
                                : 0;
        device.activeLow = (type == DeviceType::Relay) && (entry["active_low"] | false);
        if ((type == DeviceType::Rgb && (device.pixelCount == 0 || device.pixelCount > MAX_RGB_PIXELS)) ||
            !startDevice(device))
        {
            device = WebDevice{};
            continue;
        }
        nextDeviceId = max(nextDeviceId, device.id + 1);
        ++deviceCount;
    }
    Serial.printf("[WEB-DEVICES] Loaded %u saved devices\n", static_cast<unsigned>(deviceCount));
    return true;
}

String webDevicesJson()
{
    if (!lockDevices())
        return "{\"devices\":[]}";
    DynamicJsonDocument doc(2048);
    JsonArray entries = doc.createNestedArray("devices");
    for (size_t i = 0; i < deviceCount; ++i)
        appendDeviceJson(entries, devices[i]);
    String result;
    serializeJson(doc, result);
    unlockDevices();
    return result;
}

bool webDeviceAdd(const String &name, const String &typeText, int gpio,
                  int pixelCount, bool activeLow, uint32_t &deviceId,
                  String &error)
{
    if (!lockDevices())
    {
        error = "Bo quan ly thiet bi chua san sang";
        return false;
    }
    DeviceType type;
    if (name.isEmpty() || name.length() > MAX_NAME_LENGTH)
        error = "Ten thiet bi phai co tu 1 den 32 ky tu";
    else if (!parseType(typeText, type))
        error = "Loai thiet bi khong duoc ho tro";
    else if (deviceCount >= MAX_WEB_DEVICES)
        error = "Da dat gioi han so thiet bi web";
    else if (gpioIsReserved(gpio) || gpioInUse(gpio))
        error = "GPIO khong hop le, dang duoc su dung hoac dang duoc danh rieng";
    else if (type == DeviceType::Rgb && (pixelCount < 1 || pixelCount > MAX_RGB_PIXELS))
        error = "NeoPixel ho tro tu 1 den 64 pixel";
    else if (type != DeviceType::Relay && activeLow)
        error = "Tuy chon active-low chi ap dung cho relay";
    else
    {
        WebDevice &device = devices[deviceCount];
        device.id = nextDeviceId++;
        device.name = name;
        device.type = type;
        device.gpio = static_cast<uint8_t>(gpio);
        device.pixelCount = (type == DeviceType::Rgb) ? static_cast<uint8_t>(pixelCount) : 0;
        device.activeLow = activeLow;
        if (!startDevice(device))
        {
            device = WebDevice{};
            error = "Khong the khoi tao thiet bi tren GPIO nay";
        }
        else
        {
            ++deviceCount;
            if (!writeConfig())
            {
                --deviceCount;
                releaseDevice(device);
                device = WebDevice{};
                error = "Khong luu duoc cau hinh thiet bi vao LittleFS";
            }
            else
            {
                deviceId = device.id;
                unlockDevices();
                return true;
            }
        }
    }
    unlockDevices();
    return false;
}

bool webDeviceRemove(uint32_t deviceId, String &error)
{
    if (!lockDevices())
    {
        error = "Bo quan ly thiet bi chua san sang";
        return false;
    }
    const int index = findDevice(deviceId);
    if (index < 0)
    {
        error = "Khong tim thay thiet bi";
        unlockDevices();
        return false;
    }
    if (!writeConfig(index))
    {
        error = "Khong luu duoc thay doi vao LittleFS";
        unlockDevices();
        return false;
    }
    releaseDevice(devices[index]);
    for (size_t i = static_cast<size_t>(index); i + 1 < deviceCount; ++i)
        devices[i] = devices[i + 1];
    devices[deviceCount - 1] = WebDevice{};
    --deviceCount;
    unlockDevices();
    return true;
}

bool webDeviceControl(uint32_t deviceId, const String &typeText, int value,
                      bool &state, String &error)
{
    if (!lockDevices())
    {
        error = "Bo quan ly thiet bi chua san sang";
        return false;
    }
    const int index = findDevice(deviceId);
    if (index < 0)
    {
        error = "Khong tim thay thiet bi";
        unlockDevices();
        return false;
    }
    WebDevice &device = devices[index];
    if (typeText != typeName(device.type))
    {
        error = "Loai lenh khong khop voi thiet bi";
        unlockDevices();
        return false;
    }

    if (device.type == DeviceType::Relay)
    {
        if (value != 0 && value != 1)
        {
            error = "Relay chi nhan trang thai 0 hoac 1";
            unlockDevices();
            return false;
        }
        device.relayOn = value == 1;
        digitalWrite(device.gpio, device.relayOn == device.activeLow ? LOW : HIGH);
        state = device.relayOn;
        unlockDevices();
        return true;
    }
    if (device.type == DeviceType::Fan)
    {
        if (value < 0 || value > 100)
        {
            error = "Toc do quat phai trong khoang 0 den 100";
            unlockDevices();
            return false;
        }
        device.fanPercent = static_cast<uint8_t>(value);
        analogWrite(device.gpio, (value * 255 + 50) / 100);
        state = value > 0;
        unlockDevices();
        return true;
    }
    if (value < 0 || value > 0xFFFFFF || device.pixels == nullptr)
    {
        error = "Mau RGB khong hop le";
        unlockDevices();
        return false;
    }
    device.color = static_cast<uint32_t>(value);
    device.pixels->fill(device.pixels->Color((value >> 16) & 0xFF,
                                             (value >> 8) & 0xFF,
                                             value & 0xFF));
    device.pixels->show();
    state = device.color != 0;
    unlockDevices();
    return true;
}
