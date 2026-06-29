#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_event.h"
#include "esp_http_server.h"
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

#include "dashboard.h"

#define WIFI_CHANNEL 6
#define AP_SSID "Drone-Servo-TX"
#define AP_PASSWORD "servo-control"

#define PICO_UART UART_NUM_2
#define PICO_UART_TX_PIN GPIO_NUM_17
#define PICO_UART_RX_PIN GPIO_NUM_16
#define PICO_UART_BAUD_RATE 115200

#define UART_SYNC_0 0xA5
#define UART_SYNC_1 0x5A
#define CONTROL_PAYLOAD_SIZE 16
#define RADIO_PACKET_MAGIC 0x52
#define RADIO_PACKET_VERSION 1
#define SERVO_PACKET_MAGIC 0x53
#define CONTROLLER_STATUS_MAGIC 0x54
#define TELEMETRY_PACKET_MAGIC 0x55
#define CONTROLLER_SERVO_MAGIC 0x56
#define SERVO_COUNT 4
#define SERVO_MIN_US 500
#define SERVO_MAX_US 2500
#define SERVO_DEFAULT_US 1500
#define SERVO_RAMP_MIN_US_S 10
#define SERVO_RAMP_MAX_US_S 500
#define SERVO_RAMP_DEFAULT_US_S 250
#define SERVO_KEEPALIVE_US 1000000
#define SERVO_RETRY_US 20000
#define CONTROLLER_SERVO_TIMEOUT_US 350000
#define SERVO_FLAG_SAVE_DEFAULTS (1U << 0)
#define SERVO_NVS_NAMESPACE "servo"
#define SERVO_NVS_KEY "config"

#define CONTROL_FLAG_ARM (1U << 0)
#define CONTROL_FLAG_ESTOP (1U << 1)
#define CONTROL_FLAG_CLEAR_ESTOP (1U << 2)

static const char *TAG = "pico_espnow_tx";
// Drone ESP32-WROOM STA MAC, read from the verified onboard receiver.
static const uint8_t drone_mac[ESP_NOW_ETH_ALEN] = {
    0x68, 0x09, 0x47, 0x5c, 0x2f, 0x8c,
};

static uint32_t uart_good;
static uint32_t uart_bad;
static uint32_t uart_status;
static uint32_t uart_servo;
static uint32_t espnow_queued;
static uint32_t espnow_failed;
static uint32_t espnow_delivered;
static uint32_t espnow_lost;
static uint16_t last_sequence;
static QueueHandle_t send_result_queue;
static portMUX_TYPE servo_mux = portMUX_INITIALIZER_UNLOCKED;
static uint16_t servo_values[SERVO_COUNT] = {
    SERVO_DEFAULT_US, SERVO_DEFAULT_US, SERVO_DEFAULT_US, SERVO_DEFAULT_US,
};
static uint16_t servo_ramp_speed_us_s = SERVO_RAMP_DEFAULT_US_S;
static uint16_t servo_startup_values[SERVO_COUNT] = {
    SERVO_DEFAULT_US, SERVO_DEFAULT_US, SERVO_DEFAULT_US, SERVO_DEFAULT_US,
};
static uint16_t servo_startup_ramp_speed_us_s = SERVO_RAMP_DEFAULT_US_S;
static uint32_t servo_generation;
static uint8_t servo_save_repeats;
static bool controller_servo_active;
static int64_t controller_servo_last_us;

typedef struct {
    uint16_t values[SERVO_COUNT];
    uint16_t ramp_speed_us_s;
} servo_config_t;

