/*
  09/16/2020
  Author: Morten Nielsen
  Platforms: ESP8266
  Language: C++
  File: GpsStart.ino

  3rd libs used:
  ThingPulse / esp8266-oled-ssd1306: https://github.com/ThingPulse/esp8266-oled-ssd1306/commit/c1fa10ea5e3700cfde1d032fa20b468bc43c997c
  NmeaParser (modified): https://github.com/Glinnes/NMEAParser/commit/fa99f9ce0c08459e7afbf8dd8ad1caf1589ae0c8

*/
#include <Wire.h>
#include "SSD1306Wire.h"
#include "NMEAParser.h"
NMEAParser<4> parser; 
bool ledstate;

// Initialize the OLED display using Arduino Wire:
SSD1306Wire display(0x3c, SDA, SCL);
int16_t currentDisplay = 0;
int16_t fontHeight = 10;

// State variables
float latitude;
String latIndicator;
float longitude;
String lonIndicator;
float elevation;
String mode;
float verticalError;
float horizontalError;
String hdop;
String vdop;
String pdop;
int sats;
String gpstime;

void showDisplay()
{
  display.clear();
   if(currentDisplay == 0)
      drawPage1();
   else if(currentDisplay == 1)
      drawPage2();
   else if(currentDisplay == 2)
      drawPage3();
   else if(currentDisplay == 3)
      drawPage4();
  display.display();
}
void writepair(const String lefttext, const String righttext, const int row)
{
  uint16_t y = row*fontHeight;
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, y, lefttext);
  display.setTextAlignment(TEXT_ALIGN_RIGHT);
  display.drawString(128, y, righttext);
}
void writepair(const String lefttext, const float righttext, const int decimals, const int row)
{
  if(righttext == NAN)
    writepair(lefttext, "---", row);
  else
    writepair(lefttext, String(righttext, decimals), row);
}
void drawPage1()
{ 
  display.setFont(ArialMT_Plain_10);
  writepair("Mode", mode, 0);
  writepair("Sats",String(sats), 1);
  if(latitude == NAN)
    writepair("Lat","---", 2);
  else
    writepair("Lat",String(latitude, 2) + "*****" + latIndicator, 2);
  if(longitude == NAN)
    writepair("Lon","---", 3);
  else
    writepair("Lon",String(longitude, 2) + "*****" + lonIndicator, 3);

  if(elevation == NAN)
    writepair("MSL","---", 4);
  else
    writepair("MSL",String(elevation, 3) + "m", 4);

  writepair("Time",gpstime, 5);
}
void drawPage2()
{ 
  display.setFont(ArialMT_Plain_10);
  writepair("Mode", mode, 0);
  writepair("Horizontal Error", horizontalError, 3, 1);
  writepair("Vertical Error", verticalError, 3, 2);
  writepair("HDOP ", hdop, 3);
  writepair("VDOP ", vdop, 4);
  writepair("PDOP ", pdop, 5);
}
void drawPage3()
{ 
  display.drawCircle(64, 32, 31);
  // TODO: Draw satellite plot
  display.setPixel(60,12);
  display.setPixel(80,42);
  display.setPixel(85,22);
  display.setPixel(85,42);
  display.setPixel(40,30);
  display.setPixel(45,58);
}

