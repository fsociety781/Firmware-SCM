#include <EEPROM.h>
#include "TopicConfig.h"
#include <LiquidCrystal_I2C.h>
#include <FastLED.h>
// #include <SPI.h>
#include <RTClib.h>
#include "dataSensor.h"
#include <ArduinoJson.h>

#define bUP 14
#define bOK 12
#define bDN 13
#define LED_PIN     4
#define NUM_LEDS    8
#define modeMan 16
#define modeAuto 17
#define buttonFan 27
#define buttonPump 26
#define fan 25
#define pump 33

#define tekan LOW
#define tahan 50

#define Length_data 13

char temp_str[10];
char humi_str[10];

// NusabotSimpleTimer timer;

struct SetPoints {
  uint8_t MinS;
  // uint8_t MaxS;
  uint8_t MidS;
  uint8_t MinK;
  // uint8_t MaxK;
  uint8_t MidK;
};

struct Schedule1{
  uint8_t  AH1;
  uint8_t  AM1;
  uint8_t  AH2;
  uint8_t  AM2;
  uint8_t  AH3;
  uint8_t  AM3;
};

struct timers {
  uint8_t  Min;
  uint8_t  Sec;
};

struct address {
  static const int addresMinS = 0;
  static const int addresMidS = 1;
  static const int addresMinK = 3;
  static const int addresMidK = 4;
  static const int addresAH1 = 5;
  static const int addresAM1 = 6;
  static const int addresAH2 = 7;
  static const int addresAM2 = 8;
  static const int addresAH3 = 11;
  static const int addresAM3 = 12;
  static const int addresMin = 9;
  static const int addresSec = 10;
  static const int addresMode = 13;
  static const int addresSmode = 14;
};

struct MoDe{
   uint8_t statusModeW;
  String statusScheduleM;
};

char state = 0;
bool statusFan, statusPump;
String statusMode, StatusModeH; 
// statusModeW, statusScheduleM;

int buttonStateFan = 0;     
bool manualFanControl = false;
bool previousButtonStateFan = HIGH;
int buttonStatePump = 0;     
bool manualPumpControl = false;
bool previousButtonStatePump = HIGH;

address Address;
SetPoints setPoints;
Schedule1 setSchedule1;
timers setTimer;
MoDe Mode;

LiquidCrystal_I2C lcd(0x27, 20, 4);
CRGB leds[NUM_LEDS];
