#include <Arduino.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "debug.hpp"
#include "pref.hpp"

static Preferences preferences;

static int gUWBMode;
static int gUWBId;
void runUWBAtCommand(const std::string&At);

#define UWB_ Serial2
void initUWB(void) {
  auto goodPref = preferences.begin(preferencesZone);
  LOG_I(goodPref);
  gUWBMode = preferences.getInt(strConstUWBModeKey);
  LOG_I(gUWBMode);
  gUWBId = preferences.getInt(strConstUWBIdKey);
  LOG_I(gUWBId);
  preferences.end();
  std::string mode("AT+anchor_tag=");
  mode += std::to_string(gUWBMode);
  mode += ",";
  mode += std::to_string(gUWBId);
  mode += "\r\n";
  LOG_S(mode);
  UWB_.print(mode.c_str());
  delay(5000);
  if(gUWBMode == 0) {
    UWB_.print("AT+interval=1\r\n");
    delay(2000);
    UWB_.print("AT+switchdis=1\r\n");
  } else {
    UWB_.print("AT+switchdis=0\r\n");
  }
}
