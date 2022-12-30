#include <Arduino.h>
#include <base64.hpp>

unsigned int my_encode_base64(unsigned char input[], unsigned int input_length, unsigned char output[]){
  return encode_base64(input,input_length,output);
}
unsigned int my_decode_base64(unsigned char input[], unsigned char output[]){
  return decode_base64(input,output);
}
unsigned int my_decode_base64(unsigned char input[], unsigned int input_length, unsigned char output[]){
  return decode_base64(input,input_length,output);
}
