#include <Arduino.h>
#include <string>
#include <mutex>
#include <list>
#include "FS.h"
#include "SPIFFS.h"
#include "debug.hpp"
void WifiMeshTask( void * parameter);
void BLESettingTask( void * parameter);
#define GPS_ Serial1
#define UWB_ Serial2
void initUWB(void);

static const int GPS_TX_PIN = 27; 
static const int GPS_RX_PIN = 26; 
//static const int UWB_TX_PIN = 16; 
//static const int UWB_RX_PIN = 17; 
static const int UWB_TX_PIN = 18; 
static const int UWB_RX_PIN = 19; 

void loadAddressConfig(void);
void loadWifiMeshConfig(void);
void loadUWBConfig(void);

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("UWB GPS Anchor start");
  GPS_.begin(115200,SERIAL_8N1,GPS_RX_PIN,GPS_TX_PIN);
  UWB_.begin(115200,SERIAL_8N1,UWB_RX_PIN,UWB_TX_PIN);

  Serial.println(__DATE__);
  Serial.println(__TIME__);

  SPIFFS.begin(true);
  loadAddressConfig();
  loadWifiMeshConfig();
  loadUWBConfig();
  SPIFFS.end();

  xTaskCreatePinnedToCore(WifiMeshTask, "WifiMeshTask", 1024*8, nullptr, 1, nullptr,  0); 
  xTaskCreatePinnedToCore(BLESettingTask, "BLESettingTask", 1024*8, nullptr, 1, nullptr,  0); 

  delay(5000);
  initUWB();
}



static const int iConstGPSLineMax = 1024;
static std::string gpsLine;
std::mutex gGpsLineMtx;
std::list <std::string> gGPSLineBuff;
static const int iConstUWBLineMax = 128;
static std::string uwbLine;
std::mutex gUWBLineMtx;
std::list <std::string> gUWBLineBuff;

static const int iConstLineBuffMax = 8;


//#define UART_DIR_GPS 1
#define UART_DIR_UWB 1


std::mutex gUWBAtCmdMtx;
std::list <std::string> gUWBAtCmdBuff;
void runUWBAtCommand(const std::string&At) {
  std::lock_guard<std::mutex> lock(gUWBAtCmdMtx);
  gUWBAtCmdBuff.push_back(At);
}


void loop() {
  // put your main code here, to run repeatedly:
  if(GPS_.available()) {
    auto ch = GPS_.read();
#ifdef UART_DIR_GPS
    Serial.write(ch);
#endif
    gpsLine.push_back(ch);
    if(gpsLine.size() > iConstGPSLineMax) {
      gpsLine.clear();
    }
    if(ch == '\n') {
      std::lock_guard<std::mutex> lock(gGpsLineMtx);
      DUMP_I(gGPSLineBuff.size());
      if(gGPSLineBuff.size() >= iConstLineBuffMax) {
        gGPSLineBuff.clear();
      }
      gGPSLineBuff.push_back(gpsLine);
      gpsLine.clear();
    }
  }
  if(UWB_.available()) {
    auto ch = UWB_.read();
#ifdef UART_DIR_UWB
    Serial.write(ch);
#endif
    {
      std::lock_guard<std::mutex> lock(gUWBLineMtx);
      uwbLine.push_back(ch);
      if(uwbLine.size() > iConstUWBLineMax) {
        uwbLine.clear();
      }
    }
    if(ch == '\n') {
      std::lock_guard<std::mutex> lock(gUWBLineMtx);
      DUMP_I(gUWBLineBuff.size());
      if(gUWBLineBuff.size() >= iConstLineBuffMax) {
        gUWBLineBuff.clear();
      }
      gUWBLineBuff.push_back(uwbLine);
      uwbLine.clear();   
    }
  }
  {
    std::lock_guard<std::mutex> lock(gUWBAtCmdMtx);
    if(gUWBAtCmdBuff.empty() == false) {
      auto at = gUWBAtCmdBuff.front();
      LOG_S(at);
      UWB_.write(at.c_str(),at.size());
      gUWBAtCmdBuff.pop_front();
    }
  }
}
std::mutex print_mtx_;
