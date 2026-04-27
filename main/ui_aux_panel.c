#include "ui_aux_panel.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#if LV_USE_QRCODE
#include "extra/libs/qrcode/lv_qrcode.h"
#endif

LV_FONT_DECLARE(ui_font_FontZZh16);
LV_FONT_DECLARE(ui_font_FontZZh20);
LV_FONT_DECLARE(ui_font_FontZHHH16);

static lv_obj_t *s_clock_label = NULL;
static lv_obj_t *s_clock_chip = NULL;
static lv_obj_t *s_settings_btn = NULL;
static lv_obj_t *s_room_name_label = NULL;
static lv_obj_t *s_status_pill = NULL;
static lv_obj_t *s_status_word_label = NULL;
static lv_obj_t *s_remaining_label = NULL;
static lv_obj_t *s_status_note_label = NULL;
static lv_obj_t *s_date_label = NULL;
static lv_obj_t *s_settings_overlay = NULL;
static lv_obj_t *s_settings_card = NULL;
static lv_obj_t *s_nav_wifi_btn = NULL;
static lv_obj_t *s_nav_device_btn = NULL;
static lv_obj_t *s_nav_link_btn = NULL;
static lv_obj_t *s_wifi_forget_btn = NULL;
static lv_obj_t *s_link_unlink_btn = NULL;
static lv_obj_t *s_settings_title_label = NULL;
static lv_obj_t *s_wifi_title_label = NULL;
static lv_obj_t *s_device_title_label = NULL;
static lv_obj_t *s_link_title_label = NULL;
static lv_obj_t *s_status_section_label = NULL;
static lv_obj_t *s_lang_btn = NULL;
static lv_obj_t *s_lang_btn_label = NULL;
static lv_obj_t *s_wifi_panel = NULL;
static lv_obj_t *s_device_panel = NULL;
static lv_obj_t *s_link_panel = NULL;
static lv_obj_t *s_link_list = NULL;
static lv_obj_t *s_link_status_label = NULL;
static lv_obj_t *s_wifi_status_label = NULL;
static lv_obj_t *s_device_setup_label = NULL;
static lv_obj_t *s_device_qr = NULL;
static aux_panel_ui_main_select_cb_t s_main_select_cb = NULL;
static aux_panel_ui_action_cb_t s_unlink_all_cb = NULL;
static aux_panel_ui_action_cb_t s_wifi_forget_cb = NULL;
static aux_panel_ui_main_entry_t s_discovered_entries[AUX_PANEL_UI_MAX_DISCOVERED] = {0};
static size_t s_discovered_entry_count = 0;
static char s_selected_main_id[16] = {0};
static int s_settings_tab = 0;
static aux_panel_ui_language_t s_ui_lang = AUX_PANEL_UI_LANG_EN;
static aux_panel_ui_language_change_cb_t s_lang_change_cb = NULL;
static aux_panel_room_state_t s_last_state = {0};
static bool s_has_last_state = false;

#define AUX_COLOR_BG_TOP        0x000000
#define AUX_COLOR_BG_BOTTOM     0x000000
#define AUX_COLOR_PANEL         0x2A2522
#define AUX_COLOR_PANEL_ALT     0x231E1B
#define AUX_COLOR_BORDER        0x433A35
#define AUX_COLOR_TEXT          0xF5EFE8
#define AUX_COLOR_TEXT_MUTED    0xC2B5AA
#define AUX_COLOR_TEXT_DIM      0x968A7F
#define AUX_COLOR_ACCENT        0x6E955F
#define AUX_COLOR_ACCENT_LIGHT  0x93B783
#define AUX_COLOR_DANGER        0xD32F2F
#define AUX_COLOR_DANGER_LIGHT  0xFF8A80
#define AUX_COLOR_SHADOW        0x000000

#define AUX_HEADER_X            28
#define AUX_HEADER_Y            18
#define AUX_HEADER_W            424
#define AUX_HEADER_H            56

#define AUX_STATUS_X            28
#define AUX_STATUS_Y            186
#define AUX_STATUS_W            424
#define AUX_STATUS_H            126

#define AUX_TIME_X              28
#define AUX_TIME_Y              82
#define AUX_TIME_W              424
#define AUX_TIME_H              92

#define AUX_SETTINGS_OVERLAY_W  480
#define AUX_SETTINGS_OVERLAY_H  320
#define AUX_SETTINGS_CARD_W     480
#define AUX_SETTINGS_CARD_H     320

static const aux_panel_room_state_t k_default_state = {
    .clock_text = "--:--",
    .room_name = "Meeting Room",
    .link_online = false,
    .occupied = false,
    .remaining_sec = 0,
};

