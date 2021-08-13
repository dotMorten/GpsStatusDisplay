#ifndef __settingsMenu_h__
#define __settingsMenu_h__

#include "ubloxextensions.h"

const int CONNECTIONSMENUID=10;
const int NMEAMENUID=20;
const int NMEAMSGMENUID = NMEAMENUID + 100; //NOTE This goes to 120..132
const int GNSSMENUID=50;
const int ABOUTMENUID=80;
const int USBSETTINGSMENUID = CONNECTIONSMENUID+100;
const int BLUETOOTHSETTINGSMENUID = CONNECTIONSMENUID+200;
  MenuItem *gnssMenuItems[] =
  {
    new MenuItem(GNSSMENUID + 6, "Rate", "---"),
    new MenuItem(GNSSMENUID + 5, "High Precision", "On"),
    new MenuItem(GNSSMENUID + 1, "GPS", "Enabled"),
    new MenuItem(GNSSMENUID + 2, "GLONASS", "Enabled"),
    new MenuItem(GNSSMENUID + 3, "Galileo", "Enabled"),
    new MenuItem(GNSSMENUID + 4, "Beidou", "Enabled"),
  };
  MenuItem *aboutMenuItems[] =
  {
    new MenuItem(ABOUTMENUID + 1, "Version", "1.0"),
    new MenuItem(ABOUTMENUID + 2, "Device Info"),
    new MenuItem(ABOUTMENUID + 3, "Reset"),
    new MenuItem(ABOUTMENUID + 4, "Privacy Mode", "Off"),
  };
  MenuItem *connectionsMenuItems[] =
  {
    new MenuItem(CONNECTIONSMENUID + 1, "USB", "---"),
    new MenuItem(CONNECTIONSMENUID + 2, "Bluetooth", "---"),
  };
  MenuItem *enabledNmeaMessagesItems[] = {    
    new MenuItem(NMEAMSGMENUID, "GGA", "--", UBLOX_CFG_MSGOUT_NMEA_ID_GGA_USB),
    new MenuItem(NMEAMSGMENUID, "GLL", "--", UBLOX_CFG_MSGOUT_NMEA_ID_GLL_USB),
    new MenuItem(NMEAMSGMENUID, "GSA", "--", UBLOX_CFG_MSGOUT_NMEA_ID_GSA_USB),
    new MenuItem(NMEAMSGMENUID, "GSV", "--", UBLOX_CFG_MSGOUT_NMEA_ID_GSV_USB),
    new MenuItem(NMEAMSGMENUID, "RMC", "--", UBLOX_CFG_MSGOUT_NMEA_ID_RMC_USB),
    new MenuItem(NMEAMSGMENUID, "VTG", "--", UBLOX_CFG_MSGOUT_NMEA_ID_VTG_USB),
    new MenuItem(NMEAMSGMENUID, "GRS", "--", UBLOX_CFG_MSGOUT_NMEA_ID_GRS_USB),
    new MenuItem(NMEAMSGMENUID, "GST", "--", UBLOX_CFG_MSGOUT_NMEA_ID_GST_USB),
    new MenuItem(NMEAMSGMENUID, "ZDA", "--", UBLOX_CFG_MSGOUT_NMEA_ID_ZDA_USB),
    new MenuItem(NMEAMSGMENUID, "GBS", "--", UBLOX_CFG_MSGOUT_NMEA_ID_GBS_USB),
    new MenuItem(NMEAMSGMENUID, "DTM", "--", UBLOX_CFG_MSGOUT_NMEA_ID_DTM_USB),
    new MenuItem(NMEAMSGMENUID, "GNS", "--", UBLOX_CFG_MSGOUT_NMEA_ID_GNS_USB),
    new MenuItem(NMEAMSGMENUID, "VLW", "--", UBLOX_CFG_MSGOUT_NMEA_ID_VLW_USB),
  };
    MenuItem *nmeaSettingsMenuItems[] =
  {
    new MenuItem(NMEAMENUID + 1, "Version", "---"),
    new MenuItem(NMEAMENUID + 2, "Messages", enabledNmeaMessagesItems, 13),
    new MenuItem(NMEAMENUID + 3, "High Precision", "--"), // Enable high precision mode: CFG_NMEA_HIGHPREC  on/off
    new MenuItem(NMEAMENUID + 4, "Compat Mode", "--"), // NMEA Compat mode: On/Off  CFG-NMEA-COMPAT (0x10930003)
    new MenuItem(NMEAMENUID + 5, "Limit 82 chars", "--"), // Enable strict limit to 82 characters maximum NMEA message length: On/Off  - CFG-NMEA-LIMIT82 (0x10930005)
  };

  MenuItem *mainMenuItems[] =
  {
    new MenuItem(1, "Outputs", connectionsMenuItems, 2),
    new MenuItem(2, "NMEA", nmeaSettingsMenuItems, 5),
    new MenuItem(3, "RTCM", ">"),
    new MenuItem(4, "SBAS", "---"),
    new MenuItem(5, "GNSS", gnssMenuItems, 6),
    new MenuItem(6, "Info/About", aboutMenuItems, 4)
  };
