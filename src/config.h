#pragma once
#include <libinput.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
#include <wayland-server-protocol.h>
#include <wlr/types/wlr_keyboard.h>
#include <xkbcommon/xkbcommon.h>

typedef struct{
  uint32_t mod;
  xkb_keysym_t keysym;
  void (*func)(const Arg *);
  Arg arg;
} KeyBinding;

typedef struct{
 uint32_t mod;
 uint32_t button;
 void (*func)(const Arg *);
 Arg arg;
} MouseBinding;

typedef struct{
  char *name;
  float scale;
  int32_t x;
  int32_t y;
  uint32_t transform;
} MonitorRule;

typedef struct{
  //keyboard;
  uint32_t repeat_rate;
  uint32_t repeat_delay;
  uint32_t numlockon;
  //mouse;?? dont know what to name this
  uint32_t scroll_method;
  uint32_t click_method;
  uint32_t send_events_mode;
  uint32_t accel_profile;
  double   accel_speed;
  uint32_t button_map;
  // trackpad
  uint32_t tap_to_click;
  uint32_t tap_and_drag;
  uint32_t drag_lock;
  uint32_t natural_scrolling;
  uint32_t disable_while_typing;
  uint32_t left_handed;
  uint32_t middle_button_emulation;
  //
  KeyBinding *key_bindings; 
  uint32_t key_bind_count;
  MouseBinding *mouse_bindings;
  uint32_t mouse_bind_count;
  MonitorRule *mon_rules;
  uint32_t mon_rule_count;
  char **exec;
  uint32_t exec_count;
  char **exec_once;
  uint32_t exec_once_count;
  // window eye candy
  uint32_t border_size;
  uint32_t border_radius;
  float focuscolor[4];
  float unfocusedcolor[4];
  float fullscreencolor[4];
} Config;

typedef void (*FuncType)(const Arg *);
Config config;

void cfgtrim(char *str){
  char *start = str;
  char *end = str + strlen(str) - 1;

  while(isspace(*start)){
    start++;
  }

  while(end > start && isspace(*end)){
    end--;
  }

  *(end + 1) = '\0';

  if(start != str){
    memmove(str, start, end - start + 2);
  }
}

void cfghextorgba(float *color, uint32_t hex){
  color[0] = ((hex >> 24) & 0xFF) / 255.0f;
  color[1] = ((hex >> 16) & 0xFF) / 255.0f;
  color[2] = ((hex >> 8) & 0xFF) / 255.0f;
  color[3] = (hex & 0xFF) / 255.0f;
}

uint32_t cfgparsecolor(char *hex_str){
  char *end;
  uint32_t hex = strtoul(hex_str, &end, 16);
  if(*end != '\0'){
    return -1;
  }
  return hex;
}

uint32_t cfgparsebutton(char *button_str){
  uint32_t button = 0;
  char lower_str[64];
  int i = 0;

  for(i = 0; button_str[i] && i < sizeof(lower_str) - 1; i++){
    lower_str[i] = tolower(button_str[i]);
  }
  lower_str[i] = '\0';

  if(strcmp(lower_str, "btn_left") == 0){
    return BTN_LEFT;
  }
  else if(strcmp(lower_str, "btn_right") == 0){
    return BTN_RIGHT;
  }
  else if(strcmp(lower_str, "btn_middle") == 0){
    return BTN_MIDDLE;
  }
  else{
    return 0;
  }
}

uint32_t cfgparsemod(char *mod_str){
  if(!mod_str){
    return UINT32_MAX;
  }
  uint32_t mod = 0;
  char input_copy[256];
  char *token;
  char *saveptr = NULL;

  strncpy(input_copy, mod_str, sizeof(input_copy) - 1);
  input_copy[sizeof(input_copy) - 1] = '\0';
  for(char *p = input_copy; *p; p++){
    *p = tolower(*p);
  }


  token = strtok_r(input_copy, "+", &saveptr);
  while(token != NULL){
    cfgtrim(token);

    if(strstr(token, "super") || strstr(token, "logo") || strstr(token, "win")){
      mod |= WLR_MODIFIER_LOGO;
    }
    if(strcmp(token, "ctrl") == 0){
      mod |= WLR_MODIFIER_CTRL;
    }
    if(strcmp(token, "shift") == 0){
      mod |= WLR_MODIFIER_SHIFT;
    }
    if(strcmp(token, "alt") == 0){
      mod |= WLR_MODIFIER_ALT;
    }
    else{
      fprintf(stderr, "Failed to parse modkey: %s\n", mod_str);
    }
    token = strtok_r(NULL, "+", &saveptr);
  }
  return mod;
}

