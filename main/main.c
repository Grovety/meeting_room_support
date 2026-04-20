#include "ui_aux_panel.h"
#include "aux_udp_protocol.h"

#include "driver/gpio.h"
#include "driver/i2c.h"
#include "driver/spi_master.h"
#include "esp_event.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_io.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_rom_crc.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_http_server.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "nvs.h"
#include "nvs_flash.h"

#include <assert.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

static const char *TAG = "mycube3_5";

#define AUX_PANEL_HOR_RES 480
#define AUX_PANEL_VER_RES 320
#define AUX_PANEL_TICK_MS 5
#define AUX_PANEL_BUF_LINES 20

#define AUX_PANEL_LCD_HOST SPI2_HOST

#define AUX_PANEL_LCD_PIXEL_CLOCK_HZ (20 * 1000 * 1000)
#define AUX_PANEL_LCD_CMD_BITS       8
#define AUX_PANEL_LCD_PARAM_BITS     8

#define AUX_PANEL_PIN_NUM_SCLK      42
#define AUX_PANEL_PIN_NUM_MOSI      39
#define AUX_PANEL_PIN_NUM_MISO      -1
#define AUX_PANEL_PIN_NUM_LCD_DC    41
#define AUX_PANEL_PIN_NUM_LCD_CS    40
#define AUX_PANEL_PIN_NUM_LCD_RST   -1
#define AUX_PANEL_PIN_NUM_LCD_POWER 21
#define AUX_PANEL_PIN_NUM_PANEL_EN  14
#define AUX_PANEL_PIN_NUM_BK_LIGHT  38

#ifndef AUX_PANEL_TOUCH_I2C_PORT
#define AUX_PANEL_TOUCH_I2C_PORT I2C_NUM_0
#endif

#ifndef AUX_PANEL_TOUCH_I2C_SCL
#define AUX_PANEL_TOUCH_I2C_SCL 1
#endif

#ifndef AUX_PANEL_TOUCH_I2C_SDA
#define AUX_PANEL_TOUCH_I2C_SDA 2
#endif

#ifndef AUX_PANEL_TOUCH_I2C_FREQ_HZ
#define AUX_PANEL_TOUCH_I2C_FREQ_HZ 400000
#endif

#define AUX_PANEL_TOUCH_POLL_MS 20

#ifndef AUX_PANEL_TOUCH_SWAP_XY
#define AUX_PANEL_TOUCH_SWAP_XY 1
#endif

#ifndef AUX_PANEL_TOUCH_MIRROR_X
#define AUX_PANEL_TOUCH_MIRROR_X 0
#endif

#ifndef AUX_PANEL_TOUCH_MIRROR_Y
#define AUX_PANEL_TOUCH_MIRROR_Y 1
#endif

#define AUX_TOUCH_ADDR_GT911_PRIMARY 0x5D
#define AUX_TOUCH_ADDR_GT911_BACKUP  0x14
#define AUX_TOUCH_ADDR_CHSC6540      0x2E
#define AUX_TOUCH_ADDR_FT6236        0x38
#define AUX_TOUCH_GT911_REG_PRODUCT  0x8140
#define AUX_TOUCH_GT911_REG_STATUS   0x814E
#define AUX_TOUCH_GT911_REG_POINTS   0x8150
#define AUX_TOUCH_FT_REG_TD_STATUS   0x02
#define AUX_TOUCH_FT_REG_CHIP_ID     0xA3
#define AUX_TOUCH_FT_REG_VENDOR_ID   0xA8

#define ILI9488_CMD_SWRESET 0x01
#define ILI9488_CMD_INVOFF  0x20
#define ILI9488_CMD_INVON   0x21
#define ILI9488_CMD_SLPOUT  0x11
#define ILI9488_CMD_DISPON  0x29
#define ILI9488_CMD_CASET   0x2A
#define ILI9488_CMD_PASET   0x2B
#define ILI9488_CMD_RAMWR   0x2C
#define ILI9488_CMD_MADCTL  0x36
#define ILI9488_CMD_COLMOD  0x3A

#define ILI9488_MADCTL_MY   0x80
#define ILI9488_MADCTL_MX   0x40
#define ILI9488_MADCTL_MV   0x20
#define ILI9488_MADCTL_BGR  0x08

#define AUX_PANEL_LCD_USE_BGR_ORDER 1
#define AUX_PANEL_LCD_USE_INVERSION 1
#define AUX_PANEL_UI_ROTATE_180     1

#ifndef AUX_PANEL_WIFI_SSID
#define AUX_PANEL_WIFI_SSID ""
#endif

#ifndef AUX_PANEL_WIFI_PASS
#define AUX_PANEL_WIFI_PASS ""
#endif

#ifndef AUX_PANEL_CLIENT_NAME
#define AUX_PANEL_CLIENT_NAME "Door Panel 3.5"
#endif

#ifndef AUX_PANEL_TARGET_MAIN_ID
#define AUX_PANEL_TARGET_MAIN_ID ""
#endif

#ifndef AUX_PANEL_TARGET_ROOM_NAME
#define AUX_PANEL_TARGET_ROOM_NAME ""
#endif

#ifndef AUX_PANEL_RESET_PAIR_ON_BOOT
#define AUX_PANEL_RESET_PAIR_ON_BOOT 0
#endif

#define AUX_PANEL_POLL_INTERVAL_MS   100
#define AUX_PANEL_PAIR_REQ_INTERVAL_MS 2000
#define AUX_PANEL_PAIR_REFRESH_INTERVAL_MS 30000
#define AUX_PANEL_STATE_STALE_MS     5000
#define AUX_PANEL_DISCOVERY_MAX      8
#define AUX_PANEL_DISCOVERY_TTL_MS   15000
#define AUX_PANEL_DISCOVERY_LOG_MS   10000
#define AUX_PANEL_PAIR_NAMESPACE       "aux_client"
#define AUX_PANEL_PAIR_MAIN_ID_KEY     "main_id"
#define AUX_PANEL_PAIR_TOKEN_KEY       "pair_token"
#define AUX_PANEL_TARGET_MAIN_ID_KEY   "target_id"
#define AUX_PANEL_TARGET_ROOM_NAME_KEY "target_room"
#define AUX_PANEL_PAIR_TOKEN_LEN       33
#define AUX_PANEL_CLIENT_ID_LEN        32
#define AUX_PANEL_PANEL_NAME_LEN       48
#define AUX_PANEL_WIFI_SSID_LEN        33
#define AUX_PANEL_WIFI_PASS_LEN        65
#define AUX_PANEL_CFG_NAMESPACE        "aux_cfg"
#define AUX_PANEL_CFG_WIFI_SSID_KEY    "wifi_ssid"
#define AUX_PANEL_CFG_WIFI_PASS_KEY    "wifi_pass"
#define AUX_PANEL_CFG_PANEL_NAME_KEY   "panel_name"
#define AUX_PANEL_CFG_LANG_KEY         "lang"
#define AUX_PANEL_SETUP_AP_CHANNEL     6

#define AUX_PANEL_SETUP_AP_MAX_CONN    4
#define AUX_PANEL_HTTPD_STACK_SIZE     8192
#define AUX_PANEL_HTTPD_URI_LEN        16
#define AUX_PANEL_HTTPD_RESP_HDR_LEN   256
#define AUX_PANEL_HTTPD_RESP_CHUNK_LEN 512
#define AUX_PANEL_WIFI_SCAN_MAX_AP     16

static lv_disp_draw_buf_t s_draw_buf;
static lv_disp_drv_t s_disp_drv;
static lv_color_t *s_buf_1 = NULL;
static lv_color_t *s_buf_2 = NULL;
static uint8_t *s_flush_rgb_buf = NULL;
static size_t s_flush_rgb_buf_size = 0;
static esp_timer_handle_t s_lvgl_tick_timer;
static esp_lcd_panel_io_handle_t s_panel_io = NULL;

static SemaphoreHandle_t s_ui_state_mutex = NULL;
static SemaphoreHandle_t s_pair_cfg_mutex = NULL;
static aux_panel_room_state_t s_pending_ui_state = {0};
static bool s_has_pending_ui_state = false;
static volatile bool s_wifi_connected = false;
static char s_client_id[AUX_PANEL_CLIENT_ID_LEN] = {0};
static char s_paired_main_id[AUX_UDP_MAIN_ID_LEN] = {0};
static char s_pair_token[AUX_PANEL_PAIR_TOKEN_LEN] = {0};
static bool s_is_paired = false;
static int s_udp_rx_sock = -1;
static uint32_t s_last_udp_seq = 0;
static bool s_have_last_udp_seq = false;
static uint32_t s_udp_seq = 1;
static char s_target_main_id[AUX_UDP_MAIN_ID_LEN] = AUX_PANEL_TARGET_MAIN_ID;
static char s_target_room_name[AUX_UDP_ROOM_NAME_LEN] = AUX_PANEL_TARGET_ROOM_NAME;
static volatile bool s_force_pair_request = false;
static struct sockaddr_in s_paired_main_addr = {0};
static bool s_have_paired_main_addr = false;
static int64_t s_last_state_rx_us = 0;
static int64_t s_last_discovery_log_us = 0;
static volatile bool s_touch_pressed = false;
static volatile uint16_t s_touch_x = 0;
static volatile uint16_t s_touch_y = 0;
static bool s_touch_i2c_installed = false;
static httpd_handle_t s_setup_httpd = NULL;
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;
static bool s_wifi_stack_ready = false;
static char s_sta_ip_text[16] = "0.0.0.0";
static char s_setup_ap_ssid[33] = {0};
static char s_cfg_wifi_ssid[AUX_PANEL_WIFI_SSID_LEN] = {0};
static char s_cfg_wifi_pass[AUX_PANEL_WIFI_PASS_LEN] = {0};
static char s_cfg_panel_name[AUX_PANEL_PANEL_NAME_LEN] = {0};
static aux_panel_ui_language_t s_cfg_ui_lang = AUX_PANEL_UI_LANG_EN;

typedef enum {
    AUX_TOUCH_NONE = 0,
    AUX_TOUCH_FT6236,
    AUX_TOUCH_GT911,
    AUX_TOUCH_CHSC6540,
} aux_touch_ic_t;

static aux_touch_ic_t s_touch_ic = AUX_TOUCH_NONE;
static uint8_t s_touch_i2c_addr = 0;
static int s_touch_i2c_sda_pin = AUX_PANEL_TOUCH_I2C_SDA;
static int s_touch_i2c_scl_pin = AUX_PANEL_TOUCH_I2C_SCL;

typedef struct {
    bool valid;
    char main_id[AUX_UDP_MAIN_ID_LEN];
    char room_name[AUX_UDP_ROOM_NAME_LEN];
    struct sockaddr_in addr;
    int64_t last_seen_us;
    bool paired;
    bool pair_mode;
} discovered_main_t;

static discovered_main_t s_discovered_mains[AUX_PANEL_DISCOVERY_MAX] = {0};

typedef struct {
    aux_panel_ui_main_entry_t entries[AUX_PANEL_UI_MAX_DISCOVERED];
    size_t count;
    char selected_main_id[AUX_UDP_MAIN_ID_LEN];
    char status_text[96];
    bool dirty;
} pending_pair_ui_t;

static pending_pair_ui_t s_pending_pair_ui = {0};

typedef struct {
    char wifi_text[96];
    char setup_text[220];
    char setup_url[64];
    bool dirty;
} pending_settings_ui_t;

static pending_settings_ui_t s_pending_settings_ui = {0};

static void copy_string_field(const char *src, char *dst, size_t dst_size);
static void update_settings_ui_texts(void);
static esp_err_t ensure_setup_http_server(void);
static esp_err_t configure_setup_ap(void);
static esp_err_t update_wifi_mode_for_connectivity(bool connected);
static esp_err_t force_setup_ap_mode(void);
static void queue_settings_ui_texts(const char *wifi_text, const char *setup_text, const char *setup_url);
static void reset_main_ui_state(void);

static char from_hex_digit(char c)
{
    if (c >= '0' && c <= '9') {
        return (char)(c - '0');
    }
    if (c >= 'a' && c <= 'f') {
        return (char)(10 + (c - 'a'));
    }
    if (c >= 'A' && c <= 'F') {
        return (char)(10 + (c - 'A'));
    }
    return 0;
}

static void url_decode_inplace(char *str)
{
    char *read_ptr = str;
    char *write_ptr = str;

    if (!str) {
        return;
    }

    while (*read_ptr) {
        if (*read_ptr == '+' ) {
            *write_ptr++ = ' ';
            ++read_ptr;
        } else if (*read_ptr == '%' &&
                   read_ptr[1] &&
                   read_ptr[2]) {
            char hi = from_hex_digit(read_ptr[1]);
            char lo = from_hex_digit(read_ptr[2]);
            *write_ptr++ = (char)((hi << 4) | lo);
            read_ptr += 3;
        } else {
            *write_ptr++ = *read_ptr++;
        }
    }
    *write_ptr = '\0';
}

static void html_escape_into(const char *src, char *dst, size_t dst_size)
{
    size_t out = 0;
    const char *in = src ? src : "";

    if (!dst || dst_size == 0) {
        return;
    }

    dst[0] = '\0';
    while (*in && out + 1 < dst_size) {
        const char *replacement = NULL;
        switch (*in) {
            case '&':
                replacement = "&amp;";
                break;
            case '<':
                replacement = "&lt;";
                break;
            case '>':
                replacement = "&gt;";
                break;
            case '"':
                replacement = "&quot;";
                break;
            case '\'':
                replacement = "&#39;";
                break;
            default:
                break;
        }

        if (replacement) {
            size_t rep_len = strlen(replacement);
            if (out + rep_len >= dst_size) {
                break;
            }
            memcpy(dst + out, replacement, rep_len);
            out += rep_len;
        } else {
            dst[out++] = *in;
        }
        ++in;
    }
    dst[out] = '\0';
}

