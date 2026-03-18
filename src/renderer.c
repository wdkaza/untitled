#include "renderer.h"
#include "pixman.h"
#include "render/egl.h"
#include "render/gles2.h"
#include "server.h"
#include "util/matrix.h"

#include <GLES2/gl2.h>
#include <assert.h>
#include <bits/time.h>
#include <wayland-server-protocol.h>
#include <wlr/backend.h>
#include <wlr/backend/libinput.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>

// massive thanks to https://github.com/jbuchermn/pywm aka newm renderer
// i would have not done it wihtout scraping half of the code from there
// its pretty much the same code but updated for wlroots 0.20 and WAY worse lol


// TODO damage support, and finishing quad shaders;

static const GLfloat verts[] = {
	1, 0, // top right
	0, 0, // top left
	1, 1, // bottom right
	0, 1, // bottom left
};

void mw_renderer_init(struct mw_renderer *renderer, struct Server *server){
  renderer->server = server;
  renderer->wlr_renderer = server->wlr_renderer;

  //wlr_renderer_init_wl_display(renderer->wlr_renderer, server->display);

  renderer->n_texture_shaders = 0;
  renderer->texture_shaders = NULL;
  if(wlr_renderer_is_gles2(renderer->wlr_renderer)){
    struct wlr_gles2_renderer *r = gles2_get_renderer(renderer->wlr_renderer);
    assert(wlr_egl_make_current(r->egl, NULL));
    mw_renderer_init_quad_shaders(renderer);
    mw_renderer_init_texture_shaders(renderer, 1);
    mw_renderer_add_texture_shaders(renderer, "default",
    texture_vertex_src,
    texture_fragment_rgba,
    texture_fragment_rgbx,
    texture_fragment_ext);
    wlr_egl_unset_current(r->egl);
  }
}


GLuint compile_shader(struct wlr_gles2_renderer *renderer, GLuint type, const GLchar *src){
  push_gles2_debug(renderer);

  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);

  GLint ok;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if(ok == GL_FALSE){
    glDeleteShader(shader);
    shader = 0;
  }
  
  pop_gles2_debug(renderer);
  return shader;
}

GLuint mw_renderer_link_program(struct mw_renderer *renderer, const GLchar *vert_src, const GLchar *frag_src){
  struct wlr_gles2_renderer *gles2_renderer = gles2_get_renderer(renderer->wlr_renderer);

  push_gles2_debug(gles2_renderer);

  GLuint vert = compile_shader(gles2_renderer, GL_VERTEX_SHADER, vert_src);
  if(!vert) return 0;
  GLuint frag = compile_shader(gles2_renderer, GL_FRAGMENT_SHADER, frag_src);
  if(!frag) return 0;

  GLuint program = glCreateProgram();
  glAttachShader(program, vert);
  glAttachShader(program, frag);
  glLinkProgram(program);
  glDetachShader(program, vert);
  glDetachShader(program, frag);
  glDeleteShader(vert);
  glDeleteShader(frag);

  GLint ok;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if(ok == GL_FALSE){
    glDeleteProgram(program);
    return 0;
  }

  pop_gles2_debug(gles2_renderer);
  return program;
}

void mw_renderer_init_texture_shaders(struct mw_renderer* renderer, int n_shaders){
  renderer->texture_shaders = calloc(n_shaders, sizeof(struct mw_renderer_texture_shaders));
  renderer->n_texture_shaders = n_shaders;
}

