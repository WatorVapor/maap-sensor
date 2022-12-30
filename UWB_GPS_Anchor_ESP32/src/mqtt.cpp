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

/*
static const char* ssid = "mayingkuiG";
static const char* password = "xuanxuanhaohao";
static const char* mqtt = "mqtt://wator.xyz:1883";
static const char* jwt = "ws://wator.xyz:8083/jwt";
*/



static StaticJsonDocument<1024> gMattMsgDoc;


static unsigned char gBase64TempBinary[512];
static unsigned char gOpenedTempMsg[512];

static uint8_t gSignBinary[512];
static uint8_t gPublicKeyBinary[32];
static const int SHA512_HASH_BIN_MAX = 512/8;
static unsigned char gCalcTempHash[SHA512_HASH_BIN_MAX];


bool verifySign(const std::string &pub,const std::string &sign,const std::string &sha);
bool isAuthedMessage(const JsonVariant &msg,const std::string &topic,const std::string &payloadStr);

void ExecGpio(int port,int level) {
  LOG_I(port);
  LOG_I(level);
  pinMode(port,OUTPUT);
  digitalWrite(port,level);
}

void onMqttDigitalOut(const JsonVariant &d_out) {
  if(d_out.containsKey("port")) {
    int port = d_out["port"].as<int>();
    if(d_out.containsKey("level")) {
      int level = d_out["level"].as<int>();
      ExecGpio(port,level);
    }
  }
}


void onMqttAuthedMsg(const JsonVariant &payload) {
  if(payload.containsKey("d_out")) {
    onMqttDigitalOut(payload["d_out"]);
  }
}

void execMqttMsg(const std::string &msg,const std::string &topic) {
  DUMP_S(msg);
  DUMP_S(topic);
  DeserializationError error = deserializeJson(gMattMsgDoc, msg);
  DUMP_S(error);
  if(error == DeserializationError::Ok) {
    std::string payloadStr;
    if(gMattMsgDoc.containsKey("payload")) {
      payloadStr = gMattMsgDoc["payload"].as<std::string>();
      DUMP_S(payloadStr);
    }

    if(gMattMsgDoc.containsKey("auth")) {
      JsonVariant auth = gMattMsgDoc["auth"];
      bool isGood = isAuthedMessage(auth,topic,payloadStr);
      if(isGood) {
        LOG_I(isGood);
        if(gMattMsgDoc.containsKey("payload")) {
          JsonVariant payload = gMattMsgDoc["payload"];
          onMqttAuthedMsg(payload);
        }
      } else {
        LOG_I(isGood);
        LOG_S(msg);
      }
    }
  }
}

static std::map<std::string,std::string> gChannelTempMsg;

void insertTopicMsg(const std::string &msg,const std::string &topic){
    auto ir = gChannelTempMsg.find(topic);
    if(ir == gChannelTempMsg.end()) {
      gChannelTempMsg[topic] = msg;
    } else {
      ir->second += msg;
    }
}
void processOneMqttMsg(const std::string &topic){
    auto ir = gChannelTempMsg.find(topic);
    if(ir != gChannelTempMsg.end()) {
      execMqttMsg(ir->second,ir->first);
      gChannelTempMsg.erase(ir);
    }
}


void onMqttMsg(StaticJsonDocument<256> &doc,const std::string &topic ){
  if(doc.containsKey("buff")) {
    std::string buffStr = doc["buff"].as<std::string>();
    DUMP_S(buffStr);
    if(doc.containsKey("finnish")) {
      bool finnish = doc["finnish"].as<bool>();
      insertTopicMsg(buffStr,topic);
      if(finnish) {
        processOneMqttMsg(topic);
      }
    }
  }
}

void callback(char* topic, byte* payload, unsigned int length) {
  std::string topicStr(topic);
  DUMP_S(topicStr);
  std::string payloadStr((char*)payload,length);
  DUMP_S(payloadStr);
  StaticJsonDocument<256> doc;
  DeserializationError error = deserializeJson(doc, payloadStr);
  DUMP_S(error);
  if(error == DeserializationError::Ok) {
    onMqttMsg(doc,topicStr);
  }
}

static Preferences preferences;
extern bool isPreferenceAllow;

static void savePref2(const char * key,const std::string &value){
  LOG_SC(key);
  LOG_S(value);
  auto retPref = preferences.putString(key,value.c_str());
  LOG_I(retPref);
  return;
}
void miningAddress(void);

