#pragma once
#include "server.h"
#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <wayland-server-protocol.h>
#include <wlr/backend.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/gles2.h>
#include <wlr/render/swapchain.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_damage_ring.h>
#include <wlr/util/transform.h>
#include <pixman.h>

static const GLchar texture_vertex_src[] =
"uniform mat3 proj;\n"
"attribute vec2 pos;\n"
"attribute vec2 texcoord;\n"
"varying vec2 v_texcoord;\n"
"void main() {\n"
"  gl_Position = vec4(proj * vec3(pos, 1.0), 1.0);\n"
"  v_texcoord = texcoord;\n"
"}\n";

static const GLchar texture_fragment_rgba[] =
"precision mediump float;\n"
"varying vec2 v_texcoord;\n"
"uniform sampler2D tex;\n"
"uniform float alpha;\n"
"uniform float width;\n"
"uniform float height;\n"
"uniform float offset_x;\n"
"uniform float offset_y;\n"
"uniform float scale_x;\n"
"uniform float scale_y;\n"
"uniform float corner_radius;\n"
"void main() {\n"
"  float x = (v_texcoord.x - offset_x)*scale_x;\n"
"  float y = (v_texcoord.y - offset_y)*scale_y;\n"
"  if(x < corner_radius && y < corner_radius){\n"
"    if(length(vec2(x,y) - vec2(corner_radius, corner_radius)) > corner_radius) discard;\n"
"  }\n"
"  if(x > width - corner_radius && y < corner_radius){\n"
"    if(length(vec2(x,y) - vec2(width - corner_radius, corner_radius)) > corner_radius) discard;\n"
"  }\n"
"  if(x < corner_radius && y > height - corner_radius){\n"
"    if(length(vec2(x,y) - vec2(corner_radius, height - corner_radius)) > corner_radius) discard;\n"
"  }\n"
"  if(x > width - corner_radius && y > height - corner_radius){\n"
"    if(length(vec2(x,y) - vec2(width - corner_radius, height - corner_radius)) > corner_radius) discard;\n"
"  }\n"
"  gl_FragColor = texture2D(tex, v_texcoord) * alpha;\n"
"}\n";

static const GLchar texture_fragment_rgbx[] =
"precision mediump float;\n"
"varying vec2 v_texcoord;\n"
"uniform sampler2D tex;\n"
"uniform float alpha;\n"
"uniform float width;\n"
"uniform float height;\n"
"uniform float offset_x;\n"
"uniform float offset_y;\n"
"uniform float scale_x;\n"
"uniform float scale_y;\n"
"uniform float corner_radius;\n"
"void main() {\n"
"  float x = (v_texcoord.x - offset_x)*scale_x;\n"
"  float y = (v_texcoord.y - offset_y)*scale_y;\n"
"  if(x < corner_radius && y < corner_radius){\n"
"    if(length(vec2(x,y) - vec2(corner_radius, corner_radius)) > corner_radius) discard;\n"
"  }\n"
"  if(x > width - corner_radius && y < corner_radius){\n"
"    if(length(vec2(x,y) - vec2(width - corner_radius, corner_radius)) > corner_radius) discard;\n"
"  }\n"
"  if(x < corner_radius && y > height - corner_radius){\n"
"    if(length(vec2(x,y) - vec2(corner_radius, height - corner_radius)) > corner_radius) discard;\n"
"  }\n"
"  if(x > width - corner_radius && y > height - corner_radius){\n"
"    if(length(vec2(x,y) - vec2(width - corner_radius, height - corner_radius)) > corner_radius) discard;\n"
"  }\n"
"  gl_FragColor = vec4(texture2D(tex, v_texcoord).rgb, 1.0) * alpha;\n"
"}\n";

static const GLchar texture_fragment_ext[] =
"#extension GL_OES_EGL_image_external : require\n"
"precision mediump float;\n"
"varying vec2 v_texcoord;\n"
"uniform samplerExternalOES texture0;\n"
"uniform float alpha;\n"
"uniform float width;\n"
"uniform float height;\n"
"uniform float offset_x;\n"
"uniform float offset_y;\n"
"uniform float scale_x;\n"
"uniform float scale_y;\n"
"uniform float corner_radius;\n"
"void main() {\n"
"  float x = (v_texcoord.x - offset_x)*scale_x;\n"
"  float y = (v_texcoord.y - offset_y)*scale_y;\n"
"  if(x < corner_radius && y < corner_radius){\n"
"    if(length(vec2(x,y) - vec2(corner_radius, corner_radius)) > corner_radius) discard;\n"
"  }\n"
"  if(x > width - corner_radius && y < corner_radius){\n"
"    if(length(vec2(x,y) - vec2(width - corner_radius, corner_radius)) > corner_radius) discard;\n"
"  }\n"
"  if(x < corner_radius && y > height - corner_radius){\n"
"    if(length(vec2(x,y) - vec2(corner_radius, height - corner_radius)) > corner_radius) discard;\n"
"  }\n"
"  if(x > width - corner_radius && y > height - corner_radius){\n"
"    if(length(vec2(x,y) - vec2(width - corner_radius, height - corner_radius)) > corner_radius) discard;\n"
"  }\n"
"  gl_FragColor = texture2D(texture0, v_texcoord) * alpha;\n"
"}\n";


struct srTextureShader{
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
  GLint corner_radius;
};

struct srTextureShaders{
  char *name;
  struct srTextureShader rgba;
  struct srTextureShader rgbx;
  struct srTextureShader ext;
};

struct srRenderer {
  struct srShader{
    uint32_t count;
    struct srTextureShaders *shaders;
  } texshader;
  struct srWlrInfo{
    struct wlr_renderer *renderer;
    struct wlr_render_pass *pass;
    struct wlr_output_state state;
    pixman_region32_t damage;
  } wlr;
  struct srInfo{
    struct Server *server;
    struct Monitor* monitor;
    uint32_t shaders_compiled;
  } info;
};

static const GLfloat verts[] = {
  1,0,
  0,0,
  1,1,
  0,1,
};

void srMatrixProjection(float mat[9], int width, int height, enum wl_output_transform transform);
void srMatrixIdentity(float mat[9]);
void srMatrixMultiply(float mat[9], float a[9], float b[9]);
void srMatrixTranslate(float mat[9], float x, float y);
void srMatrixScale(float mat[9], float x, float y);
void srMatrixProjectBox(float mat[9], struct wlr_box *box, float projection[9]);

GLuint srCompileShader(GLuint type, const GLchar *src);
GLuint srLinkShader(const GLchar *vert_src, const GLchar *frag_src);

void srInitTextureShaders(struct srRenderer *renderer, int count);
void srLinkTextureShader(struct srRenderer *renderer, struct srTextureShader *shader, const GLchar *vert_src, const GLchar *frag_src);
void srAddTextureShader(struct srRenderer *renderer, char *name, const GLchar *vert_src, const GLchar *frag_src_rgba, const GLchar *frag_src_rgbx, const GLchar *frag_src_ext);
void srRenderTextureAt(struct srRenderer *renderer, struct wlr_surface *surface, struct wlr_texture *texture, struct wlr_box *display_box, float alpha, double opacity, float corner_radius);

void srInit(struct srRenderer *renderer, struct Server *server);
void srBegin(struct srRenderer *renderer, struct Monitor *output);
void srEnd(struct srRenderer *renderer, struct Monitor *output);
