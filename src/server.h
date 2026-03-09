#pragma once
#include <wayland-server-core.h>
#include <wlr/backend.h>


struct Server{
  struct wl_display *display;
  struct wlr_backend *wlr_backend;
  struct wlr_renderer *wlr_renderer;
  struct wlr_allocator *wlr_allocator;
  struct mw_renderer *mw_renderer;
  
  struct wl_listener new_output;
};
