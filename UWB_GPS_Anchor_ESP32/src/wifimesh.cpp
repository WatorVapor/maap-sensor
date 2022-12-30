#include <Arduino.h>
#include <ArduinoJson.h>
#include <mutex>
#include <string>
#include "painlessMesh.h"
#include "debug.hpp"


static String mesh_prefix;
static String mesh_password;
static int16_t mesh_port;

static Scheduler userScheduler;
static painlessMesh  mesh;
static void sendMessage(void);
static Task taskSendMessage( TASK_MILLISECOND * 10 , TASK_FOREVER, &sendMessage );
static String serial2MeshBuff;
static std::mutex serial2MeshBuffMtx;
static const size_t iConstBufferMax = 512;
static StaticJsonDocument<512> mesh2SerialDoc;
static const size_t iConstMesh2SerialBufferMax = 512 -64;

void sendMessage(void) {
  String msg;
  {
    std::lock_guard<std::mutex> lock(serial2MeshBuffMtx);
    if(serial2MeshBuff.isEmpty()) {
      return;
    } else {
      msg = serial2MeshBuff;
      serial2MeshBuff.clear();
    }
  }
  mesh.sendBroadcast( msg );
  taskSendMessage.setInterval( random( TASK_MILLISECOND * 1, TASK_MILLISECOND * 5 ));
}

void receivedCallback( uint32_t from, String &msg ) {
  mesh2SerialDoc.clear();
  mesh2SerialDoc["from"] = from;
  if(msg.length() < iConstMesh2SerialBufferMax) {
    mesh2SerialDoc["recv"] = msg;
  } else {
    String shortedMsg(msg.c_str(),iConstMesh2SerialBufferMax);
    shortedMsg += "...";
    mesh2SerialDoc["recv"] = shortedMsg;
    mesh2SerialDoc["error"] = "over";
  }
  std::string outStr;
  serializeJson(mesh2SerialDoc, outStr);
  outStr += "\r\n";
  Serial.print(outStr.c_str());
}

void newConnectionCallback(uint32_t nodeId) {
  mesh2SerialDoc.clear();
  mesh2SerialDoc["connect"] = "new";
  mesh2SerialDoc["nodeId"] = nodeId;
  std::string outStr;
  serializeJson(mesh2SerialDoc, outStr);
  outStr += "\r\n";
  Serial.print(outStr.c_str());
}

void changedConnectionCallback() {
  mesh2SerialDoc.clear();
  mesh2SerialDoc["connect"] = "changed";
  std::string outStr;
  serializeJson(mesh2SerialDoc, outStr);
  outStr += "\r\n";
  Serial.print(outStr.c_str());
}

void nodeTimeAdjustedCallback(int32_t offset) {
  mesh2SerialDoc.clear();
  mesh2SerialDoc["connect"] = "Adjusted time";
  mesh2SerialDoc["nodeTime"] = mesh.getNodeTime();
  mesh2SerialDoc["offset"] = offset;
  std::string outStr;
  serializeJson(mesh2SerialDoc, outStr);
  outStr += "\r\n";
  Serial.print(outStr.c_str());
}

void setup_wifi_mesh(void) {
  mesh.setDebugMsgTypes( ERROR );
  mesh.init( mesh_prefix, mesh_password, &userScheduler, mesh_port );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable();
}


static const std::string Prefix("/spiffs");
static StaticJsonDocument<512> meshSavedoc;
static StaticJsonDocument<512> meshReaddoc;
#define   MESH_PREFIX     "pYZviUVDQrjaNsD5"
#define   MESH_PASSWORD   "Fqji4aPp"
#define   MESH_PORT       5555

void loadConfig(void) {
  SPIFFS.begin(true);
  std::string settings = Prefix + "/config.wifi.mesh.json";
  auto isExists =  SPIFFS.exists(settings.c_str());
  if(isExists) {
    File fp = SPIFFS.open(settings.c_str(),FILE_READ);
    if(fp){
      const int fileSize = fp.size();
      LOG_I(fileSize);
      if(fileSize < 512) {
        char buff[fileSize];
        auto readSize = fp.readBytes(buff,fileSize);
        LOG_I(readSize);
        meshReaddoc.clear();
        DeserializationError error = deserializeJson(meshReaddoc, buff);
        LOG_I(error);
        if(error == DeserializationError::Ok) {
          if(meshReaddoc.containsKey("ssid")) {
            mesh_prefix = meshReaddoc["ssid"].as<String>();
          }
          if(meshReaddoc.containsKey("password")) {
            mesh_password = meshReaddoc["password"].as<String>();
          }
          if(meshReaddoc.containsKey("port")) {
           mesh_port = meshReaddoc["port"].as<int>();
          }
        }
      }
    }
  } else {
    meshSavedoc.clear();
    meshSavedoc["ssid"] = MESH_PREFIX;
    meshSavedoc["password"] = MESH_PASSWORD;
    meshSavedoc["port"] = MESH_PORT;
    std::string saveStr;
    serializeJson(meshSavedoc, saveStr);
    LOG_S(saveStr);
    auto fs = SPIFFS.open(settings.c_str(),FILE_WRITE);
    if(fs.available()) {
      fs.print(saveStr.c_str());
      fs.flush();
      fs.close();
    }
    mesh_prefix = MESH_PREFIX;
    mesh_password = MESH_PASSWORD;
    mesh_port = MESH_PORT;
  }
  SPIFFS.end();
  LOG_S(mesh_prefix);
  LOG_S(mesh_password);
  LOG_I(mesh_port);
}

void WifiMeshTask( void * parameter) {
  loadConfig();
  setup_wifi_mesh();
  while(true) {
    mesh.update();
    if (Serial.available()) {
      int inByte = Serial.read();
      String strByte((char)inByte);
      {
        std::lock_guard<std::mutex> lock(serial2MeshBuffMtx);
        serial2MeshBuff += strByte;
        if(serial2MeshBuff.length() > iConstBufferMax) {
          serial2MeshBuff.clear();
        }
      }      
    }
    delay(10);
  }
}

