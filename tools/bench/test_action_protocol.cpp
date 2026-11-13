#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "../../firmware/screen/main/action_protocol.h"
int main(){
 using namespace ActionProtocol;
 Frame f;
 const char *valid="HOST_STATE 0123456789abcdef0123456789abcdef 1 2 12 65 50726f6a6563742073756d6d617279 576f726b696e670a506c656173652077616974";
 assert(parse(valid,f));assert(f.phase==Running&&f.elapsed==12&&!strcmp(f.title,"Project summary"));
 assert(!strcmp(f.detail,"Working\nPlease wait"));Session s;assert(!s.connected(0));
 assert(!s.hello("short",0));assert(s.hello(f.nonce,UINT32_MAX-10));
 assert(s.accept(f,UINT32_MAX-10));assert(s.connected(5));assert(!s.accept(f,20));
 assert(!s.connected(timeout_ms));
 Frame wrong=f;wrong.revision=2;wrong.nonce[0]='a';assert(!s.accept(wrong,10));
 const char *bad[]={
  "HOST_STATE 0123456789abcdef0123456789abcdef 0 2 12 65 - -",
  "HOST_STATE 0123456789abcdef0123456789abcdef 1 8 12 65 - -",
  "HOST_STATE 0123456789abcdef0123456789abcdef 1 2 86401 65 - -",
  "HOST_STATE 0123456789abcdef0123456789abcdef 1 2 12 0 - -",
  "HOST_STATE 0123456789abcdef0123456789abcdef 1 2 12 101 - -",
  "HOST_STATE 0123456789abcdef0123456789abcdef 1 2 12 65 00 -",
  "HOST_STATE 0123456789abcdef0123456789abcdef 1 2 12 65 0a -",
  "HOST_STATE 0123456789abcdef0123456789abcdef 1 2 12 65 - zz",
  "HOST_STATE 0123456789abcdef0123456789abcdef 1 2 12 65 - 1",
  "HOST_STATE 0123456789abcdef0123456789abcdef 1 2 12 65 - - extra",
  "HOST_STATE 0123456789abcdef0123456789abcdef 4294967296 2 12 65 - -"
 };
 for(const char *line:bad)assert(!parse(line,f));
 puts("action protocol: frame bounds, text, nonce, duplicate revision and heartbeat checks passed");
}
