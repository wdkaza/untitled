#pragma once

#include <GLES2/gl2.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <render/gles2.h>
#include <GLES3/gl32.h>

#include "monitor.h"

struct quad_shader{
  GLuint shader;
  GLint tex;
  GLint pos_attrib;
  GLint tex_attrib;
};

struct mw_renderer_texture_shader{
  GLuint shader;

  GLint proj;
  GLint tex;
  GLint alpha;
  GLint pos_attrib;
  GLint tex_attrib;

  GLint offset_x;
  GLint offset_y;
  GLint scale_x;
  GLint scale_y;
  GLint width;
  GLint height;
};

struct mw_renderer_texture_shaders{
  const char *name;

  struct mw_renderer_texture_shader rgba;
  struct mw_renderer_texture_shader rgbx;
  struct mw_renderer_texture_shader ext;
};

struct mw_renderer{
  struct Server *server;
  struct wlr_renderer *wlr_renderer;
  struct Monitor *current;
  struct quad_shader quad_shader;

  int n_texture_shaders;
  struct mw_renderer_texture_shaders *texture_shaders;
  //renderer mode;
};


GLuint compile_shader(struct wlr_gles2_renderer *renderer, GLuint type, const GLchar *src);
GLuint mw_renderer_link_program(struct mw_renderer *renderer, const GLchar *vert_src, const GLchar *frag_src);
void mw_renderer_init_texture_shaders(struct mw_renderer* renderer, int n_shaders);
void mw_renderer_add_texture_shaders(struct mw_renderer* renderer, const char* name,
        const GLchar* vert_src,
        const GLchar* frag_src_rgba,
        const GLchar* frag_src_rgbx,
        const GLchar* frag_src_ext);
void mw_renderer_link_texture_shader(struct mw_renderer *renderer, struct mw_renderer_texture_shader *shader, const GLchar *vert_src, const GLchar *frag_src);
void mw_renderer_init_quad_shaders(struct mw_renderer *renderer);
/*
void wm_renderer_init_primitive_shaders(struct mw_renderer* renderer, int n_shaders);
void wm_renderer_add_primitive_shader(struct mw_renderer* renderer, const char* name, const GLchar* vert_src, const GLchar* frag_src, int n_params_int, int n_params_float);
*/

void wm_renderer_render_texture_at(struct mw_renderer *renderer,
                                   pixman_region32_t *damage,
                                   struct wlr_surface* surface,
                                   struct wlr_texture *texture,
                                   struct wlr_box *box, double opacity,
                                   struct wlr_box *mask,
                                   double corner_radius, double lock_perc);

void mw_renderer_init(struct mw_renderer *renderer, struct Server *server);
int mw_renderer_init_output(struct mw_renderer *renderer, struct Monitor *output);
void mw_renderer_destroy(struct mw_renderer *renderer);
