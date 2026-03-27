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

static const Key keys[] = {
  {MODKEY,                    XKB_KEY_n,           spawn,            {.v = terminal} },
  {MODKEY,                    XKB_KEY_p,           spawn,            {.v = menu}},
  {MODKEY,                    XKB_KEY_w,           cyclefocus,       {}},
  {MODKEY,                    XKB_KEY_r,           togglefullscreen, {}},
  {MODKEY,                    XKB_KEY_q,           killclient,       {}},
  {MODKEY,                    XKB_KEY_m,           changedesktop,    {.i = 1}},
  {MODKEY,                    XKB_KEY_z,           changedesktop,    {.i = 2}},
  {MODKEY,                    XKB_KEY_3,           changedesktop,    {.i = 3}},
};
/*
static const Button Buttons[] = {
  {MODKEY, BTN_LEFT, moveresize, {.i = CursorMove}},
  {MODKEY, BTN_RIGHT, moveresize, {.i = CursorResize}},
};
*/

