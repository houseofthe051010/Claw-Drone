#pragma once

#include <stdbool.h>
#include <stdint.h>

void dashboard_start(void);
void dashboard_update_controller(int16_t roll_trim, int16_t pitch_trim,
    int16_t yaw_trim, uint16_t throttle, uint8_t sensitivity, bool armed);
void dashboard_update_flight_command(uint16_t throttle, bool armed);
void dashboard_note_delivery(void);
void dashboard_update_battery(uint16_t voltage_mv, uint8_t remaining_percent);
