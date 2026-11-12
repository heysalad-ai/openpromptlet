#include <initializer_list>

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "../../firmware/axiometa_encoder_screen_link/encoder_packet.h"
int main(){
 uint8_t bytes[OD_PACKET_SIZE];OdEvent out,e={3,0,OD_EVENT,0x01020304,9,-78};
 assert(od_encode(e,bytes));assert(bytes[0]=='O'&&bytes[4]==1&&bytes[5]==3&&bytes[8]==4&&bytes[11]==1);
 assert(bytes[16]==0xb2&&bytes[19]==0xff);assert(od_decode(bytes,sizeof bytes,out)&&out.delta==-78&&out.session==e.session);
 assert(!od_decode(bytes,19,out));bytes[4]=2;assert(!od_decode(bytes,20,out));bytes[4]=1;
 bytes[0]='X';assert(!od_decode(bytes,20,out));
 for(int32_t v:{INT32_MIN,INT32_MAX,0,-1,1}){e.delta=v;assert(od_encode(e,bytes));assert(od_decode(bytes,20,out)&&out.delta==v);}
 e.encoder=0;assert(!od_encode(e,bytes));e.encoder=4;assert(od_encode(e,bytes));assert(od_decode(bytes,20,out)&&out.encoder==4);e.encoder=5;assert(!od_encode(e,bytes));e.encoder=1;
 e.button=3;assert(!od_encode(e,bytes));e.button=1;e.delta=3;assert(!od_encode(e,bytes));e.delta=0;assert(od_encode(e,bytes));
 e.sequence=0;assert(!od_encode(e,bytes));e.sequence=1;e.session=0;assert(!od_encode(e,bytes));
 uint8_t mac[6];assert(od_mac_parse("02:00:00:00:00:01",mac));assert(!od_mac_parse("FF:FF:FF:FF:FF:FF",mac));assert(!od_mac_parse("00:00:00:00:00:00",mac));assert(!od_mac_parse("aa:bb:cc:dd:ee:ffEXTRA",mac));assert(!od_mac_parse("aa:zz:cc:dd:ee:ff",mac));
 puts("wire-format, signed bounds, malformed frame and unicast peer checks passed");
}
