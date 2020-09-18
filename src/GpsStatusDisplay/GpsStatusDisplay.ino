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
  if(righttext == NAN)
    writepair(lefttext, "---", row);
  else
    writepair(lefttext, String(righttext, decimals), row);
}

void drawPage1()
{ 
  display.setFont(ArialMT_Plain_10);
  writepair("Mode", mode(), 0);
  writepair("Sats",String(sats()), 1);
  if(latitude() == NAN)
    writepair("Lat","---", 2);
  else
    writepair("Lat",String(latitude(), 2) + "*****" + latIndicator(), 2);
  if(longitude() == NAN)
    writepair("Lon","---", 3);
  else
    writepair("Lon",String(longitude(), 2) + "*****" + lonIndicator(), 3);

  if(elevation() == NAN)
    writepair("MSL","---", 4);
  else
    writepair("MSL",String(elevation(), 3) + "m", 4);

  writepair("Time",gpstime(), 5);
}

void drawPage2()
{ 
  display.setFont(ArialMT_Plain_10);
  writepair("Mode", mode(), 0);
  writepair("Horizontal Error", horizontalError(), 3, 1);
  writepair("Vertical Error", verticalError(), 3, 2);
  writepair("HDOP ", hdop(), 3);
  writepair("VDOP ", vdop(), 4);
  writepair("PDOP ", pdop(), 5);
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
  display.drawString(64, 4, String(latitude(), 2) + "*****" + latIndicator());
  display.drawString(64, 24, String(longitude(), 2) + "*****" + lonIndicator());
  display.drawString(64, 44, String(elevation(), 3) + "m");
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
  else {
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
