#include "dashboard.h"

#include <stdio.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define TFT_HOST SPI2_HOST
#define TFT_PIN_CS GPIO_NUM_15
#define TFT_PIN_DC GPIO_NUM_2
#define TFT_PIN_RST GPIO_NUM_4
#define TFT_PIN_SCLK GPIO_NUM_18
#define TFT_PIN_MOSI GPIO_NUM_23
#define TFT_PIN_MISO GPIO_NUM_19
#define TOUCH_PIN_CS GPIO_NUM_21
#define TOUCH_PIN_IRQ GPIO_NUM_22

#define TFT_WIDTH 320
#define TFT_HEIGHT 240
#define LINK_TIMEOUT_US 1500000
#define BATTERY_TIMEOUT_US 5000000

#define COLOR_BG 0x0861
#define COLOR_HEADER 0x032C
#define COLOR_PANEL 0x10C3
#define COLOR_PANEL_ALT 0x1945
#define COLOR_WHITE 0xFFFF
#define COLOR_MUTED 0x9CF3
#define COLOR_GREEN 0x07E0
#define COLOR_RED 0xF800
#define COLOR_AMBER 0xFD20
#define COLOR_CYAN 0x07FF

typedef struct {
    int16_t roll_trim;
    int16_t pitch_trim;
    int16_t yaw_trim;
    uint16_t throttle;
    uint16_t battery_mv;
    uint8_t sensitivity;
    uint8_t battery_percent;
    int64_t last_delivery_us;
    int64_t last_battery_us;
    bool armed;
} dashboard_source_t;

typedef struct {
    int16_t roll_trim;
    int16_t pitch_trim;
    int16_t yaw_trim;
    uint16_t throttle;
    uint16_t battery_mv;
    uint8_t sensitivity;
    uint8_t battery_percent;
    bool armed;
    bool connected;
    bool battery_valid;
} dashboard_view_t;

static const char *TAG = "dashboard";
static spi_device_handle_t tft_device;
static spi_device_handle_t touch_device;
static portMUX_TYPE state_mux = portMUX_INITIALIZER_UNLOCKED;
static dashboard_source_t source_state = {
    .sensitivity = 50,
};

static const uint8_t font_rows[][7] = {
    {0, 0, 0, 0, 0, 0, 0},                    // space
    {14, 17, 19, 21, 25, 17, 14},             // 0
    {4, 12, 4, 4, 4, 4, 14},                  // 1
    {14, 17, 1, 2, 4, 8, 31},                 // 2
    {30, 1, 1, 14, 1, 1, 30},                 // 3
    {2, 6, 10, 18, 31, 2, 2},                 // 4
    {31, 16, 16, 30, 1, 1, 30},               // 5
    {14, 16, 16, 30, 17, 17, 14},             // 6
    {31, 1, 2, 4, 8, 8, 8},                   // 7
    {14, 17, 17, 14, 17, 17, 14},             // 8
    {14, 17, 17, 15, 1, 1, 14},               // 9
    {14, 17, 17, 31, 17, 17, 17},             // A
    {30, 17, 17, 30, 17, 17, 30},             // B
    {14, 17, 16, 16, 16, 17, 14},             // C
    {30, 17, 17, 17, 17, 17, 30},             // D
    {31, 16, 16, 30, 16, 16, 31},             // E
    {31, 16, 16, 30, 16, 16, 16},             // F
    {14, 17, 16, 23, 17, 17, 15},             // G
    {17, 17, 17, 31, 17, 17, 17},             // H
    {14, 4, 4, 4, 4, 4, 14},                  // I
    {7, 2, 2, 2, 2, 18, 12},                  // J
    {17, 18, 20, 24, 20, 18, 17},             // K
    {16, 16, 16, 16, 16, 16, 31},             // L
    {17, 27, 21, 21, 17, 17, 17},             // M
    {17, 25, 21, 19, 17, 17, 17},             // N
    {14, 17, 17, 17, 17, 17, 14},             // O
    {30, 17, 17, 30, 16, 16, 16},             // P
    {14, 17, 17, 17, 21, 18, 13},             // Q
    {30, 17, 17, 30, 20, 18, 17},             // R
    {15, 16, 16, 14, 1, 1, 30},               // S
    {31, 4, 4, 4, 4, 4, 4},                   // T
    {17, 17, 17, 17, 17, 17, 14},             // U
    {17, 17, 17, 17, 17, 10, 4},              // V
    {17, 17, 17, 21, 21, 21, 10},             // W
    {17, 17, 10, 4, 10, 17, 17},              // X
    {17, 17, 10, 4, 4, 4, 4},                 // Y
    {31, 1, 2, 4, 8, 16, 31},                 // Z
    {4, 4, 31, 4, 4, 0, 0},                   // +
    {0, 0, 0, 31, 0, 0, 0},                   // -
    {0, 0, 0, 0, 0, 12, 12},                  // .
    {17, 2, 4, 8, 17, 0, 0},                  // %
    {0, 12, 12, 0, 12, 12, 0},                // :
    {1, 2, 4, 8, 16, 0, 0},                   // /
};

