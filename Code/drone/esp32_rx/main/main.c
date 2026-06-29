#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/ledc.h"
#include "driver/uart.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "nvs_flash.h"

#define WIFI_CHANNEL 6

#define CRSF_UART UART_NUM_2
#define CRSF_TX_PIN GPIO_NUM_12
#define CRSF_RX_PIN GPIO_NUM_14
#define CRSF_BAUD_RATE 420000
#define CRSF_PERIOD_US 6667
#define CONTROL_TIMEOUT_US 500000
#define BEGINNER_ROLL_PITCH_LIMIT 600
#define BEGINNER_YAW_LIMIT 350
#define BEGINNER_THROTTLE_LIMIT 600

#define SERVO_COUNT 4
#define SERVO_CENTER_US 1500
#define SERVO_MIN_US 500
#define SERVO_MAX_US 2500
#define SERVO_PWM_FREQUENCY_HZ 50
#define SERVO_PWM_PERIOD_US 20000
#define SERVO_PWM_TIMER LEDC_TIMER_0
#define SERVO_PWM_MODE LEDC_HIGH_SPEED_MODE
#define SERVO_PWM_RESOLUTION LEDC_TIMER_16_BIT
#define SERVO_RAMP_PERIOD_MS 20
#define SERVO_RAMP_MIN_US_S 10
#define SERVO_RAMP_MAX_US_S 500
#define SERVO_RAMP_DEFAULT_US_S 250
#define SERVO_COMMAND_TIMEOUT_US 1500000
#define SERVO_FLAG_SAVE_DEFAULTS (1U << 0)
#define SERVO_NVS_NAMESPACE "servo"
#define SERVO_NVS_KEY "config"

#define CONTROL_PAYLOAD_SIZE 16
#define RADIO_PACKET_MAGIC 0x52
#define RADIO_PACKET_VERSION 1
#define SERVO_PACKET_MAGIC 0x53
#define TELEMETRY_PACKET_MAGIC 0x55
#define TELEMETRY_SEND_PERIOD_US 500000

#define CONTROL_FLAG_ARM (1U << 0)
#define CONTROL_FLAG_ESTOP (1U << 1)
#define CONTROL_FLAG_CLEAR_ESTOP (1U << 2)

#define CRSF_ADDRESS_FLIGHT_CONTROLLER 0xC8
#define CRSF_FRAME_LENGTH 24
#define CRSF_FRAME_TYPE_RC_CHANNELS_PACKED 0x16
#define CRSF_FRAME_TYPE_BATTERY_SENSOR 0x08
#define CRSF_CHANNEL_MIN 172
#define CRSF_CHANNEL_MID 992
#define CRSF_CHANNEL_MAX 1811
#define CRSF_CHANNEL_COUNT 16
#define CRSF_PAYLOAD_SIZE 22

static const char *TAG = "esp32_espnow_crsf_rx";
static const uint8_t controller_mac[ESP_NOW_ETH_ALEN] = {
    0x68, 0x09, 0x47, 0x5c, 0x04, 0xc4,
};

static const gpio_num_t servo_gpio[SERVO_COUNT] = {
    GPIO_NUM_16,
    GPIO_NUM_17,
    GPIO_NUM_5,
    GPIO_NUM_18,
};

static const ledc_channel_t servo_channel[SERVO_COUNT] = {
    LEDC_CHANNEL_0,
    LEDC_CHANNEL_1,
    LEDC_CHANNEL_2,
    LEDC_CHANNEL_3,
};

typedef struct {
    int16_t roll;
    int16_t pitch;
    int16_t throttle;
    int16_t yaw;
    uint16_t sequence;
    int64_t last_update_us;
    bool arm_requested;
    bool emergency_latched;
} control_state_t;

typedef struct {
    int16_t roll;
    int16_t pitch;
    int16_t throttle;
    int16_t yaw;
    uint16_t sequence;
    int64_t age_us;
    bool arm_requested;
    bool emergency_latched;
    bool link_active;
} control_snapshot_t;

typedef struct {
    uint8_t data[CONTROL_PAYLOAD_SIZE];
} espnow_control_packet_t;

typedef struct {
    uint16_t values[SERVO_COUNT];
    uint16_t ramp_speed_us_s;
} servo_config_t;