static bool form_get_value(const char *body, const char *key, char *out, size_t out_size)
{
    size_t key_len = 0;
    const char *match = NULL;
    const char *value_start = NULL;
    const char *value_end = NULL;
    size_t copy_len = 0;

    if (!body || !key || !out || out_size == 0) {
        return false;
    }
    out[0] = '\0';
    key_len = strlen(key);
    match = body;
    while ((match = strstr(match, key)) != NULL) {
        if ((match == body || match[-1] == '&') && match[key_len] == '=') {
            break;
        }
        ++match;
    }
    if (!match) {
        return false;
    }

    value_start = match + key_len + 1;
    value_end = strchr(value_start, '&');
    if (!value_end) {
        value_end = value_start + strlen(value_start);
    }

    copy_len = (size_t)(value_end - value_start);
    if (copy_len >= out_size) {
        copy_len = out_size - 1;
    }
    memcpy(out, value_start, copy_len);
    out[copy_len] = '\0';
    url_decode_inplace(out);
    return true;
}

static esp_err_t load_runtime_config(void)
{
    nvs_handle_t handle;
    size_t ssid_len = sizeof(s_cfg_wifi_ssid);
    size_t pass_len = sizeof(s_cfg_wifi_pass);
    size_t panel_len = sizeof(s_cfg_panel_name);
    char lang_str[8] = {0};
    size_t lang_len = sizeof(lang_str);

    s_cfg_wifi_ssid[0] = '\0';
    s_cfg_wifi_pass[0] = '\0';
    s_cfg_panel_name[0] = '\0';
    s_cfg_ui_lang = AUX_PANEL_UI_LANG_EN;

    if (nvs_open(AUX_PANEL_CFG_NAMESPACE, NVS_READONLY, &handle) == ESP_OK) {
        (void)nvs_get_str(handle, AUX_PANEL_CFG_WIFI_SSID_KEY, s_cfg_wifi_ssid, &ssid_len);
        (void)nvs_get_str(handle, AUX_PANEL_CFG_WIFI_PASS_KEY, s_cfg_wifi_pass, &pass_len);
        (void)nvs_get_str(handle, AUX_PANEL_CFG_PANEL_NAME_KEY, s_cfg_panel_name, &panel_len);
        if (nvs_get_str(handle, AUX_PANEL_CFG_LANG_KEY, lang_str, &lang_len) == ESP_OK) {
            s_cfg_ui_lang = (strcmp(lang_str, "zh") == 0) ? AUX_PANEL_UI_LANG_ZH : AUX_PANEL_UI_LANG_EN;
        }
        nvs_close(handle);
    }

    if (s_cfg_wifi_ssid[0] == '\0') {
        nvs_handle_t legacy_handle;
        size_t legacy_ssid_len = sizeof(s_cfg_wifi_ssid);
        size_t legacy_pass_len = sizeof(s_cfg_wifi_pass);
        if (nvs_open("storage", NVS_READONLY, &legacy_handle) == ESP_OK) {
            (void)nvs_get_str(legacy_handle, "wifi_ssid", s_cfg_wifi_ssid, &legacy_ssid_len);
            (void)nvs_get_str(legacy_handle, "wifi_pass", s_cfg_wifi_pass, &legacy_pass_len);
            nvs_close(legacy_handle);
        }
    }

    if (s_cfg_wifi_ssid[0] == '\0' && AUX_PANEL_WIFI_SSID[0] != '\0') {
        copy_string_field(AUX_PANEL_WIFI_SSID, s_cfg_wifi_ssid, sizeof(s_cfg_wifi_ssid));
        copy_string_field(AUX_PANEL_WIFI_PASS, s_cfg_wifi_pass, sizeof(s_cfg_wifi_pass));
    }
    if (s_cfg_panel_name[0] == '\0') {
        copy_string_field(AUX_PANEL_CLIENT_NAME, s_cfg_panel_name, sizeof(s_cfg_panel_name));
    }

    ESP_LOGI(TAG,
             "Runtime config loaded: ssid=%s panel_name=%s lang=%s",
             s_cfg_wifi_ssid[0] ? s_cfg_wifi_ssid : "<empty>",
             s_cfg_panel_name[0] ? s_cfg_panel_name : "<empty>",
             s_cfg_ui_lang == AUX_PANEL_UI_LANG_ZH ? "zh" : "en");

    return ESP_OK;
}

static esp_err_t save_runtime_config(const char *ssid, const char *pass, const char *panel_name)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(AUX_PANEL_CFG_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, AUX_PANEL_CFG_WIFI_SSID_KEY, ssid ? ssid : "");
    if (err == ESP_OK) {
        err = nvs_set_str(handle, AUX_PANEL_CFG_WIFI_PASS_KEY, pass ? pass : "");
    }
    if (err == ESP_OK) {
        err = nvs_set_str(handle, AUX_PANEL_CFG_PANEL_NAME_KEY, panel_name ? panel_name : "");
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);

    if (err == ESP_OK) {
        copy_string_field(ssid, s_cfg_wifi_ssid, sizeof(s_cfg_wifi_ssid));
        copy_string_field(pass, s_cfg_wifi_pass, sizeof(s_cfg_wifi_pass));
        copy_string_field(panel_name, s_cfg_panel_name, sizeof(s_cfg_panel_name));
        update_settings_ui_texts();
        ESP_LOGI(TAG,
                 "Runtime config saved: ssid=%s panel_name=%s",
                 s_cfg_wifi_ssid[0] ? s_cfg_wifi_ssid : "<empty>",
                 s_cfg_panel_name[0] ? s_cfg_panel_name : "<empty>");
    }
    return err;
}

static esp_err_t save_ui_language_config(aux_panel_ui_language_t lang)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(AUX_PANEL_CFG_NAMESPACE, NVS_READWRITE, &handle);
    const char *lang_str = (lang == AUX_PANEL_UI_LANG_ZH) ? "zh" : "en";

    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_str(handle, AUX_PANEL_CFG_LANG_KEY, lang_str);
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err == ESP_OK) {
        s_cfg_ui_lang = lang;
        ESP_LOGI(TAG, "UI language saved: %s", lang_str);
    }
    return err;
}

static void ui_language_changed_callback(aux_panel_ui_language_t lang)
{
    (void)save_ui_language_config(lang);
}

static esp_err_t apply_saved_wifi_runtime(void)
{
    wifi_config_t wifi_sta_config = {0};
    esp_err_t err = ESP_OK;

    if (s_cfg_wifi_ssid[0] == '\0') {
        (void)esp_wifi_disconnect();
        s_wifi_connected = false;
        copy_string_field("0.0.0.0", s_sta_ip_text, sizeof(s_sta_ip_text));
        err = force_setup_ap_mode();
        if (err != ESP_OK) {
            return err;
        }
        update_settings_ui_texts();
        (void)ensure_setup_http_server();
        return ESP_OK;
    }

    copy_string_field(s_cfg_wifi_ssid, (char *)wifi_sta_config.sta.ssid, sizeof(wifi_sta_config.sta.ssid));
    copy_string_field(s_cfg_wifi_pass, (char *)wifi_sta_config.sta.password, sizeof(wifi_sta_config.sta.password));
    wifi_sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifi_sta_config.sta.pmf_cfg.capable = true;
    wifi_sta_config.sta.pmf_cfg.required = false;

    err = update_wifi_mode_for_connectivity(false);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to switch Wi-Fi mode before STA config: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to apply STA config for ssid '%s': %s",
                 s_cfg_wifi_ssid,
                 esp_err_to_name(err));
        return err;
    }
    (void)esp_wifi_disconnect();
    err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to start STA connect for ssid '%s': %s",
                 s_cfg_wifi_ssid,
                 esp_err_to_name(err));
        return err;
    }
    return ESP_OK;
}

static esp_err_t forget_saved_wifi_runtime(void)
{
    esp_err_t err = save_runtime_config("", "", s_cfg_panel_name);

    if (err != ESP_OK) {
        return err;
    }

    if (!s_wifi_stack_ready) {
        s_wifi_connected = false;
        copy_string_field("0.0.0.0", s_sta_ip_text, sizeof(s_sta_ip_text));
        update_settings_ui_texts();
        return ESP_OK;
    }

    return apply_saved_wifi_runtime();
}

static esp_err_t force_setup_ap_mode(void)
{
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) {
        err = configure_setup_ap();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to configure setup AP after mode switch: %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "Setup AP+STA mode restored");
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Direct switch to AP+STA mode failed: %s; retrying with stop/start", esp_err_to_name(err));

    esp_err_t stop_err = esp_wifi_stop();
    if (stop_err != ESP_OK && stop_err != ESP_ERR_WIFI_NOT_STOPPED) {
        ESP_LOGW(TAG, "Failed to stop Wi-Fi before AP recovery: %s", esp_err_to_name(stop_err));
        return err;
    }

    err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set AP+STA mode after stop: %s", esp_err_to_name(err));
        return err;
    }

    err = configure_setup_ap();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to reconfigure setup AP after stop: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to restart Wi-Fi in AP+STA mode: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "Setup AP+STA mode restored after Wi-Fi restart");
    return ESP_OK;
}

static const char *wifi_auth_mode_label(wifi_auth_mode_t authmode)
{
    switch (authmode) {
        case WIFI_AUTH_OPEN:
            return "Open";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA/WPA2";
        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "WPA2-Ent";
        case WIFI_AUTH_WPA3_PSK:
            return "WPA3";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2/WPA3";
        case WIFI_AUTH_WAPI_PSK:
            return "WAPI";
        case WIFI_AUTH_OWE:
            return "OWE";
        default:
            return "Secure";
    }
}

static bool wifi_auth_requires_password(wifi_auth_mode_t authmode)
{
    return !(authmode == WIFI_AUTH_OPEN || authmode == WIFI_AUTH_OWE);
}

static size_t build_wifi_scan_list_html(char *out, size_t out_size)
{
    wifi_ap_record_t records[AUX_PANEL_WIFI_SCAN_MAX_AP] = {0};
    uint16_t ap_count = AUX_PANEL_WIFI_SCAN_MAX_AP;
    wifi_scan_config_t scan_cfg = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
    };
    size_t used = 0;

    if (!out || out_size == 0) {
        return 0;
    }
    out[0] = '\0';

    esp_err_t mode_err = update_wifi_mode_for_connectivity(false);
    if (mode_err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to enable scan-capable Wi-Fi mode: %s", esp_err_to_name(mode_err));
        return 0;
    }

    esp_err_t scan_err = esp_wifi_scan_start(&scan_cfg, true);
    if (scan_err != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi scan failed: %s", esp_err_to_name(scan_err));
        return 0;
    }

    esp_err_t records_err = esp_wifi_scan_get_ap_records(&ap_count, records);
    if (records_err != ESP_OK || ap_count == 0) {
        if (records_err != ESP_OK) {
            ESP_LOGW(TAG, "Failed to fetch Wi-Fi scan records: %s", esp_err_to_name(records_err));
        }
        int len = snprintf(out,
                           out_size,
                           "<div class='scan-empty'>No visible Wi-Fi networks found. You can still enter SSID "
                           "manually below.</div>");
        if (len > 0 && (size_t)len < out_size) {
            return (size_t)len;
        }
        return 0;
    }

    for (uint16_t i = 0; i < ap_count; ++i) {
        char ssid_html[(sizeof(records[i].ssid) + 1) * 6] = {0};
        const char *auth_label = wifi_auth_mode_label(records[i].authmode);
        const bool needs_password = wifi_auth_requires_password(records[i].authmode);
        const char *tag_class = needs_password ? "wifi-tag" : "wifi-tag wifi-tag-open";
        int len = 0;
        const char *ssid = (const char *)records[i].ssid;
        if (!ssid || ssid[0] == '\0') {
            continue;
        }
        html_escape_into(ssid, ssid_html, sizeof(ssid_html));

        len = snprintf(out + used,
                       out_size - used,
                       "<button class='wifi-item%s' type='button' data-ssid='%s' data-secure='%d' "
                       "onclick='selectNetwork(this)'>"
                       "<span class='wifi-name'>%s</span>"
                       "<span class='wifi-meta'><span class='%s'>%s</span><span>%d dBm</span></span>"
                       "</button>",
                       (s_cfg_wifi_ssid[0] != '\0' && strcmp(ssid, s_cfg_wifi_ssid) == 0) ? " selected" : "",
                       ssid_html,
                       needs_password ? 1 : 0,
                       ssid_html,
                       tag_class,
                       auth_label,
                       (int)records[i].rssi);
        if (len <= 0 || (size_t)len >= (out_size - used)) {
            break;
        }
        used += (size_t)len;
    }

    return used;
}