static String gAddress;
static std::list<std::string> gOutTopics;
StaticJsonDocument<512> gOutTopicsDoc;
void setupMQTT(void) {
  auto goodPref = preferences.begin(preferencesZone);
  LOG_I(goodPref);
  gAddress = preferences.getString(strConstEdtokenAddressKey);
  auto pubKeyB64 = preferences.getString(strConstEdtokenPublicKey);
  auto secKeyB64 = preferences.getString(strConstEdtokenSecretKey);

  auto ssid = preferences.getString(strConstWifiSsidKey);
  auto password = preferences.getString(strConstWifiPasswordKey);
  LOG_S(ssid);
  LOG_S(password);

  auto topicOut = preferences.getString(strConstMqttTopicOutKey);
  LOG_S(topicOut);
  DeserializationError error = deserializeJson(gOutTopicsDoc, topicOut);
  LOG_S(error);
  if(error == DeserializationError::Ok) {
    JsonArray array = gOutTopicsDoc.as<JsonArray>();
    for(auto v : array) {
      auto topic = v.as<std::string>();
      gOutTopics.push_back(topic);
    }
  }


  preferences.end();
  if(gAddress.isEmpty() || pubKeyB64.isEmpty() || secKeyB64.isEmpty()) {
    miningAddress();
    auto goodPref2 = preferences.begin(preferencesZone);
    LOG_I(goodPref2);
    gAddress = preferences.getString(strConstEdtokenAddressKey);
    pubKeyB64 = preferences.getString(strConstEdtokenPublicKey);
    secKeyB64 = preferences.getString(strConstEdtokenSecretKey);
    preferences.end();
  }
  LOG_S(gAddress);
  LOG_S(pubKeyB64);
  LOG_S(secKeyB64);


  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), password.c_str());
  while ( true ) {
    auto result = WiFi.waitForConnectResult();
    LOG_I(result);
    if(result == WL_CONNECTED){
      break;
    }
    delay(1000);
  }
  LOG_S(WiFi.localIP().toString());
  LOG_S(WiFi.localIPv6().toString());
  Serial.println("Wifi Is Ready");
}



#include <list>
extern std::string gMqttJWTToken;


void subscribeAtConnected(PubSubClient &client) {
  auto topic = gAddress + "/#";
  client.subscribe(topic.c_str(),0);
}

void reconnectMqtt(PubSubClient &client) {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "uwb-gps-anchor-";
    clientId += gAddress;
    // Attempt to connect
    LOG_S(clientId);
    auto rc = client.connect(clientId.c_str(),"",gMqttJWTToken.c_str());
    if (rc) {
      LOG_I(client.connected());
      // ... and resubscribe
      subscribeAtConnected(client);
    } else {
      LOG_I(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void JWTTask( void * parameter);

static WiFiClient espClient;
static PubSubClient mqttClient(espClient);
void runMqttTransimit(void);

static String mqtt_host;
static uint16_t mqtt_port;
static uint16_t uwb_id;

//static String mqtt_path;
void connectMqtt(void) {
  auto goodPref = preferences.begin(preferencesZone);
  mqtt_host = preferences.getString(strConstMqttURLHostKey);
  mqtt_port = preferences.getInt(strConstMqttURLPortKey);
  uwb_id = preferences.getInt(strConstUWBIdKey);
  //mqtt_path = preferences.getString(strConstMqttURLPathKey);
  preferences.end();
  LOG_S(mqtt_host);
  LOG_I(mqtt_host.length());
  LOG_I(mqtt_port);

  mqttClient.setServer(mqtt_host.c_str(), (uint16_t)mqtt_port);
  mqttClient.setCallback(callback);
  mqttClient.setBufferSize(512);
  LOG_I(mqttClient.getBufferSize());

} 
void loopMqtt(void) {
  if (!mqttClient.connected()) {
    reconnectMqtt(mqttClient);
  }
  mqttClient.loop();
  runMqttTransimit();
}
#include <sstream>
#include <string>
#include <mutex>
#include <list>
extern std::mutex gUWBLineMtx;
extern std::list <std::string> gUWBLineBuff;

extern std::string gDateOfSign;

StaticJsonDocument<512> signMsg(StaticJsonDocument<512> &msg,const std::string &ts);
static StaticJsonDocument<512> gTempMqttReportDoc;
static StaticJsonDocument<512> gTempMqttReportDocSign;

static void reportJson(void) {
  if(gDateOfSign.empty()) {
    return;
  }
  auto gTempMqttReportDocSign = signMsg(gTempMqttReportDoc,gDateOfSign);
  String report;
  serializeJson(gTempMqttReportDocSign, report);
  DUMP_S(report);
  if(mqttClient.connected()) {
    for(auto &topic : gOutTopics) {
      std::string outTopic = topic + "/uwb";
      DUMP_S(outTopic);
      auto goodPublish = mqttClient.publish_P(outTopic.c_str(),report.c_str(),report.length());
      DUMP_I(goodPublish);
    }
  }
}
static void reportUWB(void)
{
  std::lock_guard<std::mutex> lock(gUWBLineMtx);
  if(gUWBLineBuff.empty() == false) {
    gTempMqttReportDoc.clear();
    auto line = gUWBLineBuff.front();
    gTempMqttReportDoc["uwb"] = line;
    if(line.empty() == false) {
      reportJson();
    }
    gUWBLineBuff.pop_front();
  }
}
static void discardUWB(void) {
  std::lock_guard<std::mutex> lock(gUWBLineMtx);
  if(gUWBLineBuff.empty() == false) {
    gUWBLineBuff.clear();
  }
}

extern std::mutex gGpsLineMtx;
extern std::list <std::string> gGPSLineBuff;
static void reportGPS(void)
{
  std::lock_guard<std::mutex> lock(gGpsLineMtx);
  if(gGPSLineBuff.empty() == false) {
    gTempMqttReportDoc.clear();
    auto line = gGPSLineBuff.front();
    gTempMqttReportDoc["gps"]["raw"] = line;
    gTempMqttReportDoc["gps"]["id"] = uwb_id;
    if(line.find("$GNGGA,") == 0) {
      reportJson();
    }
    gGPSLineBuff.pop_front();
  }
}
static void discardGPS(void) {
  std::lock_guard<std::mutex> lock(gGpsLineMtx);
  if(gGPSLineBuff.empty() == false) {
    gGPSLineBuff.clear();
  }  
}

void runMqttTransimit(void) {
  reportUWB();
  reportGPS();
}
