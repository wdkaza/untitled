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

static inline int toplevel_from_wlr_surface(struct wlr_surface *s, struct Client **pc, struct LayerSurface **pl){
	struct wlr_xdg_surface *xdg_surface, *tmp_xdg_surface;
	struct wlr_surface *root_surface;
	struct wlr_layer_surface_v1 *layer_surface;
	struct Client *c = NULL;
	struct LayerSurface *l = NULL;
	int type = -1;
#ifdef XWAYLAND
	struct wlr_xwayland_surface *xsurface;
#endif

	if (!s)
		return -1;
	root_surface = wlr_surface_get_root_surface(s);

#ifdef XWAYLAND
	if ((xsurface = wlr_xwayland_surface_try_from_wlr_surface(root_surface))) {
		c = xsurface->data;
		type = c->type;
		goto end;
	}
#endif

	if ((layer_surface = wlr_layer_surface_v1_try_from_wlr_surface(root_surface))) {
		l = layer_surface->data;
		type = LayerShell;
		goto end;
	}

	xdg_surface = wlr_xdg_surface_try_from_wlr_surface(root_surface);
	while (xdg_surface) {
		tmp_xdg_surface = NULL;
		switch (xdg_surface->role) {
		case WLR_XDG_SURFACE_ROLE_POPUP:
			if (!xdg_surface->popup || !xdg_surface->popup->parent)
				return -1;

			tmp_xdg_surface = wlr_xdg_surface_try_from_wlr_surface(xdg_surface->popup->parent);

			if (!tmp_xdg_surface)
				return toplevel_from_wlr_surface(xdg_surface->popup->parent, pc, pl);

			xdg_surface = tmp_xdg_surface;
			break;
		case WLR_XDG_SURFACE_ROLE_TOPLEVEL:
			c = xdg_surface->data;
			type = c->type;
			goto end;
		case WLR_XDG_SURFACE_ROLE_NONE:
			return -1;
		}
	}

end:
	if (pl)
		*pl = l;
	if (pc)
		*pc = c;
	return type;
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

