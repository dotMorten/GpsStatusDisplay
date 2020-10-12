/*
  09/16/2020
  Author: Morten Nielsen
  Platforms: ESP8266
  Language: C++
  File: GpsStart.ino

  3rd party libs used:
  ucglib : https://github.com/olikraus/ucglib
  NmeaParser (modified): https://github.com/Glinnes/NMEAParser/commit/fa99f9ce0c08459e7afbf8dd8ad1caf1589ae0c8
*/
#include <Wire.h>

#include <SPI.h>
#include "Ucglib.h"
#include "NMEAParser.h"
#include "GnssMonitor.h"

bool ledstate;
bool isDisplayOff;
bool privacy = false; // set to true to limit location precision for privacy (good for screenshots)

int16_t currentDisplay = 2;
int8_t fontHeight = 14;
const unsigned char satBitmap[] PROGMEM = { //10x6
    0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0xff,
};
const unsigned char plusminus[] PROGMEM = { //5x7
    0x00, 0x00, 0xff, 0x00, 0x00,
    0x00, 0x00, 0xff, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0xff, 0x00, 0x00,
    0x00, 0x00, 0xff, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00,
    0xff, 0xff, 0xff, 0xff, 0xff
};
const unsigned char mountainBitmap[] PROGMEM = { //12x10
    0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00,
    0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00,
    0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00,
    0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00,
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff,
    0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff
};
// Initialize the OLED display:
Ucglib_SSD1351_18x128x128_FT_HWSPI ucg(/*cd=*/ 1, /*cs=*/ 3, /*reset=*/ 2);

void showDisplay(bool newPage)
{
   if(isDisplayOff)
     return;
   drawStatusBar(newPage);
   if(currentDisplay == 0)
      drawPage_ErrorInfo(newPage);
   else if(currentDisplay == 1)
      drawPage_NavigationInfo(newPage);
   else if(currentDisplay == 2)
      drawPage_LocationInfo(newPage);
}
void drawString(int x, int y, const char* text, bool rightAlign)
{
  auto ascent = ucg.getFontAscent();
  auto descent = ucg.getFontDescent();
  int width = 0;
  //if(rightAlign)
  //   width = ucg.getStrWidth("   " + text);
  //else 
  width = ucg.getStrWidth(text);
  if(rightAlign)
    ucg.setPrintPos(x - width,y);
  else
    ucg.setPrintPos(x,y);
  ucg.print(text);
}
void drawStringCenter(int y, const char* text)
{
  auto width = ucg.getStrWidth(text);
  ucg.setPrintPos(64 - width / 2,y);
  ucg.print(text);
}
void drawStringCenterCenter(int x, int y, const char* text)
{
  auto width = ucg.getStrWidth(text);  
  auto ascent = ucg.getFontAscent();
  auto descent = 0; //ucg.getFontDescent();
  ucg.setPrintPos(x - width / 2,y + (ascent+descent)/2);
  ucg.print(text);
}

void drawString(int x, int y, String text, bool rightAlign)
{
   drawString(x,y, text.c_str(), rightAlign);
}

void drawString(int x, int y, String text)
{
   drawString(x,y, text, false);
}

void drawString(int x, int y, const char* text)
{
  drawString(x,y,text,false);
}

void writepair(const String lefttext, const String righttext, const int row)
{
  uint8_t y = row*fontHeight;
  drawString(0, y, lefttext);  
  drawString(128, y, righttext, true);
}

void writepair(const String lefttext, const float righttext, const int decimals, const int row)
{
  if(isnan(righttext))
    writepair(lefttext, "---", row);
  else
    writepair(lefttext, String(righttext, decimals), row);
}

void writepair(const String lefttext, const float righttext, const String unit, const int decimals, const int row)
{
  if(isnan(righttext))
    writepair(lefttext, "---", row);
  else
    writepair(lefttext, String(righttext, decimals) + unit, row);
}

