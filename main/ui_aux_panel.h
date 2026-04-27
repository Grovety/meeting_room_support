#ifndef UI_AUX_PANEL_H
#define UI_AUX_PANEL_H

#include <stdbool.h>
#include <stddef.h>

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    char clock_text[16];
    char date_text[48];
    char room_name[48];
    bool link_online;
    bool occupied;
    int remaining_sec;
} aux_panel_room_state_t;

#define AUX_PANEL_UI_MAX_DISCOVERED 8

typedef struct {
    char main_id[16];
    char room_name[48];
    bool pair_mode;
    bool paired;
} aux_panel_ui_main_entry_t;

typedef void (*aux_panel_ui_main_select_cb_t)(const char *main_id, const char *room_name);
typedef void (*aux_panel_ui_action_cb_t)(void);
typedef enum {
    AUX_PANEL_UI_LANG_EN = 0,
    AUX_PANEL_UI_LANG_ZH = 1,
} aux_panel_ui_language_t;
typedef void (*aux_panel_ui_language_change_cb_t)(aux_panel_ui_language_t lang);

lv_obj_t *aux_panel_ui_create_mock(void);
void aux_panel_ui_apply_state(const aux_panel_room_state_t *state);
void aux_panel_ui_set_main_select_callback(aux_panel_ui_main_select_cb_t cb);
void aux_panel_ui_set_unlink_all_callback(aux_panel_ui_action_cb_t cb);
void aux_panel_ui_set_wifi_forget_callback(aux_panel_ui_action_cb_t cb);
void aux_panel_ui_update_discovered_mains(const aux_panel_ui_main_entry_t *entries, size_t count, const char *selected_main_id);
void aux_panel_ui_set_pairing_status_text(const char *text);
void aux_panel_ui_set_wifi_status_text(const char *text);
void aux_panel_ui_set_device_setup_text(const char *text);
void aux_panel_ui_set_device_setup_qr_url(const char *url);
void aux_panel_ui_set_language(aux_panel_ui_language_t lang);
aux_panel_ui_language_t aux_panel_ui_get_language(void);
void aux_panel_ui_set_language_change_callback(aux_panel_ui_language_change_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif
