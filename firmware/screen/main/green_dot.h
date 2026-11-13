#pragma once
#include "lvgl.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// UI-only extension to the existing nonce-scoped USB action protocol.
// No device input, launch, approval or network permission is added here.
namespace GreenDot {
static lv_obj_t *ring=nullptr,*dot=nullptr,*caption=nullptr;
static unsigned phase=7,checkpoints=0;
static uint32_t revision=0;
static void init(lv_obj_t *screen){
 ring=lv_arc_create(screen);lv_obj_set_size(ring,390,390);lv_obj_center(ring);
 lv_arc_set_rotation(ring,270);lv_arc_set_bg_angles(ring,0,360);lv_arc_set_range(ring,0,5);lv_arc_set_value(ring,0);
 lv_obj_remove_style(ring,nullptr,LV_PART_KNOB);lv_obj_clear_flag(ring,LV_OBJ_FLAG_CLICKABLE);
 lv_obj_set_style_arc_color(ring,lv_color_hex(0x345443),LV_PART_MAIN);lv_obj_set_style_arc_width(ring,3,LV_PART_MAIN);
 lv_obj_set_style_arc_color(ring,lv_color_hex(0x8fe8a7),LV_PART_INDICATOR);lv_obj_set_style_arc_width(ring,4,LV_PART_INDICATOR);
 lv_obj_set_style_bg_opa(ring,LV_OPA_TRANSP,0);
 dot=lv_obj_create(screen);lv_obj_set_size(dot,54,54);lv_obj_set_style_radius(dot,LV_RADIUS_CIRCLE,0);
 lv_obj_set_style_border_width(dot,0,0);lv_obj_set_style_bg_color(dot,lv_color_hex(0x8fe8a7),0);
 lv_obj_align(dot,LV_ALIGN_CENTER,0,-104);lv_obj_clear_flag(dot,LV_OBJ_FLAG_SCROLLABLE);
 caption=lv_label_create(screen);lv_obj_set_style_text_color(caption,lv_color_hex(0xbdd6c5),0);
 lv_label_set_text(caption,"0 of 5 checkpoints");lv_obj_align(caption,LV_ALIGN_CENTER,0,121);
}
static void reset(){revision=0;checkpoints=0;if(ring)lv_arc_set_value(ring,0);if(caption)lv_label_set_text(caption,"0 of 5 checkpoints");}
static bool checkpoint(const char *line,const char *nonce,bool connected){
 if(strncmp(line,"VISUAL_STATE ",13))return false;
 char n[33],extra;unsigned r,done;
 if(connected&&sscanf(line,"VISUAL_STATE %32s %u %u %c",n,&r,&done,&extra)==3&&
    !strcmp(n,nonce)&&r>revision&&done<=5){
  revision=r;checkpoints=done;lv_arc_set_value(ring,done);
  lv_label_set_text_fmt(caption,"%u of 5 checkpoints",done);
  printf("VISUAL_ACK revision=%u checkpoints=%u\n",r,done);
 }
 return true;
}
static void state(unsigned value){if(value==7&&phase!=7)reset();phase=value;if(!dot)return;lv_obj_set_style_bg_color(dot,lv_color_hex(phase==4?0xeab58c:phase==7?0x789782:0x8fe8a7),0);}
static void tick(uint32_t now){
 if(!dot)return;
 const int t=int(now%1800);const int wave=t<900?t:1800-t;
 const bool active=phase==1||phase==2;
 const int size=active?48+wave/60:phase==3?66:54;
 lv_obj_set_size(dot,size,size);lv_obj_align(dot,LV_ALIGN_CENTER,active?wave/50-9:0,-104);
}
}