static const char *tr_settings(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "设置" : "Settings"; }
static const char *tr_wifi(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "无线网络" : "Wi-Fi"; }
static const char *tr_device_setup(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "设备设置" : "Device setup"; }
static const char *tr_link(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "绑定" : "Link"; }
static const char *tr_forget_wifi(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "忘记 Wi-Fi" : "Forget Wi-Fi"; }
static const char *tr_unlink_all(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "全部解绑" : "Unlink all"; }
static const char *tr_room_status(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "会议室状态" : "ROOM STATUS"; }
static const char *tr_free(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "空闲" : "FREE"; }
static const char *tr_busy(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "忙碌" : "BUSY"; }
static const char *tr_busy_now(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "当前忙碌" : "Busy now"; }
static const char *tr_no_active_meeting(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "当前没有会议" : "No active meeting"; }
static const char *tr_meeting_in_progress(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "会议进行中" : "Meeting in progress"; }
static const char *tr_link_lost(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "主面板离线" : "Main panel offline"; }
static const char *tr_no_main_panels(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "暂未发现主面板" : "No main panels found yet"; }
static const char *tr_unknown_room(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "未知会议室" : "Unknown room"; }
static const char *tr_unknown(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "未知" : "unknown"; }
static const char *tr_pair_open(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "可配对" : "pair open"; }
static const char *tr_locked(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "已锁定" : "locked"; }
static const char *tr_select_main_panel(void)
{
    return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "从列表选择主面板" : "Select a main panel from list";
}
static const char *tr_scan_qr(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "扫描二维码" : "Scan QR code"; }
static const char *tr_not_connected(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "未连接" : "Not connected"; }
static const char *tr_open_device_setup_hint(void)
{
    return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "打开设备设置以修改 Wi-Fi" : "Open device setup portal to change Wi-Fi credentials.";
}
static const char *tr_min_left_fmt(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "剩余 %d 分钟" : "%d min left"; }
static const char *tr_hour_min_left_fmt(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? "剩余 %d小时%02d分钟" : "%dh %02dm left"; }

static const lv_font_t *font12(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? &ui_font_FontZZh16 : &lv_font_montserrat_12; }
static const lv_font_t *font14(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? &ui_font_FontZZh16 : &lv_font_montserrat_14; }
static const lv_font_t *font16(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? &ui_font_FontZZh16 : &lv_font_montserrat_16; }
static const lv_font_t *font16_extra_zh(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? &ui_font_FontZHHH16 : &lv_font_montserrat_16; }
static const lv_font_t *font18(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? &ui_font_FontZZh20 : &lv_font_montserrat_18; }
static const lv_font_t *font20(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? &ui_font_FontZZh20 : &lv_font_montserrat_20; }
static const lv_font_t *font24(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? &ui_font_FontZZh20 : &lv_font_montserrat_24; }
static const lv_font_t *font28(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? &ui_font_FontZZh20 : &lv_font_montserrat_28; }
static const lv_font_t *font36(void) { return s_ui_lang == AUX_PANEL_UI_LANG_ZH ? &ui_font_FontZZh20 : &lv_font_montserrat_36; }
static const lv_font_t *font48(void) { return &lv_font_montserrat_48; }

static lv_color_t color_hex(uint32_t value)
{
    return lv_color_hex(value);
}

static lv_obj_t *create_plain_object(lv_obj_t *parent)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *create_label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color_hex(color), 0);
    lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
    return label;
}

static lv_obj_t *get_button_label(lv_obj_t *btn)
{
    if (!btn) {
        return NULL;
    }
    return lv_obj_get_child(btn, 0);
}

static void set_button_text(lv_obj_t *btn, const char *text)
{
    lv_obj_t *label = get_button_label(btn);
    if (label) {
        lv_label_set_text(label, text ? text : "");
    }
}

static void set_button_font(lv_obj_t *btn, const lv_font_t *font)
{
    lv_obj_t *label = get_button_label(btn);
    if (label && font) {
        lv_obj_set_style_text_font(label, font, 0);
    }
}

static void style_surface_card(lv_obj_t *obj, uint32_t bg, uint32_t border, lv_coord_t radius)
{
    lv_obj_remove_style_all(obj);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_radius(obj, radius, 0);
    lv_obj_set_style_bg_color(obj, color_hex(bg), 0);
    lv_obj_set_style_bg_grad_color(obj, color_hex(bg), 0);
    lv_obj_set_style_bg_grad_dir(obj, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 1, 0);
    lv_obj_set_style_border_color(obj, color_hex(border), 0);
    lv_obj_set_style_border_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_outline_width(obj, 0, 0);
    lv_obj_set_style_shadow_width(obj, 14, 0);
    lv_obj_set_style_shadow_color(obj, color_hex(AUX_COLOR_SHADOW), 0);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_30, 0);
    lv_obj_set_style_shadow_ofs_y(obj, 4, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
}

