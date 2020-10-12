
#ifndef __GnssMonitor_h__
#define __GnssMonitor_h__

#include "NMEAParser.h"

typedef void (*LocationHandler)(const char *type);
NMEAParser<6> mParser; 
static LocationHandler mLocationHandler;
// State variables
float mSpeed = 0;
float mCourse = 0;
float mLatitude = NAN;
char mLatIndicator = 'N';
float mLongitude = NAN;
char mLonIndicator = 'E';
float mElevation = NAN;
String mMode = "---";
float mVerticalError = NAN;
float mHorizontalError = NAN;
float mHdop = NAN;
float mVdop = NAN;
float mPdop = NAN;
int mSats = 0;
int mQuality = 0;
String mGpstime = "---";
int mSatsBySystem [5] = { 0, 0, 0, 0, 0 }; 

void raiseLocationEvent(const char *type)
{
  if (mLocationHandler != NULL)
  {
     mLocationHandler(type);
  }
};
void handleRMCMessage(void)
{
  String speedStr;
  if (mParser.getArg(6, speedStr) && speedStr.length() > 0)
  {
    mSpeed = speedStr.toFloat();
  }
  else 
  {
    mSpeed = 0;
  }
  String courseStr;
  if (mParser.getArg(7, courseStr) && courseStr.length() > 0)
  {
    mCourse = courseStr.toFloat();
  }
  raiseLocationEvent("RMC");
};
void handleGGAMessage(void)
{
  float lat;
  float lon;
  if (mParser.getArg(1, lat) && mParser.getArg(3, lon))
  {
    // Write latitude
    String strlat;
    mParser.getArg(1, strlat);
    mParser.getArg(2, mLatIndicator);
    mLatitude = strlat.substring(0,2).toFloat() + strlat.substring(2).toFloat() / 60;
    // Write longitude
    String strlon;
    mParser.getArg(3, strlon);
    mParser.getArg(4, mLonIndicator);
    mLongitude = strlon.substring(0,3).toFloat() + strlon.substring(3).toFloat() / 60;
  }
  else
  {
    mLatitude = NAN;
    mLongitude = NAN;
  }
  // Write elevation
  float el;
  if(mParser.getArg(8, el))
  {
    mElevation = el;
  }
  else
  {
    mElevation = NAN;
  }
  
  int quality = 0;
  
  mParser.getArg(5, quality);
  if(quality == 0)
     mMode = "No Fix";
  else if(quality == 1)
     mMode = "GPS Fix";
  else if(quality == 2)
     mMode = "Differential";
  else if(quality == 3)
     mMode = "GPS PPS";
  else if(quality == 4)
     mMode = "RTK";
  else if(quality == 5)
     mMode = "RTK Float";
  else if(quality == 6)
     mMode = "Estimated";
  else
     mMode = "---";
  mQuality = quality;
  int satCount = 0;
  mParser.getArg(6, satCount);
  mSats = satCount;
  String time;
  if(mParser.getArg(0, time))
      mGpstime = time.substring(0,2) + ":" + time.substring(2,4) + ":" + time.substring(4,6);
  else
    mGpstime = "---";
  raiseLocationEvent("GGA");
};

void handleGSAMessage(void)
{
  int count = mParser.argCount();
  int systemId;
  if(mParser.getArg(count-1, systemId))
  {
    if(systemId > 0 && systemId<5)
    {
      int satCount = 0;
      for(int i=2;i<count-4;i++)
      {
        String satId;
        if(mParser.getArg(i, satId) && satId.length() > 0)
          satCount++;
      }
      mSatsBySystem[systemId-1] = satCount;
      // Get DOP values from GPS system (the values should be the same for all systems, so just limiting to pulling it from GPS)
      if(systemId == 1) 
      {
        if(count > 4 && mParser.getArg(count - 4, mPdop) && mParser.getArg(count - 3, mHdop) && mParser.getArg(count - 2, mVdop)) 
        {
        }
        else 
        {
           mPdop = NAN;
           mHdop = NAN;
           mVdop = NAN;
        }
      }
    }
  }
  raiseLocationEvent("GSA");
};

void handleGSTMessage(void)
{
  float error_lat;
  float error_lon;
  
  if (mParser.getArg(5, error_lat) && mParser.getArg(6, error_lon))
  {
    float error = sqrt(error_lat * error_lat + error_lon * error_lon);
    mHorizontalError = error;
  }
  else
  {
    String error;
    mParser.getArg(5, error);
    mHorizontalError = NAN;
  }
  float error_v;
  if (mParser.getArg(7, error_v))
  {
    mVerticalError = error_v;
  }
  else
  {
    mVerticalError = NAN;
  }
  raiseLocationEvent("GST");
};

void handleGSVMessage(void)
{
  //char buf[6];
  //mParser.getType(buf);  
};

void handleVTGMessage(void)
{/*
  if (!mParser.getArg(0, mCourse) && !mParser.getArg(1, mCourse))
  {
    mCourse = -999;
  }
  if (!mParser.getArg(2, mSpeed))
  {
    mSpeed = -1;
  }
  raiseLocationEvent("VTG");*/
};

void unknownCommandMessage()
{
  //char buf[6];
  //mParser.getType(buf);
  //if(buf == "GNVTG" || buf == "GNGLL)
   // return;
  //write(buf, 1, 0);
};

void errorHandlerMessage()
{
  //char buf[6];
  //mParser.getType(buf);  
  //write(String(mParser.error()) + " " + buf, 6, 0); 
};

  void initNmeaParser() 
  {    
     mParser.addHandler("GNRMC", handleRMCMessage);
     mParser.addHandler("GNGGA", handleGGAMessage);
     mParser.addHandler("GNGST", handleGSTMessage);
     mParser.addHandler("GNGSA", handleGSAMessage);
     mParser.addHandler("--GSV", handleGSVMessage);
     mParser.addHandler("GNVTG", handleVTGMessage);
     mParser.setDefaultHandler(unknownCommandMessage);
  };
  
  void setLocationHandler(LocationHandler inHandler)
  {
    mLocationHandler = inHandler;
  };
  
char buf[1024];
void readData()
{
  int count;
  if (Serial1.available())
  {
    while((count = Serial1.readBytes(buf, 1024)) > 0)
    {
      for(int i=0;i<count;i++)
        mParser << buf[i];
    }
  }
};

  float speed() { return mSpeed; }
  float course() { return mCourse; }
  float latitude() { return mLatitude; }
  char latIndicator() { return mLatIndicator; }
  float longitude() { return mLongitude; }
  char lonIndicator() { return mLonIndicator; }
  float elevation() { return mElevation; }
  String mode() { return mMode; }
  float verticalError() { return mVerticalError; }
  float horizontalError() { return mHorizontalError; }
  float hdop() { return mHdop; }
  float vdop() { return mVdop; }
  float pdop() { return mPdop; }
  int sats() { return mSatsBySystem[0] + mSatsBySystem[1] + mSatsBySystem[2] + mSatsBySystem[3] + mSatsBySystem[4]; }
  int quality() { return mQuality; }
  String gpstime() { return mGpstime; }
#endif
