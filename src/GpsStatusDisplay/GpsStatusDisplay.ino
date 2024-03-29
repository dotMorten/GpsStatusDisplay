/*
  09/16/2020
  Author: Morten Nielsen
  Platforms: SAMD51
  Language: C++
  File: GpsStatusDisplay.ino

  3rd party libs used:
  ucglib : https://github.com/olikraus/ucglib  v1.5.2 http://librarymanager/All#Ucglib
  SparkFun u-blox lib: https://github.com/sparkfun/SparkFun_u-blox_GNSS_Arduino_Library
*/

#include <Wire.h>
#include <Ucglib.h> //http://librarymanager/All#Ucglib

// Initialize the OLED display:
Ucglib_SSD1351_18x128x128_FT_HWSPI ucg(/*cd=*/ 1, /*cs=*/ 0, /*reset=*/ 4);

#include "SparkFun_u-blox_GNSS_Arduino_Library.h"
#include "GnssMonitor.h"
#include "drawhelpers.h"
#include "Menu.h"
#include "buttons.h"
#include "bitmaps.h"
#include "statuspages.h"
#include "settingsMenu.h"

bool ledstate;
bool isDisplayOff;
SFE_UBLOX_GNSS gps;
Menu *currentMenu = nullptr;
bool isInitializing = true;
bool gpsConnectionError = false;

int16_t currentDisplay = 2;