static void style_status_pill(bool link_online, bool occupied)
{
    uint32_t bg = AUX_COLOR_ACCENT;
    uint32_t border = AUX_COLOR_ACCENT_LIGHT;

    if (!link_online) {
        bg = AUX_COLOR_PANEL_ALT;
        border = AUX_COLOR_BORDER;
    } else if (occupied) {
        bg = AUX_COLOR_DANGER;
        border = AUX_COLOR_DANGER_LIGHT;
    }

    if (!s_status_pill) {
        return;
    }

    style_surface_card(s_status_pill, bg, border, 18);
    lv_obj_set_style_shadow_width(s_status_pill, 0, 0);
}

static void format_remaining_text(bool link_online, int remaining_sec, bool occupied, char *out, size_t out_size)
{
    int total_minutes = 0;
    int hours = 0;
    int minutes = 0;

    if (!out || out_size == 0) {
        return;
    }

    if (!link_online) {
        snprintf(out, out_size, "%s", tr_link_lost());
        out[out_size - 1] = '\0';
        return;
    }

    if (!occupied) {
        snprintf(out, out_size, "%s", tr_no_active_meeting());
        out[out_size - 1] = '\0';
        return;
    }

    if (remaining_sec <= 0) {
        snprintf(out, out_size, "%s", tr_busy_now());
        out[out_size - 1] = '\0';
        return;
    }

    total_minutes = (remaining_sec + 59) / 60;
    hours = total_minutes / 60;
    minutes = total_minutes % 60;

    if (hours > 0) {
        snprintf(out, out_size, tr_hour_min_left_fmt(), hours, minutes);
    } else {
        snprintf(out, out_size, tr_min_left_fmt(), total_minutes);
    }
    out[out_size - 1] = '\0';
}

static bool is_selected_main(const char *main_id)
{
    return (main_id && main_id[0] && s_selected_main_id[0] && strcmp(main_id, s_selected_main_id) == 0);
}

static void settings_overlay_set_visible(bool visible)
{
    if (!s_settings_overlay) {
        return;
    }
    if (visible) {
        lv_obj_move_foreground(s_settings_overlay);
        lv_obj_clear_flag(s_settings_overlay, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_settings_overlay, LV_OBJ_FLAG_HIDDEN);
    }
}

static void link_item_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    uint32_t index = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    if (index >= s_discovered_entry_count) {
        return;
    }

    if (s_main_select_cb) {
        s_main_select_cb(s_discovered_entries[index].main_id, s_discovered_entries[index].room_name);
    }
    snprintf(s_selected_main_id, sizeof(s_selected_main_id), "%s", s_discovered_entries[index].main_id);
    s_selected_main_id[sizeof(s_selected_main_id) - 1] = '\0';
}

static void settings_close_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    settings_overlay_set_visible(false);
}

static void settings_open_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        settings_overlay_set_visible(true);
    }
}

static void refresh_static_texts(void);

static void settings_nav_select(int tab)
{
    s_settings_tab = tab;
    if (s_wifi_panel) {
        if (tab == 0) {
            lv_obj_clear_flag(s_wifi_panel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_wifi_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_device_panel) {
        if (tab == 1) {
            lv_obj_clear_flag(s_device_panel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_device_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_link_panel) {
        if (tab == 2) {
            lv_obj_clear_flag(s_link_panel, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_link_panel, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_nav_wifi_btn) {
        lv_obj_set_style_bg_color(s_nav_wifi_btn, color_hex(tab == 0 ? AUX_COLOR_ACCENT : AUX_COLOR_PANEL), 0);
    }
    if (s_nav_device_btn) {
        lv_obj_set_style_bg_color(s_nav_device_btn, color_hex(tab == 1 ? AUX_COLOR_ACCENT : AUX_COLOR_PANEL), 0);
    }
    if (s_nav_link_btn) {
        lv_obj_set_style_bg_color(s_nav_link_btn, color_hex(tab == 2 ? AUX_COLOR_ACCENT : AUX_COLOR_PANEL), 0);
    }
}

static void settings_wifi_nav_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        settings_nav_select(0);
    }
}

static void settings_device_nav_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        settings_nav_select(1);
    }
}

static void settings_link_nav_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) == LV_EVENT_CLICKED) {
        settings_nav_select(2);
    }
}

static void settings_lang_toggle_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    s_ui_lang = (s_ui_lang == AUX_PANEL_UI_LANG_EN) ? AUX_PANEL_UI_LANG_ZH : AUX_PANEL_UI_LANG_EN;
    refresh_static_texts();
    if (s_lang_change_cb) {
        s_lang_change_cb(s_ui_lang);
    }
}

static void settings_wifi_forget_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_wifi_forget_cb) {
        s_wifi_forget_cb();
    }
}

static void settings_unlink_all_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
    if (s_unlink_all_cb) {
        s_unlink_all_cb();
    }
}

