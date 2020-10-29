
#ifndef __GnssMonitor_h__
#define __GnssMonitor_h__

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
int8_t mFixType = 0;
int mSats = 0;
int mQuality = 0;
String mGpstime = "---";
int mSatsBySystem [5] = { 0, 0, 0, 0, 0 }; 

char buf[1024];
bool readData(SFE_UBLOX_GPS *gps)
{
  bool result = false;
  if (gps->getPVT())
  {    
    mSpeed = gps->getGroundSpeed() * 0.00194384449;
    mCourse = gps->getHeading()/ 100000.0;
    mElevation = gps->getAltitude() / 1000.0;
    mLatitude = gps->getLatitude() / 10000000.0;
    if(mLatitude <0)
    {
      mLatIndicator = 'S';
      mLatitude = -mLatitude;
    }
    else 
      mLatIndicator = 'N';
    mLongitude = gps->getLongitude() / 10000000.0;
    if(mLongitude <0)
    {
      mLonIndicator = 'W';
      mLongitude = -mLongitude;
    }
    else 
      mLonIndicator = 'E';
    mGpstime = String(gps->getHour()) + ":" + String(gps->getMinute())+ ":" + String(gps->getSecond());
    auto sol = gps->getCarrierSolutionType();
    mFixType = gps->getFixType();
    bool isValid = gps->getGnssFixOk();
    bool diffSoln = gps->getDiffSoln();
    if(sol == 1) {
      mMode = "RTK Float";
      mQuality = 5;
    }
    else if(sol == 2) {
      mMode = "RTK";
      mQuality = 4;
    }
    else if(!isValid)
    {
      mQuality = 0;
      mMode = "No fix";
    }
    else if(diffSoln)
    {
      mMode = "Differential";
      mQuality = 2;
    }
    else {
      if(mFixType == 0) {
         mQuality = 0;
         mMode = "No fix";
      }
      else if(mFixType == 2) {
         mMode = "Dead Reckoning";
         mQuality = 0;
      }
      else if(mFixType == 2) {
         mMode = "GPS 2D";
         mQuality = 1;
      }
      else if(mFixType == 3) {
         mMode = "GPS";
         mQuality = 1;
      }
      else if(mFixType == 4) {
         mMode = "Differential";
         mQuality = 2;
      }
      else if(mFixType == 5) {
         mMode = "Time-only fix";
         mQuality = 0;
      }
      else
         mMode = String(mFixType) + ":" + String(sol); // "???";
    }
    mSats = gps->getSIV();
    result = true;

  }
  if (gps->getHPPOSLLH())
  {
    mVerticalError = gps->getVerticalAccuracy() / 10000.0;
    mHorizontalError = gps->getHorizontalAccuracy() / 10000.0;
    result = true;
  }
  if (gps->getDOP())
  {
    mPdop = gps->getPositionDOP() / 100.0;
    mHdop = gps->getHorizontalDOP() / 100.0;
    mVdop = gps->getVerticalDOP() / 100.0;
    result = true;
  }
  return result;
};
  bool hasFix() { return mFixType > 0; };
  int8_t fixType() { return mFixType; };
  float speed() { return hasFix() ? mSpeed : NAN; };
  float course() { return hasFix() ? mCourse : NAN; }; 
  float latitude() { return hasFix() ? mLatitude : NAN; };
  char latIndicator() { return mLatIndicator; }
  float longitude() { return hasFix() ? mLongitude : NAN; }
  char lonIndicator() { return mLonIndicator; }
  float elevation() { return hasFix() ? mElevation : NAN; }
  String mode() { return mMode; }
  float verticalError() { return hasFix() ? mVerticalError : NAN; }
  float horizontalError() { return hasFix() ? mHorizontalError : NAN; }
  float hdop() { return hasFix() && mHdop < 99.9 ? mHdop : NAN; }
  float vdop() { return hasFix() && mVdop < 99.9 ? mVdop : NAN; }
  float pdop() { return hasFix() && mPdop < 99.9 ? mPdop : NAN; }
  int sats() { return mSats; } // mSatsBySystem[0] + mSatsBySystem[1] + mSatsBySystem[2] + mSatsBySystem[3] + mSatsBySystem[4]; }
  int quality() { return mQuality; }
  String gpstime() { return mGpstime; }
#endif