uint32_t cfgparsetransform(const char *str){
  if(strcmp(str, "WL_OUTPUT_TRANSFORM_NORMAL") == 0){
    return WL_OUTPUT_TRANSFORM_NORMAL;
  }
  if(strcmp(str, "WL_OUTPUT_TRANSFORM_90") == 0){
    return WL_OUTPUT_TRANSFORM_90;
  }
  if(strcmp(str, "WL_OUTPUT_TRANSFORM_180") == 0){
    return WL_OUTPUT_TRANSFORM_180;
  }
  if(strcmp(str, "WL_OUTPUT_TRANSFORM_270") == 0){
    return WL_OUTPUT_TRANSFORM_270;
  }
  if(strcmp(str, "WL_OUTPUT_TRANSFORM_FLIPPED") == 0){
    return WL_OUTPUT_TRANSFORM_FLIPPED;
  }
  if(strcmp(str, "WL_OUTPUT_TRANSFORM_FLIPPED_90") == 0){
    return WL_OUTPUT_TRANSFORM_FLIPPED_90;
  }
  if(strcmp(str, "WL_OUTPUT_TRANSFORM_FLIPPED_180") == 0){
    return WL_OUTPUT_TRANSFORM_FLIPPED_180;
  }
  if(strcmp(str, "WL_OUTPUT_TRANSFORM_FLIPPED_270") == 0){
    return WL_OUTPUT_TRANSFORM_FLIPPED_270;
  }
  fprintf(stderr, "Failed to parse monrule transform: %s\n", str);
  return WL_OUTPUT_TRANSFORM_NORMAL;
}

int cfgparsedirection(const char *str){
  char lower_str[64];
  int i = 0;
  for(i = 0; str[i] && i < sizeof(lower_str) - 1; i++){
    lower_str[i] = tolower(str[i]);
  }
  lower_str[i] = '\0';

  if(strcmp(lower_str, "up") == 0){
    return UP;
  }
  else if(strcmp(lower_str, "down") == 0){
    return DOWN;
  }
  else if(strcmp(lower_str, "left") == 0){
    return LEFT;
  }
  else if(strcmp(lower_str, "right") == 0){
    return RIGHT;
  }
  else{
    return -1;
  }
}

xkb_keysym_t cfgparsekeysym(char *keysym_str){
  return xkb_keysym_from_name(keysym_str, XKB_KEYSYM_NO_FLAGS);
}

uint32_t cfgparsemouseaction(char *arg_value){
  char lower_str[64];
  int i = 0;
  for(i = 0; arg_value[i] && i < sizeof(lower_str) - 1; i++){
    lower_str[i] = tolower(arg_value[i]);
  }
  lower_str[i] = '\0';

  if(strcmp(lower_str, "cursormove") == 0){
    return CursorMove;
  }
  else if(strcmp(lower_str, "cursorresize") == 0){
    return CursorResize;
  }
  else if(strcmp(lower_str, "cursorpassthrough") == 0){
    return CursorPassthrough;
  }
  fprintf(stderr, "Failed to parse mouseaction: %s\n", arg_value);
  return 0;
}