static size_t glyph_index(char character)
{
    if (character >= '0' && character <= '9') {
        return 1U + (size_t)(character - '0');
    }
    if (character >= 'A' && character <= 'Z') {
        return 11U + (size_t)(character - 'A');
    }
    switch (character) {
    case '+': return 37;
    case '-': return 38;
    case '.': return 39;
    case '%': return 40;
    case ':': return 41;
    case '/': return 42;
    default: return 0;
    }
}

static void tft_write(bool data_mode, const void *data, size_t length)
{
    gpio_set_level(TFT_PIN_DC, data_mode ? 1 : 0);
    spi_transaction_t transaction = {
        .length = length * 8,
        .tx_buffer = data,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(tft_device, &transaction));
}

static void tft_command(uint8_t command)
{
    tft_write(false, &command, 1);
}

static void tft_data(const void *data, size_t length)
{
    tft_write(true, data, length);
}

static void tft_command_data(uint8_t command, const uint8_t *data, size_t length)
{
    tft_command(command);
    if (length > 0) {
        tft_data(data, length);
    }
}

static void set_window(int x0, int y0, int x1, int y1)
{
    const uint8_t columns[] = {
        (uint8_t)(x0 >> 8), (uint8_t)x0, (uint8_t)(x1 >> 8), (uint8_t)x1,
    };
    const uint8_t rows[] = {
        (uint8_t)(y0 >> 8), (uint8_t)y0, (uint8_t)(y1 >> 8), (uint8_t)y1,
    };
    tft_command_data(0x2A, columns, sizeof(columns));
    tft_command_data(0x2B, rows, sizeof(rows));
    tft_command(0x2C);
}

static void fill_rect(int x, int y, int width, int height, uint16_t color)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    uint8_t pixels[256];
    for (size_t i = 0; i < sizeof(pixels); i += 2) {
        pixels[i] = (uint8_t)(color >> 8);
        pixels[i + 1] = (uint8_t)color;
    }
    set_window(x, y, x + width - 1, y + height - 1);
    size_t remaining = (size_t)width * height * 2;
    while (remaining > 0) {
        const size_t amount = remaining > sizeof(pixels) ? sizeof(pixels) : remaining;
        tft_data(pixels, amount);
        remaining -= amount;
    }
}

static void draw_text(int x, int y, const char *text, int scale,
    uint16_t foreground, uint16_t background)
{
    const size_t length = strlen(text);
    const int width = (int)length * 6 * scale;
    if (length == 0 || width <= 0 || width > TFT_WIDTH || y + 7 * scale > TFT_HEIGHT) {
        return;
    }

    uint8_t line[TFT_WIDTH * 2];
    for (int glyph_row = 0; glyph_row < 7; ++glyph_row) {
        int output_x = 0;
        for (size_t character = 0; character < length; ++character) {
            const uint8_t bits = font_rows[glyph_index(text[character])][glyph_row];
            for (int column = 0; column < 6; ++column) {
                const uint16_t color = column < 5 && (bits & (1U << (4 - column)))
                    ? foreground : background;
                for (int repeat = 0; repeat < scale; ++repeat) {
                    line[output_x * 2] = (uint8_t)(color >> 8);
                    line[output_x * 2 + 1] = (uint8_t)color;
                    output_x++;
                }
            }
        }
        for (int row_repeat = 0; row_repeat < scale; ++row_repeat) {
            set_window(x, y + glyph_row * scale + row_repeat,
                x + width - 1, y + glyph_row * scale + row_repeat);
            tft_data(line, (size_t)width * 2);
        }
    }
}