void showDisplay(bool newPage)
{
   if(isDisplayOff)
     return;
   drawStatusBar(newPage);
   if (currentMenu)
     return;
   if(gpsConnectionError)
     return;
   if(currentDisplay == 0)
      drawPage_ErrorInfo(newPage);
   else if(currentDisplay == 1)
      drawPage_NavigationInfo(newPage);
   else if(currentDisplay == 2)
      drawPage_LocationInfo(newPage);
}
void drawStatusBar(bool newPage)
{
  ucg.setFont(ucg_font_helvR08_hr);  
  ucg.setFontMode(UCG_FONT_MODE_SOLID);
  
  if(newPage && currentMenu==nullptr) {
    ucg.setColor(255, 255, 0);
    ucg.drawHLine(0,10,128);
  }
  
  if(gpsConnectionError)
  {
    ucg.setColor(255, 0, 0);
    ucg.setPrintPos(20,64);
    ucg.print("ERROR: GPS CHIP");
    ucg.setPrintPos(20,84);
    ucg.print("NOT DETECTED");
    return;
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
  String str = "      ";
  auto err = horizontalError();
  if(!isnan(err)) {
    int decimals = 1;
    if(err < .02)
      decimals = 3;
    else if(err < .2)
      decimals = 2;
    else if(err < 2)
      decimals = 1;
    str = " " + String(err, decimals) + "m  ";
  }
  auto width = ucg.getStrWidth(str.c_str());
  ucg.setColor(255, 255, 0);
  ucg.setPrintPos(64-width/2+1,8); 
  ucg.print(str.c_str());
  if(!isnan(err))
    drawBitmap(64-width/2-3, 1, 5, 7, plusminus, 255, 255, 0);

  // Satellite count
  if(newPage)
    drawBitmap(100, 1, 10, 6, satBitmap, 255, 255, 0);
  ucg.setColor(255, 255, 0);
  ucg.setPrintPos(115,8);
  ucg.print(String(sats()).c_str());
}
const int buttonPin = 10;
unsigned long lastButtonPressTime;
//MenuItem menu;

bool hasNewData = false;
void onPVTDataChanged(UBX_NAV_PVT_data_t pvt)
{
  if(ledstate)
    digitalWrite(LED_BUILTIN, HIGH);
  else
    digitalWrite(LED_BUILTIN, LOW);
  ledstate = !ledstate;

  onPVTDataChanged_(pvt);
  hasNewData = true;
}
void OnHPPOSLLHChanged(UBX_NAV_HPPOSLLH_data_t hppos)
{
  OnHPPOSLLHChanged_(hppos);
  hasNewData = true;
}
void OnDOPChanged(UBX_NAV_DOP_data_t dop)
{
  OnDOPChanged_(dop);
  hasNewData = true;
}
void configureGps()
{
  //Wire.setClock(400000); //Increase I2C clock speed to 400kHz
  gps.setI2COutput(COM_TYPE_UBX); //Sets I2C to communicate with just the UBX protocol
  // Ensure UART2 bluetooth is configured correctly: 115200 baud, 1 stopbit, 8 databits, parity none(0)
  gps.setVal32(UBLOX_CFG_UART2_BAUDRATE, 115200, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR);
  gps.setVal8(UBLOX_CFG_UART2_STOPBITS, 1, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR);
  gps.setVal8(UBLOX_CFG_UART2_DATABITS, 8, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR);
  gps.setVal8(UBLOX_CFG_UART2_PARITY, 0, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR);

  //gps.enableDebugging(Serial, true); 
  gps.setAutoPVTcallback(onPVTDataChanged);
  gps.setAutoHPPOSLLHcallback(OnHPPOSLLHChanged);
  gps.setAutoDOPcallback(OnDOPChanged);
}

void setup()
{
  Serial.begin(115200);
  // while (!Serial); //Wait for user to open terminal
  Serial.println("App start");
  lastButtonPressTime = millis();
  Wire.begin();   
#if defined(AM_PART_APOLLO3)
  Wire.setPullups(0); // On the Artemis, we can disable the internal I2C pull-ups too to help reduce bus errors
#endif
  //Serial1.setTimeout(10);
  initButtons();
  pinMode(LED_BUILTIN, OUTPUT);
  ucg.begin(UCG_FONT_MODE_SOLID);
  ucg.clearScreen();
  
  ucg.setColor(255, 255, 255);
  ucg.setFontMode(UCG_FONT_MODE_SOLID);

  int count = 0;
  bool gpsStarted = gps.begin(Wire);
  while(!gpsStarted && count < 20)
  {    
    delay(100); // Wait for GPS to start up
    gpsStarted = gps.begin(Wire);
    count++;
  }
  if(!gpsStarted)
      gpsConnectionError = true;
  if(!gpsConnectionError){
    configureGps();
  }
  initSettingsMenu(&gps);
  showDisplay(true);
  menu->setDisplay(&ucg);
  digitalWrite(LED_BUILTIN, HIGH);
}

int buttonState = KEY_NONE;
void loop()
{
  bool requireFullRedraw = false;
  
  if(!gpsConnectionError)
    gps.checkUblox(); // Check for the arrival of new data and process it.
    gps.checkCallbacks(); // Check if any callbacks are waiting to be processed. 
  // Flash LED on each new data
  if(!hasNewData)
  {
    delay(1);
  }
  // Process UI
  
  auto t = millis();
  auto newButtonState = getButtonState();
  if(t - lastButtonPressTime > 30000 && !isDisplayOff)
  {
    // Turn display off after 30 seconds of not pressing any button
    isDisplayOff = true;
    ucg.powerDown();
    currentMenu = nullptr;
  }
  else if(newButtonState != KEY_NONE && isDisplayOff)
  {
    // A key was pressed -> Turn screen back on
    isDisplayOff = false;
    ucg.clearScreen();
    ucg.powerUp();
    requireFullRedraw = true;
  }
  else if(currentMenu == nullptr && newButtonState == KEY_LEFT && buttonState == KEY_LEFT && t - lastButtonPressTime > 1000)
  {  
    //Enter menu on hold
    currentMenu = menu;
    menu->reset();
    menu->initScreen();
  }
  else if(currentMenu) // A menu is currently active
  {
    auto result = processMenu(currentMenu, &gps);
    if(result == MENU_RESULT_EXIT)
    {
      currentMenu = nullptr;
      ucg.clearScreen();
      requireFullRedraw = true;
    }
  }
  else if (newButtonState != buttonState && newButtonState != KEY_NONE)
  {
    // flip status pages
    if(newButtonState == KEY_RIGHT || newButtonState == KEY_DOWN)
    {
      currentDisplay++;
      if(currentDisplay > 2)
        currentDisplay = 0;
    }
    else if(newButtonState == KEY_UP || newButtonState == KEY_LEFT)
    {
      currentDisplay--;
      if(currentDisplay < 0)
        currentDisplay = 2;
    }
    ucg.clearScreen();
    requireFullRedraw = true;
  }
  if(hasNewData || requireFullRedraw)
    showDisplay(requireFullRedraw);
  if (newButtonState != buttonState)
      lastButtonPressTime = t; //reset button press inactivity timer
  buttonState = newButtonState;
  hasNewData = false;
}