void drawPage4()
{ 
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 4, String(latitude, 2) + "*****" + latIndicator);
  display.drawString(64, 24, String(longitude, 2) + "*****" + lonIndicator);
  display.drawString(64, 44, String(elevation, 3) + "m");
}
void handleRMC(void)
{
  // Flash LED each time RMC is received
  if(ledstate)
    digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  else
    digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
  ledstate = !ledstate;
}
void handleGGA(void)
{
  float lat;
  float lon;
  if (parser.getArg(1, lat) && parser.getArg(3, lon))
  {
    // Write latitude
    String strlat;
    parser.getArg(1, strlat);
    parser.getArg(2, latIndicator);
    latitude = strlat.substring(0,2).toFloat() + strlat.substring(2).toFloat() / 60;
    // Write longitude
    String strlon;
    parser.getArg(3, strlon);
    parser.getArg(4, lonIndicator);
    longitude = strlon.substring(0,3).toFloat() + strlon.substring(3).toFloat() / 60;
  }
  else
  {
    latitude = NAN;
    longitude = NAN;
  }
  // Write elevation
  float el;
  if(parser.getArg(8, el))
  {
    elevation = el;
  }
  else
  {
    elevation = NAN;
  }
  
  int quality = 0;
  
  parser.getArg(5, quality);
  if(quality == 0)
     mode = "No Fix";
  else if(quality == 1)
     mode = "GPS Fix";
  else if(quality == 2)
     mode = "Differential";
  else if(quality == 3)
     mode = "GPS PPS";
  else if(quality == 4)
     mode = "RTK";
  else if(quality == 5)
     mode = "RTK Float";
  else if(quality == 6)
     mode = "Estimated";
  else
     mode = "---";

  int satCount = 0;
  parser.getArg(6, satCount);
  //writepair("Satellites", String(satCount), 1, display);
  sats = satCount;
  String time;
  if(parser.getArg(0, time))
      gpstime = time.substring(0,2) + ":" + time.substring(2,4) + ":" + time.substring(4,6);
  else
    gpstime = "---";
  showDisplay();
}

void handleGSA(void)
{
  int count = parser.argCount();
  int systemId;
  if(parser.getArg(count-1, systemId) && systemId == 1)
  {
    if(count > 4 && parser.getArg(count - 4, pdop) && parser.getArg(count - 3, hdop) && pdop.length() > 0 && hdop.length() > 0
    && parser.getArg(count - 2, vdop)) 
    {
    }
    else 
    {
       pdop = "---";
       hdop = "---";
       vdop = "---";
    }
  }
  showDisplay();
}

void handleGST(void)
{
  float error_lat;
  float error_lon;
  
  if (parser.getArg(5, error_lat) && parser.getArg(6, error_lon))
  {
    float error = sqrt(error_lat * error_lat + error_lon * error_lon);
    horizontalError = error;
  }
  else
  {
    String error;
    parser.getArg(5, error);
    horizontalError = NAN;
  }
  float error_v;
  if (parser.getArg(7, error_v))
  {
    verticalError = error_v;
  }
  else
  {
    verticalError = NAN;
  }
  showDisplay();
}
void unknownCommand()
{
  char buf[6];
  parser.getType(buf);
  //if(buf == "GNVTG" || buf == "GNGLL)
   // return;
  //write(buf, 1, 0);
}

void errorHandler()
{
  char buf[6];
  parser.getType(buf);
  
  //write(String(parser.error()) + " " + buf, 6, 0); 
  //showDisplay();
}
void setup()
{
    // Initialising the UI will init the display too.
  display.init();  
  display.flipScreenVertically();
  display.clear();
  
  Wire.begin();   
  // Page 1
  showDisplay();

  Serial1.begin(38400); //38400
  Serial1.setTimeout(10);
  pinMode(LED_BUILTIN, OUTPUT);
  parser.addHandler("GNRMC", handleRMC);
  parser.addHandler("GNGGA", handleGGA);
  parser.addHandler("--GST", handleGST);
  parser.addHandler("GNGSA", handleGSA);
  parser.setDefaultHandler(unknownCommand);
  parser.setErrorHandler(errorHandler);
}
String a;
char buf[1024];
void loop()
{
  int count;
  
  auto t = millis();
  int index = (t / 5000) % 4;
  if(currentDisplay != index)
  {
    currentDisplay = index;
    showDisplay();      
  }
  
  if (Serial1.available()) {
    while((count = Serial1.readBytes(buf, 1024)) > 0)
    {
      for(int i=0;i<count;i++)
        parser << buf[i];
    }
  
    /*if(ledstate)
      digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    else
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
    ledstate = !ledstate;*/
  }
}
