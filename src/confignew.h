#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <xkbcommon/xkbcommon.h>

typedef struct{
  uint32_t mod;
  xkb_keysym_t keysym;
  //void (*func)(const Arg *);
  //const Arg arg;
} KeyBinding;

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
  KeyBinding *key_bindings; 
  // window eye candy
  uint32_t border_size;
  uint32_t border_radius;
} Config;

Config config;


void cfgparseoption(Config *config, char *key, char *value){
  if(strcmp(key, "repeat_rate") == 0){
    config->repeat_rate = atoi(value);
  }
  else if(strcmp(key, "repeat_delay") == 0){
    config->repeat_delay = atoi(value);
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