static const char servo_page[] =
    "<!doctype html><html><head><meta name=viewport content='width=device-width,initial-scale=1'>"
    "<title>Drone Servos</title><style>"
    "*{box-sizing:border-box}body{font-family:sans-serif;max-width:900px;margin:auto;padding:12px;background:#111;color:#eee}"
    "h1{font-size:1.45rem;margin:8px 0}.servo,.panel{background:#222;padding:12px;margin:10px 0;border-radius:10px}"
    "#controls{display:grid;grid-template-columns:1fr 1fr;gap:8px}.servo{margin:0}"
    "input[type=range]{width:100%}input[type=number]{width:90px;font-size:1rem}"
    "button{border:0;border-radius:10px;font-weight:bold;user-select:none;-webkit-user-select:none;-webkit-touch-callout:none;-webkit-tap-highlight-color:transparent;touch-action:none;outline:none}button:focus{outline:none}"
    ".default{width:100%;padding:16px;background:#2878d0;color:white;font-size:1.05rem}"
    ".drivegrid{display:grid;grid-template-columns:repeat(4,minmax(74px,1fr));gap:8px;margin-top:10px}"
    ".drive{min-height:92px;padding:8px 4px;color:white;font-size:1.25rem}.drive span{display:block;font-size:.72rem;margin-bottom:7px;opacity:.85}.drive:active{filter:brightness(1.35)}"
    ".cw{background:#278a45}.ccw{background:#b34444}"
    "#status{color:#8f8}small{color:#bbb}</style></head><body>"
    "<h1>Continuous Servo Control</h1><p id=status>Loading...</p>"
    "<small>Hold a direction to rotate. Releasing returns that pair to its neutral PWM.</small>"
    "<div class=panel><b>Saved startup / neutral PWM</b><div id=startup>Loading...</div></div>"
    "<div id=controls></div><button class=default onclick='saveDefaults()'>Set Startup Default</button>"
    "<div class=panel><b>Rotation PWM stretch: <span id=rv>250</span> us</b><input id=ramp type=range min=10 max=500 step=10 value=250></div>"
    "<div class=drivegrid><button class='drive cw' data-p=0 data-d=1><span>PAIR 1</span>CW</button><button class='drive ccw' data-p=0 data-d=-1><span>PAIR 1</span>CCW</button><button class='drive cw' data-p=1 data-d=1><span>PAIR 2</span>CW</button><button class='drive ccw' data-p=1 data-d=-1><span>PAIR 2</span>CCW</button></div><script>"
    "const box=document.getElementById('controls'),status=document.getElementById('status');let timer;"
    "for(let i=1;i<=4;i++){box.insertAdjacentHTML('beforeend',`<div class=servo><b>Servo ${i} neutral</b> "
    "<input id=n${i} type=number min=500 max=2500 step=1> us<input id=s${i} type=range min=500 max=2500 step=1></div>`);}"
    "function values(){return [1,2,3,4].map(i=>+document.getElementById('n'+i).value)}"
    "function ramp(){return +document.getElementById('ramp').value}function setRamp(v){document.getElementById('ramp').value=v;document.getElementById('rv').textContent=v}"
    "function setStartup(v,r){document.getElementById('startup').textContent=`S1 ${v[0]} us | S2 ${v[1]} us | S3 ${v[2]} us | S4 ${v[3]} us | Stretch ${r} us`}"
    "function schedule(){clearTimeout(timer);timer=setTimeout(()=>send(values()),120)}"
    "for(let i=1;i<=4;i++){let n=document.getElementById('n'+i),s=document.getElementById('s'+i);"
    "s.oninput=()=>{n.value=s.value;schedule()};n.onchange=()=>{s.value=n.value;schedule()}}"
    "document.getElementById('ramp').oninput=e=>setRamp(e.target.value);"
    "let sending=false,pending=null;async function send(v,save=false){pending={v:[...v],save};if(sending)return;sending=true;while(pending){let c=pending,rs=ramp();pending=null;status.textContent=c.save?'Saving startup defaults...':'Sending...';try{let r=await fetch(`/api/servos?s1=${c.v[0]}&s2=${c.v[1]}&s3=${c.v[2]}&s4=${c.v[3]}&ramp=${rs}${c.save?'&save=1':''}`);"
    "let j=await r.json();if(!r.ok)throw Error(j.error||'request failed');setStartup(j.startup,j.startupRamp);status.textContent=c.save?'Startup defaults saved':'Ready';}catch(e){status.textContent='Error: '+e.message}}sending=false}"
    "function set(v){v.forEach((x,k)=>{document.getElementById('n'+(k+1)).value=x;document.getElementById('s'+(k+1)).value=x})}"
    "const holds=new Map();function driveValues(){let v=values(),dirs=[0,0],stretch=ramp();holds.forEach(h=>dirs[h.p]+=h.d);dirs.forEach((d,p)=>{d=Math.max(-1,Math.min(1,d));let a=p*2;v[a]=Math.max(500,Math.min(2500,v[a]+d*stretch));v[a+1]=Math.max(500,Math.min(2500,v[a+1]-d*stretch))});return v}"
    "function updateDrive(){send(driveValues())}function stop(e){if(holds.delete(e.pointerId))updateDrive();e.currentTarget.blur()}function stopAll(){if(holds.size){holds.clear();send(values())}}"
    "function saveDefaults(){stopAll();send(values(),true)}"
    "document.querySelectorAll('[data-p]').forEach(b=>{b.onpointerdown=e=>{e.preventDefault();try{b.setPointerCapture(e.pointerId)}catch(x){}holds.set(e.pointerId,{p:+b.dataset.p,d:+b.dataset.d});updateDrive()};b.onpointerup=stop;b.onpointercancel=stop;b.onlostpointercapture=e=>{if(holds.has(e.pointerId))stop(e)};b.oncontextmenu=e=>e.preventDefault()});"
    "window.addEventListener('blur',stopAll);document.addEventListener('visibilitychange',()=>{if(document.hidden)stopAll()});window.addEventListener('pagehide',stopAll);"
    "fetch('/api/servos').then(r=>r.json()).then(j=>{set(j.servos);setRamp(j.ramp);setStartup(j.startup,j.startupRamp);status.textContent='Ready'}).catch(e=>status.textContent='Error: '+e.message);"
    "</script></body></html>";

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

