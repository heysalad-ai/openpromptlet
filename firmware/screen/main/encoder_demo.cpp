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
#ifdef CONFIG_OPENPROMPTLET_ACTION_DEMO
#include "action_protocol.h"
#endif
#ifdef CONFIG_OPENDOTLET_WIFI_UPDATE
#include "wifi_update.h"
#endif
#include "lvgl.h"
#include "green_dot.h"
extern "C" void od_hardware_init();
extern "C" void od_hardware_loop();
extern "C" void od_hardware_backlight(int value);
static QueueHandle_t incoming;
static uint8_t peer[6];static bool paired=false;
static uint32_t session=0,last_sequence=0,retired[8]={};static unsigned retired_next=0;static int32_t values[4]={};
static lv_obj_t *number,*heading,*state,*totals;
#ifdef CONFIG_OPENPROMPTLET_ACTION_DEMO
static ActionProtocol::Session host;
static bool run_released=false,host_was_connected=false;
static uint32_t millis_now(){return (uint32_t)(esp_timer_get_time()/1000);}
static void render_action(){
 bool connected=host.connected(millis_now());
 unsigned phase=connected?host.frame.phase:static_cast<unsigned>(ActionProtocol::Offline);
 GreenDot::state(phase);
 lv_label_set_text(heading,connected?host.frame.title:"Connect Mac companion");
 lv_label_set_text(number,ActionProtocol::phase_name(phase));
 const char *detail=connected?host.frame.detail:"Waiting for live host state";
 if(connected&&phase==ActionProtocol::Ready&&!run_released)detail="Press and release knob 1\nto arm, then press to run";
 lv_label_set_text(state,detail);
 lv_obj_set_style_text_color(number,lv_color_hex(phase==ActionProtocol::Completed?0x72ed9e:phase==ActionProtocol::Failed?0xff8b83:0xffffff),0);
 lv_label_set_text_fmt(totals,"%lus / %s",connected?(unsigned long)host.frame.elapsed:0UL,connected?(run_released?"armed":"release to arm"):"offline");
 if(connected)od_hardware_backlight(host.frame.brightness);
 host_was_connected=connected;
}
static bool action_command(const char *line){
 if(!strncmp(line,"HOST_HELLO ",11)){
  const char *nonce=line+11;
  if(host.hello(nonce,millis_now())){run_released=false;GreenDot::reset();render_action();printf("ACTION_HELLO v=1 nonce=%s rotation=180 release_required=1\n",host.nonce);}
  return true;
 }
 if(!strncmp(line,"HOST_STATE ",11)){
  ActionProtocol::Frame frame;
  if(ActionProtocol::parse(line,frame)&&host.accept(frame,millis_now())){render_action();printf("HOST_ACK v=1 nonce=%s revision=%lu phase=%u armed=%u brightness=%u\n",host.nonce,(unsigned long)frame.revision,frame.phase,run_released,frame.brightness);}
  return true;
 }
 return false;
}
static void action_event(const OdEvent &event){
 if(!host.connected(millis_now())){run_released=false;return;}
 if(event.encoder==1&&event.button==2)run_released=true;
 printf("CONTROL_EVENT v=1 nonce=%s session=%lu seq=%lu encoder=%u delta=%ld button=%u armed=%u\n",host.nonce,
  (unsigned long)event.session,(unsigned long)event.sequence,event.encoder,(long)event.delta,event.button,run_released);
 render_action();
}
#endif
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
#ifdef CONFIG_OPENPROMPTLET_ACTION_DEMO
 if(GreenDot::checkpoint(line,host.nonce,host.connected(millis_now())))return;
 if(action_command(line))return;
#endif
#ifdef CONFIG_OPENDOTLET_WIFI_UPDATE
 if(WifiUpdate::command(line))return;
