#include <bits/time.h>
#include <getopt.h>
#include <libinput.h>
#include <linux/input-event-codes.h>
#include <math.h>
#include <signal.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <wayland-protocols/xdg-shell-enum.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol.h>
#include <wayland-util.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_alpha_modifier_v1.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_cursor_shape_v1.h>
#include <wlr/types/wlr_data_control_v1.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_drm.h>
#include <wlr/types/wlr_export_dmabuf_v1.h>
#include <wlr/types/wlr_fractional_scale_v1.h>
#include <wlr/types/wlr_gamma_control_v1.h>
#include <wlr/types/wlr_idle_inhibit_v1.h>
#include <wlr/types/wlr_idle_notify_v1.h>
#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_keyboard.h>
#include <wlr/types/wlr_keyboard_group.h>
#include <wlr/types/wlr_layer_shell_v1.h>
#include <wlr/types/wlr_linux_dmabuf_v1.h>
#include <wlr/types/wlr_linux_drm_syncobj_v1.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_output_management_v1.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_pointer.h>
#include <wlr/types/wlr_pointer_constraints_v1.h>
#include <wlr/types/wlr_presentation_time.h>
#include <wlr/types/wlr_primary_selection.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_relative_pointer_v1.h>
#include <wlr/types/wlr_scene.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_server_decoration.h>
#include <wlr/types/wlr_session_lock_v1.h>
#include <wlr/types/wlr_single_pixel_buffer_v1.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_viewporter.h>
#include <wlr/types/wlr_virtual_keyboard_v1.h>
#include <wlr/types/wlr_virtual_pointer_v1.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_xdg_activation_v1.h>
#include <wlr/types/wlr_xdg_decoration_v1.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_xdg_shell.h>
#include <wlr/util/log.h>
#include <wlr/util/region.h>
#include <wlr/version.h>
#include <xkbcommon/xkbcommon.h>
#ifdef XWAYLAND
#include <wlr/xwayland.h>
#include <xcb/xcb.h>
#include <xcb/xcb_icccm.h>
#endif

#define CLEANMASK(mask)         (mask & ~WLR_MODIFIER_CAPS)
#define END(A)                  ((A) + LENGTH(A))
#define LENGTH(X)               (sizeof X / sizeof X[0])
#define LISTEN_STATIC(E, H)     do { struct wl_listener *_l = calloc(1, sizeof(*_l)); _l->notify = (H); wl_signal_add((E), _l); } while (0)

enum CURSOR_MODE {CursorPassthrough, CursorMove, CursorResize};
enum { XDGShell, LayerShell, X11 };
enum { LyrBg, LyrBottom, LyrTile, LyrFloat, LyrTop, LyrFS, LyrOverlay, LyrBlock, NUM_LAYERS };

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

struct Client{
  unsigned int type;
  struct wl_list link;
  struct Server *server;
  struct Monitor *mon;

  struct wlr_scene_tree *scene_tree; //position/container
  struct wlr_scene_tree *scene_surface; // client pixels
  struct wlr_scene_rect *border[4];
  uint32_t bw;

  union{
    struct wlr_xdg_surface *xdg;
    struct wlr_xwayland_surface *xwayland;
  } surface;


  //int isfocused;
  uint32_t desktop_index;
  int isfloating;  
  int isfullscreen;
  int isurgent;

  struct wlr_xdg_toplevel_decoration_v1 *decoration;
  struct wl_listener decoration_set_mode;
  struct wl_listener decoration_destroy;
  
  struct wl_listener fullscreen;
  struct wl_listener maximize;
  struct wl_listener map;
  struct wl_listener unmap;
  struct wl_listener commit;
  struct wl_listener destroy;

#ifdef XWAYLAND
	struct wl_listener activate;
	struct wl_listener associate;
	struct wl_listener dissociate;
	struct wl_listener configure;
	struct wl_listener set_hints;
#endif
};
/* struct Popup{}; */

typedef union{
  int i;
  float f;
  const void *v;
}Arg;

typedef struct{
	uint32_t mod;
	xkb_keysym_t keysym;
	void (*func)(const Arg *);
	const Arg arg;
}Key;

typedef struct {
	uint32_t mod;
	uint32_t button;
	void (*func)(const Arg *);
	const Arg arg;
} Button;


struct Keyboard{
  struct wl_list link;
  struct wlr_keyboard *wlr_keyboard;

  struct wl_listener key;
  struct wl_listener modifier;
  struct wl_listener destroy;
};

struct LayerSurface{
  unsigned int type;
  struct Monitor *mon;
  struct wl_list link;

  struct wlr_layer_surface_v1 *layer_surface;

  int mapped;

  struct wlr_scene_tree *scene_tree;
  struct wlr_scene_tree *popups;
  struct wlr_scene_layer_surface_v1 *scene_layer_surface;

  struct wl_listener surface_commit;
  struct wl_listener unmap;
  struct wl_listener destroy;
};

static void cursormotion(struct wl_listener *listener, void *data);
static void seatsetselection(struct wl_listener *listener, void *data);
static void seatrequestcursor(struct wl_listener *listener, void *data);
static void cursormotionabsolute(struct wl_listener *listener, void *data);
static void cursoraxis(struct wl_listener *listener, void *data);
static void cursorframe(struct wl_listener *listener, void *data);
static void cursorbutton(struct wl_listener *listener, void *data);
static void rendermon(struct wl_listener *listener, void *data);
static void requeststatemon(struct wl_listener *listener, void *data);
static void destroymon(struct wl_listener *listener, void *data);
static void createmon(struct wl_listener *listener, void *data);
static void createinput(struct wl_listener *listener, void *data);
static void newclient(struct wl_listener *listener, void *data);
static void newdecoration(struct wl_listener *listener, void *data);
static void decoration_set_mode(struct wl_listener *listener, void *data);
static void decoration_destroy(struct wl_listener *listener, void *data);
static void seatsetprimaryselection(struct wl_listener *listener, void *data);
static void createkeyboard(struct wlr_input_device *device);
static void createpointer(struct wlr_input_device *device);
static void processcursormotion(uint32_t time);
static void newlayersurface(struct wl_listener *listener, void *data);
static void startdrag(struct wl_listener *listener, void *data);
static void requeststartdrag(struct wl_listener *listener, void *data);
static void destroydragicon(struct wl_listener *listener, void *data);
static void arrangelayers(struct Monitor *mon);
static void commitlayersurfacenotify(struct wl_listener *listener, void *data);
static void unmaplayersurfacenotify(struct wl_listener *listener, void *data);
static void destroylayersurfacenotify(struct wl_listener *listener, void *data);
static void changeoutputlayout(struct wl_listener *listener, void *data);
static void setfocus(struct Client *client);
static void spawn(const Arg *arg);
static void killclient(const Arg *arg);
static void cyclefocus(const Arg *arg);
static void changedesktop(const Arg *arg);
static void cursormove();
static void cursorresize();
static void init();
static void run();
static void quit();
static void destroylisteners();
#ifdef XWAYLAND