void writepair(const String lefttext, const float righttext, const int row)
{
  if(isnan(righttext))
    writepair(lefttext, "---", row);
  else
    writepair(lefttext, String(righttext), row);
}
void drawBitmap(int ox, int oy, int width, int height, const unsigned char* bitmap, uint8_t r, uint8_t g, uint8_t b)
{
  int count = 0;
  for (int y = 0; y < height; y++) {
     for (int x = 0; x < width; x++) {
       if(bitmap[y*width+x] > 0)
         ucg.setColor(r,g,b);
       else
         ucg.setColor(0,0,0);
       ucg.drawPixel(ox+x,oy+y);
       count ++;
     }
   }
}
void drawStatusBar(bool newPage)
{
  ucg.setFont(ucg_font_helvR08_hr);
  if(newPage) {
    ucg.setColor(255, 255, 0);
    ucg.drawHLine(0,10,128);
  }

  //Fix quality
  String m;
  int q = quality();
  ucg.setColor(0,255,0);
  if(q == 0)
  {
    ucg.setColor(255,0,0);
    m = "No Fix";
  }
  else if(q == 1)
     m = "GPS   ";
  else if(q == 2)
     m = "DIFF  ";
  else if(q == 3)
     m = "PPS   ";
  else if(q == 4)
     m = "RTK   ";
  else if(q == 5)
  {
    ucg.setColor(255,255,0);
    m = "FLOAT ";
  }
  else {
    ucg.setColor(255,0,0);
     m = "---   ";
  }
  ucg.setPrintPos(0,8);
  ucg.print(m);

  // Horizontal error
  auto err = horizontalError();
  int decimals = 1;
  if(err < .02)
    decimals = 3;
  else if(err < .2)
    decimals = 2;
  else if(err < 2)
    decimals = 1;
  auto str = (" " + String(err, decimals) + "m  ").c_str();
  auto width = ucg.getStrWidth(str);
  ucg.setColor(255, 255, 0);
  ucg.setPrintPos(64-width/2+1,8);
  ucg.print(str);
  drawBitmap(64-width/2-3, 1, 5, 7, plusminus, 255, 255, 0);

  // Satellite count
  if(newPage)
    drawBitmap(100, 1, 10, 6, satBitmap, 255, 255, 0);
  ucg.setColor(255, 255, 0);
  ucg.setPrintPos(115,8);
  ucg.print(String(sats()).c_str());
}

void drawPage_ErrorInfo(bool newPage)
{    
  if(newPage)
  {
    ucg.setFont(ucg_font_helvR08_hr);
    ucg.setColor(255, 255, 0);
    drawString(0, 3*fontHeight, "Error:");
    drawString(0, 6*fontHeight, "DOP:");
    ucg.setColor(255, 255, 255);
    ucg.setFont(ucg_font_helvR10_hr);
    drawString(7, 4*fontHeight, "Horizontal");
    drawString(7, 5*fontHeight, "Vertical");
    drawString(7, 7*fontHeight, "Horizontal");
    drawString(7, 8*fontHeight, "Vertical");
    drawString(7, 9*fontHeight, "Point");
  }
  else {
    ucg.setFont(ucg_font_helvR10_hr);
    ucg.setColor(255, 255, 255);
  }
  drawStringCenter(fontHeight * 2, ("  " + mode() + "  ").c_str());
  writepair("", horizontalError(), "m", 3, 4);
  writepair("", verticalError(), "m", 3, 5);
  if(hdop() < 7)
    ucg.setColor(0, 255, 0);  
  else
    ucg.setColor(255, 0, 0);  
  writepair("", hdop(), 7);
  writepair("", vdop(), 8);
  writepair("", pdop(), 9);
}
float lastCourse;
void drawPage_NavigationInfo(bool newPage)
{ 
  ucg.setColor(255, 255, 255);
  auto currentCourse = course();
  if(newPage || lastCourse != currentCourse)
  {
    // Redraw compass
    if(!newPage){
      ucg.setColor(0, 0, 0);
      ucg.drawBox(1, 12, 75, 75);
      ucg.setColor(255, 255, 255);
    }
    lastCourse = currentCourse;
    
    ucg.setFont(ucg_font_helvR08_hr);
    int const cx = 40;
    int const cy = 50;
    int const radius = 24;
    float scale = 1.5;
    ucg.drawCircle(cx, cy, radius, UCG_DRAW_ALL);
    int const dx = 24;
    int const dy = 33;
    ucg.drawLine(dx + 11 * scale, dy + 0 * scale,   dx + 17 * scale, dy + 11 * scale);
    ucg.drawLine(dx + 17 * scale, dy + 11 * scale,  dx + 14 * scale, dy + 11 * scale);
    ucg.drawLine(dx + 14 * scale, dy + 11 * scale,  dx + 14 * scale, dy + 23 * scale);
    ucg.drawLine(dx + 14 * scale, dy + 11 * scale,  dx + 14 * scale, dy + 23 * scale);
    ucg.drawLine(dx + 14 * scale, dy + 23 * scale,  dx + 8 * scale,  dy + 23 * scale);
    ucg.drawLine(dx + 8 * scale,  dy + 23 * scale,  dx + 8 * scale,  dy + 11 * scale);
    ucg.drawLine(dx + 8 * scale,  dy + 11 * scale,  dx + 5 * scale,  dy + 11 * scale);
    ucg.drawLine(dx + 5 * scale,  dy + 11 * scale,  dx + 11 * scale, dy + 0 * scale);
    float c = currentCourse / 180.0 * M_PI;
    float sn = sin(c);
    float cn = cos(c);  
    int const charoffset = radius+9;
    ucg.drawLine(sn * (radius-2) + cx, cn * (radius-2) + cy, sn * (radius+3) + cx, cn * (radius+3) + cy);
    drawStringCenterCenter(sn * charoffset + cx, cn * charoffset + cy, "S");  
    
    sn = sin(c + M_PI/2);
    cn = cos(c + M_PI/2);  
    ucg.drawLine(sn * (radius-2) + cx, cn * (radius-2) + cy, sn * (radius+3) + cx, cn * (radius+3) + cy);
    drawStringCenterCenter(sn * charoffset + cx, cn * charoffset + cy, "E");

    sn = sin(c + M_PI*2/2);
    cn = cos(c + M_PI*2/2);  
    ucg.drawLine(sn * (radius-2) + cx, cn * (radius-2) + cy, sn * (radius+3) + cx, cn * (radius+3) + cy);
    drawStringCenterCenter(sn * charoffset + cx, cn * charoffset + cy, "N");
  
    sn = sin(c + M_PI*3/2);
    cn = cos(c + M_PI*3/2);
    ucg.drawLine(sn * (radius-2) + cx, cn * (radius-2) + cy, sn * (radius+3) + cx, cn * (radius+3) + cy);
    drawStringCenterCenter(sn * charoffset + cx, cn * charoffset + cy, "W");
  }
  // Speed+Course
  drawString(84, 30, "Course");
  drawString(0, 110, "Speed");
  ucg.setFont(ucg_font_helvR14_hr);
  drawString(80, 53, String(currentCourse,0) + "°");  drawString(40, 110, String(speed(),1) + "kn");
}

