#include <Arduino.h>
#include <ArduinoJson.h>
#include "debug.hpp"
#include "FS.h"
#include "SPIFFS.h"


int gUWBMode;
int gUWBId;
void runUWBAtCommand(const std::string&At);


static const std::string Prefix("/spiffs");
static StaticJsonDocument<512> uwbSavedoc;
static StaticJsonDocument<512> uwbReaddoc;
static std::string saveJsonBuff;

void loadUWBConfig(void) {
  std::string address = Prefix + "/config.uwb.json";
  auto isExists =  SPIFFS.exists(address.c_str());
  if(isExists) {
    File fp = SPIFFS.open(address.c_str(),FILE_READ);
    if(fp){
      const int fileSize = fp.size();
      LOG_I(fileSize);
      if(fileSize < 512) {
        char buff[fileSize];
        auto readSize = fp.readBytes(buff,fileSize);
        LOG_I(readSize);
        uwbReaddoc.clear();
        DeserializationError error = deserializeJson(uwbReaddoc, buff);
        LOG_I(error);
        if(error == DeserializationError::Ok) {
          if(uwbReaddoc.containsKey("mode")) {
            gUWBMode = uwbReaddoc["mode"].as<int>();
          }
          if(uwbReaddoc.containsKey("id")) {
            gUWBId = uwbReaddoc["id"].as<int>();
          }
        }
      }
    }
  } else {
    uwbSavedoc.clear();
    uwbSavedoc["mode"] = 1;
    uwbSavedoc["id"] = 0;
    saveJsonBuff.clear();
    serializeJson(uwbSavedoc, saveJsonBuff);
    LOG_S(saveJsonBuff);
    auto fs = SPIFFS.open(address.c_str(),FILE_WRITE);
    if(fs.available()) {
      fs.print(saveJsonBuff.c_str());
      fs.flush();
      fs.close();
    }
    gUWBMode = 1;
    gUWBId = 10;
  }
  LOG_I(gUWBMode);
  LOG_I(gUWBId);
}
void storeUWBConfig(void) {
  SPIFFS.begin(true);
  std::string address = Prefix + "/config.uwb.json";
  uwbSavedoc.clear();
  uwbSavedoc["mode"] = gUWBMode;
  uwbSavedoc["id"] = gUWBId;
  saveJsonBuff.clear();
  serializeJson(uwbSavedoc, saveJsonBuff);
  LOG_S(saveJsonBuff);
  auto fs = SPIFFS.open(address.c_str(),FILE_WRITE);
  LOG_I(fs.available());
  if(fs.available()) {
    fs.print(saveJsonBuff.c_str());
    fs.flush();
    fs.close();
  } else {
    LOG_SC(fs.name());
  }
  SPIFFS.end();
}


#define UWB_ Serial2
#define UART_DIR_UWB 1


static void readOutUWB(void) {
  while(UWB_.available()) {
    auto ch = UWB_.read();
#ifdef UART_DIR_UWB
    Serial.write(ch);
#endif
  }
}

void initUWB(void) {
  {
    std::string cmd("AT+RST\r\n");
    LOG_S(cmd);
    UWB_.print(cmd.c_str());
    delay(1000);
    readOutUWB();
  }
  {
    std::string cmd("AT+version?\r\n");
    LOG_S(cmd);
    UWB_.print(cmd.c_str());
    delay(1000);
    readOutUWB();
  }
  {
    std::string mode("AT+anchor_tag=");
    mode += std::to_string(gUWBMode);
    mode += ",";
    mode += std::to_string(gUWBId);
    mode += "\r\n";
    LOG_S(mode);
    UWB_.print(mode.c_str());
    delay(1000);
    readOutUWB();
  }


  if(gUWBMode == 0) {
    std::string cmd("AT+interval=5\r\n");
    LOG_S(cmd);
    UWB_.print(cmd.c_str());
    delay(1000);
    readOutUWB();
    std::string cmd2("AT+switchdis=1\r\n");
    LOG_S(cmd2);
    UWB_.print(cmd2.c_str());
  } else {
    std::string cmd2("AT+switchdis=0\r\n");
    LOG_S(cmd2);
    UWB_.print(cmd2.c_str());
  }
  readOutUWB();
}
