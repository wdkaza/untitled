#pragma once
#include <wayland-server-core.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/util/box.h>

struct Server;

struct Monitor{
  struct wl_list link;
  struct Server *server;
  struct wlr_output *wlr_output;
  struct wlr_scene_output *scene_output;
  struct wl_list layers[4];

  struct wlr_box m; // monitor area
  struct wlr_box w; // windows area

  // TODO : per monitor specs
  // Layout layout; 
  //
  // float mfact;
  // uint32_t nmaster;
  // bool asleep;


  struct wl_listener frame;
  struct wl_listener request_state;
  struct wl_listener destroy;
};
