#pragma once
#include <WiFi.h>
#include <Preferences.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <esp_random.h>
#include <freertos/queue.h>
#include "encoder_packet.h"
namespace ScreenLink {
struct Pending {OdEvent event;uint32_t queued,last;uint8_t attempts;};
static Pending queue[32];static uint8_t head=0,tail=0,peer[6];static bool paired=false,ready=false;
static uint32_t session=0,sequence=0;static bool welcomed=false;static uint32_t helloAt=0;
static bool command(const char *line);
struct Ack {uint8_t mac[6];uint32_t session,sequence;};static QueueHandle_t acknowledgements;
static void receive(const esp_now_recv_info_t *info,const uint8_t *data,int len){
 OdEvent e;if(!od_decode(data,len,e)||e.kind!=OD_ACK)return;
 Ack a;memcpy(a.mac,info->src_addr,6);a.session=e.session;a.sequence=e.sequence;xQueueSend(acknowledgements,&a,0);
}
static void status(){uint8_t mac[6];uint8_t channel=0;wifi_second_chan_t second;esp_wifi_get_channel(&channel,&second);
 if(esp_wifi_get_mac(WIFI_IF_STA,mac)!=ESP_OK){Serial.println("LINK MAC unavailable");return;}
 Serial.printf("LINK ready=%u paired=%u channel=%u welcomed=%u queued=%u session=%lu MAC=%02x:%02x:%02x:%02x:%02x:%02x\n",ready,paired,channel,welcomed,(head+32-tail)%32,(unsigned long)session,mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);}
static void begin(){
 WiFi.mode(WIFI_STA);WiFi.setSleep(false);esp_err_t channel=esp_wifi_set_channel(6,WIFI_SECOND_CHAN_NONE);
 session=esp_random();if(!session)session=1;
 acknowledgements=xQueueCreate(16,sizeof(Ack));
 ready=acknowledgements&&channel==ESP_OK&&esp_now_init()==ESP_OK;if(ready)esp_now_register_recv_cb(receive);status();
 Preferences saved;if(saved.begin("od_link",true)){uint8_t m[6];if(saved.getBytes("peer",m,6)==6){char line[32];snprintf(line,sizeof line,"peer %02x:%02x:%02x:%02x:%02x:%02x",m[0],m[1],m[2],m[3],m[4],m[5]);saved.end();command(line);}else saved.end();}
}
static bool command(const char *line){
 if(!strcmp(line,"link")){status();return true;}
 if(strncmp(line,"peer ",5))return false;
 uint8_t m[6];if(!od_mac_parse(line+5,m)){Serial.println("LINK invalid unicast peer");return true;}
 if(!ready){Serial.println("LINK radio unavailable");return true;}
 if(paired)esp_now_del_peer(peer);
 esp_now_peer_info_t p={};memcpy(p.peer_addr,m,6);p.channel=0;p.ifidx=WIFI_IF_STA;
 paired=esp_now_add_peer(&p)==ESP_OK;if(paired){memcpy(peer,m,6);Preferences saved;if(saved.begin("od_link",false)){uint8_t old[6];if(saved.getBytes("peer",old,6)!=6||memcmp(old,m,6))saved.putBytes("peer",m,6);saved.end();}}welcomed=false;helloAt=0;head=tail=0;xQueueReset(acknowledgements);status();return true;
}
static void reset(){head=tail=0;sequence=0;session=esp_random();if(!session)session=1;welcomed=false;helloAt=0;if(acknowledgements)xQueueReset(acknowledgements);}
static void event(uint8_t encoder,int32_t delta,uint8_t button){
 if(!paired)return;
 uint8_t n=(head+1)%32;if(n==tail){Serial.println("LINK DROP queue full; no replay");return;}
 if(++sequence==0){reset();sequence=1;}
 queue[head]={{encoder,button,OD_EVENT,session,sequence,delta},millis(),0,0};head=n;
}
static void tick(){
 if(!paired)return;uint32_t ack=0;Ack a;bool applied=false;
 for(unsigned i=0;i<16&&xQueueReceive(acknowledgements,&a,0)==pdTRUE;i++)if(!memcmp(a.mac,peer,6)&&a.session==session){if(a.sequence==0)welcomed=true;else if(head!=tail&&a.sequence==queue[tail].event.sequence){ack=a.sequence;applied=true;}}
 uint32_t helloNow=millis();if(!helloAt||uint32_t(helloNow-helloAt)>=(welcomed?500:120)){OdEvent hello={0,0,OD_HELLO,session,0,0};uint8_t bytes[OD_PACKET_SIZE];od_encode(hello,bytes);esp_now_send(peer,bytes,sizeof bytes);helloAt=helloNow;}
 if(!welcomed)return;
 if(head==tail)return;Pending &p=queue[tail];
 if(applied&&ack==p.event.sequence){Serial.printf("LINK APPLIED seq=%lu encoder=%u delta=%ld button=%u\n",(unsigned long)ack,p.event.encoder,(long)p.event.delta,p.event.button);tail=(tail+1)%32;return;}
 uint32_t now=millis();if(uint32_t(now-p.queued)>1500||(p.attempts>=4&&uint32_t(now-p.last)>120)){Serial.printf("LINK DROP seq=%lu unacknowledged\n",(unsigned long)p.event.sequence);tail=(tail+1)%32;return;}
 if(!p.attempts||uint32_t(now-p.last)>=120){uint8_t bytes[OD_PACKET_SIZE];od_encode(p.event,bytes);esp_err_t err=esp_now_send(peer,bytes,sizeof bytes);p.last=now;p.attempts++;if(err!=ESP_OK)Serial.printf("LINK send error=%d\n",err);}
}
}
