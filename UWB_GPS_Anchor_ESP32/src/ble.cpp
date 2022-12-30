#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

#include <ArduinoJson.h>
#include <Preferences.h>

#include <sstream>
#include <string>
#include <mutex>
#include <list>
#include "debug.hpp"
#include "pref.hpp"

BLEServer *pServer = NULL;
BLECharacteristic * pTxCharacteristic;
bool deviceConnected = false;
bool oldDeviceConnected = false;

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
      LOG_I(deviceConnected);
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
      LOG_I(deviceConnected);
    }
};

static Preferences preferences;

extern bool isPreferenceAllow;
static void savePref(const char * key,const std::string &value){
  LOG_SC(key);
  LOG_S(value);
  int waitPressCounter = 10;
  while(waitPressCounter -- > 0) {
    if(isPreferenceAllow){
      LOG_I(isPreferenceAllow);
      auto retPref = preferences.putString(key,value.c_str());
      LOG_I(retPref);
      return;
    }
    delay(1000);
  }
}
static void savePref(const char * key,int32_t value){
  LOG_SC(key);
  LOG_I(value);
  int waitPressCounter = 10;
  while(waitPressCounter -- > 0) {
    if(isPreferenceAllow){
      LOG_I(isPreferenceAllow);
      auto retPref = preferences.putInt(key,value);
      LOG_I(retPref);
      return;
    }
    delay(1000);
  }
}


static void onSettingMqttTopic(const JsonObject &topic) {
  if(topic.containsKey("out")) {
    std::string outStr;
    serializeJson(topic["out"], outStr);
    savePref(strConstMqttTopicOutKey,outStr);
  }
}

static void onSettingCommand(const JsonObject &setting) {
  preferences.begin(preferencesZone);
  if(setting.containsKey("wifi")) {
    auto wifi = setting["wifi"];
    std::string ssid;
    if(wifi.containsKey("ssid")) {
      ssid = wifi["ssid"].as<std::string>();
    }
    std::string password;
    if(wifi.containsKey("password")) {
      password = wifi["password"].as<std::string>();
    }
    if(ssid.empty() == false && password.empty() == false) {
      savePref(strConstWifiSsidKey,ssid);
      savePref(strConstWifiPasswordKey,password);
    }
  }
  if(setting.containsKey("mqtt")) {
    auto mqtt = setting["mqtt"];
    if(mqtt.containsKey("url")) {
      auto url = mqtt["url"].as<std::string>();
      savePref(strConstMqttURLKey,url);
    }
    if(mqtt.containsKey("jwt")) {
      auto jwt = mqtt["jwt"].as<std::string>();
      savePref(strConstMqttJWTKey,jwt);
    }   
  }
  if(setting.containsKey("topic")) {
    auto topic = setting["topic"].as<JsonObject>();
    onSettingMqttTopic(topic);
  }

  if(setting.containsKey("mqtt_")) {
    auto mqtt = setting["mqtt_"];
    if(mqtt.containsKey("url")) {
      auto url = mqtt["url"];
      if(url.containsKey("host")) {
        auto host = url["host"].as<std::string>();
        savePref(strConstMqttURLHostKey,host);
      }
      if(url.containsKey("port")) {
        auto port = url["port"].as<int32_t>();
        savePref(strConstMqttURLPortKey,port);
      }
      if(url.containsKey("path")) {
        auto path = url["path"].as<std::string>();
        savePref(strConstMqttURLPathKey,path);
      }
    }
    if(mqtt.containsKey("jwt")) {
      auto jwt = mqtt["jwt"];
      if(jwt.containsKey("host")) {
        auto host = jwt["host"].as<std::string>();
        savePref(strConstMqttJWTHostKey,host);
      }
      if(jwt.containsKey("port")) {
        auto port = jwt["port"].as<int32_t>();
        savePref(strConstMqttJWTPortKey,port);
      }
      if(jwt.containsKey("path")) {
        auto path = jwt["path"].as<std::string>();
        savePref(strConstMqttJWTPathKey,path);
      }
    }
  }
  if(setting.containsKey("uwb")) {
    auto uwb = setting["uwb"];
    if(uwb.containsKey("mode")) {
      auto mode = uwb["mode"].as<int32_t>();
      savePref(strConstUWBModeKey,mode);
    }
    if(uwb.containsKey("id")) {
      auto id = uwb["id"].as<int32_t>();
      savePref(strConstUWBIdKey,id);
    }
  }
  preferences.end();
}

