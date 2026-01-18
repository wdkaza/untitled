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
#include "config.h"

enum CURSOR_MODE {CursorPassthrough, CursorMove, CursorResize};
enum { XDGShell, LayerShell, X11 };
enum { LyrBg, LyrBottom, LyrTile, LyrFloat, LyrTop, LyrFS, LyrOverlay, LyrBlock, NUM_LAYERS };

struct Monitor{
  struct wl_list link;
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
  struct Monitor *mon;

  struct wlr_scene_tree *scene_tree;
  //struct wlr_scene_rect *border[4];
  //uint32_t bw;

  union{
    struct wlr_xdg_surface *xdg;
    struct wlr_xwayland_surface *xwayland;
  } surface;


  int isfloating;  
  int isfullscreen;

  struct wlr_xdg_toplevel_decoration_v1 *decoration;
  struct wl_listener decoration_set_mode;
  struct wl_listener decoration_destroy;
  
  struct wl_listener fullscreen;
  struct wl_listener maximize;
  struct wl_listener map;
  struct wl_listener unmap;
  struct wl_listener commit;
  struct wl_listener destroy;
};
/* struct Popup{}; */

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

static void init();
static void run();
static void quit();
static void destroylisteners();
#ifdef XWAYLAND

// most of xwayland come will come from straight from dwl

//static void activatex11(struct wl_listener *listener, void *data);
//static void associatex11(struct wl_listener *listener, void *data);
//static void configurex11(struct wl_listener *listener, void *data);
//static void createnotifyx11(struct wl_listener *listener, void *data);
//static void dissociatex11(struct wl_listener *listener, void *data);
//static void sethints(struct wl_listener *listener, void *data);
//static void xwaylandready(struct wl_listener *listener, void *data);
//static struct wl_listener new_xwayland_surface = {.notify = createnotifyx11};
//static struct wl_listener xwayland_ready = {.notify = xwaylandready};
static struct wlr_xwayland *xwayland;
#endif


static struct wl_display *display;
static struct wlr_backend *backend;
static struct wlr_renderer *renderer;
static struct wlr_allocator *allocator;
static struct wlr_compositor *compositor;
static struct wlr_scene *scene;
static struct wlr_output_layout *output_layout;
static struct wlr_xdg_output_manager_v1 *xdg_output_manager;
//struct wlr_xdg_activation_v1 *activation;

static struct wlr_cursor *cursor;
static uint32_t cursor_mode;
static struct wlr_xcursor_manager *cursor_manager;

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

void init(){
  wlr_log_init(WLR_DEBUG, NULL);  
  display = wl_display_create();
  if(!(backend = wlr_backend_autocreate(wl_display_get_event_loop(display), NULL))){
    printf("couldnt create backend\n");
    exit(1);
  }
  // TODO : gpureset would be nice and easy to setup;
  if(!(renderer = wlr_renderer_autocreate(backend))){
    printf("couldnt create renderer\n");
    exit(1);
  }
  wlr_renderer_init_wl_display(renderer, display);
	if(wlr_renderer_get_texture_formats(renderer, WLR_BUFFER_CAP_DMABUF)){
		wlr_drm_create(display, renderer);
		wlr_scene_set_linux_dmabuf_v1(scene, wlr_linux_dmabuf_v1_create_with_renderer(display, 5, renderer));
	}
  int renderer_fd;
	if((renderer_fd = wlr_renderer_get_drm_fd(renderer)) >= 0
      && renderer->features.timeline && backend->features.timeline){
		wlr_linux_drm_syncobj_manager_v1_create(display, 1, renderer_fd);
  }
  if(!(allocator = wlr_allocator_autocreate(backend, renderer))){
    printf("couldnt create allocator\n");
    exit(1);
  }

  compositor = wlr_compositor_create(display, 6, renderer);
  // TODO : protocols setup here
  wlr_subcompositor_create(display);
  wlr_data_device_manager_create(display);

  scene = wlr_scene_create();
  output_layout = wlr_output_layout_create(display);
  xdg_output_manager = wlr_xdg_output_manager_v1_create(display, output_layout);
  //wl_signal_add(&xdg_output_mgr->events.apply, &output_manager_apply);
	//wl_signal_add(&xdg_output_mgr->events.test, &output_manager_test);


  wl_list_init(&keyboards);
  wl_signal_add(&backend->events.new_input, &new_input);


  seat = wlr_seat_create(display, "seat0");
  wl_signal_add(&seat->events.request_set_cursor, &request_set_cursor);
  wl_signal_add(&seat->events.request_set_selection, &request_set_selection);
  //wl_signal_add(&seat->events.request_set_primary_selection, &request_set_primary_selection);
  //wl_signal_add(&seat->events.request_start_drag, &request_start_drag);
  //wl_signal_add(&seat->events.start_drag, &start_drag);
  
  xdg_shell = wlr_xdg_shell_create(display, 6);
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

  decoration_manager = wlr_xdg_decoration_manager_v1_create(display);
  wl_signal_add(&decoration_manager->events.new_toplevel_decoration, &new_decoration);

  layer_shell = wlr_layer_shell_v1_create(display, 3);
  wl_signal_add(&layer_shell->events.new_surface, &new_layer_surface);
  for(size_t i = 0; i < NUM_LAYERS; i++){
    layers[i] = wlr_scene_tree_create(&scene->tree);
  }

  wl_list_init(&mons);
  wl_signal_add(&backend->events.new_output, &new_output);

  // for xwayland
  unsetenv("DISPLAY");
#ifdef XWAYLAND
  if((xwayland = wlr_xwayland_create(display, compositor, 1))){
		//wl_signal_add(&xwayland->events.ready, &xwayland_ready);
		//wl_signal_add(&xwayland->events.new_surface, &new_xwayland_surface);

		setenv("DISPLAY", xwayland->display_name, 1);
  }
  else{
    fprintf(stderr, "failed to init xwayland, continuing without it\n");
  }
#endif
}

void run(){
  const char *socket = wl_display_add_socket_auto(display);
  if(!socket){
    fprintf(stderr, "failed to init a socket\n");
    exit(1);
  }
  if(!wlr_backend_start(backend)){
    fprintf(stderr, "failed to start the backend\n");
    exit(1);
  }
	wlr_cursor_set_xcursor(cursor, cursor_manager, "default\n");
  wl_display_run(display);
}

void quit(){
  destroylisteners();
#ifdef XWAYLAND
	wlr_xwayland_destroy(xwayland);
	xwayland = NULL;
#endif
  wl_display_destroy_clients(display); 
	wlr_xcursor_manager_destroy(cursor_manager);
  wlr_backend_destroy(backend);
  wl_display_destroy(display); 
  wlr_scene_node_destroy(&scene->tree.node);
}

void destroylisteners(){
	wl_list_remove(&new_output.link);
}

int main(){
  init();
  run();
  quit();
}
