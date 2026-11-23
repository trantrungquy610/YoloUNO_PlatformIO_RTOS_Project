#ifndef __TASK_HANDLER_H__
#define __TASK_HANDLER_H__

#include <Arduino.h>

// Web relay is OFF unless the hardware owner approves a free GPIO.
// This is NOT a general-purpose GPIO console.
#ifndef WEB_RELAY_GPIO
#define WEB_RELAY_GPIO -1
#endif

class AsyncWebSocketClient;
void handleWebSocketMessage(AsyncWebSocketClient *client, const String &message);
int webRelayGpio();
bool webRelayState();
void Webserver_scheduleRestart(uint32_t delayMs);

#endif