static void onInfoCommand(void) {
  auto goodPref = preferences.begin(preferencesZone);
  LOG_I(goodPref);
  auto ssid = preferences.getString(strConstWifiSsidKey);
  auto password = preferences.getString(strConstWifiPasswordKey);
  auto url = preferences.getString(strConstMqttURLKey);
  auto jwt = preferences.getString(strConstMqttJWTKey);
  LOG_S(jwt);
  auto topicOut = preferences.getString(strConstMqttTopicOutKey);

  auto uwbMode = preferences.getInt(strConstUWBModeKey);
  auto uwbId = preferences.getInt(strConstUWBIdKey);

  auto address = preferences.getString(strConstEdtokenAddressKey);
  preferences.end();
  {
    static StaticJsonDocument<512> doc;
    doc.clear();
    doc["info"]["wifi"]["ssid"] = ssid;
    doc["info"]["wifi"]["password"] = password;
    std::string outStr;
    serializeJson(doc, outStr);
    outStr += "\r\n";
    pTxCharacteristic->setValue(outStr);
    pTxCharacteristic->notify();
  }

  {
    static StaticJsonDocument<512> doc;
    doc.clear();
    doc["info"]["mqtt"]["address"] = address;
    doc["info"]["mqtt"]["url"] = url;
    doc["info"]["mqtt"]["jwt"] = jwt;
    std::string outStr;
    serializeJson(doc, outStr);
    outStr += "\r\n";
    pTxCharacteristic->setValue(outStr);
    pTxCharacteristic->notify();
  }

  static StaticJsonDocument<256> topic;
  DeserializationError error = deserializeJson(topic, topicOut);
  LOG_S(error);
  if(error == DeserializationError::Ok) {
    static StaticJsonDocument<512> doc;
    doc.clear();
    doc["info"]["topic"]["out"] = topic;
    std::string outStr;
    serializeJson(doc, outStr);
    outStr += "\r\n";
    pTxCharacteristic->setValue(outStr);
    pTxCharacteristic->notify();
  }

  {
    static StaticJsonDocument<512> doc;
    doc.clear();
    doc["info"]["uwb"]["mode"] = uwbMode;
    doc["info"]["uwb"]["id"] = uwbId;
    std::string outStr;
    serializeJson(doc, outStr);
    outStr += "\r\n";
    pTxCharacteristic->setValue(outStr);
    pTxCharacteristic->notify();
  }


}

void runUWBAtCommand(const std::string &At);
void onExternalCommand(StaticJsonDocument<512> &doc) {
  if(doc.containsKey("uwb")) {
    auto uwb = doc["uwb"];
    if(uwb.containsKey("AT")) {
      auto atCmd = uwb["AT"].as<std::string>();
      LOG_S(atCmd);
      runUWBAtCommand(atCmd);
    }
  }
  if(doc.containsKey("setting")) {
    auto setting = doc["setting"].as<JsonObject>();
    onSettingCommand(setting);
  }
  if(doc.containsKey("info")) {
    onInfoCommand();
  }
}

StaticJsonDocument<512> bleInputdoc;
class MyCallbacks: public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pCharacteristic) {
    //pCharacteristic->setValue("Hello World!");
    std::string value = pCharacteristic->getValue();
  }

  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string value = pCharacteristic->getValue();
    LOG_S(value);
    DeserializationError error = deserializeJson(bleInputdoc, value);
    LOG_S(error);
    if(error == DeserializationError::Ok) {
      onExternalCommand(bleInputdoc);
    }
  }
};


#define SERVICE_UUID           "6E400001-B5A3-F393-E0A9-E50E24DCCA9E" // UART service UUID
#define CHARACTERISTIC_UUID_TX "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define CHARACTERISTIC_UUID_RX "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"


void setupBLE(void) {
  BLEDevice::init("UWB_GPS_Anchor ESP32");  // local name
  pServer = BLEDevice::createServer();  // Create the BLE Device
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pTxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_TX,BLECharacteristic::PROPERTY_NOTIFY);
  pTxCharacteristic->addDescriptor(new BLE2902());

  BLECharacteristic * pRxCharacteristic = pService->createCharacteristic(CHARACTERISTIC_UUID_RX,BLECharacteristic::PROPERTY_WRITE										);
  pRxCharacteristic->setCallbacks(new MyCallbacks());

  pService->start();
  pServer->getAdvertising()->start();

  Serial.println("Waiting a client connection to notify...");
}

std::string my_to_string(int i) {
  std::stringstream ss;
  ss << i;
  return ss.str();
}

std::string my_to_string_f(float f) {
  std::stringstream ss;
  ss << f;
  return ss.str();
}

extern std::mutex gGpsLineMtx;
extern std::list <std::string> gGPSLineBuff;
void reportGPS(void)
{
  std::lock_guard<std::mutex> lock(gGpsLineMtx);
  if(gGPSLineBuff.empty() == false) {
    auto line = gGPSLineBuff.front();
    pTxCharacteristic->setValue(line);
    pTxCharacteristic->notify();
    gGPSLineBuff.pop_front();
  }
}
void discardGPS(void) {
  std::lock_guard<std::mutex> lock(gGpsLineMtx);
  if(gGPSLineBuff.empty() == false) {
    gGPSLineBuff.clear();
  }  
}

extern std::mutex gUWBLineMtx;
extern std::list <std::string> gUWBLineBuff;

void reportUWB(void)
{
  std::lock_guard<std::mutex> lock(gUWBLineMtx);
  if(gUWBLineBuff.empty() == false) {
    auto line = gUWBLineBuff.front();
    pTxCharacteristic->setValue(line);
    pTxCharacteristic->notify();
    gUWBLineBuff.pop_front();
  }
}
void discardUWB(void) {
  std::lock_guard<std::mutex> lock(gUWBLineMtx);
  if(gUWBLineBuff.empty() == false) {
    gUWBLineBuff.clear();
  }
}


void runBleTransimit(void)
{
  if (deviceConnected) {
    //reportGPS();
    //reportUWB();
    //discardGPS();
    //discardUWB();
  } else {
    //discardGPS();
    //discardUWB();
  }
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
  // do stuff here on connecting
      oldDeviceConnected = deviceConnected;
  }
}

void BLETask( void * parameter) {
  int core = xPortGetCoreID();
  LOG_I(core);
  setupBLE();
  for(;;) {//
    runBleTransimit();
    delay(1);
  }
}
