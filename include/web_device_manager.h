#pragma once

#include <Arduino.h>

bool webDevicesInit();
String webDevicesJson();
bool webDeviceAdd(const String &name, const String &type, int gpio,
                  int pixelCount, bool activeLow, uint32_t &deviceId,
                  String &error);
bool webDeviceRemove(uint32_t deviceId, String &error);
bool webDeviceControl(uint32_t deviceId, const String &type, int value,
                      bool &state, String &error);