// most of xwayland come will come from straight from dwl

static void activatex11(struct wl_listener *listener, void *data);
static void associatex11(struct wl_listener *listener, void *data);
static void configurex11(struct wl_listener *listener, void *data);
static void newclientx11(struct wl_listener *listener, void *data);
static void dissociatex11(struct wl_listener *listener, void *data);
static void sethints(struct wl_listener *listener, void *data);
static void xwaylandready(struct wl_listener *listener, void *data);
static struct wl_listener new_xwayland_surface = {.notify = newclientx11};
static struct wl_listener xwayland_ready = {.notify = xwaylandready};
static struct wlr_xwayland *xwayland;
#endif

static struct wl_listener new_layer_surface = {.notify = newlayersurface};
static struct wl_listener cursor_motion = {.notify = cursormotion};
static struct wl_listener cursor_motion_absolute = {.notify = cursormotionabsolute};
static struct wl_listener cursor_axis = {.notify = cursoraxis};
static struct wl_listener cursor_button = {.notify = cursorbutton};
static struct wl_listener cursor_frame = {.notify = cursorframe};
static struct wl_listener request_set_selection = {.notify = seatsetselection};
static struct wl_listener request_set_primary_selection = {.notify = seatsetprimaryselection};
static struct wl_listener request_cursor = {.notify = seatrequestcursor};
static struct wl_listener output_layout_change = {.notify = changeoutputlayout};
static struct wl_listener new_decoration = {.notify = newdecoration};
static struct wl_listener new_input = {.notify = createinput};
static struct wl_listener new_client = {.notify = newclient};
static struct wl_listener start_drag = {.notify = startdrag};
static struct wl_listener request_start_drag = {.notify = requeststartdrag};


static struct Server *server;
static struct wlr_compositor *compositor;
static struct wlr_scene *scene;
static struct wlr_output_layout *output_layout;
static struct wlr_xdg_output_manager_v1 *xdg_output_manager;
static struct wlr_viewporter *viewporter;
static struct wlr_fractional_scale_manager_v1 *fractional_scale;
static struct wlr_xdg_activation_v1 *activation;

static struct wlr_cursor *cursor;
static uint32_t cursor_mode;
static struct wlr_xcursor_manager *cursor_manager;
static struct wlr_scene_tree *drag_icon;

static struct wlr_seat *seat;

static struct wlr_xdg_shell *xdg_shell;

static struct wlr_layer_shell_v1 *layer_shell;
static struct wlr_xdg_decoration_manager_v1 *decoration_manager;
static struct wlr_scene_tree *layers[NUM_LAYERS];
static const  int layermap[] = { LyrBg, LyrBottom, LyrTop, LyrOverlay};

static struct Client *gclient;
static int grab_x, grab_y;

static struct wl_list mons;
static struct wl_list clients;
static struct wl_list keyboards;

static uint32_t current_desktop;
struct Monitor *current_monitor;
static struct Client *focused_client;

#include "src/server.h"
#include "src/renderer.h"
#include "config.h"

void cyclefocus(const Arg *arg){
  if(wl_list_length(&clients) < 2) return;
  struct Client *next_client = wl_container_of(clients.prev, next_client, link);
  setfocus(next_client);
}

void changedesktop(const Arg *arg){
  current_desktop = arg->i;
  struct Client *client;
  wl_list_for_each(client, &clients, link){
    if(client->desktop_index == current_desktop && client->surface.xdg->surface->mapped){ // sedcond statement might be USELESS!!!!!
      wlr_scene_node_set_enabled(&client->scene_tree->node, true);
    }
    else{
      wlr_scene_node_set_enabled(&client->scene_tree->node, false);
    }
  }
  //arrange();
}

void
spawn(const Arg *arg)
{
	if (fork() == 0) {
		dup2(STDERR_FILENO, STDOUT_FILENO);
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
    exit(1);
	}
}

void killclient(const Arg *arg){
  if(focused_client == NULL) return;
  if(wl_list_empty(&clients)) return;
  if(focused_client->type == X11){
    wlr_xwayland_surface_close(focused_client->surface.xwayland);
  }
  else{
    wlr_xdg_toplevel_send_close(focused_client->surface.xdg->toplevel);
  }
}

void setfocus(struct Client *client){
  if(client == NULL) return;
  focused_client = client;
  struct wlr_surface *prev_surface = seat->keyboard_state.focused_surface;
  struct wlr_surface *surface = NULL;
  if(client->type == X11){
    surface = client->surface.xwayland->surface;
  }
  else{
    surface = client->surface.xdg->surface;
  }
  if(prev_surface == surface) return;
  /*
  if(prev_surface){
    struct wlr_xdg_toplevel *prev_toplevel = wlr_xdg_toplevel_try_from_wlr_surface(prev_surface);
    if(prev_toplevel != NULL){
      wlr_xdg_toplevel_set_activated(prev_toplevel, false);
    }
  }
  */
   // TODO : well alot, this function needs a rewrite later
  wlr_scene_node_raise_to_top(&client->scene_tree->node);
  wl_list_remove(&client->link);
  wl_list_insert(&clients, &client->link);
  if(client->type == X11){
    wlr_xwayland_surface_activate(client->surface.xwayland, 1); 
  }
  else{
    wlr_xdg_toplevel_set_activated(client->surface.xdg->toplevel, 1);
  }

  struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
  if(keyboard != NULL){
    wlr_seat_keyboard_notify_enter(seat, surface, keyboard->keycodes, keyboard->num_keycodes, &keyboard->modifiers);
  }  
}

