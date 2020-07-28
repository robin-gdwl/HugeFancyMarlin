
// RS485 long distance communication betweent Teensy_Schaltschrank (Master) and Teensy_Extruder (Slave) via UART with selfmade Communication Protocol
// Tutorial RS485 UART: https://circuitdigest.com/microcontroller-projects/rs485-serial-communication-between-arduino-uno-and-arduino-nano
// Tutorial designing a communication protocol: https://henryforceblog.wordpress.com/2015/03/12/designing-a-communication-protocol-using-arduinos-serial-library/

// Slave Device

// PINOUT
// R0 -> Teensy Rx1 / PIN 0
// RE = DE -> Teensy PIN 8
// DI -> Teensy Tx1 / PIN 1
// VCC -> 5V
// A -> Kabelterminal 17
// B -> Kabelterminal 18
// GND -> Teensy GND

const byte RS485_enablePin = 13;

// Send
const uint8_t bufferSizeRS485 = 5;
uint8_t bufferRS485[bufferSizeRS485];
// Receive
uint8_t readCounter = 0;
uint8_t isHeader = 0;
uint8_t firstTimeHeader = 0; // Flag that helps us restart counter when we first find header byte

// headers
const uint8_t header_AbsenderExtruder_Statusupdate = 13;
const uint8_t header_AbsenderSchaltschrank_Statusupdate = 14;
const uint8_t header_AbsenderSchaltschrank_FarbmischerAktion = 15;

//const uint8_t header_updateVariables = 0x7E; // Bufferheader: Verkündung Teensy Schaltschrank
//const uint8_t header_AnswerUpdateVariables = 0x7C; // Bufferheader: Aktion Farbmischer
//const uint8_t header_Farbmischer = 0x7D; // Bufferheader: Aktion Farbmischer

void RS485_setup()
{
  while (!Serial1);
  Serial1.begin(115200); // Serial1 für RS485
  Serial1.transmitterEnable(RS485_enablePin);
  delay(10);
}

void RS485_Extruder_CheckIfUpdateAvalible()
{
  int BreakCounter = 0;
  while (BreakCounter <= 20)
  {
    if (Serial1.available() > 0) // Check if there is any data available to read
    {
      uint8_t c = Serial1.read(); // read only one byte at a time

      if ((c == header_AbsenderSchaltschrank_Statusupdate) or (c == header_AbsenderSchaltschrank_FarbmischerAktion)) // Check if header is found
      {
        // We must consider that we may sometimes receive unformatted data, and given the case we must ignore it and restart our reading code.
        // If it's the first time we find the header, we restart readCounter indicating that data is coming.
        // It's possible the header appears again as a data byte. That's why this conditional is implemented, so that we don't restart readCounter and corrupt the data.
        if (!firstTimeHeader)
        {
          isHeader = true;
          readCounter = 0;
          firstTimeHeader = true;
        }
      }
      bufferRS485[readCounter] = c; // store received byte, increase readCounter ### FEHLER???
      Serial.print("counter = "); Serial.print(readCounter); Serial.print(" // "); Serial.println(bufferRS485[readCounter]);
      readCounter++;



      if (readCounter >= bufferSizeRS485) // prior overflow, we have to restart readCounter
      {
        readCounter = 0;

        if (isHeader == 1) // if header was found
        {
          uint8_t checksumValue = bufferRS485[4]; // get checksum value from buffer's last value, according to defined protocol
          if (verifyChecksum(checksumValue) == true) // perform checksum validation, it's optional but really suggested
          {
            if (bufferRS485[0] == header_AbsenderSchaltschrank_Statusupdate)
            {
              // Byte 0 Header
              // Byte 1 TargetTempExtruderMarlin Byte 01
              // Byte 2 TargetTempExtruderMarlin Byte 02
              // Byte 3 pwmValuePartCoolingFanMarlin
              // Byte 4 Checksum

              TargetTempExtruderMarlin = bufferRS485[1] + bufferRS485[2]; // gesendete 8 Bit Werte wiedeer auf die ursprünglichen 9 Bit zurückführen
              Serial.print("Empfange Statusupdate vom Schaltschrank: TargetTempExtruderMarlin = "); Serial.println(TargetTempExtruderMarlin);
              PwmValuePartCoolingFanMarlin = bufferRS485[3];
              KuehlungPWM(); // Kühlung aktuallisieren

              // Timeout Verbindung Schaltschrank – Extruder weggebrochen
              RS485_updateVariables_LastUpdatePreviousMillis = currentMillis; // für Timeout falls wir lange nichts mehr vom Teensy Schaltschrank gehört haben

              // Antwort und sende das eigene Statusupdate
              RS485_Extruder_Send_Statusupdate();
            }
            
              if (bufferRS485[0] == header_AbsenderSchaltschrank_FarbmischerAktion)
              {
              // Byte 0 Header
              // Byte 1 Schaufeln Links
              // Byte 2 Schaufeln Rechts
              // Byte 3 frei
              // Byte 4 Checksum

              byte Schaufeln_L = bufferRS485[1];
              byte Schaufeln_R = bufferRS485[2];
              Farbmischer_GibFarbe(Schaufeln_L, Schaufeln_R); // Motoren Farbmischer starten
              }
            
          }
          // restart header flag
          isHeader = 0;
          firstTimeHeader = 0;
        }
      }
      BreakCounter++;
      //delay(5);
    }
  }
  Serial.println("Break Loop");
}

void RS485_Extruder_Send_Statusupdate()
{
  RGB_Lila();
  Serial.println("Sende Statusupdate an Schaltschrank");

  // Byte 0 Header
  // Byte 1 RealTempExtruderForMarlin_01 Byte 01
  // Byte 2 RealTempExtruderForMarlin_02 Byte 02
  // Byte 3 leer
  // Byte 4 Checksum

  bufferRS485[0] = header_AbsenderExtruder_Statusupdate; // Bufferheader
  if (CombinedRealTempExtruder <= 255)
  {
    bufferRS485[1] = CombinedRealTempExtruder; // Wert von 0-255°C
    bufferRS485[2] = 0;
  }
  else if ((CombinedRealTempExtruder > 255) and (CombinedRealTempExtruder <= 510))
  {
    bufferRS485[1] = 255;
    bufferRS485[2] = CombinedRealTempExtruder - 255; // Wert von 256-510°C
  }
  bufferRS485[3] = 0;
  bufferRS485[4] = checksum();

  // Senden
  Serial1.write(bufferRS485, bufferSizeRS485);
  RGB_Aus();
}

//We perform a sum of all bytes, except the one that corresponds to the original checksum value. After summing we need to AND the result to a byte value.
uint8_t checksum() {
  uint8_t result = 0;
  uint16_t sum = 0;

  for (uint8_t i = 0; i < (bufferSizeRS485 - 1); i++)
  {
    sum += bufferRS485[i];
  }
  sum = sum + 10;
  result = sum & 0xFF;

  return result;
}

// This a common checksum validation method. We perform a sum of all bytes, except the one that corresponds to the original checksum value. After summing we need to AND the result to a byte value.
uint8_t verifyChecksum(uint8_t originalResult)
{
  uint8_t result = 0;
  uint16_t sum = 0;

  for (uint8_t i = 0; i < (bufferSizeRS485 - 1); i++)
  {
    sum += bufferRS485[i];
  }
    sum = sum + 10;
  result = sum & 0xFF;

  if (originalResult == result)
  {
    return 1;
  } else
  {
    return 0;
  }
}
