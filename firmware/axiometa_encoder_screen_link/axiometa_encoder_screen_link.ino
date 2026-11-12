#include <Arduino.h>
#include <pins_arduino.h>
#include "decoder.h"
#include "screen_link.h"
#ifdef OD_WIFI_UPDATE
#include "wifi_update.h"
#endif

#ifndef ARDUINO_AXIOMETA_GENESIS_MINI
#error Select the official Axiometa Genesis Mini board, esp32 core >= 3.3.10.
#endif
// Official core port constants, not guessed generic ESP32 pins.
constexpr uint8_t encoderCount = 4;
constexpr uint8_t pinCount = encoderCount * 3;
const uint8_t ports[encoderCount] = {3,2,1,4};
const uint8_t pins[pinCount] = {P3_IO0,P3_IO1,P3_IO2,P2_IO0,P2_IO1,P2_IO2,P1_IO0,P1_IO1,P1_IO2,P4_IO0,P4_IO1,P4_IO2};
// Official AX22-0003 Rev 2 schematic: IO0=BT(active HIGH), IO1=A, IO2=B.
// Pending hardware confirmation. All inputs use the module's own resistors.
uint8_t bt[encoderCount]={0,0,0,0}, a[encoderCount]={1,1,1,1}, b[encoderCount]={2,2,2,2};
int8_t direction[encoderCount]={1,1,1,1}; // Assumed sign until known CW movement is measured.
bool activeHigh[encoderCount]={true,true,true,true};
constexpr uint32_t debounceMs=20; // Design choice, not a measured module spec.
constexpr uint16_t capacity=1024; // Design: bounded raw-edge queue; overflow invalidates trial.
struct Edge { uint32_t us; uint8_t pin,level; };
volatile Edge edges[capacity];
volatile uint16_t head=0,tail=0;
volatile uint32_t dropped=0,invalid[encoderCount]={0,0,0};
volatile int32_t counts[encoderCount]={0,0,0};
volatile uint8_t ab[encoderCount]={0,0,0};
portMUX_TYPE mux=portMUX_INITIALIZER_UNLOCKED;
DebouncedButton buttons[encoderCount];
bool discovery=false;
uint32_t lastReport=0,lastDrop=0;
int32_t reported[encoderCount]={0,0,0};
uint32_t reportedInvalid[encoderCount]={0,0,0};