FuncType cfgparsefunc(char *func_name, Arg *arg, char *arg_value){
  FuncType function = NULL;
  (*arg).v = NULL;

  if(strcmp(func_name, "spawn") == 0){
    function = spawn;
    char **argv = malloc(2 * sizeof(char *));
    argv[0] = strdup(arg_value);
    argv[1] = NULL;
    (*arg).v = argv;
  }
  else if(strcmp(func_name, "exitwm") == 0){
    function = exitwm;
  }
  else if(strcmp(func_name, "focusdir") == 0){
    function = focusdir;
    (*arg).i = cfgparsedirection(arg_value);
  }
  else if(strcmp(func_name, "togglefullscreen") == 0){
    function = togglefullscreen;
  }
  else if(strcmp(func_name, "kill") == 0){
    function = killclient;
  }
  else if(strcmp(func_name, "moveresize") == 0){
    function = moveresize;
    (*arg).ui = cfgparsemouseaction(arg_value);
  }
  return function;
}

void cfgparseoption(Config *config, char *key, char *value){
  if(strcmp(key, "repeat_rate") == 0){
    config->repeat_rate = atoi(value);
  }
  else if(strcmp(key, "repeat_delay") == 0){
    config->repeat_delay = atoi(value);
  }
  else if(strcmp(key, "border_radius") == 0){
    config->border_radius = atoi(value);
  }
  else if(strcmp(key, "tap_to_click") == 0){
    config->tap_to_click = atoi(value);
  }
  else if(strcmp(key, "tap_and_drag") == 0){
    config->tap_and_drag = atoi(value);
  }
  else if(strcmp(key, "drag_lock") == 0){
    config->drag_lock = atoi(value);
  }
  else if(strcmp(key, "natural_scrolling") == 0){
    config->natural_scrolling = atoi(value);
  }
  else if(strcmp(key, "disable_while_typing") == 0){
    config->disable_while_typing = atoi(value);
  }
  else if(strcmp(key, "left_handed") == 0){
    config->left_handed = atoi(value);
  }
  else if(strcmp(key, "middle_button_emulation") == 0){
    config->middle_button_emulation = atoi(value);
  }
  else if(strcmp(key, "scroll_method") == 0){
    if(strcmp(value, "LIBINPUT_CONFIG_SCROLL_2FG") == 0){
      config->scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
    }
    else if(strcmp(value, "LIBINPUT_CONFIG_SCROLL_EDGE") == 0){
      config->scroll_method = LIBINPUT_CONFIG_SCROLL_EDGE;
    }
    else if(strcmp(value, "LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN") == 0){
      config->scroll_method = LIBINPUT_CONFIG_SCROLL_ON_BUTTON_DOWN;
    }
    else if(strcmp(value, "LIBINPUT_CONFIG_SCROLL_NO_SCROLL") == 0){
      config->scroll_method = LIBINPUT_CONFIG_SCROLL_NO_SCROLL;
    }
  }
  else if(strcmp(key, "click_method") == 0){
    if(strcmp(value, "LIBINPUT_CONFIG_CLICK_METHOD_NONE") == 0){
      config->click_method = LIBINPUT_CONFIG_CLICK_METHOD_NONE;
    }
    else if(strcmp(value, "LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS") == 0){
      config->click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
    }
    else if(strcmp(value, "LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER") == 0){
      config->click_method = LIBINPUT_CONFIG_CLICK_METHOD_CLICKFINGER;
    }
  }
  else if(strcmp(key, "send_events_mode") == 0){
    if(strcmp(value, "LIBINPUT_CONFIG_SEND_EVENTS_ENABLED") == 0){
      config->send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
    }
    else if(strcmp(value, "LIBINPUT_CONFIG_SEND_EVENTS_DISABLED") == 0){
      config->send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED;
    }
    else if(strcmp(value, "LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE") == 0){
      config->send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_DISABLED_ON_EXTERNAL_MOUSE;
    }
  }
  else if(strcmp(key, "accel_profile") == 0){
    if(strcmp(value, "LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT") == 0){
      config->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_FLAT;
    }
    else if(strcmp(value, "LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE") == 0){
      config->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
    }
  }
  else if(strcmp(key, "accel_speed") == 0){
    config->accel_speed = atof(value);
  }
  else if(strcmp(key, "button_map") == 0){
    config->button_map = atoi(value);
  }
  else if(strcmp(key, "focuscolor") == 0){
    uint32_t color = cfgparsecolor(value);
    if(color == -1){
      fprintf(stderr, "Invalid color format: %s\n", value);
    }
    else{
      cfghextorgba(config->focuscolor, color);
    }
  }
  else if(strcmp(key, "unfocusedcolor") == 0){
    uint32_t color = cfgparsecolor(value);
    if(color == -1){
      fprintf(stderr, "Invalud color format: %s\n", value);
    }
    else{
      cfghextorgba(config->unfocusedcolor, color);
    }
  }
  else if(strcmp(key, "fullscreencolor") == 0){
    uint32_t color = cfgparsecolor(value);
    if(color == -1){
      fprintf(stderr, "Invalud color format: %s\n", value);
    }
    else{
      cfghextorgba(config->fullscreencolor, color);
    }
  }
  else if(strcmp(key, "exec") == 0){
    config->exec = realloc(config->exec, (config->exec_count + 1) * sizeof(char *));
    if(!config->exec){
      fprintf(stderr, "Failed to alloc memory for exec\n");
      return;
    }
    config->exec[config->exec_count++] = strdup(value);
  }
  else if(strcmp(key, "exec-once") == 0){
    config->exec_once = realloc(config->exec_once, (config->exec_once_count + 1) * sizeof(char *));
    if(!config->exec_once){
      fprintf(stderr, "Failed to alloc memory for exec-once\n");
      return;
    }
    config->exec_once[config->exec_once_count++] = strdup(value);
  }
  else if(strncmp(key, "bind", 4) == 0){
    config->key_bindings = realloc(config->key_bindings, (config->key_bind_count + 1) * sizeof(KeyBinding));
    if(!config->key_bindings){
      fprintf(stderr, "Failed to alloc memory for keybindings;\n");
      return;
    }

    KeyBinding *binding = &config->key_bindings[config->key_bind_count];
    memset(binding, 0, sizeof(KeyBinding));

    char mod_str[256], keysym_str[256], func_name[256], arg_value[256] = "none";
    if(sscanf(value, "%[^,],%[^,],%[^,],%[^,\n]", mod_str, keysym_str, func_name, arg_value) < 3){
      fprintf(stderr, "Invalid keybinding format: %s\n", value);
      return;
    }

    cfgtrim(mod_str);
    cfgtrim(keysym_str);
    cfgtrim(func_name);
    cfgtrim(arg_value);

    binding->mod = cfgparsemod(mod_str);
    binding->keysym = cfgparsekeysym(keysym_str);
    binding->arg.v = NULL;
    binding->func = cfgparsefunc(func_name, &binding->arg, arg_value);
    if(!binding->func){
      fprintf(stderr, "Invalid functions in the bind: %s\n", func_name);
    }
    else{
      config->key_bind_count++;
    }
  }
  else if(strncmp(key, "mousebind", 9) == 0){
    config->mouse_bindings = realloc(config->mouse_bindings, (config->mouse_bind_count +1) * sizeof(MouseBinding));
    if(!config->mouse_bindings){
      fprintf(stderr, "Failed to alloc memory for mousebindings;\n");
      return;
    }

    MouseBinding *binding = &config->mouse_bindings[config->mouse_bind_count];
    memset(binding, 0, sizeof(MouseBinding));

    char mod_str[256], button_str[256], func_name[256], arg_value[256] = "none";
    if(sscanf(value, "%[^,],%[^,],%[^,],%[^\n]", mod_str, button_str, func_name, arg_value) < 3){
      fprintf(stderr, "Invalid mousebind format: %s\n", func_name);
      return;
    }

    cfgtrim(mod_str);
    cfgtrim(button_str);
    cfgtrim(func_name);
    cfgtrim(arg_value);
    
    binding->mod = cfgparsemod(mod_str);
    binding->button = cfgparsebutton(button_str);
    binding->arg.v = NULL;
    binding->func = cfgparsefunc(func_name, &binding->arg, arg_value);
    if(!binding->func){
      fprintf(stderr, "Invalid function in the bind: %s\n", func_name);
    }
    else{
      config->mouse_bind_count++;
    }
  }
  else if(strncmp(key, "monrule", 7) == 0){
    config->mon_rules = realloc(config->mon_rules, (config->mon_rule_count + 1) * sizeof(MonitorRule));
    if(!config->mon_rules){
      fprintf(stderr, "Failed to alloc memory for monitorrules;\n");
      return;
    }

    MonitorRule *rule = &config->mon_rules[config->mon_rule_count];
    memset(rule, 0, sizeof(MonitorRule));

    char name_str[256], transform_str[256];
    float scale;
    int32_t x, y;

    if(sscanf(value, "%[^,],%f,%d,%d,%[^\n]", name_str, &scale, &x, &y, transform_str) < 5){
      fprintf(stderr, "Invalid monrule format: %s\n", value);
      return;
    }

    cfgtrim(name_str);
    cfgtrim(transform_str);

    rule->name = (strcmp(name_str, "NULL") == 0) ? NULL : strdup(name_str);
    rule->scale = scale;
    rule->x = x;
    rule->y = y;
    rule->transform = cfgparsetransform(transform_str);

    config->mon_rule_count++;
  }
}