static void updateborders(struct Client *client, int width, int height){
  //t
  wlr_scene_rect_set_size(client->border[0], width, client->bw);
  wlr_scene_node_set_position(&client->border[0]->node, 0, 0);
  //l
  wlr_scene_rect_set_size(client->border[1], client->bw, height);
  wlr_scene_node_set_position(&client->border[1]->node, 0, 0);
  //b
  wlr_scene_rect_set_size(client->border[2], width, client->bw);
  wlr_scene_node_set_position(&client->border[2]->node, 0, height - client->bw);
  //r
  wlr_scene_rect_set_size(client->border[3], client->bw, height);
  wlr_scene_node_set_position(&client->border[3]->node, width - client->bw, 0);
}

void arrangelayers(struct Monitor *mon){
  if(!mon) return;

  for(int i = 0; i < 4; i++){
    struct LayerSurface *layer;
    wl_list_for_each(layer, &mon->layers[i], link){
      struct wlr_layer_surface_v1 *layer_surface = layer->layer_surface;
      struct wlr_layer_surface_v1_state *state = &layer_surface->current;

      struct wlr_box box = {.width = state->desired_width, .height = state->desired_height};
      if(box.width == 0) box.width = mon->m.width;
      if(box.height == 0) box.height = mon->m.height;

      box.x = mon->m.x;
      box.y = mon->m.y;

      wlr_scene_node_set_position(&layer->scene_tree->node, box.x, box.y);
      wlr_layer_surface_v1_configure(layer_surface, box.width, box.height);

      if(state->exclusive_zone > 0){
        if((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP)){
          mon->m.y += state->exclusive_zone;
        }
        if((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM)){
          mon->m.height -= state->exclusive_zone;
        }
        if((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT)){
          mon->m.x += state->exclusive_zone;
        }
        if((state->anchor & ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT)){
          mon->m.width -= state->exclusive_zone;
        }
      }
    }
  }
}

void commitlayersurfacenotify(struct wl_listener *listener, void *data){
  struct LayerSurface *layer = wl_container_of(listener, layer, surface_commit);
  struct wlr_layer_surface_v1 *layer_surface = layer->layer_surface;
  struct wlr_scene_tree *scene_layer = layers[layermap[layer_surface->current.layer]];
  struct wlr_layer_surface_v1_state old_state;

  if(layer->layer_surface->initial_commit){
    old_state = layer->layer_surface->current;
    layer->layer_surface->current = layer->layer_surface->pending;
    arrangelayers(layer->mon);
    layer->layer_surface->current = old_state;
    return;
  }

  if(layer_surface->current.committed == 0 && layer->mapped == layer_surface->surface->mapped) return;

  layer->mapped = layer_surface->surface->mapped;
  wlr_scene_node_set_enabled(&layer->scene_tree->node, layer->mapped);
  wlr_scene_node_set_enabled(&layer->popups->node, layer->mapped);
	if(scene_layer != layer->scene_tree->node.parent){
		wlr_scene_node_reparent(&layer->scene_tree->node, scene_layer);
		wl_list_remove(&layer->link);
		wl_list_insert(&layer->mon->layers[layer_surface->current.layer], &layer->link);
		wlr_scene_node_reparent(&layer->popups->node, (layer_surface->current.layer
				< ZWLR_LAYER_SHELL_V1_LAYER_TOP ? layers[LyrTop] : scene_layer));
	}

	arrangelayers(layer->mon);
}

void unmaplayersurfacenotify(struct wl_listener *listener, void *data){
  struct LayerSurface *layer = wl_container_of(listener, layer, unmap);

  layer->mapped = 0;
  wlr_scene_node_set_enabled(&layer->scene_tree->node, 0);
	if(layer->layer_surface->output && (layer->mon = layer->layer_surface->output->data))
		arrangelayers(layer->mon);
	if(layer->layer_surface->surface == seat->keyboard_state.focused_surface)
    return;
		//setfocus(focustop(current_monitor), 1);
}

void destroylayersurfacenotify(struct wl_listener *listener, void *data){
  struct LayerSurface *layer = wl_container_of(listener, layer, destroy);
  wl_list_remove(&layer->link);
  wl_list_remove(&layer->destroy.link);
  wl_list_remove(&layer->unmap.link);
  wl_list_remove(&layer->surface_commit.link);
  wlr_scene_node_destroy(&layer->scene_tree->node);
  wlr_scene_node_destroy(&layer->popups->node);
  free(layer);
}

void newlayersurface(struct wl_listener *listener, void *data){
  struct wlr_layer_surface_v1 *layer_surface = data;
  struct LayerSurface *layer;
  struct wlr_surface *surface = layer_surface->surface;
  struct wlr_scene_tree *scene_layer = layers[layermap[layer_surface->pending.layer]]; 

  if(!layer_surface->output && 
    !(layer_surface->output = current_monitor ? current_monitor->wlr_output : NULL)){
    wlr_layer_surface_v1_destroy(layer_surface);
    return;
  }

  layer = layer_surface->data = calloc(1, sizeof(*layer));
  layer->type = LayerShell;

  layer->surface_commit.notify = commitlayersurfacenotify;
  wl_signal_add(&surface->events.commit, &layer->surface_commit);
  layer->unmap.notify = unmaplayersurfacenotify;
  wl_signal_add(&surface->events.unmap, &layer->unmap);
  layer->destroy.notify = destroylayersurfacenotify;
  wl_signal_add(&layer_surface->events.destroy, &layer->destroy);

  layer->layer_surface = layer_surface;
  layer->mon = layer_surface->output->data;
  layer->scene_layer_surface = wlr_scene_layer_surface_v1_create(scene_layer, layer_surface);
  layer->scene_tree = layer->scene_layer_surface->tree;
  layer->popups = surface->data = wlr_scene_tree_create(layer_surface->current.layer
			< ZWLR_LAYER_SHELL_V1_LAYER_TOP ? layers[LyrTop] : scene_layer);
	layer->scene_tree->node.data = layer->popups->node.data = layer;

  wlr_scene_node_set_enabled(&layer->scene_tree->node, false);
  wl_list_insert(&layer->mon->layers[layer_surface->pending.layer], &layer->link);
  wlr_surface_send_enter(surface, layer_surface->output);
}


