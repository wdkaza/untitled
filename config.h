#pragma once

#define MODKEY WLR_MODIFIER_ALT

static const char *terminal[] = { "kitty", NULL };

static const Key keys[] = {
  {MODKEY,                    XKB_KEY_p,           spawn,            {.v = terminal} }
};
