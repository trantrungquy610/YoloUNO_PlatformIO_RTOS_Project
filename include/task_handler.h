
#ifndef __TASK_HANDLER_H__
#define __TASK_HANDLER_H__

#include <ArduinoJson.h>
#include <task_check_info.h>

// Configure ONLY after checking actual wiring. -1 disables Web relay control.
#ifndef WEB_RELAY_GPIO
#define WEB_RELAY_GPIO -1
#endif
extern void handleWebSocketMessage(String message);
extern int webRelayGpio();
extern bool webRelayState();
extern void Webserver_scheduleRestart(uint32_t delayMs);
#endif