// ISR never prints. Arduino's default GPIO interrupt is not IRAM-only on this target.
void edgeISR(void *arg) {
  const uint8_t idx=uint8_t(uintptr_t(arg)), enc=idx/3;
  const uint8_t level=digitalRead(pins[idx]);
  const uint8_t next=(digitalRead(pins[enc*3+a[enc]])<<1)|digitalRead(pins[enc*3+b[enc]]);
  portENTER_CRITICAL_ISR(&mux);
  const uint16_t n=(head+1)%capacity;
  if(n==tail) ++dropped;
  else { edges[head].us=micros(); edges[head].pin=idx; edges[head].level=level; head=n; }
  if(idx%3==a[enc] || idx%3==b[enc]) {
    if((ab[enc]^next)==3) ++invalid[enc];
    counts[enc]+=direction[enc]*quadratureStep(ab[enc],next);
    ab[enc]=next;
  }
  portEXIT_CRITICAL_ISR(&mux);
}
void attachInputs() {
  for(uint8_t e=0;e<encoderCount;++e) {
    ab[e]=(digitalRead(pins[e*3+a[e]])<<1)|digitalRead(pins[e*3+b[e]]);
    buttons[e].reset(digitalRead(pins[e*3+bt[e]]),millis());
  }
  for(uint8_t i=0;i<pinCount;++i) attachInterruptArg(pins[i],edgeISR,reinterpret_cast<void*>(uintptr_t(i)),CHANGE);
}
void detachInputs() { for(auto p:pins) detachInterrupt(p); }
void resetTrial() {
  portENTER_CRITICAL(&mux);
  head=tail=0; dropped=0;
  for(uint8_t e=0;e<encoderCount;++e) { counts[e]=0; invalid[e]=0; reported[e]=0; reportedInvalid[e]=0; }
  portEXIT_CRITICAL(&mux);
  lastDrop=0;
  ScreenLink::reset();
}
void status() {
  ScreenLink::status();
  Serial.println("Official port GPIOs; signal roles from Rev 2 schematic, HARDWARE UNCONFIRMED.");
  for(uint8_t e=0;e<encoderCount;++e) Serial.printf("ENC%u port=%u BT=%u A=%u B=%u pressed=%s sign=%d (CW UNCONFIRMED)\n",e+1,ports[e],pins[e*3+bt[e]],pins[e*3+a[e]],pins[e*3+b[e]],activeHigh[e]?"HIGH":"LOW",direction[e]);
  for(uint8_t i=0;i<pinCount;++i) Serial.printf("LEVEL gpio=%u value=%d\n",pins[i],digitalRead(pins[i]));
  Serial.printf("mode=%s debounce_ms=%lu; ESP-NOW link v1; USB power only.\n",discovery?"discovery":"test",(unsigned long)debounceMs);
  Serial.println("Commands (newline): d discovery, t test, z zero, s status, r 1|2|3|4 reverse; m ENC BT_IO A_IO B_IO HIGH (IO=0..2,HIGH=0|1)");
}
void command(char *line) {
#ifdef OD_WIFI_UPDATE
  if(WifiUpdate::command(line))return;
#endif
  if(ScreenLink::command(line))return;
  if(!strcmp(line,"s")) { status(); return; }
  unsigned enc,btIO,aIO,bIO,high;
  if(!strcmp(line,"d")) discovery=true;
  else if(!strcmp(line,"t")) discovery=false;
  else if(!strcmp(line,"z")) { }
  else if(sscanf(line,"r %u",&enc)==1 && enc>=1 && enc<=encoderCount) {
    detachInputs(); direction[enc-1]*=-1; resetTrial(); attachInputs(); status(); return;
  } else if(sscanf(line,"m %u %u %u %u %u",&enc,&btIO,&aIO,&bIO,&high)==5 && enc>=1 && enc<=encoderCount && btIO<3 && aIO<3 && bIO<3 && btIO!=aIO && btIO!=bIO && aIO!=bIO && high<=1) {
    detachInputs(); --enc; bt[enc]=btIO; a[enc]=aIO; b[enc]=bIO; activeHigh[enc]=high;
    resetTrial(); attachInputs(); status(); return;
  } else { Serial.println("Invalid command; use s for help."); return; }
  detachInputs(); resetTrial(); attachInputs(); status();
}
void setup() {
  Serial.begin(115200);
  for(auto p:pins) pinMode(p,INPUT);
  delay(1000);
  attachInputs();
  ScreenLink::begin();
#ifdef OD_WIFI_UPDATE
  WifiUpdate::begin("encoder-link-v1-4enc");
#endif
  Serial.println("Genesis quad encoder bench v3; 115200 baud"); status();
}
void loop() {
  static char line[384]; static uint16_t used=0; static bool tooLong=false;
  while(Serial.available()) {
    char c=Serial.read(); if(c=='\r')continue;
    if(c=='\n') { if(!tooLong) {line[used]=0;command(line);} else Serial.println("Command too long."); used=0;tooLong=false; }
    else if(used<sizeof(line)-1)line[used++]=c; else tooLong=true;
  }
  // Limit each pass so flooding raw output cannot starve button polling.
  for(unsigned n=0;n<32;++n) {
    Edge edge; bool have=false;
    portENTER_CRITICAL(&mux);
    if(tail!=head) { edge.us=edges[tail].us; edge.pin=edges[tail].pin; edge.level=edges[tail].level; tail=(tail+1)%capacity; have=true; }
    portEXIT_CRITICAL(&mux);
    if(!have)break;
    if(discovery)Serial.printf("EDGE us=%lu port=%u IO%u gpio=%u level=%u\n",(unsigned long)edge.us,ports[edge.pin/3],edge.pin%3,pins[edge.pin],edge.level);
  }
  ScreenLink::tick();
  const uint32_t now=millis();
  for(uint8_t e=0;e<encoderCount;++e) if(buttons[e].update(digitalRead(pins[e*3+bt[e]]),now,debounceMs)) {
    ScreenLink::event(e+1,0,buttons[e].stable==activeHigh[e]?1:2);
    Serial.printf("BUTTON ms=%lu encoder=%u event=%s raw=%u\n",(unsigned long)now,e+1,buttons[e].stable==activeHigh[e]?"PRESS":"RELEASE",buttons[e].stable);
  }
  if(uint32_t(now-lastReport)>=50) {
    lastReport=now; int32_t c[encoderCount]; uint32_t bad[encoderCount],lost;
    portENTER_CRITICAL(&mux);
    for(uint8_t e=0;e<encoderCount;++e) {c[e]=counts[e];bad[e]=invalid[e];} lost=dropped;
    portEXIT_CRITICAL(&mux);
    for(uint8_t e=0;e<encoderCount;++e) if(c[e]!=reported[e] || bad[e]!=reportedInvalid[e]) {
      Serial.printf("COUNT encoder=%u delta=%ld total=%ld invalid=%lu\n",e+1,(long)(c[e]-reported[e]),(long)c[e],(unsigned long)bad[e]);
      if(c[e]!=reported[e])ScreenLink::event(e+1,c[e]-reported[e],0);
      reported[e]=c[e];reportedInvalid[e]=bad[e];
    }
    if(lost!=lastDrop) { Serial.printf("OVERFLOW dropped=%lu TRIAL INVALID: repeat slower\n",(unsigned long)lost); lastDrop=lost; }
  }
}
