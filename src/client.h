// some functions ripped directly from dwl here
//
static inline int client_is_x11(struct Client *client){
#ifdef XWAYLAND
	return client->type == X11;
#endif
	return 0;
}

static inline struct wlr_surface *client_surface(struct Client *client){
#ifdef XWAYLAND
	if (client_is_x11(client))
		return client->surface.xwayland->surface;
#endif
	return client->surface.xdg->surface;
}

static inline uint32_t client_set_size(struct Client *client, uint32_t width, uint32_t height){
#ifdef XWAYLAND
	if (client_is_x11(client)) {
		wlr_xwayland_surface_configure(client->surface.xwayland,
				client->geom.x + client->bw, client->geom.y + client->bw, width, height);
		return 0;
	}
#endif
	if ((int32_t)width == client->surface.xdg->toplevel->current.width
			&& (int32_t)height == client->surface.xdg->toplevel->current.height)
		return 0;
	return wlr_xdg_toplevel_set_size(client->surface.xdg->toplevel, (int32_t)width, (int32_t)height);
}

static inline void client_get_geometry(struct Client *client, struct wlr_box *geom){
#ifdef XWAYLAND
	if (client_is_x11(client)) {
		geom->x = client->surface.xwayland->x;
		geom->y = client->surface.xwayland->y;
		geom->width = client->surface.xwayland->width;
		geom->height = client->surface.xwayland->height;
		return;
	}
#endif
	*geom = client->surface.xdg->geometry;
}

static inline void client_get_clip(struct Client *client, struct wlr_box *clip){
	*clip = (struct wlr_box){
		.x = 0,
		.y = 0,
		.width = client->geom.width - client->bw,
		.height = client->geom.height - client->bw,
	};

#ifdef XWAYLAND
	if (client_is_x11(client))
		return;
#endif

	clip->x = client->surface.xdg->geometry.x;
	clip->y = client->surface.xdg->geometry.y;
}

static inline void client_notify_enter(struct wlr_surface *s, struct wlr_keyboard *kb){
  if(kb){
    wlr_seat_keyboard_notify_enter(seat, s, kb->keycodes, kb->num_keycodes, &kb->modifiers);
  }
  else{
    wlr_seat_keyboard_notify_enter(seat, s, NULL, 0, NULL);
  }
}

static inline void client_set_fullscreen(struct Client *client, int fullscreen){
#ifdef XWAYLAND
	if (client_is_x11(client)) {
		wlr_xwayland_surface_set_fullscreen(client->surface.xwayland, fullscreen);
		return;
	}
#endif
	wlr_xdg_toplevel_set_fullscreen(client->surface.xdg->toplevel, fullscreen);
}


static inline int client_wants_fullscreen(struct Client *client){
#ifdef XWAYLAND
	if (client_is_x11(client))
		return client->surface.xwayland->fullscreen;
#endif
	return client->surface.xdg->toplevel->requested.fullscreen;
}

