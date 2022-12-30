#include <Arduino.h>
#include <ArduinoJson.h>
#include <map>
#include <WiFi.h>
#include <Preferences.h>

#include "mbedtls/sha1.h"
#include "mybase64.hpp"
#include "base32/Base32.h"
extern "C" {
  #include <tweetnacl.h>
}
#include "debug.hpp"



static unsigned char gBase64TempBinary[512];
static unsigned char gOpenedTempMsg[512];
static uint8_t gSignBinary[512];
static uint8_t gPublicKeyBinary[32];
static const int SHA512_HASH_BIN_MAX = 512/8;
static unsigned char gCalcTempHash[SHA512_HASH_BIN_MAX];

std::string sha1Address(byte *msg,size_t length);
std::string sha1Bin(byte *msg,size_t length);

extern byte gSecretKeyBin[crypto_sign_SECRETKEYBYTES];
extern String gPublicKey;

static   byte gTempSignedMsg[1024];

/*
StaticJsonDocument<512> signMsg(StaticJsonDocument<512> &msg,const std::string &ts) {
  auto goodPref = preferences.begin(preferencesZone);
  DUMP_I(goodPref);
  auto secretKey = preferences.getString(strConstEdtokenSecretKey);
  preferences.end();
  int secretRet = my_decode_base64((unsigned char*)secretKey.c_str(),secretKey.length(),gBase64TempBinary);
  DUMP_I(secretRet);
  msg["ts"] = ts;
  String origMsgStr;
  serializeJson(msg, origMsgStr);
  auto hashRet = crypto_hash_sha512(gCalcTempHash,(unsigned char*)origMsgStr.c_str(),origMsgStr.length());
  DUMP_I(hashRet);
  int b64Ret1 = my_encode_base64(gCalcTempHash,SHA512_HASH_BIN_MAX,gBase64TempBinary);
  DUMP_I(b64Ret1);
  std::string calHashB64((char*)gBase64TempBinary,b64Ret1);
  DUMP_S(calHashB64);
  auto sha1 = sha1Bin((byte*)calHashB64.c_str(),calHashB64.size());
  DUMP_S(sha1);
  DUMP_I(sha1.length());
  
  uint32_t signedSize = 0;
  int signRet = crypto_sign(gTempSignedMsg,(uint64_t*)&signedSize,(byte*)sha1.c_str(),sha1.length(),gSecretKeyBin);
  DUMP_I(signRet);
  DUMP_I(signedSize);
  int b64Ret2 = my_encode_base64(gTempSignedMsg,signedSize,gBase64TempBinary);
  std::string signedB64((char*)gBase64TempBinary,b64Ret2);
  DUMP_S(signedB64);
  StaticJsonDocument<512> signedDoc;
  DeserializationError error = deserializeJson(signedDoc, origMsgStr);
  DUMP_S(error);
  if(error == DeserializationError::Ok) {
    signedDoc["auth"]["pub"] = gPublicKey;
    signedDoc["auth"]["sign"] = signedB64;
  }
  return signedDoc;
}
*/

bool verifySign(const std::string &pub,const std::string &sign,const std::string &sha){
  DUMP_S(pub);
  DUMP_S(sign);
  DUMP_S(sha);
  int pubRet = my_decode_base64((unsigned char*)pub.c_str(),pub.size(),gBase64TempBinary);
  DUMP_I(pubRet);
  memcpy(gPublicKeyBinary,gBase64TempBinary,sizeof(gPublicKeyBinary));
  DUMP_H(gPublicKeyBinary,sizeof(gPublicKeyBinary));
  int signRet = my_decode_base64((unsigned char*)sign.c_str(),sign.size(),gBase64TempBinary);
  DUMP_I(signRet);
  memcpy(gSignBinary,gBase64TempBinary,signRet);
  DUMP_H(gSignBinary,signRet);
  
  unsigned long long mSize = 0;
  unsigned long long signSize = signRet;
  int openRet = crypto_sign_open(gOpenedTempMsg,&mSize,gSignBinary,signSize,gPublicKeyBinary);
  if(openRet == 0){
    int shaRet = my_encode_base64(gOpenedTempMsg,SHA512_HASH_BIN_MAX,gBase64TempBinary);
    std::string shaOpened((char*)gBase64TempBinary,shaRet);
    if(shaOpened ==sha) {
      return true;
    }
    LOG_S(shaOpened);
    LOG_S(sha);
  }
  return false;
}
bool isAuthedMessage(const JsonVariant &msg,const std::string &topic,const std::string &payloadStr) {
  auto hashRet = crypto_hash_sha512(gCalcTempHash,(unsigned char*)payloadStr.c_str(),payloadStr.size());
  DUMP_I(hashRet);
  int shaRet = my_encode_base64(gCalcTempHash,SHA512_HASH_BIN_MAX,gBase64TempBinary);
  DUMP_I(shaRet);
  std::string calHash((char*)gBase64TempBinary,shaRet);
  DUMP_S(calHash);

  std::string pubStr;
  std::string signStr;
  std::string shaStr;
  if(msg.containsKey("pub")){
    pubStr = msg["pub"].as<std::string>();
    DUMP_S(pubStr);
  }
  if(msg.containsKey("sign")){
    signStr = msg["sign"].as<std::string>();
    DUMP_S(signStr);
  }
  if(msg.containsKey("sha")){
    shaStr = msg["sha"].as<std::string>();
    DUMP_S(shaStr);
  }
  if(shaStr != calHash) {
    return false;
  }
  if(pubStr.empty() == false && signStr.empty() == false && shaStr.empty() == false) {
    return verifySign(pubStr,signStr,shaStr);
  }
  return false;
}
std::string sha1Bin(byte *msg,size_t length) {
  mbedtls_sha1_context sha1_ctx;
  mbedtls_sha1_init(&sha1_ctx);
  mbedtls_sha1_starts(&sha1_ctx);
  mbedtls_sha1_update(&sha1_ctx, msg, length);
  byte digest[20] = {0};
  mbedtls_sha1_finish(&sha1_ctx, digest);
  std::string result((char*)digest,sizeof(digest));
  return result;
}


std::string sha1Address(byte *msg,size_t length) {
  mbedtls_sha1_context sha1_ctx;
  mbedtls_sha1_init(&sha1_ctx);
  mbedtls_sha1_starts(&sha1_ctx);
  mbedtls_sha1_update(&sha1_ctx, msg, length);
  byte digest[20] = {0};
  mbedtls_sha1_finish(&sha1_ctx, digest);
  LOG_H(digest,sizeof(digest));
  Base32 base32;
  byte *digestB32 = NULL;
  auto b32Ret = base32.toBase32(digest,sizeof(digest),digestB32,true);
  LOG_I(b32Ret);
  std::string result((char*)digestB32,b32Ret);
  return result;
}


void miningAddress(void) {
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
}

