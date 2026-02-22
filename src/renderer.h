#pragma once

#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>

struct Monitor;

struct mw_renderer{
  struct Server *server;
  struct wlr_renderer *wlr_renderer;
  struct Monitor *current;
  //renderer mode;
};
void mw_renderer_init(struct mw_renderer *renderer, struct Server *server);
void mw_renderer_destroy(struct mw_renderer *renderer);
