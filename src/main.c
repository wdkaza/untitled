#include "pixman.h"
#include "render/gles2.h"
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
#include <wlr/types/wlr_ext_data_control_v1.h>
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
#include <limits.h>
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
enum { UP, DOWN, LEFT, RIGHT };

#include "monitor.h"

struct Monitor;
struct mw_renderer;
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

  struct wlr_box prev;
  struct wlr_box geom;


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

  uint32_t resize;
};
/* struct Popup{}; */

typedef union{
  int i;
  uint32_t ui;
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

typedef struct{
  const char *name;
  float scale;
  int x, y;
  enum wl_output_transform rotation;
}MonitorRule;

struct PointerConstraint{
  struct wlr_pointer_constraint_v1 *constraint;
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
static void createpointer(struct wlr_pointer *pointer);
static void processcursormotion(uint32_t time);
static void newlayersurface(struct wl_listener *listener, void *data);
static void startdrag(struct wl_listener *listener, void *data);
static void requeststartdrag(struct wl_listener *listener, void *data);
static void destroydragicon(struct wl_listener *listener, void *data);
static void powermanagersetmode(struct wl_listener *listener, void *data);
static void arrangelayers(struct Monitor *mon);
static void commitlayersurfacenotify(struct wl_listener *listener, void *data);
static void renderlayer(struct mw_renderer *renderer, struct Monitor *mon, struct wl_list *layer_list);
static void renderpopups(struct mw_renderer *renderer, struct Monitor *mon, struct wlr_xdg_surface *xdg, int sx, int sy);
static void rendersurface(struct mw_renderer *renderer, struct Monitor *mon, struct wlr_surface *surface, int sx, int sy);
static void unmaplayersurfacenotify(struct wl_listener *listener, void *data);
static void destroylayersurfacenotify(struct wl_listener *listener, void *data);
static void setfullscreen(struct Client *client, int fullscreen);
static void changeoutputlayout(struct wl_listener *listener, void *data);
static void outputmanagerapply(struct wl_listener *listener, void *data);
static void outputmanagertest(struct wl_listener *listener, void *data);
static void newpopup(struct wl_listener *listener, void *data);
static void commitpopup(struct wl_listener *listener, void *data);
static void createpointerconstraint(struct wl_listener *listener, void *data);
static void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint);
static void destroypointerconstraint(struct wl_listener *listener, void *data);
static void cursorwarptohint(void);
static void urgent(struct wl_listener *listener, void *data);
static void sendframedone(struct wlr_surface *surface, int sx, int sy, void *data);
static void outputmanagerapplyortest(struct wlr_output_configuration_v1 *config, int test);
struct Client *find_client_by_direction(struct Client *tc, const Arg *arg, bool findfloating, bool ignore_align);
struct Client *direction_select(const Arg *arg);
static void setfocus(struct Client *client);
static void spawn(const Arg *arg);
static void killclient(const Arg *arg);
static void moveresize(const Arg *arg);
static void togglefullscreen(const Arg *arg);
static void cyclefocus(const Arg *arg);
static void changedesktop(const Arg *arg);
static void focusdir(const Arg *arg);
static void exitwm(const Arg *arg);
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
static struct wl_listener output_manager_apply = {.notify = outputmanagerapply};
static struct wl_listener request_activate = {.notify = urgent};
static struct wl_listener output_manager_test = {.notify = outputmanagertest};
static struct wl_listener request_set_primary_selection = {.notify = seatsetprimaryselection};
static struct wl_listener new_pointer_constraint = {.notify = createpointerconstraint};
static struct wl_listener output_power_manager_set_mode = {.notify = powermanagersetmode};
static struct wl_listener request_cursor = {.notify = seatrequestcursor};
static struct wl_listener output_layout_change = {.notify = changeoutputlayout};
static struct wl_listener new_decoration = {.notify = newdecoration};
static struct wl_listener new_input = {.notify = createinput};
static struct wl_listener new_client = {.notify = newclient};
static struct wl_listener new_popup = {.notify = newpopup};
static struct wl_listener start_drag = {.notify = startdrag};
static struct wl_listener request_start_drag = {.notify = requeststartdrag};


static struct Server *server;
static struct wlr_compositor *compositor;
static struct wlr_scene *scene;
static struct wlr_output_layout *output_layout;
static struct wlr_output_manager_v1 *output_manager;
static struct wlr_viewporter *viewporter;
static struct wlr_fractional_scale_manager_v1 *fractional_scale;
static struct wlr_xdg_activation_v1 *activation;
static struct wlr_pointer_constraints_v1 *pointer_constraints;
static struct wlr_relative_pointer_manager_v1 *relative_pointer_mgr;
static struct wlr_pointer_constraint_v1 *active_constraint;
static struct wlr_output_power_manager_v1 *power_manager;
//static struct wlr_idle_notifier_v1 *idle_notifier;
//static struct wlr_idle_inhibit_manager_v1 *idle_inihibit_manager;

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
static struct Client *focused_client; // dont know if i should keep it, 
// not a common practice in other wm's from what i saw


#include "server.h"
#include "renderer.h"
#include "config.h"
#include "client.h"
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

void exitwm(const Arg *arg){
  (void)arg;
  quit();
}

void spawn(const Arg *arg){
	if (fork() == 0) {
		dup2(STDERR_FILENO, STDOUT_FILENO);
		setsid();
		execvp(((char **)arg->v)[0], (char **)arg->v);
    exit(1);
	}
}
//
void togglefullscreen(const Arg *arg){
  if(focused_client == NULL) return;
  if(focused_client){
    setfullscreen(focused_client, !focused_client->isfullscreen);
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

// focus dir is ported from mangowc, because im using mangowc while developing this, and 
// that just feels WAY TOO GOOD to not port it over here, anyways props to mangowc for this code

struct Client *find_client_by_direction(struct Client *tc, const Arg *arg, bool findfloating, bool ignore_align) {
	struct Client *c = NULL;
	struct Client **tempClients = NULL;
	int32_t last = -1;

	wl_list_for_each(c, &clients, link) {
		if (c && (findfloating || !c->isfloating) &&
			c->desktop_index == current_desktop) {
			last++;
		}
	}

	if (last < 0) {
		return NULL;
	}

	tempClients = malloc((last + 1) * sizeof(struct Client *));
	if (!tempClients) {
		return NULL;
	}

	last = -1;
	wl_list_for_each(c, &clients, link) {
		if (c && (findfloating || !c->isfloating) &&
			c->desktop_index == current_desktop) {
			last++;
			tempClients[last] = c;
		}
	}

	int32_t sel_x = tc->geom.x;
	int32_t sel_y = tc->geom.y;
	int64_t distance = LLONG_MAX;
	int64_t same_monitor_distance = LLONG_MAX;
	struct Client *tempFocusClients = NULL;
	struct Client *tempSameMonitorFocusClients = NULL;

	switch (arg->i) {
	case UP:
		if (!ignore_align) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.y < sel_y &&
					tempClients[_i]->geom.x == sel_x &&
					tempClients[_i]->mon == tc->mon) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance = 
            dis_x * dis_x + dis_y * dis_y;
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				int32_t dis_x = tempClients[_i]->geom.x - sel_x;
				int32_t dis_y = tempClients[_i]->geom.y - sel_y;
				if (tempClients[_i]->geom.y < sel_y &&
					tempClients[_i]->mon == tc->mon &&
					(dis_y < 0 ? -dis_y : dis_y) >= (dis_x < 0 ? -dis_x : dis_x)) {
					int64_t tmp_distance = 
            dis_x * dis_x + dis_y * dis_y;
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				int32_t dis_x = tempClients[_i]->geom.x - sel_x;
				int32_t dis_y = tempClients[_i]->geom.y - sel_y;
				if (tempClients[_i]->geom.y < sel_y &&
					(dis_y < 0 ? -dis_y : dis_y) >= (dis_x < 0 ? -dis_x : dis_x)) {
					int64_t tmp_distance = 
            dis_x * dis_x + dis_y * dis_y;
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tempClients[_i]->mon == tc->mon &&
						tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		break;
	case DOWN:
		if (!ignore_align) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.y > sel_y &&
					tempClients[_i]->geom.x == sel_x &&
					tempClients[_i]->mon == tc->mon) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
            dis_x * dis_x + dis_y * dis_y;
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				int32_t dis_x = tempClients[_i]->geom.x - sel_x;
				int32_t dis_y = tempClients[_i]->geom.y - sel_y;
				if (tempClients[_i]->geom.y > sel_y &&
					tempClients[_i]->mon == tc->mon &&
					(dis_y < 0 ? -dis_y : dis_y) >= (dis_x < 0 ? -dis_x : dis_x)) {
					int64_t tmp_distance = 
            dis_x * dis_x + dis_y * dis_y;
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				int32_t dis_x = tempClients[_i]->geom.x - sel_x;
				int32_t dis_y = tempClients[_i]->geom.y - sel_y;
				if (tempClients[_i]->geom.y > sel_y &&
					(dis_y < 0 ? -dis_y : dis_y) >= (dis_x < 0 ? -dis_x : dis_x)) {
					int64_t tmp_distance = dis_x * dis_x + dis_y * dis_y;
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tempClients[_i]->mon == tc->mon &&
						tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		break;
	case LEFT:
		if (!ignore_align) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.x < sel_x &&
					tempClients[_i]->geom.y == sel_y &&
					tempClients[_i]->mon == tc->mon) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance = dis_x * dis_x + dis_y * dis_y;
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				int32_t dis_x = tempClients[_i]->geom.x - sel_x;
				int32_t dis_y = tempClients[_i]->geom.y - sel_y;
				if (tempClients[_i]->geom.x < sel_x &&
					tempClients[_i]->mon == tc->mon &&
					(dis_x < 0 ? -dis_x : dis_x) >= (dis_y < 0 ? -dis_y : dis_y)) {
					int64_t tmp_distance =
            dis_x * dis_x + dis_y * dis_y;
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				int32_t dis_x = tempClients[_i]->geom.x - sel_x;
				int32_t dis_y = tempClients[_i]->geom.y - sel_y;
				if (tempClients[_i]->geom.x < sel_x &&
					(dis_x < 0 ? -dis_x : dis_x) >= (dis_y < 0 ? -dis_y : dis_y)) {
					int64_t tmp_distance =
            dis_x * dis_x + dis_y * dis_y;
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tempClients[_i]->mon == tc->mon &&
						tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		break;
	case RIGHT:
		if (!ignore_align) {
			for (int32_t _i = 0; _i <= last; _i++) {
				if (tempClients[_i]->geom.x > sel_x &&
					tempClients[_i]->geom.y == sel_y &&
					tempClients[_i]->mon == tc->mon) {
					int32_t dis_x = tempClients[_i]->geom.x - sel_x;
					int32_t dis_y = tempClients[_i]->geom.y - sel_y;
					int64_t tmp_distance =
            dis_x * dis_x + dis_y * dis_y;
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				int32_t dis_x = tempClients[_i]->geom.x - sel_x;
				int32_t dis_y = tempClients[_i]->geom.y - sel_y;
				if (tempClients[_i]->geom.x > sel_x &&
					tempClients[_i]->mon == tc->mon &&
					(dis_x < 0 ? -dis_x : dis_x) >= (dis_y < 0 ? -dis_y : dis_y)) {
					int64_t tmp_distance =
            dis_x * dis_x + dis_y * dis_y;
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		if (!tempFocusClients) {
			for (int32_t _i = 0; _i <= last; _i++) {
				int32_t dis_x = tempClients[_i]->geom.x - sel_x;
				int32_t dis_y = tempClients[_i]->geom.y - sel_y;
				if (tempClients[_i]->geom.x > sel_x &&
					(dis_x < 0 ? -dis_x : dis_x) >= (dis_y < 0 ? -dis_y : dis_y)) {
					int64_t tmp_distance =
            dis_x * dis_x + dis_y * dis_y;
					if (tmp_distance < distance) {
						distance = tmp_distance;
						tempFocusClients = tempClients[_i];
					}
					if (tempClients[_i]->mon == tc->mon &&
						tmp_distance < same_monitor_distance) {
						same_monitor_distance = tmp_distance;
						tempSameMonitorFocusClients = tempClients[_i];
					}
				}
			}
		}
		break;
	}

	free(tempClients);
	if (tempSameMonitorFocusClients) {
		return tempSameMonitorFocusClients;
	} else {
		return tempFocusClients;
	}
}

struct Client *direction_select(const Arg *arg){
	struct Client *tc = focused_client;

	if(!tc)
		return NULL;

	if(tc->isfullscreen && !tc->isfloating){
		return NULL;
	}

	return find_client_by_direction(tc, arg, true, 0);
}

void focusdir(const Arg *arg) {
	struct Client *client = direction_select(arg);
	if(client){
		setfocus(client);
  }
}
// mangowc massive w end

void setfocus(struct Client *client){ // TODO small rewrite
  if(client == NULL) return;
  focused_client = client;
  client->isurgent = 0;
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

void updateborders(struct Client *client, int width, int height){
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

void arrangelayer(struct Monitor *mon, struct wl_list *list, struct wlr_box *usable_area, int exclusive){
  struct LayerSurface *layer;
  struct wlr_box full_area = mon->m;

  wl_list_for_each(layer, list, link){
    struct wlr_layer_surface_v1 *layer_surface = layer->layer_surface;

    if(!layer_surface->initialized){
      continue;
    }

		if (exclusive != (layer_surface->current.exclusive_zone > 0)){
      continue;
    }

    wlr_scene_layer_surface_v1_configure(layer->scene_layer_surface, &full_area, usable_area);
		wlr_scene_node_set_position(&layer->popups->node, layer->scene_tree->node.x, layer->scene_tree->node.y);
  }
}

void outputmanagerapply(struct wl_listener *listener, void *data){
  struct wlr_output_configuration_v1 *config = data;
  outputmanagerapplyortest(config, 0);
}

void outputmanagertest(struct wl_listener *listener, void *data){
  struct wlr_output_configuration_v1 *config = data;
  outputmanagerapplyortest(config, 1);
}

void outputmanagerapplyortest(struct wlr_output_configuration_v1 *config, int test){
  struct wlr_output_configuration_head_v1 *config_head;
  int ok = 1;

  wl_list_for_each(config_head, &config->heads, link){
    struct wlr_output *wlr_output = config_head->state.output;
    struct Monitor *mon = wlr_output->data;
    struct wlr_output_state state;

    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, config_head->state.enabled);
    if(!config_head->state.enabled)
      goto apply_or_test;
    
    if(config_head->state.mode){
      wlr_output_state_set_mode(&state, config_head->state.mode);
    }
    else{
      wlr_output_state_set_custom_mode(&state,
                          config_head->state.custom_mode.width,
                          config_head->state.custom_mode.height,
                          config_head->state.custom_mode.refresh);
    }

    wlr_output_state_set_transform(&state, config_head->state.transform);
    wlr_output_state_set_scale(&state, config_head->state.scale);
    wlr_output_state_set_adaptive_sync_enabled(&state, config_head->state.adaptive_sync_enabled);
apply_or_test:
    ok &= test ? wlr_output_test_state(wlr_output, &state)
               : wlr_output_commit_state(wlr_output, &state);

    if(!test && wlr_output->enabled && (mon->m.x != config_head->state.x || mon->m.y != config_head->state.y)){
      wlr_output_layout_add(output_layout, wlr_output,
                            config_head->state.x, config_head->state.y);

      wlr_output_state_finish(&state);
    }
  }

	if(ok){
		wlr_output_configuration_v1_send_succeeded(config);
  }
	else{
		wlr_output_configuration_v1_send_failed(config);
  }
	wlr_output_configuration_v1_destroy(config);
}

void arrangelayers(struct Monitor *mon){
  if(!mon->wlr_output->enabled) return;
  struct wlr_box usable_area = mon->m;
  struct LayerSurface *layer;
  uint32_t layers_above_shell[] = {
    ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
    ZWLR_LAYER_SHELL_V1_LAYER_TOP,
  };

  for(int i = 3; i >= 0; i--){
    arrangelayer(mon, &mon->layers[i], &usable_area, 1);
  }

  for(int i = 3; i >= 0; i--){
    arrangelayer(mon, &mon->layers[i], &usable_area, 0);
  }

  for(int i = 0; i < (int)LENGTH(layers_above_shell); i++){
    wl_list_for_each_reverse(layer, &mon->layers[layers_above_shell[i]], link){
      if(!layer->layer_surface->current.keyboard_interactive || !layer->mapped){
        continue;
      }
      setfocus(NULL);
      client_notify_enter(layer->layer_surface->surface, wlr_seat_get_keyboard(seat));
      return;
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

void renderlayer(struct mw_renderer *renderer, struct Monitor *mon, struct wl_list *layer_list){
  struct LayerSurface *layer;
  wl_list_for_each(layer, layer_list, link){
    struct wlr_surface *surface = layer->layer_surface->surface;
    struct wlr_texture *texture = wlr_surface_get_texture(surface);
    if(!texture) continue;

    struct wlr_box box = {
      .x = layer->scene_tree->node.x - mon->m.x,
      .y = layer->scene_tree->node.y - mon->m.y,
      .width = surface->current.width,
      .height = surface->current.height
    };

    pixman_region32_t damage;
    pixman_region32_init(&damage);
    pixman_region32_union_rect(&damage, &damage, box.x, box.y, box.width, box.height);

    mw_renderer_render_texture_at(renderer, &damage, surface, texture, &box, 1.0, 0.0f);
    pixman_region32_fini(&damage);
  }
}

void rendersurface(struct mw_renderer *renderer, struct Monitor *mon, struct wlr_surface *surface, int sx, int sy){
  struct wlr_subsurface *subsurface;
  wl_list_for_each(subsurface, &surface->current.subsurfaces_below, current.link){
    rendersurface(renderer, mon, subsurface->surface, sx + subsurface->current.x, sy + subsurface->current.y);
  }

  struct wlr_texture *texture = wlr_surface_get_texture(surface);
  if(texture){
    struct wlr_box box = {
      .x = sx,
      .y = sy,
      .width = surface->current.width,
      .height = surface->current.height,
    };

    pixman_region32_t damage;
    pixman_region32_init(&damage);
    pixman_region32_union_rect(&damage, &damage, box.x, box.y, box.width, box.height);
    mw_renderer_render_texture_at(renderer, &damage, surface, texture, &box, 1.0, 10.0f); // corner_radius here
    pixman_region32_fini(&damage);
  }

  wl_list_for_each(subsurface, &surface->current.subsurfaces_above, current.link){
    rendersurface(renderer, mon, subsurface->surface, sx + subsurface->current.x, sy + subsurface->current.y);
  }
}

void sendframedone(struct wlr_surface *surface, int sx, int sy, void *data){
  struct timespec *now = data;
  wlr_surface_send_frame_done(surface, now);
}

void renderpopups(struct mw_renderer *renderer, struct Monitor *mon, struct wlr_xdg_surface *xdg, int sx, int sy){
  struct wlr_xdg_popup *popup;
  wl_list_for_each(popup, &xdg->popups, link){
    if(!popup->base->surface->mapped) continue;

    struct wlr_box geo = popup->current.geometry;
    int popup_sx = sx + xdg->current.geometry.x + popup->current.geometry.x - popup->base->current.geometry.x;
    int popup_sy = sy + xdg->current.geometry.y + popup->current.geometry.y - popup->base->current.geometry.y;
    rendersurface(renderer, mon, popup->base->surface, popup_sx, popup_sy);

    renderpopups(renderer, mon, popup->base, popup_sx, popup_sy);
  }
}

void rendermon(struct wl_listener *listener, void *data){
  struct Monitor *mon = wl_container_of(listener, mon, frame);  
  if(mon->asleep) return;

  //glClearColor(0.0, 0.0, 0.0, 1.0);
  //glClear(GL_COLOR_BUFFER_BIT);
  mw_renderer_begin(server->mw_renderer, mon);
  float color[4] = {0.0, 0.0, 0.0, 1.0}; // tempoarry
  struct wlr_render_rect_options rect = {
    .box = {.x = 0, .y = 0, .width = mon->wlr_output->width, .height = mon->wlr_output->height},
    .color = {.r = color[0], .g = color[1], .b = color[2], .a = color[3]},
  };
  wlr_render_pass_add_rect(server->mw_renderer->pass, &rect);

  renderlayer(server->mw_renderer, mon, &mon->layers[ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND]);
  renderlayer(server->mw_renderer, mon, &mon->layers[ZWLR_LAYER_SHELL_V1_LAYER_BOTTOM]);

  struct Client *client;
  wl_list_for_each_reverse(client, &clients, link){// reverse to fix the z order/stacking whatever you call it
    if(client->isfullscreen && client->mon == mon){
      struct wlr_render_rect_options background_rect = {
        .box = {.x = 0, .y = 0, .width = mon->wlr_output->width, .height = mon->wlr_output->height},
        .color = {.r = fullscreen_bg[0], .g = fullscreen_bg[1], .b = fullscreen_bg[2], .a = fullscreen_bg[3]},
      };
      wlr_render_pass_add_rect(server->mw_renderer->pass, &background_rect);
    }
    struct wlr_surface *surface = NULL;
    struct wlr_texture *texture = NULL;
    surface = client_surface(client);

    if(!surface || !surface->mapped) continue;

    int sx = client->scene_tree->node.x - mon->m.x;
    int sy = client->scene_tree->node.y - mon->m.y;

    if(client->type == XDGShell){
      sx -= client->surface.xdg->current.geometry.x;
      sy -= client->surface.xdg->current.geometry.y;
    }

    rendersurface(server->mw_renderer, mon, surface, sx, sy);

    if(client->type == XDGShell){
      renderpopups(server->mw_renderer, mon, client->surface.xdg, sx, sy);
    }
  }

  renderlayer(server->mw_renderer, mon, &mon->layers[ZWLR_LAYER_SHELL_V1_LAYER_TOP]);
  renderlayer(server->mw_renderer, mon, &mon->layers[ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY]);

  
  if(seat->drag && seat->drag->icon && seat->drag->icon->surface->mapped){
    struct wlr_drag_icon *icon = seat->drag->icon;
    int sx = (int)cursor->x; // BUG : somewhere here with drag icon
    int sy = (int)cursor->y;
    rendersurface(server->mw_renderer, mon, icon->surface, sx, sy);
  }

  pixman_region32_t damage;
  pixman_region32_init(&damage);
  pixman_region32_union_rect(&damage, &damage, 0,0 , mon->wlr_output->width, mon->wlr_output->height);
  mw_renderer_end(server->mw_renderer, &damage, mon);
  pixman_region32_fini(&damage);

  struct timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  struct Client *client2;
  wl_list_for_each(client2, &clients, link){
    struct wlr_surface *surface = client2->type == X11 ? client2->surface.xwayland->surface : client2->surface.xdg->surface;
    if(surface && surface->mapped){
      wlr_surface_for_each_surface(surface, sendframedone, &now);
    }
  }

  struct LayerSurface *layer;
  for(int i = 0; i < 4;i++){
    wl_list_for_each(layer, &mon->layers[i], link){
      if(layer->mapped){
        wlr_surface_for_each_surface(layer->layer_surface->surface, sendframedone, &now);
      }
    }
  }
}

void requeststatemon(struct wl_listener *listener, void *data){
  struct Monitor *mon = wl_container_of(listener, mon, request_state);
  struct wlr_output_event_request_state *event = data;
  wlr_output_commit_state(mon->wlr_output, event->state);
}

void destroymon(struct wl_listener *listener, void *data){
  struct Monitor *mon = wl_container_of(listener, mon, destroy);
  // still TODO : move all of the clients move code to another function later
  struct Monitor *newmon = NULL;
  if(!wl_list_empty(&mons)){
    struct Monitor *monitor;
    wl_list_for_each(monitor, &mons, link){
      if(monitor != mon){
        newmon = monitor;
        break;
      }
    }
  }

  struct Client *client;
  wl_list_for_each(client, &clients, link){
    if(client->mon == mon){
      client->mon = newmon;
      if(newmon && client->scene_tree){
        wlr_scene_node_set_position(&client->scene_tree->node,
                                    client->geom.x - mon->m.x + newmon->m.x,
                                    client->geom.y - mon->m.y + newmon->m.y);
        client->geom.x = client->scene_tree->node.x;
        client->geom.y = client->scene_tree->node.y;// COMEBACK
      }
    }
  }

  if(current_monitor == mon){
    current_monitor = newmon;
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
  const MonitorRule *rule;

  mon = calloc(1, sizeof(*mon));
  mon->wlr_output = wlr_output;
  mon->m.x = -1;
  mon->m.y = -1;
  mon->asleep = 0;

  mw_renderer_init_output(server->mw_renderer, mon);
  struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);

  wlr_output_state_init(&state);
  for(rule = monrules; rule < END(monrules); rule++){
    if(!rule->name || strstr(wlr_output->name, rule->name)){
      mon->m.x = rule->x,
      mon->m.y = rule->y,
      wlr_output_state_set_scale(&state, rule->scale);
      wlr_output_state_set_transform(&state, rule->rotation);
      break;
    }
  }

  mon->w = mon->m;

  wlr_output_state_set_enabled(&state, true);
  wlr_output_state_set_mode(&state, mode);
  wlr_output_commit_state(wlr_output, &state);
  wlr_output_state_finish(&state);

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

  if(mon->m.x == -1 && mon->m.y == -1){
    wlr_output_layout_add_auto(output_layout, wlr_output);
  }
  else{
    wlr_output_layout_add(output_layout, wlr_output, mon->m.x, mon->m.y);
  }
  current_monitor = mon;
}

void changeoutputlayout(struct wl_listener *listener, void *data){
  struct Monitor *mon;
  struct wlr_output_layout_output *layout_output;
  struct wlr_output_configuration_v1 *config = wlr_output_configuration_v1_create();
  struct wlr_output_configuration_head_v1 *config_head;
  wl_list_for_each(mon, &mons, link){
    layout_output = wlr_output_layout_get(output_layout, mon->wlr_output);
    if(!layout_output) continue;
    mon->m.x = layout_output->x;
    mon->m.y = layout_output->y;
    mon->m.width = mon->wlr_output->width;
    mon->m.height = mon->wlr_output->height;
    mon->w = mon->m;
    wlr_scene_output_set_position(mon->scene_output, layout_output->x, layout_output->y);

    config_head = wlr_output_configuration_head_v1_create(config, mon->wlr_output);
    config_head->state.x = mon->m.x;
    config_head->state.y = mon->m.y;
  }

  wlr_output_manager_v1_set_configuration(output_manager, config);
}

void powermanagersetmode(struct wl_listener *listener, void *data){
  struct wlr_output_power_v1_set_mode_event *event = data;
  struct Monitor *mon = event->output->data;
  struct wlr_output_state state;
  wlr_output_state_init(&state);

  switch(event->mode){
    case ZWLR_OUTPUT_POWER_V1_MODE_OFF:
      mon->asleep = 1;
      wlr_output_state_set_enabled(&state, false);
      break;
    case ZWLR_OUTPUT_POWER_V1_MODE_ON:
      mon->asleep = 0;
      wlr_output_state_set_enabled(&state, true);
    break;
  }

  wlr_output_commit_state(mon->wlr_output, &state);
  wlr_output_state_finish(&state);
}

void urgent(struct wl_listener *listener, void *data){
	struct wlr_xdg_activation_v1_request_activate_event *event = data;
	struct Client *client = NULL;
	toplevel_from_wlr_surface(event->surface, &client, NULL);
	if(!client || client == focused_client){
		return;
  }

	client->isurgent = 1;
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
  if((modifiers & MODKEY) && event->state == WL_KEYBOARD_KEY_STATE_PRESSED){
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

void createpointer(struct wlr_pointer *pointer){
	struct libinput_device *device;
	if (wlr_input_device_is_libinput(&pointer->base)
			&& (device = wlr_libinput_get_device_handle(&pointer->base))){

		if(libinput_device_config_tap_get_finger_count(device)){
			libinput_device_config_tap_set_enabled(device, tap_to_click);
			libinput_device_config_tap_set_drag_enabled(device, tap_and_drag);
			libinput_device_config_tap_set_drag_lock_enabled(device, drag_lock);
			libinput_device_config_tap_set_button_map(device, button_map);
		}

		if(libinput_device_config_scroll_has_natural_scroll(device))
			libinput_device_config_scroll_set_natural_scroll_enabled(device, natural_scrolling);

		if(libinput_device_config_dwt_is_available(device))
			libinput_device_config_dwt_set_enabled(device, disable_while_typing);

		if(libinput_device_config_left_handed_is_available(device))
			libinput_device_config_left_handed_set(device, left_handed);

		if(libinput_device_config_middle_emulation_is_available(device))
			libinput_device_config_middle_emulation_set_enabled(device, middle_button_emulation);

		if(libinput_device_config_scroll_get_methods(device) != LIBINPUT_CONFIG_SCROLL_NO_SCROLL)
			libinput_device_config_scroll_set_method(device, scroll_method);

		if(libinput_device_config_click_get_methods(device) != LIBINPUT_CONFIG_CLICK_METHOD_NONE)
			libinput_device_config_click_set_method(device, click_method);

		if(libinput_device_config_send_events_get_modes(device))
			libinput_device_config_send_events_set_mode(device, send_events_mode);

		if(libinput_device_config_accel_is_available(device)){
			libinput_device_config_accel_set_profile(device, accel_profile);
			libinput_device_config_accel_set_speed(device, accel_speed);
		}
	}

  wlr_cursor_attach_input_device(cursor, &pointer->base);
}

void createinput(struct wl_listener *listener, void *data){
  struct wlr_input_device *device = data;
  switch(device->type){
    case WLR_INPUT_DEVICE_KEYBOARD:
      createkeyboard(device);
      break;
    case WLR_INPUT_DEVICE_POINTER:
      createpointer(wlr_pointer_from_input_device(device));
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
  client->geom.x = cursor->x - grab_x;
  client->geom.y = cursor->y - grab_y;
  wlr_scene_node_set_position(&client->scene_tree->node, client->geom.x, client->geom.y);
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
    wlr_xwayland_surface_configure(client->surface.xwayland,
                                   client->geom.x,
                                   client->geom.y,
                                   client->surface.xwayland->width,
                                   client->surface.xwayland->height);
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
  if(new_width < 50 || new_height < 50) return;
#ifdef XWAYLAND
  if(client->type == X11){
    if(client->surface.xwayland){
    wlr_xwayland_surface_configure(client->surface.xwayland, client->scene_tree->node.x, client->scene_tree->node.y, new_width, new_height);
      return;
    }
  }
#endif
  wlr_xdg_toplevel_set_size(client->surface.xdg->toplevel, new_width, new_height);
}

struct Client *clientat(double x, double y){
  struct Client *client;
  wl_list_for_each(client, &clients, link){
    struct wlr_surface *surface = client_surface(client);
    if(!surface || !surface->mapped) continue;
    if(client->desktop_index != current_desktop) continue;
    int cx = client->scene_tree->node.x;
    int cy = client->scene_tree->node.y;
    int cw;
    int ch;
    if(client->type == X11){
      cw = client->surface.xwayland->width;
      ch = client->surface.xwayland->height;
    }
    else{
      struct wlr_box box;
      if(client->surface.xdg->current.geometry.width > 0){
          box = client->surface.xdg->current.geometry;
      } 
      else{
          wlr_surface_get_extents(client->surface.xdg->surface, &box);
      }
      
      cx -= box.x;
      cy -= box.y;
      cw = box.width;
      ch = box.height;
    }    

    if(x >= cx && x < cx + cw && y >= cy && y < cy + ch){
      return client;
    } 
  }
  return NULL;
}

void processcursormotion(uint32_t time){
	wlr_scene_node_set_position(&drag_icon->node, (int)round(cursor->x), (int)round(cursor->y));

  struct wlr_output_layout_output *loutput;
  struct Monitor *mon;
  wl_list_for_each(mon, &mons, link){
    loutput = wlr_output_layout_get(output_layout, mon->wlr_output);
    if(!loutput) continue; 
    if(cursor->x >= loutput->x && cursor->x <= loutput->x + loutput->output->width && cursor->y >= loutput->y && cursor->y <= loutput->y + loutput->output->height){
      current_monitor = mon;
      break;
    }
  }

  if(cursor_mode == CursorMove){
    cursormove();
    return;
  }
  if(cursor_mode == CursorResize){
    cursorresize();
    return;
  }

  struct Client *client = clientat(cursor->x, cursor->y);
  if(!client){
    wlr_cursor_set_xcursor(cursor, cursor_manager, "default");
    if(seat->pointer_state.focused_surface){
      wlr_seat_pointer_clear_focus(seat);
    }
    if(active_constraint){
      wlr_pointer_constraint_v1_send_deactivated(active_constraint);
      active_constraint = NULL;
    }
    return;
  }

  struct wlr_surface *surface = client_surface(client);
  surface = client_surface(client);
  int cx = client->scene_tree->node.x;
  int cy = client->scene_tree->node.y;

  if(client->type == XDGShell){
    cx -= client->surface.xdg->current.geometry.x;
    cy -= client->surface.xdg->current.geometry.y;
  }

  double lx = cursor->x - cx;
  double ly = cursor->y - cy;

  struct wlr_surface *subsurface = NULL;
  double sub_sx;
  double sub_sy;
  if(client->type == XDGShell){
    subsurface = wlr_xdg_surface_surface_at(client->surface.xdg, lx, ly, &sub_sx, &sub_sy);
  }
#ifdef XWAYLAND
  else if(client->type == X11){
    subsurface = wlr_surface_surface_at(client->surface.xwayland->surface, lx, ly, &sub_sx, &sub_sy);
  }
#endif
  if(!subsurface){
    subsurface = surface;
    sub_sx = lx;
    sub_sy = ly;
  }

  //setfocus(client);
  wlr_seat_pointer_notify_enter(seat, subsurface, sub_sx, sub_sy);
  wlr_seat_pointer_notify_motion(seat, time, sub_sx, sub_sy);
  struct wlr_surface *focused = seat->pointer_state.focused_surface;
  
  if(active_constraint && active_constraint->surface != focused){
    wlr_pointer_constraint_v1_send_deactivated(active_constraint);
    active_constraint = NULL;
  }
  
  if(!active_constraint && focused){
    struct wlr_pointer_constraint_v1 *constraint = wlr_pointer_constraints_v1_constraint_for_surface(pointer_constraints, focused, seat);
    if(constraint){
      active_constraint = constraint;
      wlr_pointer_constraint_v1_send_activated(constraint);
    }
  }
};

void cursormotion(struct wl_listener *listener, void *data){
  struct wlr_pointer_motion_event *event = data;
  if(active_constraint && active_constraint->surface == seat->pointer_state.focused_surface){
    wlr_relative_pointer_manager_v1_send_relative_motion(relative_pointer_mgr, seat, (uint64_t)event->time_msec * 1000,
                                                         event->delta_x, event->delta_y, event->unaccel_dx, event->unaccel_dy);

    if(active_constraint->type == WLR_POINTER_CONSTRAINT_V1_LOCKED){
      return;
    }
  }
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

void moveresize(const Arg *arg){
  struct Client *client = clientat(cursor->x, cursor->y);
  if(!client || client->isfullscreen){
      cursor_mode = CursorPassthrough;
      gclient = NULL;
      return;
  }

#ifdef XWAYLAND
  if(client->type == X11 && client->surface.xwayland->override_redirect){
    cursor_mode = CursorPassthrough;
    gclient = NULL;
    return;
  }
#endif

  switch(cursor_mode = arg->ui){
    case CursorMove:
      cursor_mode = CursorMove;
      setfocus(client);
      gclient = client;
      grab_x = cursor->x - client->scene_tree->node.x;
      grab_y = cursor->y - client->scene_tree->node.y;
      client->isfloating = true;
      break;
    case CursorResize:
      cursor_mode = CursorResize;
      setfocus(client);
      gclient = client;
      grab_x = cursor->x - client->scene_tree->node.x;
      grab_y = cursor->y - client->scene_tree->node.y;
      client->isfloating = true;
      return; 
  }
}

void cursorbutton(struct wl_listener *listener, void *data){
  struct wlr_pointer_button_event *event = data;
  struct wlr_keyboard *keyboard;
  uint32_t mods;
  const Button *b;
  switch(event->state){
  case WL_POINTER_BUTTON_STATE_PRESSED:{
    struct wlr_keyboard *keyboard = wlr_seat_get_keyboard(seat);
    uint32_t mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;

    keyboard = wlr_seat_get_keyboard(seat);
    mods = keyboard ? wlr_keyboard_get_modifiers(keyboard) : 0;
    for(b = buttons; b < END(buttons); b++){
      if(CLEANMASK(mods) == CLEANMASK(b->mod) && event->button == b->button && b->func){
        b->func(&b->arg);
        return;
      }
    }
    wlr_seat_pointer_notify_button(seat, event->time_msec, event->button, event->state);
    return;
  }
  case WL_POINTER_BUTTON_STATE_RELEASED:
    // BUG : monitors focus could be broken here
    cursor_mode = CursorPassthrough;
    gclient = NULL;
    wlr_seat_pointer_notify_button(seat, event->time_msec, event->button, event->state);
    return;
  }
}

void cursorwarptohint(void){
	struct Client *client = NULL;
	double sx = active_constraint->current.cursor_hint.x;
	double sy = active_constraint->current.cursor_hint.y;

	toplevel_from_wlr_surface(active_constraint->surface, &client, NULL);
	if(client && active_constraint->current.cursor_hint.enabled){
		wlr_cursor_warp(cursor, NULL, sx + client->geom.x + client->bw, sy + client->geom.y + client->bw);
		wlr_seat_pointer_warp(active_constraint->seat, sx, sy);
	}
}


void createpointerconstraint(struct wl_listener *listener, void *data){
  struct PointerConstraint *pointer_constraint = calloc(1, sizeof(*pointer_constraint));
	pointer_constraint->constraint = data;
  pointer_constraint->destroy.notify = destroypointerconstraint;
  wl_signal_add(&pointer_constraint->constraint->events.destroy, &pointer_constraint->destroy);
}

void cursorconstrain(struct wlr_pointer_constraint_v1 *constraint){
	if(active_constraint == constraint){
		return;
  }

	if(active_constraint){
		wlr_pointer_constraint_v1_send_deactivated(active_constraint);
  }

	active_constraint = constraint;
	wlr_pointer_constraint_v1_send_activated(constraint);
}


void destroypointerconstraint(struct wl_listener *listener, void *data){
	struct PointerConstraint *pointer_constraint = wl_container_of(listener, pointer_constraint, destroy);

	if(active_constraint == pointer_constraint->constraint){
		cursorwarptohint();
		active_constraint = NULL;
	}

	wl_list_remove(&pointer_constraint->destroy.link);
	free(pointer_constraint);
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
  client_surface(client)->data = client->scene_surface;

  client_get_geometry(client, &client->geom);

  if(client_is_unmanaged(client)){
    wlr_scene_node_reparent(&client->scene_tree->node, layers[LyrFloat]);
    wlr_scene_node_set_position(&client->scene_tree->node, client->geom.x, client->geom.y);
    client_set_size(client, client->geom.width, client->geom.height);
    // if client wants focus, we give it focus : TODO
    struct Client *c;
    wl_list_for_each(c, &clients, link){
      if(c != client && client->isfullscreen && current_monitor == c->mon){
        setfullscreen(c, 0);
      }
    }
  }

  if(client->type == X11){
    wlr_scene_node_set_position(&client->scene_tree->node, client->surface.xwayland->x, client->surface.xwayland->y);
    client->geom.x = client->surface.xwayland->x;
    client->geom.y = client->surface.xwayland->y;
  }
  else{
    wlr_scene_node_set_position(&client->scene_tree->node, client->mon->m.x, client->mon->m.y);
    client->geom.x = client->mon->m.x;
    client->geom.y = client->mon->m.y;
  }

  for(int i = 0; i < 4; i++){
    client->border[i] = wlr_scene_rect_create(client->scene_tree, 0, 0, focused_border_color);
    client->border[i]->node.data = client;
  }

  struct Client *window;
  wl_list_for_each(window, &clients, link){
    if(window->isfullscreen && window->mon == client->mon) setfullscreen(window, 0);
  }

  wl_list_insert(&clients, &client->link);
  focused_client = client;
  setfocus(client);
}

void unmapnotify(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, unmap);
  if(client == focused_client){
    focused_client = NULL;
  }

  if(client == gclient){
    gclient = NULL;
    cursor_mode = CursorPassthrough;
  }

  wl_list_remove(&client->link);
  if(!wl_list_empty(&clients)){
    struct Client *next = wl_container_of(clients.next, next, link);
    setfocus(next);
  }
  else{
    wlr_seat_keyboard_clear_focus(seat);
  }
  if(client->scene_tree){
    wlr_scene_node_destroy(&client->scene_tree->node);
    client->scene_tree = NULL;
    client->scene_surface = NULL;
    for(int i = 0; i < 4; i++){
      client->border[i] = NULL;
    }
  }
}

void commitnotify(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, commit);
  if(client->surface.xdg->initial_commit){
    wlr_xdg_toplevel_set_size(client->surface.xdg->toplevel, 0, 0);
  } 
  else{
    struct wlr_box old_geom = client->geom;
    struct wlr_box new_geom;
    client_get_geometry(client, &new_geom);
    client->geom.width = new_geom.width;
    client->geom.height = new_geom.height;
  }
}

void destroynotify(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, destroy);
  struct wlr_surface *surface = client_surface(client);
  if(seat->keyboard_state.focused_surface == surface){
    wlr_seat_keyboard_clear_focus(seat);
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
#ifdef XWAYLAND
  if(client_is_x11(client) && client->surface.xwayland->override_redirect) return;
#endif
  setfullscreen(client, client_wants_fullscreen(client));
}

void maximizenotify(struct wl_listener *listener, void *data){
  struct Client *client = wl_container_of(listener, client, maximize);
  if(client->surface.xdg->initialized){
 		wlr_xdg_surface_schedule_configure(client->surface.xdg);
  }
}

void resize(struct Client *client, struct wlr_box geo){
  struct wlr_box *bbox;
  struct wlr_box clip;

  if(!client->mon || !client_surface(client)->mapped){
    return;
  }

  client->geom = geo;

	wlr_scene_node_set_position(&client->scene_tree->node, client->geom.x, client->geom.y);
	wlr_scene_node_set_position(&client->scene_surface->node, client->bw, client->bw);

  client->resize = client_set_size(client, client->geom.width - 2 * client->bw, client->geom.height - 2 * client->bw);
	client_get_clip(client, &clip);
	wlr_scene_subsurface_tree_set_clip(&client->scene_surface->node, &clip);
}

void setfullscreen(struct Client *client, int fullscreen){
#ifdef XWAYLAND
  if(client_is_x11(client) && client->surface.xwayland->override_redirect) return; 
#endif
  client->isfullscreen = fullscreen;
  if(!client->mon || !client_surface(client)->mapped){
    return;
  }
  client_set_fullscreen(client, fullscreen);
  wlr_scene_node_reparent(&client->scene_tree->node, layers[client->isfullscreen  ? LyrFS : client->isfloating ? LyrFloat : LyrTile]);

  if(fullscreen){
    client->prev = client->geom;
    client->bw = 0; // TODO : temporary fixing the size from border calculation since borders dont work currently anyways
    resize(client, client->mon->m);
  }
  else{
    client->bw = 0; // TODO : same as above
    resize(client, client->prev);
  }
}

void newpopup(struct wl_listener *listener, void *data){
  struct wlr_xdg_popup *popup = data;
  LISTEN_STATIC(&popup->base->surface->events.commit, commitpopup);
}

void commitpopup(struct wl_listener *listener, void *data){
  struct wlr_surface *surface = data;
  struct wlr_xdg_popup *popup = wlr_xdg_popup_try_from_wlr_surface(surface);
  struct Client *client = NULL;
  struct LayerSurface *layer = NULL;
  int type = -1;

  if(!popup->base->initial_commit) return;

  type = toplevel_from_wlr_surface(popup->base->surface, &client, &layer);
  popup->base->surface->data = wlr_scene_xdg_surface_create(popup->parent->data, popup->base);

  if((layer && !layer->mon) || (client && !client->mon)){
    wlr_xdg_popup_destroy(popup);
    return;
  }

  struct wlr_box box;
  if(type == LayerShell){
    box = layer->mon->m;
    box.x -= layer->scene_tree->node.x;
    box.y -= layer->scene_tree->node.y;
  } 
  else{
    struct Monitor *mon = client->mon;
    box.x = mon->m.x - client->scene_tree->node.x + client->surface.xdg->current.geometry.x;
    box.y = mon->m.y - client->scene_tree->node.y + client->surface.xdg->current.geometry.y;
    box.width = mon->m.width;
    box.height = mon->m.height;
  }
	wlr_xdg_popup_unconstrain_from_box(popup, &box);
	wl_list_remove(&listener->link);
	free(listener);
}

void newclient(struct wl_listener *listener, void *data){
  struct wlr_xdg_toplevel *toplevel = data;
  struct Client *client;

  client = toplevel->base->data = calloc(1, sizeof(*client));
  client->surface.xdg = toplevel->base;
  client->type = XDGShell;
  client->mon = current_monitor;
  client->desktop_index = current_desktop;
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

  if(client != focused_client){
    client->isurgent = xcb_icccm_wm_hints_get_urgency(client->surface.xwayland->hints);
  }
}

void newclientx11(struct wl_listener *listener, void *data){
  struct wlr_xwayland_surface *xsurface = data;
  struct Client *client;
  
  client = xsurface->data = calloc(1, sizeof(*client));
  client->surface.xwayland = xsurface;
  client->mon = current_monitor;
  client->type = X11;
  client->desktop_index = current_desktop;
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
                            wlr_xcursor_image_get_buffer(xcursor->images[0]),
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

  server->mw_renderer = calloc(1, sizeof(*server->mw_renderer));
  mw_renderer_init(server->mw_renderer, server);

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
	wlr_single_pixel_buffer_manager_v1_create(server->display);
  wlr_export_dmabuf_manager_v1_create(server->display);
  wlr_alpha_modifier_v1_create(server->display);
  wlr_ext_data_control_manager_v1_create(server->display, 1);
  wlr_presentation_create(server->display, server->wlr_backend, 2);
  wlr_data_device_manager_create(server->display);
  wlr_primary_selection_v1_device_manager_create(server->display);
  wlr_data_control_manager_v1_create(server->display);

  output_layout = wlr_output_layout_create(server->display);
  wl_signal_add(&output_layout->events.change, &output_layout_change);
  wlr_xdg_output_manager_v1_create(server->display, output_layout);
  viewporter = wlr_viewporter_create(server->display);
	fractional_scale = wlr_fractional_scale_manager_v1_create(server->display, 1);

  power_manager = wlr_output_power_manager_v1_create(server->display);
  wl_signal_add(&power_manager->events.set_mode, &output_power_manager_set_mode);

  //idle_notifier = wlr_idle_notifier_v1_create(server->display);

  //idle_inihibit_manager = wlr_idle_inhibit_v1_create(server->display);
  //wl_signal_add(&idle_inihibit_manager->events.new_inhibitor, &new_idle_inhibitor);

  activation = wlr_xdg_activation_v1_create(server->display);
	wl_signal_add(&activation->events.request_activate, &request_activate);

  output_manager = wlr_output_manager_v1_create(server->display);
  wl_signal_add(&output_manager->events.apply, &output_manager_apply);
	wl_signal_add(&output_manager->events.test, &output_manager_test);


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
  wl_signal_add(&xdg_shell->events.new_popup, &new_popup);
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

	pointer_constraints = wlr_pointer_constraints_v1_create(server->display);
	wl_signal_add(&pointer_constraints->events.new_constraint, &new_pointer_constraint);

  relative_pointer_mgr = wlr_relative_pointer_manager_v1_create(server->display);

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

  wlr_cursor_destroy(cursor);
	wlr_xcursor_manager_destroy(cursor_manager);

  wlr_scene_node_destroy(&scene->tree.node);
  
  mw_renderer_destroy(server->mw_renderer);
  free(server->mw_renderer);

  wlr_allocator_destroy(server->wlr_allocator);
  wlr_backend_destroy(server->wlr_backend);

  wlr_scene_node_destroy(&scene->tree.node);
  wl_display_destroy(server->display);
  free(server);
  server = NULL;
}

void destroylisteners(){
  wl_list_remove(&server->new_output.link);
  wl_list_remove(&output_layout_change.link);
  wl_list_remove(&output_manager_apply.link);
  wl_list_remove(&output_manager_test.link);
  wl_list_remove(&output_power_manager_set_mode.link);
  wl_list_remove(&new_input.link);
  wl_list_remove(&request_cursor.link);
  wl_list_remove(&request_set_selection.link);
  wl_list_remove(&request_set_primary_selection.link);
  wl_list_remove(&request_start_drag.link);
  wl_list_remove(&start_drag.link);
  wl_list_remove(&cursor_motion.link);
  wl_list_remove(&cursor_motion_absolute.link);
  wl_list_remove(&cursor_button.link);
  wl_list_remove(&cursor_axis.link);
  wl_list_remove(&cursor_frame.link);
  wl_list_remove(&new_pointer_constraint.link);
  wl_list_remove(&new_client.link);
  wl_list_remove(&new_popup.link);
  wl_list_remove(&new_layer_surface.link);
  wl_list_remove(&new_decoration.link);
#ifdef XWAYLAND
  wl_list_remove(&xwayland_ready.link);
  wl_list_remove(&new_xwayland_surface.link);
#endif
}

int main(){
  init();
  run();
  quit();
}