static bool validate_control_payload(const uint8_t packet[CONTROL_PAYLOAD_SIZE])
{
    if (packet[0] != RADIO_PACKET_MAGIC || packet[15] != RADIO_PACKET_VERSION) {
        return false;
    }

    const uint16_t expected_crc = read_u16_le(&packet[12]);
    const uint16_t actual_crc = crc16_ccitt(packet, 12);
    if (expected_crc != actual_crc) {
        return false;
    }

    const int16_t roll = read_i16_le(&packet[4]);
    const int16_t pitch = read_i16_le(&packet[6]);
    const int16_t throttle = read_i16_le(&packet[8]);
    const int16_t yaw = read_i16_le(&packet[10]);
    if (roll < -1000 || roll > 1000
        || pitch < -1000 || pitch > 1000
        || throttle < 0 || throttle > 1000
        || yaw < -1000 || yaw > 1000) {
        return false;
    }

    return true;
}

static bool apply_controller_status(const uint8_t packet[CONTROL_PAYLOAD_SIZE])
{
    if (packet[0] != CONTROLLER_STATUS_MAGIC
        || packet[1] != RADIO_PACKET_VERSION
        || packet[15] != RADIO_PACKET_VERSION
        || read_u16_le(&packet[12]) != crc16_ccitt(packet, 12)) {
        return false;
    }

    const int16_t roll_trim = read_i16_le(&packet[2]);
    const int16_t pitch_trim = read_i16_le(&packet[4]);
    const int16_t yaw_trim = read_i16_le(&packet[6]);
    const uint16_t throttle = read_u16_le(&packet[8]);
    const uint8_t sensitivity = packet[10];
    const uint8_t armed = packet[11];
    if (roll_trim < -250 || roll_trim > 250
        || pitch_trim < -250 || pitch_trim > 250
        || yaw_trim < -250 || yaw_trim > 250
        || throttle > 1000 || sensitivity < 25 || sensitivity > 100
        || armed > 1) {
        return false;
    }

    dashboard_update_controller(roll_trim, pitch_trim, yaw_trim,
        throttle, sensitivity, armed != 0);
    return true;
}

static uint16_t clamp_servo_us(int32_t value)
{
    if (value < SERVO_MIN_US) {
        return SERVO_MIN_US;
    }
    if (value > SERVO_MAX_US) {
        return SERVO_MAX_US;
    }
    return (uint16_t)value;
}

static bool apply_controller_servo(const uint8_t packet[CONTROL_PAYLOAD_SIZE])
{
    if (packet[0] != CONTROLLER_SERVO_MAGIC
        || packet[1] != RADIO_PACKET_VERSION
        || packet[15] != RADIO_PACKET_VERSION
        || read_u16_le(&packet[12]) != crc16_ccitt(packet, 12)) {
        return false;
    }

    const int8_t direction = (int8_t)packet[2];
    if (direction < -1 || direction > 1) {
        return false;
    }

    portENTER_CRITICAL(&servo_mux);
    for (size_t i = 0; i < SERVO_COUNT; ++i) {
        const int32_t polarity = (i & 1U) == 0 ? 1 : -1;
        const int32_t pulse = (int32_t)servo_startup_values[i]
            + (int32_t)direction * polarity * servo_ramp_speed_us_s;
        servo_values[i] = clamp_servo_us(pulse);
    }
    servo_generation++;
    controller_servo_active = direction != 0;
    controller_servo_last_us = esp_timer_get_time();
    portEXIT_CRITICAL(&servo_mux);
    return true;
}