#endif
 if(!strcmp(line,"STATUS")){uint8_t mac[6],ch=0;wifi_second_chan_t second;esp_wifi_get_channel(&ch,&second);esp_wifi_get_mac(WIFI_IF_STA,mac);printf("SCREEN_LINK v1 MAC=" MACSTR " channel=%u paired=%u session=%lu applied=%lu rotation=%u\n",MAC2STR(mac),ch,paired,(unsigned long)session,(unsigned long)last_sequence,(unsigned)lv_disp_get_rotation(NULL)*90);
#ifdef CONFIG_OPENPROMPTLET_ACTION_DEMO
 printf("ACTION_STATUS v=1 connected=%u armed=%u rotation=180 phase=%u revision=%lu brightness=%u\n",host.connected(millis_now()),run_released,host.frame.phase,(unsigned long)host.frame.revision,host.frame.brightness);
#endif
 return;}
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
  if(session!=e.session){if(session)retired[retired_next++%8]=session;session=e.session;last_sequence=0;memset(values,0,sizeof values);
#ifdef CONFIG_OPENPROMPTLET_ACTION_DEMO
 run_released=false;render_action();
 if(*host.nonce)printf("CONTROL_SESSION v=1 nonce=%s session=%lu\n",host.nonce,(unsigned long)session);
#else
 lv_label_set_text(number,"0");lv_label_set_text(totals,"E1 0   E2 0\nE3 0   E4 0");lv_label_set_text(state,"PAIRED / turn a knob");
#endif
printf("SCREEN_SESSION session=%lu\n",(unsigned long)session);}
  acknowledge(e);return;
 }
 if(!session||session!=e.session)return;
 if(e.sequence==last_sequence){acknowledge(e);return;} // Retry acknowledged without applying twice.
 if(e.sequence<last_sequence)return;
 int64_t v=(int64_t)values[e.encoder-1]+e.delta;if(v>99999)v=99999;if(v< -99999)v=-99999;values[e.encoder-1]=(int32_t)v;
 #ifdef CONFIG_OPENPROMPTLET_ACTION_DEMO
 action_event(e);
#else
 const unsigned ports[4]={3,2,1,4};lv_label_set_text_fmt(heading,"ENC%u / AX22 port %u",e.encoder,ports[e.encoder-1]);
 lv_label_set_text_fmt(number,"%+ld",(long)values[e.encoder-1]);
 if(e.button==1){lv_label_set_text_fmt(state,"CONFIRMED %+ld",(long)values[e.encoder-1]);lv_obj_set_style_text_color(state,lv_color_hex(0x72ed9e),0);}
 else if(e.button==0){lv_label_set_text(state,"SELECTION CHANGED");lv_obj_set_style_text_color(state,lv_color_hex(0xff9c52),0);}
 lv_label_set_text_fmt(totals,"E1 %+ld   E2 %+ld\nE3 %+ld   E4 %+ld",(long)values[0],(long)values[1],(long)values[2],(long)values[3]);
 #endif
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
 #ifdef CONFIG_OPENPROMPTLET_ACTION_DEMO
 GreenDot::init(scr);
 label("OpenPromptlet",-159);heading=label("Connect Mac companion",-35);
 lv_obj_set_width(heading,300);lv_obj_set_style_text_align(heading,LV_TEXT_ALIGN_CENTER,0);
 number=label("Mac offline",3);lv_obj_set_style_text_font(number,&lv_font_montserrat_28,0);
 state=label("Waiting for live host state",55);lv_obj_set_width(state,300);lv_obj_set_style_text_align(state,LV_TEXT_ALIGN_CENTER,0);
 totals=label("offline",92);label("1 Run  2 View  3 Scroll  4 Light",151);
 render_action();
#else
 label("OpenDotlet / encoder bench",-125);heading=label("USB pair setup required",-75);number=label("0",-20);lv_obj_set_style_transform_zoom(number,600,0);state=label("WAITING FOR PEER",70);totals=label("E1 0   E2 0\nE3 0   E4 0",110);label("Raw transitions / UI demo only",140);
#endif
 radio_init();
#ifdef CONFIG_OPENDOTLET_WIFI_UPDATE
 #ifdef CONFIG_OPENPROMPTLET_ACTION_DEMO
 WifiUpdate::begin("openpromptlet-green-dot-v1-r180");
#else
 WifiUpdate::begin("screen-link-v1-4enc-r180");
#endif
#endif
 setvbuf(stdin,NULL,_IONBF,0);
 char line[640];unsigned used=0;bool too_long=false;
 while(true){
  Received r;for(unsigned i=0;i<8&&xQueueReceive(incoming,&r,0)==pdTRUE;i++)apply(r);
  // Drain a bounded serial batch so heartbeat frames do not queue behind
  // one character per display tick. Radio and LVGL retain their regular turn.
  for(unsigned n=0;n<1024;n++){
   fd_set fds;FD_ZERO(&fds);FD_SET(0,&fds);timeval tv={0,0};
   if(select(1,&fds,NULL,NULL,&tv)<=0)break;
   int c=getchar();if(c<0){clearerr(stdin);break;}
   if(c=='\n'){if(!too_long){line[used]=0;command(line);}used=0;too_long=false;}
   else if(c>=32&&c<127){if(used<sizeof(line)-1)line[used++]=c;else too_long=true;}
  }
  #ifdef CONFIG_OPENPROMPTLET_ACTION_DEMO
  if(host_was_connected&&!host.connected(millis_now())){run_released=false;render_action();}
#endif
  GreenDot::tick((uint32_t)(esp_timer_get_time()/1000));
  lv_timer_handler();od_hardware_loop();vTaskDelay(pdMS_TO_TICKS(10));
 }
}