Menu *menu = new Menu(new MenuItem(0, "Settings", mainMenuItems, 6));
MenuItem *gpsInfoMenu[] =
    {
      new MenuItem(0, "Hardware Version"),
      new MenuItem(0, "---"),
      new MenuItem(0, "Software Version"),
      new MenuItem(0, "---"),
      new MenuItem(0, ""),
      new MenuItem(0, ""),
      new MenuItem(0, ""),
      new MenuItem(0, ""),
      new MenuItem(0, ""),
      new MenuItem(0, ""),
      new MenuItem(0, ""),
      new MenuItem(0, ""),
      new MenuItem(0, ""),
    };
int initSettingsMenu(SFE_UBLOX_GNSS *gps)
{
  auto sbas = gps->getVal8(CFG_SBAS_USE_DIFFCORR);
  mainMenuItems[3]->setValue(sbas == 0 ? "Disabled" : "Enabled");
  auto frequency = gps->getVal16(UBLOX_CFG_RATE_MEAS);
  gnssMenuItems[0]->setValue((String(frequency) + " ms").c_str());
  for(uint8_t i = 0; i<13; i++) {
    auto child = enabledNmeaMessagesItems[i];
    child->setValue(gps->getVal8(child->getTag()) > 0 ? "Enabled" : "Disabled");
  }
  //Get NMEA version
  uint8_t nmeaVersion = gps->getVal8(CFG_NMEA_PROTVER);
  if(nmeaVersion == CFG_NMEA_PROTVER_V21)
    nmeaSettingsMenuItems[0]->setValue("v2.1");
  else if(nmeaVersion == CFG_NMEA_PROTVER_V23)
    nmeaSettingsMenuItems[0]->setValue("v2.3");
  else if(nmeaVersion == CFG_NMEA_PROTVER_V40)
    nmeaSettingsMenuItems[0]->setValue("v4.0");
  else if(nmeaVersion == CFG_NMEA_PROTVER_V41)
    nmeaSettingsMenuItems[0]->setValue("v4.1");
  else if(nmeaVersion == CFG_NMEA_PROTVER_V411)
    nmeaSettingsMenuItems[0]->setValue("v4.11");
  else 
    nmeaSettingsMenuItems[0]->setValue("v?.?");

  nmeaSettingsMenuItems[2]->setValue(gps->getVal8(CFG_NMEA_HIGHPREC) ? "On" : "Off");
  nmeaSettingsMenuItems[3]->setValue(gps->getVal8(CFG_NMEA_COMPAT) ? "On" : "Off");
  nmeaSettingsMenuItems[4]->setValue(gps->getVal8(CFG_NMEA_LIMIT82) ? "On" : "Off");
  
  auto nmeaOn = gps->getVal8(CFG_USBOUTPROT_NMEA);
  auto rtcmOn = gps->getVal8(CFG_USBOUTPROT_RTCM3X);
  if(nmeaOn && rtcmOn)
    connectionsMenuItems[0]->setValue("NMEA+RTCM");
  else if(nmeaOn)
    connectionsMenuItems[0]->setValue("NMEA");
  else if(rtcmOn)
    connectionsMenuItems[0]->setValue("RTCM");
  else
    connectionsMenuItems[0]->setValue("Off");
      
    
  nmeaOn = gps->getVal8(CFG_UART2OUTPROT_NMEA);
  rtcmOn = gps->getVal8(CFG_UART2OUTPROT_RTCM3X);
  if(nmeaOn && rtcmOn)
    connectionsMenuItems[1]->setValue("NMEA+RTCM");
  else if(nmeaOn)
    connectionsMenuItems[1]->setValue("NMEA");
  else if(rtcmOn)
    connectionsMenuItems[1]->setValue("RTCM");
  else
    connectionsMenuItems[1]->setValue("Off");
  
/*
  int menuCount = 4;
  if(getModuleInfo(gps, 1100))
  {
    gpsInfoMenu[3]->setTitle(minfo.swVersion);
    gpsInfoMenu[1]->setTitle(minfo.hwVersion);
    for (int i = 0; i < minfo.extensionNo; i++)
    {
      gpsInfoMenu[4+i]->setTitle(minfo.extension[i]);
    }
    menuCount+=minfo.extensionNo;
  }
  else
  {
    gpsInfoMenu[3]->setTitle("N/A");
    gpsInfoMenu[1]->setTitle("N/A");
  }
  aboutMenuItems[1]->setChildren(gpsInfoMenu, menuCount);
  */
};
int resetGps(SFE_UBLOX_GNSS *gps)
{
  gps->factoryReset();
  gps->setAutoPVT(true, true); //Tell the GPS to "send" each solution
  gps->setAutoHPPOSLLH(true); //Tell the GPS to "send" each high-accuracy solution, accuracy etc
  gps->setAutoDOP(true, true); //Tell the GPS to "send" each DOP value
  gps->setVal8(CFG_NMEA_HIGHPREC, 1, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR); // Ensure high precision mode for NMEA is on
  gps->setSerialRate(115200, COM_PORT_UART2); // Configure speed on bluetooth port
  gps->saveConfiguration(); //Save the current settings to flash and BBR
  initSettingsMenu(gps);
}
int processMenu(Menu *currentMenu, SFE_UBLOX_GNSS *gps)
{
  // Process menu if active
    auto result = currentMenu->processMenu();
    if(result == MENU_RESULT_EXIT)
    {
      return result;
    }
    else if(result == 4) // SBAS
    {
      uint8_t sbas = gps->getVal8(CFG_SBAS_USE_DIFFCORR);
      if(sbas == 0)
        sbas = 1;
      else sbas = 0;
      bool isok = gps->setVal8(CFG_SBAS_USE_DIFFCORR, sbas, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR);
      mainMenuItems[3]->setValue(sbas == 0 ? "Disabled" : "Enabled");
      
      /*if(item->getValue() == "Enabled")
        item->setValue("Disabled");
      else
        item->setValue("Enabled");*/
      currentMenu->refresh();
    }
    else if(result == CONNECTIONSMENUID + 1) // USB Output
    {
      auto item = currentMenu->selectedMenuItem();
      bool nmeaOn = false;
      bool rtcmOn = false;
      if(item->getValue() == "NMEA") {
        rtcmOn=true;
      }     
      else if(item->getValue() == "RTCM") {
        rtcmOn=false;
        nmeaOn=false;
      }
      else {
        nmeaOn = true;
      }    
      gps->setVal8(CFG_USBOUTPROT_NMEA, nmeaOn ? 1 : 0, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR);
      gps->setVal8(CFG_USBOUTPROT_RTCM3X, rtcmOn ? 1 : 0, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR);
      if(nmeaOn && rtcmOn)
        connectionsMenuItems[0]->setValue("NMEA+RTCM");
      else if(nmeaOn)
        connectionsMenuItems[0]->setValue("NMEA");
      else if(rtcmOn)
        connectionsMenuItems[0]->setValue("RTCM");
      else if(!nmeaOn && !rtcmOn)
        connectionsMenuItems[0]->setValue("Off");
      currentMenu->refresh();
    }
    else if(result == CONNECTIONSMENUID + 2) // Bluetooth Output
    {
      auto item = currentMenu->selectedMenuItem();
      bool nmeaOn = false;
      bool rtcmOn = false;
      if(item->getValue() == "NMEA") {
        rtcmOn=true;
      }
      else if(item->getValue() == "RTCM") {
        rtcmOn=false;
        nmeaOn=false;
      }
      else {
        nmeaOn = true;
      }    
      gps->setVal8(CFG_UART2OUTPROT_NMEA, nmeaOn ? 1 : 0, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR);
      gps->setVal8(CFG_UART2OUTPROT_RTCM3X, rtcmOn ? 1 : 0, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR);
      if(nmeaOn && rtcmOn)
        connectionsMenuItems[1]->setValue("NMEA+RTCM");
      else if(nmeaOn)
        connectionsMenuItems[1]->setValue("NMEA");
      else if(rtcmOn)
        connectionsMenuItems[1]->setValue("RTCM");
      else if(!nmeaOn && !rtcmOn)
        connectionsMenuItems[1]->setValue("Off");
      currentMenu->refresh();
    }
    else if(result == GNSSMENUID + 6) // Navigation rate
    {
       auto item = currentMenu->selectedMenuItem();
       uint16_t value = 1000;
       if(item->getValue() == "1000 ms")   
         value = 500;
       else if(item->getValue() == "500 ms")
         value = 250;         
       else if(item->getValue() == "250 ms")
         value = 100;        
       else if(item->getValue() == "100 ms")
         value = 1000;
       gps->setVal16(UBLOX_CFG_RATE_MEAS, value, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR);
       item->setValue((String(value) + " ms").c_str());
       currentMenu->refresh();
    }
    else if(result == NMEAMENUID + 1) // NMEA Version
    {
      auto value = nmeaSettingsMenuItems[0]->getValue();
      uint8_t nmeaVersion = gps->getVal8(CFG_NMEA_PROTVER);      
      if(value == "v2.1")
        nmeaVersion = CFG_NMEA_PROTVER_V23;
      else if(value == "v2.3")
        nmeaVersion = CFG_NMEA_PROTVER_V40;
      else if(value == "v4.0")
        nmeaVersion = CFG_NMEA_PROTVER_V41;
      else if(value == "v4.1")
        nmeaVersion = CFG_NMEA_PROTVER_V411;
      else if(value == "v4.11")
        nmeaVersion = CFG_NMEA_PROTVER_V21;
      if(gps->setVal(CFG_NMEA_PROTVER, nmeaVersion, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR))
      {
        if(nmeaVersion == CFG_NMEA_PROTVER_V21)
          nmeaSettingsMenuItems[0]->setValue("v2.1");
        else if(nmeaVersion == CFG_NMEA_PROTVER_V23)
          nmeaSettingsMenuItems[0]->setValue("v2.3");
        else if(nmeaVersion == CFG_NMEA_PROTVER_V40)
          nmeaSettingsMenuItems[0]->setValue("v4.0");
        else if(nmeaVersion == CFG_NMEA_PROTVER_V41)
          nmeaSettingsMenuItems[0]->setValue("v4.1");
        else if(nmeaVersion == CFG_NMEA_PROTVER_V411)
          nmeaSettingsMenuItems[0]->setValue("v4.11");
        currentMenu->refresh();
      }
    }
    else if(result == NMEAMENUID + 3) // NMEA high precision
    {      
      auto item = currentMenu->selectedMenuItem();
      bool isOn = item->getValue() == "On";
      if(gps->setVal8(CFG_NMEA_HIGHPREC, isOn ? 0 : 1, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR))
      {
        item->setValue(isOn ? "Off" : "On");
        currentMenu->refresh();
      }
    }
    else if(result == NMEAMENUID + 4) // NMEA Compat
    {
      auto item = currentMenu->selectedMenuItem();
      bool isOn = item->getValue() == "On";
      if(gps->setVal8(CFG_NMEA_COMPAT, isOn ? 0 : 1, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR))
      {
        item->setValue(isOn ? "Off" : "On");
        currentMenu->refresh();
      }
    }
    else if(result == NMEAMENUID + 5) // NMEA max 82 chars
    {
      auto item = currentMenu->selectedMenuItem();
      bool isOn = item->getValue() == "On";
      if(gps->setVal8(CFG_NMEA_LIMIT82, isOn ? 0 : 1, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR))
      {
        item->setValue(isOn ? "Off" : "On");
        currentMenu->refresh();
      }
    }
    else if(result == NMEAMSGMENUID) // NMEA message toggles
    {
      auto item = currentMenu->selectedMenuItem();
      //auto title = item->getTitle();
      uint32_t messageid = item->getTag();
      bool enabled =  (item->getValue() == "Enabled");
      bool isok = gps->setVal8(messageid, enabled ? 0 : 1, VAL_LAYER_FLASH + VAL_LAYER_RAM + VAL_LAYER_BBR);
      if(isok)
      {
        if(enabled) item->setValue("Disabled");
        else item->setValue("Enabled");
        currentMenu->refresh();
      }
    }
  
    else if(result == ABOUTMENUID + 3) // Reset
    {
      resetGps(gps);
    }
    else if(result == ABOUTMENUID + 4) // toggle privacy mode
    {
      privacy = !privacy;
      auto item = currentMenu->selectedMenuItem();
      item->setValue(privacy ? "On" : "Off");
      currentMenu->refresh();
    }
    else if(result > 0) {
      Serial.println("UNKNOWN MENU ID: " + String(result));
    }
  }

#endif
