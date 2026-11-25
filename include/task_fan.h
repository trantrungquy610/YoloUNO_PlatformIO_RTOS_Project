#ifndef TASK_FAN_H
#define TASK_FAN_H

#include <stdint.h>

void task_fan_control(void *pvParameters);
bool fan_control_init();
bool fan_set_speed_percent(uint8_t percent);
uint8_t fan_get_speed_percent();

#endif
