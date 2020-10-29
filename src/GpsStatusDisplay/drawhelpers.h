#ifndef __drawhelpers_h__
#define __drawhelpers_h__

#include <Ucglib.h> //http://librarymanager/All#Ucglib

int8_t getLineHeight()
{
  return ucg.getFontAscent() - ucg.getFontDescent();
}
void drawString(int x, int y, const char* text, bool rightAlign)
{
  auto ascent = ucg.getFontAscent();
  auto descent = ucg.getFontDescent();
  int width = 0;
  if(rightAlign)
   text = ("   " + String(text)).c_str();
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
  ucg.setPrintPos(x - width / 2,y + ascent/2);
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
  uint8_t y = row*getLineHeight() + 1;
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
/*
void writepair(const String lefttext, const float righttext, const String unit, const int decimals, const int row)
{
  if(isnan(righttext))
    writepair(lefttext, "---", row);
  else
    writepair(lefttext, String(righttext, decimals) + unit, row);
}
*/
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

#endif
