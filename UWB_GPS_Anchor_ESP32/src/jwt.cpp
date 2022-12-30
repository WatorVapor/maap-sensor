#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebSocketsClient.h>

#include "mbedtls/sha1.h"
#include "mybase64.hpp"
#include "base32/Base32.h"
extern "C" {
  #include <tweetnacl.h>
}
#include "debug.hpp"
#include "pref.hpp"

static Preferences preferences;
static WebSocketsClient webSocket;

static String createJWTRequest(const std::string &ts);
static String createDateRequest(void);
std::string gDateOfSign;
std::string gMqttJWTToken;
void onWSMsg(const StaticJsonDocument<512> &doc) {
  if(doc.containsKey("date")) {
    std::string dateStr = doc["date"].as<std::string>();
    gDateOfSign = dateStr;
    auto jwtReq = createJWTRequest(dateStr);
    LOG_S(jwtReq);
    webSocket.sendTXT(jwtReq);
  }
  if(doc.containsKey("jwt")) {
    gMqttJWTToken = doc["jwt"].as<std::string>();
    LOG_S(gMqttJWTToken);
  }
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
  case WStype_DISCONNECTED:
    {
      LOG_I(WStype_DISCONNECTED);
    }
    break;
  case WStype_CONNECTED:
    {
      LOG_I(WStype_CONNECTED);
      auto token = createDateRequest();
      webSocket.sendTXT(token);
    }
    break;
  case WStype_TEXT:
    {
      LOG_SC((char*)payload);
      LOG_I(length);
      std::string payloadStr((char*)payload,length);
      LOG_S(payloadStr);
      static StaticJsonDocument<512> doc;
      doc.clear();
      DeserializationError error = deserializeJson(doc, payloadStr);
      LOG_S(error);
      if(error == DeserializationError::Ok) {
        onWSMsg(doc);
      }
    }
    break;
  case WStype_BIN:
    break;
  case WStype_ERROR:			
  case WStype_FRAGMENT_TEXT_START:
  case WStype_FRAGMENT_BIN_START:
  case WStype_FRAGMENT:
  case WStype_FRAGMENT_FIN:
    break;
  }
}

StaticJsonDocument<512> signMsg(StaticJsonDocument<512> &msg,const std::string &ts);

String gAddress;
String gSecretKey;
byte gSecretKeyBin[crypto_sign_SECRETKEYBYTES] = {0};
String gPublicKey;


static String createJWTRequest(const std::string &ts) {
  static StaticJsonDocument<512> doc;
  doc.clear();
  doc["jwt"]["add"] = gAddress;
  doc["jwt"]["min"] = true;
  auto signedRequest = signMsg(doc,ts);
  String request;
  serializeJson(signedRequest, request);
  return request;
}

static String createDateRequest(void) {
  static StaticJsonDocument<256> doc;
  doc.clear();
  doc["date"] = "req";
  String request;
  serializeJson(doc, request);
  return request;
}

void connectJWT(void) {
  int core = xPortGetCoreID();
  LOG_I(core);
  auto goodPref = preferences.begin(preferencesZone);
  LOG_I(goodPref);
  auto jwt_url = preferences.getString(strConstMqttJWTKey);
  LOG_S(jwt_url);
  auto jwt_host = preferences.getString(strConstMqttJWTHostKey);
  LOG_S(jwt_host);
  auto jwt_port = preferences.getInt(strConstMqttJWTPortKey);
  LOG_I(jwt_port);
  auto jwt_path = preferences.getString(strConstMqttJWTPathKey);
  LOG_S(jwt_path);

  gAddress = preferences.getString(strConstEdtokenAddressKey);
  LOG_S(gAddress);
  gPublicKey = preferences.getString(strConstEdtokenPublicKey);
  LOG_S(gPublicKey);
  gSecretKey = preferences.getString(strConstEdtokenSecretKey);
  LOG_S(gSecretKey);
  preferences.end();

  int b64Ret2 = decode_base64((byte*)(gSecretKey.c_str()),gSecretKey.length(),gSecretKeyBin);
  LOG_H(gSecretKeyBin,sizeof(gSecretKeyBin));

  webSocket.begin(jwt_host.c_str(),(uint16_t)jwt_port,jwt_path.c_str());
  webSocket.onEvent(webSocketEvent);
}
void loopJWT(void) {
    webSocket.loop();  
}
