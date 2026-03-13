#pragma once

#include <GLES2/gl2.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <util/matrix.h>
#include <render/gles2.h>
#include <GLES3/gl32.h>

#include "monitor.h"

static const GLchar quad_vertex_src[] = "";

static const GLchar quad_fragment[] = "";

static const GLchar texture_vertex_src[] =
"uniform mat3 proj;\n"
"attribute vec2 pos;\n"
"attribute vec2 texcoord;\n"
"varying vec2 v_texcoord;\n"
"void main() {\n"
"  gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);\n"
"  gl_Position.y = -gl_Position.y;\n"
"  v_texcoord = texcoord;\n"
"}\n";

static const GLchar texture_fragment_rgba[] =
"precision mediump float;\n"
"uniform sampler2D tex;\n"
"uniform float alpha;\n"
"uniform float width;\n"
"uniform float height;\n"
"varying vec2 v_texcoord;\n"
"void main() {\n"
"  vec4 color = texture2D(tex, v_texcoord);\n"
"  color.rgb = mix(color.rgb, vec3(0.0, 1.0, 0.0), 0.5);\n"
"  gl_FragColor = color * alpha;\n"
"}\n";

static const GLchar texture_fragment_rgbx[] =
"precision mediump float;\n"
"uniform sampler2D tex;\n"
"uniform float alpha;\n"
"uniform float width;\n"
"uniform float height;\n"
"varying vec2 v_texcoord;\n"
"void main() {\n"
"    vec4 color = texture2D(tex, v_texcoord);\n"
"    color.a = 1.0;\n"
"    color.rgb = mix(color.rgb, vec3(0.0, 0.0, 1.0), 0.5);\n"
"    gl_FragColor = color * alpha;\n"
"}\n";

static const GLchar texture_fragment_ext[] =
"#extension GL_OES_EGL_image_external : require\n"
"precision mediump float;\n"
"uniform samplerExternalOES tex;\n"
"uniform float alpha;\n"
"uniform float width;\n"
"uniform float height;\n"
"varying vec2 v_texcoord;\n"
"void main() {\n"
"    vec4 color = texture2D(tex, v_texcoord);\n"
"    color.rgb = mix(color.rgb, vec3(1.0, 0.0, 0.0), 0.5);\n"
"    gl_FragColor = color * alpha;\n"
"}\n";

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
  struct Monitor *current;// current means current monitor, a bit confusing
  struct quad_shader quad_shader;

  struct wlr_render_pass *pass;
  struct wlr_output_state state;

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

void mw_renderer_render_texture_at(struct mw_renderer *renderer,
                                   pixman_region32_t *damage,
                                   struct wlr_surface* surface,
                                   struct wlr_texture *texture,
                                   struct wlr_box *box, double opacity
                                   );

static bool mw_renderer_render_subtexture(struct mw_renderer *renderer,
                                   struct wlr_texture *wlr_texture,
                                   const struct wlr_fbox *box,
                                   float alpha,
                                   pixman_region32_t *damage,
                                   /*const struct wlr_box *src_box,*/
                                   const struct wlr_box *display_box
                                   /*float alpha        */
                                   /*float corner_radius*/);


void mw_renderer_init(struct mw_renderer *renderer, struct Server *server);
int mw_renderer_init_output(struct mw_renderer *renderer, struct Monitor *output);
void mw_renderer_destroy(struct mw_renderer *renderer);
void mw_renderer_begin(struct mw_renderer *renderer, struct Monitor *output);
void mw_renderer_end(struct mw_renderer *renderer, pixman_region32_t *damage, struct Monitor *output);
