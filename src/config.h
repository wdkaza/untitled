#pragma once

#include <wayland-server-protocol.h>
#define MODKEY WLR_MODIFIER_ALT

static const char *terminal[] = { "kitty", NULL };
static const char *menu[] = {"wmenu-run", NULL};

//static const float fullscreen_bg[] =; 
static const float focused_border_color[] = {1.0f, 0.0f, 0.0f, 1.0f};
static const float unfocused_border_color[] = {0.0f, 1.0f, 0.0f, 1.0f};
static const float fullscreen_bg[] = {0.2f, 0.2f, 0.2f, 1.0f};

static const MonitorRule monrules[] = {
// name, scale, x, y, rotation
  {"NULL", 1,  -1,  -1,     WL_OUTPUT_TRANSFORM_NORMAL},
  //{"HDMI-A-1", 1, -1, -1, WL_OUTPUT_TRANSFORM_NORMAL},
};

/* Trackpad */
static const int tap_to_click = 1;
static const int tap_and_drag = 1;
static const int drag_lock = 1;
static const int natural_scrolling = 0;
static const int disable_while_typing = 1;
static const int left_handed = 0;
static const int middle_button_emulation = 0;
/* You can choose between:
LIBINPUT_CONFIG_SCROLL_NO_SCROLL
LIBINPUT_CONFIG_SCROLL_2FG
LIBINPUT_CONFIG_SCROLL_EDGE
LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN
*/
static const enum libinput_config_scroll_method scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;

/* You can choose between:
LIBINPUT_CONFIG_CLICK_METHOD_NONE
LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS
LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER
*/
static const enum libinput_config_click_method click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;

/* You can choose between:
LIBINPUT_CONFIG_SEND_EVENTS_ENABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED
LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE
*/
static const uint32_t send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;

/* You can choose between:
LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT
LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE
*/
static const enum libinput_config_accel_profile accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
static const double accel_speed = 0.0;

/* You can choose between:
LIBINPUT_CONFIG_TAP_MAP_LRM -- 1/2/3 finger tap maps to left/right/middle
LIBINPUT_CONFIG_TAP_MAP_LMR -- 1/2/3 finger tap maps to left/middle/right
*/
static const enum libinput_config_tap_button_map button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;


static const Key keys[] = {
  {MODKEY,                    XKB_KEY_n,           spawn,            {.v = terminal} },
  {MODKEY,                    XKB_KEY_p,           spawn,            {.v = menu}},
  {MODKEY,                    XKB_KEY_o,           exitwm,           {}},
  {MODKEY,                    XKB_KEY_w,           cyclefocus,       {}},
  {MODKEY,                    XKB_KEY_r,           togglefullscreen, {}},
  {MODKEY,                    XKB_KEY_q,           killclient,       {}},
  //{MODKEY,                    XKB_KEY_m,           changedesktop,    {.i = 1}},
  //{MODKEY,                    XKB_KEY_z,           changedesktop,    {.i = 2}},
  //{MODKEY,                    XKB_KEY_3,           changedesktop,    {.i = 3}},
};
/*
static const Button Buttons[] = {
  {MODKEY, BTN_LEFT, moveresize, {.i = CursorMove}},
  {MODKEY, BTN_RIGHT, moveresize, {.i = CursorResize}},
};
*/