static esp_err_t setup_get_handler(httpd_req_t *req)
{
    char *info_chunk = NULL;
    char wifi_ssid_html[AUX_PANEL_WIFI_SSID_LEN * 6] = {0};
    char wifi_pass_html[AUX_PANEL_WIFI_PASS_LEN * 6] = {0};
    char *wifi_list_html = NULL;
    size_t wifi_list_len = 0;
    int len = 0;

    info_chunk = (char *)calloc(1, 8192);
    wifi_list_html = (char *)calloc(1, 8192);
    if (!info_chunk || !wifi_list_html) {
        free(info_chunk);
        free(wifi_list_html);
        return ESP_FAIL;
    }

    html_escape_into(s_cfg_wifi_ssid, wifi_ssid_html, sizeof(wifi_ssid_html));
    html_escape_into(s_cfg_wifi_pass, wifi_pass_html, sizeof(wifi_pass_html));
    wifi_list_len = build_wifi_scan_list_html(wifi_list_html, 8192);

    if (httpd_resp_set_type(req, "text/html") != ESP_OK) {
        goto fail;
    }

    if (httpd_resp_sendstr_chunk(req,
                                 "<!doctype html><html lang='en'><head><meta charset='utf-8'>"
                                 "<meta name='viewport' content='width=device-width,initial-scale=1'>"
                                 "<title>Meeting Room Setup</title><style>"
                                 ":root{color-scheme:dark;--bg:#0f1218;--panel:#171c25;--muted:#98a2b3;--line:#273142;--accent:#3fb983;--accent2:#5ba6ff;}"
                                 "*{box-sizing:border-box}"
                                 "body{margin:0;min-height:100vh;padding:24px;font:16px/1.45 -apple-system,BlinkMacSystemFont,'Segoe UI',sans-serif;color:#fff;background:radial-gradient(circle at top,#1a2330,#0b0d12 68%);display:flex;align-items:center;justify-content:center}"
                                 ".card{width:min(760px,100%);background:rgba(23,28,37,.96);border:1px solid var(--line);border-radius:24px;padding:28px;box-shadow:0 24px 60px rgba(0,0,0,.35)}"
                                 "h1{margin:0 0 10px;font-size:32px}.lead{margin:0 0 18px;color:var(--muted)}"
                                 ".ap{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px;margin:0 0 20px}"
                                 ".ap div{padding:14px 16px;border-radius:16px;background:#11161f;border:1px solid var(--line)}"
                                 ".ap small{display:block;margin-bottom:6px;color:var(--muted);text-transform:uppercase;letter-spacing:.06em;font-size:11px}"
                                 ".scan{display:grid;gap:12px;margin:0 0 22px}.scan h2{margin:0;font-size:20px}.scan p{margin:0;color:var(--muted);font-size:14px}"
                                 ".picker{padding:12px 14px;border-radius:14px;background:#10151d;border:1px solid var(--line);color:#dbe4f3;font-size:14px}"
                                 ".wifi-list{display:grid;gap:10px;max-height:280px;overflow:auto;padding-right:4px}"
                                 ".wifi-item{width:100%;display:flex;align-items:center;justify-content:space-between;gap:14px;padding:14px 16px;border-radius:16px;border:1px solid #2f3b4f;background:#10151d;color:#fff;text-align:left;cursor:pointer}"
                                 ".wifi-item:hover{border-color:#4a6287}.wifi-item.selected{border-color:var(--accent2);box-shadow:0 0 0 1px rgba(91,166,255,.32) inset}"
                                 ".wifi-name{display:block;font-weight:700}.wifi-meta{display:flex;align-items:center;justify-content:flex-end;gap:10px;flex-wrap:wrap;color:var(--muted);font-size:13px}"
                                 ".wifi-tag{display:inline-flex;align-items:center;padding:5px 8px;border-radius:999px;background:rgba(63,185,131,.16);color:#d8ffe8;font-size:12px}"
                                 ".wifi-tag-open{background:rgba(91,166,255,.16);color:#dbe9ff}"
                                 ".scan-empty{padding:16px;border-radius:16px;background:#10151d;border:1px dashed #324055;color:var(--muted)}"
                                 "form{display:grid;gap:16px}label{display:grid;gap:8px;font-weight:600}"
                                 "input{width:100%;border:1px solid #324055;border-radius:14px;background:#0f141c;color:#fff;padding:14px 16px;font:inherit}"
                                 "button{border:0;border-radius:14px;padding:14px 18px;font:inherit;font-weight:700;cursor:pointer;background:linear-gradient(135deg,var(--accent),var(--accent2));color:#081018}"
                                 ".foot{margin-top:16px;color:var(--muted);font-size:13px}"
                                 "@media(max-width:640px){body{padding:14px}.card{padding:18px;border-radius:20px}h1{font-size:26px}.wifi-item{align-items:flex-start;flex-direction:column}.wifi-meta{justify-content:flex-start}}"
                                 "</style></head><body><div class='card'>"
                                 "<h1>Wi-Fi Setup</h1>"
                                 "<div class='scan'><h2>Available Networks</h2>"
                                 "<div class='picker' id='selectedNote'>Select a network from the list.</div>"
                                 "<div class='wifi-list'>") != ESP_OK) {
        goto fail;
    }

    if (wifi_list_len == 0) {
        if (httpd_resp_sendstr_chunk(req,
                                     "<div class='scan-empty'>No visible Wi-Fi networks found. You can still enter SSID manually below.</div>") != ESP_OK) {
            goto fail;
        }
    } else if (httpd_resp_send_chunk(req, wifi_list_html, wifi_list_len) != ESP_OK) {
        goto fail;
    }

    len = snprintf(info_chunk,
                   8192,
                   "</div></div><form method='POST' action='/save'>"
                   "<label>Wi-Fi SSID<input type='text' name='wifi_ssid' maxlength='32' required value='%s'></label>"
                   "<label>Wi-Fi Password<input type='password' name='wifi_pass' maxlength='64' value='%s'></label>"
                   "<button type='submit'>Save Settings</button></form>"
                   "<script>"
                   "function selectNetwork(btn){"
                   "var ssid=btn.getAttribute('data-ssid')||'';"
                   "var secure=btn.getAttribute('data-secure')==='1';"
                   "var ssidInput=document.querySelector(\"input[name='wifi_ssid']\");"
                   "var passInput=document.querySelector(\"input[name='wifi_pass']\");"
                   "var note=document.getElementById('selectedNote');"
                   "var selected=document.querySelectorAll('.wifi-item.selected');"
                   "for(var i=0;i<selected.length;i++){selected[i].classList.remove('selected');}"
                   "btn.classList.add('selected');ssidInput.value=ssid;"
                   "if(secure){note.textContent='Selected ' + ssid + '. Verify or enter the password below.';}"
                   "else{passInput.value='';note.textContent='Selected ' + ssid + '. This network does not require a password.';}"
                   "passInput.focus();"
                   "}"
                   "</script></div></body></html>",
                   wifi_ssid_html,
                   wifi_pass_html);
    if (len < 0 || (size_t)len >= 8192) {
        goto fail;
    }
    if (httpd_resp_send_chunk(req, info_chunk, len) != ESP_OK) {
        goto fail;
    }

    free(info_chunk);
    free(wifi_list_html);
    return httpd_resp_sendstr_chunk(req, NULL);

fail:
    free(info_chunk);
    free(wifi_list_html);
    return ESP_FAIL;
}

static esp_err_t setup_save_handler(httpd_req_t *req)
{
    char body[512] = {0};
    int body_len = req->content_len;
    int total_read = 0;
    char ssid[AUX_PANEL_WIFI_SSID_LEN] = {0};
    char pass[AUX_PANEL_WIFI_PASS_LEN] = {0};

    if (body_len <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid payload");
    }
    if (body_len >= (int)sizeof(body)) {
        return httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Payload too large");
    }

    while (total_read < body_len) {
        int read_now = httpd_req_recv(req, body + total_read, body_len - total_read);
        if (read_now <= 0) {
            return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Incomplete payload");
        }
        total_read += read_now;
    }

    if (total_read <= 0) {
        return httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Empty payload");
    }
    body[total_read] = '\0';

    (void)form_get_value(body, "wifi_ssid", ssid, sizeof(ssid));
    (void)form_get_value(body, "wifi_pass", pass, sizeof(pass));

    if (save_runtime_config(ssid, pass, s_cfg_panel_name) != ESP_OK) {
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save settings");
    }

    esp_err_t apply_err = apply_saved_wifi_runtime();
    if (apply_err != ESP_OK) {
        ESP_LOGW(TAG, "apply_saved_wifi_runtime failed after setup save: %s", esp_err_to_name(apply_err));
        return httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to apply Wi-Fi settings");
    }

    httpd_resp_set_type(req, "text/html");
    httpd_resp_sendstr(req,
                       "<!doctype html><html><head><meta charset='utf-8'>"
                       "<meta name='viewport' content='width=device-width,initial-scale=1'></head>"
                       "<body><h3>Changes are being applied...</h3></body></html>");
    return ESP_OK;
}

static esp_err_t ensure_setup_http_server(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    httpd_uri_t uri_get = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = setup_get_handler,
        .user_ctx = NULL,
    };
    httpd_uri_t uri_save = {
        .uri = "/save",
        .method = HTTP_POST,
        .handler = setup_save_handler,
        .user_ctx = NULL,
    };

    if (s_setup_httpd) {
        return ESP_OK;
    }

    config.stack_size = AUX_PANEL_HTTPD_STACK_SIZE;
    config.max_uri_handlers = AUX_PANEL_HTTPD_URI_LEN;
    if (httpd_start(&s_setup_httpd, &config) != ESP_OK) {
        s_setup_httpd = NULL;
        return ESP_FAIL;
    }

    httpd_register_uri_handler(s_setup_httpd, &uri_get);
    httpd_register_uri_handler(s_setup_httpd, &uri_save);
    ESP_LOGI(TAG, "Device setup portal started");
    return ESP_OK;
}

static bool lcd_flush_ready_cb(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    (void)panel_io;
    (void)edata;

    lv_disp_flush_ready((lv_disp_drv_t *)user_ctx);
    return false;
}

static inline uint8_t rgb565_expand_5_to_8(uint16_t value)
{
    return (uint8_t)((value * 255U) / 31U);
}

static inline uint8_t rgb565_expand_6_to_8(uint16_t value)
{
    return (uint8_t)((value * 255U) / 63U);
}

static void convert_rgb565_to_rgb888(const lv_color_t *src, uint8_t *dst, size_t pixel_count)
{
    for (size_t i = 0; i < pixel_count; ++i) {
        const uint16_t color = src[i].full;
        dst[(i * 3U) + 0U] = rgb565_expand_5_to_8((color >> 11) & 0x1FU);
        dst[(i * 3U) + 1U] = rgb565_expand_6_to_8((color >> 5) & 0x3FU);
        dst[(i * 3U) + 2U] = rgb565_expand_5_to_8(color & 0x1FU);
    }
}

static void lcd_tx_param(uint8_t cmd, const void *data, size_t data_size)
{
    ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(s_panel_io, cmd, data, data_size));
}

static void lcd_set_addr_window(int x_start, int y_start, int x_end, int y_end)
{
    const uint8_t col_data[] = {
        (uint8_t)(x_start >> 8),
        (uint8_t)(x_start & 0xFF),
        (uint8_t)(x_end >> 8),
        (uint8_t)(x_end & 0xFF),
    };
    const uint8_t row_data[] = {
        (uint8_t)(y_start >> 8),
        (uint8_t)(y_start & 0xFF),
        (uint8_t)(y_end >> 8),
        (uint8_t)(y_end & 0xFF),
    };

    lcd_tx_param(ILI9488_CMD_CASET, col_data, sizeof(col_data));
    lcd_tx_param(ILI9488_CMD_PASET, row_data, sizeof(row_data));
}

static void lcd_flush_cb(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_map)
{
    const int width = area->x2 - area->x1 + 1;
    const int height = area->y2 - area->y1 + 1;
    const size_t pixel_count = (size_t)width * (size_t)height;
    const size_t tx_size = pixel_count * 3U;

    if (tx_size > s_flush_rgb_buf_size) {
        ESP_LOGE(TAG, "Flush buffer too small: need %u bytes, have %u bytes",
                 (unsigned)tx_size, (unsigned)s_flush_rgb_buf_size);
        lv_disp_flush_ready(disp_drv);
        return;
    }

    convert_rgb565_to_rgb888(color_map, s_flush_rgb_buf, pixel_count);
    lcd_set_addr_window(area->x1, area->y1, area->x2, area->y2);

    const esp_err_t err = esp_lcd_panel_io_tx_color(s_panel_io, ILI9488_CMD_RAMWR, s_flush_rgb_buf, tx_size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LCD flush failed: %s", esp_err_to_name(err));
        lv_disp_flush_ready(disp_drv);
    }
}

static void lvgl_tick_cb(void *arg)
{
    (void)arg;
    lv_tick_inc(AUX_PANEL_TICK_MS);
}

static esp_err_t touch_i2c_probe_addr(uint8_t addr)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    esp_err_t err;

    if (!cmd) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(AUX_PANEL_TOUCH_I2C_PORT, cmd, pdMS_TO_TICKS(30));
    i2c_cmd_link_delete(cmd);
    return err;
}

static esp_err_t touch_i2c_init_bus(int sda_pin, int scl_pin)
{
    const i2c_config_t i2c_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = sda_pin,
        .scl_io_num = scl_pin,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = AUX_PANEL_TOUCH_I2C_FREQ_HZ,
        .clk_flags = 0,
    };
    esp_err_t err = i2c_param_config(AUX_PANEL_TOUCH_I2C_PORT, &i2c_cfg);
    if (err != ESP_OK) {
        return err;
    }
    err = i2c_driver_install(AUX_PANEL_TOUCH_I2C_PORT, I2C_MODE_MASTER, 0, 0, 0);
    if (err == ESP_ERR_INVALID_STATE) {
        err = ESP_OK;
        s_touch_i2c_installed = true;
    } else if (err == ESP_OK) {
        s_touch_i2c_installed = true;
    }
    return err;
}

static void touch_i2c_deinit_bus(void)
{
    esp_err_t err;
    if (!s_touch_i2c_installed) {
        return;
    }
    err = i2c_driver_delete(AUX_PANEL_TOUCH_I2C_PORT);
    if (err == ESP_OK || err == ESP_ERR_INVALID_STATE) {
        s_touch_i2c_installed = false;
    }
}

static esp_err_t touch_i2c_write_read(uint8_t addr,
                                      const uint8_t *wbuf,
                                      size_t wlen,
                                      uint8_t *rbuf,
                                      size_t rlen)
{
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    esp_err_t err = ESP_OK;

    if (!cmd) {
        return ESP_ERR_NO_MEM;
    }

    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_WRITE), true);
    if (wbuf && wlen > 0) {
        i2c_master_write(cmd, (uint8_t *)wbuf, (size_t)wlen, true);
    }

    if (rbuf && rlen > 0) {
        i2c_master_start(cmd);
        i2c_master_write_byte(cmd, (uint8_t)((addr << 1) | I2C_MASTER_READ), true);
        if (rlen > 1) {
            i2c_master_read(cmd, rbuf, rlen - 1, I2C_MASTER_ACK);
        }
        i2c_master_read_byte(cmd, rbuf + (rlen - 1), I2C_MASTER_NACK);
    }

    i2c_master_stop(cmd);
    err = i2c_master_cmd_begin(AUX_PANEL_TOUCH_I2C_PORT, cmd, pdMS_TO_TICKS(30));
    i2c_cmd_link_delete(cmd);
    return err;
}

static esp_err_t touch_i2c_read_reg16(uint8_t addr, uint16_t reg, uint8_t *data, size_t len)
{
    uint8_t reg_buf[2] = {
        (uint8_t)((reg >> 8) & 0xFF),
        (uint8_t)(reg & 0xFF),
    };
    return touch_i2c_write_read(addr, reg_buf, sizeof(reg_buf), data, len);
}

static esp_err_t touch_i2c_read_reg8(uint8_t addr, uint8_t reg, uint8_t *data, size_t len)
{
    return touch_i2c_write_read(addr, &reg, 1, data, len);
}

static esp_err_t touch_i2c_write_reg16_u8(uint8_t addr, uint16_t reg, uint8_t value)
{
    uint8_t tx[3] = {
        (uint8_t)((reg >> 8) & 0xFF),
        (uint8_t)(reg & 0xFF),
        value,
    };
    return touch_i2c_write_read(addr, tx, sizeof(tx), NULL, 0);
}

static void touch_map_xy(uint16_t raw_x, uint16_t raw_y, uint16_t *x_out, uint16_t *y_out)
{
    bool do_swap = false;
    uint16_t x = raw_x;
    uint16_t y = raw_y;

    if (!x_out || !y_out) {
        return;
    }

    if (AUX_PANEL_TOUCH_SWAP_XY >= 0) {
        do_swap = AUX_PANEL_TOUCH_SWAP_XY != 0;
    } else {
        do_swap = (raw_x >= AUX_PANEL_HOR_RES || raw_y >= AUX_PANEL_VER_RES) &&
                  (raw_y < AUX_PANEL_HOR_RES && raw_x < AUX_PANEL_VER_RES);
    }

    if (do_swap) {
        uint16_t t = x;
        x = y;
        y = t;
    }

    if (AUX_PANEL_TOUCH_MIRROR_X) {
        x = (x < AUX_PANEL_HOR_RES) ? (uint16_t)(AUX_PANEL_HOR_RES - 1 - x) : 0;
    }
    if (AUX_PANEL_TOUCH_MIRROR_Y) {
        y = (y < AUX_PANEL_VER_RES) ? (uint16_t)(AUX_PANEL_VER_RES - 1 - y) : 0;
    }

    if (x >= AUX_PANEL_HOR_RES) {
        x = AUX_PANEL_HOR_RES - 1;
    }
    if (y >= AUX_PANEL_VER_RES) {
        y = AUX_PANEL_VER_RES - 1;
    }

#if AUX_PANEL_UI_ROTATE_180
    x = (uint16_t)(AUX_PANEL_HOR_RES - 1 - x);
    y = (uint16_t)(AUX_PANEL_VER_RES - 1 - y);
#endif

    *x_out = x;
    *y_out = y;
}

static bool touch_read_gt911(uint16_t *x, uint16_t *y)
{
    uint8_t status = 0;
    uint8_t point_buf[8] = {0};
    uint8_t count = 0;

    if (!x || !y) {
        return false;
    }

    if (touch_i2c_read_reg16(s_touch_i2c_addr, AUX_TOUCH_GT911_REG_STATUS, &status, 1) != ESP_OK) {
        return false;
    }
    if ((status & 0x80) == 0) {
        return false;
    }

    count = status & 0x0F;
    if (count == 0 || count > 5) {
        (void)touch_i2c_write_reg16_u8(s_touch_i2c_addr, AUX_TOUCH_GT911_REG_STATUS, 0);
        return false;
    }
    if (touch_i2c_read_reg16(s_touch_i2c_addr, AUX_TOUCH_GT911_REG_POINTS, point_buf, sizeof(point_buf)) != ESP_OK) {
        (void)touch_i2c_write_reg16_u8(s_touch_i2c_addr, AUX_TOUCH_GT911_REG_STATUS, 0);
        return false;
    }
    (void)touch_i2c_write_reg16_u8(s_touch_i2c_addr, AUX_TOUCH_GT911_REG_STATUS, 0);

    *x = (uint16_t)(((uint16_t)point_buf[1] << 8) | point_buf[0]);
    *y = (uint16_t)(((uint16_t)point_buf[3] << 8) | point_buf[2]);
    return true;
}

static bool touch_read_ft6236(uint16_t *x, uint16_t *y)
{
    uint8_t buf[5] = {0};
    uint8_t points = 0;

    if (!x || !y) {
        return false;
    }

    if (touch_i2c_read_reg8(s_touch_i2c_addr, AUX_TOUCH_FT_REG_TD_STATUS, buf, sizeof(buf)) != ESP_OK) {
        return false;
    }

    points = buf[0] & 0x0F;
    if (points == 0 || points > 2) {
        return false;
    }

    *x = (uint16_t)((((uint16_t)buf[1] & 0x0F) << 8) | buf[2]);
    *y = (uint16_t)((((uint16_t)buf[3] & 0x0F) << 8) | buf[4]);
    return true;
}

static bool touch_read_chsc6540(uint16_t *x, uint16_t *y)
{
    uint8_t buf[7] = {0};
    uint8_t points = 0;

    if (!x || !y) {
        return false;
    }

    if (touch_i2c_read_reg8(s_touch_i2c_addr, 0x00, buf, sizeof(buf)) != ESP_OK) {
        return false;
    }

    points = buf[2];
    if (points == 0) {
        return false;
    }

    *x = (uint16_t)((((uint16_t)buf[3] & 0x0F) << 8) | buf[4]);
    *y = (uint16_t)((((uint16_t)buf[5] & 0x0F) << 8) | buf[6]);
    return true;
}

static void touch_poll_once(void)
{
    bool touched = false;
    uint16_t raw_x = 0;
    uint16_t raw_y = 0;
    uint16_t mapped_x = 0;
    uint16_t mapped_y = 0;

    if (s_touch_ic == AUX_TOUCH_FT6236) {
        touched = touch_read_ft6236(&raw_x, &raw_y);
    } else if (s_touch_ic == AUX_TOUCH_GT911) {
        touched = touch_read_gt911(&raw_x, &raw_y);
    } else if (s_touch_ic == AUX_TOUCH_CHSC6540) {
        touched = touch_read_chsc6540(&raw_x, &raw_y);
    }

    if (!touched) {
        s_touch_pressed = false;
        return;
    }

    touch_map_xy(raw_x, raw_y, &mapped_x, &mapped_y);
    s_touch_x = mapped_x;
    s_touch_y = mapped_y;
    s_touch_pressed = true;
}

static void touch_poll_task(void *arg)
{
    (void)arg;

    while (true) {
        touch_poll_once();
        vTaskDelay(pdMS_TO_TICKS(AUX_PANEL_TOUCH_POLL_MS));
    }
}

