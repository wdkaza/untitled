#pragma once

#define MODKEY WLR_MODIFIER_ALT

static const char *terminal[] = { "kitty", NULL };

static const Key keys[] = {
  {MODKEY,                    XKB_KEY_n,           spawn,            {.v = terminal} },
  {MODKEY,                    XKB_KEY_w,           cyclefocus,       {}},
  {MODKEY,                    XKB_KEY_r,           killclient,       {}}
};
