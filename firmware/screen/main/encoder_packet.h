#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
// Explicit little-endian wire format; no native struct layout on the radio.
#define OD_PACKET_SIZE 20
#define OD_EVENT 0
#define OD_ACK 1
#define OD_HELLO 2
struct OdEvent { uint8_t encoder,button,kind; uint32_t session,sequence; int32_t delta; };
static inline void od_put32(uint8_t *p,uint32_t v){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i));}
static inline uint32_t od_get32(const uint8_t *p){uint32_t v=0;for(unsigned i=0;i<4;i++)v|=(uint32_t)p[i]<<(8*i);return v;}
static inline bool od_valid(const OdEvent &e){if(e.kind==OD_HELLO||(e.kind==OD_ACK&&e.encoder==0))return e.encoder==0&&e.button==0&&e.delta==0&&e.session!=0&&e.sequence==0;return e.encoder>=1&&e.encoder<=4&&e.button<=2&&e.kind<=1&&e.session!=0&&e.sequence!=0&&!(e.button&&e.delta);}
static inline bool od_encode(const OdEvent &e,uint8_t *p){if(!od_valid(e))return false;memcpy(p,"ODEN",4);p[4]=1;p[5]=e.encoder;p[6]=e.button;p[7]=e.kind;od_put32(p+8,e.session);od_put32(p+12,e.sequence);od_put32(p+16,(uint32_t)e.delta);return true;}
static inline bool od_decode(const uint8_t *p,size_t n,OdEvent &e){if(n!=OD_PACKET_SIZE||memcmp(p,"ODEN",4)||p[4]!=1)return false;e.encoder=p[5];e.button=p[6];e.kind=p[7];e.session=od_get32(p+8);e.sequence=od_get32(p+12);uint32_t bits=od_get32(p+16);memcpy(&e.delta,&bits,4);return od_valid(e);}
static inline bool od_mac_parse(const char *s,uint8_t *mac){if(strlen(s)!=17)return false;for(unsigned i=0;i<6;i++){unsigned v=0;for(unsigned j=0;j<2;j++){char c=s[i*3+j];unsigned d=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:c>='A'&&c<='F'?c-'A'+10:16;if(d>15)return false;v=v*16+d;}mac[i]=v;if(i<5&&s[i*3+2]!=':')return false;}uint8_t any=0;for(unsigned i=0;i<6;i++)any|=mac[i];return any&&!(mac[0]&1);}
