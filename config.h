#pragma once

#define MODKEY WLR_MODIFIER_ALT

static const char *terminal[] = { "kitty", NULL };

//static const float fullscreen_bg[] =; 
static const float focused_border_color[] = {1.0f, 0.0f, 0.0f, 1.0f};
static const float unfocused_border_color[] = {0.0f, 1.0f, 0.0f, 1.0f};

static const Key keys[] = {
  {MODKEY,                    XKB_KEY_n,           spawn,            {.v = terminal} },
  {MODKEY,                    XKB_KEY_w,           cyclefocus,       {}},
  {MODKEY,                    XKB_KEY_r,           killclient,       {}},
  {MODKEY,                    XKB_KEY_1,           changedesktop,    {.i = 1}},
  {MODKEY,                    XKB_KEY_2,           changedesktop,    {.i = 2}},
  {MODKEY,                    XKB_KEY_3,           changedesktop,    {.i = 3}},
};
/*
static const Button Buttons[] = {
  {MODKEY, BTN_LEFT, moveresize, {.i = CursorMove}},
  {MODKEY, BTN_RIGHT, moveresize, {.i = CursorResize}},
};
*/