static void stop_controller_servo_if_timed_out(int64_t now)
{
    if (!controller_servo_active
        || now - controller_servo_last_us <= CONTROLLER_SERVO_TIMEOUT_US) {
        return;
    }

    portENTER_CRITICAL(&servo_mux);
    memcpy(servo_values, servo_startup_values, sizeof(servo_values));
    servo_generation++;
    controller_servo_active = false;
    portEXIT_CRITICAL(&servo_mux);
}

static void receive_callback(const esp_now_recv_info_t *info,
    const uint8_t *data, int length)
{
    if (info == NULL || data == NULL || length != CONTROL_PAYLOAD_SIZE
        || memcmp(info->src_addr, drone_mac, sizeof(drone_mac)) != 0
        || data[0] != TELEMETRY_PACKET_MAGIC
        || data[1] != RADIO_PACKET_VERSION
        || data[15] != RADIO_PACKET_VERSION
        || read_u16_le(&data[12]) != crc16_ccitt(data, 12)) {
        return;
    }

    const uint16_t voltage_mv = read_u16_le(&data[2]);
    const uint8_t remaining_percent = data[4];
    if (data[5] == 0 || voltage_mv < 1000 || voltage_mv > 60000
        || (remaining_percent > 100 && remaining_percent != 255)) {
        return;
    }
    dashboard_update_battery(voltage_mv, remaining_percent);
}

static bool build_servo_payload(uint8_t packet[CONTROL_PAYLOAD_SIZE])
{
    uint16_t values[SERVO_COUNT];
    uint16_t ramp_speed_us_s;
    bool save_defaults;

    portENTER_CRITICAL(&servo_mux);
    memcpy(values, servo_values, sizeof(values));
    ramp_speed_us_s = servo_ramp_speed_us_s;
    save_defaults = servo_save_repeats > 0;
    portEXIT_CRITICAL(&servo_mux);

    memset(packet, 0, CONTROL_PAYLOAD_SIZE);
    packet[0] = SERVO_PACKET_MAGIC;
    packet[1] = save_defaults ? SERVO_FLAG_SAVE_DEFAULTS : 0;
    write_u16_le(&packet[2], ramp_speed_us_s);
    for (size_t i = 0; i < SERVO_COUNT; ++i) {
        write_u16_le(&packet[4 + (i * 2)], values[i]);
    }
    write_u16_le(&packet[12], crc16_ccitt(packet, 12));
    packet[15] = RADIO_PACKET_VERSION;
    return save_defaults;
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
        memcpy(servo_values, config.values, sizeof(config.values));
        servo_ramp_speed_us_s = stretch_us;
        memcpy(servo_startup_values, config.values, sizeof(config.values));
        servo_startup_ramp_speed_us_s = stretch_us;
        ESP_LOGI(TAG, "loaded continuous servo neutrals: %u %u %u %u us stretch=%u us",
            config.values[0], config.values[1], config.values[2], config.values[3],
            stretch_us);
    }
    nvs_close(handle);
}

static esp_err_t save_servo_defaults(const uint16_t values[SERVO_COUNT], uint16_t ramp_speed_us_s)
{
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
    return result;
}

static bool servo_query_requests_save(httpd_req_t *request)
{
    const size_t query_length = httpd_req_get_url_query_len(request);
    if (query_length == 0 || query_length > 160) {
        return false;
    }
    char query[161];
    char save[4];
    return httpd_req_get_url_query_str(request, query, sizeof(query)) == ESP_OK
        && httpd_query_key_value(query, "save", save, sizeof(save)) == ESP_OK
        && strcmp(save, "1") == 0;
}

static esp_err_t servo_page_handler(httpd_req_t *request)
{
    httpd_resp_set_type(request, "text/html");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_send(request, servo_page, HTTPD_RESP_USE_STRLEN);
}