void seatsetprimaryselection(struct wl_listener *listener, void *data){
  struct wlr_seat_request_set_primary_selection_event *event = data;
  wlr_seat_set_primary_selection(seat, event->source, event->serial);
}

void seatsetselection(struct wl_listener *listener, void *data){
  struct wlr_seat_request_set_selection_event *event = data; 
  wlr_seat_set_selection(seat, event->source, event->serial);
}

void seatrequestcursor(struct wl_listener *listener, void *data){
  struct wlr_seat_pointer_request_set_cursor_event *event = data;
  struct wlr_seat_client *focused_client = seat->pointer_state.focused_client;
  if(focused_client == event->seat_client){
    wlr_cursor_set_surface(cursor, event->surface, event->hotspot_x, event->hotspot_y);
  }
}

void decoration_set_mode(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, decoration_set_mode);
  if(client->surface.xdg->initialized){
    wlr_xdg_toplevel_decoration_v1_set_mode(client->decoration, WLR_XDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  }
}

void decoration_destroy(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, decoration_destroy);
  wl_list_remove(&client->decoration_destroy.link);
  wl_list_remove(&client->decoration_set_mode.link);
}

void newdecoration(struct wl_listener *listener, void *data){
  struct wlr_xdg_toplevel_decoration_v1 *decoration = data; 
  struct wlr_xdg_toplevel *toplevel = decoration->toplevel;
  struct Client *client = toplevel->base->data;

  client->decoration = decoration;
  client->decoration_set_mode.notify = decoration_set_mode;
  wl_signal_add(&client->decoration->events.request_mode, &client->decoration_set_mode);
  client->decoration_destroy.notify = decoration_destroy;
  wl_signal_add(&client->decoration->events.destroy, &client->decoration_destroy);
}

void rendermon(struct wl_listener *listener, void *data){
  struct Monitor *mon = wl_container_of(listener, mon, frame);  
  struct wlr_scene_output *scene_output = wlr_scene_get_scene_output(scene, mon->wlr_output);

  wlr_scene_output_commit(scene_output, NULL);
  
  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  wlr_scene_output_send_frame_done(scene_output, &now);
}

void requeststatemon(struct wl_listener *listener, void *data){
  struct Monitor *mon = wl_container_of(listener, mon, request_state);
  struct wlr_output_event_request_state *event = data;
  wlr_output_commit_state(mon->wlr_output, event->state);
}

void destroymon(struct wl_listener *listener, void *data){
  struct Monitor *mon = wl_container_of(listener, mon, destroy);
  // this is a bit retarded, i need to make a seperate function to also move the clients to the newmon, so this is a temp solution
  // TODO
  if(current_monitor == mon && !wl_list_empty(&mons)){
    struct Monitor *newmon;
    wl_list_for_each(newmon, &mons, link){
      if(mon != newmon){
        current_monitor = newmon;
        break;
      }
    }
  }
  wl_list_remove(&mon->request_state.link);
  wl_list_remove(&mon->frame.link);
  wl_list_remove(&mon->destroy.link);
  wl_list_remove(&mon->link);
  free(mon);
}

void createmon(struct wl_listener *listener, void *data){
  struct wlr_output *wlr_output = data;
  struct Monitor *mon;
  struct wlr_output_state state;
  
  wlr_output_init_render(wlr_output, server->wlr_allocator, server->wlr_renderer);
  
  wlr_output_state_init(&state);
  wlr_output_state_set_enabled(&state, true);
  wlr_output_state_set_mode(&state, wlr_output_preferred_mode(wlr_output));
  
  wlr_output_commit_state(wlr_output, &state);
  wlr_output_state_finish(&state);

  mon = calloc(1, sizeof(*mon));
  mon->wlr_output = wlr_output;
  struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
  if(mode){
    mon->m.width = mode->width;
    mon->m.height = mode->height;
  }
  else{
    mon->m.width = wlr_output->width;
    mon->m.height = wlr_output->height;
  }
  mon->w = mon->m;
  //mon->m.x = 0; // TODO THIS IS VERY BAD
  //mon->m.y = 0; // TODO THIS IS VERY BAD
  wlr_output->data = mon;
  mon->scene_output = wlr_scene_output_create(scene, mon->wlr_output);
  mon->frame.notify = rendermon; 
  wl_signal_add(&wlr_output->events.frame, &mon->frame);
  mon->request_state.notify = requeststatemon;
  wl_signal_add(&wlr_output->events.request_state, &mon->request_state);
  mon->destroy.notify = destroymon;
  wl_signal_add(&wlr_output->events.destroy, &mon->destroy);

  for(size_t i = 0; i < 4; i++){
    wl_list_init(&mon->layers[i]);
  }

  wl_list_insert(&mons, &mon->link); 

  wlr_output_layout_add_auto(output_layout, wlr_output);
  current_monitor = mon;
}

void changeoutputlayout(struct wl_listener *listener, void *data){
  struct Monitor *mon;
  struct wlr_output_layout_output *output = NULL;
  wl_list_for_each(mon, &mons, link){
    output = wlr_output_layout_get(output_layout, mon->wlr_output);
    mon->m.x = output->x;
    mon->m.y = output->y;
    wlr_scene_output_set_position(mon->scene_output, output->x, output->y);
  }
}

// from dwl
bool keybinding(uint32_t mods, xkb_keysym_t sym)
{
	const Key *k;
	for (k = keys; k < END(keys); k++) {
		if (CLEANMASK(mods) == CLEANMASK(k->mod)
				&& xkb_keysym_to_lower(sym) == xkb_keysym_to_lower(k->keysym)
				&& k->func) {
			k->func(&k->arg);
			return 1;
		}
	}
	return 0;
}