void cfgparseline(Config *config, char *line){
  char key[256];
  char value[256];
  if(sscanf(line, "%255[^=]=%255[^\n]", key, value) != 2){
    fprintf(stderr, "Failed to read line, line: %s\n", line);
    return;
  } 
  cfgtrim(key);
  cfgtrim(value);
  cfgparseoption(config, key, value);
}

void cfgparsefile(Config *config, const char *file_path){
  FILE *file;
  char full_path[1024];
  file = fopen(file_path, "r");
  if(!file){
    fprintf(stderr, "Failed to open file, File path provided: %s\n", file_path);
    return;
  }

  char line[512];
  uint32_t count = 0;
  while(fgets(line, sizeof(line), file)){
    count++;
    if(line[0] == '#' || line[0] == '\n'){
      continue;
    }
    cfgparseline(config, line);
  }

  fclose(file);
}

void cfgprintf(Config config){
  cfgparsefile(&config, "/home/wdkaza/code/untitled/src/config.conf");
  printf("config.repeat_rate:%d \n", config.repeat_rate);
  printf("config.repeat_delay%d \n", config.repeat_delay);
}

void cfgrunexec(Config *config){
  for(uint32_t i = 0; i < config->exec_count; i++){
    if(fork() == 0){
      execl("/bin/sh", "sh", "-c", config->exec[i], NULL);
      fprintf(stderr, "Failed to exec: %s\n", config->exec[i]);
      exit(1);
    }
  }
}

