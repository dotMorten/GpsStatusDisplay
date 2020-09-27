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
#include "GnssMonitor.h"
bool ledstate;
bool isDisplayOff;

// Initialize the OLED display using Arduino Wire:
SSD1306Wire display(0x3c, SDA, SCL);
int16_t currentDisplay = 0;
int16_t fontHeight = 10;

void showDisplay()
{
  if(isDisplayOff)
    return;
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
  if(std::isnan(righttext))
    writepair(lefttext, "---", row);
  else
    writepair(lefttext, String(righttext, decimals), row);
}

void writepair(const String lefttext, const float righttext, const String unit, const int decimals, const int row)
{
  if(std::isnan(righttext))
    writepair(lefttext, "---", row);
  else
    writepair(lefttext, String(righttext, decimals) + unit, row);
}

void drawPage1()
{ 
  display.setFont(ArialMT_Plain_10);
  writepair("Mode", mode(), 0);
  writepair("Sats",String(sats()), 1);
  if(std::isnan(latitude()))
    writepair("Lat","---", 2);
  else
    writepair("Lat",String(latitude(), 2) + "*****" + latIndicator(), 2);
  if(std::isnan(longitude()))
    writepair("Lon","---", 3);
  else
    writepair("Lon",String(longitude(), 2) + "*****" + lonIndicator(), 3);

  if(std::isnan(elevation()))
    writepair("MSL","---", 4);
  else
    writepair("MSL",String(elevation(), 3) + "m", 4);

  writepair("Time",gpstime(), 5);
}

void drawPage2()
{ 
  display.setFont(ArialMT_Plain_10);
  writepair("Mode", mode(), 0);
  writepair("Horizontal Error", horizontalError(), "m", 3, 1);
  writepair("Vertical Error", verticalError(), "m", 3, 2);
  writepair("HDOP ", hdop(), 3);
  writepair("VDOP ", vdop(), 4);
  writepair("PDOP ", pdop(), 5);
}

void drawPage3()
{ 
  int cx = 31;
  int cy = 31;
  display.drawCircle(cx, cy, 16);
  int dx = 20;
  int dy = 19;
  display.drawLine(dx + 11, dy + 0,   dx + 17, dy + 11);
  display.drawLine(dx + 17, dy + 11,  dx + 14, dy + 11);
  display.drawLine(dx + 14, dy + 11,  dx + 14, dy + 23);
  display.drawLine(dx + 14, dy + 11,  dx + 14, dy + 23);
  display.drawLine(dx + 14, dy + 23,  dx + 8,  dy + 23);
  display.drawLine(dx + 8,  dy + 23,  dx + 8,  dy + 11);
  display.drawLine(dx + 8,  dy + 11,  dx + 5,  dy + 11);
  display.drawLine(dx + 5,  dy + 11,  dx + 11, dy + 0);
  float c = course() / 180.0 * M_PI;
  float sn = sin(c);
  float cn = cos(c);
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawLine(sn * 14 + cx, cn * 14 + cy, sn * 19 + cx, cn * 19 + cy);
  display.drawString(sn * 25 + cx, cn * 25 + cy - 5, "S");

  sn = sin(c + M_PI/2);
  cn = cos(c + M_PI/2);  
  display.drawLine(sn * 14 + cx, cn * 14 + cy, sn * 19 + cx, cn * 19 + cy);
  display.drawString(sn * 25 + cx, cn * 25 + cy - 5, "E");

  sn = sin(c + M_PI*2/2);
  cn = cos(c + M_PI*2/2);  
  display.drawLine(sn * 14 + cx, cn * 14 + cy, sn * 19 + cx, cn * 19 + cy);
  display.drawString(sn * 25 + cx, cn * 25 + cy - 5, "N");
  
  sn = sin(c + M_PI*3/2);
  cn = cos(c + M_PI*3/2);
  display.drawLine(sn * 14 + cx, cn * 14 + cy, sn * 19 + cx, cn * 19 + cy);
  display.drawString(sn * 25 + cx, cn * 25 + cy - 5, "W");

  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(64, 0, "Speed");
  display.drawString(64, 32, "Course");
  display.setFont(ArialMT_Plain_16);
  display.drawString(64, 13, String(speed(),1) + "kn");
  display.drawString(64, 45, String(course(),0) + "°");
}

void drawPage4()
{ 
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  auto lat = String(latitude(), 2) + "*****" + latIndicator();
  auto lon = String(longitude(), 2) + "*****" + lonIndicator();
  auto z = String(elevation(), 3) + "m";
  if(std::isnan(latitude()))
    lat = "---";
  if(std::isnan(longitude()))
    lon = "---";
  if(std::isnan(elevation()))
    z = "---";
  display.drawString(64, 4, lat);
  display.drawString(64, 24, lon);
  display.drawString(64, 44, z);
}

void onLocation(const char *type)
{
  if(type == "RMC")
  {  
    // Flash LED each time RMC is received
    if(ledstate)
      digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
    else
      digitalWrite(LED_BUILTIN, LOW);   // turn the LED on (HIGH is the voltage level)
    ledstate = !ledstate;
  }
  if (type == "GSA") // GSA is the last message in the bursts
  {
    showDisplay();
  }
}
const int buttonPin = 2;
unsigned long lastButtonPressTime;
void setup()
{
    // Initialising the UI will init the display too.
  display.init();  
  display.flipScreenVertically();
  display.clear();
  setLocationHandler(onLocation);
  Wire.begin();   
  // Page 1
  showDisplay();
  initNmeaParser();
  lastButtonPressTime = millis();
  Serial1.begin(38400); //38400
  Serial1.setTimeout(10);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(buttonPin, INPUT);
}
int buttonState = 0;
void loop()
{
  auto t = millis();
  // Turn display off after 30 seconds of not pressing the button
  if(t - lastButtonPressTime > 30000 && !isDisplayOff)
  {
    isDisplayOff = true;
    display.displayOff();
  }
  // Check button state and turn screen back on or flip pages
  auto newButtonState = digitalRead(buttonPin);
  if (newButtonState != buttonState && newButtonState == HIGH) {
    lastButtonPressTime = t;
    if(isDisplayOff)
    {
      isDisplayOff = false;
      display.displayOn();
    }
    else {
      currentDisplay++;
      if(currentDisplay > 3)
        currentDisplay = 0;
    }
    showDisplay();
  }
  buttonState = newButtonState;

  //Parse serial NMEA data
  readData();
}