void keyboardmodifiers(struct wl_listener *listener, void *data){
  struct Keyboard *keyboard = wl_container_of(listener, keyboard, modifier);
  wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
  wlr_seat_keyboard_notify_modifiers(seat, &keyboard->wlr_keyboard->modifiers);
}

void keyboardkey(struct wl_listener *listener, void *data){
  struct Keyboard *keyboard = wl_container_of(listener, keyboard, key);
  struct wlr_keyboard_key_event *event = data;

  uint32_t keycode = event->keycode + 8;
  const xkb_keysym_t *syms;
  int nsyms = xkb_state_key_get_syms(keyboard->wlr_keyboard->xkb_state, keycode, &syms);
  bool handled = false;
  uint32_t modifiers = wlr_keyboard_get_modifiers(keyboard->wlr_keyboard);
  if((modifiers & WLR_MODIFIER_ALT) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED){
    for(int i = 0; i < nsyms; i++){
      handled = keybinding(modifiers, syms[i]);
    }
  } 
  if(!handled){
    wlr_seat_set_keyboard(seat, keyboard->wlr_keyboard);
    wlr_seat_keyboard_notify_key(seat, event->time_msec, event->keycode, event->state);
  }
}

void keyboardremove(struct wl_listener *listener, void *data){
  struct Keyboard *keyboard = wl_container_of(listener, keyboard, destroy);
  wl_list_remove(&keyboard->modifier.link);
  wl_list_remove(&keyboard->key.link);
  wl_list_remove(&keyboard->destroy.link);
  free(keyboard);
}

void createkeyboard(struct wlr_input_device *device){
  struct wlr_keyboard *wlr_keyboard = wlr_keyboard_from_input_device(device);
  struct Keyboard *keyboard = calloc(1, sizeof(*keyboard));
  keyboard->wlr_keyboard = wlr_keyboard;
  struct xkb_context *context = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  struct xkb_keymap *keymap = xkb_keymap_new_from_names(context, NULL, XKB_KEYMAP_COMPILE_NO_FLAGS);
  wlr_keyboard_set_keymap(wlr_keyboard, keymap);
  xkb_keymap_unref(keymap);
  xkb_context_unref(context);

  
  wlr_keyboard_set_repeat_info(wlr_keyboard, 35, 200);
  keyboard->key.notify = keyboardkey;
  wl_signal_add(&wlr_keyboard->events.key, &keyboard->key);
  keyboard->modifier.notify = keyboardmodifiers;
  wl_signal_add(&wlr_keyboard->events.modifiers, &keyboard->modifier);
  keyboard->destroy.notify = keyboardremove;
  wl_signal_add(&device->events.destroy, &keyboard->destroy);

  wl_list_insert(&keyboards, &keyboard->link);
}

void createpointer(struct wlr_input_device *device){
  wlr_cursor_attach_input_device(cursor, device);
}

void createinput(struct wl_listener *listener, void *data){
  struct wlr_input_device *device = data;
  switch(device->type){
    case WLR_INPUT_DEVICE_KEYBOARD:
      createkeyboard(device);
      break;
    case WLR_INPUT_DEVICE_POINTER:
      createpointer(device);
      break;
    default:
      break;
  }
	uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
	if (!wl_list_empty(&keyboards)){
		caps |= WL_SEAT_CAPABILITY_KEYBOARD;
	}
	wlr_seat_set_capabilities(seat, caps);
}

void cursormove(){
  if(gclient == NULL) return;
  struct Client *client = gclient; 
  wlr_scene_node_set_position(&client->scene_tree->node, cursor->x - grab_x, cursor->y - grab_y);
  struct wlr_output_layout_output *loutput;
  struct Monitor *mon;
  wl_list_for_each(mon, &mons, link){
    loutput = wlr_output_layout_get(output_layout, mon->wlr_output);
    if(!loutput) continue; 
    if(cursor->x >= loutput->x && cursor->x <= loutput->x + loutput->output->width && cursor->y >= loutput->y && cursor->y <= loutput->y + loutput->output->height){
      client->mon = mon;
      break;
    }
  }
  if(client->type == X11){
    updateborders(client, client->surface.xwayland->width, client->surface.xwayland->height);
  }
  else{
    updateborders(client, client->surface.xdg->current.geometry.width, client->surface.xdg->current.geometry.height);
  }
}

void cursorresize(){
  if(gclient == NULL) return;
  struct Client *client = gclient;
  int new_width = cursor->x - client->scene_tree->node.x;
  int new_height = cursor->y - client->scene_tree->node.y;
  if(new_width >= 50 && new_height >= 50){
    wlr_xdg_toplevel_set_size(client->surface.xdg->toplevel, new_width, new_height);
    updateborders(client, new_width, new_height);
  }
#ifdef XWAYLAND
  else if(client->type == X11){
    wlr_xwayland_surface_configure(client->surface.xwayland, client->scene_tree->node.x, client->scene_tree->node.y, new_width, new_height);
  }
#endif
  updateborders(client, new_width, new_height);
}

void processcursormotion(uint32_t time){
	wlr_scene_node_set_position(&drag_icon->node, (int)round(cursor->x), (int)round(cursor->y)); // TODO : may be broken
  if(cursor_mode == CursorMove){
    cursormove();
    return;
  }
  if(cursor_mode == CursorResize){
    cursorresize();
    return;
  }
  struct wlr_output_layout_output *loutput;
  struct Monitor *mon;
  wl_list_for_each(mon, &mons, link){
    loutput = wlr_output_layout_get(output_layout, mon->wlr_output);
    if(!loutput) return; 
    if(cursor->x >= loutput->x && cursor->x <= loutput->x + loutput->output->width && cursor->y >= loutput->y && cursor->y <= loutput->y + loutput->output->height){
      current_monitor = mon;
      break;
    }
  }
  wlr_cursor_set_xcursor(cursor, cursor_manager, "default");

  double sx, sy;
  struct wlr_surface *surface = NULL;
  struct wlr_scene_node *node = wlr_scene_node_at(&scene->tree.node, cursor->x, cursor->y, &sx, &sy);
  if(!node || node->type != WLR_SCENE_NODE_BUFFER){
    return; 
  } 
  struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
  struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);

  if(scene_surface){
    surface = scene_surface->surface;

    wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
    wlr_seat_pointer_notify_motion(seat, time, sx, sy);
  }
  else{
    wlr_seat_pointer_clear_focus(seat);
  }
};

