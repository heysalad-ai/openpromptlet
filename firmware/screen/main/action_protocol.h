#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace ActionProtocol {
static constexpr uint32_t timeout_ms = 3200;
enum Phase { Ready, Starting, Running, Completed, Failed, Busy, Uncertain, Offline };
struct Frame {
 char nonce[33] = {};
 uint32_t revision=0, elapsed=0;
 unsigned phase=Offline, brightness=65;
 char title[49] = {}, detail[145] = {};
};
inline bool nonce_valid(const char *s) {
 if(strlen(s)!=32)return false;
 for(unsigned i=0;i<32;i++)if(!strchr("0123456789abcdef",s[i]))return false;
 return true;
}
inline bool unsigned32(const char *s,uint32_t &out) {
 if(!*s)return false;
 uint64_t v=0;
 for(unsigned i=0;s[i];i++) {if(i>=10||s[i]<'0'||s[i]>'9')return false;v=v*10+s[i]-'0';if(v>UINT32_MAX)return false;}
 out=static_cast<uint32_t>(v);return true;
}
inline int digit(char c) {
 if(c>='0'&&c<='9')return c-'0';
 if(c>='a'&&c<='f')return c-'a'+10;
 if(c>='A'&&c<='F')return c-'A'+10;
 return -1;
}
inline bool unhex(const char *s,char *out,unsigned size,bool multiline) {
 if(!strcmp(s,"-")){*out=0;return true;}
 const unsigned length=strlen(s);if(length%2||length/2>=size)return false;
 for(unsigned i=0;i<length/2;i++) {
  int hi=digit(s[i*2]),lo=digit(s[i*2+1]);if(hi<0||lo<0)return false;
  int c=hi*16+lo;if((c<32||c>126)&&!(multiline&&c==10))return false;out[i]=char(c);
 }
 out[length/2]=0;return true;
}
inline bool parse(const char *line,Frame &out) {
 char nonce[33],rev[11],phase[3],elapsed[11],brightness[4],title[97],detail[289],extra[2];
 if(sscanf(line,"HOST_STATE %32s %10s %2s %10s %3s %96s %288s %1s",nonce,rev,phase,elapsed,brightness,title,detail,extra)!=7)return false;
 Frame f;uint32_t p,b;
 if(!nonce_valid(nonce)||!unsigned32(rev,f.revision)||!f.revision||!unsigned32(phase,p)||p>Offline||
    !unsigned32(elapsed,f.elapsed)||f.elapsed>86400||!unsigned32(brightness,b)||b<10||b>100||
    !unhex(title,f.title,sizeof f.title,false)||!unhex(detail,f.detail,sizeof f.detail,true))return false;
 strcpy(f.nonce,nonce);f.phase=p;f.brightness=b;out=f;return true;
}
struct Session {
 char nonce[33] = {};
 Frame frame;
 uint32_t seen=0;
 bool has_state=false;
 bool hello(const char *value,uint32_t now) {
  if(!nonce_valid(value))return false;
  strcpy(nonce,value);frame=Frame();seen=now;has_state=false;return true;
 }
 bool accept(const Frame &value,uint32_t now) {
  if(!*nonce||strcmp(nonce,value.nonce)||value.revision<=frame.revision)return false;
  frame=value;seen=now;has_state=true;return true;
 }
 bool connected(uint32_t now)const{return has_state&&uint32_t(now-seen)<timeout_ms;}
};
inline const char *phase_name(unsigned p) {
 const char *names[]={"Ready","Starting","Working","Complete","Failed","Busy","Check Mac","Mac offline"};
 return p<=Offline?names[p]:"Mac offline";
}
}