static void initialize_display(void)
{
    const spi_bus_config_t bus_config = {
        .mosi_io_num = TFT_PIN_MOSI,
        .miso_io_num = TFT_PIN_MISO,
        .sclk_io_num = TFT_PIN_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = TFT_WIDTH * 2,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(TFT_HOST, &bus_config, SPI_DMA_CH_AUTO));

    const spi_device_interface_config_t tft_config = {
        .clock_speed_hz = 24000000,
        .mode = 0,
        .spics_io_num = TFT_PIN_CS,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(TFT_HOST, &tft_config, &tft_device));

    const spi_device_interface_config_t touch_config = {
        .clock_speed_hz = 2000000,
        .mode = 0,
        .spics_io_num = TOUCH_PIN_CS,
        .queue_size = 1,
    };
    ESP_ERROR_CHECK(spi_bus_add_device(TFT_HOST, &touch_config, &touch_device));

    gpio_config_t outputs = {
        .pin_bit_mask = (1ULL << TFT_PIN_DC) | (1ULL << TFT_PIN_RST),
        .mode = GPIO_MODE_OUTPUT,
    };
    ESP_ERROR_CHECK(gpio_config(&outputs));
    gpio_config_t touch_irq = {
        .pin_bit_mask = 1ULL << TOUCH_PIN_IRQ,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&touch_irq));

    gpio_set_level(TFT_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TFT_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(20));
    gpio_set_level(TFT_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(120));

    tft_command(0x01);
    vTaskDelay(pdMS_TO_TICKS(120));
    const uint8_t power1[] = {0x23};
    const uint8_t power2[] = {0x10};
    const uint8_t vcom1[] = {0x3E, 0x28};
    const uint8_t vcom2[] = {0x86};
    // Landscape orientation, rotated 180 degrees from the previous mounting.
    const uint8_t madctl[] = {0xE8};
    const uint8_t pixel_format[] = {0x55};
    const uint8_t frame_rate[] = {0x00, 0x18};
    const uint8_t display_function[] = {0x08, 0x82, 0x27};
    tft_command_data(0xC0, power1, sizeof(power1));
    tft_command_data(0xC1, power2, sizeof(power2));
    tft_command_data(0xC5, vcom1, sizeof(vcom1));
    tft_command_data(0xC7, vcom2, sizeof(vcom2));
    tft_command_data(0x36, madctl, sizeof(madctl));
    tft_command_data(0x3A, pixel_format, sizeof(pixel_format));
    tft_command_data(0xB1, frame_rate, sizeof(frame_rate));
    tft_command_data(0xB6, display_function, sizeof(display_function));
    tft_command(0x11);
    vTaskDelay(pdMS_TO_TICKS(120));
    tft_command(0x29);
    vTaskDelay(pdMS_TO_TICKS(20));

    const uint8_t touch_wake = 0x80;
    spi_transaction_t touch_transaction = {
        .length = 8,
        .tx_buffer = &touch_wake,
    };
    ESP_ERROR_CHECK(spi_device_polling_transmit(touch_device, &touch_transaction));
}

static dashboard_view_t get_view(void)
{
    dashboard_source_t source;
    dashboard_view_t view = {0};
    const int64_t now = esp_timer_get_time();
    portENTER_CRITICAL(&state_mux);
    source = source_state;
    portEXIT_CRITICAL(&state_mux);

    view.roll_trim = source.roll_trim;
    view.pitch_trim = source.pitch_trim;
    view.yaw_trim = source.yaw_trim;
    view.throttle = source.throttle;
    view.battery_mv = source.battery_mv;
    view.sensitivity = source.sensitivity;
    view.battery_percent = source.battery_percent;
    view.armed = source.armed;
    view.connected = source.last_delivery_us != 0
        && now - source.last_delivery_us <= LINK_TIMEOUT_US;
    view.battery_valid = source.last_battery_us != 0
        && now - source.last_battery_us <= BATTERY_TIMEOUT_US;
    return view;
}

static void draw_card(int x, int y, int width, int height, const char *label,
    const char *value, uint16_t accent)
{
    fill_rect(x, y, width, height, COLOR_PANEL);
    fill_rect(x, y, 4, height, accent);
    draw_text(x + 10, y + 7, label, 1, COLOR_MUTED, COLOR_PANEL);
    draw_text(x + 10, y + 25, value, 2, COLOR_WHITE, COLOR_PANEL);
}

