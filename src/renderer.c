#include "renderer.h"
#include "monitor.h"
#include "render/gles2.h"
#include "util/matrix.h"
#include "wlr/render/swapchain.h"
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <wayland-server-protocol.h>

void srMatrixProjection(float mat[9], int width, int height, enum wl_output_transform transform){
  memset(mat, 0, sizeof(float) * 9);
  float x = 2.0f / (float)width;
  float y = 2.0f / (float)height;

  switch(wlr_output_transform_invert(transform)){
    default:
      return;
    case WL_OUTPUT_TRANSFORM_NORMAL:
      mat[0] = x;
      mat[4] = y;
      break;
    case WL_OUTPUT_TRANSFORM_90:
      mat[1] = -y;
      mat[3] = x;
      break;
    case WL_OUTPUT_TRANSFORM_180:
      mat[0] = -x;
      mat[4] = -y;
      break;
    case WL_OUTPUT_TRANSFORM_270:
      mat[1] = y;
      mat[3] = -x;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED:
      mat[0] = -x;
      mat[4] = y;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_180:
      mat[0] = x;
      mat[4] = -y;
      break;
    case WL_OUTPUT_TRANSFORM_FLIPPED_270:
      mat[1] = -y;
      mat[3] = -x;
      break;
  }
  mat[2] = -1.0f;
  mat[5] = -1.0f;
  mat[8] = 1.0f;
}

void srMatrixIdentity(float mat[9]){
  float identity[9] = {
    1,0,0,
    0,1,0,
    0,0,1,
  };
  memcpy(mat, identity, sizeof(identity));
}

void srMatrixMultiply(float mat[9], float a[9], float b[9]){
  float output[9];
  output[0] = a[0]*b[0] + a[1]*b[3] + a[2]*b[6];
  output[1] = a[0]*b[1] + a[1]*b[4] + a[2]*b[7];
  output[2] = a[0]*b[2] + a[1]*b[5] + a[2]*b[8];

  output[3] = a[3]*b[0] + a[4]*b[3] + a[5]*b[6];
  output[4] = a[3]*b[1] + a[4]*b[4] + a[5]*b[7];
  output[5] = a[3]*b[2] + a[4]*b[5] + a[5]*b[8];

  output[6] = a[6]*b[0] + a[7]*b[3] + a[8]*b[6];
  output[7] = a[6]*b[1] + a[7]*b[4] + a[8]*b[7];
  output[8] = a[6]*b[2] + a[7]*b[5] + a[8]*b[8];
  memcpy(mat, output, sizeof(output));
}

void srMatrixTranslate(float mat[9], float x, float y){
  float translate[9] = {
    1,0,x,
    0,1,y,
    0,0,1,
  };
  srMatrixMultiply(mat, mat, translate);
}

void srMatrixScale(float mat[9], float x, float y){
  float scale[9] = {
    x,0,0,
    0,y,0,
    0,0,1,
  };
  srMatrixMultiply(mat, mat, scale);
}

void srMatrixProjectBox(float mat[9], struct wlr_box *box, float projection[9]){
  float matrix[9];
  srMatrixIdentity(matrix);
  srMatrixTranslate(matrix, box->x, box->y);
  srMatrixScale(matrix, box->width, box->height);
  srMatrixMultiply(mat, projection, matrix);
}

GLuint srCompileShader(GLuint type, const GLchar *src){
  char info_log[1024]; int result;
  GLuint shader = glCreateShader(type);

  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);

  glGetShaderiv(shader, GL_COMPILE_STATUS, &result);
  if(!result){
    glGetShaderInfoLog(shader, 1024, NULL, info_log);
    fprintf(stderr, "shader failed to compile!, log: %s\n", info_log);
    glDeleteShader(shader);
    return 0;
  }

  return shader;
}

GLuint srLinkShader(const GLchar *vert_src, const GLchar *frag_src){
  GLuint vert = srCompileShader(GL_VERTEX_SHADER, vert_src);
  GLuint frag = srCompileShader(GL_FRAGMENT_SHADER, frag_src);
  if(!vert || !frag){
    fprintf(stderr, "Failed to compile shaders!\n");
    return 0;
  }

  GLuint shader = glCreateProgram();
  glAttachShader(shader, vert);
  glAttachShader(shader, frag);

  glLinkProgram(shader);

  glDetachShader(shader, vert);
  glDetachShader(shader, frag);
  glDeleteShader(vert);
  glDeleteShader(frag);

  int result;
  glGetProgramiv(shader, GL_LINK_STATUS, &result);
  if(!result){
    glDeleteProgram(shader);
    fprintf(stderr, "Failed to link shader program!\n"); 
    return 0;
  }

  return shader;
}

