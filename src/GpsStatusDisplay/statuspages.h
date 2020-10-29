#ifndef __statuspages_h__
#define __statuspages_h__

#include <Ucglib.h> //http://librarymanager/All#Ucglib
#include "bitmaps.h"
#include "drawhelpers.h"

bool privacy = false; // set to true to limit location precision for privacy (good for screenshots)



void writeDop(float dop, uint8_t row)
{
   if(isnan(dop))
     writepair("", "---", row);
   else {
    if(dop < 7)
      ucg.setColor(0, 255, 0);  
    else
      ucg.setColor(255, 0, 0);  
    writepair("", String(dop), row);
  }
}

void drawPage_ErrorInfo(bool newPage)
{    
  if(newPage)
  {
    auto fontHeight = 14;
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
  drawStringCenter(getLineHeight() * 2, ("  " + mode() + "  ").c_str());
  if(isnan(horizontalError()))
     writepair("", "---", 4);
  else
     writepair("", String(horizontalError(), 3) + "m", 4);
  if(isnan(verticalError()))
     writepair("", "---", 5);
  else
     writepair("", String(verticalError(), 3) + "m", 5);
  writeDop(hdop(), 7);
  writeDop(vdop(), 8);
  writeDop(pdop(), 9);
}
float lastCourse;
void drawPage_NavigationInfo(bool newPage)
{ 
  ucg.setColor(255, 255, 255);
  auto currentCourse = course();
  bool isValid = true;
  if(isnan(currentCourse)) {
    currentCourse = 0;
    isValid = false;
  }
  if(newPage || lastCourse != currentCourse)
  {
    // Redraw compass
    if(!newPage){
      ucg.setColor(0, 0, 0);
      ucg.drawBox(1, 12, 75, 75);
      ucg.setColor(255, 255, 255);
    }
    if(isValid)
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
  if(isValid)
  {   drawString(80, 53, String(currentCourse,0) + "°");
      drawString(40, 110, String(speed(),1) + "kn");
  }
  else
  {
    drawString(80, 53, "---");
    drawString(40, 110, "---");
  }
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
  auto _latitude = latitude();
  auto _longitude = longitude();
  if(privacy)
  {
    decimals = 2; 
    lat = String(_latitude, decimals) + "*****" + latIndicator();
    lon = String(_longitude, decimals) + "*****" + lonIndicator();
  }
  else {
    lat = String(_latitude, decimals) + "°" + latIndicator();
    lon = String(_longitude, decimals) + "°" + lonIndicator();
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
  auto altitude = elevation();
  auto z = String(altitude, decimals) + "m";
  if(isnan(_latitude))
    lat = "---";
  if(isnan(_longitude))
    lon = "---";
  if(isnan(altitude))
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

#endif
