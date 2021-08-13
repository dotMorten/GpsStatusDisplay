
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
void onPVTDataChanged_(UBX_NAV_PVT_data_t pvt)
{
    mSpeed = pvt.gSpeed * 0.00194384449;
    mCourse = pvt.headVeh/ 100000.0;
    mElevation = pvt.hMSL / 1000.0;
    mLatitude = pvt.lat / 10000000.0;
    if(mLatitude <0)
    {
      mLatIndicator = 'S';
      mLatitude = -mLatitude;
    }
    else 
      mLatIndicator = 'N';
    mLongitude = pvt.lon / 10000000.0;
    if(mLongitude <0)
    {
      mLonIndicator = 'W';
      mLongitude = -mLongitude;
    }
    else 
      mLonIndicator = 'E';

   mGpstime = String(pvt.hour) + (pvt.min < 10 ? ":0" : ":") + String(pvt.min)+ (pvt.sec < 10 ? ":0" : ":") + String(pvt.sec);
   mFixType = pvt.fixType;
   auto flags = pvt.flags.bits.gnssFixOK;
   bool isValid = pvt.flags.bits.gnssFixOK == 1;
   uint8_t sol = pvt.flags.bits.carrSoln;
   uint8_t diffSoln = pvt.flags.bits.diffSoln;
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
    mSats = pvt.numSV;
}
void OnHPPOSLLHChanged_(UBX_NAV_HPPOSLLH_data_t hppos)
{
  mVerticalError = hppos.vAcc / 10000.0;
  mHorizontalError = hppos.hAcc / 10000.0;
}
void OnDOPChanged_(UBX_NAV_DOP_data_t dop)
{   
  mPdop = dop.pDOP / 100.0;
  mHdop = dop.hDOP / 100.0;
  mVdop = dop.vDOP / 100.0;  
}
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
