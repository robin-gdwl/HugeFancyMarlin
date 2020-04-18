#include <Arduino.h>

unsigned long currentMillis = 0;

// Zielwerte aus Marlin
int targetTempExtruderMarlin = 0; // max 9 Bit = 511°C
byte targetExtruderCoolingStatusMarlin = 0; // 0 = off, 1 = on // muss 8 Bit, statt boolean sein
byte targetPwmValuePartCoolingFanMarlin = 0; // max 8 Bit = 255

// Variabeln
int TempExtruderObenrum = 0; // max 9 Bit = 511°C
int TempExtruderUntenrum = 0; // max 9 Bit = 511°C
int TempWatercoolingCold = 0; // max 9 Bit = 511°C
int TempWatercoolingWarm = 0; // max 9 Bit = 511°C

byte ExtruderCoolingStatusMarlin = 0; // 0 = off, 1 = on // muss 8 Bit, statt boolean sein
byte pwmValuePartCoolingFan = 0; // max 8 Bit = 255


void setup()
{
  Serial.begin(9600); // serieller Monitor

  RS485_setup();
  TM1637_setup();
  Farbmischer_setup();
}

void loop()
{
  currentMillis = millis(); // Für das periodische Aufrufen von diversen Funktionen ohne ein delay zu verursachen
  
  RS485_updateVariables();
  TM1637_update();
}