void cursormotion(struct wl_listener *listener, void *data){
  struct wlr_pointer_motion_event *event = data;
  wlr_cursor_move(cursor, &event->pointer->base, event->delta_x, event->delta_y);
  processcursormotion(event->time_msec);
}

void cursormotionabsolute(struct wl_listener *listener, void *data){
  struct wlr_pointer_motion_absolute_event *event = data;
  wlr_cursor_warp_absolute(cursor, &event->pointer->base, event->x, event->y);
  processcursormotion(event->time_msec);
}

void cursoraxis(struct wl_listener *listener, void *data){
  struct wlr_pointer_axis_event *event = data;
  wlr_seat_pointer_notify_axis(seat, event->time_msec, event->orientation, event->delta, event->delta_discrete,
                               event->source, event->relative_direction);
}

void cursorframe(struct wl_listener *listener, void *data){
  wlr_seat_pointer_notify_frame(seat);
}

void cursorbutton(struct wl_listener *listener, void *data){
  struct wlr_pointer_button_event *event = data;
  wlr_seat_pointer_notify_button(seat, event->time_msec, event->button, event->state);

  double sx, sy;
  struct wlr_surface *surface = NULL;
  struct wlr_scene_node *node = wlr_scene_node_at(&scene->tree.node, cursor->x, cursor->y, &sx, &sy);
  if(!node || node->type != WLR_SCENE_NODE_BUFFER){
    return; 
  }

  switch(event->state){
  case WL_POINTER_BUTTON_STATE_PRESSED:
    /* TODO : expand this into a button.config like in dwl
     * Plus remake this entire garbage
    */
    if(event->button == BTN_LEFT){
      struct wlr_scene_buffer *scene_buffer = wlr_scene_buffer_from_node(node);
      struct wlr_scene_surface *scene_surface = wlr_scene_surface_try_from_buffer(scene_buffer);
      if(!scene_surface){
        cursor_mode = CursorPassthrough;
        gclient = NULL;
        return;
      }

      surface = scene_surface->surface;
      struct wlr_scene_tree *tree = node->parent;
      while(tree != NULL && tree->node.data == NULL){
        tree = tree->node.parent;
      }

      if(tree == NULL || tree->node.data == NULL){
        cursor_mode = CursorPassthrough;
        gclient = NULL;
        return;
      }
     
      /*
      struct LayerSurface *layer = tree->node.data;
      if(layer->type == LayerShell){
        wlr_seat_pointer_notify_enter(seat, surface, sx, sy);
        cursor_mode = CursorPassthrough;
        gclient = NULL;
        return;
      }
      */


      struct Client *client = tree->node.data;
      cursor_mode = CursorMove;
      setfocus(client);
      gclient = client;
      grab_x = cursor->x - client->scene_tree->node.x;
      grab_y = cursor->y - client->scene_tree->node.y;
      client->isfloating = true;
    }
    if(event->button == BTN_RIGHT){
        // TOOD : special resizing in layout/normal when floating 
        return;
      }
    return;
  case WL_POINTER_BUTTON_STATE_RELEASED:
    // BUG : monitors focus could be broken here
    cursor_mode = CursorPassthrough;
    gclient = NULL;
    return;
  }
}

void mapnotify(struct wl_listener *listener, void *data){
  struct wlr_xdg_toplevel *toplevel = data;
  struct Client *client = wl_container_of(listener, client, map);

  if(!client->mon && current_monitor){
    client->mon = current_monitor;
  }

  client->scene_tree = wlr_scene_tree_create(layers[LyrTile]);
  client->scene_surface = client->type == XDGShell ? 
    wlr_scene_xdg_surface_create(client->scene_tree, client->surface.xdg)
    : wlr_scene_subsurface_tree_create(client->scene_tree, client->surface.xwayland->surface);
  client->scene_tree->node.data = client->scene_surface->node.data = client;

  for(int i = 0; i < 4; i++){
    client->border[i] = wlr_scene_rect_create(client->scene_tree, 0, 0, focused_border_color);
    client->border[i]->node.data = client;
  }

  wl_list_insert(&clients, &client->link);
}

void unmapnotify(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, unmap);
  wl_list_remove(&client->link);
  if(!wl_list_empty(&clients)){
    struct Client *next = wl_container_of(clients.next, next, link);
    setfocus(next);
  }
  else{
    wlr_seat_keyboard_clear_focus(seat);
  }
}

void commitnotify(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, commit);
  if(client->surface.xdg->initial_commit){
    wlr_xdg_toplevel_set_size(client->surface.xdg->toplevel, 0, 0);
  }
}

void destroynotify(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, destroy);
  if(seat->keyboard_state.focused_surface == client->surface.xdg->surface){
    wlr_seat_keyboard_clear_focus(seat);
  }

  if(client->border[0]){
    for(int i = 0; i <= 3; i++){
      wlr_scene_node_destroy(&client->border[i]->node);
    }
  }
  wl_list_remove(&client->map.link);
  wl_list_remove(&client->unmap.link);
  wl_list_remove(&client->commit.link);
  wl_list_remove(&client->destroy.link);
  wl_list_remove(&client->maximize.link);
  wl_list_remove(&client->fullscreen.link);
  free(client);
}

void destroynotifyx11(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, destroy);
  if(seat->keyboard_state.focused_surface == client->surface.xwayland->surface){
    wlr_seat_keyboard_clear_focus(seat);
  }
  wl_list_remove(&client->activate.link);
  wl_list_remove(&client->associate.link);
  wl_list_remove(&client->dissociate.link);
  wl_list_remove(&client->destroy.link);
  wl_list_remove(&client->configure.link);
  wl_list_remove(&client->set_hints.link);
  wl_list_remove(&client->maximize.link);
  wl_list_remove(&client->fullscreen.link);
  free(client);
}

void fullscreennotify(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, fullscreen);
  if(client->surface.xdg->initialized){
 		wlr_xdg_surface_schedule_configure(client->surface.xdg);
  }
}
void maximizenotify(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, maximize);
  if(client->surface.xdg->initialized){
 		wlr_xdg_surface_schedule_configure(client->surface.xdg);
  }
}

