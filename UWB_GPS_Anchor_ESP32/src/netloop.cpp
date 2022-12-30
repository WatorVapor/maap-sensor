#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <list>
#include <WiFi.h>
#include <PubSubClient.h>

#include <Preferences.h>

#include "mbedtls/sha1.h"
#include "mybase64.hpp"
#include "base32/Base32.h"
extern "C" {
  #include <tweetnacl.h>
}
#include "debug.hpp"
#include "pref.hpp"




void setupMQTT(void);

void connectMqtt(void);
void loopMqtt(void); 

void connectJWT(void);
void loopJWT(void); 

void NetWorkTask( void * parameter){
  int core = xPortGetCoreID();
  LOG_I(core);
  setupMQTT();
  connectMqtt();
  connectJWT();

  for(;;) {//
    loopMqtt();
    loopJWT();
    delay(1);
  }
}




