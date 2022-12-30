#include <Arduino.h>
#include <ArduinoJson.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>
#include "debug.hpp"

static BLEServer *pServer = NULL;
static BLECharacteristic * pTxCharacteristic;
static bool deviceConnected = false;
static bool oldDeviceConnected = false;

static StaticJsonDocument<512> bleInputdoc;
static StaticJsonDocument<512> bleOutputdoc;

static const size_t iConstBLEMaxBytes = 256;


static std::string gOutBleStrBuff;
static std::mutex gOutBleStrBuffMtx;

static void sendOutBle(void) {
  std::string outStr;
  {
    std::lock_guard<std::mutex> lock(gOutBleStrBuffMtx);
    if(gOutBleStrBuff.length() > 0) {
      outStr = gOutBleStrBuff;
      gOutBleStrBuff.clear();
    }
  }
  if(outStr.length() == 0) {
    return;
  }

  const auto totalLen = outStr.length();
  for(int index = 0;index < totalLen;index += iConstBLEMaxBytes) {
    size_t piece_len = iConstBLEMaxBytes;
    if(totalLen - index < iConstBLEMaxBytes ){
      piece_len = totalLen - index ;
    }
    std::string piece = outStr.substr(index, piece_len);
    pTxCharacteristic->setValue(piece);
    pTxCharacteristic->notify();
    delay(100);
  }
}


void readSettingInfo(StaticJsonDocument<512> &doc) {
  if(doc.containsKey("motor")) {
    auto motor = doc["motor"].as<int>();
    LOG_I(motor);
    bleOutputdoc.clear();
    bleOutputdoc["motor"] = motor;
    std::string outStr;
    serializeJson(bleOutputdoc, outStr);
    outStr += "\r\n";
    LOG_S(outStr);
    std::lock_guard<std::mutex> lock(gOutBleStrBuffMtx);
    gOutBleStrBuff = outStr;
  }
}

void settingMesh(StaticJsonDocument<512> &doc) {
  if(doc.containsKey("ssid")) {
  }
}


void onExternalCommand(StaticJsonDocument<512> &doc) {
  if(doc.containsKey("maap")) {
    auto isMaap = doc["maap"].as<bool>();
    LOG_I(isMaap);
    if(isMaap == false) {
      return;
    }
  }
  if(doc.containsKey("setting")) {
    auto settingStr = doc["setting"].as<std::string>();
    LOG_S(settingStr);
    if(settingStr == "mesh") {
      settingMesh(doc);
    }
  }
}


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
  BLEDevice::init("ESP32 KOMATU");  // local name
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


void runBleTransimit(void)
{
  if (deviceConnected) {
    sendOutBle();
  } else {
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


void BLESettingTask( void * parameter) {
  int core = xPortGetCoreID();
  LOG_I(core);
  setupBLE();
  for(;;) {//
    runBleTransimit();
    delay(1);
  }
}