void srInitTextureShaders(struct srRenderer *renderer, int count){
  renderer->texshader.shaders = calloc(count, sizeof(struct srTextureShaders));
  renderer->texshader.count = count;
}

void srLinkTextureShader(struct srRenderer *renderer, struct srTextureShader *shader, const GLchar *vert_src, const GLchar *frag_src){
  shader->shader = srLinkShader(vert_src, frag_src);
  if(!shader->shader) return;

  shader->proj = glGetUniformLocation(shader->shader, "proj");
  shader->tex = glGetUniformLocation(shader->shader, "tex");
  shader->alpha = glGetUniformLocation(shader->shader, "alpha");
  shader->offset_x = glGetUniformLocation(shader->shader, "offset_x");
  shader->offset_y = glGetUniformLocation(shader->shader, "offset_y");
  shader->scale_x = glGetUniformLocation(shader->shader, "scale_x");
  shader->scale_y = glGetUniformLocation(shader->shader, "scale_y");
  shader->width = glGetUniformLocation(shader->shader, "width");
  shader->height = glGetUniformLocation(shader->shader, "height");
  shader->corner_radius = glGetUniformLocation(shader->shader, "corner_radius");
  shader->pos_attrib = glGetAttribLocation(shader->shader, "pos");
  shader->tex_attrib = glGetAttribLocation(shader->shader, "texcoord");
}

void srAddTextureShader(struct srRenderer *renderer, char *name, const GLchar *vert_src, const GLchar *frag_src_rgba, const GLchar *frag_src_rgbx, const GLchar *frag_src_ext){
  uint32_t i = 0;
  for(; i < renderer->texshader.count; i++){
    if(!renderer->texshader.shaders[i].name){
      break;
    }
  }
  assert(i < renderer->texshader.count);

  renderer->texshader.shaders[i].name = strdup(name);
  srLinkTextureShader(renderer, &renderer->texshader.shaders[i].rgba, vert_src, frag_src_rgba);
  srLinkTextureShader(renderer, &renderer->texshader.shaders[i].rgbx, vert_src, frag_src_rgbx);
  srLinkTextureShader(renderer, &renderer->texshader.shaders[i].ext, vert_src, frag_src_ext);
}
void srInit(struct srRenderer *renderer, struct Server *server){
  renderer->info.server = server;
  renderer->wlr.renderer = server->wlr_renderer;
  assert(renderer->wlr.renderer);

  renderer->texshader.count = 0;
  renderer->texshader.shaders = NULL;

  renderer->info.monitor = NULL;
  if(wlr_renderer_is_gles2(renderer->wlr.renderer)){
    struct wlr_egl *egl = wlr_gles2_renderer_get_egl(renderer->wlr.renderer);
    assert(wlr_egl_make_current(egl, NULL));
    srInitTextureShaders(renderer, 1);
    srAddTextureShader(renderer, "default",
                       texture_vertex_src,
                       texture_fragment_rgba,
                       texture_fragment_rgbx,
                       texture_fragment_ext);
    renderer->info.shaders_compiled = true;
    wlr_egl_unset_current(egl);
  }
  else{
    wlr_log(WLR_INFO, "Not using GLES2 - custom renderer disabled");
  }
}