void cfgrunexeconce(Config *config){
  static int run = 0;
  if(run){
    return;
  }
  run = 1;

  for(uint32_t i = 0; i < config->exec_once_count; i++){
    if(fork() == 0){
      execl("/bin/sh", "sh", "-c", config->exec_once[i], NULL);
      fprintf(stderr, "Failed to exec-once: %s\n", config->exec_once[i]);
      exit(1);
    }
  }
}

void cfgsetdefaultvalue(Config *config){
  memset(config, 0, sizeof(Config));
  config->repeat_rate = 25;
  config->repeat_delay = 600;
  config->numlockon = 0;
  config->scroll_method = LIBINPUT_CONFIG_SCROLL_2FG;
  config->click_method = LIBINPUT_CONFIG_CLICK_METHOD_BUTTON_AREAS;
  config->send_events_mode = LIBINPUT_CONFIG_SEND_EVENTS_ENABLED;
  config->accel_profile = LIBINPUT_CONFIG_ACCEL_PROFILE_ADAPTIVE;
  config->accel_speed = 0.0f;
  config->button_map = LIBINPUT_CONFIG_TAP_MAP_LRM;
  config->tap_to_click = 1;
  config->tap_and_drag = 1;
  config->drag_lock = 0;
  config->natural_scrolling = 0;
  config->disable_while_typing = 1;
  config->left_handed = 0;
  config->middle_button_emulation = 0;
  config->border_size = 2;
  config->border_radius = 0;
  cfghextorgba(config->focuscolor,      0xFF0000FF);
  cfghextorgba(config->unfocusedcolor,  0x00FF00FF);
  cfghextorgba(config->fullscreencolor, 0x000000FF);
}
