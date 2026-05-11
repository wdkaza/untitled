#pragma once

#include <wayland-server-protocol.h>
#define MODKEY WLR_MODIFIER_ALT

static const char *terminal[] = { "kitty", NULL };
static const char *menu[] = {"wmenu-run", NULL};


static const MonitorRule monrules[] = {
// name,   scale, x, y, rotation
  {"HDMI", 1,0,0, WL_OUTPUT_TRANSFORM_NORMAL},
  {NULL,   1,0,1080, WL_OUTPUT_TRANSFORM_NORMAL},
};

static const Key keys[] = {
  {MODKEY,                    XKB_KEY_n,           spawn,            {.v = terminal} },
  {MODKEY,                    XKB_KEY_p,           spawn,            {.v = menu}},
  {MODKEY,                    XKB_KEY_o,           exitwm,           {}},
  {MODKEY,                    XKB_KEY_w,           cyclefocus,       {}},
  {MODKEY,                    XKB_KEY_r,           togglefullscreen, {}},
  {MODKEY,                    XKB_KEY_q,           killclient,       {}},
  {MODKEY,                    XKB_KEY_Up,          focusdir,         {.i = UP}},
  {MODKEY,                    XKB_KEY_Down,        focusdir,         {.i = DOWN}},
  {MODKEY,                    XKB_KEY_Left,        focusdir,         {.i = LEFT}},
  {MODKEY,                    XKB_KEY_Right,       focusdir,         {.i = RIGHT}},
  //{MODKEY,                    XKB_KEY_m,           changedesktop,    {.i = 1}},
  //{MODKEY,                    XKB_KEY_z,           changedesktop,    {.i = 2}},
  //{MODKEY,                    XKB_KEY_3,           changedesktop,    {.i = 3}},
};

static const Button buttons[] = {
  {MODKEY, BTN_LEFT, moveresize, {.ui = CursorMove}},
  //{MODKEY, BTN_MIDDLE, ... , ...},
  {MODKEY, BTN_RIGHT, moveresize, {.ui = CursorResize}},
};