void mw_renderer_add_texture_shaders(struct mw_renderer* renderer, const char* name, const GLchar* vert_src, const GLchar* frag_src_rgba, const GLchar* frag_src_rgbx, const GLchar* frag_src_ext){
  struct wlr_gles2_renderer *gles2_renderer = gles2_get_renderer(renderer->wlr_renderer);

  int i = 0;
  for(; i < renderer->n_texture_shaders; i++){
    if(!renderer->texture_shaders[i].name){
      break;
    }
  }
  assert(i < renderer->n_texture_shaders);

  renderer->texture_shaders[i].name = strdup(name);
  mw_renderer_link_texture_shader(renderer, &renderer->texture_shaders[i].rgba, vert_src, frag_src_rgba);
  mw_renderer_link_texture_shader(renderer, &renderer->texture_shaders[i].rgbx, vert_src, frag_src_rgbx);

  if(gles2_renderer->exts.OES_egl_image_external){
    mw_renderer_link_texture_shader(renderer, &renderer->texture_shaders[i].ext, vert_src, frag_src_ext);
  }
}

void mw_renderer_link_texture_shader(struct mw_renderer *renderer, struct mw_renderer_texture_shader *shader, const GLchar *vert_src, const GLchar *frag_src){
  shader->shader = mw_renderer_link_program(renderer, vert_src, frag_src);
  assert(shader->shader);

  shader->proj = glGetUniformLocation(shader->shader, "proj");
  shader->tex = glGetUniformLocation(shader->shader, "tex");
  shader->alpha = glGetUniformLocation(shader->shader, "alpha");
  shader->offset_x = glGetUniformLocation(shader->shader, "offset_x");
  shader->offset_y = glGetUniformLocation(shader->shader, "offset_y");
  shader->scale_x = glGetUniformLocation(shader->shader, "scale_x");
  shader->scale_y = glGetUniformLocation(shader->shader, "scale_y");
  shader->width = glGetUniformLocation(shader->shader, "width");
  shader->height = glGetUniformLocation(shader->shader, "height");

  shader->pos_attrib = glGetAttribLocation(shader->shader, "pos");
  shader->tex_attrib = glGetAttribLocation(shader->shader, "texcoord");
}

void mw_renderer_init_quad_shaders(struct mw_renderer *renderer){
  renderer->quad_shader.shader = mw_renderer_link_program(renderer, quad_vertex_src, quad_fragment);

  renderer->quad_shader.tex = glGetUniformLocation(renderer->quad_shader.shader, "tex");
  renderer->quad_shader.pos_attrib = glGetAttribLocation(renderer->quad_shader.shader, "pos");
  renderer->quad_shader.tex_attrib = glGetAttribLocation(renderer->quad_shader.shader, "texcoord");
}