static bool parse_servo_query(httpd_req_t *request, uint16_t values[SERVO_COUNT],
    uint16_t *ramp_speed_us_s)
{
    const size_t query_length = httpd_req_get_url_query_len(request);
    if (query_length == 0 || query_length > 160) {
        return false;
    }

    char query[161];
    if (httpd_req_get_url_query_str(request, query, sizeof(query)) != ESP_OK) {
        return false;
    }

    for (size_t i = 0; i < SERVO_COUNT; ++i) {
        char key[4];
        char value_text[8];
        snprintf(key, sizeof(key), "s%u", (unsigned)(i + 1));
        if (httpd_query_key_value(query, key, value_text, sizeof(value_text)) != ESP_OK) {
            return false;
        }
        char *end = NULL;
        const long value = strtol(value_text, &end, 10);
        if (end == value_text || *end != '\0' || value < SERVO_MIN_US || value > SERVO_MAX_US) {
            return false;
        }
        values[i] = (uint16_t)value;
    }
    char ramp_text[8];
    if (httpd_query_key_value(query, "ramp", ramp_text, sizeof(ramp_text)) != ESP_OK) {
        return false;
    }
    char *end = NULL;
    const long ramp = strtol(ramp_text, &end, 10);
    if (end == ramp_text || *end != '\0'
        || ramp < SERVO_RAMP_MIN_US_S || ramp > SERVO_RAMP_MAX_US_S) {
        return false;
    }
    *ramp_speed_us_s = (uint16_t)ramp;
    return true;
}

static esp_err_t servo_api_handler(httpd_req_t *request)
{
    uint16_t values[SERVO_COUNT];
    uint16_t ramp_speed_us_s;
    if (httpd_req_get_url_query_len(request) > 0) {
        if (!parse_servo_query(request, values, &ramp_speed_us_s)) {
            httpd_resp_set_status(request, "400 Bad Request");
            httpd_resp_set_type(request, "application/json");
            return httpd_resp_sendstr(request,
                "{\"error\":\"servo neutrals must be 500-2500 and stretch must be 10-500\"}");
        }
        const bool save_defaults = servo_query_requests_save(request);
        if (save_defaults && save_servo_defaults(values, ramp_speed_us_s) != ESP_OK) {
            httpd_resp_set_status(request, "500 Internal Server Error");
            httpd_resp_set_type(request, "application/json");
            return httpd_resp_sendstr(request, "{\"error\":\"failed to save defaults\"}");
        }
        portENTER_CRITICAL(&servo_mux);
        memcpy(servo_values, values, sizeof(values));
        servo_ramp_speed_us_s = ramp_speed_us_s;
        servo_generation++;
        if (save_defaults) {
            memcpy(servo_startup_values, values, sizeof(values));
            servo_startup_ramp_speed_us_s = ramp_speed_us_s;
            servo_save_repeats = 3;
        }
        portEXIT_CRITICAL(&servo_mux);
        ESP_LOGI(TAG, "continuous servo command: %u %u %u %u us stretch=%u us save=%d",
            values[0], values[1], values[2], values[3], ramp_speed_us_s, save_defaults);
    } else {
        portENTER_CRITICAL(&servo_mux);
        memcpy(values, servo_values, sizeof(values));
        ramp_speed_us_s = servo_ramp_speed_us_s;
        portEXIT_CRITICAL(&servo_mux);
    }

    uint16_t startup_values[SERVO_COUNT];
    uint16_t startup_ramp_speed_us_s;
    portENTER_CRITICAL(&servo_mux);
    memcpy(startup_values, servo_startup_values, sizeof(startup_values));
    startup_ramp_speed_us_s = servo_startup_ramp_speed_us_s;
    portEXIT_CRITICAL(&servo_mux);

    char response[192];
    snprintf(response, sizeof(response),
        "{\"servos\":[%u,%u,%u,%u],\"ramp\":%u,"
        "\"startup\":[%u,%u,%u,%u],\"startupRamp\":%u}",
        values[0], values[1], values[2], values[3], ramp_speed_us_s,
        startup_values[0], startup_values[1], startup_values[2], startup_values[3],
        startup_ramp_speed_us_s);
    httpd_resp_set_type(request, "application/json");
    httpd_resp_set_hdr(request, "Cache-Control", "no-store");
    return httpd_resp_sendstr(request, response);
}