static void lvgl_touch_read_cb(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    (void)indev_drv;
    if (s_touch_pressed) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = (lv_coord_t)s_touch_x;
        data->point.y = (lv_coord_t)s_touch_y;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void init_touch_controller(void)
{
    typedef struct {
        int sda;
        int scl;
    } i2c_pin_pair_t;
    const i2c_pin_pair_t candidates[] = {
        {AUX_PANEL_TOUCH_I2C_SDA, AUX_PANEL_TOUCH_I2C_SCL},
        {2, 1},
        {1, 2},
        {15, 16},
        {16, 15},
        {1, 3},
        {3, 1},
    };
    const int max_attempts = 12;
    esp_err_t err = ESP_OK;
    uint8_t prod_id[4] = {0};
    uint8_t chsc_id = 0;
    uint8_t ft_chip = 0;
    uint8_t ft_vendor = 0;

    s_touch_ic = AUX_TOUCH_NONE;
    s_touch_i2c_addr = 0;

    for (int attempt = 0; attempt < max_attempts; ++attempt) {
        for (size_t i = 0; i < (sizeof(candidates) / sizeof(candidates[0])); ++i) {
            bool duplicate = false;
            for (size_t j = 0; j < i; ++j) {
                if (candidates[j].sda == candidates[i].sda &&
                    candidates[j].scl == candidates[i].scl) {
                    duplicate = true;
                    break;
                }
            }
            if (duplicate) {
                continue;
            }

            touch_i2c_deinit_bus();
            err = touch_i2c_init_bus(candidates[i].sda, candidates[i].scl);
            if (err != ESP_OK) {
                continue;
            }

            if (touch_i2c_probe_addr(AUX_TOUCH_ADDR_FT6236) == ESP_OK) {
                (void)touch_i2c_read_reg8(AUX_TOUCH_ADDR_FT6236, AUX_TOUCH_FT_REG_CHIP_ID, &ft_chip, 1);
                (void)touch_i2c_read_reg8(AUX_TOUCH_ADDR_FT6236, AUX_TOUCH_FT_REG_VENDOR_ID, &ft_vendor, 1);
                s_touch_ic = AUX_TOUCH_FT6236;
                s_touch_i2c_addr = AUX_TOUCH_ADDR_FT6236;
                s_touch_i2c_sda_pin = candidates[i].sda;
                s_touch_i2c_scl_pin = candidates[i].scl;
                ESP_LOGI(TAG,
                         "Touch controller: FT6236/FT5x06 at 0x%02X on SDA=%d SCL=%d (chip=0x%02X vendor=0x%02X)",
                         s_touch_i2c_addr,
                         s_touch_i2c_sda_pin,
                         s_touch_i2c_scl_pin,
                         ft_chip,
                         ft_vendor);
                return;
            }

            if (touch_i2c_probe_addr(AUX_TOUCH_ADDR_GT911_PRIMARY) == ESP_OK &&
                touch_i2c_read_reg16(AUX_TOUCH_ADDR_GT911_PRIMARY, AUX_TOUCH_GT911_REG_PRODUCT, prod_id, sizeof(prod_id)) == ESP_OK) {
                s_touch_ic = AUX_TOUCH_GT911;
                s_touch_i2c_addr = AUX_TOUCH_ADDR_GT911_PRIMARY;
                s_touch_i2c_sda_pin = candidates[i].sda;
                s_touch_i2c_scl_pin = candidates[i].scl;
                ESP_LOGI(TAG, "Touch controller: GT911 at 0x%02X on SDA=%d SCL=%d",
                         s_touch_i2c_addr,
                         s_touch_i2c_sda_pin,
                         s_touch_i2c_scl_pin);
                return;
            }

            if (touch_i2c_probe_addr(AUX_TOUCH_ADDR_GT911_BACKUP) == ESP_OK &&
                touch_i2c_read_reg16(AUX_TOUCH_ADDR_GT911_BACKUP, AUX_TOUCH_GT911_REG_PRODUCT, prod_id, sizeof(prod_id)) == ESP_OK) {
                s_touch_ic = AUX_TOUCH_GT911;
                s_touch_i2c_addr = AUX_TOUCH_ADDR_GT911_BACKUP;
                s_touch_i2c_sda_pin = candidates[i].sda;
                s_touch_i2c_scl_pin = candidates[i].scl;
                ESP_LOGI(TAG, "Touch controller: GT911 at 0x%02X on SDA=%d SCL=%d",
                         s_touch_i2c_addr,
                         s_touch_i2c_sda_pin,
                         s_touch_i2c_scl_pin);
                return;
            }

            if (touch_i2c_probe_addr(AUX_TOUCH_ADDR_CHSC6540) == ESP_OK &&
                touch_i2c_read_reg8(AUX_TOUCH_ADDR_CHSC6540, 0xA7, &chsc_id, 1) == ESP_OK) {
                s_touch_ic = AUX_TOUCH_CHSC6540;
                s_touch_i2c_addr = AUX_TOUCH_ADDR_CHSC6540;
                s_touch_i2c_sda_pin = candidates[i].sda;
                s_touch_i2c_scl_pin = candidates[i].scl;
                ESP_LOGI(TAG,
                         "Touch controller: CHSC6540 at 0x%02X on SDA=%d SCL=%d (chip_id=0x%02X)",
                         s_touch_i2c_addr,
                         s_touch_i2c_sda_pin,
                         s_touch_i2c_scl_pin,
                         chsc_id);
                return;
            }

            if (touch_i2c_probe_addr(AUX_TOUCH_ADDR_CHSC6540) == ESP_OK) {
                s_touch_ic = AUX_TOUCH_CHSC6540;
                s_touch_i2c_addr = AUX_TOUCH_ADDR_CHSC6540;
                s_touch_i2c_sda_pin = candidates[i].sda;
                s_touch_i2c_scl_pin = candidates[i].scl;
                ESP_LOGI(TAG, "Touch controller: CHSC6540 at 0x%02X on SDA=%d SCL=%d",
                         s_touch_i2c_addr,
                         s_touch_i2c_sda_pin,
                         s_touch_i2c_scl_pin);
                return;
            }
        }

        if (attempt < (max_attempts - 1)) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    touch_i2c_deinit_bus();
    s_touch_ic = AUX_TOUCH_NONE;
    s_touch_i2c_addr = 0;
    ESP_LOGW(TAG,
             "Touch controller not detected (tested I2C pairs: %d/%d, 2/1, 1/2, 15/16, 16/15, 1/3, 3/1). UI taps will not work.",
             AUX_PANEL_TOUCH_I2C_SDA,
             AUX_PANEL_TOUCH_I2C_SCL);
}

static void init_lvgl_input(void)
{
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = lvgl_touch_read_cb;
    (void)lv_indev_drv_register(&indev_drv);
}

static void copy_string_field(const char *src, char *dst, size_t dst_size)
{
    if (!dst || dst_size == 0) {
        return;
    }

    dst[0] = '\0';
    if (!src || !src[0]) {
        return;
    }

    snprintf(dst, dst_size, "%s", src);
    dst[dst_size - 1] = '\0';
}

static esp_err_t save_target_filters_to_nvs(const char *main_id, const char *room_name)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(AUX_PANEL_PAIR_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, AUX_PANEL_TARGET_MAIN_ID_KEY, main_id ? main_id : "");
    if (err == ESP_OK) {
        err = nvs_set_str(handle, AUX_PANEL_TARGET_ROOM_NAME_KEY, room_name ? room_name : "");
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    return err;
}

static esp_err_t load_target_filters_from_nvs(char *main_id_out,
                                              size_t main_id_out_size,
                                              char *room_name_out,
                                              size_t room_name_out_size)
{
    nvs_handle_t handle;
    esp_err_t err;
    size_t main_len = main_id_out_size;
    size_t room_len = room_name_out_size;

    if (main_id_out && main_id_out_size > 0) {
        main_id_out[0] = '\0';
    }
    if (room_name_out && room_name_out_size > 0) {
        room_name_out[0] = '\0';
    }

    err = nvs_open(AUX_PANEL_PAIR_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    if (main_id_out && main_id_out_size > 0) {
        (void)nvs_get_str(handle, AUX_PANEL_TARGET_MAIN_ID_KEY, main_id_out, &main_len);
    }
    if (room_name_out && room_name_out_size > 0) {
        (void)nvs_get_str(handle, AUX_PANEL_TARGET_ROOM_NAME_KEY, room_name_out, &room_len);
    }

    nvs_close(handle);
    return ESP_OK;
}

static bool parse_clock_text_minutes(const char *clock_text, int *minutes_out)
{
    int hours = 0;
    int minutes = 0;

    if (!clock_text || !minutes_out) {
        return false;
    }

    if (sscanf(clock_text, "%d:%d", &hours, &minutes) != 2) {
        return false;
    }
    if (hours < 0 || hours > 23 || minutes < 0 || minutes > 59) {
        return false;
    }

    *minutes_out = (hours * 60) + minutes;
    return true;
}

static int infer_server_timezone_offset_sec(time_t server_time, const char *clock_text)
{
    struct tm utc_tm = {0};
    int clock_minutes = 0;
    int utc_minutes = 0;
    int offset_minutes = 0;

    if (server_time <= 0 || !parse_clock_text_minutes(clock_text, &clock_minutes)) {
        return 0;
    }

    gmtime_r(&server_time, &utc_tm);
    utc_minutes = (utc_tm.tm_hour * 60) + utc_tm.tm_min;
    offset_minutes = clock_minutes - utc_minutes;

    while (offset_minutes <= -720) {
        offset_minutes += 1440;
    }
    while (offset_minutes > 840) {
        offset_minutes -= 1440;
    }

    return offset_minutes * 60;
}

static void __attribute__((unused)) format_next_booking_date_text(time_t server_time,
                                                                  const char *clock_text,
                                                                  time_t next_start,
                                                                  char *out,
                                                                  size_t out_size)
{
    struct tm now_tm = {0};
    struct tm next_tm = {0};
    time_t adjusted_now = 0;
    time_t adjusted_next = 0;
    int offset_sec = 0;

    if (!out || out_size == 0) {
        return;
    }

    out[0] = '\0';
    if (server_time <= 0 || next_start <= 0) {
        return;
    }

    offset_sec = infer_server_timezone_offset_sec(server_time, clock_text);
    adjusted_now = server_time + offset_sec;
    adjusted_next = next_start + offset_sec;

    gmtime_r(&adjusted_now, &now_tm);
    gmtime_r(&adjusted_next, &next_tm);

    if (now_tm.tm_year == next_tm.tm_year && now_tm.tm_yday == next_tm.tm_yday) {
        return;
    }

    strftime(out, out_size, "%a, %d %b", &next_tm);
    out[out_size - 1] = '\0';
}

static void build_client_id(char *out, size_t out_size)
{
    uint8_t mac[6] = {0};

    if (!out || out_size == 0) {
        return;
    }

    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(out, out_size, "aux-%02X%02X%02X", mac[3], mac[4], mac[5]);
    out[out_size - 1] = '\0';
}

static void clear_pair_state(void);

static void get_target_filters(char *main_id_out,
                               size_t main_id_out_size,
                               char *room_name_out,
                               size_t room_name_out_size)
{
    if (main_id_out && main_id_out_size > 0) {
        main_id_out[0] = '\0';
    }
    if (room_name_out && room_name_out_size > 0) {
        room_name_out[0] = '\0';
    }

    if (s_pair_cfg_mutex) {
        xSemaphoreTake(s_pair_cfg_mutex, portMAX_DELAY);
    }
    copy_string_field(s_target_main_id, main_id_out, main_id_out_size);
    copy_string_field(s_target_room_name, room_name_out, room_name_out_size);
    if (s_pair_cfg_mutex) {
        xSemaphoreGive(s_pair_cfg_mutex);
    }
}

static void set_target_filters(const char *main_id, const char *room_name)
{
    char local_main_id[AUX_UDP_MAIN_ID_LEN] = {0};
    char local_room_name[AUX_UDP_ROOM_NAME_LEN] = {0};

    copy_string_field(main_id, local_main_id, sizeof(local_main_id));
    copy_string_field(room_name, local_room_name, sizeof(local_room_name));

    if (s_pair_cfg_mutex) {
        xSemaphoreTake(s_pair_cfg_mutex, portMAX_DELAY);
    }
    copy_string_field(local_main_id, s_target_main_id, sizeof(s_target_main_id));
    copy_string_field(local_room_name, s_target_room_name, sizeof(s_target_room_name));
    if (s_pair_cfg_mutex) {
        xSemaphoreGive(s_pair_cfg_mutex);
    }

    (void)save_target_filters_to_nvs(local_main_id, local_room_name);
}

static bool sockaddr_same_host(const struct sockaddr_in *a, const struct sockaddr_in *b)
{
    if (!a || !b) {
        return false;
    }
    return a->sin_addr.s_addr == b->sin_addr.s_addr;
}

static void set_paired_main_addr(const struct sockaddr_in *addr)
{
    if (!addr) {
        return;
    }

    s_paired_main_addr = *addr;
    s_paired_main_addr.sin_port = htons(AUX_UDP_PORT);
    s_have_paired_main_addr = true;
}

static void clear_paired_main_addr(void)
{
    memset(&s_paired_main_addr, 0, sizeof(s_paired_main_addr));
    s_have_paired_main_addr = false;
}

static void clear_discovery_cache(void)
{
    memset(s_discovered_mains, 0, sizeof(s_discovered_mains));
}

static void update_discovered_main(const aux_udp_packet_v1_t *packet,
                                   const struct sockaddr_in *source_addr)
{
    int slot = -1;
    int oldest_slot = -1;
    int64_t oldest_seen = 0;
    int64_t now_us = esp_timer_get_time();

    if (!packet || !source_addr || packet->main_id[0] == '\0') {
        return;
    }

    for (int i = 0; i < AUX_PANEL_DISCOVERY_MAX; ++i) {
        if (s_discovered_mains[i].valid &&
            strcmp(s_discovered_mains[i].main_id, packet->main_id) == 0) {
            slot = i;
            break;
        }
        if (!s_discovered_mains[i].valid && slot < 0) {
            slot = i;
        }
        if (s_discovered_mains[i].valid &&
            (oldest_slot < 0 || s_discovered_mains[i].last_seen_us < oldest_seen)) {
            oldest_slot = i;
            oldest_seen = s_discovered_mains[i].last_seen_us;
        }
    }

    if (slot < 0) {
        slot = (oldest_slot >= 0) ? oldest_slot : 0;
    }

    s_discovered_mains[slot].valid = true;
    copy_string_field(packet->main_id, s_discovered_mains[slot].main_id, sizeof(s_discovered_mains[slot].main_id));
    copy_string_field(packet->room_name, s_discovered_mains[slot].room_name, sizeof(s_discovered_mains[slot].room_name));
    s_discovered_mains[slot].addr = *source_addr;
    s_discovered_mains[slot].addr.sin_port = htons(AUX_UDP_PORT);
    s_discovered_mains[slot].last_seen_us = now_us;
    s_discovered_mains[slot].paired = (packet->flags & AUX_UDP_FLAG_MAIN_PAIRED) != 0;
    s_discovered_mains[slot].pair_mode = (packet->flags & AUX_UDP_FLAG_MAIN_PAIR_MODE) != 0;
}

static void log_discovered_mains_if_needed(void)
{
    char ip_text[16] = {0};
    int64_t now_us = esp_timer_get_time();

    if (s_last_discovery_log_us != 0 &&
        (now_us - s_last_discovery_log_us) < (AUX_PANEL_DISCOVERY_LOG_MS * 1000LL)) {
        return;
    }
    s_last_discovery_log_us = now_us;

    ESP_LOGI(TAG, "Discovered mains:");
    for (int i = 0; i < AUX_PANEL_DISCOVERY_MAX; ++i) {
        int64_t age_ms = 0;
        if (!s_discovered_mains[i].valid) {
            continue;
        }
        age_ms = (now_us - s_discovered_mains[i].last_seen_us) / 1000LL;
        if (age_ms > AUX_PANEL_DISCOVERY_TTL_MS) {
            continue;
        }
        inet_ntoa_r(s_discovered_mains[i].addr.sin_addr, ip_text, sizeof(ip_text));
        ESP_LOGI(TAG,
                 "  - main='%s' room='%s' ip=%s paired=%d pair_mode=%d age=%lldms",
                 s_discovered_mains[i].main_id,
                 s_discovered_mains[i].room_name,
                 ip_text,
                 s_discovered_mains[i].paired ? 1 : 0,
                 s_discovered_mains[i].pair_mode ? 1 : 0,
                 (long long)age_ms);
    }
}

static bool select_target_main_addr(struct sockaddr_in *out_addr)
{
    int best_idx = -1;
    int64_t best_seen = 0;
    int64_t now_us = esp_timer_get_time();
    char target_main_id[AUX_UDP_MAIN_ID_LEN] = {0};
    char target_room_name[AUX_UDP_ROOM_NAME_LEN] = {0};

    if (!out_addr) {
        return false;
    }

    get_target_filters(target_main_id, sizeof(target_main_id), target_room_name, sizeof(target_room_name));

    if (s_is_paired && s_have_paired_main_addr) {
        *out_addr = s_paired_main_addr;
        return true;
    }

    for (int i = 0; i < AUX_PANEL_DISCOVERY_MAX; ++i) {
        if (!s_discovered_mains[i].valid) {
            continue;
        }
        if ((now_us - s_discovered_mains[i].last_seen_us) > (AUX_PANEL_DISCOVERY_TTL_MS * 1000LL)) {
            continue;
        }
        if (s_is_paired &&
            s_paired_main_id[0] != '\0' &&
            strcmp(s_discovered_mains[i].main_id, s_paired_main_id) != 0) {
            continue;
        }
        if (target_main_id[0] != '\0' &&
            strcmp(s_discovered_mains[i].main_id, target_main_id) != 0) {
            continue;
        }
        if (target_room_name[0] != '\0' &&
            s_discovered_mains[i].room_name[0] != '\0' &&
            strcmp(s_discovered_mains[i].room_name, target_room_name) != 0) {
            continue;
        }
        if (best_idx < 0 || s_discovered_mains[i].last_seen_us > best_seen) {
            best_idx = i;
            best_seen = s_discovered_mains[i].last_seen_us;
        }
    }

    if (best_idx < 0) {
        return false;
    }

    *out_addr = s_discovered_mains[best_idx].addr;
    return true;
}

static void queue_pair_ui_snapshot(const aux_panel_ui_main_entry_t *entries,
                                   size_t count,
                                   const char *selected_main_id,
                                   const char *status_text)
{
    if (!s_ui_state_mutex) {
        return;
    }

    if (count > AUX_PANEL_UI_MAX_DISCOVERED) {
        count = AUX_PANEL_UI_MAX_DISCOVERED;
    }

    xSemaphoreTake(s_ui_state_mutex, portMAX_DELAY);
    memset(&s_pending_pair_ui, 0, sizeof(s_pending_pair_ui));
    for (size_t i = 0; i < count; ++i) {
        s_pending_pair_ui.entries[i] = entries[i];
    }
    s_pending_pair_ui.count = count;
    copy_string_field(selected_main_id, s_pending_pair_ui.selected_main_id, sizeof(s_pending_pair_ui.selected_main_id));
    copy_string_field(status_text, s_pending_pair_ui.status_text, sizeof(s_pending_pair_ui.status_text));
    s_pending_pair_ui.dirty = true;
    xSemaphoreGive(s_ui_state_mutex);
}

static void queue_settings_ui_texts(const char *wifi_text, const char *setup_text, const char *setup_url)
{
    if (!s_ui_state_mutex) {
        return;
    }

    xSemaphoreTake(s_ui_state_mutex, portMAX_DELAY);
    copy_string_field(wifi_text, s_pending_settings_ui.wifi_text, sizeof(s_pending_settings_ui.wifi_text));
    copy_string_field(setup_text, s_pending_settings_ui.setup_text, sizeof(s_pending_settings_ui.setup_text));
    copy_string_field(setup_url, s_pending_settings_ui.setup_url, sizeof(s_pending_settings_ui.setup_url));
    s_pending_settings_ui.dirty = true;
    xSemaphoreGive(s_ui_state_mutex);
}

static void publish_pair_ui_snapshot(void)
{
    aux_panel_ui_main_entry_t entries[AUX_PANEL_UI_MAX_DISCOVERED] = {0};
    size_t count = 0;
    int64_t now_us = esp_timer_get_time();
    char selected_main_id[AUX_UDP_MAIN_ID_LEN] = {0};
    char target_main_id[AUX_UDP_MAIN_ID_LEN] = {0};
    char target_room_name[AUX_UDP_ROOM_NAME_LEN] = {0};
    char selected_room_name[AUX_UDP_ROOM_NAME_LEN] = {0};
    char status_text[96] = {0};

    get_target_filters(target_main_id, sizeof(target_main_id), target_room_name, sizeof(target_room_name));

    for (int i = 0; i < AUX_PANEL_DISCOVERY_MAX && count < AUX_PANEL_UI_MAX_DISCOVERED; ++i) {
        if (!s_discovered_mains[i].valid) {
            continue;
        }
        if ((now_us - s_discovered_mains[i].last_seen_us) > (AUX_PANEL_DISCOVERY_TTL_MS * 1000LL)) {
            continue;
        }
        copy_string_field(s_discovered_mains[i].main_id, entries[count].main_id, sizeof(entries[count].main_id));
        copy_string_field(s_discovered_mains[i].room_name, entries[count].room_name, sizeof(entries[count].room_name));
        entries[count].pair_mode = s_discovered_mains[i].pair_mode;
        entries[count].paired = s_discovered_mains[i].paired;
        ++count;
    }

    if (s_is_paired && s_paired_main_id[0] != '\0') {
        copy_string_field(s_paired_main_id, selected_main_id, sizeof(selected_main_id));
        for (size_t i = 0; i < count; ++i) {
            if (strcmp(entries[i].main_id, s_paired_main_id) == 0) {
                copy_string_field(entries[i].room_name, selected_room_name, sizeof(selected_room_name));
                break;
            }
        }
        if (selected_room_name[0] != '\0') {
            snprintf(status_text, sizeof(status_text), "Selected room: %s", selected_room_name);
        } else {
            snprintf(status_text, sizeof(status_text), "Selected panel: %s", s_paired_main_id);
        }
    } else if (target_main_id[0] != '\0') {
        copy_string_field(target_main_id, selected_main_id, sizeof(selected_main_id));
        if (target_room_name[0] != '\0') {
            snprintf(status_text, sizeof(status_text), "Selected room: %s", target_room_name);
        } else {
            snprintf(status_text, sizeof(status_text), "Selected panel: %s", target_main_id);
        }
    } else if (count == 0) {
        snprintf(status_text, sizeof(status_text), "Searching main panels...");
    }
    status_text[sizeof(status_text) - 1] = '\0';

    queue_pair_ui_snapshot(entries, count, selected_main_id, status_text);
}

static void ui_main_selected_callback(const char *main_id, const char *room_name)
{
    if (!main_id || main_id[0] == '\0') {
        return;
    }

    ESP_LOGI(TAG, "UI selected main: id='%s' room='%s'", main_id, room_name ? room_name : "");
    set_target_filters(main_id, room_name ? room_name : "");
    clear_pair_state();
    s_have_last_udp_seq = false;
    s_last_state_rx_us = 0;
    s_force_pair_request = true;
    publish_pair_ui_snapshot();
}

static void ui_unlink_all_callback(void)
{
    ESP_LOGI(TAG, "UI requested unlink all");
    set_target_filters("", "");
    clear_pair_state();
    s_have_last_udp_seq = false;
    s_last_state_rx_us = 0;
    reset_main_ui_state();
    publish_pair_ui_snapshot();
}

static void ui_wifi_forget_callback(void)
{
    esp_err_t err = forget_saved_wifi_runtime();

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to forget Wi-Fi credentials: %s", esp_err_to_name(err));
        return;
    }

    set_target_filters("", "");
    clear_pair_state();
    clear_paired_main_addr();
    clear_discovery_cache();
    s_have_last_udp_seq = false;
    s_last_state_rx_us = 0;
    s_force_pair_request = false;
    reset_main_ui_state();
    publish_pair_ui_snapshot();
    ESP_LOGI(TAG, "Wi-Fi credentials forgotten");
}

static void queue_ui_state(const aux_panel_room_state_t *state)
{
    if (!state || !s_ui_state_mutex) {
        return;
    }

    xSemaphoreTake(s_ui_state_mutex, portMAX_DELAY);
    s_pending_ui_state = *state;
    s_has_pending_ui_state = true;
    xSemaphoreGive(s_ui_state_mutex);
}

static void reset_main_ui_state(void)
{
    aux_panel_room_state_t reset_state = {
        .clock_text = "--:--",
        .date_text = "--",
        .room_name = "",
        .occupied = false,
        .remaining_sec = 0,
    };

    copy_string_field(s_cfg_panel_name, reset_state.room_name, sizeof(reset_state.room_name));
    queue_ui_state(&reset_state);
}

static void apply_pending_ui_state(void)
{
    aux_panel_room_state_t state = {0};
    pending_pair_ui_t pair_ui = {0};
    pending_settings_ui_t settings_ui = {0};
    bool has_state = false;
    bool has_pair_ui = false;
    bool has_settings_ui = false;

    if (!s_ui_state_mutex) {
        return;
    }

    xSemaphoreTake(s_ui_state_mutex, portMAX_DELAY);
    if (s_has_pending_ui_state) {
        state = s_pending_ui_state;
        s_has_pending_ui_state = false;
        has_state = true;
    }
    if (s_pending_pair_ui.dirty) {
        pair_ui = s_pending_pair_ui;
        s_pending_pair_ui.dirty = false;
        has_pair_ui = true;
    }
    if (s_pending_settings_ui.dirty) {
        settings_ui = s_pending_settings_ui;
        s_pending_settings_ui.dirty = false;
        has_settings_ui = true;
    }
    xSemaphoreGive(s_ui_state_mutex);

    if (has_state) {
        aux_panel_ui_apply_state(&state);
    }
    if (has_pair_ui) {
        aux_panel_ui_update_discovered_mains(pair_ui.entries, pair_ui.count, pair_ui.selected_main_id);
        aux_panel_ui_set_pairing_status_text(pair_ui.status_text);
    }
    if (has_settings_ui) {
        aux_panel_ui_set_wifi_status_text(settings_ui.wifi_text);
        aux_panel_ui_set_device_setup_text(settings_ui.setup_text);
        aux_panel_ui_set_device_setup_qr_url(settings_ui.setup_url);
    }
}

static esp_err_t load_pair_state(void)
{
    nvs_handle_t handle;
    size_t main_id_len = sizeof(s_paired_main_id);
    size_t token_len = sizeof(s_pair_token);
    esp_err_t err;

    s_is_paired = false;
    s_paired_main_id[0] = '\0';
    s_pair_token[0] = '\0';
    clear_paired_main_addr();

    err = nvs_open(AUX_PANEL_PAIR_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    if (nvs_get_str(handle, AUX_PANEL_PAIR_MAIN_ID_KEY, s_paired_main_id, &main_id_len) == ESP_OK &&
        nvs_get_str(handle, AUX_PANEL_PAIR_TOKEN_KEY, s_pair_token, &token_len) == ESP_OK &&
        s_paired_main_id[0] != '\0' &&
        s_pair_token[0] != '\0') {
        s_is_paired = true;
    }

    nvs_close(handle);
    return s_is_paired ? ESP_OK : ESP_ERR_NOT_FOUND;
}

static esp_err_t save_pair_state(const char *main_id, const char *token)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(AUX_PANEL_PAIR_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    err = nvs_set_str(handle, AUX_PANEL_PAIR_MAIN_ID_KEY, main_id ? main_id : "");
    if (err == ESP_OK) {
        err = nvs_set_str(handle, AUX_PANEL_PAIR_TOKEN_KEY, token ? token : "");
    }
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }
    nvs_close(handle);
    if (err == ESP_OK) {
        copy_string_field(main_id, s_paired_main_id, sizeof(s_paired_main_id));
        copy_string_field(token, s_pair_token, sizeof(s_pair_token));
        s_is_paired = (s_paired_main_id[0] != '\0' && s_pair_token[0] != '\0');
    }
    return err;
}

static void clear_pair_state(void)
{
    (void)save_pair_state("", "");
    clear_paired_main_addr();
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        if (s_cfg_wifi_ssid[0] != '\0') {
            esp_wifi_connect();
        }
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        copy_string_field("0.0.0.0", s_sta_ip_text, sizeof(s_sta_ip_text));
        (void)update_wifi_mode_for_connectivity(false);
        update_settings_ui_texts();
        if (s_cfg_wifi_ssid[0] != '\0') {
            ESP_LOGW(TAG, "Wi-Fi disconnected, retrying");
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ip_event = (ip_event_got_ip_t *)event_data;
        s_wifi_connected = true;
        if (ip_event) {
            snprintf(s_sta_ip_text, sizeof(s_sta_ip_text), IPSTR, IP2STR(&ip_event->ip_info.ip));
            s_sta_ip_text[sizeof(s_sta_ip_text) - 1] = '\0';
        } else {
            copy_string_field("0.0.0.0", s_sta_ip_text, sizeof(s_sta_ip_text));
        }
        ESP_LOGI(TAG, "Wi-Fi connected (ip=%s)", s_sta_ip_text);
        (void)update_wifi_mode_for_connectivity(true);
        update_settings_ui_texts();
        (void)ensure_setup_http_server();
    }
}

static esp_err_t ensure_wifi_stack(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    uint8_t mac[6] = {0};

    if (s_wifi_stack_ready) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(s_setup_ap_ssid, sizeof(s_setup_ap_ssid), "Meeting-Room-%02X%02X", mac[4], mac[5]);
    s_setup_ap_ssid[sizeof(s_setup_ap_ssid) - 1] = '\0';
    s_wifi_stack_ready = true;
    return ESP_OK;
}

static esp_err_t configure_setup_ap(void)
{
    wifi_config_t wifi_ap_config = {0};

    copy_string_field(s_setup_ap_ssid, (char *)wifi_ap_config.ap.ssid, sizeof(wifi_ap_config.ap.ssid));
    wifi_ap_config.ap.ssid_len = (uint8_t)strlen((const char *)wifi_ap_config.ap.ssid);
    wifi_ap_config.ap.channel = AUX_PANEL_SETUP_AP_CHANNEL;
    wifi_ap_config.ap.max_connection = AUX_PANEL_SETUP_AP_MAX_CONN;
    wifi_ap_config.ap.authmode = WIFI_AUTH_OPEN;
    return esp_wifi_set_config(WIFI_IF_AP, &wifi_ap_config);
}

static esp_err_t update_wifi_mode_for_connectivity(bool connected)
{
    wifi_mode_t current_mode = WIFI_MODE_NULL;
    wifi_mode_t desired_mode = WIFI_MODE_APSTA;
    esp_err_t err = esp_wifi_get_mode(&current_mode);

    if (err != ESP_OK) {
        return err;
    }

    if (connected) {
        desired_mode = WIFI_MODE_STA;
    }

    if (current_mode == desired_mode) {
        return ESP_OK;
    }

    err = esp_wifi_set_mode(desired_mode);
    if (err != ESP_OK) {
        return err;
    }

    if (desired_mode != WIFI_MODE_STA) {
        err = configure_setup_ap();
        if (err != ESP_OK) {
            return err;
        }
    }

    ESP_LOGI(TAG,
             "Wi-Fi mode switched to %s",
             desired_mode == WIFI_MODE_STA ? "STA" :
             (desired_mode == WIFI_MODE_APSTA ? "AP+STA" : "AP"));
    return ESP_OK;
}

static esp_err_t init_wifi_station(void)
{
    wifi_config_t wifi_sta_config = {0};
    wifi_mode_t mode = WIFI_MODE_APSTA;

    ESP_ERROR_CHECK(ensure_wifi_stack());

    if (s_cfg_wifi_ssid[0] != '\0') {
        copy_string_field(s_cfg_wifi_ssid, (char *)wifi_sta_config.sta.ssid, sizeof(wifi_sta_config.sta.ssid));
        copy_string_field(s_cfg_wifi_pass, (char *)wifi_sta_config.sta.password, sizeof(wifi_sta_config.sta.password));
        wifi_sta_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
        wifi_sta_config.sta.pmf_cfg.capable = true;
        wifi_sta_config.sta.pmf_cfg.required = false;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(mode));
    ESP_ERROR_CHECK(configure_setup_ap());

    if (s_cfg_wifi_ssid[0] != '\0') {
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_sta_config));
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    if (s_cfg_wifi_ssid[0] != '\0') {
        ESP_LOGI(TAG, "Setup AP active until Wi-Fi connects: %s (http://192.168.4.1)", s_setup_ap_ssid);
        ESP_LOGI(TAG, "Connecting to Wi-Fi SSID: %s", s_cfg_wifi_ssid);
    } else {
        ESP_LOGI(TAG, "Setup AP active: %s (http://192.168.4.1)", s_setup_ap_ssid);
        ESP_LOGW(TAG, "No Wi-Fi credentials configured; use setup AP to configure");
    }
    update_settings_ui_texts();
    (void)ensure_setup_http_server();
    return ESP_OK;
}

static void update_settings_ui_texts(void)
{
    char wifi_text[96] = {0};
    char setup_text[220] = {0};
    char setup_url[64] = {0};

    if (s_wifi_connected && s_sta_ip_text[0] != '\0' && strcmp(s_sta_ip_text, "0.0.0.0") != 0) {
        snprintf(wifi_text, sizeof(wifi_text), "%s", s_cfg_wifi_ssid[0] ? s_cfg_wifi_ssid : "Connected");
        snprintf(setup_url, sizeof(setup_url), "http://%s", s_sta_ip_text);
        snprintf(setup_text,
                 sizeof(setup_text),
                 "Open setup:\n%s",
                 setup_url);
    } else {
        snprintf(wifi_text, sizeof(wifi_text), "Not connected");
        snprintf(setup_url, sizeof(setup_url), "http://192.168.4.1");
        snprintf(setup_text,
                 sizeof(setup_text),
                 "Connect to Wi-Fi:\n%s\nthen scan QR.",
                 s_setup_ap_ssid[0] ? s_setup_ap_ssid : "Meeting-Room");
    }
    wifi_text[sizeof(wifi_text) - 1] = '\0';
    setup_text[sizeof(setup_text) - 1] = '\0';
    setup_url[sizeof(setup_url) - 1] = '\0';
    queue_settings_ui_texts(wifi_text, setup_text, setup_url);
}

static void close_udp_rx_socket(void)
{
    if (s_udp_rx_sock >= 0) {
        close(s_udp_rx_sock);
        s_udp_rx_sock = -1;
    }
}

static esp_err_t ensure_udp_rx_socket(void)
{
    struct sockaddr_in bind_addr = {0};
    int reuse_addr = 1;
    int enable_broadcast = 1;
    struct timeval timeout = {
        .tv_sec = 0,
        .tv_usec = 500 * 1000,
    };

    if (s_udp_rx_sock >= 0) {
        return ESP_OK;
    }

    s_udp_rx_sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_udp_rx_sock < 0) {
        ESP_LOGW(TAG, "UDP socket create failed");
        return ESP_FAIL;
    }

    if (setsockopt(s_udp_rx_sock, SOL_SOCKET, SO_REUSEADDR, &reuse_addr, sizeof(reuse_addr)) < 0) {
        close_udp_rx_socket();
        return ESP_FAIL;
    }
    if (setsockopt(s_udp_rx_sock, SOL_SOCKET, SO_BROADCAST, &enable_broadcast, sizeof(enable_broadcast)) < 0) {
        close_udp_rx_socket();
        return ESP_FAIL;
    }
    if (setsockopt(s_udp_rx_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        close_udp_rx_socket();
        return ESP_FAIL;
    }

    bind_addr.sin_family = AF_INET;
    bind_addr.sin_port = htons(AUX_UDP_PORT);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(s_udp_rx_sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        close_udp_rx_socket();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Listening for aux UDP state on port %d", AUX_UDP_PORT);
    return ESP_OK;
}

static void build_broadcast_target(struct sockaddr_in *out_addr)
{
    esp_netif_t *sta_netif = NULL;
    esp_netif_ip_info_t ip_info = {0};

    if (!out_addr) {
        return;
    }

    memset(out_addr, 0, sizeof(*out_addr));
    out_addr->sin_family = AF_INET;
    out_addr->sin_port = htons(AUX_UDP_PORT);
    out_addr->sin_addr.s_addr = htonl(INADDR_BROADCAST);

    sta_netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!sta_netif) {
        return;
    }

    if (esp_netif_get_ip_info(sta_netif, &ip_info) != ESP_OK || ip_info.netmask.addr == 0) {
        return;
    }

    out_addr->sin_addr.s_addr = (ip_info.ip.addr & ip_info.netmask.addr) | (~ip_info.netmask.addr);
}

static void finalize_packet_crc(aux_udp_packet_v1_t *packet)
{
    if (!packet) {
        return;
    }
    packet->crc32 = 0;
    packet->crc32 = esp_rom_crc32_le(0, (const uint8_t *)packet, sizeof(*packet));
}

static bool validate_udp_packet(const aux_udp_packet_v1_t *packet, size_t packet_len)
{
    aux_udp_packet_v1_t packet_copy;
    uint32_t expected_crc = 0;

    if (!packet || packet_len != sizeof(aux_udp_packet_v1_t)) {
        return false;
    }

    packet_copy = *packet;
    if (packet_copy.magic != AUX_UDP_MAGIC ||
        packet_copy.version != AUX_UDP_VERSION ||
        packet_copy.packet_size != sizeof(aux_udp_packet_v1_t)) {
        return false;
    }

    expected_crc = packet_copy.crc32;
    packet_copy.crc32 = 0;
    return esp_rom_crc32_le(0, (const uint8_t *)&packet_copy, sizeof(packet_copy)) == expected_crc;
}

static void decode_udp_packet_to_state(const aux_udp_packet_v1_t *packet,
                                       aux_panel_room_state_t *state_out)
{
    struct tm date_tm = {0};
    int offset_sec = 0;
    time_t adjusted_server_time = 0;

    memset(state_out, 0, sizeof(*state_out));
    copy_string_field(packet->room_name[0] ? packet->room_name : s_cfg_panel_name,
                      state_out->room_name,
                      sizeof(state_out->room_name));
    copy_string_field(packet->clock_text[0] ? packet->clock_text : "--:--",
                      state_out->clock_text,
                      sizeof(state_out->clock_text));
    copy_string_field("--", state_out->date_text, sizeof(state_out->date_text));

    if (packet->sent_epoch_sec > 0) {
        offset_sec = infer_server_timezone_offset_sec((time_t)packet->sent_epoch_sec, state_out->clock_text);
        adjusted_server_time = (time_t)packet->sent_epoch_sec + offset_sec;
        if (gmtime_r(&adjusted_server_time, &date_tm)) {
            strftime(state_out->date_text, sizeof(state_out->date_text), "%A, %d %b", &date_tm);
            state_out->date_text[sizeof(state_out->date_text) - 1] = '\0';
        }
    }

    state_out->occupied = packet->occupied != 0;
    state_out->remaining_sec = packet->remaining_sec > 0 ? packet->remaining_sec : 0;
}

static esp_err_t send_pair_request(const struct sockaddr_in *target_addr)
{
    aux_udp_packet_v1_t request = {0};
    struct sockaddr_in target = {0};
    const struct sockaddr_in *dst = target_addr;
    char target_main_id[AUX_UDP_MAIN_ID_LEN] = {0};
    char target_room_name[AUX_UDP_ROOM_NAME_LEN] = {0};
    int sent = 0;

    if (ensure_udp_rx_socket() != ESP_OK) {
        return ESP_FAIL;
    }

    request.magic = AUX_UDP_MAGIC;
    request.version = AUX_UDP_VERSION;
    request.packet_size = (uint16_t)sizeof(request);
    request.msg_type = AUX_UDP_MSG_PAIR_REQ;
    request.seq = s_udp_seq++;
    request.sent_epoch_sec = (int64_t)time(NULL);
    get_target_filters(target_main_id, sizeof(target_main_id), target_room_name, sizeof(target_room_name));
    if (!s_is_paired && target_main_id[0] == '\0') {
        return ESP_ERR_INVALID_STATE;
    }
    if (s_is_paired && s_paired_main_id[0] != '\0') {
        copy_string_field(s_paired_main_id, request.main_id, sizeof(request.main_id));
        copy_string_field(s_pair_token, request.pair_token, sizeof(request.pair_token));
    } else {
        copy_string_field(target_main_id, request.main_id, sizeof(request.main_id));
    }
    copy_string_field(s_client_id, request.aux_id, sizeof(request.aux_id));
    copy_string_field(target_room_name, request.room_name, sizeof(request.room_name));
    finalize_packet_crc(&request);

    if (!dst) {
        build_broadcast_target(&target);
        dst = &target;
    }
    sent = sendto(s_udp_rx_sock,
                  &request,
                  sizeof(request),
                  0,
                  (const struct sockaddr *)dst,
                  sizeof(*dst));
    return (sent == (int)sizeof(request)) ? ESP_OK : ESP_FAIL;
}

static esp_err_t handle_pair_ack(const aux_udp_packet_v1_t *packet, const struct sockaddr_in *source_addr)
{
    char target_main_id[AUX_UDP_MAIN_ID_LEN] = {0};
    char target_room_name[AUX_UDP_ROOM_NAME_LEN] = {0};

    if (!packet) {
        return ESP_ERR_INVALID_ARG;
    }
    if (strcmp(packet->aux_id, s_client_id) != 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (packet->main_id[0] == '\0' || packet->pair_token[0] == '\0') {
        return ESP_ERR_INVALID_RESPONSE;
    }
    get_target_filters(target_main_id, sizeof(target_main_id), target_room_name, sizeof(target_room_name));
    if (s_is_paired) {
        if (strcmp(packet->main_id, s_paired_main_id) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
    } else {
        if (target_main_id[0] == '\0') {
            return ESP_ERR_INVALID_STATE;
        }
        if (strcmp(packet->main_id, target_main_id) != 0) {
            return ESP_ERR_INVALID_STATE;
        }
    }
    if (target_room_name[0] != '\0' &&
        packet->room_name[0] != '\0' &&
        strcmp(packet->room_name, target_room_name) != 0) {
        return ESP_ERR_INVALID_STATE;
    }

    if (save_pair_state(packet->main_id, packet->pair_token) != ESP_OK) {
        return ESP_FAIL;
    }
    if (source_addr) {
        set_paired_main_addr(source_addr);
    }

    ESP_LOGI(TAG, "Paired with main '%s' (room='%s')", s_paired_main_id, packet->room_name);
    return ESP_OK;
}

static void handle_beacon(const aux_udp_packet_v1_t *packet, const struct sockaddr_in *source_addr)
{
    if (!packet || !source_addr) {
        return;
    }

    update_discovered_main(packet, source_addr);

    if (s_is_paired &&
        packet->main_id[0] != '\0' &&
        strcmp(packet->main_id, s_paired_main_id) == 0) {
        if (!s_have_paired_main_addr || !sockaddr_same_host(&s_paired_main_addr, source_addr)) {
            set_paired_main_addr(source_addr);
            ESP_LOGI(TAG, "Updated paired main IP from beacon");
        }
    }
}

static esp_err_t fetch_state_from_master(void)
{
    aux_udp_packet_v1_t packet = {0};
    struct sockaddr_in source_addr = {0};
    socklen_t source_addr_len = sizeof(source_addr);
    int len = 0;
    aux_panel_room_state_t state = {0};
    if (ensure_udp_rx_socket() != ESP_OK) {
        return ESP_FAIL;
    }

    len = recvfrom(s_udp_rx_sock,
                   &packet,
                   sizeof(packet),
                   0,
                   (struct sockaddr *)&source_addr,
                   &source_addr_len);
    if (len < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return ESP_ERR_TIMEOUT;
        }
        close_udp_rx_socket();
        return ESP_FAIL;
    }

    if (!validate_udp_packet(&packet, (size_t)len)) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    if (packet.msg_type == AUX_UDP_MSG_BEACON) {
        handle_beacon(&packet, &source_addr);
        return ESP_ERR_TIMEOUT;
    }
    if (packet.msg_type == AUX_UDP_MSG_PAIR_ACK) {
        return handle_pair_ack(&packet, &source_addr);
    }
    if (packet.msg_type == AUX_UDP_MSG_PAIR_NACK) {
        if (strcmp(packet.aux_id, s_client_id) == 0) {
            ESP_LOGW(TAG, "Pair request rejected by main '%s'", packet.main_id);
        }
        return ESP_ERR_INVALID_STATE;
    }
    if (packet.msg_type != AUX_UDP_MSG_STATE) {
        return ESP_ERR_INVALID_RESPONSE;
    }
    if (!s_is_paired) {
        return ESP_ERR_INVALID_STATE;
    }
    if (strcmp(packet.main_id, s_paired_main_id) != 0 ||
        strcmp(packet.pair_token, s_pair_token) != 0) {
        return ESP_ERR_INVALID_STATE;
    }
    if (!s_have_paired_main_addr || !sockaddr_same_host(&s_paired_main_addr, &source_addr)) {
        set_paired_main_addr(&source_addr);
    }
    if (s_have_last_udp_seq && packet.seq == s_last_udp_seq) {
        return ESP_OK;
    }

    decode_udp_packet_to_state(&packet, &state);
    s_have_last_udp_seq = true;
    s_last_udp_seq = packet.seq;
    s_last_state_rx_us = esp_timer_get_time();
    queue_ui_state(&state);
    return ESP_OK;
}

static void aux_link_task(void *arg)
{
    int64_t last_pair_req_us = 0;
    int64_t last_ui_publish_us = 0;
    char target_main_id[AUX_UDP_MAIN_ID_LEN] = {0};
    char target_room_name[AUX_UDP_ROOM_NAME_LEN] = {0};

    (void)arg;

    build_client_id(s_client_id, sizeof(s_client_id));
    if (load_pair_state() == ESP_OK) {
        ESP_LOGI(TAG, "Loaded pair: main='%s'", s_paired_main_id);
    } else {
        ESP_LOGI(TAG, "No saved pair state, entering pairing mode");
    }
    if (AUX_PANEL_TARGET_MAIN_ID[0] != '\0' || AUX_PANEL_TARGET_ROOM_NAME[0] != '\0') {
        set_target_filters(AUX_PANEL_TARGET_MAIN_ID, AUX_PANEL_TARGET_ROOM_NAME);
    } else {
        char saved_target_main[AUX_UDP_MAIN_ID_LEN] = {0};
        char saved_target_room[AUX_UDP_ROOM_NAME_LEN] = {0};
        if (load_target_filters_from_nvs(saved_target_main, sizeof(saved_target_main),
                                         saved_target_room, sizeof(saved_target_room)) == ESP_OK &&
            (saved_target_main[0] != '\0' || saved_target_room[0] != '\0')) {
            if (s_pair_cfg_mutex) {
                xSemaphoreTake(s_pair_cfg_mutex, portMAX_DELAY);
            }
            copy_string_field(saved_target_main, s_target_main_id, sizeof(s_target_main_id));
            copy_string_field(saved_target_room, s_target_room_name, sizeof(s_target_room_name));
            if (s_pair_cfg_mutex) {
                xSemaphoreGive(s_pair_cfg_mutex);
            }
            ESP_LOGI(TAG, "Loaded selected target from NVS: main='%s' room='%s'",
                     saved_target_main,
                     saved_target_room);
        }
    }
    if (AUX_PANEL_RESET_PAIR_ON_BOOT) {
        clear_pair_state();
        ESP_LOGW(TAG, "Pair state cleared on boot by AUX_PANEL_RESET_PAIR_ON_BOOT");
    }
    get_target_filters(target_main_id, sizeof(target_main_id), target_room_name, sizeof(target_room_name));
    if (s_is_paired &&
        target_main_id[0] != '\0' &&
        strcmp(s_paired_main_id, target_main_id) != 0) {
        ESP_LOGW(TAG, "Stored pair main '%s' != target '%s', resetting pair",
                 s_paired_main_id, target_main_id);
        clear_pair_state();
    }
    ESP_LOGI(TAG,
             "Aux ID: %s, target_main='%s', target_room='%s'",
             s_client_id,
             target_main_id,
             target_room_name);

    if (init_wifi_station() != ESP_OK) {
        ESP_LOGW(TAG, "Wi-Fi init failed; link task will keep retrying UDP receive after reconnect");
    }

    while (true) {
        if (!s_wifi_connected) {
            close_udp_rx_socket();
            s_have_last_udp_seq = false;
            clear_paired_main_addr();
            clear_discovery_cache();
            s_last_state_rx_us = 0;
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        int64_t now_us = esp_timer_get_time();
        struct sockaddr_in pair_target = {0};
        bool have_pair_target = select_target_main_addr(&pair_target);
        bool has_manual_target = false;
        bool should_send_pair_req = false;

        get_target_filters(target_main_id, sizeof(target_main_id), target_room_name, sizeof(target_room_name));
        has_manual_target = target_main_id[0] != '\0';

        if (s_force_pair_request && (s_is_paired || has_manual_target)) {
            should_send_pair_req = true;
        }
        s_force_pair_request = false;

        if (!s_is_paired) {
            if (has_manual_target) {
                should_send_pair_req = should_send_pair_req ||
                                       (last_pair_req_us == 0 ||
                                        (now_us - last_pair_req_us) >= (AUX_PANEL_PAIR_REQ_INTERVAL_MS * 1000LL));
            } else {
                should_send_pair_req = false;
            }
        } else {
            bool state_stale = (s_last_state_rx_us == 0) ||
                               ((now_us - s_last_state_rx_us) >= (AUX_PANEL_STATE_STALE_MS * 1000LL));
            if (state_stale &&
                (last_pair_req_us == 0 ||
                 (now_us - last_pair_req_us) >= (AUX_PANEL_PAIR_REQ_INTERVAL_MS * 1000LL))) {
                should_send_pair_req = true;
            } else if (last_pair_req_us == 0 ||
                       (now_us - last_pair_req_us) >= (AUX_PANEL_PAIR_REFRESH_INTERVAL_MS * 1000LL)) {
                should_send_pair_req = true;
            }
        }

        if (should_send_pair_req) {
            if (send_pair_request(have_pair_target ? &pair_target : NULL) == ESP_OK) {
                ESP_LOGI(TAG, "Pair request sent (paired=%d target_main='%s' target_room='%s')",
                         s_is_paired ? 1 : 0,
                         target_main_id,
                         target_room_name);
            }
            last_pair_req_us = now_us;
        }

        (void)fetch_state_from_master();
        log_discovered_mains_if_needed();
        if (last_ui_publish_us == 0 || (now_us - last_ui_publish_us) >= 1000 * 1000LL) {
            publish_pair_ui_snapshot();
            last_ui_publish_us = now_us;
        }
        vTaskDelay(pdMS_TO_TICKS(AUX_PANEL_POLL_INTERVAL_MS));
    }
}

static void init_board_power(void)
{
    const gpio_config_t output_config = {
        .pin_bit_mask = (1ULL << AUX_PANEL_PIN_NUM_PANEL_EN)
                        | (1ULL << AUX_PANEL_PIN_NUM_LCD_POWER)
                        | (1ULL << AUX_PANEL_PIN_NUM_BK_LIGHT),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    ESP_ERROR_CHECK(gpio_config(&output_config));

    ESP_ERROR_CHECK(gpio_set_level(AUX_PANEL_PIN_NUM_PANEL_EN, 0));
    ESP_ERROR_CHECK(gpio_set_level(AUX_PANEL_PIN_NUM_LCD_POWER, 1));
    ESP_ERROR_CHECK(gpio_set_level(AUX_PANEL_PIN_NUM_BK_LIGHT, 0));
    vTaskDelay(pdMS_TO_TICKS(20));
}

static void init_lcd_panel(void)
{
    const uint8_t madctl = ILI9488_MADCTL_MV
#if AUX_PANEL_UI_ROTATE_180
                           | ILI9488_MADCTL_MX
                           | ILI9488_MADCTL_MY
#endif
                           | (AUX_PANEL_LCD_USE_BGR_ORDER ? ILI9488_MADCTL_BGR : 0);

    const spi_bus_config_t bus_config = {
        .sclk_io_num = AUX_PANEL_PIN_NUM_SCLK,
        .mosi_io_num = AUX_PANEL_PIN_NUM_MOSI,
        .miso_io_num = AUX_PANEL_PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = AUX_PANEL_HOR_RES * AUX_PANEL_BUF_LINES * 3,
    };

    ESP_ERROR_CHECK(spi_bus_initialize(AUX_PANEL_LCD_HOST, &bus_config, SPI_DMA_CH_AUTO));

    const esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = AUX_PANEL_PIN_NUM_LCD_DC,
        .cs_gpio_num = AUX_PANEL_PIN_NUM_LCD_CS,
        .pclk_hz = AUX_PANEL_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = AUX_PANEL_LCD_CMD_BITS,
        .lcd_param_bits = AUX_PANEL_LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        .on_color_trans_done = lcd_flush_ready_cb,
        .user_ctx = &s_disp_drv,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)AUX_PANEL_LCD_HOST, &io_config, &s_panel_io));

    lcd_tx_param(ILI9488_CMD_SWRESET, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));

    lcd_tx_param(0xE0, (uint8_t[]) {
        0x00, 0x03, 0x09, 0x08, 0x16, 0x0A, 0x3F, 0x78, 0x4C, 0x09, 0x0A, 0x08, 0x16, 0x1A, 0x0F
    }, 15);
    lcd_tx_param(0xE1, (uint8_t[]) {
        0x00, 0x16, 0x19, 0x03, 0x0F, 0x05, 0x32, 0x45, 0x46, 0x04, 0x0E, 0x0D, 0x35, 0x37, 0x0F
    }, 15);
    lcd_tx_param(0xC0, (uint8_t[]) {0x17, 0x15}, 2);
    lcd_tx_param(0xC1, (uint8_t[]) {0x41}, 1);
    lcd_tx_param(0xC5, (uint8_t[]) {0x00, 0x12, 0x80}, 3);
    lcd_tx_param(ILI9488_CMD_MADCTL, &madctl, 1);
    lcd_tx_param(ILI9488_CMD_COLMOD, (uint8_t[]) {0x66}, 1);
    lcd_tx_param(0xB0, (uint8_t[]) {0x80}, 1);
    lcd_tx_param(0xB1, (uint8_t[]) {0xA0}, 1);
    lcd_tx_param(0xB4, (uint8_t[]) {0x02}, 1);
    lcd_tx_param(0xB6, (uint8_t[]) {0x02, 0x02}, 2);
    lcd_tx_param(0xE9, (uint8_t[]) {0x00}, 1);
    lcd_tx_param(0xF7, (uint8_t[]) {0xA9, 0x51, 0x2C, 0x82}, 4);
    lcd_tx_param(ILI9488_CMD_SLPOUT, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_tx_param(AUX_PANEL_LCD_USE_INVERSION ? ILI9488_CMD_INVON : ILI9488_CMD_INVOFF, NULL, 0);
    lcd_tx_param(ILI9488_CMD_DISPON, NULL, 0);

    ESP_ERROR_CHECK(gpio_set_level(AUX_PANEL_PIN_NUM_BK_LIGHT, 1));
    ESP_LOGI(TAG, "ILI9488 panel initialized in 480x320 landscape mode");
}

static void init_lvgl_display(void)
{
    const size_t buffer_pixels = AUX_PANEL_HOR_RES * AUX_PANEL_BUF_LINES;

    s_buf_1 = heap_caps_malloc(buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_buf_2 = heap_caps_malloc(buffer_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    s_flush_rgb_buf_size = buffer_pixels * 3U;
    s_flush_rgb_buf = heap_caps_malloc(s_flush_rgb_buf_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    assert(s_buf_1 && s_buf_2 && s_flush_rgb_buf);

    lv_disp_draw_buf_init(&s_draw_buf, s_buf_1, s_buf_2, buffer_pixels);
    lv_disp_drv_init(&s_disp_drv);
    s_disp_drv.hor_res = AUX_PANEL_HOR_RES;
    s_disp_drv.ver_res = AUX_PANEL_VER_RES;
    s_disp_drv.flush_cb = lcd_flush_cb;
    s_disp_drv.draw_buf = &s_draw_buf;
    lv_disp_drv_register(&s_disp_drv);
}

static void start_lvgl_tick(void)
{
    const esp_timer_create_args_t timer_args = {
        .callback = lvgl_tick_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "lv_tick",
        .skip_unhandled_events = true,
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(s_lvgl_tick_timer, AUX_PANEL_TICK_MS * 1000));
}

static void lvgl_task(void *arg)
{
    (void)arg;

    while (true) {
        lv_timer_handler();
        apply_pending_ui_state();
        vTaskDelay(pdMS_TO_TICKS(AUX_PANEL_TICK_MS));
    }
}

void app_main(void)
{
    aux_panel_room_state_t boot_state = {
        .clock_text = "--:--",
        .room_name = "",
        .occupied = false,
        .remaining_sec = 0,
    };
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    s_ui_state_mutex = xSemaphoreCreateMutex();
    s_pair_cfg_mutex = xSemaphoreCreateMutex();
    ESP_ERROR_CHECK(load_runtime_config());

    ESP_LOGI(TAG, "Starting MyCube3_5 auxiliary panel UI");

    init_board_power();
    lv_init();
    init_lcd_panel();
    init_touch_controller();
    init_lvgl_display();
    init_lvgl_input();
    start_lvgl_tick();
    aux_panel_ui_set_language(s_cfg_ui_lang);
    aux_panel_ui_create_mock();
    aux_panel_ui_set_language_change_callback(ui_language_changed_callback);
    copy_string_field(s_cfg_panel_name, boot_state.room_name, sizeof(boot_state.room_name));
    aux_panel_ui_apply_state(&boot_state);
    aux_panel_ui_set_main_select_callback(ui_main_selected_callback);
    aux_panel_ui_set_unlink_all_callback(ui_unlink_all_callback);
    aux_panel_ui_set_wifi_forget_callback(ui_wifi_forget_callback);
    update_settings_ui_texts();
    publish_pair_ui_snapshot();

    if (s_touch_ic != AUX_TOUCH_NONE) {
        xTaskCreate(touch_poll_task, "touch_poll", 4096, NULL, 5, NULL);
    }
    xTaskCreate(lvgl_task, "lvgl_task", 6144, NULL, 4, NULL);
    xTaskCreate(aux_link_task, "aux_link", 8192, NULL, 4, NULL);
}