static bool mw_renderer_render_subtexture(struct mw_renderer *renderer,
                                   struct wlr_texture *wlr_texture,
                                   const struct wlr_fbox *box,
                                   float alpha,
                                   pixman_region32_t *damage,
                                   /*const struct wlr_box *src_box,*/
                                   const struct wlr_box *display_box
                                   /*float alpha        */
                                   /*float corner_radius*/){
  struct wlr_gles2_texture_attribs attribs;
  wlr_gles2_texture_get_attribs(wlr_texture, &attribs);

  struct mw_renderer_texture_shader *shader = NULL;

	switch (attribs.target) {
	case GL_TEXTURE_2D:
		if (attribs.has_alpha) {
			shader = &renderer->texture_shaders[0].rgba;
		} else {
			shader = &renderer->texture_shaders[0].rgbx;
		}
		break;
	case GL_TEXTURE_EXTERNAL_OES:
        wlr_log(WLR_ERROR, "Failed to render texture: "
            "GL_TEXTURE_EXTERNAL_OES not supported");
        return false;
	default:
		abort();
	}

  float matrix[9];
  float projection[9];

  matrix_projection(projection,
                    renderer->current->wlr_output->width,
                    renderer->current->wlr_output->height,
                    renderer->current->wlr_output->transform);
  
  wlr_matrix_project_box(matrix, display_box, WL_OUTPUT_TRANSFORM_NORMAL, projection);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(attribs.target, attribs.tex);

  glTexParameteri(attribs.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glUseProgram(shader->shader);
  glUniformMatrix3fv(shader->proj, 1, GL_TRUE, matrix);
	glUniform1i(shader->tex, 0);
	glUniform1f(shader->alpha, alpha);
	glUniform1f(shader->width, display_box->width);
	glUniform1f(shader->height, display_box->height);
	//glUniform1f(shader->cornerradius, corner_radius);

	const GLfloat x1 = box->x / wlr_texture->width;
	const GLfloat y1 = box->y / wlr_texture->height;
	const GLfloat x2 = (box->x + box->width) / wlr_texture->width;
	const GLfloat y2 = (box->y + box->height) / wlr_texture->height;
	const GLfloat texcoord[] = {
		x2, y1, // top right
		x1, y1, // top left
		x2, y2, // bottom right
		x1, y2, // bottom left
	};

	glVertexAttribPointer(shader->pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, verts);
	glVertexAttribPointer(shader->tex_attrib, 2, GL_FLOAT, GL_FALSE, 0, texcoord);

	glEnableVertexAttribArray(shader->pos_attrib);
	glEnableVertexAttribArray(shader->tex_attrib);

  /*
   * sometihgn wrong with it TODO : find
  if(damage != NULL){
    glEnable(GL_SCISSOR_TEST);
    int nrects;
    pixman_box32_t *rects = pixman_region32_rectangles(damage, &nrects);
    for(int i = 0; i < nrects; i++){
      glScissor(rects[i].x1,
                renderer->current->wlr_output->height - rects[i].y2,
                rects[i].x2 - rects[i].x1,
                rects[i].y2 - rects[i].y1);
      glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    }
    glDisable(GL_SCISSOR_TEST);
  }
  else{
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
  }
  */

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);


	glDisableVertexAttribArray(shader->pos_attrib);
	glDisableVertexAttribArray(shader->tex_attrib);

	glBindTexture(attribs.target, 0);
  return true;
}




void mw_renderer_render_texture_at(struct mw_renderer *renderer,
                                   pixman_region32_t *damage,
                                   struct wlr_surface* surface,
                                   struct wlr_texture *texture,
                                   struct wlr_box *box, double opacity
                                   ){
  struct wlr_fbox fbox;
  fbox.x = 0;
  fbox.y = 0;
  fbox.width = texture->width;
  fbox.height = texture->height;

  mw_renderer_render_subtexture(renderer,
                                texture, 
                                &fbox,
                                (float)opacity,
                                damage,
                                box);

  /* 
  float alpha = (float)opacity;
  wlr_render_pass_add_texture(renderer->pass, &(struct wlr_render_texture_options){
    .texture = texture,
    .src_box = fbox,
    .dst_box = *box,
    .alpha = &alpha,
    .clip = damage,
    .transform = WL_OUTPUT_TRANSFORM_NORMAL,
    .blend_mode = WLR_RENDER_BLEND_MODE_PREMULTIPLIED,
  });
  */
}


int mw_renderer_init_output(struct mw_renderer *renderer, struct Monitor *output){
  return wlr_output_init_render(output->wlr_output, renderer->server->wlr_allocator, renderer->wlr_renderer);
}

void mw_renderer_destroy(struct mw_renderer *renderer){
  wlr_renderer_destroy(renderer->wlr_renderer);
}

void mw_renderer_begin(struct mw_renderer *renderer, struct Monitor *output){
  renderer->current = output;
  wlr_output_state_init(&renderer->state);
  renderer->pass = wlr_output_begin_render_pass(output->wlr_output, &renderer->state, NULL);
}

void mw_renderer_end(struct mw_renderer *renderer, pixman_region32_t *damage, struct Monitor *output){
  if(renderer->pass){
    wlr_output_add_software_cursors_to_render_pass(output->wlr_output, renderer->pass, damage);
    wlr_render_pass_submit(renderer->pass);
  }
  wlr_output_commit_state(output->wlr_output, &renderer->state);
  wlr_output_state_finish(&renderer->state);
  renderer->pass = NULL;
  renderer->current = NULL;
}
