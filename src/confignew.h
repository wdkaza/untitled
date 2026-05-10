#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <regex.h>
#include <ctype.h>
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

} ConfigMonRule;

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
  // window eye candy
  uint32_t border_size;
  uint32_t border_radius;
} Config;

typedef void (*FuncType)(const Arg *);
Config config;


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
  uint32_t mod = 0;
  char lower_str[64];
  int i = 0;

  for(i = 0; mod_str[i] && i < sizeof(lower_str) - 1; i++){
    lower_str[i] = tolower(mod_str[i]);
  }
  lower_str[i] = '\0';

  if(strstr(lower_str, "super") || strstr(lower_str, "logo") || strstr(lower_str, "win")){
    return mod |= WLR_MODIFIER_LOGO;
  }
  else if(strcmp(lower_str, "ctrl") == 0){
    return mod |= WLR_MODIFIER_CTRL;
  }
  if(strcmp(lower_str, "shift") == 0){
    return mod |= WLR_MODIFIER_SHIFT;
  }
  if(strcmp(lower_str, "alt") == 0){
    return mod |= WLR_MODIFIER_ALT;
  }
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
}

FuncType cfgparsefunc(char *func_name, Arg *arg, char *arg_value){
  FuncType function = NULL;
  (*arg).v = NULL;

  if(strcmp(func_name, "spawn") == 0){
    function = spawn;
  }
  else if(strcmp(func_name, "extiwm") == 0){
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
    config->scroll_method = atoi(value);
  }
  else if(strcmp(key, "click_method") == 0){
    config->click_method = atoi(value);
  }
  else if(strcmp(key, "send_events_mode") == 0){
    config->send_events_mode = atoi(value);
  }
  else if(strcmp(key, "accel_profile") == 0){
    config->accel_profile = atoi(value);
  }
  else if(strcmp(key, "accel_speed") == 0){
    config->accel_speed = atoi(value);
  }
  else if(strcmp(key, "button_map") == 0){
    config->button_map = atoi(value);
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
  else if(strncmp(key, "mousebind", 4) == 0){
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
}

void cfgparseline(Config *config, char *line){
  char key[256];
  char value[256];
  if(sscanf(line, "%255[^=]=%255[^\n]", key, value) != 2){
    fprintf(stderr, "Failed to read line, line: %s\n", line);
  } 
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
  uint32_t count;
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
/*
void cfgsetdefaultvalue(){
  config.repeat_rate = 10;
  config.repeat_delay = 500;
}
*/