static void start_web_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 6144;
    httpd_handle_t server = NULL;
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    const httpd_uri_t page = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = servo_page_handler,
    };
    const httpd_uri_t api = {
        .uri = "/api/servos",
        .method = HTTP_GET,
        .handler = servo_api_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &page));
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &api));
    ESP_LOGI(TAG, "Servo web UI: Wi-Fi '%s', password '%s', http://192.168.4.1/",
        AP_SSID, AP_PASSWORD);
}

static void send_callback(const esp_now_send_info_t *tx_info, esp_now_send_status_t status)
{
    (void)tx_info;
    const uint8_t delivered = status == ESP_NOW_SEND_SUCCESS ? 1U : 0U;
    if (status == ESP_NOW_SEND_SUCCESS) {
        espnow_delivered++;
        dashboard_note_delivery();
    } else {
        espnow_lost++;
    }
    if (send_result_queue != NULL) {
        (void)xQueueSend(send_result_queue, &delivered, 0);
    }
}

static void start_wifi_espnow(void)
{
    uint8_t sta_mac[6];

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    wifi_config_t ap_config = {
        .ap = {
            .ssid = AP_SSID,
            .password = AP_PASSWORD,
            .ssid_len = sizeof(AP_SSID) - 1,
            .channel = WIFI_CHANNEL,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .max_connection = 2,
            .beacon_interval = 100,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_AP,
        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N));
    ESP_ERROR_CHECK(esp_wifi_set_protocol(WIFI_IF_STA,
        WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N | WIFI_PROTOCOL_LR));
    ESP_ERROR_CHECK(esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE));

    send_result_queue = xQueueCreate(32, sizeof(uint8_t));
    ESP_ERROR_CHECK(send_result_queue != NULL ? ESP_OK : ESP_ERR_NO_MEM);

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(send_callback));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(receive_callback));

    esp_now_peer_info_t peer = {
        .channel = WIFI_CHANNEL,
        .ifidx = WIFI_IF_STA,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, drone_mac, sizeof(drone_mac));
    ESP_ERROR_CHECK(esp_now_add_peer(&peer));

    esp_now_rate_config_t rate_config = {
        .phymode = WIFI_PHY_MODE_LR,
        .rate = WIFI_PHY_RATE_LORA_250K,
        .ersu = false,
        .dcm = false,
    };
    ESP_ERROR_CHECK(esp_now_set_peer_rate_config(drone_mac, &rate_config));

    ESP_ERROR_CHECK(esp_read_mac(sta_mac, ESP_MAC_WIFI_STA));
    ESP_LOGI(TAG, "ESP-NOW LR TX ready channel=%d rate=250K drone=%02X:%02X:%02X:%02X:%02X:%02X STA=%02X:%02X:%02X:%02X:%02X:%02X",
        WIFI_CHANNEL,
        drone_mac[0], drone_mac[1], drone_mac[2], drone_mac[3], drone_mac[4], drone_mac[5],
        sta_mac[0], sta_mac[1], sta_mac[2], sta_mac[3], sta_mac[4], sta_mac[5]);
}