static void rebuild_link_list(void)
{
    if (!s_link_list) {
        return;
    }

    lv_obj_clean(s_link_list);

    if (s_discovered_entry_count == 0) {
        lv_obj_t *empty = create_label(s_link_list, tr_no_main_panels(), font14(), AUX_COLOR_TEXT_MUTED);
        lv_obj_set_pos(empty, 10, 10);
        return;
    }

    for (size_t i = 0; i < s_discovered_entry_count; ++i) {
        char line2[96] = {0};
        const bool selected = is_selected_main(s_discovered_entries[i].main_id);
        const char *room = s_discovered_entries[i].room_name[0] ? s_discovered_entries[i].room_name : tr_unknown_room();
        const char *main_id = s_discovered_entries[i].main_id[0] ? s_discovered_entries[i].main_id : tr_unknown();
        const char *mode = s_discovered_entries[i].pair_mode ? tr_pair_open() : tr_locked();

        lv_obj_t *btn = create_plain_object(s_link_list);
        style_surface_card(btn,
                           selected ? AUX_COLOR_ACCENT : AUX_COLOR_PANEL,
                           selected ? AUX_COLOR_ACCENT_LIGHT : AUX_COLOR_BORDER,
                           14);
        lv_obj_set_size(btn, 280, 54);
        lv_obj_set_pos(btn, 4, (lv_coord_t)(i * 60));
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
        lv_obj_add_event_cb(btn, link_item_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);

        lv_obj_t *title = create_label(btn, room, font16(), AUX_COLOR_TEXT);
        lv_obj_set_width(title, 258);
        lv_obj_set_pos(title, 12, 6);
        lv_label_set_long_mode(title, LV_LABEL_LONG_SCROLL_CIRCULAR);

        snprintf(line2, sizeof(line2), "%s | %s", main_id, mode);
        line2[sizeof(line2) - 1] = '\0';
        lv_obj_t *subtitle = create_label(btn, line2, font12(), AUX_COLOR_TEXT_MUTED);
        lv_obj_set_width(subtitle, 258);
        lv_obj_set_pos(subtitle, 12, 30);
        lv_label_set_long_mode(subtitle, LV_LABEL_LONG_SCROLL_CIRCULAR);
    }
}

static lv_obj_t *create_nav_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = create_plain_object(parent);
    style_surface_card(btn, AUX_COLOR_PANEL, AUX_COLOR_BORDER, 12);
    lv_obj_set_style_shadow_width(btn, 0, 0);
    lv_obj_set_size(btn, 120, 40);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = create_label(btn, text, font16(), AUX_COLOR_TEXT);
    lv_obj_center(lbl);
    return btn;
}