void newclient(struct wl_listener *listener, void *data){
  struct wlr_xdg_toplevel *toplevel = data;
  struct Client *client;

  client = toplevel->base->data = calloc(1, sizeof(*client));
  client->surface.xdg = toplevel->base;
  client->type = XDGShell;
  client->mon = current_monitor;
  focused_client = client; // TODO : temp fix, maybe move somewhere else, same with x11
  client->bw = 2;
  for(int i = 0; i < 4; i++){
    client->border[i] = NULL;
  }

  client->map.notify = mapnotify;
  wl_signal_add(&toplevel->base->surface->events.map, &client->map);
  client->unmap.notify = unmapnotify;
  wl_signal_add(&toplevel->base->surface->events.unmap, &client->unmap);
  client->commit.notify = commitnotify;
  wl_signal_add(&toplevel->base->surface->events.commit, &client->commit);
  client->destroy.notify = destroynotify;
  wl_signal_add(&toplevel->events.destroy, &client->destroy);
  client->fullscreen.notify = fullscreennotify;
  wl_signal_add(&toplevel->events.request_fullscreen, &client->fullscreen);
  client->maximize.notify = maximizenotify;
  wl_signal_add(&toplevel->events.request_maximize, &client->maximize);
}

void destroydragicon(struct wl_listener *listener, void *data){
  wl_list_remove(&listener->link);
	free(listener);
}

void requeststartdrag(struct wl_listener *listener, void *data){
  struct wlr_seat_request_start_drag_event *event = data;
  if(wlr_seat_validate_pointer_grab_serial(seat, event->origin, event->serial)){
    wlr_seat_start_pointer_drag(seat, event->drag, event->serial);
  }
  else{
    wlr_data_source_destroy(event->drag->source);
  }
}

void startdrag(struct wl_listener *listener, void *data){
  struct wlr_drag *drag = data;
  if(!drag->icon) return;

	drag->icon->data = &wlr_scene_drag_icon_create(drag_icon, drag->icon)->node;
	LISTEN_STATIC(&drag->icon->events.destroy, destroydragicon);
}

void associatex11(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, associate);

  client->map.notify = mapnotify;
  wl_signal_add(&client->surface.xwayland->surface->events.map, &client->map);
  client->unmap.notify = unmapnotify;
  wl_signal_add(&client->surface.xwayland->surface->events.unmap, &client->unmap);
}

void dissociatex11(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, dissociate);
  wl_list_remove(&client->map.link);
  wl_list_remove(&client->unmap.link);
}

void configurex11(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, configure);
  struct wlr_xwayland_surface_configure_event *event = data;

  wlr_xwayland_surface_configure(client->surface.xwayland, 
                                 event->x,
                                 event->y,
                                 event->width,
                                 event->height);

  if(client->scene_tree){
    wlr_scene_node_set_position(&client->scene_tree->node, event->x, event->y);
  }
}

void activatex11(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, activate);
  if(client->surface.xwayland->override_redirect){
    wlr_xwayland_surface_activate(client->surface.xwayland, 1);
  }
}

void sethintsx11(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, set_hints);
  struct wlr_surface *surface = client->surface.xwayland->surface;

  if(!client->surface.xwayland->hints){
    return;
  }

  client->isurgent = xcb_icccm_wm_hints_get_urgency(client->surface.xwayland->hints);
  // TODO this function is mostly unfunctional right now
  // used for setting urgency to x11 client
}

void newclientx11(struct wl_listener *listener, void *data){
  struct wlr_xwayland_surface *xsurface = data;
  struct Client *client;
  
  client = xsurface->data = calloc(1, sizeof(*client));
  client->surface.xwayland = xsurface;
  client->mon = current_monitor;
  client->type = X11;
  focused_client = client; // TODO : temp fix
  if(xsurface->override_redirect){
    client->isfloating = 1;
  }

  client->associate.notify = associatex11;
  wl_signal_add(&xsurface->events.associate, &client->associate);
  client->dissociate.notify = dissociatex11;
  wl_signal_add(&xsurface->events.dissociate, &client->dissociate);
  client->activate.notify = activatex11;
  wl_signal_add(&xsurface->events.request_activate, &client->activate);
  client->configure.notify = configurex11;
  wl_signal_add(&xsurface->events.request_configure, &client->configure);
  client->set_hints.notify = sethintsx11;
  wl_signal_add(&xsurface->events.set_hints, &client->set_hints);
  client->destroy.notify = destroynotifyx11;
  wl_signal_add(&xsurface->events.destroy, &client->destroy);
  client->fullscreen.notify = fullscreennotify;
  wl_signal_add(&xsurface->events.request_fullscreen, &client->fullscreen);
  client->maximize.notify = maximizenotify;
  wl_signal_add(&xsurface->events.request_maximize, &client->maximize);
}

void xwaylandready(struct wl_listener *listener, void *data){
  struct wlr_xcursor *xcursor;
  wlr_xwayland_set_seat(xwayland, seat);

  if((xcursor = wlr_xcursor_manager_get_xcursor(cursor_manager, "default", 1))){
    wlr_xwayland_set_cursor(xwayland,
                            xcursor->images[0]->buffer,
                            xcursor->images[0]->width * 4,
                            xcursor->images[0]->width,
                            xcursor->images[0]->height,
                            xcursor->images[0]->hotspot_x,
                            xcursor->images[0]->hotspot_y);
  }
}

void
handlesig(int signo)
{
	if (signo == SIGCHLD)
		while (waitpid(-1, NULL, WNOHANG) > 0);
	else if (signo == SIGINT || signo == SIGTERM)
		quit();
}


