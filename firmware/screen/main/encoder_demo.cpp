#include <stdio.h>
#include <string.h>
#include <sys/select.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "encoder_packet.h"
#ifdef CONFIG_OPENDOTLET_WIFI_UPDATE
#include "wifi_update.h"
#endif
#include "lvgl.h"
extern "C" void od_hardware_init();
extern "C" void od_hardware_loop();
static QueueHandle_t incoming;
static uint8_t peer[6];static bool paired=false;
static uint32_t session=0,last_sequence=0,retired[8]={};static unsigned retired_next=0;static int32_t values[4]={};
static lv_obj_t *number,*heading,*state,*totals;
struct Received {uint8_t mac[6];OdEvent event;};
static void received(const esp_now_recv_info_t *info,const uint8_t *data,int length){
 Received r={};if(!od_decode(data,length,r.event)||(r.event.kind!=OD_EVENT&&r.event.kind!=OD_HELLO))return;
 memcpy(r.mac,info->src_addr,6);xQueueSend(incoming,&r,0); // No LVGL or logging from radio callback.
}
static void acknowledge(OdEvent e){e.kind=OD_ACK;uint8_t bytes[OD_PACKET_SIZE];od_encode(e,bytes);esp_now_send(peer,bytes,sizeof bytes);}
static void radio_init(){
 ESP_ERROR_CHECK(nvs_flash_init());ESP_ERROR_CHECK(esp_netif_init());ESP_ERROR_CHECK(esp_event_loop_create_default());esp_netif_create_default_wifi_sta();
 wifi_init_config_t cfg=WIFI_INIT_CONFIG_DEFAULT();ESP_ERROR_CHECK(esp_wifi_init(&cfg));ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
 ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));ESP_ERROR_CHECK(esp_wifi_start());ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
 ESP_ERROR_CHECK(esp_wifi_set_channel(6,WIFI_SECOND_CHAN_NONE));ESP_ERROR_CHECK(esp_now_init());
 incoming=xQueueCreate(16,sizeof(Received));assert(incoming);ESP_ERROR_CHECK(esp_now_register_recv_cb(received));
 uint8_t mac[6];ESP_ERROR_CHECK(esp_wifi_get_mac(WIFI_IF_STA,mac));printf("SCREEN_LINK v1 MAC=" MACSTR " channel=6 paired=0\n",MAC2STR(mac));
 nvs_handle_t saved;if(nvs_open("od_link",NVS_READONLY,&saved)==ESP_OK){size_t size=6;uint8_t m[6];if(nvs_get_blob(saved,"peer",m,&size)==ESP_OK&&size==6){esp_now_peer_info_t p={};memcpy(p.peer_addr,m,6);p.channel=0;p.ifidx=WIFI_IF_STA;if(esp_now_add_peer(&p)==ESP_OK){memcpy(peer,m,6);paired=true;lv_label_set_text(state,"PEER RESTORED / waiting");}}nvs_close(saved);}
}
static void command(const char *line){
#ifdef CONFIG_OPENDOTLET_WIFI_UPDATE
 if(WifiUpdate::command(line))return;
#endif
 if(!strcmp(line,"STATUS")){uint8_t mac[6],ch=0;wifi_second_chan_t second;esp_wifi_get_channel(&ch,&second);esp_wifi_get_mac(WIFI_IF_STA,mac);printf("SCREEN_LINK v1 MAC=" MACSTR " channel=%u paired=%u session=%lu applied=%lu rotation=%u\n",MAC2STR(mac),ch,paired,(unsigned long)session,(unsigned long)last_sequence,(unsigned)lv_disp_get_rotation(NULL)*90);return;}
 if(strncmp(line,"peer ",5))return;
 uint8_t mac[6];if(!od_mac_parse(line+5,mac)){puts("SCREEN_LINK invalid unicast peer");return;}
 if(paired){esp_now_del_peer(peer);}
 paired=false;xQueueReset(incoming);
 esp_now_peer_info_t p={};memcpy(p.peer_addr,mac,6);p.channel=0;p.ifidx=WIFI_IF_STA;
 esp_err_t err=esp_now_add_peer(&p);if(err==ESP_OK){memcpy(peer,mac,6);paired=true;session=last_sequence=0;memset(retired,0,sizeof retired);nvs_handle_t saved;if(nvs_open("od_link",NVS_READWRITE,&saved)==ESP_OK){nvs_set_blob(saved,"peer",mac,6);nvs_commit(saved);nvs_close(saved);}lv_label_set_text(state,"PAIRED / turn a knob");}
 printf("SCREEN_LINK peer_configured=%u error=%d\n",paired,err);
}
static void apply(const Received &r){
 if(!paired||memcmp(peer,r.mac,6)){return;}
 const OdEvent &e=r.event;
 if(e.kind==OD_HELLO){
  for(uint32_t old:retired){if(old==e.session)return;}
  if(session!=e.session){if(session)retired[retired_next++%8]=session;session=e.session;last_sequence=0;memset(values,0,sizeof values);lv_label_set_text(number,"0");lv_label_set_text(totals,"E1 0   E2 0\nE3 0   E4 0");lv_label_set_text(state,"PAIRED / turn a knob");printf("SCREEN_SESSION session=%lu\n",(unsigned long)session);}
  acknowledge(e);return;
 }
 if(!session||session!=e.session)return;
 if(e.sequence==last_sequence){acknowledge(e);return;} // Retry acknowledged without applying twice.
 if(e.sequence<last_sequence)return;
 int64_t v=(int64_t)values[e.encoder-1]+e.delta;if(v>99999)v=99999;if(v< -99999)v=-99999;values[e.encoder-1]=(int32_t)v;
 const unsigned ports[4]={3,2,1,4};lv_label_set_text_fmt(heading,"ENC%u / AX22 port %u",e.encoder,ports[e.encoder-1]);
 lv_label_set_text_fmt(number,"%+ld",(long)values[e.encoder-1]);
 if(e.button==1){lv_label_set_text_fmt(state,"CONFIRMED %+ld",(long)values[e.encoder-1]);lv_obj_set_style_text_color(state,lv_color_hex(0x72ed9e),0);}
 else if(e.button==0){lv_label_set_text(state,"SELECTION CHANGED");lv_obj_set_style_text_color(state,lv_color_hex(0xff9c52),0);}
 lv_label_set_text_fmt(totals,"E1 %+ld   E2 %+ld\nE3 %+ld   E4 %+ld",(long)values[0],(long)values[1],(long)values[2],(long)values[3]);
 last_sequence=e.sequence;
 printf("SCREEN_APPLIED session=%lu seq=%lu encoder=%u delta=%ld button=%u value=%ld\n",(unsigned long)session,(unsigned long)e.sequence,e.encoder,(long)e.delta,e.button,(long)values[e.encoder-1]);
 acknowledge(e);
}
extern "C" void app_main(){
 printf("OpenDotlet encoder demo v1 / ESP32-S3-Touch-LCD-1.46 / SPD2010 412x412\n");
 od_hardware_init();
 // Rotate the exact working framebuffer for the upside-down modular mounting.
 // The vendor hardware callback's NONE mirror mapping differs from panel boot
 // defaults, so use LVGL software rotation and leave panel registers unchanged.
 lv_disp_t *display=lv_disp_get_default();
 display->driver->sw_rotate=1;
 display->driver->drv_update_cb=NULL;
 lv_disp_set_rotation(display,LV_DISP_ROT_180);
 printf("SCREEN_DISPLAY rotation=%u\n",(unsigned)lv_disp_get_rotation(NULL)*90);
 lv_obj_t *scr=lv_scr_act();lv_obj_set_style_bg_color(scr,lv_color_hex(0x101516),0);lv_obj_clear_flag(scr,LV_OBJ_FLAG_SCROLLABLE);
 auto label=[&](const char *text,int y){lv_obj_t *o=lv_label_create(scr);lv_obj_set_style_text_color(o,lv_color_hex(0xffffff),0);lv_label_set_text(o,text);lv_obj_align(o,LV_ALIGN_CENTER,0,y);return o;};
 label("OpenDotlet / encoder bench",-125);heading=label("USB pair setup required",-75);number=label("0",-20);lv_obj_set_style_transform_zoom(number,600,0);state=label("WAITING FOR PEER",70);totals=label("E1 0   E2 0\nE3 0   E4 0",110);label("Raw transitions / UI demo only",140);
 radio_init();
#ifdef CONFIG_OPENDOTLET_WIFI_UPDATE
 WifiUpdate::begin("screen-link-v1-4enc-r180");
#endif
 char line[384];unsigned used=0;bool too_long=false;
 while(true){
  Received r;for(unsigned i=0;i<8&&xQueueReceive(incoming,&r,0)==pdTRUE;i++)apply(r);
  fd_set fds;FD_ZERO(&fds);FD_SET(0,&fds);timeval tv={0,0};
  if(select(1,&fds,NULL,NULL,&tv)>0){int c=getchar();if(c=='\n'){if(!too_long){line[used]=0;command(line);}used=0;too_long=false;}else if(c>=32&&c<127){if(used<sizeof(line)-1)line[used++]=c;else too_long=true;}}
  lv_timer_handler();od_hardware_loop();vTaskDelay(pdMS_TO_TICKS(10));
 }
}