void srRenderTextureAt(struct srRenderer *renderer, struct wlr_surface *surface, struct wlr_texture *texture, struct wlr_box *display_box, float alpha, double opacity, float corner_radius){
  struct wlr_fbox fbox = {
    .x = 0, .y = 0,
    .width = texture->width,
    .height = texture->height,
  };
  
  struct wlr_gles2_texture_attribs attribs;
  wlr_gles2_texture_get_attribs(texture, &attribs);

  struct srTextureShader *shader = NULL;
  switch(attribs.target){
    case GL_TEXTURE_2D:
      if(attribs.has_alpha){
        shader = &renderer->texshader.shaders[0].rgba;
      }
      else{
        shader = &renderer->texshader.shaders[0].rgbx;
      }
      break;
    case GL_TEXTURE_EXTERNAL_OES:
      shader = &renderer->texshader.shaders[0].ext;
      break;
    default:
      abort();
    }
  
  float matrix[9];
  float projection[9];

  srMatrixProjection(projection, renderer->info.monitor->wlr_output->width, renderer->info.monitor->wlr_output->height, renderer->info.monitor->wlr_output->transform);
  srMatrixProjectBox(matrix, display_box, projection);

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(attribs.target, attribs.tex);
  glTexParameteri(attribs.target, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

  glUseProgram(shader->shader);
  glUniformMatrix3fv(shader->proj, 1, GL_TRUE, matrix);
  glUniform1i(shader->tex, 0);
  glUniform1f(shader->alpha, alpha);
  glUniform1f(shader->width, display_box->width);
  glUniform1f(shader->height, display_box->height);
  glUniform1f(shader->offset_x, fbox.x / (float)texture->width);
  glUniform1f(shader->offset_y, fbox.y / (float)texture->height);
  glUniform1f(shader->scale_x, (float)display_box->width / (fbox.width / texture->width));
  glUniform1f(shader->scale_y, (float)display_box->height / (fbox.height / texture->height));
  glUniform1f(shader->corner_radius, corner_radius);

  const GLfloat x1 = (float)fbox.x / texture->width;
  const GLfloat y1 = (float)fbox.y / texture->height;
  const GLfloat x2 = (float)(fbox.x + fbox.width) / texture->width;
  const GLfloat y2 = (float)(fbox.y + fbox.height) / texture->height;
  const GLfloat texcoord[] = {
    x2, y1,
    x1, y1,
    x2, y2,
    x1, y2,
  };

  glVertexAttribPointer(shader->pos_attrib, 2, GL_FLOAT, GL_FALSE, 0, verts);
  glVertexAttribPointer(shader->tex_attrib, 2, GL_FLOAT, GL_FALSE, 0, texcoord);
  glEnableVertexAttribArray(shader->pos_attrib);
  glEnableVertexAttribArray(shader->tex_attrib);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glDisableVertexAttribArray(shader->pos_attrib);
  glDisableVertexAttribArray(shader->tex_attrib);
  glBindTexture(attribs.target, 0);
}

void srBegin(struct srRenderer *renderer, struct Monitor *output){
  struct wlr_buffer *swapbuf;
  renderer->info.monitor = output;
  wlr_output_state_init(&renderer->wlr.state);
  renderer->wlr.pass = NULL;

  wlr_output_configure_primary_swapchain(output->wlr_output, &renderer->wlr.state, &output->wlr_output->swapchain);
  swapbuf = wlr_swapchain_acquire(output->wlr_output->swapchain); 

  pixman_region32_init(&renderer->wlr.damage);
  wlr_damage_ring_rotate_buffer(&output->dring, swapbuf, &renderer->wlr.damage);

  renderer->wlr.pass = wlr_renderer_begin_buffer_pass(renderer->wlr.renderer, swapbuf, NULL);

  wlr_output_state_set_buffer(&renderer->wlr.state, swapbuf);
  wlr_buffer_unlock(swapbuf);


  if(!renderer->info.shaders_compiled){
    // will fire in the future if shaders would need to be recompiled
    return;
  }
}

void srEnd(struct srRenderer *renderer, struct Monitor *output){
  if(renderer->wlr.pass){
    wlr_output_add_software_cursors_to_render_pass(output->wlr_output, renderer->wlr.pass, NULL);
    wlr_render_pass_submit(renderer->wlr.pass);
    wlr_output_state_set_damage(&renderer->wlr.state, &renderer->wlr.damage);
    pixman_region32_fini(&renderer->wlr.damage);
  }

  if(!wlr_output_commit_state(output->wlr_output, &renderer->wlr.state)){
    wlr_damage_ring_add_whole(&output->dring);
    wlr_output_schedule_frame(output->wlr_output);
  }
  wlr_output_state_finish(&renderer->wlr.state);
  renderer->wlr.pass = NULL;
  renderer->info.monitor = NULL;
}