static void draw_dashboard(const dashboard_view_t *view)
{
    char text[32];
    fill_rect(0, 0, TFT_WIDTH, TFT_HEIGHT, COLOR_BG);
    fill_rect(0, 0, TFT_WIDTH, 38, COLOR_HEADER);
    draw_text(10, 11, "DRONE CTRL", 2, COLOR_WHITE, COLOR_HEADER);
    draw_text(220, 14, view->connected ? "LINK OK" : "LINK LOST", 1,
        view->connected ? COLOR_GREEN : COLOR_RED, COLOR_HEADER);

    draw_card(10, 48, 145, 52, "ARM COMMAND",
        view->armed ? "ON" : "OFF",
        view->armed ? COLOR_RED : COLOR_GREEN);

    if (view->battery_valid) {
        snprintf(text, sizeof(text), "%u.%02u V", view->battery_mv / 1000,
            (view->battery_mv % 1000) / 10);
    } else {
        snprintf(text, sizeof(text), "NO DATA");
    }
    draw_card(165, 48, 145, 52, "BATTERY", text,
        view->battery_valid ? COLOR_AMBER : COLOR_RED);

    snprintf(text, sizeof(text), "%+04d", view->roll_trim);
    draw_card(10, 110, 96, 58, "ROLL", text, COLOR_CYAN);
    snprintf(text, sizeof(text), "%+04d", view->pitch_trim);
    draw_card(112, 110, 96, 58, "PITCH", text, COLOR_CYAN);
    snprintf(text, sizeof(text), "%+04d", view->yaw_trim);
    draw_card(214, 110, 96, 58, "YAW", text, COLOR_CYAN);

    fill_rect(10, 178, 300, 52, COLOR_PANEL_ALT);
    snprintf(text, sizeof(text), "THROTTLE %u%%", view->throttle / 10);
    draw_text(20, 187, text, 2, COLOR_WHITE, COLOR_PANEL_ALT);
    snprintf(text, sizeof(text), "SENSITIVITY %u%%", view->sensitivity);
    draw_text(20, 211, text, 1, COLOR_MUTED, COLOR_PANEL_ALT);
    if (view->battery_valid && view->battery_percent <= 100) {
        snprintf(text, sizeof(text), "%u%%", view->battery_percent);
        draw_text(260, 211, text, 1, COLOR_AMBER, COLOR_PANEL_ALT);
    }
}

static void dashboard_task(void *argument)
{
    (void)argument;
    dashboard_view_t previous;
    memset(&previous, 0xFF, sizeof(previous));
    initialize_display();
    ESP_LOGI(TAG, "ILI9341 dashboard ready: 320x240 SPI2 touch IRQ GPIO%d",
        TOUCH_PIN_IRQ);

    while (true) {
        const dashboard_view_t current = get_view();
        if (memcmp(&current, &previous, sizeof(current)) != 0) {
            draw_dashboard(&current);
            previous = current;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void dashboard_start(void)
{
    const BaseType_t created = xTaskCreatePinnedToCore(
        dashboard_task, "dashboard", 4096, NULL, 3, NULL, 1);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}

void dashboard_update_controller(int16_t roll_trim, int16_t pitch_trim,
    int16_t yaw_trim, uint16_t throttle, uint8_t sensitivity, bool armed)
{
    portENTER_CRITICAL(&state_mux);
    source_state.roll_trim = roll_trim;
    source_state.pitch_trim = pitch_trim;
    source_state.yaw_trim = yaw_trim;
    source_state.throttle = throttle;
    source_state.sensitivity = sensitivity;
    source_state.armed = armed;
    portEXIT_CRITICAL(&state_mux);
}

void dashboard_update_flight_command(uint16_t throttle, bool armed)
{
    portENTER_CRITICAL(&state_mux);
    source_state.throttle = throttle;
    source_state.armed = armed;
    portEXIT_CRITICAL(&state_mux);
}

void dashboard_note_delivery(void)
{
    portENTER_CRITICAL(&state_mux);
    source_state.last_delivery_us = esp_timer_get_time();
    portEXIT_CRITICAL(&state_mux);
}

void dashboard_update_battery(uint16_t voltage_mv, uint8_t remaining_percent)
{
    portENTER_CRITICAL(&state_mux);
    source_state.battery_mv = voltage_mv;
    source_state.battery_percent = remaining_percent;
    source_state.last_battery_us = esp_timer_get_time();
    portEXIT_CRITICAL(&state_mux);
}