void drawPage_LocationInfo(bool newPage)
{ 
  ucg.setColor(255, 255, 255);
  ucg.setFont(ucg_font_helvR14_hr);
  int decimals = 6;
  auto herror = horizontalError();
  if(herror < 0.02)
    decimals = 8;
  else if(herror < 0.2)
    decimals = 7;
  else if(herror < 2)
    decimals = 6;
  else 
    decimals = 5;
  String lat;
  String lon;
  if(privacy)
  {
    decimals = 2; 
    lat = String(latitude(), decimals) + "*****" + latIndicator();
    lon = String(longitude(), decimals) + "*****" + lonIndicator();
  }
  else {
    lat = String(latitude(), decimals) + "°" + latIndicator();
    lon = String(longitude(), decimals) + "°" + lonIndicator();
  }
  auto zerror = verticalError();
  if(zerror < 0.05)
   decimals = 3;
  else if(zerror < 0.1)
   decimals = 2;
  else if(zerror < 2)
   decimals = 1;
  else 
   decimals = 0;
  auto z = String(elevation(), decimals) + "m";
  if(isnan(latitude()))
    lat = "---";
  if(isnan(longitude()))
    lon = "---";
  if(isnan(elevation()))
    z = "---";
  drawStringCenter(37, lat.c_str());
  drawStringCenter(64, lon.c_str());
  drawStringCenter(91, z.c_str());  
  drawStringCenter(118 ,gpstime().c_str());

  if(newPage) {
    drawBitmap(0, 80, 12, 10, mountainBitmap, 255, 255, 255);
    ucg.setColor(255, 255, 255);
    ucg.drawCircle(6, 112, 6, UCG_DRAW_ALL);
    ucg.drawHLine(6,112,3);
    ucg.drawVLine(6,107,5);
   }
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
  if (type == "GGA") // GSA is the last message in the bursts
  {
    showDisplay(false);
  }
}
const int buttonPin = 0;
unsigned long lastButtonPressTime;
void setup()
{
  setLocationHandler(onLocation);
  Wire.begin();   
  initNmeaParser();
  lastButtonPressTime = millis();
  Serial1.begin(38400);
  Serial1.setTimeout(10);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(buttonPin, INPUT);

  ucg.begin(UCG_FONT_MODE_SOLID);
  ucg.clearScreen();
  
  ucg.setColor(255, 255, 255);
  ucg.setFontMode(UCG_FONT_MODE_SOLID);
  showDisplay(true);
}
int buttonState = 0;
void loop()
{
  auto t = millis();
  // Turn display off after 30 seconds of not pressing the button
  if(t - lastButtonPressTime > 30000 && !isDisplayOff)
  {
    isDisplayOff = true;
    ucg.clearScreen();
  }
  // Check button state and turn screen back on or flip pages
  auto newButtonState = digitalRead(buttonPin);
  if (newButtonState != buttonState && newButtonState == HIGH) {
    lastButtonPressTime = t;
    if(isDisplayOff)
    {
      isDisplayOff = false;
    }
    else {
      currentDisplay++;
      ucg.clearScreen();
      if(currentDisplay > 2)
        currentDisplay = 0;
    }
    showDisplay(true);
  }
  buttonState = newButtonState;

  //Parse serial NMEA data
  readData();
}