static portMUX_TYPE control_mux = portMUX_INITIALIZER_UNLOCKED;
static portMUX_TYPE servo_mux = portMUX_INITIALIZER_UNLOCKED;
static control_state_t control_state = {
    .emergency_latched = true,
};
static uint16_t servo_current[SERVO_COUNT] = {
    SERVO_CENTER_US, SERVO_CENTER_US, SERVO_CENTER_US, SERVO_CENTER_US,
};
static uint16_t servo_target[SERVO_COUNT] = {
    SERVO_CENTER_US, SERVO_CENTER_US, SERVO_CENTER_US, SERVO_CENTER_US,
};
static uint16_t servo_defaults[SERVO_COUNT] = {
    SERVO_CENTER_US, SERVO_CENTER_US, SERVO_CENTER_US, SERVO_CENTER_US,
};
static uint16_t servo_ramp_speed_us_s = SERVO_RAMP_DEFAULT_US_S;
static uint16_t servo_default_ramp_speed_us_s = SERVO_RAMP_DEFAULT_US_S;
static int64_t last_servo_command_us;
static TaskHandle_t crsf_task_handle;
static QueueHandle_t espnow_queue;
static volatile bool telemetry_peer_ready;
static uint16_t telemetry_sequence;
static int64_t last_telemetry_send_us;

static int16_t clamp_i16(int16_t value, int16_t minimum, int16_t maximum)
{
    if (value < minimum) {
        return minimum;
    }
    if (value > maximum) {
        return maximum;
    }
    return value;
}