void init(){
  server = calloc(1, sizeof(*server));
  int renderer_fd, i, sig[] = {SIGCHLD, SIGINT, SIGTERM, SIGPIPE};
	struct sigaction sa = {.sa_flags = SA_RESTART, .sa_handler = handlesig};
	sigemptyset(&sa.sa_mask);

	for(i = 0; i < (int)LENGTH(sig); i++){
		sigaction(sig[i], &sa, NULL);
  }

  wlr_log_init(WLR_DEBUG, NULL);  
  server->display = wl_display_create();
  if(!(server->wlr_backend = wlr_backend_autocreate(wl_display_get_event_loop(server->display), NULL))){
    printf("couldnt create backend\n");
    exit(1);
  }
  // TODO : gpureset would be nice and easy to setup;
  if(!(server->wlr_renderer = wlr_renderer_autocreate(server->wlr_backend))){
    printf("couldnt create renderer\n");
    exit(1);
  }
  scene = wlr_scene_create();
  wlr_renderer_init_wl_display(server->wlr_renderer, server->display);
	if(wlr_renderer_get_texture_formats(server->wlr_renderer, WLR_BUFFER_CAP_DMABUF)){
		wlr_drm_create(server->display, server->wlr_renderer);
		wlr_scene_set_linux_dmabuf_v1(scene, wlr_linux_dmabuf_v1_create_with_renderer(server->display, 5, server->wlr_renderer));
	}
	if((renderer_fd = wlr_renderer_get_drm_fd(server->wlr_renderer)) >= 0
      && server->wlr_renderer->features.timeline && server->wlr_backend->features.timeline){
		wlr_linux_drm_syncobj_manager_v1_create(server->display, 1, renderer_fd);
  }
  if(!(server->wlr_allocator = wlr_allocator_autocreate(server->wlr_backend, server->wlr_renderer))){
    printf("couldnt create allocator\n");
    exit(1);
  }

  compositor = wlr_compositor_create(server->display, 6, server->wlr_renderer);
  // TODO : protocols setup here
  wlr_subcompositor_create(server->display);
  wlr_data_device_manager_create(server->display);

  output_layout = wlr_output_layout_create(server->display);
  wl_signal_add(&output_layout->events.change, &output_layout_change);
  xdg_output_manager = wlr_xdg_output_manager_v1_create(server->display, output_layout);
  viewporter = wlr_viewporter_create(server->display);
	fractional_scale = wlr_fractional_scale_manager_v1_create(server->display, 1);
  //activation = wlr_xdg_activation_v1_create(display);

  //wl_signal_add(&xdg_output_mgr->events.apply, &output_manager_apply);
	//wl_signal_add(&xdg_output_mgr->events.test, &output_manager_test);


  wl_list_init(&keyboards);
  wl_signal_add(&server->wlr_backend->events.new_input, &new_input);


  seat = wlr_seat_create(server->display, "seat0");
  wl_signal_add(&seat->events.request_set_cursor, &request_cursor);
  wl_signal_add(&seat->events.request_set_selection, &request_set_selection);
  wl_signal_add(&seat->events.request_set_primary_selection, &request_set_primary_selection);
  wl_signal_add(&seat->events.request_start_drag, &request_start_drag);
  wl_signal_add(&seat->events.start_drag, &start_drag);
  
  xdg_shell = wlr_xdg_shell_create(server->display, 6);
  wl_signal_add(&xdg_shell->events.new_toplevel, &new_client);
  //wl_signal_add(&xdg_shell->events.new_popup, &new_popup);
  wl_list_init(&clients);

  cursor = wlr_cursor_create();
  cursor_manager = wlr_xcursor_manager_create(NULL, 24);
  cursor_mode = CursorPassthrough;
  setenv("XCURSOR_SIZE", "24", 1);
  wlr_cursor_attach_output_layout(cursor, output_layout);
  wl_signal_add(&cursor->events.frame, &cursor_frame);
  wl_signal_add(&cursor->events.axis, &cursor_axis);
  wl_signal_add(&cursor->events.button, &cursor_button);
  wl_signal_add(&cursor->events.motion, &cursor_motion);
  wl_signal_add(&cursor->events.motion_absolute, &cursor_motion_absolute);

  decoration_manager = wlr_xdg_decoration_manager_v1_create(server->display);
  wl_signal_add(&decoration_manager->events.new_toplevel_decoration, &new_decoration);

  layer_shell = wlr_layer_shell_v1_create(server->display, 3);
  wl_signal_add(&layer_shell->events.new_surface, &new_layer_surface);
  for(size_t i = 0; i < NUM_LAYERS; i++){
    layers[i] = wlr_scene_tree_create(&scene->tree);
  }
	drag_icon = wlr_scene_tree_create(&scene->tree);
	wlr_scene_node_place_below(&drag_icon->node, &layers[LyrBlock]->node);

  current_desktop = 1;

  wl_list_init(&mons);
  server->new_output.notify = createmon;
  wl_signal_add(&server->wlr_backend->events.new_output, &server->new_output);

  // for xwayland
  unsetenv("DISPLAY");
#ifdef XWAYLAND
  if((xwayland = wlr_xwayland_create(server->display, compositor, 1))){
		wl_signal_add(&xwayland->events.ready, &xwayland_ready);
		wl_signal_add(&xwayland->events.new_surface, &new_xwayland_surface);

		setenv("DISPLAY", xwayland->display_name, 1);
  }
  else{
    fprintf(stderr, "failed to init xwayland, continuing without it\n");
  }
#endif
}

void run(){
  const char *socket = wl_display_add_socket_auto(server->display);
  if(!socket){
    fprintf(stderr, "failed to init a socket\n");
    exit(1);
  }
  setenv("WAYLAND_DISPLAY", socket, 1);
  if(!wlr_backend_start(server->wlr_backend)){
    fprintf(stderr, "failed to start the backend\n");
    exit(1);
  }
	wlr_cursor_set_xcursor(cursor, cursor_manager, "default");
  wl_display_run(server->display);
}

void quit(){
  destroylisteners();
#ifdef XWAYLAND
	wlr_xwayland_destroy(xwayland);
	xwayland = NULL;
#endif
  wl_display_destroy_clients(server->display); 
	wlr_xcursor_manager_destroy(cursor_manager);
  wlr_backend_destroy(server->wlr_backend);
  wl_display_destroy(server->display); 
  wlr_scene_node_destroy(&scene->tree.node);
}

void destroylisteners(){
	wl_list_remove(&server->new_output.link);
}

int main(){
  init();
  run();
  quit();
}
