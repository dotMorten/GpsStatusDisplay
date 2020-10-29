/*
  09/16/2020
  Author: Morten Nielsen
  Platforms: SAMD51
  Language: C++
  File: GpsStatusDisplay.ino

  3rd party libs used:
  ucglib : https://github.com/olikraus/ucglib   http://librarymanager/All#Ucglib
  SparkFun u-blox lib: https://github.com/sparkfun/SparkFun_Ublox_Arduino_Library   http://librarymanager/All#SparkFun_Ublox_GPS
*/

#include <Wire.h>
#include <Ucglib.h> //http://librarymanager/All#Ucglib

// Initialize the OLED display:
Ucglib_SSD1351_18x128x128_FT_HWSPI ucg(/*cd=*/ 1, /*cs=*/ 0, /*reset=*/ 4);

//#include <SparkFun_Ublox_Arduino_Library.h> //http://librarymanager/All#SparkFun_Ublox_GPS
#include "SparkFun_Ublox_Arduino_Library.h"
#include "GnssMonitor.h"
#include "drawhelpers.h"
#include "Menu.h"
#include "buttons.h"
#include "bitmaps.h"
#include "statuspages.h"
#include "settingsMenu.h"

bool ledstate;
bool isDisplayOff;
SFE_UBLOX_GPS gps;
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
  auto err = horizontalError();
  int decimals = 1;
  if(err < .02)
    decimals = 3;
  else if(err < .2)
    decimals = 2;
  else if(err < 2)
    decimals = 1;
  auto str = (" " + String(err, decimals) + "m  ").c_str();
  if(isnan(err))
    str = "      ";
  auto width = ucg.getStrWidth(str);
  ucg.setColor(255, 255, 0);
  ucg.setPrintPos(64-width/2+1,8); 
  ucg.print(str);
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

void configureGps()
{
  Wire.setClock(400000); //Increase I2C clock speed to 400kHz
  gps.setI2COutput(COM_TYPE_UBX); //Sets I2C to communicate with just the UBX protocol
  gps.setAutoPVT(true, true); //Tell the GPS to "send" each solution
  gps.setAutoHPPOSLLH(true, true); //Tell the GPS to "send" each high-accuracy solution, accuracy etc
  gps.setAutoDOP(true, true); //Tell the GPS to "send" each DOP value
  gps.setSerialRate(115200, COM_PORT_UART2); // Configure speed on bluetooth port
  gps.saveConfiguration(); //Save the current settings to flash and BBR
  //gps.enableDebugging(Serial);
}

void setup()
{
  Serial.begin(115200);
  //while (!Serial); //Wait for user to open terminal
  Serial.println("App start");
  lastButtonPressTime = millis();
  Wire.begin();   
  
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
  bool hasNewData = false;
  bool requireFullRedraw = false;
  if(!gpsConnectionError)
    hasNewData = readData(&gps);
  // Flash LED on each new data
  if(hasNewData)
  {
    if(ledstate)
     digitalWrite(LED_BUILTIN, HIGH);
    else
     digitalWrite(LED_BUILTIN, LOW);
    ledstate = !ledstate;
  }
  else {
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
  else if(currentMenu == nullptr && newButtonState == KEY_SELECT && buttonState == KEY_SELECT && t - lastButtonPressTime > 1000)
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
  else if (newButtonState != buttonState && (newButtonState != KEY_NONE && newButtonState != KEY_SELECT))
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
}