static uint16_t read_u16_le(const uint8_t *data)
{
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static void write_u16_le(uint8_t *data, uint16_t value)
{
    data[0] = (uint8_t)(value & 0xFFU);
    data[1] = (uint8_t)(value >> 8);
}

static int16_t read_i16_le(const uint8_t *data)
{
    return (int16_t)read_u16_le(data);
}

static uint16_t crc16_ccitt(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= (uint16_t)data[i] << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000U) ? (uint16_t)((crc << 1) ^ 0x1021U) : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

static uint8_t crc8_dvb_s2(const uint8_t *data, size_t length)
{
    uint8_t crc = 0;

    for (size_t i = 0; i < length; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80U) ? (uint8_t)((crc << 1) ^ 0xD5U) : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

static uint32_t servo_pulse_to_duty(uint16_t pulse_us)
{
    return ((uint32_t)pulse_us * ((1U << 16) - 1U)) / SERVO_PWM_PERIOD_US;
}

static bool valid_servo_values(const uint16_t values[SERVO_COUNT])
{
    for (size_t i = 0; i < SERVO_COUNT; ++i) {
        if (values[i] < SERVO_MIN_US || values[i] > SERVO_MAX_US) {
            return false;
        }
    }
    return true;
}

static bool valid_ramp_speed(uint16_t ramp_speed_us_s)
{
    return ramp_speed_us_s >= SERVO_RAMP_MIN_US_S
        && ramp_speed_us_s <= SERVO_RAMP_MAX_US_S;
}

static void load_servo_defaults(void)
{
    nvs_handle_t handle;
    if (nvs_open(SERVO_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return;
    }
    servo_config_t config;
    size_t length = sizeof(config);
    if (nvs_get_blob(handle, SERVO_NVS_KEY, &config, &length) == ESP_OK
        && length == sizeof(config) && valid_servo_values(config.values)) {
        const uint16_t stretch_us = valid_ramp_speed(config.ramp_speed_us_s)
            ? config.ramp_speed_us_s : SERVO_RAMP_DEFAULT_US_S;
        memcpy(servo_defaults, config.values, sizeof(config.values));
        memcpy(servo_current, config.values, sizeof(config.values));
        memcpy(servo_target, config.values, sizeof(config.values));
        servo_ramp_speed_us_s = stretch_us;
        servo_default_ramp_speed_us_s = stretch_us;
    }
    nvs_close(handle);
}

static esp_err_t save_servo_defaults(const uint16_t values[SERVO_COUNT], uint16_t ramp_speed_us_s)
{
    if (memcmp(values, servo_defaults, sizeof(servo_defaults)) == 0
        && ramp_speed_us_s == servo_default_ramp_speed_us_s) {
        return ESP_OK;
    }
    nvs_handle_t handle;
    esp_err_t result = nvs_open(SERVO_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (result != ESP_OK) {
        return result;
    }
    const servo_config_t config = {
        .values = {values[0], values[1], values[2], values[3]},
        .ramp_speed_us_s = ramp_speed_us_s,
    };
    result = nvs_set_blob(handle, SERVO_NVS_KEY, &config, sizeof(config));
    if (result == ESP_OK) {
        result = nvs_commit(handle);
    }
    nvs_close(handle);
    if (result == ESP_OK) {
        portENTER_CRITICAL(&servo_mux);
        memcpy(servo_defaults, values, sizeof(servo_defaults));
        servo_default_ramp_speed_us_s = ramp_speed_us_s;
        portEXIT_CRITICAL(&servo_mux);
    }
    return result;
}

static void servo_ramp_task(void *argument)
{
    (void)argument;
    while (true) {
        uint16_t outputs[SERVO_COUNT];
        bool changed = false;
        const int64_t now = esp_timer_get_time();
        portENTER_CRITICAL(&servo_mux);
        if (last_servo_command_us != 0
            && now - last_servo_command_us > SERVO_COMMAND_TIMEOUT_US) {
            for (size_t i = 0; i < SERVO_COUNT; ++i) {
                if (servo_current[i] != servo_defaults[i]) {
                    changed = true;
                }
                servo_current[i] = servo_defaults[i];
                servo_target[i] = servo_defaults[i];
            }
            last_servo_command_us = 0;
        }
        for (size_t i = 0; i < SERVO_COUNT; ++i) {
            outputs[i] = servo_current[i];
        }
        portEXIT_CRITICAL(&servo_mux);

        if (changed) {
            for (size_t i = 0; i < SERVO_COUNT; ++i) {
                ESP_ERROR_CHECK(ledc_set_duty(SERVO_PWM_MODE, servo_channel[i],
                    servo_pulse_to_duty(outputs[i])));
                ESP_ERROR_CHECK(ledc_update_duty(SERVO_PWM_MODE, servo_channel[i]));
            }
        }
        vTaskDelay(pdMS_TO_TICKS(SERVO_RAMP_PERIOD_MS));
    }
}

static void start_servo_outputs(void)
{
    load_servo_defaults();
    const ledc_timer_config_t timer_config = {
        .speed_mode = SERVO_PWM_MODE,
        .duty_resolution = SERVO_PWM_RESOLUTION,
        .timer_num = SERVO_PWM_TIMER,
        .freq_hz = SERVO_PWM_FREQUENCY_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

    for (size_t i = 0; i < SERVO_COUNT; ++i) {
        const ledc_channel_config_t channel_config = {
            .gpio_num = servo_gpio[i],
            .speed_mode = SERVO_PWM_MODE,
            .channel = servo_channel[i],
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = SERVO_PWM_TIMER,
            .duty = servo_pulse_to_duty(servo_current[i]),
            .hpoint = 0,
        };
        ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
    }

    BaseType_t created = xTaskCreatePinnedToCore(servo_ramp_task, "servo_ramp",
        3072, NULL, 7, NULL, 1);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
    ESP_LOGI(TAG, "Servo PWM GPIO16/17/5/18 defaults=%u,%u,%u,%u us ramp=%u us/s",
        servo_current[0], servo_current[1], servo_current[2], servo_current[3],
        servo_ramp_speed_us_s);
}

static bool apply_servo_packet(const uint8_t packet[CONTROL_PAYLOAD_SIZE])
{
    if (packet[0] != SERVO_PACKET_MAGIC || packet[15] != RADIO_PACKET_VERSION
        || (packet[1] & ~SERVO_FLAG_SAVE_DEFAULTS) != 0) {
        return false;
    }
    if (read_u16_le(&packet[12]) != crc16_ccitt(packet, 12)) {
        return false;
    }

    uint16_t values[SERVO_COUNT];
    const uint16_t ramp_speed_us_s = read_u16_le(&packet[2]);
    if (!valid_ramp_speed(ramp_speed_us_s)) {
        return false;
    }
    for (size_t i = 0; i < SERVO_COUNT; ++i) {
        values[i] = read_u16_le(&packet[4 + (i * 2)]);
        if (values[i] < SERVO_MIN_US || values[i] > SERVO_MAX_US) {
            return false;
        }
    }

    portENTER_CRITICAL(&servo_mux);
    memcpy(servo_target, values, sizeof(values));
    memcpy(servo_current, values, sizeof(values));
    servo_ramp_speed_us_s = ramp_speed_us_s;
    last_servo_command_us = esp_timer_get_time();
    portEXIT_CRITICAL(&servo_mux);

    for (size_t i = 0; i < SERVO_COUNT; ++i) {
        ESP_ERROR_CHECK(ledc_set_duty(SERVO_PWM_MODE, servo_channel[i],
            servo_pulse_to_duty(values[i])));
        ESP_ERROR_CHECK(ledc_update_duty(SERVO_PWM_MODE, servo_channel[i]));
    }

    const bool save_defaults = (packet[1] & SERVO_FLAG_SAVE_DEFAULTS) != 0;
    if (save_defaults) {
        const esp_err_t result = save_servo_defaults(values, ramp_speed_us_s);
        if (result != ESP_OK) {
            ESP_LOGE(TAG, "failed saving servo defaults: %s", esp_err_to_name(result));
            return false;
        }
    }
    ESP_LOGI(TAG, "continuous servo PWM=%u,%u,%u,%u us stretch=%u us save=%d",
        values[0], values[1], values[2], values[3], ramp_speed_us_s, save_defaults);
    return true;
}

static void force_emergency_stop(void)
{
    portENTER_CRITICAL(&control_mux);
    control_state.roll = 0;
    control_state.pitch = 0;
    control_state.throttle = 0;
    control_state.yaw = 0;
    control_state.arm_requested = false;
    control_state.emergency_latched = true;
    portEXIT_CRITICAL(&control_mux);
}

static control_snapshot_t get_control_snapshot(void)
{
    control_snapshot_t snapshot;
    const int64_t now = esp_timer_get_time();

    portENTER_CRITICAL(&control_mux);
    snapshot.roll = control_state.roll;
    snapshot.pitch = control_state.pitch;
    snapshot.throttle = control_state.throttle;
    snapshot.yaw = control_state.yaw;
    snapshot.sequence = control_state.sequence;
    snapshot.arm_requested = control_state.arm_requested;
    snapshot.emergency_latched = control_state.emergency_latched;
    snapshot.age_us = control_state.last_update_us == 0
        ? INT64_MAX
        : now - control_state.last_update_us;
    snapshot.link_active = control_state.last_update_us != 0
        && snapshot.age_us <= CONTROL_TIMEOUT_US
        && !control_state.emergency_latched;
    portEXIT_CRITICAL(&control_mux);

    return snapshot;
}

static uint16_t axis_to_crsf(int16_t value)
{
    value = clamp_i16(value, -1000, 1000);
    if (value >= 0) {
        return (uint16_t)(CRSF_CHANNEL_MID
            + ((int32_t)value * (CRSF_CHANNEL_MAX - CRSF_CHANNEL_MID)) / 1000);
    }
    return (uint16_t)(CRSF_CHANNEL_MID
        + ((int32_t)value * (CRSF_CHANNEL_MID - CRSF_CHANNEL_MIN)) / 1000);
}

static uint16_t throttle_to_crsf(int16_t value)
{
    value = clamp_i16(value, 0, 1000);
    return (uint16_t)(CRSF_CHANNEL_MIN
        + ((int32_t)value * (CRSF_CHANNEL_MAX - CRSF_CHANNEL_MIN)) / 1000);
}

static void pack_crsf_channels(const uint16_t channels[CRSF_CHANNEL_COUNT],
    uint8_t payload[CRSF_PAYLOAD_SIZE])
{
    uint32_t accumulator = 0;
    unsigned accumulator_bits = 0;
    size_t output_index = 0;

    memset(payload, 0, CRSF_PAYLOAD_SIZE);
    for (size_t channel = 0; channel < CRSF_CHANNEL_COUNT; ++channel) {
        accumulator |= ((uint32_t)channels[channel] & 0x7FFU) << accumulator_bits;
        accumulator_bits += 11;

        while (accumulator_bits >= 8) {
            payload[output_index++] = (uint8_t)(accumulator & 0xFFU);
            accumulator >>= 8;
            accumulator_bits -= 8;
        }
    }
}

static void build_crsf_frame(const control_snapshot_t *control, uint8_t frame[26])
{
    uint16_t channels[CRSF_CHANNEL_COUNT];
    const bool safe = control->link_active;
    const int16_t roll = clamp_i16(control->roll,
        -BEGINNER_ROLL_PITCH_LIMIT, BEGINNER_ROLL_PITCH_LIMIT);
    const int16_t pitch = clamp_i16(control->pitch,
        -BEGINNER_ROLL_PITCH_LIMIT, BEGINNER_ROLL_PITCH_LIMIT);
    const int16_t yaw = clamp_i16(control->yaw,
        -BEGINNER_YAW_LIMIT, BEGINNER_YAW_LIMIT);
    const int16_t throttle = clamp_i16(control->throttle,
        0, BEGINNER_THROTTLE_LIMIT);

    for (size_t i = 0; i < CRSF_CHANNEL_COUNT; ++i) {
        channels[i] = CRSF_CHANNEL_MID;
    }

    channels[0] = axis_to_crsf(safe ? roll : 0);
    channels[1] = axis_to_crsf(safe ? pitch : 0);
    channels[2] = throttle_to_crsf(safe ? throttle : 0);
    channels[3] = axis_to_crsf(safe ? yaw : 0);
    channels[4] = safe && control->arm_requested ? CRSF_CHANNEL_MAX : CRSF_CHANNEL_MIN;
    channels[5] = safe ? CRSF_CHANNEL_MAX : CRSF_CHANNEL_MIN;
    for (size_t i = 6; i < CRSF_CHANNEL_COUNT; ++i) {
        channels[i] = CRSF_CHANNEL_MIN;
    }

    frame[0] = CRSF_ADDRESS_FLIGHT_CONTROLLER;
    frame[1] = CRSF_FRAME_LENGTH;
    frame[2] = CRSF_FRAME_TYPE_RC_CHANNELS_PACKED;
    pack_crsf_channels(channels, &frame[3]);
    frame[25] = crc8_dvb_s2(&frame[2], 1 + CRSF_PAYLOAD_SIZE);
}

static void crsf_timer_callback(void *argument)
{
    (void)argument;
    if (crsf_task_handle != NULL) {
        xTaskNotifyGive(crsf_task_handle);
    }
}

static void crsf_output_task(void *argument)
{
    (void)argument;
    uint8_t frame[26];
    bool timeout_logged = false;

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        control_snapshot_t snapshot = get_control_snapshot();

        if (!snapshot.link_active
            && snapshot.age_us > CONTROL_TIMEOUT_US
            && !snapshot.emergency_latched) {
            ESP_LOGW(TAG, "control timeout: age_us=%lld seq=%u",
                (long long)snapshot.age_us, snapshot.sequence);
            force_emergency_stop();
            timeout_logged = true;
            snapshot = get_control_snapshot();
        } else if (snapshot.link_active) {
            timeout_logged = false;
        } else if (!timeout_logged && snapshot.age_us > CONTROL_TIMEOUT_US) {
            timeout_logged = true;
        }

        build_crsf_frame(&snapshot, frame);
        const int written = uart_write_bytes(CRSF_UART, frame, sizeof(frame));
        if (written != sizeof(frame)) {
            ESP_LOGE(TAG, "CRSF UART write failed: %d", written);
            force_emergency_stop();
        }
    }
}

static void crsf_telemetry_task(void *argument)
{
    (void)argument;
    uint8_t telemetry[128];
    uint8_t frame[64];
    size_t frame_offset = 0;
    size_t expected_length = 0;

    while (true) {
        const int count = uart_read_bytes(
            CRSF_UART, telemetry, sizeof(telemetry), pdMS_TO_TICKS(20));
        for (int index = 0; index < count; ++index) {
            const uint8_t byte = telemetry[index];
            if (frame_offset == 0) {
                frame[frame_offset++] = byte;
                continue;
            }
            if (frame_offset == 1) {
                if (byte < 2 || byte > sizeof(frame) - 2) {
                    frame_offset = 0;
                    expected_length = 0;
                    continue;
                }
                frame[frame_offset++] = byte;
                expected_length = (size_t)byte + 2;
                continue;
            }

            frame[frame_offset++] = byte;
            if (frame_offset != expected_length) {
                continue;
            }

            const uint8_t crsf_length = frame[1];
            const uint8_t expected_crc = frame[expected_length - 1];
            const uint8_t actual_crc = crc8_dvb_s2(&frame[2], crsf_length - 1);
            if (expected_crc == actual_crc
                && frame[2] == CRSF_FRAME_TYPE_BATTERY_SENSOR
                && crsf_length >= 10) {
                const uint16_t voltage_decivolts =
                    ((uint16_t)frame[3] << 8) | frame[4];
                const uint16_t voltage_mv = voltage_decivolts * 100U;
                const uint8_t remaining_percent = frame[10];
                const int64_t now = esp_timer_get_time();

                if (telemetry_peer_ready && voltage_mv > 0
                    && now - last_telemetry_send_us >= TELEMETRY_SEND_PERIOD_US) {
                    uint8_t packet[CONTROL_PAYLOAD_SIZE] = {0};
                    packet[0] = TELEMETRY_PACKET_MAGIC;
                    packet[1] = RADIO_PACKET_VERSION;
                    write_u16_le(&packet[2], voltage_mv);
                    packet[4] = remaining_percent;
                    packet[5] = 1;
                    write_u16_le(&packet[6], ++telemetry_sequence);
                    write_u16_le(&packet[12], crc16_ccitt(packet, 12));
                    packet[15] = RADIO_PACKET_VERSION;
                    if (esp_now_send(controller_mac, packet, sizeof(packet)) == ESP_OK) {
                        last_telemetry_send_us = now;
                    }
                }
            }

            frame_offset = 0;
            expected_length = 0;
        }
    }
}

static void start_crsf(void)
{
    const uart_config_t uart_config = {
        .baud_rate = CRSF_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(CRSF_UART, 2048, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(CRSF_UART, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(CRSF_UART, CRSF_TX_PIN, CRSF_RX_PIN,
        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    BaseType_t created = xTaskCreatePinnedToCore(crsf_output_task, "crsf_output", 4096,
        NULL, 18, &crsf_task_handle, 1);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    created = xTaskCreatePinnedToCore(crsf_telemetry_task, "crsf_telemetry", 3072,
        NULL, 8, NULL, 1);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);

    const esp_timer_create_args_t timer_args = {
        .callback = crsf_timer_callback,
        .name = "crsf_150hz",
        .skip_unhandled_events = true,
    };
    esp_timer_handle_t timer;
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, CRSF_PERIOD_US));

    ESP_LOGI(TAG, "CRSF started: GPIO%d TX -> FC RX3, GPIO%d RX <- FC TX3",
        CRSF_TX_PIN, CRSF_RX_PIN);
}

static bool apply_control_packet(const uint8_t packet[CONTROL_PAYLOAD_SIZE])
{
    if (packet[0] != RADIO_PACKET_MAGIC || packet[15] != RADIO_PACKET_VERSION) {
        return false;
    }

    const uint16_t expected_crc = read_u16_le(&packet[12]);
    const uint16_t actual_crc = crc16_ccitt(packet, 12);
    if (expected_crc != actual_crc) {
        return false;
    }

    const uint8_t flags = packet[1];
    const uint16_t sequence = read_u16_le(&packet[2]);
    const int16_t roll = read_i16_le(&packet[4]);
    const int16_t pitch = read_i16_le(&packet[6]);
    const int16_t throttle = read_i16_le(&packet[8]);
    const int16_t yaw = read_i16_le(&packet[10]);
    bool logged_estop = false;
    bool logged_clear_estop = false;
    bool logged_arm_change = false;
    bool arm_output = false;

    if (roll < -1000 || roll > 1000
        || pitch < -1000 || pitch > 1000
        || throttle < 0 || throttle > 1000
        || yaw < -1000 || yaw > 1000) {
        ESP_LOGW(TAG, "invalid control range seq=%u r=%d p=%d t=%d y=%d",
            sequence, roll, pitch, throttle, yaw);
        return false;
    }

    portENTER_CRITICAL(&control_mux);
    control_state.last_update_us = esp_timer_get_time();
    control_state.sequence = sequence;

    if ((flags & CONTROL_FLAG_ESTOP) != 0) {
        logged_estop = !control_state.emergency_latched;
        control_state.emergency_latched = true;
        control_state.arm_requested = false;
        control_state.roll = 0;
        control_state.pitch = 0;
        control_state.throttle = 0;
        control_state.yaw = 0;
    } else {
        if ((flags & CONTROL_FLAG_CLEAR_ESTOP) != 0
            && throttle == 0
            && (flags & CONTROL_FLAG_ARM) == 0) {
            logged_clear_estop = control_state.emergency_latched;
            control_state.emergency_latched = false;
        }

        if (!control_state.emergency_latched) {
            const bool next_arm = (flags & CONTROL_FLAG_ARM) != 0;
            logged_arm_change = control_state.arm_requested != next_arm;
            control_state.roll = roll;
            control_state.pitch = pitch;
            control_state.throttle = throttle;
            control_state.yaw = yaw;
            control_state.arm_requested = next_arm;
            arm_output = control_state.arm_requested;
        }
    }
    portEXIT_CRITICAL(&control_mux);

    if (logged_estop) {
        ESP_LOGW(TAG, "espnow_estop seq=%u flags=0x%02x", sequence, flags);
    }
    if (logged_clear_estop) {
        ESP_LOGW(TAG, "espnow_clear_estop seq=%u flags=0x%02x", sequence, flags);
    }
    if (logged_arm_change) {
        ESP_LOGW(TAG, "espnow_arm_change seq=%u arm=%d", sequence, arm_output);
    }

    return true;
}

static void espnow_recv_callback(const esp_now_recv_info_t *info, const uint8_t *data, int length)
{
    (void)info;

    if (length != CONTROL_PAYLOAD_SIZE || espnow_queue == NULL) {
        return;
    }

    espnow_control_packet_t packet;
    memcpy(packet.data, data, sizeof(packet.data));
    (void)xQueueSend(espnow_queue, &packet, 0);
}

static void espnow_rx_task(void *argument)
{
    (void)argument;
    espnow_control_packet_t packet;
    uint32_t received = 0;
    uint32_t invalid = 0;
    uint32_t servo_received = 0;

    while (true) {
        if (xQueueReceive(espnow_queue, &packet, portMAX_DELAY) == pdTRUE) {
            if (packet.data[0] == SERVO_PACKET_MAGIC && apply_servo_packet(packet.data)) {
                servo_received++;
            } else if (apply_control_packet(packet.data)) {
                received++;
                if ((received % 50U) == 0) {
                    const control_snapshot_t snapshot = get_control_snapshot();
                    ESP_LOGI(TAG, "espnow rx=%lu seq=%u age_us=%lld arm=%d t=%d r=%d p=%d y=%d",
                        (unsigned long)received, snapshot.sequence,
                        (long long)snapshot.age_us, snapshot.arm_requested,
                        snapshot.throttle, snapshot.roll, snapshot.pitch, snapshot.yaw);
                }
            } else {
                invalid++;
                if ((invalid % 10U) == 1) {
                    ESP_LOGW(TAG, "invalid ESP-NOW payload count=%lu servo_rx=%lu",
                        (unsigned long)invalid, (unsigned long)servo_received);
                }
            }
        }
    }
}

static void start_wifi_espnow(void)
{
    uint8_t sta_mac[6];

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_LR));
    ESP_ERROR_CHECK(esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_callback));

    esp_now_peer_info_t peer = {
        .channel = WIFI_CHANNEL,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, controller_mac, sizeof(controller_mac));
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    esp_now_rate_config_t rate_config = {
        .phymode = WIFI_PHY_MODE_LR,
        .rate = WIFI_PHY_RATE_LORA_250K,
        .ersu = false,
        .dcm = false,
    };
    ESP_ERROR_CHECK(esp_now_set_peer_rate_config(controller_mac, &rate_config));
    telemetry_peer_ready = true;

    ESP_ERROR_CHECK(esp_read_mac(sta_mac, ESP_MAC_WIFI_STA));
    ESP_LOGI(TAG, "ESP-NOW LR RX ready channel=%d STA MAC=%02X:%02X:%02X:%02X:%02X:%02X telemetry TX=%02X:%02X:%02X:%02X:%02X:%02X",
        WIFI_CHANNEL,
        sta_mac[0], sta_mac[1], sta_mac[2], sta_mac[3], sta_mac[4], sta_mac[5],
        controller_mac[0], controller_mac[1], controller_mac[2],
        controller_mac[3], controller_mac[4], controller_mac[5]);

    espnow_queue = xQueueCreate(32, sizeof(espnow_control_packet_t));
    ESP_ERROR_CHECK(espnow_queue != NULL ? ESP_OK : ESP_ERR_NO_MEM);

    BaseType_t created = xTaskCreatePinnedToCore(espnow_rx_task, "espnow_rx",
        4096, NULL, 17, NULL, 0);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

void app_main(void)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);

    ESP_LOGI(TAG, "boot ESP-NOW RX -> CRSF; espnow core=0, crsf core=1");
    start_servo_outputs();
    start_crsf();
    start_wifi_espnow();
}
