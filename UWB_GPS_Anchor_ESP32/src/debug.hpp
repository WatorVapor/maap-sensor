#pragma once

#if 0
#define DUMP_I(x) { \
  Serial.printf("[dump] %s::%d:%s=<%d>\r\n",__func__,__LINE__,#x,x);\
}
#define DUMP_F(x) { \
  Serial.printf("[dump] %s::%d:%s=<%f>\r\n",__func__,__LINE__,#x,x);\
}
#define DUMP_S(x) { \
  Serial.printf("[dump] %s::%d:%s=<%s>\r\n",__func__,__LINE__,#x,x.c_str());\
}
#define DUMP_H(x,y) { \
  Serial.printf("[dump] %s::%d:%s=<",__func__,__LINE__,#x);\
  for(int i = 0;i < y;i++) {\
    Serial.printf("%02x,",x[i]);\
  }\
  Serial.printf(">\r\n");\
}
#else
#define DUMP_I(x) {}
#define DUMP_F(x) {}
#define DUMP_S(x) {}
#define DUMP_H(x,y) {}
#endif

#if 1
#define LOG_I(x) { \
  Serial.printf("[log] %s::%d:%s=<%d>\r\n",__func__,__LINE__,#x,x);\
}
#define LOG_LL(x) { \
  Serial.printf("[log] %s::%d:%s=<%ld>\r\n",__func__,__LINE__,#x,x);\
}
#define LOG_F(x) { \
  Serial.printf("[log] %s::%d:%s=<%f>\r\n",__func__,__LINE__,#x,x);\
}
#define LOG_S(x) { \
  Serial.printf("[log] %s::%d:%s=<%s>\r\n",__func__,__LINE__,#x,x.c_str());\
}
#define LOG_SC(x) { \
  Serial.printf("[log] %s::%d:%s=<%s>\r\n",__func__,__LINE__,#x,x);\
}
#define LOG_H(x,y) { \
  Serial.printf("[log] %s::%d:%s=<",__func__,__LINE__,#x);\
  for(int i = 0;i < y;i++) {\
    Serial.printf("%02x,",x[i]);\
  }\
  Serial.printf(">\r\n");\
}
#else
#define LOG_I(x) {}
#define LOG_F(x) {}
#define LOG_S(x) {}
#define LOG_SC(x) {}
#define LOG_H(x,y) {}
#endif