static void start_pico_uart(void)
{
    const uart_config_t uart_config = {
        .baud_rate = PICO_UART_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(PICO_UART, 2048, 512, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(PICO_UART, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(PICO_UART, PICO_UART_TX_PIN, PICO_UART_RX_PIN,
        UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    ESP_LOGI(TAG, "Pico UART ready: GPIO%d TX2 -> Pico GP13, GPIO%d RX2 <- Pico GP12",
        PICO_UART_TX_PIN, PICO_UART_RX_PIN);
}

static void uart_bridge_task(void *argument)
{
    (void)argument;
    uint8_t payload[CONTROL_PAYLOAD_SIZE];
    uint8_t state = 0;
    uint8_t length = 0;
    uint8_t offset = 0;
    int64_t last_log_us = esp_timer_get_time();
    int64_t last_servo_send_us = 0;
    int64_t last_servo_attempt_us = 0;
    uint32_t sent_servo_generation = UINT32_MAX;

    while (true) {
        uint8_t delivered = 0;
        while (xQueueReceive(send_result_queue, &delivered, 0) == pdTRUE) {
            if (delivered) {
                uart_write_bytes(PICO_UART, "OK\n", 3);
            }
        }

        uint8_t byte = 0;
        const int read = uart_read_bytes(PICO_UART, &byte, 1, pdMS_TO_TICKS(20));
        if (read == 1) {
            switch (state) {
            case 0:
                state = byte == UART_SYNC_0 ? 1 : 0;
                break;
            case 1:
                state = byte == UART_SYNC_1 ? 2 : 0;
                break;
            case 2:
                length = byte;
                offset = 0;
                state = length == CONTROL_PAYLOAD_SIZE ? 3 : 0;
                if (state == 0) {
                    uart_bad++;
                }
                break;
            case 3:
                payload[offset++] = byte;
                if (offset == length) {
                    if (payload[0] == CONTROLLER_STATUS_MAGIC
                        && apply_controller_status(payload)) {
                        uart_status++;
                    } else if (payload[0] == CONTROLLER_SERVO_MAGIC
                        && apply_controller_servo(payload)) {
                        uart_servo++;
                    } else if (validate_control_payload(payload)) {
                        const uint16_t throttle = (uint16_t)read_i16_le(&payload[8]);
                        const bool armed = (payload[1] & CONTROL_FLAG_ARM) != 0
                            && (payload[1] & CONTROL_FLAG_ESTOP) == 0;
                        dashboard_update_flight_command(throttle, armed);
                        const esp_err_t result = esp_now_send(drone_mac, payload, sizeof(payload));
                        last_sequence = read_u16_le(&payload[2]);
                        if (result == ESP_OK) {
                            uart_good++;
                            espnow_queued++;
                        } else {
                            espnow_failed++;
                        }
                    } else {
                        uart_bad++;
                    }
                    state = 0;
                }
                break;
            default:
                state = 0;
                break;
            }
        }

        const int64_t now = esp_timer_get_time();
        stop_controller_servo_if_timed_out(now);
        uint32_t current_servo_generation;
        bool save_pending;
        portENTER_CRITICAL(&servo_mux);
        current_servo_generation = servo_generation;
        save_pending = servo_save_repeats > 0;
        portEXIT_CRITICAL(&servo_mux);
        const bool servo_change_pending = current_servo_generation != sent_servo_generation;
        if (((servo_change_pending || save_pending) && now - last_servo_attempt_us >= SERVO_RETRY_US)
            || now - last_servo_send_us >= SERVO_KEEPALIVE_US) {
            uint8_t servo_payload[CONTROL_PAYLOAD_SIZE];
            const bool included_save = build_servo_payload(servo_payload);
            last_servo_attempt_us = now;
            if (esp_now_send(drone_mac, servo_payload, sizeof(servo_payload)) == ESP_OK) {
                espnow_queued++;
                sent_servo_generation = current_servo_generation;
                last_servo_send_us = now;
                if (included_save) {
                    portENTER_CRITICAL(&servo_mux);
                    if (servo_save_repeats > 0) {
                        servo_save_repeats--;
                    }
                    portEXIT_CRITICAL(&servo_mux);
                }
            } else {
                espnow_failed++;
            }
        }

        if (now - last_log_us >= 1000000) {
            ESP_LOGI(TAG, "seq=%u uart_good=%lu uart_status=%lu uart_servo=%lu uart_bad=%lu queued=%lu failed=%lu delivered=%lu lost=%lu",
                last_sequence,
                (unsigned long)uart_good,
                (unsigned long)uart_status,
                (unsigned long)uart_servo,
                (unsigned long)uart_bad,
                (unsigned long)espnow_queued,
                (unsigned long)espnow_failed,
                (unsigned long)espnow_delivered,
                (unsigned long)espnow_lost);
            last_log_us = now;
        }
    }
}

void app_main(void)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);

    load_servo_defaults();
    ESP_LOGI(TAG, "boot Pico UART -> ESP-NOW bridge");
    dashboard_start();
    start_wifi_espnow();
    start_web_server();
    start_pico_uart();

    BaseType_t created = xTaskCreatePinnedToCore(uart_bridge_task, "uart_bridge",
        4096, NULL, 15, NULL, 0);
    ESP_ERROR_CHECK(created == pdPASS ? ESP_OK : ESP_ERR_NO_MEM);
}
