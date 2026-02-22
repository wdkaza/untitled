#include "renderer.h"
#include "server.h"

#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>

void mw_renderer_init(struct mw_renderer *renderer, struct Server *server){
  renderer->server = server;
  renderer->wlr_renderer = wlr_renderer_autocreate(server->wlr_backend);
  //wlr_renderer_init_wl_display(renderer, display);
}