static void create_settings_overlay(lv_obj_t *screen)
{
    s_settings_overlay = create_plain_object(screen);
    lv_obj_set_size(s_settings_overlay, AUX_SETTINGS_OVERLAY_W, AUX_SETTINGS_OVERLAY_H);
    lv_obj_set_pos(s_settings_overlay, 0, 0);
    lv_obj_set_style_bg_color(s_settings_overlay, color_hex(AUX_COLOR_PANEL_ALT), 0);
    lv_obj_set_style_bg_opa(s_settings_overlay, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_settings_overlay, LV_OBJ_FLAG_HIDDEN);

    s_settings_card = create_plain_object(s_settings_overlay);
    style_surface_card(s_settings_card, AUX_COLOR_PANEL_ALT, AUX_COLOR_BORDER, 20);
    lv_obj_set_size(s_settings_card, AUX_SETTINGS_CARD_W, AUX_SETTINGS_CARD_H);
    lv_obj_align(s_settings_card, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(s_settings_card, 0, 0);
    lv_obj_set_style_shadow_width(s_settings_card, 0, 0);
    lv_obj_set_style_border_width(s_settings_card, 0, 0);

    s_settings_title_label = create_label(s_settings_card, tr_settings(), font20(), AUX_COLOR_TEXT);
    lv_obj_set_pos(s_settings_title_label, 16, 14);

    lv_obj_t *close_btn = create_plain_object(s_settings_card);
    style_surface_card(close_btn, AUX_COLOR_PANEL, AUX_COLOR_BORDER, 12);
    lv_obj_set_size(close_btn, 42, 30);
    lv_obj_set_pos(close_btn, AUX_SETTINGS_CARD_W - 58, 10);
    lv_obj_add_flag(close_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(close_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(close_btn, settings_close_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close_label = create_label(close_btn, "X", font18(), AUX_COLOR_TEXT);
    lv_obj_center(close_label);

    s_lang_btn = create_plain_object(s_settings_card);
    style_surface_card(s_lang_btn, AUX_COLOR_PANEL, AUX_COLOR_BORDER, 12);
    lv_obj_set_size(s_lang_btn, 68, 30);
    lv_obj_set_pos(s_lang_btn, AUX_SETTINGS_CARD_W - 132, 10);
    lv_obj_add_flag(s_lang_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_lang_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(s_lang_btn, settings_lang_toggle_event_cb, LV_EVENT_CLICKED, NULL);
    s_lang_btn_label = create_label(s_lang_btn, (s_ui_lang == AUX_PANEL_UI_LANG_ZH) ? "中文" : "EN", font16(), AUX_COLOR_TEXT);
    lv_obj_center(s_lang_btn_label);

    lv_obj_t *nav = create_plain_object(s_settings_card);
    lv_obj_set_size(nav, 130, 260);
    lv_obj_set_pos(nav, 12, 48);
    s_nav_wifi_btn = create_nav_button(nav, tr_wifi(), settings_wifi_nav_event_cb);
    lv_obj_set_pos(s_nav_wifi_btn, 4, 4);
    s_nav_device_btn = create_nav_button(nav, tr_device_setup(), settings_device_nav_event_cb);
    lv_obj_set_pos(s_nav_device_btn, 4, 52);
    s_nav_link_btn = create_nav_button(nav, tr_link(), settings_link_nav_event_cb);
    lv_obj_set_pos(s_nav_link_btn, 4, 100);

    s_wifi_panel = create_plain_object(s_settings_card);
    style_surface_card(s_wifi_panel, AUX_COLOR_PANEL, AUX_COLOR_BORDER, 16);
    lv_obj_set_size(s_wifi_panel, 322, 260);
    lv_obj_set_pos(s_wifi_panel, 146, 48);
    s_wifi_title_label = create_label(s_wifi_panel, tr_wifi(), font20(), AUX_COLOR_TEXT);
    lv_obj_set_pos(s_wifi_title_label, 14, 14);
    s_wifi_status_label = create_label(s_wifi_panel,
                                       tr_not_connected(),
                                       font36(),
                                       AUX_COLOR_TEXT);
    lv_label_set_long_mode(s_wifi_status_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_wifi_status_label, 294);
    lv_obj_set_height(s_wifi_status_label, 110);
    lv_obj_set_pos(s_wifi_status_label, 14, 60);
    lv_obj_set_style_text_align(s_wifi_status_label, LV_TEXT_ALIGN_CENTER, 0);

    s_wifi_forget_btn = create_nav_button(s_wifi_panel, tr_forget_wifi(), settings_wifi_forget_event_cb);
    lv_obj_set_size(s_wifi_forget_btn, 150, 40);
    lv_obj_set_pos(s_wifi_forget_btn, 14, 206);
    set_button_font(s_wifi_forget_btn, font16_extra_zh());

    s_device_panel = create_plain_object(s_settings_card);
    style_surface_card(s_device_panel, AUX_COLOR_PANEL, AUX_COLOR_BORDER, 16);
    lv_obj_set_size(s_device_panel, 322, 260);
    lv_obj_set_pos(s_device_panel, 146, 48);
    s_device_title_label = create_label(s_device_panel, tr_device_setup(), font20(), AUX_COLOR_TEXT);
    lv_obj_set_pos(s_device_title_label, 14, 8);
    s_device_setup_label = create_label(s_device_panel,
                                        tr_scan_qr(),
                                        font14(),
                                        AUX_COLOR_TEXT_MUTED);
    lv_label_set_long_mode(s_device_setup_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_device_setup_label, 294);
    lv_obj_set_height(s_device_setup_label, 56);
    lv_obj_set_pos(s_device_setup_label, 14, 192);

    lv_obj_t *qr_frame = create_plain_object(s_device_panel);
    lv_obj_set_size(qr_frame, 120, 120);
    lv_obj_set_pos(qr_frame, 101, 58);
    lv_obj_set_style_bg_color(qr_frame, color_hex(0xFFFFFF), 0);
    lv_obj_set_style_bg_opa(qr_frame, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(qr_frame, 2, 0);
    lv_obj_set_style_border_color(qr_frame, color_hex(AUX_COLOR_BORDER), 0);
    lv_obj_set_style_radius(qr_frame, 8, 0);
    lv_obj_clear_flag(qr_frame, LV_OBJ_FLAG_SCROLLABLE);

#if LV_USE_QRCODE
    s_device_qr = lv_qrcode_create(qr_frame, 112, lv_color_hex(0x111111), lv_color_hex(0xFFFFFF));
    lv_qrcode_update(s_device_qr, "http://192.168.4.1", strlen("http://192.168.4.1"));
    lv_obj_center(s_device_qr);
#else
    s_device_qr = create_label(qr_frame, "QR OFF", font16(), AUX_COLOR_PANEL_ALT);
    lv_obj_center(s_device_qr);
#endif

    s_link_panel = create_plain_object(s_settings_card);
    style_surface_card(s_link_panel, AUX_COLOR_PANEL, AUX_COLOR_BORDER, 16);
    lv_obj_set_size(s_link_panel, 322, 260);
    lv_obj_set_pos(s_link_panel, 146, 48);
    s_link_title_label = create_label(s_link_panel, tr_link(), font20(), AUX_COLOR_TEXT);
    lv_obj_set_pos(s_link_title_label, 14, 10);
    s_link_list = create_plain_object(s_link_panel);
    lv_obj_set_size(s_link_list, 290, 142);
    lv_obj_set_pos(s_link_list, 14, 40);
    lv_obj_add_flag(s_link_list, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(s_link_list, LV_DIR_VER);
    lv_obj_set_style_bg_color(s_link_list, color_hex(AUX_COLOR_PANEL_ALT), 0);
    lv_obj_set_style_bg_opa(s_link_list, LV_OPA_50, 0);
    lv_obj_set_style_radius(s_link_list, 12, 0);
    lv_obj_set_style_pad_all(s_link_list, 4, 0);
    lv_obj_set_style_border_width(s_link_list, 1, 0);
    lv_obj_set_style_border_color(s_link_list, color_hex(AUX_COLOR_BORDER), 0);
    s_link_status_label = create_label(s_link_panel,
                                       tr_select_main_panel(),
                                       font12(),
                                       AUX_COLOR_TEXT_MUTED);
    lv_obj_set_width(s_link_status_label, 290);
    lv_obj_set_pos(s_link_status_label, 14, 186);
    lv_label_set_long_mode(s_link_status_label, LV_LABEL_LONG_WRAP);

    s_link_unlink_btn = create_nav_button(s_link_panel, tr_unlink_all(), settings_unlink_all_event_cb);
    lv_obj_set_size(s_link_unlink_btn, 150, 40);
    lv_obj_set_pos(s_link_unlink_btn, 14, 214);
    set_button_font(s_link_unlink_btn, font16_extra_zh());

    rebuild_link_list();
    settings_nav_select(0);
}

static void create_header(lv_obj_t *screen, const aux_panel_room_state_t *state)
{
    lv_obj_t *header = create_plain_object(screen);
    lv_obj_set_size(header, AUX_HEADER_W, AUX_HEADER_H);
    lv_obj_set_pos(header, AUX_HEADER_X, AUX_HEADER_Y);
    lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(header, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *room_wrap = create_plain_object(header);
    lv_obj_set_size(room_wrap, 292, AUX_HEADER_H);

    s_room_name_label = create_label(room_wrap, state->room_name, font28(), AUX_COLOR_TEXT);
    lv_label_set_long_mode(s_room_name_label, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(s_room_name_label, 292);
    lv_obj_set_pos(s_room_name_label, 0, 8);

    lv_obj_t *right_wrap = create_plain_object(header);
    lv_obj_set_size(right_wrap, 64, AUX_HEADER_H);

    s_settings_btn = create_plain_object(right_wrap);
    style_surface_card(s_settings_btn, AUX_COLOR_PANEL_ALT, AUX_COLOR_BORDER, 14);
    lv_obj_set_style_shadow_width(s_settings_btn, 0, 0);
    lv_obj_set_size(s_settings_btn, 40, 40);
    lv_obj_align(s_settings_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_flag(s_settings_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_settings_btn, LV_OBJ_FLAG_CLICK_FOCUSABLE);
    lv_obj_add_event_cb(s_settings_btn, settings_open_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *gear_label = create_label(s_settings_btn, LV_SYMBOL_SETTINGS, &lv_font_montserrat_20, AUX_COLOR_TEXT);
    lv_obj_center(gear_label);
}

static void create_time_card(lv_obj_t *screen, const aux_panel_room_state_t *state)
{
    s_clock_chip = create_plain_object(screen);
    style_surface_card(s_clock_chip, AUX_COLOR_PANEL_ALT, AUX_COLOR_BORDER, 18);
    lv_obj_set_size(s_clock_chip, AUX_TIME_W, AUX_TIME_H);
    lv_obj_set_pos(s_clock_chip, AUX_TIME_X, AUX_TIME_Y);
    lv_obj_set_style_shadow_width(s_clock_chip, 0, 0);

    s_clock_label = create_label(s_clock_chip, state->clock_text, font48(), AUX_COLOR_TEXT);
    lv_obj_set_pos(s_clock_label, 16, 8);

    s_date_label = create_label(s_clock_chip, "", font16(), AUX_COLOR_TEXT_MUTED);
    lv_obj_set_width(s_date_label, AUX_TIME_W - 32);
    lv_obj_set_pos(s_date_label, 16, 62);
}

static void create_status_card(lv_obj_t *screen, const aux_panel_room_state_t *state)
{
    char remaining_text[48] = {0};

    lv_obj_t *card = create_plain_object(screen);
    style_surface_card(card, AUX_COLOR_PANEL, AUX_COLOR_BORDER, 24);
    lv_obj_set_size(card, AUX_STATUS_W, AUX_STATUS_H);
    lv_obj_set_pos(card, AUX_STATUS_X, AUX_STATUS_Y);

    s_status_section_label = create_label(card, tr_room_status(), font14(), AUX_COLOR_TEXT_DIM);
    lv_obj_set_pos(s_status_section_label, 20, 10);

    s_status_pill = create_plain_object(card);
    style_status_pill(state->link_online, state->occupied);
    lv_obj_set_size(s_status_pill, 128, 42);
    lv_obj_set_pos(s_status_pill, 20, 40);

    s_status_word_label = create_label(s_status_pill,
                                       state->link_online ? (state->occupied ? tr_busy() : tr_free()) : "",
                                       font24(),
                                       AUX_COLOR_TEXT);
    lv_obj_center(s_status_word_label);

    format_remaining_text(state->link_online, state->remaining_sec, state->occupied, remaining_text, sizeof(remaining_text));
    s_remaining_label = create_label(card, remaining_text, font28(), AUX_COLOR_TEXT);
    lv_label_set_long_mode(s_remaining_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_remaining_label, 250);
    lv_obj_set_height(s_remaining_label, 72);
    lv_obj_set_pos(s_remaining_label, 164, 22);

    s_status_note_label = create_label(card,
                                       state->occupied ? tr_meeting_in_progress() : "",
                                       font14(),
                                       AUX_COLOR_TEXT_MUTED);
    lv_label_set_long_mode(s_status_note_label, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status_note_label, 250);
    lv_obj_set_height(s_status_note_label, 34);
    lv_obj_set_pos(s_status_note_label, 164, 72);
    if (!state->link_online || !state->occupied) {
        lv_obj_add_flag(s_status_note_label, LV_OBJ_FLAG_HIDDEN);
    }
}

void aux_panel_ui_apply_state(const aux_panel_room_state_t *state)
{
    char remaining_text[48] = {0};

    if (!state) {
        return;
    }

    s_last_state = *state;
    s_has_last_state = true;

    if (s_clock_label) {
        lv_label_set_text(s_clock_label, state->clock_text[0] ? state->clock_text : "--:--");
    }
    if (s_date_label) {
        lv_label_set_text(s_date_label, state->date_text[0] ? state->date_text : "--");
    }

    if (s_room_name_label) {
        lv_label_set_text(s_room_name_label, state->room_name[0] ? state->room_name : "Meeting Room");
    }

    if (s_status_pill) {
        style_status_pill(state->link_online, state->occupied);
    }

    if (s_status_word_label) {
        lv_label_set_text(s_status_word_label,
                          state->link_online ? (state->occupied ? tr_busy() : tr_free()) : "");
    }

    format_remaining_text(state->link_online, state->remaining_sec, state->occupied, remaining_text, sizeof(remaining_text));
    if (s_remaining_label) {
        lv_label_set_text(s_remaining_label, remaining_text);
    }

    if (s_status_note_label) {
        if (state->link_online && state->occupied) {
            lv_obj_clear_flag(s_status_note_label, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(s_status_note_label, tr_meeting_in_progress());
            lv_obj_set_style_text_color(s_status_note_label, color_hex(AUX_COLOR_TEXT_MUTED), 0);
        } else {
            lv_obj_add_flag(s_status_note_label, LV_OBJ_FLAG_HIDDEN);
        }
    }

}

void aux_panel_ui_set_main_select_callback(aux_panel_ui_main_select_cb_t cb)
{
    s_main_select_cb = cb;
}

void aux_panel_ui_set_unlink_all_callback(aux_panel_ui_action_cb_t cb)
{
    s_unlink_all_cb = cb;
}

void aux_panel_ui_set_wifi_forget_callback(aux_panel_ui_action_cb_t cb)
{
    s_wifi_forget_cb = cb;
}

void aux_panel_ui_update_discovered_mains(const aux_panel_ui_main_entry_t *entries, size_t count, const char *selected_main_id)
{
    if (count > AUX_PANEL_UI_MAX_DISCOVERED) {
        count = AUX_PANEL_UI_MAX_DISCOVERED;
    }

    s_discovered_entry_count = 0;
    memset(s_discovered_entries, 0, sizeof(s_discovered_entries));
    if (entries && count > 0) {
        for (size_t i = 0; i < count; ++i) {
            s_discovered_entries[s_discovered_entry_count] = entries[i];
            ++s_discovered_entry_count;
        }
    }

    s_selected_main_id[0] = '\0';
    if (selected_main_id && selected_main_id[0]) {
        snprintf(s_selected_main_id, sizeof(s_selected_main_id), "%s", selected_main_id);
        s_selected_main_id[sizeof(s_selected_main_id) - 1] = '\0';
    }

    rebuild_link_list();
}

static void refresh_static_texts(void)
{
    set_button_text(s_nav_wifi_btn, tr_wifi());
    set_button_text(s_nav_device_btn, tr_device_setup());
    set_button_text(s_nav_link_btn, tr_link());
    set_button_text(s_wifi_forget_btn, tr_forget_wifi());
    set_button_text(s_link_unlink_btn, tr_unlink_all());
    set_button_font(s_nav_wifi_btn, font16());
    set_button_font(s_nav_device_btn, font16());
    set_button_font(s_nav_link_btn, font16());
    set_button_font(s_wifi_forget_btn, font16_extra_zh());
    set_button_font(s_link_unlink_btn, font16_extra_zh());

    if (s_settings_title_label) {
        lv_label_set_text(s_settings_title_label, tr_settings());
        lv_obj_set_style_text_font(s_settings_title_label, font20(), 0);
    }
    if (s_wifi_title_label) {
        lv_label_set_text(s_wifi_title_label, tr_wifi());
        lv_obj_set_style_text_font(s_wifi_title_label, font20(), 0);
    }
    if (s_device_title_label) {
        lv_label_set_text(s_device_title_label, tr_device_setup());
        lv_obj_set_style_text_font(s_device_title_label, font20(), 0);
    }
    if (s_link_title_label) {
        lv_label_set_text(s_link_title_label, tr_link());
        lv_obj_set_style_text_font(s_link_title_label, font20(), 0);
    }
    if (s_status_section_label) {
        lv_label_set_text(s_status_section_label, tr_room_status());
        lv_obj_set_style_text_font(s_status_section_label, font14(), 0);
    }
    if (s_lang_btn_label) {
        lv_label_set_text(s_lang_btn_label, (s_ui_lang == AUX_PANEL_UI_LANG_ZH) ? "中文" : "EN");
        lv_obj_set_style_text_font(s_lang_btn_label, font16(), 0);
    }
    if (s_wifi_status_label) {
        lv_obj_set_style_text_font(s_wifi_status_label, font36(), 0);
    }
    if (s_device_setup_label) {
        lv_obj_set_style_text_font(s_device_setup_label, font14(), 0);
    }
    if (s_link_status_label) {
        lv_obj_set_style_text_font(s_link_status_label, font12(), 0);
    }
    if (s_room_name_label) {
        lv_obj_set_style_text_font(s_room_name_label, font28(), 0);
    }
    if (s_clock_label) {
        lv_obj_set_style_text_font(s_clock_label, font48(), 0);
    }
    if (s_date_label) {
        lv_obj_set_style_text_font(s_date_label, font16(), 0);
    }
    if (s_status_word_label) {
        lv_obj_set_style_text_font(s_status_word_label, font24(), 0);
    }
    if (s_remaining_label) {
        lv_obj_set_style_text_font(s_remaining_label, font28(), 0);
    }
    if (s_status_note_label) {
        lv_obj_set_style_text_font(s_status_note_label, font14(), 0);
    }

    rebuild_link_list();
    if (s_has_last_state) {
        aux_panel_ui_apply_state(&s_last_state);
    }
}

void aux_panel_ui_set_language(aux_panel_ui_language_t lang)
{
    s_ui_lang = (lang == AUX_PANEL_UI_LANG_ZH) ? AUX_PANEL_UI_LANG_ZH : AUX_PANEL_UI_LANG_EN;
    refresh_static_texts();
}

aux_panel_ui_language_t aux_panel_ui_get_language(void)
{
    return s_ui_lang;
}

void aux_panel_ui_set_language_change_callback(aux_panel_ui_language_change_cb_t cb)
{
    s_lang_change_cb = cb;
}

void aux_panel_ui_set_pairing_status_text(const char *text)
{
    const char *status_text = (text && text[0]) ? text : tr_select_main_panel();

    if (!s_link_status_label) {
        return;
    }
    lv_label_set_text(s_link_status_label, status_text);
}

void aux_panel_ui_set_wifi_status_text(const char *text)
{
    const char *status_text = (text && text[0]) ? text : tr_open_device_setup_hint();

    if (!s_wifi_status_label) {
        return;
    }
    lv_label_set_text(s_wifi_status_label, status_text);
}

void aux_panel_ui_set_device_setup_text(const char *text)
{
    const char *status_text = (text && text[0]) ? text : tr_scan_qr();

    if (!s_device_setup_label) {
        return;
    }
    lv_label_set_text(s_device_setup_label, status_text);
}

void aux_panel_ui_set_device_setup_qr_url(const char *url)
{
    const char *value = (url && url[0]) ? url : "http://192.168.4.1";

    if (!s_device_qr) {
        return;
    }
#if LV_USE_QRCODE
    lv_qrcode_update(s_device_qr, value, strlen(value));
#else
    lv_label_set_text(s_device_qr, value);
#endif
}

lv_obj_t *aux_panel_ui_create_mock(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(screen);
    lv_obj_clear_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, color_hex(AUX_COLOR_BG_TOP), 0);
    lv_obj_set_style_bg_grad_color(screen, color_hex(AUX_COLOR_BG_BOTTOM), 0);
    lv_obj_set_style_bg_grad_dir(screen, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    create_header(screen, &k_default_state);
    create_time_card(screen, &k_default_state);
    create_status_card(screen, &k_default_state);
    create_settings_overlay(screen);

    lv_scr_load(screen);
    return screen;
}
