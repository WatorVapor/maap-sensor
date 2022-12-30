#include <Arduino.h>
#include <ArduinoJson.h>
#include <mutex>
#include <string>
#include "painlessMesh.h"
#include "mbedtls/sha1.h"
#include "mybase64.hpp"
#include "base32/Base32.h"
extern "C" {
  #include <tweetnacl.h>
}
#include "debug.hpp"


static String mesh_prefix;
static String mesh_password;
static int16_t mesh_port;

static Scheduler userScheduler;
static painlessMesh  mesh;
/*
static void sendMessage(void);
static Task taskSendMessage( TASK_MILLISECOND * 10 , TASK_FOREVER, &sendMessage );
*/

static String device2MeshBuff;
static std::mutex device2MeshBuffMtx;
static const size_t iConstBufferMax = 512;
static StaticJsonDocument<512> mesh2DeviceDoc;
static const size_t iConstMesh2DeviceBufferMax = 512 -64;


void receivedCallback( uint32_t from, String &msg ) {
  LOG_I(from);
  LOG_S(msg);
}

void newConnectionCallback(uint32_t nodeId) {
  LOG_I(nodeId);
}

void changedConnectionCallback() {
  LOG_SC("");
}

void nodeTimeAdjustedCallback(int32_t offset) {
  LOG_I(offset);
}

void setup_wifi_mesh(void) {
  mesh.setDebugMsgTypes( ERROR );
  mesh.init( mesh_prefix, mesh_password, &userScheduler, mesh_port );
  mesh.onReceive(&receivedCallback);
  mesh.onNewConnection(&newConnectionCallback);
  mesh.onChangedConnections(&changedConnectionCallback);
  mesh.onNodeTimeAdjusted(&nodeTimeAdjustedCallback);
/*
  userScheduler.addTask( taskSendMessage );
  taskSendMessage.enable();
*/
}


static const std::string Prefix("/spiffs");
static StaticJsonDocument<512> meshSavedoc;
static StaticJsonDocument<512> meshReaddoc;


static String mesh_address;
static String mesh_pub_key;
static String mesh_sec_key;


std::string sha1Address(byte *msg,size_t length);
std::string sha1Bin(byte *msg,size_t length);


static String mining_address;
static String mining_pub_key;
static String mining_sec_key;

void miningMeshAddress(void) {
  Serial.println("Waiting mining address ...");
  byte secretKey[crypto_sign_SECRETKEYBYTES] = {0};
  byte publicKey[crypto_sign_PUBLICKEYBYTES] = {0};
  DUMP_H(secretKey,sizeof(secretKey));
  DUMP_H(publicKey,sizeof(publicKey));
  std::string address;
  while(true)
  {
    crypto_sign_keypair(publicKey,secretKey);
    LOG_H(secretKey,sizeof(secretKey));
    LOG_H(publicKey,sizeof(publicKey));
    byte mineSha512[crypto_hash_sha512_BYTES] = {0};
    auto hashRet = crypto_hash_sha512(mineSha512,publicKey,crypto_sign_PUBLICKEYBYTES);
    LOG_I(hashRet);
    LOG_H(mineSha512,sizeof(mineSha512));
    byte sha512B64[crypto_hash_sha512_BYTES*2] = {0};
    int b64Ret1 = my_encode_base64(mineSha512,crypto_hash_sha512_BYTES,sha512B64);
    LOG_I(b64Ret1);
    std::string sha512B64Str((char*)sha512B64,b64Ret1);
    LOG_S(sha512B64Str);
    address = sha1Address((byte*)sha512B64Str.c_str(),sha512B64Str.size());
    LOG_S(address);
/*
    break;
    if(address.at(0) == 'm') {
      break;
    }
*/
    if(address.at(0) == 'm' && address.at(1) == 'p' ) {
      break;
    }
    if(address.at(0) == 'm' && address.at(1) == 'a' && address.at(2) == 'a' && address.at(3) == 'p' ) {
      break;
    }
  }
  byte publicKeyB64[crypto_sign_PUBLICKEYBYTES*2] = {0};
  int b64Ret = my_encode_base64(publicKey,crypto_sign_PUBLICKEYBYTES,publicKeyB64);
  LOG_I(b64Ret);
  std::string pubKeyB64((char*)publicKeyB64,b64Ret);
  LOG_S(pubKeyB64);
  
  LOG_H(secretKey,sizeof(secretKey));
  byte secretKeyB64[crypto_sign_SECRETKEYBYTES*2] = {0};
  int b64Ret2 = my_encode_base64(secretKey,crypto_sign_SECRETKEYBYTES,secretKeyB64);
  LOG_I(b64Ret2);
  std::string secKeyB64((char*)secretKeyB64,b64Ret2);
  LOG_S(secKeyB64);
  mining_address = String(address.c_str());
  mining_pub_key = String(pubKeyB64.c_str());
  mining_sec_key = String(secKeyB64.c_str());
}


void loadAddressConfig(void) {
  SPIFFS.begin(true);
  std::string address = Prefix + "/config.address.json";
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
        meshReaddoc.clear();
        DeserializationError error = deserializeJson(meshReaddoc, buff);
        LOG_I(error);
        if(error == DeserializationError::Ok) {
          if(meshReaddoc.containsKey("address")) {
            mesh_address = meshReaddoc["address"].as<String>();
          }
          if(meshReaddoc.containsKey("pub")) {
            mesh_pub_key = meshReaddoc["pub"].as<String>();
          }
          if(meshReaddoc.containsKey("sec")) {
           mesh_sec_key = meshReaddoc["sec"].as<String>();
          }
        }
      }
    }
  } else {
    miningMeshAddress();
    meshSavedoc.clear();
    meshSavedoc["address"] = mining_address;
    meshSavedoc["pub"] = mining_pub_key;
    meshSavedoc["sec"] = mining_sec_key;
    std::string saveStr;
    serializeJson(meshSavedoc, saveStr);
    LOG_S(saveStr);
    auto fs = SPIFFS.open(address.c_str(),FILE_WRITE);
    if(fs.available()) {
      fs.print(saveStr.c_str());
      fs.flush();
      fs.close();
    }
    mesh_address = mining_address;
    mesh_pub_key = mining_pub_key;
    mesh_sec_key = mining_sec_key;
  }
  SPIFFS.end();
  LOG_S(mesh_address);
  LOG_S(mesh_pub_key);
  LOG_S(mesh_sec_key);
}

#define   MESH_PREFIX     "pYZviUVDQrjaNsD5"
#define   MESH_PASSWORD   "Fqji4aPp"
#define   MESH_PORT       5555

void loadWifiMeshConfig(void) {
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

extern std::mutex gGpsLineMtx;
extern std::list <std::string> gGPSLineBuff;

void transimitGps2Mesh(void) {
  std::lock_guard<std::mutex> lock(gGpsLineMtx);
  DUMP_I(gGPSLineBuff.size());
  if(gGPSLineBuff.size() > 0) {
    auto outLine = gGPSLineBuff.front();
    mesh2DeviceDoc.clear();
    mesh2DeviceDoc["gps"] = outLine;
    mesh2DeviceDoc["node"] = mesh_address;
    std::string outStr;
    serializeJson(mesh2DeviceDoc, outStr);
    outStr += "\r\n";
    String outStr_(outStr.c_str());
    mesh.sendBroadcast( outStr_);
    gGPSLineBuff.pop_front();
  }
}


void WifiMeshTask( void * parameter) {
  loadAddressConfig();
  loadWifiMeshConfig();
  setup_wifi_mesh();
  while(true) {
    mesh.update();
    transimitGps2Mesh();
    delay(10);
  }
}

