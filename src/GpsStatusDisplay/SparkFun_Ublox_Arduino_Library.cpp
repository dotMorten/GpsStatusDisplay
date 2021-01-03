/*
	This is a library written for the u-blox ZED-F9P and NEO-M8P-2
	SparkFun sells these at its website: www.sparkfun.com
	Do you like this library? Help support SparkFun. Buy a board!
	https://www.sparkfun.com/products/16481
	https://www.sparkfun.com/products/15136
	https://www.sparkfun.com/products/15005
	https://www.sparkfun.com/products/15733
	https://www.sparkfun.com/products/15193
	https://www.sparkfun.com/products/15210

  Original version by Nathan Seidle @ SparkFun Electronics, September 6th, 2018
	v2.0 rework by Paul Clark @ SparkFun Electronics, December 31st, 2020

	This library handles configuring and handling the responses
	from a u-blox GPS module. Works with most modules from u-blox including
	the Zed-F9P, NEO-M8P-2, NEO-M9N, ZOE-M8Q, SAM-M8Q, and many others.

	https://github.com/sparkfun/SparkFun_Ublox_Arduino_Library

	Development environment specifics:
	Arduino IDE 1.8.13

	SparkFun code, firmware, and software is released under the MIT License(http://opensource.org/licenses/MIT).
	The MIT License (MIT)
	Copyright (c) 2016 SparkFun Electronics
	Permission is hereby granted, free of charge, to any person obtaining a copy of this software and
	associated documentation files (the "Software"), to deal in the Software without restriction,
	including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
	and/or sell copies of the Software, and to permit persons to whom the Software is furnished to
	do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all copies or substantial
	portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT
	NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
	IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
	WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
	SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include "SparkFun_Ublox_Arduino_Library.h"

SFE_UBLOX_GPS::SFE_UBLOX_GPS(void)
{
  // Constructor
  if (debugPin >= 0)
  {
    pinMode((uint8_t)debugPin, OUTPUT);
    digitalWrite((uint8_t)debugPin, HIGH);
  }
}

//Allow the user to change packetCfgPayloadSize. Handy if you want to process big messages like RAWX
//This can be called before .begin if required / desired
void SFE_UBLOX_GPS::setPacketCfgPayloadSize(size_t payloadSize)
{
  if ((payloadSize == 0) && (payloadCfg != NULL))
  {
    // Zero payloadSize? Dangerous! But we'll free the memory anyway...
    delete[] payloadCfg;
    payloadCfg = NULL; // Redundant?
    packetCfg.payload = payloadCfg;
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setPacketCfgPayloadSize: Zero payloadSize! This will end _very_ badly..."));
  }

  else if (payloadCfg == NULL) //Memory has not yet been allocated - so use new
  {
    payloadCfg = new uint8_t[payloadSize];
    packetCfg.payload = payloadCfg;
    if (payloadCfg == NULL)
      if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
        _debugSerial->println(F("setPacketCfgPayloadSize: PANIC! RAM allocation failed! This will end _very_ badly..."));
  }

  else //Memory has already been allocated - so resize
  {
    uint8_t *newPayload = new uint8_t[payloadSize];
    for (size_t i = 0; (i < payloadSize) && (i < packetCfgPayloadSize); i++) // Copy as much existing data as we can
      newPayload[i] = payloadCfg[i];
    delete[] payloadCfg;
    payloadCfg = newPayload;
    packetCfg.payload = payloadCfg;
    if (payloadCfg == NULL)
      if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
        _debugSerial->println(F("setPacketCfgPayloadSize: PANIC! RAM resize failed! This will end _very_ badly..."));
  }

  packetCfgPayloadSize = payloadSize;
}

//Initialize the I2C port
boolean SFE_UBLOX_GPS::begin(TwoWire &wirePort, uint8_t deviceAddress)
{
  commType = COMM_TYPE_I2C;
  _i2cPort = &wirePort; //Grab which port the user wants us to use

  //We expect caller to begin their I2C port, with the speed of their choice external to the library
  //But if they forget, we start the hardware here.

  //We're moving away from the practice of starting Wire hardware in a library. This is to avoid cross platform issues.
  //ie, there are some platforms that don't handle multiple starts to the wire hardware. Also, every time you start the wire
  //hardware the clock speed reverts back to 100kHz regardless of previous Wire.setClocks().
  //_i2cPort->begin();

  _gpsI2Caddress = deviceAddress; //Store the I2C address from user

  //New in v2.0: allocate memory for the packetCfg payload here - if required. (The user may have called setPacketCfgPayloadSize already)
  if (packetCfgPayloadSize == 0)
    setPacketCfgPayloadSize(MAX_PAYLOAD_SIZE);

  //New in v2.0: allocate memory for the file buffer - if required. (The user should have called setFileBufferSize already)
  createFileBuffer();

  // Call isConnected up to three times - tests on the NEO-M8U show the CFG RATE poll occasionally being ignored
  boolean connected = isConnected();

  if (!connected)
    connected = isConnected();

  if (!connected)
    connected = isConnected();

  return (connected);
}

//Initialize the Serial port
boolean SFE_UBLOX_GPS::begin(Stream &serialPort)
{
  commType = COMM_TYPE_SERIAL;
  _serialPort = &serialPort; //Grab which port the user wants us to use

  //New in v2.0: allocate memory for the packetCfg payload here - if required. (The user may have called setPacketCfgPayloadSize already)
  if (packetCfgPayloadSize == 0)
    setPacketCfgPayloadSize(MAX_PAYLOAD_SIZE);

  //New in v2.0: allocate memory for the file buffer - if required. (The user should have called setFileBufferSize already)
  createFileBuffer();

  // Call isConnected up to three times - tests on the NEO-M8U show the CFG RATE poll occasionally being ignored
  boolean connected = isConnected();

  if (!connected)
    connected = isConnected();

  if (!connected)
    connected = isConnected();

  return (connected);
}

// Allow the user to change I2C polling wait (the minimum interval between I2C data requests - to avoid pounding the bus)
// i2cPollingWait defaults to 100ms and is adjusted automatically when setNavigationFrequency()
// or setHNRNavigationRate() are called. But if the user is using callbacks, it might be advantageous
// to be able to set the polling wait manually.
void SFE_UBLOX_GPS::setI2CpollingWait(uint8_t newPollingWait_ms)
{
  i2cPollingWait = newPollingWait_ms;
}

//Sets the global size for I2C transactions
//Most platforms use 32 bytes (the default) but this allows users to increase the transaction
//size if the platform supports it
//Note: If the transaction size is set larger than the platforms buffer size, bad things will happen.
void SFE_UBLOX_GPS::setI2CTransactionSize(uint8_t transactionSize)
{
  i2cTransactionSize = transactionSize;
}
uint8_t SFE_UBLOX_GPS::getI2CTransactionSize(void)
{
  return (i2cTransactionSize);
}

//Returns true if I2C device ack's
boolean SFE_UBLOX_GPS::isConnected(uint16_t maxWait)
{
  if (commType == COMM_TYPE_I2C)
  {
    _i2cPort->beginTransmission((uint8_t)_gpsI2Caddress);
    if (_i2cPort->endTransmission() != 0)
      return false; //Sensor did not ack
  }

  // Query navigation rate to see whether we get a meaningful response
  return (getNavigationFrequencyInternal(maxWait));
}

//Enable or disable the printing of sent/response HEX values.
//Use this in conjunction with 'Transport Logging' from the Universal Reader Assistant to see what they're doing that we're not
void SFE_UBLOX_GPS::enableDebugging(Stream &debugPort, boolean printLimitedDebug)
{
  _debugSerial = &debugPort; //Grab which port the user wants us to use for debugging
  if (printLimitedDebug == false)
  {
    _printDebug = true; //Should we print the commands we send? Good for debugging
  }
  else
  {
    _printLimitedDebug = true; //Should we print limited debug messages? Good for debugging high navigation rates
  }
}
void SFE_UBLOX_GPS::disableDebugging(void)
{
  _printDebug = false; //Turn off extra print statements
  _printLimitedDebug = false;
}

//Safely print messages
void SFE_UBLOX_GPS::debugPrint(char *message)
{
  if (_printDebug == true)
  {
    _debugSerial->print(message);
  }
}
//Safely print messages
void SFE_UBLOX_GPS::debugPrintln(char *message)
{
  if (_printDebug == true)
  {
    _debugSerial->println(message);
  }
}

const char *SFE_UBLOX_GPS::statusString(sfe_ublox_status_e stat)
{
  switch (stat)
  {
  case SFE_UBLOX_STATUS_SUCCESS:
    return "Success";
    break;
  case SFE_UBLOX_STATUS_FAIL:
    return "General Failure";
    break;
  case SFE_UBLOX_STATUS_CRC_FAIL:
    return "CRC Fail";
    break;
  case SFE_UBLOX_STATUS_TIMEOUT:
    return "Timeout";
    break;
  case SFE_UBLOX_STATUS_COMMAND_NACK:
    return "Command not acknowledged (NACK)";
    break;
  case SFE_UBLOX_STATUS_OUT_OF_RANGE:
    return "Out of range";
    break;
  case SFE_UBLOX_STATUS_INVALID_ARG:
    return "Invalid Arg";
    break;
  case SFE_UBLOX_STATUS_INVALID_OPERATION:
    return "Invalid operation";
    break;
  case SFE_UBLOX_STATUS_MEM_ERR:
    return "Memory Error";
    break;
  case SFE_UBLOX_STATUS_HW_ERR:
    return "Hardware Error";
    break;
  case SFE_UBLOX_STATUS_DATA_SENT:
    return "Data Sent";
    break;
  case SFE_UBLOX_STATUS_DATA_RECEIVED:
    return "Data Received";
    break;
  case SFE_UBLOX_STATUS_I2C_COMM_FAILURE:
    return "I2C Comm Failure";
    break;
  case SFE_UBLOX_STATUS_DATA_OVERWRITTEN:
    return "Data Packet Overwritten";
    break;
  default:
    return "Unknown Status";
    break;
  }
  return "None";
}

// Check for the arrival of new I2C/Serial data

//Allow the user to disable the "7F" check (e.g.) when logging RAWX data
void SFE_UBLOX_GPS::disableUBX7Fcheck(boolean disabled)
{
  ubx7FcheckDisabled = disabled;
}

//Called regularly to check for available bytes on the user' specified port
boolean SFE_UBLOX_GPS::checkUblox(uint8_t requestedClass, uint8_t requestedID)
{
  return checkUbloxInternal(&packetCfg, requestedClass, requestedID);
}

//PRIVATE: Called regularly to check for available bytes on the user' specified port
boolean SFE_UBLOX_GPS::checkUbloxInternal(ubxPacket *incomingUBX, uint8_t requestedClass, uint8_t requestedID)
{
  if (commType == COMM_TYPE_I2C)
    return (checkUbloxI2C(incomingUBX, requestedClass, requestedID));
  else if (commType == COMM_TYPE_SERIAL)
    return (checkUbloxSerial(incomingUBX, requestedClass, requestedID));
  return false;
}

//Polls I2C for data, passing any new bytes to process()
//Returns true if new bytes are available
boolean SFE_UBLOX_GPS::checkUbloxI2C(ubxPacket *incomingUBX, uint8_t requestedClass, uint8_t requestedID)
{
  if (millis() - lastCheck >= i2cPollingWait)
  {
    //Get the number of bytes available from the module
    uint16_t bytesAvailable = 0;
    _i2cPort->beginTransmission(_gpsI2Caddress);
    _i2cPort->write(0xFD);                     //0xFD (MSB) and 0xFE (LSB) are the registers that contain number of bytes available
    if (_i2cPort->endTransmission(false) != 0) //Send a restart command. Do not release bus.
      return (false);                          //Sensor did not ACK

    _i2cPort->requestFrom((uint8_t)_gpsI2Caddress, (uint8_t)2);
    if (_i2cPort->available())
    {
      uint8_t msb = _i2cPort->read();
      uint8_t lsb = _i2cPort->read();
      if (lsb == 0xFF)
      {
        //I believe this is a u-blox bug. Device should never present an 0xFF.
        if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
        {
          _debugSerial->println(F("checkUbloxI2C: u-blox bug, length lsb is 0xFF"));
        }
        if (debugPin >= 0)
        {
          digitalWrite((uint8_t)debugPin, LOW);
          delay(10);
          digitalWrite((uint8_t)debugPin, HIGH);
        }
        lastCheck = millis(); //Put off checking to avoid I2C bus traffic
        return (false);
      }
      bytesAvailable = (uint16_t)msb << 8 | lsb;
    }

    if (bytesAvailable == 0)
    {
      if (_printDebug == true)
      {
        _debugSerial->println(F("checkUbloxI2C: OK, zero bytes available"));
      }
      lastCheck = millis(); //Put off checking to avoid I2C bus traffic
      return (false);
    }

    //Check for undocumented bit error. We found this doing logic scans.
    //This error is rare but if we incorrectly interpret the first bit of the two 'data available' bytes as 1
    //then we have far too many bytes to check. May be related to I2C setup time violations: https://github.com/sparkfun/SparkFun_Ublox_Arduino_Library/issues/40
    if (bytesAvailable & ((uint16_t)1 << 15))
    {
      //Clear the MSbit
      bytesAvailable &= ~((uint16_t)1 << 15);

      if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      {
        _debugSerial->print(F("checkUbloxI2C: Bytes available error: "));
        _debugSerial->println(bytesAvailable);
        if (debugPin >= 0)
        {
          digitalWrite((uint8_t)debugPin, LOW);
          delay(10);
          digitalWrite((uint8_t)debugPin, HIGH);
        }
      }
    }

    if (bytesAvailable > 100)
    {
      if (_printDebug == true)
      {
        _debugSerial->print(F("checkUbloxI2C: Large packet of "));
        _debugSerial->print(bytesAvailable);
        _debugSerial->println(F(" bytes received"));
      }
    }
    else
    {
      if (_printDebug == true)
      {
        _debugSerial->print(F("checkUbloxI2C: Reading "));
        _debugSerial->print(bytesAvailable);
        _debugSerial->println(F(" bytes"));
      }
    }

    while (bytesAvailable)
    {
      _i2cPort->beginTransmission(_gpsI2Caddress);
      _i2cPort->write(0xFF);                     //0xFF is the register to read data from
      if (_i2cPort->endTransmission(false) != 0) //Send a restart command. Do not release bus.
        return (false);                          //Sensor did not ACK

      //Limit to 32 bytes or whatever the buffer limit is for given platform
      uint16_t bytesToRead = bytesAvailable;
      if (bytesToRead > i2cTransactionSize)
        bytesToRead = i2cTransactionSize;

    TRY_AGAIN:

      _i2cPort->requestFrom((uint8_t)_gpsI2Caddress, (uint8_t)bytesToRead);
      if (_i2cPort->available())
      {
        for (uint16_t x = 0; x < bytesToRead; x++)
        {
          uint8_t incoming = _i2cPort->read(); //Grab the actual character

          //Check to see if the first read is 0x7F. If it is, the module is not ready
          //to respond. Stop, wait, and try again
          if (x == 0)
          {
            if ((incoming == 0x7F) && (ubx7FcheckDisabled == false))
            {
              if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
              {
                _debugSerial->println(F("checkUbloxU2C: u-blox error, module not ready with data (7F error)"));
              }
              delay(5); //In logic analyzation, the module starting responding after 1.48ms
              if (debugPin >= 0)
              {
                digitalWrite((uint8_t)debugPin, LOW);
                delay(10);
                digitalWrite((uint8_t)debugPin, HIGH);
              }
              goto TRY_AGAIN;
            }
          }

          process(incoming, incomingUBX, requestedClass, requestedID); //Process this valid character
        }
      }
      else
        return (false); //Sensor did not respond

      bytesAvailable -= bytesToRead;
    }
  }

  return (true);

} //end checkUbloxI2C()

//Checks Serial for data, passing any new bytes to process()
boolean SFE_UBLOX_GPS::checkUbloxSerial(ubxPacket *incomingUBX, uint8_t requestedClass, uint8_t requestedID)
{
  while (_serialPort->available())
  {
    process(_serialPort->read(), incomingUBX, requestedClass, requestedID);
  }
  return (true);

} //end checkUbloxSerial()

//PRIVATE: Check if we have storage allocated for an incoming "automatic" message
boolean SFE_UBLOX_GPS::checkAutomatic(uint8_t Class, uint8_t ID)
{
  boolean result = false;
  switch (Class)
  {
    case UBX_CLASS_NAV:
    {
      switch (ID)
      {
        case UBX_NAV_POSECEF:
          if (packetUBXNAVPOSECEF != NULL) result = true;
        break;
        case UBX_NAV_STATUS:
          if (packetUBXNAVSTATUS != NULL) result = true;
        break;
        case UBX_NAV_DOP:
          if (packetUBXNAVDOP != NULL) result = true;
        break;
        case UBX_NAV_ATT:
          if (packetUBXNAVATT != NULL) result = true;
        break;
        case UBX_NAV_PVT:
          if (packetUBXNAVPVT != NULL) result = true;
        break;
        case UBX_NAV_ODO:
          if (packetUBXNAVODO != NULL) result = true;
        break;
        case UBX_NAV_VELECEF:
          if (packetUBXNAVVELECEF != NULL) result = true;
        break;
        case UBX_NAV_VELNED:
          if (packetUBXNAVVELNED != NULL) result = true;
        break;
        case UBX_NAV_HPPOSECEF:
          if (packetUBXNAVHPPOSECEF != NULL) result = true;
        break;
        case UBX_NAV_HPPOSLLH:
          if (packetUBXNAVHPPOSLLH != NULL) result = true;
        break;
        case UBX_NAV_CLOCK:
          if (packetUBXNAVCLOCK != NULL) result = true;
        break;
        case UBX_NAV_SVIN:
          if (packetUBXNAVSVIN != NULL) result = true;
        break;
        case UBX_NAV_RELPOSNED:
          if (packetUBXNAVRELPOSNED != NULL) result = true;
        break;
      }
    }
    break;
    case UBX_CLASS_RXM:
    {
      switch (ID)
      {
        case UBX_RXM_SFRBX:
          if (packetUBXRXMSFRBX != NULL) result = true;
        break;
        case UBX_RXM_RAWX:
          if (packetUBXRXMRAWX != NULL) result = true;
        break;
      }
    }
    break;
    case UBX_CLASS_CFG:
    {
      switch (ID)
      {
        case UBX_CFG_RATE:
          if (packetUBXCFGRATE != NULL) result = true;
        break;
      }
    }
    break;
    case UBX_CLASS_TIM:
    {
      switch (ID)
      {
        case UBX_TIM_TM2:
          if (packetUBXTIMTM2 != NULL) result = true;
        break;
      }
    }
    break;
    case UBX_CLASS_ESF:
    {
      switch (ID)
      {
        case UBX_ESF_ALG:
          if (packetUBXESFALG != NULL) result = true;
        break;
        case UBX_ESF_INS:
          if (packetUBXESFINS != NULL) result = true;
        break;
        case UBX_ESF_MEAS:
          if (packetUBXESFMEAS != NULL) result = true;
        break;
        case UBX_ESF_RAW:
          if (packetUBXESFRAW != NULL) result = true;
        break;
        case UBX_ESF_STATUS:
          if (packetUBXESFSTATUS != NULL) result = true;
        break;
      }
    }
    break;
    case UBX_CLASS_HNR:
    {
      switch (ID)
      {
        case UBX_HNR_PVT:
          if (packetUBXHNRPVT != NULL) result = true;
        break;
        case UBX_HNR_ATT:
          if (packetUBXHNRATT != NULL) result = true;
        break;
        case UBX_HNR_INS:
          if (packetUBXHNRINS != NULL) result = true;
        break;
      }
    }
    break;
  }
  return (result);
}

//PRIVATE: Calculate how much RAM is needed to store the payload for a given automatic message
uint16_t SFE_UBLOX_GPS::getMaxPayloadSize(uint8_t Class, uint8_t ID)
{
  uint16_t maxSize = 0;
  switch (Class)
  {
    case UBX_CLASS_NAV:
    {
      switch (ID)
      {
        case UBX_NAV_POSECEF:
          maxSize = UBX_NAV_POSECEF_LEN;
        break;
        case UBX_NAV_STATUS:
          maxSize = UBX_NAV_STATUS_LEN;
        break;
        case UBX_NAV_DOP:
          maxSize = UBX_NAV_DOP_LEN;
        break;
        case UBX_NAV_ATT:
          maxSize = UBX_NAV_ATT_LEN;
        break;
        case UBX_NAV_PVT:
          maxSize = UBX_NAV_PVT_LEN;
        break;
        case UBX_NAV_ODO:
          maxSize = UBX_NAV_ODO_LEN;
        break;
        case UBX_NAV_VELECEF:
          maxSize = UBX_NAV_VELECEF_LEN;
        break;
        case UBX_NAV_VELNED:
          maxSize = UBX_NAV_VELNED_LEN;
        break;
        case UBX_NAV_HPPOSECEF:
          maxSize = UBX_NAV_HPPOSECEF_LEN;
        break;
        case UBX_NAV_HPPOSLLH:
          maxSize = UBX_NAV_HPPOSLLH_LEN;
        break;
        case UBX_NAV_CLOCK:
          maxSize = UBX_NAV_CLOCK_LEN;
        break;
        case UBX_NAV_SVIN:
          maxSize = UBX_NAV_SVIN_LEN;
        break;
        case UBX_NAV_RELPOSNED:
          maxSize = UBX_NAV_RELPOSNED_LEN_F9;
        break;
      }
    }
    break;
    case UBX_CLASS_RXM:
    {
      switch (ID)
      {
        case UBX_RXM_SFRBX:
          maxSize = UBX_RXM_SFRBX_MAX_LEN;
        break;
        case UBX_RXM_RAWX:
          maxSize = UBX_RXM_RAWX_MAX_LEN;
        break;
      }
    }
    break;
    case UBX_CLASS_CFG:
    {
      switch (ID)
      {
        case UBX_CFG_RATE:
          maxSize = UBX_CFG_RATE_LEN;
        break;
      }
    }
    break;
    case UBX_CLASS_TIM:
    {
      switch (ID)
      {
        case UBX_TIM_TM2:
          maxSize = UBX_TIM_TM2_LEN;
        break;
      }
    }
    break;
    case UBX_CLASS_ESF:
    {
      switch (ID)
      {
        case UBX_ESF_ALG:
          maxSize = UBX_ESF_ALG_LEN;
        break;
        case UBX_ESF_INS:
          maxSize = UBX_ESF_INS_LEN;
        break;
        case UBX_ESF_MEAS:
          maxSize = UBX_ESF_MEAS_MAX_LEN;
        break;
        case UBX_ESF_RAW:
          maxSize = UBX_ESF_RAW_MAX_LEN;
        break;
        case UBX_ESF_STATUS:
          maxSize = UBX_ESF_STATUS_MAX_LEN;
        break;
      }
    }
    break;
    case UBX_CLASS_HNR:
    {
      switch (ID)
      {
        case UBX_HNR_PVT:
          maxSize = UBX_HNR_PVT_LEN;
        break;
        case UBX_HNR_ATT:
          maxSize = UBX_HNR_ATT_LEN;
        break;
        case UBX_HNR_INS:
          maxSize = UBX_HNR_INS_LEN;
        break;
      }
    }
    break;
  }
  return (maxSize);
}

//Processes NMEA and UBX binary sentences one byte at a time
//Take a given byte and file it into the proper array
void SFE_UBLOX_GPS::process(uint8_t incoming, ubxPacket *incomingUBX, uint8_t requestedClass, uint8_t requestedID)
{
  if ((currentSentence == NONE) || (currentSentence == NMEA))
  {
    if (incoming == 0xB5) //UBX binary frames start with 0xB5, aka μ
    {
      //This is the start of a binary sentence. Reset flags.
      //We still don't know the response class
      ubxFrameCounter = 0;
      currentSentence = UBX;
      //Reset the packetBuf.counter even though we will need to reset it again when ubxFrameCounter == 2
      packetBuf.counter = 0;
      ignoreThisPayload = false; //We should not ignore this payload - yet
      //Store data in packetBuf until we know if we have a requested class and ID match
      activePacketBuffer = SFE_UBLOX_PACKET_PACKETBUF;
    }
    else if (incoming == '$')
    {
      currentSentence = NMEA;
    }
    else if (incoming == 0xD3) //RTCM frames start with 0xD3
    {
      rtcmFrameCounter = 0;
      currentSentence = RTCM;
    }
    else
    {
      //This character is unknown or we missed the previous start of a sentence
    }
  }

  //Depending on the sentence, pass the character to the individual processor
  if (currentSentence == UBX)
  {
    //Decide what type of response this is
    if ((ubxFrameCounter == 0) && (incoming != 0xB5))      //ISO 'μ'
      currentSentence = NONE;                              //Something went wrong. Reset.
    else if ((ubxFrameCounter == 1) && (incoming != 0x62)) //ASCII 'b'
      currentSentence = NONE;                              //Something went wrong. Reset.
    // Note to future self:
    // There may be some duplication / redundancy in the next few lines as processUBX will also
    // load information into packetBuf, but we'll do it here too for clarity
    else if (ubxFrameCounter == 2) //Class
    {
      // Record the class in packetBuf until we know what to do with it
      packetBuf.cls = incoming; // (Duplication)
      rollingChecksumA = 0;     //Reset our rolling checksums here (not when we receive the 0xB5)
      rollingChecksumB = 0;
      packetBuf.counter = 0;                                   //Reset the packetBuf.counter (again)
      packetBuf.valid = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED; // Reset the packet validity (redundant?)
      packetBuf.startingSpot = incomingUBX->startingSpot;      //Copy the startingSpot
    }
    else if (ubxFrameCounter == 3) //ID
    {
      // Record the ID in packetBuf until we know what to do with it
      packetBuf.id = incoming; // (Duplication)
      //We can now identify the type of response
      //If the packet we are receiving is not an ACK then check for a class and ID match
      if (packetBuf.cls != UBX_CLASS_ACK)
      {
        //This is not an ACK so check for a class and ID match
        if ((packetBuf.cls == requestedClass) && (packetBuf.id == requestedID))
        {
          //This is not an ACK and we have a class and ID match
          //So start diverting data into incomingUBX (usually packetCfg)
          activePacketBuffer = SFE_UBLOX_PACKET_PACKETCFG;
          incomingUBX->cls = packetBuf.cls; //Copy the class and ID into incomingUBX (usually packetCfg)
          incomingUBX->id = packetBuf.id;
          incomingUBX->counter = packetBuf.counter; //Copy over the .counter too
        }
        //This is not an ACK and we do not have a complete class and ID match
        //So let's check if this is an "automatic" message which has its own storage defined
        else if (checkAutomatic(packetBuf.cls, packetBuf.id))
        {
          //This is not the message we were expecting but it has its own storage and so we should process it anyway.
          //We'll try to use packetAuto to buffer the message (so it can't overwrite anything in packetCfg).
          //We need to allocate memory for the packetAuto payload (payloadAuto) - and delete it once
          //reception is complete.
          uint16_t maxPayload = getMaxPayloadSize(packetBuf.cls, packetBuf.id); // Calculate how much RAM we need
          if (maxPayload == 0)
          {
            if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
            {
              _debugSerial->print(F("process: getMaxPayloadSize returned ZERO!! Class: 0x"));
              _debugSerial->print(packetBuf.cls);
              _debugSerial->print(F(" ID: 0x"));
              _debugSerial->println(packetBuf.id);
            }
          }
          if (payloadAuto != NULL) // Check if memory is already allocated - this should be impossible!
          {
            if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
            {
              _debugSerial->println(F("process: memory is already allocated for payloadAuto! Deleting..."));
            }
            delete[] payloadAuto;
            payloadAuto = NULL; // Redundant?
            packetAuto.payload = payloadAuto;
          }
          payloadAuto = new uint8_t[maxPayload]; // Allocate RAM for payloadAuto
          packetAuto.payload = payloadAuto;
          if (payloadAuto == NULL) // Check if the alloc failed
          {
            if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
            {
              _debugSerial->print(F("process: memory allocation failed for \"automatic\" message: Class: 0x"));
              _debugSerial->print(packetBuf.cls, HEX);
              _debugSerial->print(F(" ID: 0x"));
              _debugSerial->println(packetBuf.id, HEX);
              _debugSerial->println(F("process: \"automatic\" message could overwrite data"));
            }
            // The RAM allocation failed so fall back to using incomingUBX (usually packetCfg) even though we risk overwriting data
            activePacketBuffer = SFE_UBLOX_PACKET_PACKETCFG;
            incomingUBX->cls = packetBuf.cls; //Copy the class and ID into incomingUBX (usually packetCfg)
            incomingUBX->id = packetBuf.id;
            incomingUBX->counter = packetBuf.counter; //Copy over the .counter too
          }
          else
          {
            //The RAM allocation was successful so we start diverting data into packetAuto and process it
            activePacketBuffer = SFE_UBLOX_PACKET_PACKETAUTO;
            packetAuto.cls = packetBuf.cls; //Copy the class and ID into packetAuto
            packetAuto.id = packetBuf.id;
            packetAuto.counter = packetBuf.counter; //Copy over the .counter too
            if (_printDebug == true)
            {
              _debugSerial->print(F("process: incoming \"automatic\" message: Class: 0x"));
              _debugSerial->print(packetBuf.cls, HEX);
              _debugSerial->print(F(" ID: 0x"));
              _debugSerial->println(packetBuf.id, HEX);
            }
          }
        }
        else
        {
          //This is not an ACK and we do not have a class and ID match
          //so we should keep diverting data into packetBuf and ignore the payload
          ignoreThisPayload = true;
        }
      }
      else
      {
        // This is an ACK so it is to early to do anything with it
        // We need to wait until we have received the length and data bytes
        // So we should keep diverting data into packetBuf
      }
    }
    else if (ubxFrameCounter == 4) //Length LSB
    {
      //We should save the length in packetBuf even if activePacketBuffer == SFE_UBLOX_PACKET_PACKETCFG
      packetBuf.len = incoming; // (Duplication)
    }
    else if (ubxFrameCounter == 5) //Length MSB
    {
      //We should save the length in packetBuf even if activePacketBuffer == SFE_UBLOX_PACKET_PACKETCFG
      packetBuf.len |= incoming << 8; // (Duplication)
    }
    else if (ubxFrameCounter == 6) //This should be the first byte of the payload unless .len is zero
    {
      if (packetBuf.len == 0) // Check if length is zero (hopefully this is impossible!)
      {
        if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
        {
          _debugSerial->print(F("process: ZERO LENGTH packet received: Class: 0x"));
          _debugSerial->print(packetBuf.cls, HEX);
          _debugSerial->print(F(" ID: 0x"));
          _debugSerial->println(packetBuf.id, HEX);
        }
        //If length is zero (!) this will be the first byte of the checksum so record it
        packetBuf.checksumA = incoming;
      }
      else
      {
        //The length is not zero so record this byte in the payload
        packetBuf.payload[0] = incoming;
      }
    }
    else if (ubxFrameCounter == 7) //This should be the second byte of the payload unless .len is zero or one
    {
      if (packetBuf.len == 0) // Check if length is zero (hopefully this is impossible!)
      {
        //If length is zero (!) this will be the second byte of the checksum so record it
        packetBuf.checksumB = incoming;
      }
      else if (packetBuf.len == 1) // Check if length is one
      {
        //The length is one so this is the first byte of the checksum
        packetBuf.checksumA = incoming;
      }
      else // Length is >= 2 so this must be a payload byte
      {
        packetBuf.payload[1] = incoming;
      }
      // Now that we have received two payload bytes, we can check for a matching ACK/NACK
      if ((activePacketBuffer == SFE_UBLOX_PACKET_PACKETBUF) // If we are not already processing a data packet
          && (packetBuf.cls == UBX_CLASS_ACK)                // and if this is an ACK/NACK
          && (packetBuf.payload[0] == requestedClass)        // and if the class matches
          && (packetBuf.payload[1] == requestedID))          // and if the ID matches
      {
        if (packetBuf.len == 2) // Check if .len is 2
        {
          // Then this is a matching ACK so copy it into packetAck
          activePacketBuffer = SFE_UBLOX_PACKET_PACKETACK;
          packetAck.cls = packetBuf.cls;
          packetAck.id = packetBuf.id;
          packetAck.len = packetBuf.len;
          packetAck.counter = packetBuf.counter;
          packetAck.payload[0] = packetBuf.payload[0];
          packetAck.payload[1] = packetBuf.payload[1];
        }
        else // Length is not 2 (hopefully this is impossible!)
        {
          if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
          {
            _debugSerial->print(F("process: ACK received with .len != 2: Class: 0x"));
            _debugSerial->print(packetBuf.payload[0], HEX);
            _debugSerial->print(F(" ID: 0x"));
            _debugSerial->print(packetBuf.payload[1], HEX);
            _debugSerial->print(F(" len: "));
            _debugSerial->println(packetBuf.len);
          }
        }
      }
    }

    //Divert incoming into the correct buffer
    if (activePacketBuffer == SFE_UBLOX_PACKET_PACKETACK)
      processUBX(incoming, &packetAck, requestedClass, requestedID);
    else if (activePacketBuffer == SFE_UBLOX_PACKET_PACKETCFG)
      processUBX(incoming, incomingUBX, requestedClass, requestedID);
    else if (activePacketBuffer == SFE_UBLOX_PACKET_PACKETBUF)
      processUBX(incoming, &packetBuf, requestedClass, requestedID);
    else // if (activePacketBuffer == SFE_UBLOX_PACKET_PACKETAUTO)
      processUBX(incoming, &packetAuto, requestedClass, requestedID);

    //Finally, increment the frame counter
    ubxFrameCounter++;
  }
  else if (currentSentence == NMEA)
  {
    processNMEA(incoming); //Process each NMEA character
  }
  else if (currentSentence == RTCM)
  {
    processRTCMframe(incoming); //Deal with RTCM bytes
  }
}

//This is the default or generic NMEA processor. We're only going to pipe the data to serial port so we can see it.
//User could overwrite this function to pipe characters to nmea.process(c) of tinyGPS or MicroNMEA
//Or user could pipe each character to a buffer, radio, etc.
void SFE_UBLOX_GPS::processNMEA(char incoming)
{
  //If user has assigned an output port then pipe the characters there
  if (_nmeaOutputPort != NULL)
    _nmeaOutputPort->write(incoming); //Echo this byte to the serial port
}

//We need to be able to identify an RTCM packet and then the length
//so that we know when the RTCM message is completely received and we then start
//listening for other sentences (like NMEA or UBX)
//RTCM packet structure is very odd. I never found RTCM STANDARD 10403.2 but
//http://d1.amobbs.com/bbs_upload782111/files_39/ourdev_635123CK0HJT.pdf is good
//https://dspace.cvut.cz/bitstream/handle/10467/65205/F3-BP-2016-Shkalikava-Anastasiya-Prenos%20polohove%20informace%20prostrednictvim%20datove%20site.pdf?sequence=-1
//Lead me to: https://forum.u-blox.com/index.php/4348/how-to-read-rtcm-messages-from-neo-m8p
//RTCM 3.2 bytes look like this:
//Byte 0: Always 0xD3
//Byte 1: 6-bits of zero
//Byte 2: 10-bits of length of this packet including the first two-ish header bytes, + 6.
//byte 3 + 4 bits: Msg type 12 bits
//Example: D3 00 7C 43 F0 ... / 0x7C = 124+6 = 130 bytes in this packet, 0x43F = Msg type 1087
void SFE_UBLOX_GPS::processRTCMframe(uint8_t incoming)
{
  if (rtcmFrameCounter == 1)
  {
    rtcmLen = (incoming & 0x03) << 8; //Get the last two bits of this byte. Bits 8&9 of 10-bit length
  }
  else if (rtcmFrameCounter == 2)
  {
    rtcmLen |= incoming; //Bits 0-7 of packet length
    rtcmLen += 6;        //There are 6 additional bytes of what we presume is header, msgType, CRC, and stuff
  }
  /*else if (rtcmFrameCounter == 3)
  {
    rtcmMsgType = incoming << 4; //Message Type, MS 4 bits
  }
  else if (rtcmFrameCounter == 4)
  {
    rtcmMsgType |= (incoming >> 4); //Message Type, bits 0-7
  }*/

  rtcmFrameCounter++;

  processRTCM(incoming); //Here is where we expose this byte to the user

  if (rtcmFrameCounter == rtcmLen)
  {
    //We're done!
    currentSentence = NONE; //Reset and start looking for next sentence type
  }
}

//This function is called for each byte of an RTCM frame
//Ths user can overwrite this function and process the RTCM frame as they please
//Bytes can be piped to Serial or other interface. The consumer could be a radio or the internet (Ntrip broadcaster)
void SFE_UBLOX_GPS::processRTCM(uint8_t incoming)
{
  //Radio.sendReliable((String)incoming); //An example of passing this byte to a radio

  //_debugSerial->write(incoming); //An example of passing this byte out the serial port

  //Debug printing
  //  _debugSerial->print(F(" "));
  //  if(incoming < 0x10) _debugSerial->print(F("0"));
  //  if(incoming < 0x10) _debugSerial->print(F("0"));
  //  _debugSerial->print(incoming, HEX);
  //  if(rtcmFrameCounter % 16 == 0) _debugSerial->println();
}

//Given a character, file it away into the uxb packet structure
//Set valid to VALID or NOT_VALID once sentence is completely received and passes or fails CRC
//The payload portion of the packet can be 100s of bytes but the max array size is packetCfgPayloadSize bytes.
//startingSpot can be set so we only record a subset of bytes within a larger packet.
void SFE_UBLOX_GPS::processUBX(uint8_t incoming, ubxPacket *incomingUBX, uint8_t requestedClass, uint8_t requestedID)
{
    //If incomingUBX is a user-defined custom packet, then the payload size could be different to packetCfgPayloadSize.
    //TO DO: update this to prevent an overrun when receiving an automatic message
    //       and the incomingUBX payload size is smaller than packetCfgPayloadSize.
  size_t maximum_payload_size;
  if (activePacketBuffer == SFE_UBLOX_PACKET_PACKETCFG)
    maximum_payload_size = packetCfgPayloadSize;
  else if (activePacketBuffer == SFE_UBLOX_PACKET_PACKETAUTO)
  {
    // Calculate maximum payload size once Class and ID have been received
    // (This check is probably redundant as activePacketBuffer can only be SFE_UBLOX_PACKET_PACKETAUTO
    //  when ubxFrameCounter >= 3)
    //if (incomingUBX->counter >= 2)
    //{
      maximum_payload_size = getMaxPayloadSize(incomingUBX->cls, incomingUBX->id);
      if (maximum_payload_size == 0)
      {
        if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
        {
          _debugSerial->print(F("processUBX: getMaxPayloadSize returned ZERO!! Class: 0x"));
          _debugSerial->print(incomingUBX->cls);
          _debugSerial->print(F(" ID: 0x"));
          _debugSerial->println(incomingUBX->id);
        }
      }
    //}
    //else
    //  maximum_payload_size = 2;
  }
  else
    maximum_payload_size = 2;

  bool overrun = false;

  //Add all incoming bytes to the rolling checksum
  //Stop at len+4 as this is the checksum bytes to that should not be added to the rolling checksum
  if (incomingUBX->counter < incomingUBX->len + 4)
    addToChecksum(incoming);

  if (incomingUBX->counter == 0)
  {
    incomingUBX->cls = incoming;
  }
  else if (incomingUBX->counter == 1)
  {
    incomingUBX->id = incoming;
  }
  else if (incomingUBX->counter == 2) //Len LSB
  {
    incomingUBX->len = incoming;
  }
  else if (incomingUBX->counter == 3) //Len MSB
  {
    incomingUBX->len |= incoming << 8;
  }
  else if (incomingUBX->counter == incomingUBX->len + 4) //ChecksumA
  {
    incomingUBX->checksumA = incoming;
  }
  else if (incomingUBX->counter == incomingUBX->len + 5) //ChecksumB
  {
    incomingUBX->checksumB = incoming;

    currentSentence = NONE; //We're done! Reset the sentence to being looking for a new start char

    //Validate this sentence
    if ((incomingUBX->checksumA == rollingChecksumA) && (incomingUBX->checksumB == rollingChecksumB))
    {
      incomingUBX->valid = SFE_UBLOX_PACKET_VALIDITY_VALID; // Flag the packet as valid

      // Let's check if the class and ID match the requestedClass and requestedID
      // Remember - this could be a data packet or an ACK packet
      if ((incomingUBX->cls == requestedClass) && (incomingUBX->id == requestedID))
      {
        incomingUBX->classAndIDmatch = SFE_UBLOX_PACKET_VALIDITY_VALID; // If we have a match, set the classAndIDmatch flag to valid
      }

      // If this is an ACK then let's check if the class and ID match the requestedClass and requestedID
      else if ((incomingUBX->cls == UBX_CLASS_ACK) && (incomingUBX->id == UBX_ACK_ACK) && (incomingUBX->payload[0] == requestedClass) && (incomingUBX->payload[1] == requestedID))
      {
        incomingUBX->classAndIDmatch = SFE_UBLOX_PACKET_VALIDITY_VALID; // If we have a match, set the classAndIDmatch flag to valid
      }

      // If this is a NACK then let's check if the class and ID match the requestedClass and requestedID
      else if ((incomingUBX->cls == UBX_CLASS_ACK) && (incomingUBX->id == UBX_ACK_NACK) && (incomingUBX->payload[0] == requestedClass) && (incomingUBX->payload[1] == requestedID))
      {
        incomingUBX->classAndIDmatch = SFE_UBLOX_PACKET_NOTACKNOWLEDGED; // If we have a match, set the classAndIDmatch flag to NOTACKNOWLEDGED
        if (_printDebug == true)
        {
          _debugSerial->print(F("processUBX: NACK received: Requested Class: 0x"));
          _debugSerial->print(incomingUBX->payload[0], HEX);
          _debugSerial->print(F(" Requested ID: 0x"));
          _debugSerial->println(incomingUBX->payload[1], HEX);
        }
      }

      //This is not an ACK and we do not have a complete class and ID match
      //So let's check for an "automatic" message arriving
      else if (checkAutomatic(incomingUBX->cls, incomingUBX->id))
      {
        // This isn't the message we are looking for...
        // Let's say so and leave incomingUBX->classAndIDmatch _unchanged_
        if (_printDebug == true)
        {
          _debugSerial->print(F("processUBX: incoming \"automatic\" message: Class: 0x"));
          _debugSerial->print(incomingUBX->cls, HEX);
          _debugSerial->print(F(" ID: 0x"));
          _debugSerial->println(incomingUBX->id, HEX);
        }
      }

      if (_printDebug == true)
      {
        _debugSerial->print(F("Incoming: Size: "));
        _debugSerial->print(incomingUBX->len);
        _debugSerial->print(F(" Received: "));
        printPacket(incomingUBX);

        if (incomingUBX->valid == SFE_UBLOX_PACKET_VALIDITY_VALID)
        {
          _debugSerial->println(F("packetCfg now valid"));
        }
        if (packetAck.valid == SFE_UBLOX_PACKET_VALIDITY_VALID)
        {
          _debugSerial->println(F("packetAck now valid"));
        }
        if (incomingUBX->classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_VALID)
        {
          _debugSerial->println(F("packetCfg classAndIDmatch"));
        }
        if (packetAck.classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_VALID)
        {
          _debugSerial->println(F("packetAck classAndIDmatch"));
        }
      }

      //We've got a valid packet, now do something with it but only if ignoreThisPayload is false
      if (ignoreThisPayload == false)
      {
        processUBXpacket(incomingUBX);
      }
    }
    else // Checksum failure
    {
      incomingUBX->valid = SFE_UBLOX_PACKET_VALIDITY_NOT_VALID;

      // Let's check if the class and ID match the requestedClass and requestedID.
      // This is potentially risky as we are saying that we saw the requested Class and ID
      // but that the packet checksum failed. Potentially it could be the class or ID bytes
      // that caused the checksum error!
      if ((incomingUBX->cls == requestedClass) && (incomingUBX->id == requestedID))
      {
        incomingUBX->classAndIDmatch = SFE_UBLOX_PACKET_VALIDITY_NOT_VALID; // If we have a match, set the classAndIDmatch flag to not valid
      }
      // If this is an ACK then let's check if the class and ID match the requestedClass and requestedID
      else if ((incomingUBX->cls == UBX_CLASS_ACK) && (incomingUBX->payload[0] == requestedClass) && (incomingUBX->payload[1] == requestedID))
      {
        incomingUBX->classAndIDmatch = SFE_UBLOX_PACKET_VALIDITY_NOT_VALID; // If we have a match, set the classAndIDmatch flag to not valid
      }

      if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      {
        //Drive an external pin to allow for easier logic analyzation
        if (debugPin >= 0)
        {
          digitalWrite((uint8_t)debugPin, LOW);
          delay(10);
          digitalWrite((uint8_t)debugPin, HIGH);
        }

        _debugSerial->print(F("Checksum failed:"));
        _debugSerial->print(F(" checksumA: "));
        _debugSerial->print(incomingUBX->checksumA);
        _debugSerial->print(F(" checksumB: "));
        _debugSerial->print(incomingUBX->checksumB);

        _debugSerial->print(F(" rollingChecksumA: "));
        _debugSerial->print(rollingChecksumA);
        _debugSerial->print(F(" rollingChecksumB: "));
        _debugSerial->print(rollingChecksumB);
        _debugSerial->println();
      }
    }

    // Now that the packet is complete and has been processed, we need to delete the memory
    // allocated for packetAuto
    if (activePacketBuffer == SFE_UBLOX_PACKET_PACKETAUTO)
    {
      delete[] payloadAuto;
      payloadAuto = NULL; // Redundant?
      packetAuto.payload = payloadAuto;
    }
  }
  else //Load this byte into the payload array
  {
    //If an automatic packet comes in asynchronously, we need to fudge the startingSpot
    uint16_t startingSpot = incomingUBX->startingSpot;
    if (checkAutomatic(incomingUBX->cls, incomingUBX->id))
      startingSpot = 0;
    // Check if this is payload data which should be ignored
    if (ignoreThisPayload == false)
    {
      //Begin recording if counter goes past startingSpot
      if ((incomingUBX->counter - 4) >= startingSpot)
      {
        //Check to see if we have room for this byte
        if (((incomingUBX->counter - 4) - startingSpot) < maximum_payload_size) //If counter = 208, starting spot = 200, we're good to record.
        {
          incomingUBX->payload[(incomingUBX->counter - 4) - startingSpot] = incoming; //Store this byte into payload array
        }
        else
        {
          overrun = true;
        }
      }
    }
  }

  // incomingUBX->counter should never reach maximum_payload_size + class + id + len[2] + checksum[2]
  if (overrun || ((incomingUBX->counter == maximum_payload_size + 6) && (ignoreThisPayload == false)))
  {
    //Something has gone very wrong
    currentSentence = NONE; //Reset the sentence to being looking for a new start char
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
    {
      if (overrun)
        _debugSerial->print(F("processUBX: buffer overrun detected!"));
      else
        _debugSerial->print(F("processUBX: counter hit maximum_payload_size + 6!"));
      _debugSerial->print(F(" activePacketBuffer: "));
      _debugSerial->print(activePacketBuffer);
      _debugSerial->print(F(" maximum_payload_size: "));
      _debugSerial->println(maximum_payload_size);
    }
  }

  //Increment the counter
  incomingUBX->counter++;
}

//Once a packet has been received and validated, identify this packet's class/id and update internal flags
void SFE_UBLOX_GPS::processUBXpacket(ubxPacket *msg)
{
  switch (msg->cls)
  {
  case UBX_CLASS_NAV:
    if (msg->id == UBX_NAV_POSECEF && msg->len == UBX_NAV_POSECEF_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXNAVPOSECEF != NULL)
      {
        packetUBXNAVPOSECEF->data.iTOW = extractLong(msg, 0);
        packetUBXNAVPOSECEF->data.ecefX = extractSignedLong(msg, 4);
        packetUBXNAVPOSECEF->data.ecefY = extractSignedLong(msg, 8);
        packetUBXNAVPOSECEF->data.ecefZ = extractSignedLong(msg, 12);
        packetUBXNAVPOSECEF->data.pAcc = extractLong(msg, 16);

        //Mark all datums as fresh (not read before)
        packetUBXNAVPOSECEF->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXNAVPOSECEFcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXNAVPOSECEF->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXNAVPOSECEFcopy->iTOW, &packetUBXNAVPOSECEF->data.iTOW, sizeof(UBX_NAV_POSECEF_data_t));
          packetUBXNAVPOSECEF->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXNAVPOSECEF->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_NAV_STATUS && msg->len == UBX_NAV_STATUS_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXNAVSTATUS != NULL)
      {
        packetUBXNAVSTATUS->data.iTOW = extractLong(msg, 0);
        packetUBXNAVSTATUS->data.gpsFix = extractByte(msg, 4);
        packetUBXNAVSTATUS->data.flags.all = extractByte(msg, 5);
        packetUBXNAVSTATUS->data.fixStat.all = extractByte(msg, 6);
        packetUBXNAVSTATUS->data.flags2.all = extractByte(msg, 7);
        packetUBXNAVSTATUS->data.ttff = extractLong(msg, 8);
        packetUBXNAVSTATUS->data.msss = extractLong(msg, 12);

        //Mark all datums as fresh (not read before)
        packetUBXNAVSTATUS->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXNAVSTATUScopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXNAVSTATUS->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXNAVSTATUScopy->iTOW, &packetUBXNAVSTATUS->data.iTOW, sizeof(UBX_NAV_STATUS_data_t));
          packetUBXNAVSTATUS->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXNAVSTATUS->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_NAV_DOP && msg->len == UBX_NAV_DOP_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXNAVDOP != NULL)
      {
        packetUBXNAVDOP->data.iTOW = extractLong(msg, 0);
        packetUBXNAVDOP->data.gDOP = extractInt(msg, 4);
        packetUBXNAVDOP->data.pDOP = extractInt(msg, 6);
        packetUBXNAVDOP->data.tDOP = extractInt(msg, 8);
        packetUBXNAVDOP->data.vDOP = extractInt(msg, 10);
        packetUBXNAVDOP->data.hDOP = extractInt(msg, 12);
        packetUBXNAVDOP->data.nDOP = extractInt(msg, 14);
        packetUBXNAVDOP->data.eDOP = extractInt(msg, 16);

        //Mark all datums as fresh (not read before)
        packetUBXNAVDOP->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXNAVDOPcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXNAVDOP->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXNAVDOPcopy->iTOW, &packetUBXNAVDOP->data.iTOW, sizeof(UBX_NAV_DOP_data_t));
          packetUBXNAVDOP->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXNAVDOP->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_NAV_ATT && msg->len == UBX_NAV_ATT_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXNAVATT != NULL)
      {
        packetUBXNAVATT->data.iTOW = extractLong(msg, 0);
        packetUBXNAVATT->data.version = extractByte(msg, 4);
        packetUBXNAVATT->data.roll = extractSignedLong(msg, 8);
        packetUBXNAVATT->data.pitch = extractSignedLong(msg, 12);
        packetUBXNAVATT->data.heading = extractSignedLong(msg, 16);
        packetUBXNAVATT->data.accRoll = extractLong(msg, 20);
        packetUBXNAVATT->data.accPitch = extractLong(msg, 24);
        packetUBXNAVATT->data.accHeading = extractLong(msg, 28);

        //Mark all datums as fresh (not read before)
        packetUBXNAVATT->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXNAVATTcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXNAVATT->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXNAVATTcopy->iTOW, &packetUBXNAVATT->data.iTOW, sizeof(UBX_NAV_ATT_data_t));
          packetUBXNAVATT->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXNAVATT->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_NAV_PVT && msg->len == UBX_NAV_PVT_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXNAVPVT != NULL)
      {
        packetUBXNAVPVT->data.iTOW = extractLong(msg, 0);
        packetUBXNAVPVT->data.year = extractInt(msg, 4);
        packetUBXNAVPVT->data.month = extractByte(msg, 6);
        packetUBXNAVPVT->data.day = extractByte(msg, 7);
        packetUBXNAVPVT->data.hour = extractByte(msg, 8);
        packetUBXNAVPVT->data.min = extractByte(msg, 9);
        packetUBXNAVPVT->data.sec = extractByte(msg, 10);
        packetUBXNAVPVT->data.valid.all = extractByte(msg, 11);
        packetUBXNAVPVT->data.tAcc = extractLong(msg, 12);
        packetUBXNAVPVT->data.nano = extractSignedLong(msg, 16); //Includes milliseconds
        packetUBXNAVPVT->data.fixType = extractByte(msg, 20);
        packetUBXNAVPVT->data.flags.all = extractByte(msg, 21);
        packetUBXNAVPVT->data.flags2.all = extractByte(msg, 22);
        packetUBXNAVPVT->data.numSV = extractByte(msg, 23);
        packetUBXNAVPVT->data.lon = extractSignedLong(msg, 24);
        packetUBXNAVPVT->data.lat = extractSignedLong(msg, 28);
        packetUBXNAVPVT->data.height = extractSignedLong(msg, 32);
        packetUBXNAVPVT->data.hMSL = extractSignedLong(msg, 36);
        packetUBXNAVPVT->data.hAcc = extractLong(msg, 40);
        packetUBXNAVPVT->data.vAcc = extractLong(msg, 44);
        packetUBXNAVPVT->data.velN = extractSignedLong(msg, 48);
        packetUBXNAVPVT->data.velE = extractSignedLong(msg, 52);
        packetUBXNAVPVT->data.velD = extractSignedLong(msg, 56);
        packetUBXNAVPVT->data.gSpeed = extractSignedLong(msg, 60);
        packetUBXNAVPVT->data.headMot = extractSignedLong(msg, 64);
        packetUBXNAVPVT->data.sAcc = extractLong(msg, 68);
        packetUBXNAVPVT->data.headAcc = extractLong(msg, 72);
        packetUBXNAVPVT->data.pDOP = extractInt(msg, 76);
        packetUBXNAVPVT->data.flags3.all = extractByte(msg, 78);
        packetUBXNAVPVT->data.headVeh = extractSignedLong(msg, 84);
        packetUBXNAVPVT->data.magDec = extractSignedInt(msg, 88);
        packetUBXNAVPVT->data.magAcc = extractInt(msg, 90);

        //Mark all datums as fresh (not read before)
        packetUBXNAVPVT->moduleQueried.moduleQueried1.all = 0xFFFFFFFF;
        packetUBXNAVPVT->moduleQueried.moduleQueried2.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXNAVPVTcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXNAVPVT->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXNAVPVTcopy->iTOW, &packetUBXNAVPVT->data.iTOW, sizeof(UBX_NAV_PVT_data_t));
          packetUBXNAVPVT->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXNAVPVT->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_NAV_ODO && msg->len == UBX_NAV_ODO_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXNAVODO != NULL)
      {
        packetUBXNAVODO->data.version = extractByte(msg, 0);
        packetUBXNAVODO->data.iTOW = extractLong(msg, 4);
        packetUBXNAVODO->data.distance = extractLong(msg, 8);
        packetUBXNAVODO->data.totalDistance = extractLong(msg, 12);
        packetUBXNAVODO->data.distanceStd = extractLong(msg, 16);

        //Mark all datums as fresh (not read before)
        packetUBXNAVODO->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXNAVODOcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXNAVODO->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXNAVODOcopy->version, &packetUBXNAVODO->data.version, sizeof(UBX_NAV_ODO_data_t));
          packetUBXNAVODO->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXNAVODO->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_NAV_VELECEF && msg->len == UBX_NAV_VELECEF_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXNAVVELECEF != NULL)
      {
        packetUBXNAVVELECEF->data.iTOW = extractLong(msg, 0);
        packetUBXNAVVELECEF->data.ecefVX = extractSignedLong(msg, 4);
        packetUBXNAVVELECEF->data.ecefVY = extractSignedLong(msg, 8);
        packetUBXNAVVELECEF->data.ecefVZ = extractSignedLong(msg, 12);
        packetUBXNAVVELECEF->data.sAcc = extractLong(msg, 16);

        //Mark all datums as fresh (not read before)
        packetUBXNAVVELECEF->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXNAVVELECEFcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXNAVVELECEF->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXNAVVELECEFcopy->iTOW, &packetUBXNAVVELECEF->data.iTOW, sizeof(UBX_NAV_VELECEF_data_t));
          packetUBXNAVVELECEF->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXNAVVELECEF->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_NAV_VELNED && msg->len == UBX_NAV_VELNED_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXNAVVELNED != NULL)
      {
        packetUBXNAVVELNED->data.iTOW = extractLong(msg, 0);
        packetUBXNAVVELNED->data.velN = extractSignedLong(msg, 4);
        packetUBXNAVVELNED->data.velE = extractSignedLong(msg, 8);
        packetUBXNAVVELNED->data.velD = extractSignedLong(msg, 12);
        packetUBXNAVVELNED->data.speed = extractLong(msg, 16);
        packetUBXNAVVELNED->data.gSpeed = extractLong(msg, 20);
        packetUBXNAVVELNED->data.heading = extractSignedLong(msg, 24);
        packetUBXNAVVELNED->data.sAcc = extractLong(msg, 28);
        packetUBXNAVVELNED->data.cAcc = extractLong(msg, 32);

        //Mark all datums as fresh (not read before)
        packetUBXNAVVELNED->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXNAVVELNEDcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXNAVVELNED->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXNAVVELNEDcopy->iTOW, &packetUBXNAVVELNED->data.iTOW, sizeof(UBX_NAV_VELNED_data_t));
          packetUBXNAVVELNED->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXNAVVELNED->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_NAV_HPPOSECEF && msg->len == UBX_NAV_HPPOSECEF_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXNAVHPPOSECEF != NULL)
      {
        packetUBXNAVHPPOSECEF->data.version = extractByte(msg, 0);
        packetUBXNAVHPPOSECEF->data.iTOW = extractLong(msg, 4);
        packetUBXNAVHPPOSECEF->data.ecefX = extractSignedLong(msg, 8);
        packetUBXNAVHPPOSECEF->data.ecefY = extractSignedLong(msg, 12);
        packetUBXNAVHPPOSECEF->data.ecefZ = extractSignedLong(msg, 16);
        packetUBXNAVHPPOSECEF->data.ecefXHp = extractSignedChar(msg, 20);
        packetUBXNAVHPPOSECEF->data.ecefYHp = extractSignedChar(msg, 21);
        packetUBXNAVHPPOSECEF->data.ecefZHp = extractSignedChar(msg, 22);
        packetUBXNAVHPPOSECEF->data.flags.all = extractByte(msg, 23);
        packetUBXNAVHPPOSECEF->data.pAcc = extractLong(msg, 24);

        //Mark all datums as fresh (not read before)
        packetUBXNAVHPPOSECEF->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXNAVHPPOSECEFcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXNAVHPPOSECEFcopy->version, &packetUBXNAVHPPOSECEF->data.version, sizeof(UBX_NAV_HPPOSECEF_data_t));
          packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_NAV_HPPOSLLH && msg->len == UBX_NAV_HPPOSLLH_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXNAVHPPOSLLH != NULL)
      {
        packetUBXNAVHPPOSLLH->data.version = extractByte(msg, 0);
        packetUBXNAVHPPOSLLH->data.flags.all = extractByte(msg, 3);
        packetUBXNAVHPPOSLLH->data.iTOW = extractLong(msg, 4);
        packetUBXNAVHPPOSLLH->data.lon = extractSignedLong(msg, 8);
        packetUBXNAVHPPOSLLH->data.lat = extractSignedLong(msg, 12);
        packetUBXNAVHPPOSLLH->data.height = extractSignedLong(msg, 16);
        packetUBXNAVHPPOSLLH->data.hMSL = extractSignedLong(msg, 20);
        packetUBXNAVHPPOSLLH->data.lonHp = extractSignedChar(msg, 24);
        packetUBXNAVHPPOSLLH->data.latHp = extractSignedChar(msg, 25);
        packetUBXNAVHPPOSLLH->data.heightHp = extractSignedChar(msg, 26);
        packetUBXNAVHPPOSLLH->data.hMSLHp = extractSignedChar(msg, 27);
        packetUBXNAVHPPOSLLH->data.hAcc = extractLong(msg, 28);
        packetUBXNAVHPPOSLLH->data.vAcc = extractLong(msg, 32);

        //Mark all datums as fresh (not read before)
        packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXNAVHPPOSLLHcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXNAVHPPOSLLHcopy->version, &packetUBXNAVHPPOSLLH->data.version, sizeof(UBX_NAV_HPPOSLLH_data_t));
          packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_NAV_CLOCK && msg->len == UBX_NAV_CLOCK_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXNAVCLOCK != NULL)
      {
        packetUBXNAVCLOCK->data.iTOW = extractLong(msg, 0);
        packetUBXNAVCLOCK->data.clkB = extractSignedLong(msg, 4);
        packetUBXNAVCLOCK->data.clkD = extractSignedLong(msg, 8);
        packetUBXNAVCLOCK->data.tAcc = extractLong(msg, 12);
        packetUBXNAVCLOCK->data.fAcc = extractLong(msg, 16);

        //Mark all datums as fresh (not read before)
        packetUBXNAVCLOCK->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXNAVCLOCKcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXNAVCLOCK->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXNAVCLOCKcopy->iTOW, &packetUBXNAVCLOCK->data.iTOW, sizeof(UBX_NAV_CLOCK_data_t));
          packetUBXNAVCLOCK->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXNAVCLOCK->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_NAV_SVIN && msg->len == UBX_NAV_SVIN_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXNAVSVIN != NULL)
      {
        packetUBXNAVSVIN->data.version = extractByte(msg, 0);
        packetUBXNAVSVIN->data.iTOW = extractLong(msg, 4);
        packetUBXNAVSVIN->data.dur = extractLong(msg, 8);
        packetUBXNAVSVIN->data.meanX = extractSignedLong(msg, 12);
        packetUBXNAVSVIN->data.meanY = extractSignedLong(msg, 16);
        packetUBXNAVSVIN->data.meanZ = extractSignedLong(msg, 20);
        packetUBXNAVSVIN->data.meanXHP = extractSignedChar(msg, 24);
        packetUBXNAVSVIN->data.meanYHP = extractSignedChar(msg, 25);
        packetUBXNAVSVIN->data.meanZHP = extractSignedChar(msg, 26);
        packetUBXNAVSVIN->data.meanAcc = extractLong(msg, 28);
        packetUBXNAVSVIN->data.obs = extractLong(msg, 32);
        packetUBXNAVSVIN->data.valid = extractSignedChar(msg, 36);
        packetUBXNAVSVIN->data.active = extractSignedChar(msg, 37);

        //Mark all datums as fresh (not read before)
        packetUBXNAVSVIN->moduleQueried.moduleQueried.all = 0xFFFFFFFF;
      }
    }
    else if (msg->id == UBX_NAV_RELPOSNED && ((msg->len == UBX_NAV_RELPOSNED_LEN) || (msg->len == UBX_NAV_RELPOSNED_LEN_F9)))
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXNAVRELPOSNED != NULL)
      {
        //Note:
        //  RELPOSNED on the M8 is only 40 bytes long
        //  RELPOSNED on the F9 is 64 bytes long and contains much more information

        packetUBXNAVRELPOSNED->data.version = extractByte(msg, 0);
        packetUBXNAVRELPOSNED->data.refStationId = extractInt(msg, 2);
        packetUBXNAVRELPOSNED->data.iTOW = extractLong(msg, 4);
        packetUBXNAVRELPOSNED->data.relPosN = extractSignedLong(msg, 8);
        packetUBXNAVRELPOSNED->data.relPosE = extractSignedLong(msg, 12);
        packetUBXNAVRELPOSNED->data.relPosD = extractSignedLong(msg, 16);

        if (msg->len == UBX_NAV_RELPOSNED_LEN)
        {
          // The M8 version does not contain relPosLength or relPosHeading
          packetUBXNAVRELPOSNED->data.relPosLength = 0;
          packetUBXNAVRELPOSNED->data.relPosHeading = 0;
          packetUBXNAVRELPOSNED->data.relPosHPN = extractSignedChar(msg, 20);
          packetUBXNAVRELPOSNED->data.relPosHPE = extractSignedChar(msg, 21);
          packetUBXNAVRELPOSNED->data.relPosHPD = extractSignedChar(msg, 22);
          packetUBXNAVRELPOSNED->data.relPosHPLength = 0; // The M8 version does not contain relPosHPLength
          packetUBXNAVRELPOSNED->data.accN = extractLong(msg, 24);
          packetUBXNAVRELPOSNED->data.accE = extractLong(msg, 28);
          packetUBXNAVRELPOSNED->data.accD = extractLong(msg, 32);
          // The M8 version does not contain accLength or accHeading
          packetUBXNAVRELPOSNED->data.accLength = 0;
          packetUBXNAVRELPOSNED->data.accHeading = 0;
          packetUBXNAVRELPOSNED->data.flags.all = extractLong(msg, 36);
        }
        else
        {
          packetUBXNAVRELPOSNED->data.relPosLength = extractSignedLong(msg, 20);
          packetUBXNAVRELPOSNED->data.relPosHeading = extractSignedLong(msg, 24);
          packetUBXNAVRELPOSNED->data.relPosHPN = extractSignedChar(msg, 32);
          packetUBXNAVRELPOSNED->data.relPosHPE = extractSignedChar(msg, 33);
          packetUBXNAVRELPOSNED->data.relPosHPD = extractSignedChar(msg, 34);
          packetUBXNAVRELPOSNED->data.relPosHPLength = extractSignedChar(msg, 35);
          packetUBXNAVRELPOSNED->data.accN = extractLong(msg, 36);
          packetUBXNAVRELPOSNED->data.accE = extractLong(msg, 40);
          packetUBXNAVRELPOSNED->data.accD = extractLong(msg, 44);
          packetUBXNAVRELPOSNED->data.accLength = extractLong(msg, 48);
          packetUBXNAVRELPOSNED->data.accHeading = extractLong(msg, 52);
          packetUBXNAVRELPOSNED->data.flags.all = extractLong(msg, 60);
        }

        //Mark all datums as fresh (not read before)
        packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXNAVRELPOSNEDcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXNAVRELPOSNED->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXNAVRELPOSNEDcopy->version, &packetUBXNAVRELPOSNED->data.version, sizeof(UBX_NAV_RELPOSNED_data_t));
          packetUBXNAVRELPOSNED->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXNAVRELPOSNED->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    break;
  case UBX_CLASS_RXM:
    if (msg->id == UBX_RXM_SFRBX)
    // Note: length is variable
    // Note: on protocol version 17: numWords is (0..16)
    //       on protocol version 18+: numWords is (0..10)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXRXMSFRBX != NULL)
      {
        packetUBXRXMSFRBX->data.gnssId = extractByte(msg, 0);
        packetUBXRXMSFRBX->data.svId = extractByte(msg, 1);
        packetUBXRXMSFRBX->data.freqId = extractByte(msg, 3);
        packetUBXRXMSFRBX->data.numWords = extractByte(msg, 4);
        packetUBXRXMSFRBX->data.chn = extractByte(msg, 5);
        packetUBXRXMSFRBX->data.version = extractByte(msg, 6);

        for (uint8_t i = 0; (i < UBX_RXM_SFRBX_MAX_WORDS) && (i < packetUBXRXMSFRBX->data.numWords)
          && ((i * 4) < (msg->len - 8)); i++)
        {
          packetUBXRXMSFRBX->data.dwrd[i] = extractLong(msg, 8 + (i * 4));
        }

        //Mark all datums as fresh (not read before)
        packetUBXRXMSFRBX->moduleQueried = true;

        //Check if we need to copy the data for the callback
        if ((packetUBXRXMSFRBXcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXRXMSFRBX->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXRXMSFRBXcopy->gnssId, &packetUBXRXMSFRBX->data.gnssId, sizeof(UBX_RXM_SFRBX_data_t));
          packetUBXRXMSFRBX->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXRXMSFRBX->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_RXM_RAWX)
    // Note: length is variable
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXRXMRAWX != NULL)
      {
        for (uint8_t i = 0; i < 8; i++)
        {
          packetUBXRXMRAWX->data.header.rcvTow[i] = extractByte(msg, i);
        }
        packetUBXRXMRAWX->data.header.week = extractInt(msg, 8);
        packetUBXRXMRAWX->data.header.leapS = extractSignedChar(msg, 10);
        packetUBXRXMRAWX->data.header.numMeas = extractByte(msg, 11);
        packetUBXRXMRAWX->data.header.recStat.all = extractByte(msg, 12);
        packetUBXRXMRAWX->data.header.version = extractByte(msg, 13);

        for (uint8_t i = 0; (i < UBX_RXM_RAWX_MAX_BLOCKS) && (i < packetUBXRXMRAWX->data.header.numMeas)
          && ((((uint16_t)i) * 32) < (msg->len - 16)); i++)
        {
          uint16_t offset = (((uint16_t)i) * 32) + 16;
          for (uint8_t j = 0; j < 8; j++)
          {
            packetUBXRXMRAWX->data.blocks[i].prMes[j] = extractByte(msg, offset + j);
            packetUBXRXMRAWX->data.blocks[i].cpMes[j] = extractByte(msg, offset + 8 + j);
            if (j < 4)
              packetUBXRXMRAWX->data.blocks[i].doMes[j] = extractByte(msg, offset + 16 + j);
          }
          packetUBXRXMRAWX->data.blocks[i].gnssId = extractByte(msg, offset + 20);
          packetUBXRXMRAWX->data.blocks[i].svId = extractByte(msg, offset + 21);
          packetUBXRXMRAWX->data.blocks[i].sigId = extractByte(msg, offset + 22);
          packetUBXRXMRAWX->data.blocks[i].freqId = extractByte(msg, offset + 23);
          packetUBXRXMRAWX->data.blocks[i].lockTime = extractInt(msg, offset + 24);
          packetUBXRXMRAWX->data.blocks[i].cno = extractByte(msg, offset + 26);
          packetUBXRXMRAWX->data.blocks[i].prStdev = extractByte(msg, offset + 27);
          packetUBXRXMRAWX->data.blocks[i].cpStdev = extractByte(msg, offset + 28);
          packetUBXRXMRAWX->data.blocks[i].doStdev = extractByte(msg, offset + 29);
          packetUBXRXMRAWX->data.blocks[i].trkStat.all = extractByte(msg, offset + 30);
        }

        //Mark all datums as fresh (not read before)
        packetUBXRXMRAWX->moduleQueried = true;

        //Check if we need to copy the data for the callback
        if ((packetUBXRXMRAWXcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXRXMRAWX->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXRXMRAWXcopy->header.rcvTow[0], &packetUBXRXMRAWX->data.header.rcvTow[0], sizeof(UBX_RXM_RAWX_data_t));
          packetUBXRXMRAWX->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXRXMRAWX->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    break;
  case UBX_CLASS_CFG:
    if (msg->id == UBX_CFG_RATE && msg->len == UBX_CFG_RATE_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXCFGRATE != NULL)
      {
        packetUBXCFGRATE->data.measRate = extractInt(msg, 0);
        packetUBXCFGRATE->data.navRate = extractInt(msg, 2);
        packetUBXCFGRATE->data.timeRef = extractInt(msg, 4);

        //Mark all datums as fresh (not read before)
        packetUBXCFGRATE->moduleQueried.moduleQueried.all = 0xFFFFFFFF;
      }
    }
    break;
  case UBX_CLASS_TIM:
    if (msg->id == UBX_TIM_TM2 && msg->len == UBX_TIM_TM2_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXTIMTM2 != NULL)
      {
        packetUBXTIMTM2->data.ch = extractByte(msg, 0);
        packetUBXTIMTM2->data.flags.all = extractByte(msg, 1);
        packetUBXTIMTM2->data.count = extractInt(msg, 2);
        packetUBXTIMTM2->data.wnR = extractInt(msg, 4);
        packetUBXTIMTM2->data.wnF = extractInt(msg, 6);
        packetUBXTIMTM2->data.towMsR = extractLong(msg, 8);
        packetUBXTIMTM2->data.towSubMsR = extractLong(msg, 12);
        packetUBXTIMTM2->data.towMsF = extractLong(msg, 16);
        packetUBXTIMTM2->data.towSubMsF = extractLong(msg, 20);
        packetUBXTIMTM2->data.accEst = extractLong(msg, 24);

        //Mark all datums as fresh (not read before)
        packetUBXTIMTM2->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXTIMTM2copy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXTIMTM2->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXTIMTM2copy->ch, &packetUBXTIMTM2->data.ch, sizeof(UBX_TIM_TM2_data_t));
          packetUBXTIMTM2->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXTIMTM2->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    break;
  case UBX_CLASS_ESF:
    if (msg->id == UBX_ESF_ALG && msg->len == UBX_ESF_ALG_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXESFALG != NULL)
      {
        packetUBXESFALG->data.iTOW = extractLong(msg, 0);
        packetUBXESFALG->data.version = extractByte(msg, 4);
        packetUBXESFALG->data.flags.all = extractByte(msg, 5);
        packetUBXESFALG->data.error.all = extractByte(msg, 6);
        packetUBXESFALG->data.yaw = extractLong(msg, 8);
        packetUBXESFALG->data.pitch = extractSignedInt(msg, 12);
        packetUBXESFALG->data.roll = extractSignedInt(msg, 14);

        //Mark all datums as fresh (not read before)
        packetUBXESFALG->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXESFALGcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXESFALG->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXESFALGcopy->iTOW, &packetUBXESFALG->data.iTOW, sizeof(UBX_ESF_ALG_data_t));
          packetUBXESFALG->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXESFALG->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_ESF_INS && msg->len == UBX_ESF_INS_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXESFINS != NULL)
      {
        packetUBXESFINS->data.bitfield0.all = extractLong(msg, 0);
        packetUBXESFINS->data.iTOW = extractLong(msg, 8);
        packetUBXESFINS->data.xAngRate = extractSignedLong(msg, 12);
        packetUBXESFINS->data.yAngRate = extractSignedLong(msg, 16);
        packetUBXESFINS->data.zAngRate = extractSignedLong(msg, 20);
        packetUBXESFINS->data.xAccel = extractSignedLong(msg, 24);
        packetUBXESFINS->data.yAccel = extractSignedLong(msg, 28);
        packetUBXESFINS->data.zAccel = extractSignedLong(msg, 32);

        //Mark all datums as fresh (not read before)
        packetUBXESFINS->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXESFINScopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXESFINS->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXESFINScopy->bitfield0.all, &packetUBXESFINS->data.bitfield0.all, sizeof(UBX_ESF_INS_data_t));
          packetUBXESFINS->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXESFINS->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_ESF_MEAS)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXESFMEAS != NULL)
      {
        packetUBXESFMEAS->data.timeTag = extractLong(msg, 0);
        packetUBXESFMEAS->data.flags.all = extractInt(msg, 4);
        packetUBXESFMEAS->data.id = extractInt(msg, 6);
        for (int i = 0; (i < DEF_NUM_SENS) && (i < packetUBXESFMEAS->data.flags.bits.numMeas)
          && ((i * 4) < (msg->len - 8)); i++)
        {
          packetUBXESFMEAS->data.data[i].data.all = extractLong(msg, 8 + (i * 4));
        }
        if (msg->len > (8 + (packetUBXESFMEAS->data.flags.bits.numMeas * 4)))
          packetUBXESFMEAS->data.calibTtag = extractLong(msg, 8 + (packetUBXESFMEAS->data.flags.bits.numMeas * 4));

        //Mark all datums as fresh (not read before)
        packetUBXESFMEAS->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXESFMEAScopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXESFMEAS->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXESFMEAScopy->timeTag, &packetUBXESFMEAS->data.timeTag, sizeof(UBX_ESF_MEAS_data_t));
          packetUBXESFMEAS->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXESFMEAS->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_ESF_RAW)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXESFRAW != NULL)
      {
        for (int i = 0; (i < DEF_NUM_SENS) && ((i * 8) < (msg->len - 4)); i++)
        {
          packetUBXESFRAW->data.data[i].data.all = extractLong(msg, 8 + (i * 8));
          packetUBXESFRAW->data.data[i].sTag = extractLong(msg, 8 + (i * 8) + 4);
        }

        //Mark all datums as fresh (not read before)
        packetUBXESFRAW->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXESFRAWcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXESFRAW->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXESFRAWcopy->data[0].data.all, &packetUBXESFRAW->data.data[0].data.all, sizeof(UBX_ESF_RAW_data_t));
          packetUBXESFRAW->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXESFRAW->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_ESF_STATUS)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXESFSTATUS != NULL)
      {
        packetUBXESFSTATUS->data.iTOW = extractLong(msg, 0);
        packetUBXESFSTATUS->data.version = extractByte(msg, 4);
        packetUBXESFSTATUS->data.fusionMode = extractByte(msg, 12);
        packetUBXESFSTATUS->data.numSens = extractByte(msg, 15);
        for (int i = 0; (i < DEF_NUM_SENS) && (i < packetUBXESFSTATUS->data.numSens)
          && ((i * 4) < (msg->len - 16)); i++)
        {
          packetUBXESFSTATUS->data.status[i].sensStatus1.all = extractByte(msg, 16 + (i * 4) + 0);
          packetUBXESFSTATUS->data.status[i].sensStatus2.all = extractByte(msg, 16 + (i * 4) + 1);
          packetUBXESFSTATUS->data.status[i].freq = extractByte(msg, 16 + (i * 4) + 2);
          packetUBXESFSTATUS->data.status[i].faults.all = extractByte(msg, 16 + (i * 4) + 3);
        }

        //Mark all datums as fresh (not read before)
        packetUBXESFSTATUS->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXESFSTATUScopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXESFSTATUS->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXESFSTATUScopy->iTOW, &packetUBXESFSTATUS->data.iTOW, sizeof(UBX_ESF_STATUS_data_t));
          packetUBXESFSTATUS->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXESFSTATUS->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    break;
  case UBX_CLASS_HNR:
    if (msg->id == UBX_HNR_PVT && msg->len == UBX_HNR_PVT_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXHNRPVT != NULL)
      {
        packetUBXHNRPVT->data.iTOW = extractLong(msg, 0);
        packetUBXHNRPVT->data.year = extractInt(msg, 4);
        packetUBXHNRPVT->data.month = extractByte(msg, 6);
        packetUBXHNRPVT->data.day = extractByte(msg, 7);
        packetUBXHNRPVT->data.hour = extractByte(msg, 8);
        packetUBXHNRPVT->data.min = extractByte(msg, 9);
        packetUBXHNRPVT->data.sec = extractByte(msg, 10);
        packetUBXHNRPVT->data.valid.all = extractByte(msg, 11);
        packetUBXHNRPVT->data.nano = extractSignedLong(msg, 12);
        packetUBXHNRPVT->data.gpsFix = extractByte(msg, 16);
        packetUBXHNRPVT->data.flags.all = extractByte(msg, 17);
        packetUBXHNRPVT->data.lon = extractSignedLong(msg, 20);
        packetUBXHNRPVT->data.lat = extractSignedLong(msg, 24);
        packetUBXHNRPVT->data.height = extractSignedLong(msg, 28);
        packetUBXHNRPVT->data.hMSL = extractSignedLong(msg, 32);
        packetUBXHNRPVT->data.gSpeed = extractSignedLong(msg, 36);
        packetUBXHNRPVT->data.speed = extractSignedLong(msg, 40);
        packetUBXHNRPVT->data.headMot = extractSignedLong(msg, 44);
        packetUBXHNRPVT->data.headVeh = extractSignedLong(msg, 48);
        packetUBXHNRPVT->data.hAcc = extractLong(msg, 52);
        packetUBXHNRPVT->data.vAcc = extractLong(msg, 56);
        packetUBXHNRPVT->data.sAcc = extractLong(msg, 60);
        packetUBXHNRPVT->data.headAcc = extractLong(msg, 64);

        //Mark all datums as fresh (not read before)
        packetUBXHNRPVT->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXHNRPVTcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXHNRPVT->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXHNRPVTcopy->iTOW, &packetUBXHNRPVT->data.iTOW, sizeof(UBX_HNR_PVT_data_t));
          packetUBXHNRPVT->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXHNRPVT->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_HNR_ATT && msg->len == UBX_HNR_ATT_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXHNRATT != NULL)
      {
        packetUBXHNRATT->data.iTOW = extractLong(msg, 0);
        packetUBXHNRATT->data.version = extractByte(msg, 4);
        packetUBXHNRATT->data.roll = extractSignedLong(msg, 8);
        packetUBXHNRATT->data.pitch = extractSignedLong(msg, 12);
        packetUBXHNRATT->data.heading = extractSignedLong(msg, 16);
        packetUBXHNRATT->data.accRoll = extractLong(msg, 20);
        packetUBXHNRATT->data.accPitch = extractLong(msg, 24);
        packetUBXHNRATT->data.accHeading = extractLong(msg, 28);

        //Mark all datums as fresh (not read before)
        packetUBXHNRATT->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXHNRATTcopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXHNRATT->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXHNRATTcopy->iTOW, &packetUBXHNRATT->data.iTOW, sizeof(UBX_HNR_ATT_data_t));
          packetUBXHNRATT->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXHNRATT->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    else if (msg->id == UBX_HNR_INS && msg->len == UBX_HNR_INS_LEN)
    {
      //Parse various byte fields into storage - but only if we have memory allocated for it
      if (packetUBXHNRINS != NULL)
      {
        packetUBXHNRINS->data.bitfield0.all = extractLong(msg, 0);
        packetUBXHNRINS->data.iTOW = extractLong(msg, 8);
        packetUBXHNRINS->data.xAngRate = extractSignedLong(msg, 12);
        packetUBXHNRINS->data.yAngRate = extractSignedLong(msg, 16);
        packetUBXHNRINS->data.zAngRate = extractSignedLong(msg, 20);
        packetUBXHNRINS->data.xAccel = extractSignedLong(msg, 24);
        packetUBXHNRINS->data.yAccel = extractSignedLong(msg, 28);
        packetUBXHNRINS->data.zAccel = extractSignedLong(msg, 32);

        //Mark all datums as fresh (not read before)
        packetUBXHNRINS->moduleQueried.moduleQueried.all = 0xFFFFFFFF;

        //Check if we need to copy the data for the callback
        if ((packetUBXHNRINScopy != NULL) // If RAM has been allocated for the copy of the data
          && (packetUBXHNRINS->automaticFlags.flags.bits.callbackCopyValid == false)) // AND the data is stale
        {
          memcpy(&packetUBXHNRINScopy->bitfield0.all, &packetUBXHNRINS->data.bitfield0.all, sizeof(UBX_HNR_INS_data_t));
          packetUBXHNRINS->automaticFlags.flags.bits.callbackCopyValid = true;
        }

        //Check if we need to copy the data into the file buffer
        if (packetUBXHNRINS->automaticFlags.flags.bits.addToFileBuffer)
        {
          storePacket(msg);
        }
      }
    }
    break;
  }
}

//Given a message, calc and store the two byte "8-Bit Fletcher" checksum over the entirety of the message
//This is called before we send a command message
void SFE_UBLOX_GPS::calcChecksum(ubxPacket *msg)
{
  msg->checksumA = 0;
  msg->checksumB = 0;

  msg->checksumA += msg->cls;
  msg->checksumB += msg->checksumA;

  msg->checksumA += msg->id;
  msg->checksumB += msg->checksumA;

  msg->checksumA += (msg->len & 0xFF);
  msg->checksumB += msg->checksumA;

  msg->checksumA += (msg->len >> 8);
  msg->checksumB += msg->checksumA;

  for (uint16_t i = 0; i < msg->len; i++)
  {
    msg->checksumA += msg->payload[i];
    msg->checksumB += msg->checksumA;
  }
}

//Given a message and a byte, add to rolling "8-Bit Fletcher" checksum
//This is used when receiving messages from module
void SFE_UBLOX_GPS::addToChecksum(uint8_t incoming)
{
  rollingChecksumA += incoming;
  rollingChecksumB += rollingChecksumA;
}

//Given a packet and payload, send everything including CRC bytes via I2C port
sfe_ublox_status_e SFE_UBLOX_GPS::sendCommand(ubxPacket *outgoingUBX, uint16_t maxWait)
{
  sfe_ublox_status_e retVal = SFE_UBLOX_STATUS_SUCCESS;

  calcChecksum(outgoingUBX); //Sets checksum A and B bytes of the packet

  if (_printDebug == true)
  {
    _debugSerial->print(F("\nSending: "));
    printPacket(outgoingUBX, true); // Always print payload
  }

  if (commType == COMM_TYPE_I2C)
  {
    retVal = sendI2cCommand(outgoingUBX, maxWait);
    if (retVal != SFE_UBLOX_STATUS_SUCCESS)
    {
      if (_printDebug == true)
      {
        _debugSerial->println(F("Send I2C Command failed"));
      }
      return retVal;
    }
  }
  else if (commType == COMM_TYPE_SERIAL)
  {
    sendSerialCommand(outgoingUBX);
  }

  if (maxWait > 0)
  {
    //Depending on what we just sent, either we need to look for an ACK or not
    if (outgoingUBX->cls == UBX_CLASS_CFG)
    {
      if (_printDebug == true)
      {
        _debugSerial->println(F("sendCommand: Waiting for ACK response"));
      }
      retVal = waitForACKResponse(outgoingUBX, outgoingUBX->cls, outgoingUBX->id, maxWait); //Wait for Ack response
    }
    else
    {
      if (_printDebug == true)
      {
        _debugSerial->println(F("sendCommand: Waiting for No ACK response"));
      }
      retVal = waitForNoACKResponse(outgoingUBX, outgoingUBX->cls, outgoingUBX->id, maxWait); //Wait for Ack response
    }
  }
  return retVal;
}

//Returns false if sensor fails to respond to I2C traffic
sfe_ublox_status_e SFE_UBLOX_GPS::sendI2cCommand(ubxPacket *outgoingUBX, uint16_t maxWait)
{
  //Point at 0xFF data register
  _i2cPort->beginTransmission((uint8_t)_gpsI2Caddress); //There is no register to write to, we just begin writing data bytes
  _i2cPort->write(0xFF);
  if (_i2cPort->endTransmission(false) != 0)         //Don't release bus
    return (SFE_UBLOX_STATUS_I2C_COMM_FAILURE); //Sensor did not ACK

  //Write header bytes
  _i2cPort->beginTransmission((uint8_t)_gpsI2Caddress); //There is no register to write to, we just begin writing data bytes
  _i2cPort->write(UBX_SYNCH_1);                         //μ - oh ublox, you're funny. I will call you micro-blox from now on.
  _i2cPort->write(UBX_SYNCH_2);                         //b
  _i2cPort->write(outgoingUBX->cls);
  _i2cPort->write(outgoingUBX->id);
  _i2cPort->write(outgoingUBX->len & 0xFF);     //LSB
  _i2cPort->write(outgoingUBX->len >> 8);       //MSB
  if (_i2cPort->endTransmission(false) != 0)    //Do not release bus
    return (SFE_UBLOX_STATUS_I2C_COMM_FAILURE); //Sensor did not ACK

  //Write payload. Limit the sends into 32 byte chunks
  //This code based on ublox: https://forum.u-blox.com/index.php/20528/how-to-use-i2c-to-get-the-nmea-frames
  uint16_t bytesToSend = outgoingUBX->len;

  //"The number of data bytes must be at least 2 to properly distinguish
  //from the write access to set the address counter in random read accesses."
  uint16_t startSpot = 0;
  while (bytesToSend > 1)
  {
    uint8_t len = bytesToSend;
    if (len > i2cTransactionSize)
      len = i2cTransactionSize;

    _i2cPort->beginTransmission((uint8_t)_gpsI2Caddress);
    //_i2cPort->write(outgoingUBX->payload, len); //Write a portion of the payload to the bus

    for (uint16_t x = 0; x < len; x++)
      _i2cPort->write(outgoingUBX->payload[startSpot + x]); //Write a portion of the payload to the bus

    if (_i2cPort->endTransmission(false) != 0)    //Don't release bus
      return (SFE_UBLOX_STATUS_I2C_COMM_FAILURE); //Sensor did not ACK

    //*outgoingUBX->payload += len; //Move the pointer forward
    startSpot += len; //Move the pointer forward
    bytesToSend -= len;
  }

  //Write checksum
  _i2cPort->beginTransmission((uint8_t)_gpsI2Caddress);
  if (bytesToSend == 1)
    _i2cPort->write(outgoingUBX->payload, 1);
  _i2cPort->write(outgoingUBX->checksumA);
  _i2cPort->write(outgoingUBX->checksumB);

  //All done transmitting bytes. Release bus.
  if (_i2cPort->endTransmission() != 0)
    return (SFE_UBLOX_STATUS_I2C_COMM_FAILURE); //Sensor did not ACK
  return (SFE_UBLOX_STATUS_SUCCESS);
}

//Given a packet and payload, send everything including CRC bytesA via Serial port
void SFE_UBLOX_GPS::sendSerialCommand(ubxPacket *outgoingUBX)
{
  //Write header bytes
  _serialPort->write(UBX_SYNCH_1); //μ - oh ublox, you're funny. I will call you micro-blox from now on.
  _serialPort->write(UBX_SYNCH_2); //b
  _serialPort->write(outgoingUBX->cls);
  _serialPort->write(outgoingUBX->id);
  _serialPort->write(outgoingUBX->len & 0xFF); //LSB
  _serialPort->write(outgoingUBX->len >> 8);   //MSB

  //Write payload.
  for (int i = 0; i < outgoingUBX->len; i++)
  {
    _serialPort->write(outgoingUBX->payload[i]);
  }

  //Write checksum
  _serialPort->write(outgoingUBX->checksumA);
  _serialPort->write(outgoingUBX->checksumB);
}

//Pretty prints the current ubxPacket
void SFE_UBLOX_GPS::printPacket(ubxPacket *packet, boolean alwaysPrintPayload)
{
  if (_printDebug == true)
  {
    _debugSerial->print(F("CLS:"));
    if (packet->cls == UBX_CLASS_NAV) //1
      _debugSerial->print(F("NAV"));
    else if (packet->cls == UBX_CLASS_ACK) //5
      _debugSerial->print(F("ACK"));
    else if (packet->cls == UBX_CLASS_CFG) //6
      _debugSerial->print(F("CFG"));
    else if (packet->cls == UBX_CLASS_MON) //0x0A
      _debugSerial->print(F("MON"));
    else
    {
      _debugSerial->print(F("0x"));
      _debugSerial->print(packet->cls, HEX);
    }

    _debugSerial->print(F(" ID:"));
    if (packet->cls == UBX_CLASS_NAV && packet->id == UBX_NAV_PVT)
      _debugSerial->print(F("PVT"));
    else if (packet->cls == UBX_CLASS_CFG && packet->id == UBX_CFG_RATE)
      _debugSerial->print(F("RATE"));
    else if (packet->cls == UBX_CLASS_CFG && packet->id == UBX_CFG_CFG)
      _debugSerial->print(F("SAVE"));
    else
    {
      _debugSerial->print(F("0x"));
      _debugSerial->print(packet->id, HEX);
    }

    _debugSerial->print(F(" Len: 0x"));
    _debugSerial->print(packet->len, HEX);

    // Only print the payload is ignoreThisPayload is false otherwise
    // we could be printing gibberish from beyond the end of packetBuf
    if ((alwaysPrintPayload == true) || (ignoreThisPayload == false))
    {
      _debugSerial->print(F(" Payload:"));

      for (int x = 0; x < packet->len; x++)
      {
        _debugSerial->print(F(" "));
        _debugSerial->print(packet->payload[x], HEX);
      }
    }
    else
    {
      _debugSerial->print(F(" Payload: IGNORED"));
    }
    _debugSerial->println();
  }
}

//When messages from the class CFG are sent to the receiver, the receiver will send an "acknowledge"(UBX - ACK - ACK) or a
//"not acknowledge"(UBX-ACK-NAK) message back to the sender, depending on whether or not the message was processed correctly.
//Some messages from other classes also use the same acknowledgement mechanism.

//When we poll or get a setting, we will receive _both_ a config packet and an ACK
//If the poll or get request is not valid, we will receive _only_ a NACK

//If we are trying to get or poll a setting, then packetCfg.len will be 0 or 1 when the packetCfg is _sent_.
//If we poll the setting for a particular port using UBX-CFG-PRT then .len will be 1 initially
//For all other gets or polls, .len will be 0 initially
//(It would be possible for .len to be 2 _if_ we were using UBX-CFG-MSG to poll the settings for a particular message - but we don't use that (currently))

//If the get or poll _fails_, i.e. is NACK'd, then packetCfg.len could still be 0 or 1 after the NACK is received
//But if the get or poll is ACK'd, then packetCfg.len will have been updated by the incoming data and will always be at least 2

//If we are going to set the value for a setting, then packetCfg.len will be at least 3 when the packetCfg is _sent_.
//(UBX-CFG-MSG appears to have the shortest set length of 3 bytes)

//We need to think carefully about how interleaved PVT packets affect things.
//It is entirely possible that our packetCfg and packetAck were received successfully
//but while we are still in the "if (checkUblox() == true)" loop a PVT packet is processed
//or _starts_ to arrive (remember that Serial data can arrive very slowly).

//Returns SFE_UBLOX_STATUS_DATA_RECEIVED if we got an ACK and a valid packetCfg (module is responding with register content)
//Returns SFE_UBLOX_STATUS_DATA_SENT if we got an ACK and no packetCfg (no valid packetCfg needed, module absorbs new register data)
//Returns SFE_UBLOX_STATUS_FAIL if something very bad happens (e.g. a double checksum failure)
//Returns SFE_UBLOX_STATUS_COMMAND_NACK if the packet was not-acknowledged (NACK)
//Returns SFE_UBLOX_STATUS_CRC_FAIL if we had a checksum failure
//Returns SFE_UBLOX_STATUS_TIMEOUT if we timed out
//Returns SFE_UBLOX_STATUS_DATA_OVERWRITTEN if we got an ACK and a valid packetCfg but that the packetCfg has been
// or is currently being overwritten (remember that Serial data can arrive very slowly)
sfe_ublox_status_e SFE_UBLOX_GPS::waitForACKResponse(ubxPacket *outgoingUBX, uint8_t requestedClass, uint8_t requestedID, uint16_t maxTime)
{
  outgoingUBX->valid = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED; //This will go VALID (or NOT_VALID) when we receive a response to the packet we sent
  packetAck.valid = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED;
  packetBuf.valid = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED;
  packetAuto.valid = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED;
  outgoingUBX->classAndIDmatch = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED; // This will go VALID (or NOT_VALID) when we receive a packet that matches the requested class and ID
  packetAck.classAndIDmatch = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED;
  packetBuf.classAndIDmatch = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED;
  packetAuto.classAndIDmatch = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED;

  unsigned long startTime = millis();
  while (millis() - startTime < maxTime)
  {
    if (checkUbloxInternal(outgoingUBX, requestedClass, requestedID) == true) //See if new data is available. Process bytes as they come in.
    {
      // If both the outgoingUBX->classAndIDmatch and packetAck.classAndIDmatch are VALID
      // and outgoingUBX->valid is _still_ VALID and the class and ID _still_ match
      // then we can be confident that the data in outgoingUBX is valid
      if ((outgoingUBX->classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_VALID) && (packetAck.classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_VALID) && (outgoingUBX->valid == SFE_UBLOX_PACKET_VALIDITY_VALID) && (outgoingUBX->cls == requestedClass) && (outgoingUBX->id == requestedID))
      {
        if (_printDebug == true)
        {
          _debugSerial->print(F("waitForACKResponse: valid data and valid ACK received after "));
          _debugSerial->print(millis() - startTime);
          _debugSerial->println(F(" msec"));
        }
        return (SFE_UBLOX_STATUS_DATA_RECEIVED); //We received valid data and a correct ACK!
      }

      // We can be confident that the data packet (if we are going to get one) will always arrive
      // before the matching ACK. So if we sent a config packet which only produces an ACK
      // then outgoingUBX->classAndIDmatch will be NOT_DEFINED and the packetAck.classAndIDmatch will VALID.
      // We should not check outgoingUBX->valid, outgoingUBX->cls or outgoingUBX->id
      // as these may have been changed by an automatic packet.
      else if ((outgoingUBX->classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED) && (packetAck.classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_VALID))
      {
        if (_printDebug == true)
        {
          _debugSerial->print(F("waitForACKResponse: no data and valid ACK after "));
          _debugSerial->print(millis() - startTime);
          _debugSerial->println(F(" msec"));
        }
        return (SFE_UBLOX_STATUS_DATA_SENT); //We got an ACK but no data...
      }

      // If both the outgoingUBX->classAndIDmatch and packetAck.classAndIDmatch are VALID
      // but the outgoingUBX->cls or ID no longer match then we can be confident that we had
      // valid data but it has been or is currently being overwritten by an automatic packet (e.g. PVT).
      // If (e.g.) a PVT packet is _being_ received: outgoingUBX->valid will be NOT_DEFINED
      // If (e.g.) a PVT packet _has been_ received: outgoingUBX->valid will be VALID (or just possibly NOT_VALID)
      // So we cannot use outgoingUBX->valid as part of this check.
      // Note: the addition of packetBuf should make this check redundant!
      else if ((outgoingUBX->classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_VALID) && (packetAck.classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_VALID) && ((outgoingUBX->cls != requestedClass) || (outgoingUBX->id != requestedID)))
      {
        if (_printDebug == true)
        {
          _debugSerial->print(F("waitForACKResponse: data being OVERWRITTEN after "));
          _debugSerial->print(millis() - startTime);
          _debugSerial->println(F(" msec"));
        }
        return (SFE_UBLOX_STATUS_DATA_OVERWRITTEN); // Data was valid but has been or is being overwritten
      }

      // If packetAck.classAndIDmatch is VALID but both outgoingUBX->valid and outgoingUBX->classAndIDmatch
      // are NOT_VALID then we can be confident we have had a checksum failure on the data packet
      else if ((packetAck.classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_VALID) && (outgoingUBX->classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_NOT_VALID) && (outgoingUBX->valid == SFE_UBLOX_PACKET_VALIDITY_NOT_VALID))
      {
        if (_printDebug == true)
        {
          _debugSerial->print(F("waitForACKResponse: CRC failed after "));
          _debugSerial->print(millis() - startTime);
          _debugSerial->println(F(" msec"));
        }
        return (SFE_UBLOX_STATUS_CRC_FAIL); //Checksum fail
      }

      // If our packet was not-acknowledged (NACK) we do not receive a data packet - we only get the NACK.
      // So you would expect outgoingUBX->valid and outgoingUBX->classAndIDmatch to still be NOT_DEFINED
      // But if a full PVT packet arrives afterwards outgoingUBX->valid could be VALID (or just possibly NOT_VALID)
      // but outgoingUBX->cls and outgoingUBX->id would not match...
      // So I think this is telling us we need a special state for packetAck.classAndIDmatch to tell us
      // the packet was definitely NACK'd otherwise we are possibly just guessing...
      // Note: the addition of packetBuf changes the logic of this, but we'll leave the code as is for now.
      else if (packetAck.classAndIDmatch == SFE_UBLOX_PACKET_NOTACKNOWLEDGED)
      {
        if (_printDebug == true)
        {
          _debugSerial->print(F("waitForACKResponse: data was NOTACKNOWLEDGED (NACK) after "));
          _debugSerial->print(millis() - startTime);
          _debugSerial->println(F(" msec"));
        }
        return (SFE_UBLOX_STATUS_COMMAND_NACK); //We received a NACK!
      }

      // If the outgoingUBX->classAndIDmatch is VALID but the packetAck.classAndIDmatch is NOT_VALID
      // then the ack probably had a checksum error. We will take a gamble and return DATA_RECEIVED.
      // If we were playing safe, we should return FAIL instead
      else if ((outgoingUBX->classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_VALID) && (packetAck.classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_NOT_VALID) && (outgoingUBX->valid == SFE_UBLOX_PACKET_VALIDITY_VALID) && (outgoingUBX->cls == requestedClass) && (outgoingUBX->id == requestedID))
      {
        if (_printDebug == true)
        {
          _debugSerial->print(F("waitForACKResponse: VALID data and INVALID ACK received after "));
          _debugSerial->print(millis() - startTime);
          _debugSerial->println(F(" msec"));
        }
        return (SFE_UBLOX_STATUS_DATA_RECEIVED); //We received valid data and an invalid ACK!
      }

      // If the outgoingUBX->classAndIDmatch is NOT_VALID and the packetAck.classAndIDmatch is NOT_VALID
      // then we return a FAIL. This must be a double checksum failure?
      else if ((outgoingUBX->classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_NOT_VALID) && (packetAck.classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_NOT_VALID))
      {
        if (_printDebug == true)
        {
          _debugSerial->print(F("waitForACKResponse: INVALID data and INVALID ACK received after "));
          _debugSerial->print(millis() - startTime);
          _debugSerial->println(F(" msec"));
        }
        return (SFE_UBLOX_STATUS_FAIL); //We received invalid data and an invalid ACK!
      }

      // If the outgoingUBX->classAndIDmatch is VALID and the packetAck.classAndIDmatch is NOT_DEFINED
      // then the ACK has not yet been received and we should keep waiting for it
      else if ((outgoingUBX->classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_VALID) && (packetAck.classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED))
      {
        // if (_printDebug == true)
        // {
        //   _debugSerial->print(F("waitForACKResponse: valid data after "));
        //   _debugSerial->print(millis() - startTime);
        //   _debugSerial->println(F(" msec. Waiting for ACK."));
        // }
      }

    } //checkUbloxInternal == true

    delayMicroseconds(500);
  } //while (millis() - startTime < maxTime)

  // We have timed out...
  // If the outgoingUBX->classAndIDmatch is VALID then we can take a gamble and return DATA_RECEIVED
  // even though we did not get an ACK
  if ((outgoingUBX->classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_VALID) && (packetAck.classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED) && (outgoingUBX->valid == SFE_UBLOX_PACKET_VALIDITY_VALID) && (outgoingUBX->cls == requestedClass) && (outgoingUBX->id == requestedID))
  {
    if (_printDebug == true)
    {
      _debugSerial->print(F("waitForACKResponse: TIMEOUT with valid data after "));
      _debugSerial->print(millis() - startTime);
      _debugSerial->println(F(" msec. "));
    }
    return (SFE_UBLOX_STATUS_DATA_RECEIVED); //We received valid data... But no ACK!
  }

  if (_printDebug == true)
  {
    _debugSerial->print(F("waitForACKResponse: TIMEOUT after "));
    _debugSerial->print(millis() - startTime);
    _debugSerial->println(F(" msec."));
  }

  return (SFE_UBLOX_STATUS_TIMEOUT);
}

//For non-CFG queries no ACK is sent so we use this function
//Returns SFE_UBLOX_STATUS_DATA_RECEIVED if we got a config packet full of response data that has CLS/ID match to our query packet
//Returns SFE_UBLOX_STATUS_CRC_FAIL if we got a corrupt config packet that has CLS/ID match to our query packet
//Returns SFE_UBLOX_STATUS_TIMEOUT if we timed out
//Returns SFE_UBLOX_STATUS_DATA_OVERWRITTEN if we got an a valid packetCfg but that the packetCfg has been
// or is currently being overwritten (remember that Serial data can arrive very slowly)
sfe_ublox_status_e SFE_UBLOX_GPS::waitForNoACKResponse(ubxPacket *outgoingUBX, uint8_t requestedClass, uint8_t requestedID, uint16_t maxTime)
{
  outgoingUBX->valid = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED; //This will go VALID (or NOT_VALID) when we receive a response to the packet we sent
  packetAck.valid = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED;
  packetBuf.valid = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED;
  packetAuto.valid = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED;
  outgoingUBX->classAndIDmatch = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED; // This will go VALID (or NOT_VALID) when we receive a packet that matches the requested class and ID
  packetAck.classAndIDmatch = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED;
  packetBuf.classAndIDmatch = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED;
  packetAuto.classAndIDmatch = SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED;

  unsigned long startTime = millis();
  while (millis() - startTime < maxTime)
  {
    if (checkUbloxInternal(outgoingUBX, requestedClass, requestedID) == true) //See if new data is available. Process bytes as they come in.
    {

      // If outgoingUBX->classAndIDmatch is VALID
      // and outgoingUBX->valid is _still_ VALID and the class and ID _still_ match
      // then we can be confident that the data in outgoingUBX is valid
      if ((outgoingUBX->classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_VALID) && (outgoingUBX->valid == SFE_UBLOX_PACKET_VALIDITY_VALID) && (outgoingUBX->cls == requestedClass) && (outgoingUBX->id == requestedID))
      {
        if (_printDebug == true)
        {
          _debugSerial->print(F("waitForNoACKResponse: valid data with CLS/ID match after "));
          _debugSerial->print(millis() - startTime);
          _debugSerial->println(F(" msec"));
        }
        return (SFE_UBLOX_STATUS_DATA_RECEIVED); //We received valid data!
      }

      // If the outgoingUBX->classAndIDmatch is VALID
      // but the outgoingUBX->cls or ID no longer match then we can be confident that we had
      // valid data but it has been or is currently being overwritten by another packet (e.g. PVT).
      // If (e.g.) a PVT packet is _being_ received: outgoingUBX->valid will be NOT_DEFINED
      // If (e.g.) a PVT packet _has been_ received: outgoingUBX->valid will be VALID (or just possibly NOT_VALID)
      // So we cannot use outgoingUBX->valid as part of this check.
      // Note: the addition of packetBuf should make this check redundant!
      else if ((outgoingUBX->classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_VALID) && ((outgoingUBX->cls != requestedClass) || (outgoingUBX->id != requestedID)))
      {
        if (_printDebug == true)
        {
          _debugSerial->print(F("waitForNoACKResponse: data being OVERWRITTEN after "));
          _debugSerial->print(millis() - startTime);
          _debugSerial->println(F(" msec"));
        }
        return (SFE_UBLOX_STATUS_DATA_OVERWRITTEN); // Data was valid but has been or is being overwritten
      }

      // If outgoingUBX->classAndIDmatch is NOT_DEFINED
      // and outgoingUBX->valid is VALID then this must be (e.g.) a PVT packet
      else if ((outgoingUBX->classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED) && (outgoingUBX->valid == SFE_UBLOX_PACKET_VALIDITY_VALID))
      {
        // if (_printDebug == true)
        // {
        //   _debugSerial->print(F("waitForNoACKResponse: valid but UNWANTED data after "));
        //   _debugSerial->print(millis() - startTime);
        //   _debugSerial->print(F(" msec. Class: "));
        //   _debugSerial->print(outgoingUBX->cls);
        //   _debugSerial->print(F(" ID: "));
        //   _debugSerial->print(outgoingUBX->id);
        // }
      }

      // If the outgoingUBX->classAndIDmatch is NOT_VALID then we return CRC failure
      else if (outgoingUBX->classAndIDmatch == SFE_UBLOX_PACKET_VALIDITY_NOT_VALID)
      {
        if (_printDebug == true)
        {
          _debugSerial->print(F("waitForNoACKResponse: CLS/ID match but failed CRC after "));
          _debugSerial->print(millis() - startTime);
          _debugSerial->println(F(" msec"));
        }
        return (SFE_UBLOX_STATUS_CRC_FAIL); //We received invalid data
      }
    }

    delayMicroseconds(500);
  }

  if (_printDebug == true)
  {
    _debugSerial->print(F("waitForNoACKResponse: TIMEOUT after "));
    _debugSerial->print(millis() - startTime);
    _debugSerial->println(F(" msec. No packet received."));
  }

  return (SFE_UBLOX_STATUS_TIMEOUT);
}

// Check if any callbacks are waiting to be processed
void SFE_UBLOX_GPS::checkCallbacks(void)
{
  if (checkCallbacksReentrant == true) // Check for reentry (i.e. checkCallbacks has been called from inside a callback)
    return;

  checkCallbacksReentrant = true;

  if ((packetUBXNAVPOSECEFcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXNAVPOSECEF->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXNAVPOSECEF->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for NAV POSECEF"));
    packetUBXNAVPOSECEF->automaticFlags.callbackPointer(); // Call the callback
    packetUBXNAVPOSECEF->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXNAVSTATUScopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXNAVSTATUS->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXNAVSTATUS->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for NAV STATUS"));
    packetUBXNAVSTATUS->automaticFlags.callbackPointer(); // Call the callback
    packetUBXNAVSTATUS->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXNAVDOPcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXNAVDOP->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXNAVDOP->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for NAV DOP"));
    packetUBXNAVDOP->automaticFlags.callbackPointer(); // Call the callback
    packetUBXNAVDOP->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXNAVATTcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXNAVATT->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXNAVATT->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for NAV ATT"));
    packetUBXNAVATT->automaticFlags.callbackPointer(); // Call the callback
    packetUBXNAVATT->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXNAVPVTcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXNAVPVT->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXNAVPVT->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for NAV PVT"));
    packetUBXNAVPVT->automaticFlags.callbackPointer(); // Call the callback
    packetUBXNAVPVT->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXNAVODOcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXNAVODO->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXNAVODO->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for NAV ODO"));
    packetUBXNAVODO->automaticFlags.callbackPointer(); // Call the callback
    packetUBXNAVODO->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXNAVVELECEFcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXNAVVELECEF->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXNAVVELECEF->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for NAV VELECEF"));
    packetUBXNAVVELECEF->automaticFlags.callbackPointer(); // Call the callback
    packetUBXNAVVELECEF->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXNAVVELNEDcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXNAVVELNED->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXNAVVELNED->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for NAV VELNED"));
    packetUBXNAVVELNED->automaticFlags.callbackPointer(); // Call the callback
    packetUBXNAVVELNED->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXNAVHPPOSECEFcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXNAVHPPOSECEF->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for NAV HPPOSECEF"));
    packetUBXNAVHPPOSECEF->automaticFlags.callbackPointer(); // Call the callback
    packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXNAVHPPOSLLHcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXNAVHPPOSLLH->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for NAV HPPOSLLH"));
    packetUBXNAVHPPOSLLH->automaticFlags.callbackPointer(); // Call the callback
    packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXNAVCLOCKcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXNAVCLOCK->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXNAVCLOCK->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for NAV CLOCK"));
    packetUBXNAVCLOCK->automaticFlags.callbackPointer(); // Call the callback
    packetUBXNAVCLOCK->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXNAVRELPOSNEDcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXNAVRELPOSNED->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXNAVRELPOSNED->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for NAV RELPOSNED"));
    packetUBXNAVRELPOSNED->automaticFlags.callbackPointer(); // Call the callback
    packetUBXNAVRELPOSNED->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXRXMSFRBXcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXRXMSFRBX->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXRXMSFRBX->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for RXM SFRBX"));
    packetUBXRXMSFRBX->automaticFlags.callbackPointer(); // Call the callback
    packetUBXRXMSFRBX->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXRXMRAWXcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXRXMRAWX->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXRXMRAWX->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for RXM RAWX"));
    packetUBXRXMRAWX->automaticFlags.callbackPointer(); // Call the callback
    packetUBXRXMRAWX->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXTIMTM2copy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXTIMTM2->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXTIMTM2->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for TIM TM2"));
    packetUBXTIMTM2->automaticFlags.callbackPointer(); // Call the callback
    packetUBXTIMTM2->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXESFALGcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXESFALG->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXESFALG->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for ESF ALG"));
    packetUBXESFALG->automaticFlags.callbackPointer(); // Call the callback
    packetUBXESFALG->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXESFINScopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXESFINS->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXESFINS->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for ESF INS"));
    packetUBXESFINS->automaticFlags.callbackPointer(); // Call the callback
    packetUBXESFINS->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXESFMEAScopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXESFMEAS->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXESFMEAS->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for ESF MEAS"));
    packetUBXESFMEAS->automaticFlags.callbackPointer(); // Call the callback
    packetUBXESFMEAS->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXESFRAWcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXESFRAW->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXESFRAW->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for ESF RAW"));
    packetUBXESFRAW->automaticFlags.callbackPointer(); // Call the callback
    packetUBXESFRAW->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXESFSTATUScopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXESFSTATUS->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXESFSTATUS->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for ESF STATUS"));
    packetUBXESFSTATUS->automaticFlags.callbackPointer(); // Call the callback
    packetUBXESFSTATUS->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXHNRATTcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXHNRATT->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXHNRATT->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for HNR ATT"));
    packetUBXHNRATT->automaticFlags.callbackPointer(); // Call the callback
    packetUBXHNRATT->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXHNRINScopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXHNRINS->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXHNRINS->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for HNR INS"));
    packetUBXHNRINS->automaticFlags.callbackPointer(); // Call the callback
    packetUBXHNRINS->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  if ((packetUBXHNRPVTcopy != NULL) // If RAM has been allocated for the copy of the data
    && (packetUBXHNRPVT->automaticFlags.callbackPointer != NULL) // If the pointer to the callback has been defined
    && (packetUBXHNRPVT->automaticFlags.flags.bits.callbackCopyValid == true)) // If the copy of the data is valid
  {
    // if (_printDebug == true)
    //   _debugSerial->println(F("checkCallbacks: calling callback for HNR PVT"));
    packetUBXHNRPVT->automaticFlags.callbackPointer(); // Call the callback
    packetUBXHNRPVT->automaticFlags.flags.bits.callbackCopyValid = false; // Mark the data as stale
  }

  checkCallbacksReentrant = false;
}

// Push (e.g.) RTCM data directly to the module
// Returns true if all numDataBytes were pushed successfully
// Warning: this function does not check that the data is valid. It is the user's responsibility to ensure the data is valid before pushing.
boolean SFE_UBLOX_GPS::pushRawData(uint8_t *dataBytes, size_t numDataBytes)
{
  if (commType == COMM_TYPE_SERIAL)
  {
    // Serial: write all the bytes in one go
    size_t bytesWritten = _serialPort->write(dataBytes, numDataBytes);
    return (bytesWritten == numDataBytes);
  }
  else
  {
    // I2C: split the data up into packets of i2cTransactionSize
    size_t bytesLeftToWrite = numDataBytes;
    size_t bytesWrittenTotal = 0;

    while (bytesLeftToWrite > 0)
    {
      size_t bytesToWrite; // Limit bytesToWrite to i2cTransactionSize
      if (bytesLeftToWrite > i2cTransactionSize)
        bytesToWrite = i2cTransactionSize;
      else
        bytesToWrite = bytesLeftToWrite;

      _i2cPort->beginTransmission(_gpsI2Caddress);
      size_t bytesWritten = _i2cPort->write(dataBytes, bytesToWrite); // Write the bytes

      bytesWrittenTotal += bytesWritten; // Update the totals
      bytesLeftToWrite -= bytesToWrite;
      dataBytes += bytesToWrite; // Point to fresh data

      if (bytesLeftToWrite > 0)
      {
        if (_i2cPort->endTransmission(false) != 0) //Send a restart command. Do not release bus.
          return (false);                          //Sensor did not ACK
      }
      else
      {
        if (_i2cPort->endTransmission() != 0) //We're done. Release bus.
          return (false);                     //Sensor did not ACK
      }
    }

    return (bytesWrittenTotal == numDataBytes);
  }
}

// Support for data logging

//Set the file buffer size. This must be called _before_ .begin
void SFE_UBLOX_GPS::setFileBufferSize(uint16_t bufferSize)
{
  fileBufferSize = bufferSize;
}

// Extract numBytes of data from the file buffer. Copy it to destination.
// It is the user's responsibility to ensure destination is large enough.
// Returns the number of bytes extracted - which may be less than numBytes.
uint16_t SFE_UBLOX_GPS::extractFileBufferData(uint8_t *destination, uint16_t numBytes)
{
  // Check how many bytes are available in the buffer
  uint16_t bytesAvailable = fileBufferSpaceUsed();
  if (numBytes > bytesAvailable) // Limit numBytes if required
    numBytes = bytesAvailable;

  // Start copying at fileBufferTail. Wrap-around if required.
  uint16_t bytesBeforeWrapAround = fileBufferSize - fileBufferTail; // How much space is available 'above' Tail?
  if (bytesBeforeWrapAround > numBytes) // Will we need to wrap-around?
  {
    bytesBeforeWrapAround = numBytes; // We need to wrap-around
  }
  memcpy(destination, &ubxFileBuffer[fileBufferTail], bytesBeforeWrapAround); // Copy the data out of the buffer

  // Is there any data leftover which we need to copy from the 'bottom' of the buffer?
  uint16_t bytesLeftToCopy = numBytes - bytesBeforeWrapAround; // Calculate if there are any bytes left to copy
  if (bytesLeftToCopy > 0) // If there are bytes left to copy
  {
    memcpy(&destination[bytesBeforeWrapAround], &ubxFileBuffer[0], bytesLeftToCopy); // Copy the remaining data out of the buffer
    fileBufferTail = bytesLeftToCopy; // Update Tail. The next byte to be read will be read from here.
  }
  else
  {
    fileBufferTail += numBytes; // Only update Tail. The next byte to be read will be read from here.
  }

  return (numBytes); // Return the number of bytes extracted
}

// Returns the number of bytes available in file buffer which are waiting to be read
uint16_t SFE_UBLOX_GPS::fileBufferAvailable(void)
{
  return (fileBufferSpaceUsed());
}

// Returns the maximum number of bytes which the file buffer contained.
// Handy for checking the buffer is large enough to handle all the incoming data.
uint16_t SFE_UBLOX_GPS::getMaxFileBufferAvail(void)
{
  return (fileBufferMaxAvail);
}

// PRIVATE: Create the file buffer. Called by .begin
boolean SFE_UBLOX_GPS::createFileBuffer(void)
{
  if (fileBufferSize == 0) // Bail if the user has not called setFileBufferSize
  {
    if (_printDebug == true)
    {
      _debugSerial->println(F("createFileBuffer: fileBufferSize is zero!"));
    }
    return(false);
  }

  ubxFileBuffer = new uint8_t[fileBufferSize]; // Allocate RAM for the buffer

  if (ubxFileBuffer == NULL) // Check if the new (alloc) was successful
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
    {
      _debugSerial->println(F("createFileBuffer: RAM alloc failed!"));
    }
    return(false);
  }

  fileBufferHead = 0; // Initialize head and tail
  fileBufferTail = 0;

  return (true);
}

// PRIVATE: Check how much space is available in the buffer
uint16_t SFE_UBLOX_GPS::fileBufferSpaceAvailable(void)
{
  return (fileBufferSize - fileBufferSpaceUsed());
}

// PRIVATE: Check how much space is used in the buffer
uint16_t SFE_UBLOX_GPS::fileBufferSpaceUsed(void)
{
  if (fileBufferHead >= fileBufferTail) // Check if wrap-around has occurred
  {
    // Wrap-around has not occurred so do a simple subtraction
    return (fileBufferHead - fileBufferTail);
  }
  else
  {
    // Wrap-around has occurred so do a simple subtraction but add in the fileBufferSize
    return ((uint16_t)(((uint32_t)fileBufferHead + (uint32_t)fileBufferSize) - (uint32_t)fileBufferTail));
  }
}

// PRIVATE: Add a UBX packet to the file buffer
boolean SFE_UBLOX_GPS::storePacket(ubxPacket *msg)
{
  // First, check that the file buffer has been created
  if ((ubxFileBuffer == NULL) || (fileBufferSize == 0))
  {
    if (_printDebug == true)
    {
      _debugSerial->println(F("storePacket: file buffer not available!"));
    }
    return(false);
  }

  // Now, check if there is enough space in the buffer for all of the data
  uint16_t totalLength = msg->len + 8; // Total length. Include sync chars, class, id, length and checksum bytes
  if (totalLength > fileBufferSpaceAvailable())
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
    {
      _debugSerial->println(F("storePacket: insufficient space available! Data will be lost!"));
    }
    return(false);
  }

  //Store the two sync chars
  uint8_t sync_chars[] = {0xB5, 0x62};
  writeToFileBuffer(sync_chars, 2);

  //Store the Class & ID
  writeToFileBuffer(&msg->cls, 1);
  writeToFileBuffer(&msg->id, 1);

  //Store the length. Ensure length is little-endian
  uint8_t msg_length[2];
  msg_length[0] = msg->len & 0xFF;
  msg_length[1] = msg->len >> 8;
  writeToFileBuffer(msg_length, 2);

  //Store the payload
  writeToFileBuffer(msg->payload, msg->len);

  //Store the checksum
  writeToFileBuffer(&msg->checksumA, 1);
  writeToFileBuffer(&msg->checksumB, 1);

  return (true);
}

// PRIVATE: Add theBytes to the file buffer
boolean SFE_UBLOX_GPS::storeFileBytes(uint8_t *theBytes, uint16_t numBytes)
{
  // First, check that the file buffer has been created
  if ((ubxFileBuffer == NULL) || (fileBufferSize == 0))
  {
    if (_printDebug == true)
    {
      _debugSerial->println(F("storeFileBytes: file buffer not available!"));
    }
    return(false);
  }

  // Now, check if there is enough space in the buffer for all of the data
  if (numBytes > fileBufferSpaceAvailable())
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
    {
      _debugSerial->println(F("storeFileBytes: insufficient space available! Data will be lost!"));
    }
    return(false);
  }

  // There is room for all the data in the buffer so copy the data into the buffer
  writeToFileBuffer(theBytes, numBytes);

  return (true);
}

// PRIVATE: Write theBytes to the file buffer
void SFE_UBLOX_GPS::writeToFileBuffer(uint8_t *theBytes, uint16_t numBytes)
{
  // Start writing at fileBufferHead. Wrap-around if required.
  uint16_t bytesBeforeWrapAround = fileBufferSize - fileBufferHead; // How much space is available 'above' Head?
  if (bytesBeforeWrapAround > numBytes) // Is there enough room for all the data?
  {
    bytesBeforeWrapAround = numBytes; // There is enough room for all the data
  }
  memcpy(&ubxFileBuffer[fileBufferHead], theBytes, bytesBeforeWrapAround); // Copy the data into the buffer

  // Is there any data leftover which we need to copy to the 'bottom' of the buffer?
  uint16_t bytesLeftToCopy = numBytes - bytesBeforeWrapAround; // Calculate if there are any bytes left to copy
  if (bytesLeftToCopy > 0) // If there are bytes left to copy
  {
    memcpy(&ubxFileBuffer[0], &theBytes[bytesBeforeWrapAround], bytesLeftToCopy); // Copy the remaining data into the buffer
    fileBufferHead = bytesLeftToCopy; // Update Head. The next byte written will be written here.
  }
  else
  {
    fileBufferHead += numBytes; // Only update Head. The next byte written will be written here.
  }

  //Update fileBufferMaxAvail if required
  uint16_t bytesInBuffer = fileBufferSpaceUsed();
  if (bytesInBuffer > fileBufferMaxAvail)
    fileBufferMaxAvail = bytesInBuffer;
}

//=-=-=-=-=-=-=-= Specific commands =-=-=-=-=-=-=-==-=-=-=-=-=-=-=
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-==-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

//Loads the payloadCfg array with the current protocol bits located the UBX-CFG-PRT register for a given port
boolean SFE_UBLOX_GPS::getPortSettings(uint8_t portID, uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_PRT;
  packetCfg.len = 1;
  packetCfg.startingSpot = 0;

  payloadCfg[0] = portID;

  return ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_RECEIVED); // We are expecting data and an ACK
}

//Configure a given port to output UBX, NMEA, RTCM3 or a combination thereof
//Port 0=I2c, 1=UART1, 2=UART2, 3=USB, 4=SPI
//Bit:0 = UBX, :1=NMEA, :5=RTCM3
boolean SFE_UBLOX_GPS::setPortOutput(uint8_t portID, uint8_t outStreamSettings, uint16_t maxWait)
{
  //Get the current config values for this port ID
  if (getPortSettings(portID, maxWait) == false)
    return (false); //Something went wrong. Bail.

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_PRT;
  packetCfg.len = 20;
  packetCfg.startingSpot = 0;

  //payloadCfg is now loaded with current bytes. Change only the ones we need to
  payloadCfg[14] = outStreamSettings; //OutProtocolMask LSB - Set outStream bits

  return ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Configure a given port to input UBX, NMEA, RTCM3 or a combination thereof
//Port 0=I2c, 1=UART1, 2=UART2, 3=USB, 4=SPI
//Bit:0 = UBX, :1=NMEA, :5=RTCM3
boolean SFE_UBLOX_GPS::setPortInput(uint8_t portID, uint8_t inStreamSettings, uint16_t maxWait)
{
  //Get the current config values for this port ID
  //This will load the payloadCfg array with current port settings
  if (getPortSettings(portID, maxWait) == false)
    return (false); //Something went wrong. Bail.

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_PRT;
  packetCfg.len = 20;
  packetCfg.startingSpot = 0;

  //payloadCfg is now loaded with current bytes. Change only the ones we need to
  payloadCfg[12] = inStreamSettings; //InProtocolMask LSB - Set inStream bits

  return ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Changes the I2C address that the u-blox module responds to
//0x42 is the default but can be changed with this command
boolean SFE_UBLOX_GPS::setI2CAddress(uint8_t deviceAddress, uint16_t maxWait)
{
  //Get the current config values for the I2C port
  getPortSettings(COM_PORT_I2C, maxWait); //This will load the payloadCfg array with current port settings

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_PRT;
  packetCfg.len = 20;
  packetCfg.startingSpot = 0;

  //payloadCfg is now loaded with current bytes. Change only the ones we need to
  payloadCfg[4] = deviceAddress << 1; //DDC mode LSB

  if (sendCommand(&packetCfg, maxWait) == SFE_UBLOX_STATUS_DATA_SENT) // We are only expecting an ACK
  {
    //Success! Now change our internal global.
    _gpsI2Caddress = deviceAddress; //Store the I2C address from user
    return (true);
  }
  return (false);
}

//Changes the serial baud rate of the u-blox module, can't return success/fail 'cause ACK from modem
//is lost due to baud rate change
void SFE_UBLOX_GPS::setSerialRate(uint32_t baudrate, uint8_t uartPort, uint16_t maxWait)
{
  //Get the current config values for the UART port
  getPortSettings(uartPort, maxWait); //This will load the payloadCfg array with current port settings

  if (_printDebug == true)
  {
    _debugSerial->print(F("Current baud rate: "));
    _debugSerial->println(((uint32_t)payloadCfg[10] << 16) | ((uint32_t)payloadCfg[9] << 8) | payloadCfg[8]);
  }

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_PRT;
  packetCfg.len = 20;
  packetCfg.startingSpot = 0;

  //payloadCfg is now loaded with current bytes. Change only the ones we need to
  payloadCfg[8] = baudrate;
  payloadCfg[9] = baudrate >> 8;
  payloadCfg[10] = baudrate >> 16;
  payloadCfg[11] = baudrate >> 24;

  if (_printDebug == true)
  {
    _debugSerial->print(F("New baud rate:"));
    _debugSerial->println(((uint32_t)payloadCfg[10] << 16) | ((uint32_t)payloadCfg[9] << 8) | payloadCfg[8]);
  }

  sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);
  if (_printDebug == true)
  {
    _debugSerial->print(F("setSerialRate: sendCommand returned: "));
    _debugSerial->println(statusString(retVal));
  }
}

//Configure a port to output UBX, NMEA, RTCM3 or a combination thereof
boolean SFE_UBLOX_GPS::setI2COutput(uint8_t comSettings, uint16_t maxWait)
{
  return (setPortOutput(COM_PORT_I2C, comSettings, maxWait));
}
boolean SFE_UBLOX_GPS::setUART1Output(uint8_t comSettings, uint16_t maxWait)
{
  return (setPortOutput(COM_PORT_UART1, comSettings, maxWait));
}
boolean SFE_UBLOX_GPS::setUART2Output(uint8_t comSettings, uint16_t maxWait)
{
  return (setPortOutput(COM_PORT_UART2, comSettings, maxWait));
}
boolean SFE_UBLOX_GPS::setUSBOutput(uint8_t comSettings, uint16_t maxWait)
{
  return (setPortOutput(COM_PORT_USB, comSettings, maxWait));
}
boolean SFE_UBLOX_GPS::setSPIOutput(uint8_t comSettings, uint16_t maxWait)
{
  return (setPortOutput(COM_PORT_SPI, comSettings, maxWait));
}

//Want to see the NMEA messages on the Serial port? Here's how
void SFE_UBLOX_GPS::setNMEAOutputPort(Stream &nmeaOutputPort)
{
  _nmeaOutputPort = &nmeaOutputPort; //Store the port from user
}

// Reset to defaults

void SFE_UBLOX_GPS::factoryReset()
{
  // Copy default settings to permanent
  // Note: this does not load the permanent configuration into the current configuration. Calling factoryDefault() will do that.
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_CFG;
  packetCfg.len = 13;
  packetCfg.startingSpot = 0;
  for (uint8_t i = 0; i < 4; i++)
  {
    payloadCfg[0 + i] = 0xff; // clear mask: copy default config to permanent config
    payloadCfg[4 + i] = 0x00; // save mask: don't save current to permanent
    payloadCfg[8 + i] = 0x00; // load mask: don't copy permanent config to current
  }
  payloadCfg[12] = 0xff;      // all forms of permanent memory
  sendCommand(&packetCfg, 0); // don't expect ACK
  hardReset();                // cause factory default config to actually be loaded and used cleanly
}

void SFE_UBLOX_GPS::hardReset()
{
  // Issue hard reset
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_RST;
  packetCfg.len = 4;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = 0xff;       // cold start
  payloadCfg[1] = 0xff;       // cold start
  payloadCfg[2] = 0;          // 0=HW reset
  payloadCfg[3] = 0;          // reserved
  sendCommand(&packetCfg, 0); // don't expect ACK
}

//Reset module to factory defaults
//This still works but it is the old way of configuring ublox modules. See getVal and setVal for the new methods
boolean SFE_UBLOX_GPS::factoryDefault(uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_CFG;
  packetCfg.len = 12;
  packetCfg.startingSpot = 0;

  //Clear packet payload
  memset(payloadCfg, 0, packetCfg.len);

  packetCfg.payload[0] = 0xFF; //Set any bit in the clearMask field to clear saved config
  packetCfg.payload[1] = 0xFF;
  packetCfg.payload[8] = 0xFF; //Set any bit in the loadMask field to discard current config and rebuild from lower non-volatile memory layers
  packetCfg.payload[9] = 0xFF;

  return (sendCommand(&packetCfg, maxWait) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Save configuration to BBR / Flash

//Save current configuration to flash and BBR (battery backed RAM)
//This still works but it is the old way of configuring ublox modules. See getVal and setVal for the new methods
boolean SFE_UBLOX_GPS::saveConfiguration(uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_CFG;
  packetCfg.len = 12;
  packetCfg.startingSpot = 0;

  //Clear packet payload
  memset(payloadCfg, 0, packetCfg.len);

  packetCfg.payload[4] = 0xFF; //Set any bit in the saveMask field to save current config to Flash and BBR
  packetCfg.payload[5] = 0xFF;

  return (sendCommand(&packetCfg, maxWait) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Save the selected configuration sub-sections to flash and BBR (battery backed RAM)
//This still works but it is the old way of configuring ublox modules. See getVal and setVal for the new methods
boolean SFE_UBLOX_GPS::saveConfigSelective(uint32_t configMask, uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_CFG;
  packetCfg.len = 12;
  packetCfg.startingSpot = 0;

  //Clear packet payload
  memset(payloadCfg, 0, packetCfg.len);

  packetCfg.payload[4] = configMask & 0xFF; //Set the appropriate bits in the saveMask field to save current config to Flash and BBR
  packetCfg.payload[5] = (configMask >> 8) & 0xFF;
  packetCfg.payload[6] = (configMask >> 16) & 0xFF;
  packetCfg.payload[7] = (configMask >> 24) & 0xFF;

  return (sendCommand(&packetCfg, maxWait) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Configure a given message type for a given port (UART1, I2C, SPI, etc)
boolean SFE_UBLOX_GPS::configureMessage(uint8_t msgClass, uint8_t msgID, uint8_t portID, uint8_t sendRate, uint16_t maxWait)
{
  //Poll for the current settings for a given message
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 2;
  packetCfg.startingSpot = 0;

  payloadCfg[0] = msgClass;
  payloadCfg[1] = msgID;

  //This will load the payloadCfg array with current settings of the given register
  if (sendCommand(&packetCfg, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED) // We are expecting data and an ACK
    return (false);                                                       //If command send fails then bail

  //Now send it back with new mods
  packetCfg.len = 8;

  //payloadCfg is now loaded with current bytes. Change only the ones we need to
  payloadCfg[2 + portID] = sendRate; //Send rate is relative to the event a message is registered on. For example, if the rate of a navigation message is set to 2, the message is sent every 2nd navigation solution.

  return ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Enable a given message type, default of 1 per update rate (usually 1 per second)
boolean SFE_UBLOX_GPS::enableMessage(uint8_t msgClass, uint8_t msgID, uint8_t portID, uint8_t rate, uint16_t maxWait)
{
  return (configureMessage(msgClass, msgID, portID, rate, maxWait));
}
//Disable a given message type on a given port
boolean SFE_UBLOX_GPS::disableMessage(uint8_t msgClass, uint8_t msgID, uint8_t portID, uint16_t maxWait)
{
  return (configureMessage(msgClass, msgID, portID, 0, maxWait));
}

boolean SFE_UBLOX_GPS::enableNMEAMessage(uint8_t msgID, uint8_t portID, uint8_t rate, uint16_t maxWait)
{
  return (configureMessage(UBX_CLASS_NMEA, msgID, portID, rate, maxWait));
}
boolean SFE_UBLOX_GPS::disableNMEAMessage(uint8_t msgID, uint8_t portID, uint16_t maxWait)
{
  return (enableNMEAMessage(msgID, portID, 0, maxWait));
}

//Given a message number turns on a message ID for output over a given portID (UART, I2C, SPI, USB, etc)
//To disable a message, set secondsBetween messages to 0
//Note: This function will return false if the message is already enabled
//For base station RTK output we need to enable various sentences

//NEO-M8P has four:
//1005 = 0xF5 0x05 - Stationary RTK reference ARP
//1077 = 0xF5 0x4D - GPS MSM7
//1087 = 0xF5 0x57 - GLONASS MSM7
//1230 = 0xF5 0xE6 - GLONASS code-phase biases, set to once every 10 seconds

//ZED-F9P has six:
//1005, 1074, 1084, 1094, 1124, 1230

//Much of this configuration is not documented and instead discerned from u-center binary console
boolean SFE_UBLOX_GPS::enableRTCMmessage(uint8_t messageNumber, uint8_t portID, uint8_t sendRate, uint16_t maxWait)
{
  return (configureMessage(UBX_RTCM_MSB, messageNumber, portID, sendRate, maxWait));
}

//Disable a given message on a given port by setting secondsBetweenMessages to zero
boolean SFE_UBLOX_GPS::disableRTCMmessage(uint8_t messageNumber, uint8_t portID, uint16_t maxWait)
{
  return (enableRTCMmessage(messageNumber, portID, 0, maxWait));
}

//Functions used for RTK and base station setup

//Get the current TimeMode3 settings - these contain survey in statuses
boolean SFE_UBLOX_GPS::getSurveyMode(uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_TMODE3;
  packetCfg.len = 0;
  packetCfg.startingSpot = 0;

  return ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_RECEIVED); // We are expecting data and an ACK
}

//Control Survey-In for NEO-M8P
boolean SFE_UBLOX_GPS::setSurveyMode(uint8_t mode, uint16_t observationTime, float requiredAccuracy, uint16_t maxWait)
{
  if (getSurveyMode(maxWait) == false) //Ask module for the current TimeMode3 settings. Loads into payloadCfg.
    return (false);

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_TMODE3;
  packetCfg.len = 40;
  packetCfg.startingSpot = 0;

  //payloadCfg should be loaded with poll response. Now modify only the bits we care about
  payloadCfg[2] = mode; //Set mode. Survey-In and Disabled are most common. Use ECEF (not LAT/LON/ALT).

  //svinMinDur is U4 (uint32_t) but we'll only use a uint16_t (waiting more than 65535 seconds seems excessive!)
  payloadCfg[24] = observationTime & 0xFF; //svinMinDur in seconds
  payloadCfg[25] = observationTime >> 8;   //svinMinDur in seconds
  payloadCfg[26] = 0;                      //Truncate to 16 bits
  payloadCfg[27] = 0;                      //Truncate to 16 bits

  //svinAccLimit is U4 (uint32_t) in 0.1mm.
  uint32_t svinAccLimit = (uint32_t)(requiredAccuracy * 10000.0); //Convert m to 0.1mm
  payloadCfg[28] = svinAccLimit & 0xFF;                           //svinAccLimit in 0.1mm increments
  payloadCfg[29] = svinAccLimit >> 8;
  payloadCfg[30] = svinAccLimit >> 16;
  payloadCfg[31] = svinAccLimit >> 24;

  return ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Begin Survey-In for NEO-M8P
boolean SFE_UBLOX_GPS::enableSurveyMode(uint16_t observationTime, float requiredAccuracy, uint16_t maxWait)
{
  return (setSurveyMode(SVIN_MODE_ENABLE, observationTime, requiredAccuracy, maxWait));
}

//Stop Survey-In for NEO-M8P
boolean SFE_UBLOX_GPS::disableSurveyMode(uint16_t maxWait)
{
  return (setSurveyMode(SVIN_MODE_DISABLE, 0, 0, maxWait));
}

//Set the ECEF or Lat/Long coordinates of a receiver
//This imediately puts the receiver in TIME mode (fixed) and will begin outputting RTCM sentences if enabled
//This is helpful once an antenna's position has been established. See this tutorial: https://learn.sparkfun.com/tutorials/how-to-build-a-diy-gnss-reference-station#gather-raw-gnss-data
// For ECEF the units are: cm, 0.1mm, cm, 0.1mm, cm, 0.1mm
// For Lat/Lon/Alt the units are: degrees^-7, degrees^-9, degrees^-7, degrees^-9, cm, 0.1mm
bool SFE_UBLOX_GPS::setStaticPosition(int32_t ecefXOrLat, int8_t ecefXOrLatHP, int32_t ecefYOrLon, int8_t ecefYOrLonHP, int32_t ecefZOrAlt, int8_t ecefZOrAltHP, bool latLong, uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_TMODE3;
  packetCfg.len = 0;
  packetCfg.startingSpot = 0;

  //Ask module for the current TimeMode3 settings. Loads into payloadCfg.
  if (sendCommand(&packetCfg, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED)
    return (false);

  packetCfg.len = 40;

  //customCfg should be loaded with poll response. Now modify only the bits we care about
  payloadCfg[2] = 2; //Set mode to fixed. Use ECEF (not LAT/LON/ALT).

  if (latLong == true)
    payloadCfg[3] = (uint8_t)(1 << 0); //Set mode to fixed. Use LAT/LON/ALT.

  //Set ECEF X or Lat
  payloadCfg[4] = (ecefXOrLat >> 8 * 0) & 0xFF; //LSB
  payloadCfg[5] = (ecefXOrLat >> 8 * 1) & 0xFF;
  payloadCfg[6] = (ecefXOrLat >> 8 * 2) & 0xFF;
  payloadCfg[7] = (ecefXOrLat >> 8 * 3) & 0xFF; //MSB

  //Set ECEF Y or Long
  payloadCfg[8] = (ecefYOrLon >> 8 * 0) & 0xFF; //LSB
  payloadCfg[9] = (ecefYOrLon >> 8 * 1) & 0xFF;
  payloadCfg[10] = (ecefYOrLon >> 8 * 2) & 0xFF;
  payloadCfg[11] = (ecefYOrLon >> 8 * 3) & 0xFF; //MSB

  //Set ECEF Z or Altitude
  payloadCfg[12] = (ecefZOrAlt >> 8 * 0) & 0xFF; //LSB
  payloadCfg[13] = (ecefZOrAlt >> 8 * 1) & 0xFF;
  payloadCfg[14] = (ecefZOrAlt >> 8 * 2) & 0xFF;
  payloadCfg[15] = (ecefZOrAlt >> 8 * 3) & 0xFF; //MSB

  //Set high precision parts
  payloadCfg[16] = ecefXOrLatHP;
  payloadCfg[17] = ecefYOrLonHP;
  payloadCfg[18] = ecefZOrAltHP;

  return ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

bool SFE_UBLOX_GPS::setStaticPosition(int32_t ecefXOrLat, int32_t ecefYOrLon, int32_t ecefZOrAlt, bool latlong, uint16_t maxWait)
{
  return (setStaticPosition(ecefXOrLat, 0, ecefYOrLon, 0, ecefZOrAlt, 0, latlong, maxWait));
}

// Module Protocol Version

//Get the current protocol version of the u-blox module we're communicating with
//This is helpful when deciding if we should call the high-precision Lat/Long (HPPOSLLH) or the regular (POSLLH)
uint8_t SFE_UBLOX_GPS::getProtocolVersionHigh(uint16_t maxWait)
{
  if (moduleSWVersion == NULL) initModuleSWVersion(); //Check that RAM has been allocated for the SW version
  if (moduleSWVersion == NULL) //Bail if the RAM allocation failed
    return (false);

  if (moduleSWVersion->moduleQueried == false)
    getProtocolVersion(maxWait);
  return (moduleSWVersion->versionHigh);
}

//Get the current protocol version of the u-blox module we're communicating with
//This is helpful when deciding if we should call the high-precision Lat/Long (HPPOSLLH) or the regular (POSLLH)
uint8_t SFE_UBLOX_GPS::getProtocolVersionLow(uint16_t maxWait)
{
  if (moduleSWVersion == NULL) initModuleSWVersion(); //Check that RAM has been allocated for the SW version
  if (moduleSWVersion == NULL) //Bail if the RAM allocation failed
    return (false);

  if (moduleSWVersion->moduleQueried == false)
    getProtocolVersion(maxWait);
  return (moduleSWVersion->versionLow);
}

//Get the current protocol version of the u-blox module we're communicating with
//This is helpful when deciding if we should call the high-precision Lat/Long (HPPOSLLH) or the regular (POSLLH)
boolean SFE_UBLOX_GPS::getProtocolVersion(uint16_t maxWait)
{
  if (moduleSWVersion == NULL) initModuleSWVersion(); //Check that RAM has been allocated for the SW version
  if (moduleSWVersion == NULL) //Bail if the RAM allocation failed
    return (false);

  //Send packet with only CLS and ID, length of zero. This will cause the module to respond with the contents of that CLS/ID.
  packetCfg.cls = UBX_CLASS_MON;
  packetCfg.id = UBX_MON_VER;

  packetCfg.len = 0;
  packetCfg.startingSpot = 40; //Start at first "extended software information" string

  if (sendCommand(&packetCfg, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED) // We are only expecting data (no ACK)
    return (false);                                                       //If command send fails then bail

  //Payload should now contain ~220 characters (depends on module type)

  // if (_printDebug == true)
  // {
  //   _debugSerial->print(F("MON VER Payload:"));
  //   for (int location = 0; location < packetCfg.len; location++)
  //   {
  //     if (location % 30 == 0)
  //       _debugSerial->println();
  //     _debugSerial->write(payloadCfg[location]);
  //   }
  //   _debugSerial->println();
  // }

  //We will step through the payload looking at each extension field of 30 bytes
  for (uint8_t extensionNumber = 0; extensionNumber < 10; extensionNumber++)
  {
    //Now we need to find "PROTVER=18.00" in the incoming byte stream
    if ((payloadCfg[(30 * extensionNumber) + 0] == 'P') && (payloadCfg[(30 * extensionNumber) + 6] == 'R'))
    {
      moduleSWVersion->versionHigh = (payloadCfg[(30 * extensionNumber) + 8] - '0') * 10 + (payloadCfg[(30 * extensionNumber) + 9] - '0');  //Convert '18' to 18
      moduleSWVersion->versionLow = (payloadCfg[(30 * extensionNumber) + 11] - '0') * 10 + (payloadCfg[(30 * extensionNumber) + 12] - '0'); //Convert '00' to 00
      moduleSWVersion->moduleQueried = true; // Mark this data as new

      if (_printDebug == true)
      {
        _debugSerial->print(F("Protocol version: "));
        _debugSerial->print(moduleSWVersion->versionHigh);
        _debugSerial->print(F("."));
        _debugSerial->println(moduleSWVersion->versionLow);
      }
      return (true); //Success!
    }
  }

  return (false); //We failed
}

// PRIVATE: Allocate RAM for moduleSWVersion and initialize it
boolean SFE_UBLOX_GPS::initModuleSWVersion()
{
  moduleSWVersion = new moduleSWVersion_t; //Allocate RAM for the main struct
  if (moduleSWVersion == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initModuleSWVersion: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  moduleSWVersion->moduleQueried = false;
  return (true);
}

// Geofences

//Add a new geofence using UBX-CFG-GEOFENCE
boolean SFE_UBLOX_GPS::addGeofence(int32_t latitude, int32_t longitude, uint32_t radius, byte confidence, byte pinPolarity, byte pin, uint16_t maxWait)
{
  if (currentGeofenceParams == NULL) initGeofenceParams(); // Check if RAM has been allocated for currentGeofenceParams
  if (currentGeofenceParams == NULL) // Abort if the RAM allocation failed
    return (false);

  if (currentGeofenceParams->numFences >= 4)
    return (false); // Quit if we already have four geofences defined

  // Store the new geofence parameters
  currentGeofenceParams->lats[currentGeofenceParams->numFences] = latitude;
  currentGeofenceParams->longs[currentGeofenceParams->numFences] = longitude;
  currentGeofenceParams->rads[currentGeofenceParams->numFences] = radius;
  currentGeofenceParams->numFences = currentGeofenceParams->numFences + 1; // Increment the number of fences

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_GEOFENCE;
  packetCfg.len = (currentGeofenceParams->numFences * 12) + 8;
  packetCfg.startingSpot = 0;

  payloadCfg[0] = 0;                               // Message version = 0x00
  payloadCfg[1] = currentGeofenceParams->numFences; // numFences
  payloadCfg[2] = confidence;                      // confLvl = Confidence level 0-4 (none, 68%, 95%, 99.7%, 99.99%)
  payloadCfg[3] = 0;                               // reserved1
  if (pin > 0)
  {
    payloadCfg[4] = 1; // enable PIO combined fence state
  }
  else
  {
    payloadCfg[4] = 0; // disable PIO combined fence state
  }
  payloadCfg[5] = pinPolarity; // PIO pin polarity (0 = low means inside, 1 = low means outside (or unknown))
  payloadCfg[6] = pin;         // PIO pin
  payloadCfg[7] = 0;           //reserved2
  payloadCfg[8] = currentGeofenceParams->lats[0] & 0xFF;
  payloadCfg[9] = currentGeofenceParams->lats[0] >> 8;
  payloadCfg[10] = currentGeofenceParams->lats[0] >> 16;
  payloadCfg[11] = currentGeofenceParams->lats[0] >> 24;
  payloadCfg[12] = currentGeofenceParams->longs[0] & 0xFF;
  payloadCfg[13] = currentGeofenceParams->longs[0] >> 8;
  payloadCfg[14] = currentGeofenceParams->longs[0] >> 16;
  payloadCfg[15] = currentGeofenceParams->longs[0] >> 24;
  payloadCfg[16] = currentGeofenceParams->rads[0] & 0xFF;
  payloadCfg[17] = currentGeofenceParams->rads[0] >> 8;
  payloadCfg[18] = currentGeofenceParams->rads[0] >> 16;
  payloadCfg[19] = currentGeofenceParams->rads[0] >> 24;
  if (currentGeofenceParams->numFences >= 2)
  {
    payloadCfg[20] = currentGeofenceParams->lats[1] & 0xFF;
    payloadCfg[21] = currentGeofenceParams->lats[1] >> 8;
    payloadCfg[22] = currentGeofenceParams->lats[1] >> 16;
    payloadCfg[23] = currentGeofenceParams->lats[1] >> 24;
    payloadCfg[24] = currentGeofenceParams->longs[1] & 0xFF;
    payloadCfg[25] = currentGeofenceParams->longs[1] >> 8;
    payloadCfg[26] = currentGeofenceParams->longs[1] >> 16;
    payloadCfg[27] = currentGeofenceParams->longs[1] >> 24;
    payloadCfg[28] = currentGeofenceParams->rads[1] & 0xFF;
    payloadCfg[29] = currentGeofenceParams->rads[1] >> 8;
    payloadCfg[30] = currentGeofenceParams->rads[1] >> 16;
    payloadCfg[31] = currentGeofenceParams->rads[1] >> 24;
  }
  if (currentGeofenceParams->numFences >= 3)
  {
    payloadCfg[32] = currentGeofenceParams->lats[2] & 0xFF;
    payloadCfg[33] = currentGeofenceParams->lats[2] >> 8;
    payloadCfg[34] = currentGeofenceParams->lats[2] >> 16;
    payloadCfg[35] = currentGeofenceParams->lats[2] >> 24;
    payloadCfg[36] = currentGeofenceParams->longs[2] & 0xFF;
    payloadCfg[37] = currentGeofenceParams->longs[2] >> 8;
    payloadCfg[38] = currentGeofenceParams->longs[2] >> 16;
    payloadCfg[39] = currentGeofenceParams->longs[2] >> 24;
    payloadCfg[40] = currentGeofenceParams->rads[2] & 0xFF;
    payloadCfg[41] = currentGeofenceParams->rads[2] >> 8;
    payloadCfg[42] = currentGeofenceParams->rads[2] >> 16;
    payloadCfg[43] = currentGeofenceParams->rads[2] >> 24;
  }
  if (currentGeofenceParams->numFences >= 4)
  {
    payloadCfg[44] = currentGeofenceParams->lats[3] & 0xFF;
    payloadCfg[45] = currentGeofenceParams->lats[3] >> 8;
    payloadCfg[46] = currentGeofenceParams->lats[3] >> 16;
    payloadCfg[47] = currentGeofenceParams->lats[3] >> 24;
    payloadCfg[48] = currentGeofenceParams->longs[3] & 0xFF;
    payloadCfg[49] = currentGeofenceParams->longs[3] >> 8;
    payloadCfg[50] = currentGeofenceParams->longs[3] >> 16;
    payloadCfg[51] = currentGeofenceParams->longs[3] >> 24;
    payloadCfg[52] = currentGeofenceParams->rads[3] & 0xFF;
    payloadCfg[53] = currentGeofenceParams->rads[3] >> 8;
    payloadCfg[54] = currentGeofenceParams->rads[3] >> 16;
    payloadCfg[55] = currentGeofenceParams->rads[3] >> 24;
  }
  return ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Clear all geofences using UBX-CFG-GEOFENCE
boolean SFE_UBLOX_GPS::clearGeofences(uint16_t maxWait)
{
  if (currentGeofenceParams == NULL) initGeofenceParams(); // Check if RAM has been allocated for currentGeofenceParams
  if (currentGeofenceParams == NULL) // Abort if the RAM allocation failed
    return (false);

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_GEOFENCE;
  packetCfg.len = 8;
  packetCfg.startingSpot = 0;

  payloadCfg[0] = 0; // Message version = 0x00
  payloadCfg[1] = 0; // numFences
  payloadCfg[2] = 0; // confLvl
  payloadCfg[3] = 0; // reserved1
  payloadCfg[4] = 0; // disable PIO combined fence state
  payloadCfg[5] = 0; // PIO pin polarity (0 = low means inside, 1 = low means outside (or unknown))
  payloadCfg[6] = 0; // PIO pin
  payloadCfg[7] = 0; //reserved2

  currentGeofenceParams->numFences = 0; // Zero the number of geofences currently in use

  return ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Clear the antenna control settings using UBX-CFG-ANT
//This function is hopefully redundant but may be needed to release
//any PIO pins pre-allocated for antenna functions
boolean SFE_UBLOX_GPS::clearAntPIO(uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_ANT;
  packetCfg.len = 4;
  packetCfg.startingSpot = 0;

  payloadCfg[0] = 0x10; // Antenna flag mask: set the recovery bit
  payloadCfg[1] = 0;
  payloadCfg[2] = 0xFF; // Antenna pin configuration: set pinSwitch and pinSCD to 31
  payloadCfg[3] = 0xFF; // Antenna pin configuration: set pinOCD to 31, set reconfig bit

  return ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Returns the combined geofence state using UBX-NAV-GEOFENCE
boolean SFE_UBLOX_GPS::getGeofenceState(geofenceState &currentGeofenceState, uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_NAV;
  packetCfg.id = UBX_NAV_GEOFENCE;
  packetCfg.len = 0;
  packetCfg.startingSpot = 0;

  //Ask module for the geofence status. Loads into payloadCfg.
  if (sendCommand(&packetCfg, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED) // We are expecting data and an ACK
    return (false);

  currentGeofenceState.status = payloadCfg[5];    // Extract the status
  currentGeofenceState.numFences = payloadCfg[6]; // Extract the number of geofences
  currentGeofenceState.combState = payloadCfg[7]; // Extract the combined state of all geofences
  if (currentGeofenceState.numFences > 0)
    currentGeofenceState.states[0] = payloadCfg[8]; // Extract geofence 1 state
  if (currentGeofenceState.numFences > 1)
    currentGeofenceState.states[1] = payloadCfg[10]; // Extract geofence 2 state
  if (currentGeofenceState.numFences > 2)
    currentGeofenceState.states[2] = payloadCfg[12]; // Extract geofence 3 state
  if (currentGeofenceState.numFences > 3)
    currentGeofenceState.states[3] = payloadCfg[14]; // Extract geofence 4 state

  return (true);
}

// PRIVATE: Allocate RAM for currentGeofenceParams and initialize it
boolean SFE_UBLOX_GPS::initGeofenceParams()
{
  currentGeofenceParams = new geofenceParams_t; //Allocate RAM for the main struct
  if (currentGeofenceParams == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initGeofenceParams: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  currentGeofenceParams->numFences = 0;
  return (true);
}

//Power Save Mode
//Enables/Disables Low Power Mode using UBX-CFG-RXM
boolean SFE_UBLOX_GPS::powerSaveMode(bool power_save, uint16_t maxWait)
{
  // Let's begin by checking the Protocol Version as UBX_CFG_RXM is not supported on the ZED (protocol >= 27)
  uint8_t protVer = getProtocolVersionHigh(maxWait);
  /*
  if (_printDebug == true)
  {
    _debugSerial->print(F("Protocol version is "));
    _debugSerial->println(protVer);
  }
  */
  if (protVer >= 27)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
    {
      _debugSerial->println(F("powerSaveMode (UBX-CFG-RXM) is not supported by this protocol version"));
    }
    return (false);
  }

  // Now let's change the power setting using UBX-CFG-RXM
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_RXM;
  packetCfg.len = 0;
  packetCfg.startingSpot = 0;

  //Ask module for the current power management settings. Loads into payloadCfg.
  if (sendCommand(&packetCfg, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED) // We are expecting data and an ACK
    return (false);

  if (power_save)
  {
    payloadCfg[1] = 1; // Power Save Mode
  }
  else
  {
    payloadCfg[1] = 0; // Continuous Mode
  }

  packetCfg.len = 2;
  packetCfg.startingSpot = 0;

  return (sendCommand(&packetCfg, maxWait) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

// Get Power Save Mode
// Returns the current Low Power Mode using UBX-CFG-RXM
// Returns 255 if the sendCommand fails
uint8_t SFE_UBLOX_GPS::getPowerSaveMode(uint16_t maxWait)
{
  // Let's begin by checking the Protocol Version as UBX_CFG_RXM is not supported on the ZED (protocol >= 27)
  uint8_t protVer = getProtocolVersionHigh(maxWait);
  /*
  if (_printDebug == true)
  {
    _debugSerial->print(F("Protocol version is "));
    _debugSerial->println(protVer);
  }
  */
  if (protVer >= 27)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
    {
      _debugSerial->println(F("powerSaveMode (UBX-CFG-RXM) is not supported by this protocol version"));
    }
    return (255);
  }

  // Now let's read the power setting using UBX-CFG-RXM
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_RXM;
  packetCfg.len = 0;
  packetCfg.startingSpot = 0;

  //Ask module for the current power management settings. Loads into payloadCfg.
  if (sendCommand(&packetCfg, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED) // We are expecting data and an ACK
    return (255);

  return (payloadCfg[1]); // Return the low power mode
}

// Powers off the GPS device for a given duration to reduce power consumption.
// NOTE: Querying the device before the duration is complete, for example by "getLatitude()" will wake it up!
// Returns true if command has not been not acknowledged.
// Returns false if command has not been acknowledged or maxWait = 0.
boolean SFE_UBLOX_GPS::powerOff(uint32_t durationInMs, uint16_t maxWait)
{
  // use durationInMs = 0 for infinite duration
  if (_printDebug == true)
  {
    _debugSerial->print(F("Powering off for "));
    _debugSerial->print(durationInMs);
    _debugSerial->println(" ms");
  }

  // Power off device using UBX-RXM-PMREQ
  packetCfg.cls = UBX_CLASS_RXM; // 0x02
  packetCfg.id = UBX_RXM_PMREQ;  // 0x41
  packetCfg.len = 8;
  packetCfg.startingSpot = 0;

  // duration
  // big endian to little endian, switch byte order
  payloadCfg[0] = (durationInMs >> (8 * 0)) & 0xff;
  payloadCfg[1] = (durationInMs >> (8 * 1)) & 0xff;
  payloadCfg[2] = (durationInMs >> (8 * 2)) & 0xff;
  payloadCfg[3] = (durationInMs >> (8 * 3)) & 0xff;

  payloadCfg[4] = 0x02; //Flags : set the backup bit
  payloadCfg[5] = 0x00; //Flags
  payloadCfg[6] = 0x00; //Flags
  payloadCfg[7] = 0x00; //Flags

  if (maxWait != 0)
  {
    // check for "not acknowledged" command
    return (sendCommand(&packetCfg, maxWait) != SFE_UBLOX_STATUS_COMMAND_NACK);
  }
  else
  {
    sendCommand(&packetCfg, maxWait);
    return false; // can't tell if command not acknowledged if maxWait = 0
  }
}

// Powers off the GPS device for a given duration to reduce power consumption.
// While powered off it can be woken up by creating a falling or rising voltage edge on the specified pin.
// NOTE: The GPS seems to be sensitve to signals on the pins while powered off. Works best when Microcontroller is in deepsleep.
// NOTE: Querying the device before the duration is complete, for example by "getLatitude()" will wake it up!
// Returns true if command has not been not acknowledged.
// Returns false if command has not been acknowledged or maxWait = 0.
boolean SFE_UBLOX_GPS::powerOffWithInterrupt(uint32_t durationInMs, uint32_t wakeupSources, boolean forceWhileUsb, uint16_t maxWait)
{
  // use durationInMs = 0 for infinite duration
  if (_printDebug == true)
  {
    _debugSerial->print(F("Powering off for "));
    _debugSerial->print(durationInMs);
    _debugSerial->println(" ms");
  }

  // Power off device using UBX-RXM-PMREQ
  packetCfg.cls = UBX_CLASS_RXM; // 0x02
  packetCfg.id = UBX_RXM_PMREQ;  // 0x41
  packetCfg.len = 16;
  packetCfg.startingSpot = 0;

  payloadCfg[0] = 0x00; // message version

  // bytes 1-3 are reserved - and must be set to zero
  payloadCfg[1] = 0x00;
  payloadCfg[2] = 0x00;
  payloadCfg[3] = 0x00;

  // duration
  // big endian to little endian, switch byte order
  payloadCfg[4] = (durationInMs >> (8 * 0)) & 0xff;
  payloadCfg[5] = (durationInMs >> (8 * 1)) & 0xff;
  payloadCfg[6] = (durationInMs >> (8 * 2)) & 0xff;
  payloadCfg[7] = (durationInMs >> (8 * 3)) & 0xff;

  // flags

  // disables USB interface when powering off, defaults to true
  if (forceWhileUsb)
  {
    payloadCfg[8] = 0x06; // force | backup
  }
  else
  {
    payloadCfg[8] = 0x02; // backup only (leave the force bit clear - module will stay on if USB is connected)
  }

  payloadCfg[9] = 0x00;
  payloadCfg[10] = 0x00;
  payloadCfg[11] = 0x00;

  // wakeUpSources

  // wakeupPin mapping, defaults to VAL_RXM_PMREQ_WAKEUPSOURCE_EXTINT0

  // Possible values are:
  // VAL_RXM_PMREQ_WAKEUPSOURCE_UARTRX
  // VAL_RXM_PMREQ_WAKEUPSOURCE_EXTINT0
  // VAL_RXM_PMREQ_WAKEUPSOURCE_EXTINT1
  // VAL_RXM_PMREQ_WAKEUPSOURCE_SPICS

  payloadCfg[12] = (wakeupSources >> (8 * 0)) & 0xff;
  payloadCfg[13] = (wakeupSources >> (8 * 1)) & 0xff;
  payloadCfg[14] = (wakeupSources >> (8 * 2)) & 0xff;
  payloadCfg[15] = (wakeupSources >> (8 * 3)) & 0xff;

  if (maxWait != 0)
  {
    // check for "not acknowledged" command
    return (sendCommand(&packetCfg, maxWait) != SFE_UBLOX_STATUS_COMMAND_NACK);
  }
  else
  {
    sendCommand(&packetCfg, maxWait);
    return false; // can't tell if command not acknowledged if maxWait = 0
  }
}

//Dynamic Platform Model

//Change the dynamic platform model using UBX-CFG-NAV5
//Possible values are:
//PORTABLE,STATIONARY,PEDESTRIAN,AUTOMOTIVE,SEA,
//AIRBORNE1g,AIRBORNE2g,AIRBORNE4g,WRIST,BIKE
//WRIST is not supported in protocol versions less than 18
//BIKE is supported in protocol versions 19.2
boolean SFE_UBLOX_GPS::setDynamicModel(dynModel newDynamicModel, uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_NAV5;
  packetCfg.len = 0;
  packetCfg.startingSpot = 0;

  //Ask module for the current navigation model settings. Loads into payloadCfg.
  if (sendCommand(&packetCfg, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED) // We are expecting data and an ACK
    return (false);

  payloadCfg[0] = 0x01;            // mask: set only the dyn bit (0)
  payloadCfg[1] = 0x00;            // mask
  payloadCfg[2] = newDynamicModel; // dynModel

  packetCfg.len = 36;
  packetCfg.startingSpot = 0;

  return (sendCommand(&packetCfg, maxWait) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Get the dynamic platform model using UBX-CFG-NAV5
//Returns 255 if the sendCommand fails
uint8_t SFE_UBLOX_GPS::getDynamicModel(uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_NAV5;
  packetCfg.len = 0;
  packetCfg.startingSpot = 0;

  //Ask module for the current navigation model settings. Loads into payloadCfg.
  if (sendCommand(&packetCfg, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED) // We are expecting data and an ACK
    return (255);

  return (payloadCfg[2]); // Return the dynamic model
}

//Reset the odometer
boolean SFE_UBLOX_GPS::resetOdometer(uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_NAV;
  packetCfg.id = UBX_NAV_RESETODO;
  packetCfg.len = 0;
  packetCfg.startingSpot = 0;

  return (sendCommand(&packetCfg, maxWait) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

// CONFIGURATION INTERFACE (protocol v27 and above)

//Form 32-bit key from group/id/size
uint32_t SFE_UBLOX_GPS::createKey(uint16_t group, uint16_t id, uint8_t size)
{
  uint32_t key = 0;
  key |= (uint32_t)id;
  key |= (uint32_t)group << 16;
  key |= (uint32_t)size << 28;
  return (key);
}

//Given a key, load the payload with data that can then be extracted to 8, 16, or 32 bits
//This function takes a full 32-bit key
//Default layer is RAM
//Configuration of modern u-blox modules is now done via getVal/setVal/delVal, ie protocol v27 and above found on ZED-F9P
sfe_ublox_status_e SFE_UBLOX_GPS::getVal(uint32_t key, uint8_t layer, uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_VALGET;
  packetCfg.len = 4 + 4 * 1; //While multiple keys are allowed, we will send only one key at a time
  packetCfg.startingSpot = 0;

  //Clear packet payload
  memset(payloadCfg, 0, packetCfg.len);

  //VALGET uses different memory layer definitions to VALSET
  //because it can only return the value for one layer.
  //So we need to fiddle the layer here.
  //And just to complicate things further, the ZED-F9P only responds
  //correctly to layer 0 (RAM) and layer 7 (Default)!
  uint8_t getLayer = 7;                         // 7 is the "Default Layer"
  if ((layer & VAL_LAYER_RAM) == VAL_LAYER_RAM) // Did the user request the RAM layer?
  {
    getLayer = 0; // Layer 0 is RAM
  }

  payloadCfg[0] = 0;        //Message Version - set to 0
  payloadCfg[1] = getLayer; //Layer

  //Load key into outgoing payload
  payloadCfg[4] = key >> 8 * 0; //Key LSB
  payloadCfg[5] = key >> 8 * 1;
  payloadCfg[6] = key >> 8 * 2;
  payloadCfg[7] = key >> 8 * 3;

  if (_printDebug == true)
  {
    _debugSerial->print(F("key: 0x"));
    _debugSerial->print(key, HEX);
    _debugSerial->println();
  }

  //Send VALGET command with this key

  sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);
  if (_printDebug == true)
  {
    _debugSerial->print(F("getVal: sendCommand returned: "));
    _debugSerial->println(statusString(retVal));
  }

  //Verify the response is the correct length as compared to what the user called (did the module respond with 8-bits but the user called getVal32?)
  //Response is 8 bytes plus cfg data
  //if(packet->len > 8+1)

  //The response is now sitting in payload, ready for extraction
  return (retVal);
}

//Given a key, return its value
//This function takes a full 32-bit key
//Default layer is RAM
//Configuration of modern u-blox modules is now done via getVal/setVal/delVal, ie protocol v27 and above found on ZED-F9P
uint8_t SFE_UBLOX_GPS::getVal8(uint32_t key, uint8_t layer, uint16_t maxWait)
{
  if (getVal(key, layer, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED)
    return (0);

  return (extractByte(&packetCfg, 8));
}
uint16_t SFE_UBLOX_GPS::getVal16(uint32_t key, uint8_t layer, uint16_t maxWait)
{
  if (getVal(key, layer, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED)
    return (0);

  return (extractInt(&packetCfg, 8));
}
uint32_t SFE_UBLOX_GPS::getVal32(uint32_t key, uint8_t layer, uint16_t maxWait)
{
  if (getVal(key, layer, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED)
    return (0);

  return (extractLong(&packetCfg, 8));
}

//Given a group, ID and size, return the value of this config spot
//The 32-bit key is put together from group/ID/size. See other getVal to send key directly.
//Configuration of modern u-blox modules is now done via getVal/setVal/delVal, ie protocol v27 and above found on ZED-F9P
uint8_t SFE_UBLOX_GPS::getVal8(uint16_t group, uint16_t id, uint8_t size, uint8_t layer, uint16_t maxWait)
{
  uint32_t key = createKey(group, id, size);
  return getVal8(key, layer, maxWait);
}
uint16_t SFE_UBLOX_GPS::getVal16(uint16_t group, uint16_t id, uint8_t size, uint8_t layer, uint16_t maxWait)
{
  uint32_t key = createKey(group, id, size);
  return getVal16(key, layer, maxWait);
}
uint32_t SFE_UBLOX_GPS::getVal32(uint16_t group, uint16_t id, uint8_t size, uint8_t layer, uint16_t maxWait)
{
  uint32_t key = createKey(group, id, size);
  return getVal32(key, layer, maxWait);
}

//Given a key, set a 16-bit value
//This function takes a full 32-bit key
//Default layer is all: RAM+BBR+Flash
//Configuration of modern u-blox modules is now done via getVal/setVal/delVal, ie protocol v27 and above found on ZED-F9P
uint8_t SFE_UBLOX_GPS::setVal(uint32_t key, uint16_t value, uint8_t layer, uint16_t maxWait)
{
  return setVal16(key, value, layer, maxWait);
}

//Given a key, set a 16-bit value
//This function takes a full 32-bit key
//Default layer is all: RAM+BBR+Flash
//Configuration of modern u-blox modules is now done via getVal/setVal/delVal, ie protocol v27 and above found on ZED-F9P
uint8_t SFE_UBLOX_GPS::setVal16(uint32_t key, uint16_t value, uint8_t layer, uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_VALSET;
  packetCfg.len = 4 + 4 + 2; //4 byte header, 4 byte key ID, 2 bytes of value
  packetCfg.startingSpot = 0;

  //Clear packet payload
  memset(payloadCfg, 0, packetCfg.len);

  payloadCfg[0] = 0;     //Message Version - set to 0
  payloadCfg[1] = layer; //By default we ask for the BBR layer

  //Load key into outgoing payload
  payloadCfg[4] = key >> 8 * 0; //Key LSB
  payloadCfg[5] = key >> 8 * 1;
  payloadCfg[6] = key >> 8 * 2;
  payloadCfg[7] = key >> 8 * 3;

  //Load user's value
  payloadCfg[8] = value >> 8 * 0; //Value LSB
  payloadCfg[9] = value >> 8 * 1;

  //Send VALSET command with this key and value
  return (sendCommand(&packetCfg, maxWait) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Given a key, set an 8-bit value
//This function takes a full 32-bit key
//Default layer is all: RAM+BBR+Flash
//Configuration of modern u-blox modules is now done via getVal/setVal/delVal, ie protocol v27 and above found on ZED-F9P
uint8_t SFE_UBLOX_GPS::setVal8(uint32_t key, uint8_t value, uint8_t layer, uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_VALSET;
  packetCfg.len = 4 + 4 + 1; //4 byte header, 4 byte key ID, 1 byte value
  packetCfg.startingSpot = 0;

  //Clear packet payload
  memset(payloadCfg, 0, packetCfg.len);

  payloadCfg[0] = 0;     //Message Version - set to 0
  payloadCfg[1] = layer; //By default we ask for the BBR layer

  //Load key into outgoing payload
  payloadCfg[4] = key >> 8 * 0; //Key LSB
  payloadCfg[5] = key >> 8 * 1;
  payloadCfg[6] = key >> 8 * 2;
  payloadCfg[7] = key >> 8 * 3;

  //Load user's value
  payloadCfg[8] = value; //Value

  //Send VALSET command with this key and value
  return (sendCommand(&packetCfg, maxWait) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Given a key, set a 32-bit value
//This function takes a full 32-bit key
//Default layer is all: RAM+BBR+Flash
//Configuration of modern u-blox modules is now done via getVal/setVal/delVal, ie protocol v27 and above found on ZED-F9P
uint8_t SFE_UBLOX_GPS::setVal32(uint32_t key, uint32_t value, uint8_t layer, uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_VALSET;
  packetCfg.len = 4 + 4 + 4; //4 byte header, 4 byte key ID, 4 bytes of value
  packetCfg.startingSpot = 0;

  //Clear packet payload
  memset(payloadCfg, 0, packetCfg.len);

  payloadCfg[0] = 0;     //Message Version - set to 0
  payloadCfg[1] = layer; //By default we ask for the BBR layer

  //Load key into outgoing payload
  payloadCfg[4] = key >> 8 * 0; //Key LSB
  payloadCfg[5] = key >> 8 * 1;
  payloadCfg[6] = key >> 8 * 2;
  payloadCfg[7] = key >> 8 * 3;

  //Load user's value
  payloadCfg[8] = value >> 8 * 0; //Value LSB
  payloadCfg[9] = value >> 8 * 1;
  payloadCfg[10] = value >> 8 * 2;
  payloadCfg[11] = value >> 8 * 3;

  //Send VALSET command with this key and value
  return (sendCommand(&packetCfg, maxWait) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Start defining a new UBX-CFG-VALSET ubxPacket
//This function takes a full 32-bit key and 32-bit value
//Default layer is BBR
//Configuration of modern u-blox modules is now done via getVal/setVal/delVal, ie protocol v27 and above found on ZED-F9P
uint8_t SFE_UBLOX_GPS::newCfgValset32(uint32_t key, uint32_t value, uint8_t layer)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_VALSET;
  packetCfg.len = 4 + 4 + 4; //4 byte header, 4 byte key ID, 4 bytes of value
  packetCfg.startingSpot = 0;

  //Clear all of packet payload
  memset(payloadCfg, 0, packetCfgPayloadSize);

  payloadCfg[0] = 0;     //Message Version - set to 0
  payloadCfg[1] = layer; //By default we ask for the BBR layer

  //Load key into outgoing payload
  payloadCfg[4] = key >> 8 * 0; //Key LSB
  payloadCfg[5] = key >> 8 * 1;
  payloadCfg[6] = key >> 8 * 2;
  payloadCfg[7] = key >> 8 * 3;

  //Load user's value
  payloadCfg[8] = value >> 8 * 0; //Value LSB
  payloadCfg[9] = value >> 8 * 1;
  payloadCfg[10] = value >> 8 * 2;
  payloadCfg[11] = value >> 8 * 3;

  //All done
  return (true);
}

//Start defining a new UBX-CFG-VALSET ubxPacket
//This function takes a full 32-bit key and 16-bit value
//Default layer is BBR
//Configuration of modern u-blox modules is now done via getVal/setVal/delVal, ie protocol v27 and above found on ZED-F9P
uint8_t SFE_UBLOX_GPS::newCfgValset16(uint32_t key, uint16_t value, uint8_t layer)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_VALSET;
  packetCfg.len = 4 + 4 + 2; //4 byte header, 4 byte key ID, 2 bytes of value
  packetCfg.startingSpot = 0;

  //Clear all of packet payload
  memset(payloadCfg, 0, packetCfgPayloadSize);

  payloadCfg[0] = 0;     //Message Version - set to 0
  payloadCfg[1] = layer; //By default we ask for the BBR layer

  //Load key into outgoing payload
  payloadCfg[4] = key >> 8 * 0; //Key LSB
  payloadCfg[5] = key >> 8 * 1;
  payloadCfg[6] = key >> 8 * 2;
  payloadCfg[7] = key >> 8 * 3;

  //Load user's value
  payloadCfg[8] = value >> 8 * 0; //Value LSB
  payloadCfg[9] = value >> 8 * 1;

  //All done
  return (true);
}

//Start defining a new UBX-CFG-VALSET ubxPacket
//This function takes a full 32-bit key and 8-bit value
//Default layer is BBR
//Configuration of modern u-blox modules is now done via getVal/setVal/delVal, ie protocol v27 and above found on ZED-F9P
uint8_t SFE_UBLOX_GPS::newCfgValset8(uint32_t key, uint8_t value, uint8_t layer)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_VALSET;
  packetCfg.len = 4 + 4 + 1; //4 byte header, 4 byte key ID, 1 byte value
  packetCfg.startingSpot = 0;

  //Clear all of packet payload
  memset(payloadCfg, 0, packetCfgPayloadSize);

  payloadCfg[0] = 0;     //Message Version - set to 0
  payloadCfg[1] = layer; //By default we ask for the BBR layer

  //Load key into outgoing payload
  payloadCfg[4] = key >> 8 * 0; //Key LSB
  payloadCfg[5] = key >> 8 * 1;
  payloadCfg[6] = key >> 8 * 2;
  payloadCfg[7] = key >> 8 * 3;

  //Load user's value
  payloadCfg[8] = value; //Value

  //All done
  return (true);
}

//Add another keyID and value to an existing UBX-CFG-VALSET ubxPacket
//This function takes a full 32-bit key and 32-bit value
uint8_t SFE_UBLOX_GPS::addCfgValset32(uint32_t key, uint32_t value)
{
  //Load key into outgoing payload
  payloadCfg[packetCfg.len + 0] = key >> 8 * 0; //Key LSB
  payloadCfg[packetCfg.len + 1] = key >> 8 * 1;
  payloadCfg[packetCfg.len + 2] = key >> 8 * 2;
  payloadCfg[packetCfg.len + 3] = key >> 8 * 3;

  //Load user's value
  payloadCfg[packetCfg.len + 4] = value >> 8 * 0; //Value LSB
  payloadCfg[packetCfg.len + 5] = value >> 8 * 1;
  payloadCfg[packetCfg.len + 6] = value >> 8 * 2;
  payloadCfg[packetCfg.len + 7] = value >> 8 * 3;

  //Update packet length: 4 byte key ID, 4 bytes of value
  packetCfg.len = packetCfg.len + 4 + 4;

  //All done
  return (true);
}

//Add another keyID and value to an existing UBX-CFG-VALSET ubxPacket
//This function takes a full 32-bit key and 16-bit value
uint8_t SFE_UBLOX_GPS::addCfgValset16(uint32_t key, uint16_t value)
{
  //Load key into outgoing payload
  payloadCfg[packetCfg.len + 0] = key >> 8 * 0; //Key LSB
  payloadCfg[packetCfg.len + 1] = key >> 8 * 1;
  payloadCfg[packetCfg.len + 2] = key >> 8 * 2;
  payloadCfg[packetCfg.len + 3] = key >> 8 * 3;

  //Load user's value
  payloadCfg[packetCfg.len + 4] = value >> 8 * 0; //Value LSB
  payloadCfg[packetCfg.len + 5] = value >> 8 * 1;

  //Update packet length: 4 byte key ID, 2 bytes of value
  packetCfg.len = packetCfg.len + 4 + 2;

  //All done
  return (true);
}

//Add another keyID and value to an existing UBX-CFG-VALSET ubxPacket
//This function takes a full 32-bit key and 8-bit value
uint8_t SFE_UBLOX_GPS::addCfgValset8(uint32_t key, uint8_t value)
{
  //Load key into outgoing payload
  payloadCfg[packetCfg.len + 0] = key >> 8 * 0; //Key LSB
  payloadCfg[packetCfg.len + 1] = key >> 8 * 1;
  payloadCfg[packetCfg.len + 2] = key >> 8 * 2;
  payloadCfg[packetCfg.len + 3] = key >> 8 * 3;

  //Load user's value
  payloadCfg[packetCfg.len + 4] = value; //Value

  //Update packet length: 4 byte key ID, 1 byte value
  packetCfg.len = packetCfg.len + 4 + 1;

  //All done
  return (true);
}

//Add a final keyID and value to an existing UBX-CFG-VALSET ubxPacket and send it
//This function takes a full 32-bit key and 32-bit value
uint8_t SFE_UBLOX_GPS::sendCfgValset32(uint32_t key, uint32_t value, uint16_t maxWait)
{
  //Load keyID and value into outgoing payload
  addCfgValset32(key, value);

  //Send VALSET command with this key and value
  return (sendCommand(&packetCfg, maxWait) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Add a final keyID and value to an existing UBX-CFG-VALSET ubxPacket and send it
//This function takes a full 32-bit key and 16-bit value
uint8_t SFE_UBLOX_GPS::sendCfgValset16(uint32_t key, uint16_t value, uint16_t maxWait)
{
  //Load keyID and value into outgoing payload
  addCfgValset16(key, value);

  //Send VALSET command with this key and value
  return (sendCommand(&packetCfg, maxWait) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Add a final keyID and value to an existing UBX-CFG-VALSET ubxPacket and send it
//This function takes a full 32-bit key and 8-bit value
uint8_t SFE_UBLOX_GPS::sendCfgValset8(uint32_t key, uint8_t value, uint16_t maxWait)
{
  //Load keyID and value into outgoing payload
  addCfgValset8(key, value);

  //Send VALSET command with this key and value
  return (sendCommand(&packetCfg, maxWait) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//=-=-=-=-=-=-=-= "Automatic" Messages =-=-=-=-=-=-=-==-=-=-=-=-=-=-=
//=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-==-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-


// ***** NAV POSECEF automatic support

boolean SFE_UBLOX_GPS::getNAVPOSECEF(uint16_t maxWait)
{
  if (packetUBXNAVPOSECEF == NULL) initPacketUBXNAVPOSECEF(); //Check that RAM has been allocated for the POSECEF data
  if (packetUBXNAVPOSECEF == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVPOSECEF->automaticFlags.flags.bits.automatic && packetUBXNAVPOSECEF->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    checkUbloxInternal(&packetCfg, UBX_CLASS_NAV, UBX_NAV_POSECEF);
    return packetUBXNAVPOSECEF->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXNAVPOSECEF->automaticFlags.flags.bits.automatic && !packetUBXNAVPOSECEF->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    return (false);
  }
  else
  {
    //The GPS is not automatically reporting navigation position so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_NAV;
    packetCfg.id = UBX_NAV_POSECEF;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      return (true);
    }

    return (false);
  }
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getPOSECEF
//works.
boolean SFE_UBLOX_GPS::setAutoNAVPOSECEF(boolean enable, uint16_t maxWait)
{
  return setAutoNAVPOSECEF(enable, true, maxWait);
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getPOSECEF
//works.
boolean SFE_UBLOX_GPS::setAutoNAVPOSECEF(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXNAVPOSECEF == NULL) initPacketUBXNAVPOSECEF(); //Check that RAM has been allocated for the data
  if (packetUBXNAVPOSECEF == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_NAV;
  payloadCfg[1] = UBX_NAV_POSECEF;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXNAVPOSECEF->automaticFlags.flags.bits.automatic = enable;
    packetUBXNAVPOSECEF->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXNAVPOSECEF->moduleQueried.moduleQueried.bits.all = false;
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXNAVPOSECEFcopy.
boolean SFE_UBLOX_GPS::setAutoNAVPOSECEFcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoNAVPOSECEF(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXNAVPOSECEFcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXNAVPOSECEFcopy = new UBX_NAV_POSECEF_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXNAVPOSECEFcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoNAVPOSECEFcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXNAVPOSECEF->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and POSECEF is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoNAVPOSECEF(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXNAVPOSECEF == NULL) initPacketUBXNAVPOSECEF(); //Check that RAM has been allocated for the data
  if (packetUBXNAVPOSECEF == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXNAVPOSECEF->automaticFlags.flags.bits.automatic != enabled || packetUBXNAVPOSECEF->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXNAVPOSECEF->automaticFlags.flags.bits.automatic = enabled;
    packetUBXNAVPOSECEF->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXNAVPOSECEF and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXNAVPOSECEF()
{
  packetUBXNAVPOSECEF = new UBX_NAV_POSECEF_t; //Allocate RAM for the main struct
  if (packetUBXNAVPOSECEF == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXNAVPOSECEF: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXNAVPOSECEF->automaticFlags.flags.all = 0;
  packetUBXNAVPOSECEF->automaticFlags.callbackPointer = NULL;
  packetUBXNAVPOSECEF->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale. This is handy to get data alignment after CRC failure
//or if there are no helper functions and the user wants to request fresh data
void SFE_UBLOX_GPS::flushNAVPOSECEF()
{
  if (packetUBXNAVPOSECEF == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVPOSECEF->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logNAVPOSECEF(boolean enabled)
{
  if (packetUBXNAVPOSECEF == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVPOSECEF->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** NAV STATUS automatic support

boolean SFE_UBLOX_GPS::getNAVSTATUS(uint16_t maxWait)
{
  if (packetUBXNAVSTATUS == NULL) initPacketUBXNAVSTATUS(); //Check that RAM has been allocated for the STATUS data
  if (packetUBXNAVSTATUS == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVSTATUS->automaticFlags.flags.bits.automatic && packetUBXNAVSTATUS->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    checkUbloxInternal(&packetCfg, UBX_CLASS_NAV, UBX_NAV_STATUS);
    return packetUBXNAVSTATUS->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXNAVSTATUS->automaticFlags.flags.bits.automatic && !packetUBXNAVSTATUS->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    return (false);
  }
  else
  {
    //The GPS is not automatically reporting navigation position so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_NAV;
    packetCfg.id = UBX_NAV_STATUS;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      return (true);
    }

    return (false);
  }
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getSTATUS
//works.
boolean SFE_UBLOX_GPS::setAutoNAVSTATUS(boolean enable, uint16_t maxWait)
{
  return setAutoNAVSTATUS(enable, true, maxWait);
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getSTATUS
//works.
boolean SFE_UBLOX_GPS::setAutoNAVSTATUS(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXNAVSTATUS == NULL) initPacketUBXNAVSTATUS(); //Check that RAM has been allocated for the data
  if (packetUBXNAVSTATUS == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_NAV;
  payloadCfg[1] = UBX_NAV_STATUS;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXNAVSTATUS->automaticFlags.flags.bits.automatic = enable;
    packetUBXNAVSTATUS->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXNAVSTATUS->moduleQueried.moduleQueried.bits.all = false;
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXNAVSTATUScopy.
boolean SFE_UBLOX_GPS::setAutoNAVSTATUScallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoNAVSTATUS(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXNAVSTATUScopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXNAVSTATUScopy = new UBX_NAV_STATUS_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXNAVSTATUScopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoNAVSTATUScallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXNAVSTATUS->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and STATUS is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoNAVSTATUS(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXNAVSTATUS == NULL) initPacketUBXNAVSTATUS(); //Check that RAM has been allocated for the data
  if (packetUBXNAVSTATUS == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXNAVSTATUS->automaticFlags.flags.bits.automatic != enabled || packetUBXNAVSTATUS->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXNAVSTATUS->automaticFlags.flags.bits.automatic = enabled;
    packetUBXNAVSTATUS->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXNAVSTATUS and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXNAVSTATUS()
{
  packetUBXNAVSTATUS = new UBX_NAV_STATUS_t; //Allocate RAM for the main struct
  if (packetUBXNAVSTATUS == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXNAVSTATUS: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXNAVSTATUS->automaticFlags.flags.all = 0;
  packetUBXNAVSTATUS->automaticFlags.callbackPointer = NULL;
  packetUBXNAVSTATUS->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale. This is handy to get data alignment after CRC failure
//or if there are no helper functions and the user wants to request fresh data
void SFE_UBLOX_GPS::flushNAVSTATUS()
{
  if (packetUBXNAVSTATUS == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVSTATUS->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logNAVSTATUS(boolean enabled)
{
  if (packetUBXNAVSTATUS == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVSTATUS->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** DOP automatic support

boolean SFE_UBLOX_GPS::getDOP(uint16_t maxWait)
{
  if (packetUBXNAVDOP == NULL) initPacketUBXNAVDOP(); //Check that RAM has been allocated for the DOP data
  if (packetUBXNAVDOP == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVDOP->automaticFlags.flags.bits.automatic && packetUBXNAVDOP->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getDOP: Autoreporting"));
    // }
    checkUbloxInternal(&packetCfg, UBX_CLASS_NAV, UBX_NAV_DOP);
    return packetUBXNAVDOP->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXNAVDOP->automaticFlags.flags.bits.automatic && !packetUBXNAVDOP->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getDOP: Exit immediately"));
    // }
    return (false);
  }
  else
  {
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getDOP: Polling"));
    // }

    //The GPS is not automatically reporting navigation position so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_NAV;
    packetCfg.id = UBX_NAV_DOP;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      // if (_printDebug == true)
      // {
      //   _debugSerial->println(F("getDOP: data in packetCfg was OVERWRITTEN by another message (but that's OK)"));
      // }
      return (true);
    }

    // if (_printDebug == true)
    // {
    //   _debugSerial->print(F("getDOP retVal: "));
    //   _debugSerial->println(statusString(retVal));
    // }
    return (false);
  }
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getDOP
//works.
boolean SFE_UBLOX_GPS::setAutoDOP(boolean enable, uint16_t maxWait)
{
  return setAutoDOP(enable, true, maxWait);
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getDOP
//works.
boolean SFE_UBLOX_GPS::setAutoDOP(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXNAVDOP == NULL) initPacketUBXNAVDOP(); //Check that RAM has been allocated for the data
  if (packetUBXNAVDOP == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_NAV;
  payloadCfg[1] = UBX_NAV_DOP;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXNAVDOP->automaticFlags.flags.bits.automatic = enable;
    packetUBXNAVDOP->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.all = false;
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXNAVDOPcopy.
boolean SFE_UBLOX_GPS::setAutoDOPcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoDOP(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXNAVDOPcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXNAVDOPcopy = new UBX_NAV_DOP_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXNAVDOPcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoDOPcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXNAVDOP->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and DOP is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoDOP(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXNAVDOP == NULL) initPacketUBXNAVDOP(); //Check that RAM has been allocated for the data
  if (packetUBXNAVDOP == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXNAVDOP->automaticFlags.flags.bits.automatic != enabled || packetUBXNAVDOP->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXNAVDOP->automaticFlags.flags.bits.automatic = enabled;
    packetUBXNAVDOP->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXNAVDOP and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXNAVDOP()
{
  packetUBXNAVDOP = new UBX_NAV_DOP_t; //Allocate RAM for the main struct
  if (packetUBXNAVDOP == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXNAVDOP: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXNAVDOP->automaticFlags.flags.all = 0;
  packetUBXNAVDOP->automaticFlags.callbackPointer = NULL;
  packetUBXNAVDOP->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the DOP data as read/stale. This is handy to get data alignment after CRC failure
void SFE_UBLOX_GPS::flushDOP()
{
  if (packetUBXNAVDOP == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVDOP->moduleQueried.moduleQueried.all = 0; //Mark all DOPs as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logNAVDOP(boolean enabled)
{
  if (packetUBXNAVDOP == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVDOP->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** VEH ATT automatic support

boolean SFE_UBLOX_GPS::getVehAtt(uint16_t maxWait)
{
  if (packetUBXNAVATT == NULL) initPacketUBXNAVATT(); //Check that RAM has been allocated for the ESF RAW data
  if (packetUBXNAVATT == NULL) //Only attempt this if RAM allocation was successful
    return false;

  if (packetUBXNAVATT->automaticFlags.flags.bits.automatic && packetUBXNAVATT->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    checkUbloxInternal(&packetCfg, UBX_CLASS_NAV, UBX_NAV_ATT);
    return packetUBXNAVATT->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXNAVATT->automaticFlags.flags.bits.automatic && !packetUBXNAVATT->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    return (false);
  }
  else
  {
    //The GPS is not automatically reporting HNR PVT so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_NAV;
    packetCfg.id = UBX_NAV_ATT;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      return (true);
    }

    return (false);
  }

  return (false); // Trap. We should never get here...
}

//Enable or disable automatic NAV ATT message generation by the GNSS. This changes the way getVehAtt
//works.
boolean SFE_UBLOX_GPS::setAutoNAVATT(boolean enable, uint16_t maxWait)
{
  return setAutoNAVATT(enable, true, maxWait);
}

//Enable or disable automatic NAV ATT attitude message generation by the GNSS. This changes the way getVehAtt
//works.
boolean SFE_UBLOX_GPS::setAutoNAVATT(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXNAVATT == NULL) initPacketUBXNAVATT(); //Check that RAM has been allocated for the data
  if (packetUBXNAVATT == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_NAV;
  payloadCfg[1] = UBX_NAV_ATT;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXNAVATT->automaticFlags.flags.bits.automatic = enable;
    packetUBXNAVATT->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXNAVATT->moduleQueried.moduleQueried.bits.all = false; // Mark data as stale
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXNAVATTcopy.
boolean SFE_UBLOX_GPS::setAutoNAVATTcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoNAVATT(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXNAVATTcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXNAVATTcopy = new UBX_NAV_ATT_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXNAVATTcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoNAVATTcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXNAVATT->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and NAV ATT attitude is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoNAVATT(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXNAVATT == NULL) initPacketUBXNAVATT(); //Check that RAM has been allocated for the ESF RAW data
  if (packetUBXNAVATT == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXNAVATT->automaticFlags.flags.bits.automatic != enabled || packetUBXNAVATT->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXNAVATT->automaticFlags.flags.bits.automatic = enabled;
    packetUBXNAVATT->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXNAVATT and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXNAVATT()
{
  packetUBXNAVATT = new UBX_NAV_ATT_t; //Allocate RAM for the main struct
  if (packetUBXNAVATT == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXNAVATT: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXNAVATT->automaticFlags.flags.all = 0;
  packetUBXNAVATT->automaticFlags.callbackPointer = NULL;
  packetUBXNAVATT->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the DOP data as read/stale. This is handy to get data alignment after CRC failure
void SFE_UBLOX_GPS::flushNAVATT()
{
  if (packetUBXNAVATT == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVATT->moduleQueried.moduleQueried.all = 0; //Mark all DOPs as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logNAVATT(boolean enabled)
{
  if (packetUBXNAVATT == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVATT->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** PVT automatic support

//Get the latest Position/Velocity/Time solution and fill all global variables
boolean SFE_UBLOX_GPS::getPVT(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVPVT->automaticFlags.flags.bits.automatic && packetUBXNAVPVT->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getPVT: Autoreporting"));
    // }
    checkUbloxInternal(&packetCfg, UBX_CLASS_NAV, UBX_NAV_PVT);
    return packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all;
  }
  else if (packetUBXNAVPVT->automaticFlags.flags.bits.automatic && !packetUBXNAVPVT->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getPVT: Exit immediately"));
    // }
    return (false);
  }
  else
  {
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getPVT: Polling"));
    // }

    //The GPS is not automatically reporting navigation position so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_NAV;
    packetCfg.id = UBX_NAV_PVT;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;
    //packetCfg.startingSpot = 20; //Begin listening at spot 20 so we can record up to 20+packetCfgPayloadSize = 84 bytes Note:now hard-coded in processUBX

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      // if (_printDebug == true)
      // {
      //   _debugSerial->println(F("getPVT: data in packetCfg was OVERWRITTEN by another message (but that's OK)"));
      // }
      return (true);
    }

    // if (_printDebug == true)
    // {
    //   _debugSerial->print(F("getPVT retVal: "));
    //   _debugSerial->println(statusString(retVal));
    // }
    return (false);
  }
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getPVT
//works.
boolean SFE_UBLOX_GPS::setAutoPVT(boolean enable, uint16_t maxWait)
{
  return setAutoPVT(enable, true, maxWait);
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getPVT
//works.
boolean SFE_UBLOX_GPS::setAutoPVT(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_NAV;
  payloadCfg[1] = UBX_NAV_PVT;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXNAVPVT->automaticFlags.flags.bits.automatic = enable;
    packetUBXNAVPVT->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return ok;
}

//Enable automatic navigation message generation by the GNSS. This changes the way getPVT works.
//Data is passed to the callback in packetUBXNAVPVTcopy.
boolean SFE_UBLOX_GPS::setAutoPVTcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoPVT(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAutoPVT failed

  if (packetUBXNAVPVTcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXNAVPVTcopy = new UBX_NAV_PVT_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXNAVPVTcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoPVTcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXNAVPVT->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and PVT is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoPVT(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXNAVPVT->automaticFlags.flags.bits.automatic != enabled || packetUBXNAVPVT->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
      packetUBXNAVPVT->automaticFlags.flags.bits.automatic = enabled;
      packetUBXNAVPVT->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXNAVPVT and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXNAVPVT()
{
  packetUBXNAVPVT = new UBX_NAV_PVT_t; //Allocate RAM for the main struct
  if (packetUBXNAVPVT == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXNAVPVT: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXNAVPVT->automaticFlags.flags.all = 0;
  packetUBXNAVPVT->automaticFlags.callbackPointer = NULL;
  packetUBXNAVPVT->moduleQueried.moduleQueried1.all = 0;
  packetUBXNAVPVT->moduleQueried.moduleQueried2.all = 0;
  return (true);
}

//Mark all the PVT data as read/stale. This is handy to get data alignment after CRC failure
void SFE_UBLOX_GPS::flushPVT()
{
  if (packetUBXNAVPVT == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVPVT->moduleQueried.moduleQueried1.all = 0; //Mark all datums as stale (read before)
  packetUBXNAVPVT->moduleQueried.moduleQueried2.all = 0;
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logNAVPVT(boolean enabled)
{
  if (packetUBXNAVPVT == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVPVT->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** NAV ODO automatic support

boolean SFE_UBLOX_GPS::getNAVODO(uint16_t maxWait)
{
  if (packetUBXNAVODO == NULL) initPacketUBXNAVODO(); //Check that RAM has been allocated for the ODO data
  if (packetUBXNAVODO == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVODO->automaticFlags.flags.bits.automatic && packetUBXNAVODO->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    checkUbloxInternal(&packetCfg, UBX_CLASS_NAV, UBX_NAV_ODO);
    return packetUBXNAVODO->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXNAVODO->automaticFlags.flags.bits.automatic && !packetUBXNAVODO->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    return (false);
  }
  else
  {
    //The GPS is not automatically reporting navigation position so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_NAV;
    packetCfg.id = UBX_NAV_ODO;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      return (true);
    }

    return (false);
  }
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getODO
//works.
boolean SFE_UBLOX_GPS::setAutoNAVODO(boolean enable, uint16_t maxWait)
{
  return setAutoNAVODO(enable, true, maxWait);
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getODO
//works.
boolean SFE_UBLOX_GPS::setAutoNAVODO(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXNAVODO == NULL) initPacketUBXNAVODO(); //Check that RAM has been allocated for the data
  if (packetUBXNAVODO == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_NAV;
  payloadCfg[1] = UBX_NAV_ODO;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXNAVODO->automaticFlags.flags.bits.automatic = enable;
    packetUBXNAVODO->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXNAVODO->moduleQueried.moduleQueried.bits.all = false;
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXNAVODOcopy.
boolean SFE_UBLOX_GPS::setAutoNAVODOcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoNAVODO(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXNAVODOcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXNAVODOcopy = new UBX_NAV_ODO_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXNAVODOcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoNAVODOcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXNAVODO->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and ODO is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoNAVODO(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXNAVODO == NULL) initPacketUBXNAVODO(); //Check that RAM has been allocated for the data
  if (packetUBXNAVODO == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXNAVODO->automaticFlags.flags.bits.automatic != enabled || packetUBXNAVODO->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXNAVODO->automaticFlags.flags.bits.automatic = enabled;
    packetUBXNAVODO->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXNAVODO and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXNAVODO()
{
  packetUBXNAVODO = new UBX_NAV_ODO_t; //Allocate RAM for the main struct
  if (packetUBXNAVODO == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXNAVODO: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXNAVODO->automaticFlags.flags.all = 0;
  packetUBXNAVODO->automaticFlags.callbackPointer = NULL;
  packetUBXNAVODO->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushNAVODO()
{
  if (packetUBXNAVODO == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVODO->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logNAVODO(boolean enabled)
{
  if (packetUBXNAVODO == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVODO->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** NAV VELECEF automatic support

boolean SFE_UBLOX_GPS::getNAVVELECEF(uint16_t maxWait)
{
  if (packetUBXNAVVELECEF == NULL) initPacketUBXNAVVELECEF(); //Check that RAM has been allocated for the VELECEF data
  if (packetUBXNAVVELECEF == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVVELECEF->automaticFlags.flags.bits.automatic && packetUBXNAVVELECEF->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    checkUbloxInternal(&packetCfg, UBX_CLASS_NAV, UBX_NAV_VELECEF);
    return packetUBXNAVVELECEF->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXNAVVELECEF->automaticFlags.flags.bits.automatic && !packetUBXNAVVELECEF->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    return (false);
  }
  else
  {
    //The GPS is not automatically reporting navigation position so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_NAV;
    packetCfg.id = UBX_NAV_VELECEF;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      return (true);
    }

    return (false);
  }
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getVELECEF
//works.
boolean SFE_UBLOX_GPS::setAutoNAVVELECEF(boolean enable, uint16_t maxWait)
{
  return setAutoNAVVELECEF(enable, true, maxWait);
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getVELECEF
//works.
boolean SFE_UBLOX_GPS::setAutoNAVVELECEF(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXNAVVELECEF == NULL) initPacketUBXNAVVELECEF(); //Check that RAM has been allocated for the data
  if (packetUBXNAVVELECEF == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_NAV;
  payloadCfg[1] = UBX_NAV_VELECEF;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXNAVVELECEF->automaticFlags.flags.bits.automatic = enable;
    packetUBXNAVVELECEF->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXNAVVELECEF->moduleQueried.moduleQueried.bits.all = false;
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXNAVVELECEFcopy.
boolean SFE_UBLOX_GPS::setAutoNAVVELECEFcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoNAVVELECEF(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXNAVVELECEFcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXNAVVELECEFcopy = new UBX_NAV_VELECEF_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXNAVVELECEFcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoNAVVELECEFcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXNAVVELECEF->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and VELECEF is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoNAVVELECEF(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXNAVVELECEF == NULL) initPacketUBXNAVVELECEF(); //Check that RAM has been allocated for the data
  if (packetUBXNAVVELECEF == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXNAVVELECEF->automaticFlags.flags.bits.automatic != enabled || packetUBXNAVVELECEF->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXNAVVELECEF->automaticFlags.flags.bits.automatic = enabled;
    packetUBXNAVVELECEF->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXNAVVELECEF and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXNAVVELECEF()
{
  packetUBXNAVVELECEF = new UBX_NAV_VELECEF_t; //Allocate RAM for the main struct
  if (packetUBXNAVVELECEF == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXNAVVELECEF: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXNAVVELECEF->automaticFlags.flags.all = 0;
  packetUBXNAVVELECEF->automaticFlags.callbackPointer = NULL;
  packetUBXNAVVELECEF->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushNAVVELECEF()
{
  if (packetUBXNAVVELECEF == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVVELECEF->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logNAVVELECEF(boolean enabled)
{
  if (packetUBXNAVVELECEF == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVVELECEF->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** NAV VELNED automatic support

boolean SFE_UBLOX_GPS::getNAVVELNED(uint16_t maxWait)
{
  if (packetUBXNAVVELNED == NULL) initPacketUBXNAVVELNED(); //Check that RAM has been allocated for the VELNED data
  if (packetUBXNAVVELNED == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVVELNED->automaticFlags.flags.bits.automatic && packetUBXNAVVELNED->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    checkUbloxInternal(&packetCfg, UBX_CLASS_NAV, UBX_NAV_VELNED);
    return packetUBXNAVVELNED->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXNAVVELNED->automaticFlags.flags.bits.automatic && !packetUBXNAVVELNED->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    return (false);
  }
  else
  {
    //The GPS is not automatically reporting navigation position so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_NAV;
    packetCfg.id = UBX_NAV_VELNED;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      return (true);
    }

    return (false);
  }
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getVELNED
//works.
boolean SFE_UBLOX_GPS::setAutoNAVVELNED(boolean enable, uint16_t maxWait)
{
  return setAutoNAVVELNED(enable, true, maxWait);
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getVELNED
//works.
boolean SFE_UBLOX_GPS::setAutoNAVVELNED(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXNAVVELNED == NULL) initPacketUBXNAVVELNED(); //Check that RAM has been allocated for the data
  if (packetUBXNAVVELNED == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_NAV;
  payloadCfg[1] = UBX_NAV_VELNED;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXNAVVELNED->automaticFlags.flags.bits.automatic = enable;
    packetUBXNAVVELNED->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXNAVVELNED->moduleQueried.moduleQueried.bits.all = false;
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXNAVVELNEDcopy.
boolean SFE_UBLOX_GPS::setAutoNAVVELNEDcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoNAVVELNED(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXNAVVELNEDcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXNAVVELNEDcopy = new UBX_NAV_VELNED_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXNAVVELNEDcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoNAVVELNEDcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXNAVVELNED->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and VELNED is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoNAVVELNED(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXNAVVELNED == NULL) initPacketUBXNAVVELNED(); //Check that RAM has been allocated for the data
  if (packetUBXNAVVELNED == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXNAVVELNED->automaticFlags.flags.bits.automatic != enabled || packetUBXNAVVELNED->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXNAVVELNED->automaticFlags.flags.bits.automatic = enabled;
    packetUBXNAVVELNED->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXNAVVELNED and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXNAVVELNED()
{
  packetUBXNAVVELNED = new UBX_NAV_VELNED_t; //Allocate RAM for the main struct
  if (packetUBXNAVVELNED == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXNAVVELNED: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXNAVVELNED->automaticFlags.flags.all = 0;
  packetUBXNAVVELNED->automaticFlags.callbackPointer = NULL;
  packetUBXNAVVELNED->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushNAVVELNED()
{
  if (packetUBXNAVVELNED == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!
  packetUBXNAVVELNED->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logNAVVELNED(boolean enabled)
{
  if (packetUBXNAVVELNED == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVVELNED->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** NAV HPPOSECEF automatic support

boolean SFE_UBLOX_GPS::getNAVHPPOSECEF(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSECEF == NULL) initPacketUBXNAVHPPOSECEF(); //Check that RAM has been allocated for the HPPOSECEF data
  if (packetUBXNAVHPPOSECEF == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.automatic && packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    checkUbloxInternal(&packetCfg, UBX_CLASS_NAV, UBX_NAV_HPPOSECEF);
    return packetUBXNAVHPPOSECEF->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.automatic && !packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    return (false);
  }
  else
  {
    //The GPS is not automatically reporting navigation position so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_NAV;
    packetCfg.id = UBX_NAV_HPPOSECEF;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      return (true);
    }

    return (false);
  }
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getHPPOSECEF
//works.
boolean SFE_UBLOX_GPS::setAutoNAVHPPOSECEF(boolean enable, uint16_t maxWait)
{
  return setAutoNAVHPPOSECEF(enable, true, maxWait);
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getHPPOSECEF
//works.
boolean SFE_UBLOX_GPS::setAutoNAVHPPOSECEF(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXNAVHPPOSECEF == NULL) initPacketUBXNAVHPPOSECEF(); //Check that RAM has been allocated for the data
  if (packetUBXNAVHPPOSECEF == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_NAV;
  payloadCfg[1] = UBX_NAV_HPPOSECEF;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.automatic = enable;
    packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXNAVHPPOSECEF->moduleQueried.moduleQueried.bits.all = false;
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXNAVHPPOSECEFcopy.
boolean SFE_UBLOX_GPS::setAutoNAVHPPOSECEFcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoNAVHPPOSECEF(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXNAVHPPOSECEFcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXNAVHPPOSECEFcopy = new UBX_NAV_HPPOSECEF_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXNAVHPPOSECEFcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoNAVHPPOSECEFcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXNAVHPPOSECEF->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and HPPOSECEF is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoNAVHPPOSECEF(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXNAVHPPOSECEF == NULL) initPacketUBXNAVHPPOSECEF(); //Check that RAM has been allocated for the data
  if (packetUBXNAVHPPOSECEF == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.automatic != enabled || packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.automatic = enabled;
    packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXNAVHPPOSECEF and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXNAVHPPOSECEF()
{
  packetUBXNAVHPPOSECEF = new UBX_NAV_HPPOSECEF_t; //Allocate RAM for the main struct
  if (packetUBXNAVHPPOSECEF == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXNAVHPPOSECEF: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXNAVHPPOSECEF->automaticFlags.flags.all = 0;
  packetUBXNAVHPPOSECEF->automaticFlags.callbackPointer = NULL;
  packetUBXNAVHPPOSECEF->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushNAVHPPOSECEF()
{
  if (packetUBXNAVHPPOSECEF == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVHPPOSECEF->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logNAVHPPOSECEF(boolean enabled)
{
  if (packetUBXNAVHPPOSECEF == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVHPPOSECEF->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** NAV HPPOSLLH automatic support

boolean SFE_UBLOX_GPS::getHPPOSLLH(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the HPPOSLLH data
  if (packetUBXNAVHPPOSLLH == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.automatic && packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getHPPOSLLH: Autoreporting"));
    // }
    checkUbloxInternal(&packetCfg, UBX_CLASS_NAV, UBX_NAV_HPPOSLLH);
    return packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.automatic && !packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getHPPOSLLH: Exit immediately"));
    // }
    return (false);
  }
  else
  {
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getHPPOSLLH: Polling"));
    // }

    //The GPS is not automatically reporting navigation position so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_NAV;
    packetCfg.id = UBX_NAV_HPPOSLLH;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      // if (_printDebug == true)
      // {
      //   _debugSerial->println(F("getHPPOSLLH: data in packetCfg was OVERWRITTEN by another message (but that's OK)"));
      // }
      return (true);
    }

    // if (_printDebug == true)
    // {
    //   _debugSerial->print(F("getHPPOSLLH retVal: "));
    //   _debugSerial->println(statusString(retVal));
    // }
    return (false);
  }
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getHPPOSLLH
//works.
boolean SFE_UBLOX_GPS::setAutoHPPOSLLH(boolean enable, uint16_t maxWait)
{
  return setAutoHPPOSLLH(enable, true, maxWait);
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getHPPOSLLH
//works.
boolean SFE_UBLOX_GPS::setAutoHPPOSLLH(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the data
  if (packetUBXNAVHPPOSLLH == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_NAV;
  payloadCfg[1] = UBX_NAV_HPPOSLLH;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.automatic = enable;
    packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.all = false;
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXNAVHPPOSLLHcopy.
boolean SFE_UBLOX_GPS::setAutoHPPOSLLHcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoHPPOSLLH(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXNAVHPPOSLLHcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXNAVHPPOSLLHcopy = new UBX_NAV_HPPOSLLH_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXNAVHPPOSLLHcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoHPPOSLLHcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXNAVHPPOSLLH->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and HPPOSLLH is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoHPPOSLLH(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the data
  if (packetUBXNAVHPPOSLLH == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.automatic != enabled || packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.automatic = enabled;
    packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXNAVHPPOSLLH and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXNAVHPPOSLLH()
{
  packetUBXNAVHPPOSLLH = new UBX_NAV_HPPOSLLH_t; //Allocate RAM for the main struct
  if (packetUBXNAVHPPOSLLH == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXNAVHPPOSLLH: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXNAVHPPOSLLH->automaticFlags.flags.all = 0;
  packetUBXNAVHPPOSLLH->automaticFlags.callbackPointer = NULL;
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the HPPOSLLH data as read/stale. This is handy to get data alignment after CRC failure
void SFE_UBLOX_GPS::flushHPPOSLLH()
{
  if (packetUBXNAVHPPOSLLH == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.all = 0;   //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logNAVHPPOSLLH(boolean enabled)
{
  if (packetUBXNAVHPPOSLLH == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVHPPOSLLH->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** NAV CLOCK automatic support

boolean SFE_UBLOX_GPS::getNAVCLOCK(uint16_t maxWait)
{
  if (packetUBXNAVCLOCK == NULL) initPacketUBXNAVCLOCK(); //Check that RAM has been allocated for the CLOCK data
  if (packetUBXNAVCLOCK == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVCLOCK->automaticFlags.flags.bits.automatic && packetUBXNAVCLOCK->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    checkUbloxInternal(&packetCfg, UBX_CLASS_NAV, UBX_NAV_CLOCK);
    return packetUBXNAVCLOCK->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXNAVCLOCK->automaticFlags.flags.bits.automatic && !packetUBXNAVCLOCK->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    return (false);
  }
  else
  {
    //The GPS is not automatically reporting CLOCK so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_NAV;
    packetCfg.id = UBX_NAV_CLOCK;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      return (true);
    }

    return (false);
  }
}

//Enable or disable automatic CLOCK message generation by the GNSS. This changes the way getNAVCLOCK
//works.
boolean SFE_UBLOX_GPS::setAutoNAVCLOCK(boolean enable, uint16_t maxWait)
{
  return setAutoNAVCLOCK(enable, true, maxWait);
}

//Enable or disable automatic CLOCK attitude message generation by the GNSS. This changes the way getNAVCLOCK
//works.
boolean SFE_UBLOX_GPS::setAutoNAVCLOCK(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXNAVCLOCK == NULL) initPacketUBXNAVCLOCK(); //Check that RAM has been allocated for the data
  if (packetUBXNAVCLOCK == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_NAV;
  payloadCfg[1] = UBX_NAV_CLOCK;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXNAVCLOCK->automaticFlags.flags.bits.automatic = enable;
    packetUBXNAVCLOCK->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXNAVCLOCK->moduleQueried.moduleQueried.bits.all = false; // Mark data as stale
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXNAVCLOCKcopy.
boolean SFE_UBLOX_GPS::setAutoNAVCLOCKcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoNAVCLOCK(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXNAVCLOCKcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXNAVCLOCKcopy = new UBX_NAV_CLOCK_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXNAVCLOCKcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoNAVCLOCKcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXNAVCLOCK->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and HNR attitude is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoNAVCLOCK(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXNAVCLOCK == NULL) initPacketUBXNAVCLOCK(); //Check that RAM has been allocated for the CLOCK data
  if (packetUBXNAVCLOCK == NULL) //Bail if the RAM allocation failed
    return (false);

  boolean changes = packetUBXNAVCLOCK->automaticFlags.flags.bits.automatic != enabled || packetUBXNAVCLOCK->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXNAVCLOCK->automaticFlags.flags.bits.automatic = enabled;
    packetUBXNAVCLOCK->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXNAVCLOCK and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXNAVCLOCK()
{
  packetUBXNAVCLOCK = new UBX_NAV_CLOCK_t ; //Allocate RAM for the main struct
  if (packetUBXNAVCLOCK == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXNAVCLOCK: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXNAVCLOCK->automaticFlags.flags.all = 0;
  packetUBXNAVCLOCK->automaticFlags.callbackPointer = NULL;
  packetUBXNAVCLOCK->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushNAVCLOCK()
{
  if (packetUBXNAVCLOCK == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVCLOCK->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logNAVCLOCK(boolean enabled)
{
  if (packetUBXNAVCLOCK == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVCLOCK->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** NAV SVIN automatic support

//Reads survey in status and sets the global variables
//for status, position valid, observation time, and mean 3D StdDev
//Returns true if commands was successful
boolean SFE_UBLOX_GPS::getSurveyStatus(uint16_t maxWait)
{
  if (packetUBXNAVSVIN == NULL) initPacketUBXNAVSVIN(); //Check that RAM has been allocated for the SVIN data
  if (packetUBXNAVSVIN == NULL) // Abort if the RAM allocation failed
    return (false);

  packetCfg.cls = UBX_CLASS_NAV;
  packetCfg.id = UBX_NAV_SVIN;
  packetCfg.len = 0;
  packetCfg.startingSpot = 0;

  //The data is parsed as part of processing the response
  sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

  if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
    return (true);

  if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
  {
    return (true);
  }

  return (false);
}

// Allocate RAM for packetUBXNAVSVIN and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXNAVSVIN()
{
  packetUBXNAVSVIN = new UBX_NAV_SVIN_t; //Allocate RAM for the main struct
  if (packetUBXNAVSVIN == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXNAVSVIN: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXNAVSVIN->automaticFlags.flags.all = 0;
  packetUBXNAVSVIN->automaticFlags.callbackPointer = NULL;
  packetUBXNAVSVIN->moduleQueried.moduleQueried.all = 0;
  return (true);
}

// ***** NAV RELPOSNED automatic support

//Relative Positioning Information in NED frame
//Returns true if commands was successful
//Note:
//  RELPOSNED on the M8 is only 40 bytes long
//  RELPOSNED on the F9 is 64 bytes long and contains much more information
boolean SFE_UBLOX_GPS::getRELPOSNED(uint16_t maxWait)
{
  if (packetUBXNAVRELPOSNED == NULL) initPacketUBXNAVRELPOSNED(); //Check that RAM has been allocated for the RELPOSNED data
  if (packetUBXNAVRELPOSNED == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVRELPOSNED->automaticFlags.flags.bits.automatic && packetUBXNAVRELPOSNED->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    checkUbloxInternal(&packetCfg, UBX_CLASS_NAV, UBX_NAV_RELPOSNED);
    return packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXNAVRELPOSNED->automaticFlags.flags.bits.automatic && !packetUBXNAVRELPOSNED->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    return (false);
  }
  else
  {
    //The GPS is not automatically reporting RELPOSNED so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_NAV;
    packetCfg.id = UBX_NAV_RELPOSNED;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      return (true);
    }

    return (false);
  }
}

//Enable or disable automatic RELPOSNED message generation by the GNSS. This changes the way getRELPOSNED
//works.
boolean SFE_UBLOX_GPS::setAutoRELPOSNED(boolean enable, uint16_t maxWait)
{
  return setAutoRELPOSNED(enable, true, maxWait);
}

//Enable or disable automatic HNR attitude message generation by the GNSS. This changes the way getRELPOSNED
//works.
boolean SFE_UBLOX_GPS::setAutoRELPOSNED(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXNAVRELPOSNED == NULL) initPacketUBXNAVRELPOSNED(); //Check that RAM has been allocated for the data
  if (packetUBXNAVRELPOSNED == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_NAV;
  payloadCfg[1] = UBX_NAV_RELPOSNED;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXNAVRELPOSNED->automaticFlags.flags.bits.automatic = enable;
    packetUBXNAVRELPOSNED->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.all = false; // Mark data as stale
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXNAVRELPOSNEDcopy.
boolean SFE_UBLOX_GPS::setAutoRELPOSNEDcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoRELPOSNED(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXNAVRELPOSNEDcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXNAVRELPOSNEDcopy = new UBX_NAV_RELPOSNED_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXNAVRELPOSNEDcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoRELPOSNEDcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXNAVRELPOSNED->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and HNR attitude is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoRELPOSNED(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXNAVRELPOSNED == NULL) initPacketUBXNAVRELPOSNED(); //Check that RAM has been allocated for the RELPOSNED data
  if (packetUBXNAVRELPOSNED == NULL) //Bail if the RAM allocation failed
    return (false);

  boolean changes = packetUBXNAVRELPOSNED->automaticFlags.flags.bits.automatic != enabled || packetUBXNAVRELPOSNED->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXNAVRELPOSNED->automaticFlags.flags.bits.automatic = enabled;
    packetUBXNAVRELPOSNED->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXNAVRELPOSNED and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXNAVRELPOSNED()
{
  packetUBXNAVRELPOSNED = new UBX_NAV_RELPOSNED_t ; //Allocate RAM for the main struct
  if (packetUBXNAVRELPOSNED == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXNAVRELPOSNED: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXNAVRELPOSNED->automaticFlags.flags.all = 0;
  packetUBXNAVRELPOSNED->automaticFlags.callbackPointer = NULL;
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushNAVRELPOSNED()
{
  if (packetUBXNAVRELPOSNED == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logNAVRELPOSNED(boolean enabled)
{
  if (packetUBXNAVRELPOSNED == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXNAVRELPOSNED->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** RXM SFRBX automatic support

boolean SFE_UBLOX_GPS::getRXMSFRBX(uint16_t maxWait)
{
  if (packetUBXRXMSFRBX == NULL) initPacketUBXRXMSFRBX(); //Check that RAM has been allocated for the TM2 data
  if (packetUBXRXMSFRBX == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXRXMSFRBX->automaticFlags.flags.bits.automatic && packetUBXRXMSFRBX->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    checkUbloxInternal(&packetCfg, UBX_CLASS_TIM, UBX_TIM_TM2);
    return packetUBXRXMSFRBX->moduleQueried;
  }
  else if (packetUBXRXMSFRBX->automaticFlags.flags.bits.automatic && !packetUBXRXMSFRBX->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    return (false);
  }
  else
  {
    //The GPS is not automatically reporting navigation position so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_RXM;
    packetCfg.id = UBX_RXM_SFRBX;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      return (true);
    }

    return (false);
  }
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getRXMSFRBX
//works.
boolean SFE_UBLOX_GPS::setAutoRXMSFRBX(boolean enable, uint16_t maxWait)
{
  return setAutoRXMSFRBX(enable, true, maxWait);
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getRXMSFRBX
//works.
boolean SFE_UBLOX_GPS::setAutoRXMSFRBX(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXRXMSFRBX == NULL) initPacketUBXRXMSFRBX(); //Check that RAM has been allocated for the data
  if (packetUBXRXMSFRBX == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_RXM;
  payloadCfg[1] = UBX_RXM_SFRBX;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXRXMSFRBX->automaticFlags.flags.bits.automatic = enable;
    packetUBXRXMSFRBX->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXRXMSFRBX->moduleQueried = false;
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXRXMSFRBXcopy.
boolean SFE_UBLOX_GPS::setAutoRXMSFRBXcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoRXMSFRBX(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXRXMSFRBXcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXRXMSFRBXcopy = new UBX_RXM_SFRBX_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXRXMSFRBXcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoRXMSFRBXcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXRXMSFRBX->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and SFRBX is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoRXMSFRBX(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXRXMSFRBX == NULL) initPacketUBXRXMSFRBX(); //Check that RAM has been allocated for the data
  if (packetUBXRXMSFRBX == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXRXMSFRBX->automaticFlags.flags.bits.automatic != enabled || packetUBXRXMSFRBX->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXRXMSFRBX->automaticFlags.flags.bits.automatic = enabled;
    packetUBXRXMSFRBX->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXRXMSFRBX and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXRXMSFRBX()
{
  packetUBXRXMSFRBX = new UBX_RXM_SFRBX_t; //Allocate RAM for the main struct
  if (packetUBXRXMSFRBX == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXRXMSFRBX: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXRXMSFRBX->automaticFlags.flags.all = 0;
  packetUBXRXMSFRBX->automaticFlags.callbackPointer = NULL;
  packetUBXRXMSFRBX->moduleQueried = false;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushRXMSFRBX()
{
  if (packetUBXRXMSFRBX == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXRXMSFRBX->moduleQueried = false; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logRXMSFRBX(boolean enabled)
{
  if (packetUBXRXMSFRBX == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXRXMSFRBX->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** RXM RAWX automatic support

boolean SFE_UBLOX_GPS::getRXMRAWX(uint16_t maxWait)
{
  if (packetUBXRXMRAWX == NULL) initPacketUBXRXMRAWX(); //Check that RAM has been allocated for the TM2 data
  if (packetUBXRXMRAWX == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXRXMRAWX->automaticFlags.flags.bits.automatic && packetUBXRXMRAWX->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    checkUbloxInternal(&packetCfg, UBX_CLASS_TIM, UBX_TIM_TM2);
    return packetUBXRXMRAWX->moduleQueried;
  }
  else if (packetUBXRXMRAWX->automaticFlags.flags.bits.automatic && !packetUBXRXMRAWX->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    return (false);
  }
  else
  {
    //The GPS is not automatically reporting navigation position so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_RXM;
    packetCfg.id = UBX_RXM_RAWX;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      return (true);
    }

    return (false);
  }
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getRXMRAWX
//works.
boolean SFE_UBLOX_GPS::setAutoRXMRAWX(boolean enable, uint16_t maxWait)
{
  return setAutoRXMRAWX(enable, true, maxWait);
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getRXMRAWX
//works.
boolean SFE_UBLOX_GPS::setAutoRXMRAWX(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXRXMRAWX == NULL) initPacketUBXRXMRAWX(); //Check that RAM has been allocated for the data
  if (packetUBXRXMRAWX == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_RXM;
  payloadCfg[1] = UBX_RXM_RAWX;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXRXMRAWX->automaticFlags.flags.bits.automatic = enable;
    packetUBXRXMRAWX->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXRXMRAWX->moduleQueried = false;
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXRXMRAWXcopy.
boolean SFE_UBLOX_GPS::setAutoRXMRAWXcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoRXMRAWX(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXRXMRAWXcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXRXMRAWXcopy = new UBX_RXM_RAWX_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXRXMRAWXcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoRXMRAWXcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXRXMRAWX->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and VELNED is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoRXMRAWX(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXRXMRAWX == NULL) initPacketUBXRXMRAWX(); //Check that RAM has been allocated for the data
  if (packetUBXRXMRAWX == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXRXMRAWX->automaticFlags.flags.bits.automatic != enabled || packetUBXRXMRAWX->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXRXMRAWX->automaticFlags.flags.bits.automatic = enabled;
    packetUBXRXMRAWX->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXRXMRAWX and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXRXMRAWX()
{
  packetUBXRXMRAWX = new UBX_RXM_RAWX_t; //Allocate RAM for the main struct
  if (packetUBXRXMRAWX == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXRXMRAWX: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXRXMRAWX->automaticFlags.flags.all = 0;
  packetUBXRXMRAWX->automaticFlags.callbackPointer = NULL;
  packetUBXRXMRAWX->moduleQueried = false;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushRXMRAWX()
{
  if (packetUBXRXMRAWX == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXRXMRAWX->moduleQueried = false; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logRXMRAWX(boolean enabled)
{
  if (packetUBXRXMRAWX == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXRXMRAWX->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** CFG automatic support

//Get the latest CFG RATE - as used by isConnected
boolean SFE_UBLOX_GPS::getNavigationFrequencyInternal(uint16_t maxWait)
{
  if (packetUBXCFGRATE == NULL) initPacketUBXCFGRATE(); //Check that RAM has been allocated for the data
  if (packetUBXCFGRATE == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXCFGRATE->automaticFlags.flags.bits.automatic && packetUBXCFGRATE->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    checkUbloxInternal(&packetCfg, UBX_CLASS_CFG, UBX_CFG_RATE);
    return packetUBXCFGRATE->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXCFGRATE->automaticFlags.flags.bits.automatic && !packetUBXCFGRATE->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    return (false);
  }
  else
  {
    //The GPS is not automatically reporting navigation rate so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_CFG;
    packetCfg.id = UBX_CFG_RATE;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      return (true);
    }

    return (false);
  }
}

// Allocate RAM for packetUBXCFGRATE and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXCFGRATE()
{
  packetUBXCFGRATE = new UBX_CFG_RATE_t; //Allocate RAM for the main struct
  if (packetUBXCFGRATE == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXCFGRATE: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXCFGRATE->automaticFlags.flags.all = 0;
  packetUBXCFGRATE->automaticFlags.callbackPointer = NULL;
  packetUBXCFGRATE->moduleQueried.moduleQueried.all = 0;
  return (true);
}

// ***** TIM TM2 automatic support

boolean SFE_UBLOX_GPS::getTIMTM2(uint16_t maxWait)
{
  if (packetUBXTIMTM2 == NULL) initPacketUBXTIMTM2(); //Check that RAM has been allocated for the TM2 data
  if (packetUBXTIMTM2 == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXTIMTM2->automaticFlags.flags.bits.automatic && packetUBXTIMTM2->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    checkUbloxInternal(&packetCfg, UBX_CLASS_TIM, UBX_TIM_TM2);
    return packetUBXTIMTM2->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXTIMTM2->automaticFlags.flags.bits.automatic && !packetUBXTIMTM2->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    return (false);
  }
  else
  {
    //The GPS is not automatically reporting navigation position so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_TIM;
    packetCfg.id = UBX_TIM_TM2;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      return (true);
    }

    return (false);
  }
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getTIMTM2
//works.
boolean SFE_UBLOX_GPS::setAutoTIMTM2(boolean enable, uint16_t maxWait)
{
  return setAutoTIMTM2(enable, true, maxWait);
}

//Enable or disable automatic navigation message generation by the GNSS. This changes the way getTIMTM2
//works.
boolean SFE_UBLOX_GPS::setAutoTIMTM2(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXTIMTM2 == NULL) initPacketUBXTIMTM2(); //Check that RAM has been allocated for the data
  if (packetUBXTIMTM2 == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_TIM;
  payloadCfg[1] = UBX_TIM_TM2;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXTIMTM2->automaticFlags.flags.bits.automatic = enable;
    packetUBXTIMTM2->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXTIMTM2->moduleQueried.moduleQueried.bits.all = false;
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXTIMTM2copy.
boolean SFE_UBLOX_GPS::setAutoTIMTM2callback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoTIMTM2(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXTIMTM2copy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXTIMTM2copy = new UBX_TIM_TM2_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXTIMTM2copy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoTIMTM2callback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXTIMTM2->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and VELNED is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoTIMTM2(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXTIMTM2 == NULL) initPacketUBXTIMTM2(); //Check that RAM has been allocated for the data
  if (packetUBXTIMTM2 == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXTIMTM2->automaticFlags.flags.bits.automatic != enabled || packetUBXTIMTM2->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXTIMTM2->automaticFlags.flags.bits.automatic = enabled;
    packetUBXTIMTM2->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXTIMTM2 and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXTIMTM2()
{
  packetUBXTIMTM2 = new UBX_TIM_TM2_t; //Allocate RAM for the main struct
  if (packetUBXTIMTM2 == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXTIMTM2: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXTIMTM2->automaticFlags.flags.all = 0;
  packetUBXTIMTM2->automaticFlags.callbackPointer = NULL;
  packetUBXTIMTM2->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushTIMTM2()
{
  if (packetUBXTIMTM2 == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXTIMTM2->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logTIMTM2(boolean enabled)
{
  if (packetUBXTIMTM2 == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXTIMTM2->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** ESF ALG automatic support

boolean SFE_UBLOX_GPS::getEsfAlignment(uint16_t maxWait)
{
  if (packetUBXESFALG == NULL) initPacketUBXESFALG(); //Check that RAM has been allocated for the ESF alignment data
  if (packetUBXESFALG == NULL) //Only attempt this if RAM allocation was successful
    return false;

  if (packetUBXESFALG->automaticFlags.flags.bits.automatic && packetUBXESFALG->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfAlignment: Autoreporting"));
    // }
    checkUbloxInternal(&packetCfg, UBX_CLASS_ESF, UBX_ESF_ALG);
    return packetUBXESFALG->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXESFALG->automaticFlags.flags.bits.automatic && !packetUBXESFALG->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfAlignment: Exit immediately"));
    // }
    return (false);
  }
  else
  {
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfAlignment: Polling"));
    // }

    //The GPS is not automatically reporting HNR PVT so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_ESF;
    packetCfg.id = UBX_ESF_ALG;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      // if (_printDebug == true)
      // {
      //   _debugSerial->println(F("getEsfAlignment: data in packetCfg was OVERWRITTEN by another message (but that's OK)"));
      // }
      return (true);
    }

    // if (_printDebug == true)
    // {
    //   _debugSerial->print(F("getEsfAlignment retVal: "));
    //   _debugSerial->println(statusString(retVal));
    // }
    return (false);
  }

  return (false); // Trap. We should never get here...
}

//Enable or disable automatic ESF ALG message generation by the GNSS. This changes the way getEsfAlignment
//works.
boolean SFE_UBLOX_GPS::setAutoESFALG(boolean enable, uint16_t maxWait)
{
  return setAutoESFALG(enable, true, maxWait);
}

//Enable or disable automatic ESF ALG message generation by the GNSS. This changes the way getEsfAlignment
//works.
boolean SFE_UBLOX_GPS::setAutoESFALG(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXESFALG == NULL) initPacketUBXESFALG(); //Check that RAM has been allocated for the data
  if (packetUBXESFALG == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_ESF;
  payloadCfg[1] = UBX_ESF_ALG;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXESFALG->automaticFlags.flags.bits.automatic = enable;
    packetUBXESFALG->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXESFALG->moduleQueried.moduleQueried.bits.all = false; // Mark data as stale
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXESFALGcopy.
boolean SFE_UBLOX_GPS::setAutoESFALGcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoESFALG(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXESFALGcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXESFALGcopy = new UBX_ESF_ALG_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXESFALGcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoESFALGcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXESFALG->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and ESF ALG is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoESFALG(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXESFALG == NULL) initPacketUBXESFALG(); //Check that RAM has been allocated for the ESF alignment data
  if (packetUBXESFALG == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXESFALG->automaticFlags.flags.bits.automatic != enabled || packetUBXESFALG->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXESFALG->automaticFlags.flags.bits.automatic = enabled;
    packetUBXESFALG->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXESFALG and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXESFALG()
{
  packetUBXESFALG = new UBX_ESF_ALG_t; //Allocate RAM for the main struct
  if (packetUBXESFALG == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXESFALG: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXESFALG->automaticFlags.flags.all = 0;
  packetUBXESFALG->automaticFlags.callbackPointer = NULL;
  packetUBXESFALG->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushESFALG()
{
  if (packetUBXESFALG == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXESFALG->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logESFALG(boolean enabled)
{
  if (packetUBXESFALG == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXESFALG->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** ESF STATUS automatic support

boolean SFE_UBLOX_GPS::getEsfInfo(uint16_t maxWait)
{
  if (packetUBXESFSTATUS == NULL) initPacketUBXESFSTATUS(); //Check that RAM has been allocated for the ESF status data
  if (packetUBXESFSTATUS == NULL) //Only attempt this if RAM allocation was successful
    return false;

  if (packetUBXESFSTATUS->automaticFlags.flags.bits.automatic && packetUBXESFSTATUS->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfInfo: Autoreporting"));
    // }
    checkUbloxInternal(&packetCfg, UBX_CLASS_ESF, UBX_ESF_STATUS);
    return packetUBXESFSTATUS->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXESFSTATUS->automaticFlags.flags.bits.automatic && !packetUBXESFSTATUS->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfInfo: Exit immediately"));
    // }
    return (false);
  }
  else
  {
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfInfo: Polling"));
    // }

    //The GPS is not automatically reporting HNR PVT so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_ESF;
    packetCfg.id = UBX_ESF_STATUS;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      // if (_printDebug == true)
      // {
      //   _debugSerial->println(F("getEsfInfo: data in packetCfg was OVERWRITTEN by another message (but that's OK)"));
      // }
      return (true);
    }

    // if (_printDebug == true)
    // {
    //   _debugSerial->print(F("getEsfInfo retVal: "));
    //   _debugSerial->println(statusString(retVal));
    // }
    return (false);
  }

  return (false); // Trap. We should never get here...
}

//Enable or disable automatic ESF STATUS message generation by the GNSS. This changes the way getESFInfo
//works.
boolean SFE_UBLOX_GPS::setAutoESFSTATUS(boolean enable, uint16_t maxWait)
{
  return setAutoESFSTATUS(enable, true, maxWait);
}

//Enable or disable automatic ESF STATUS message generation by the GNSS. This changes the way getESFInfo
//works.
boolean SFE_UBLOX_GPS::setAutoESFSTATUS(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXESFSTATUS == NULL) initPacketUBXESFSTATUS(); //Check that RAM has been allocated for the data
  if (packetUBXESFSTATUS == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_ESF;
  payloadCfg[1] = UBX_ESF_STATUS;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXESFSTATUS->automaticFlags.flags.bits.automatic = enable;
    packetUBXESFSTATUS->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXESFSTATUS->moduleQueried.moduleQueried.bits.all = false; // Mark data as stale
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXESFSTATUScopy.
boolean SFE_UBLOX_GPS::setAutoESFSTATUScallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoESFSTATUS(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXESFSTATUScopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXESFSTATUScopy = new UBX_ESF_STATUS_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXESFSTATUScopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoESFSTATUScallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXESFSTATUS->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and ESF STATUS is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoESFSTATUS(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXESFSTATUS == NULL) initPacketUBXESFSTATUS(); //Check that RAM has been allocated for the ESF status data
  if (packetUBXESFSTATUS == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXESFSTATUS->automaticFlags.flags.bits.automatic != enabled || packetUBXESFSTATUS->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXESFSTATUS->automaticFlags.flags.bits.automatic = enabled;
    packetUBXESFSTATUS->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXESFSTATUS and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXESFSTATUS()
{
  packetUBXESFSTATUS = new UBX_ESF_STATUS_t; //Allocate RAM for the main struct
  if (packetUBXESFSTATUS == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXESFSTATUS: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXESFSTATUS->automaticFlags.flags.all = 0;
  packetUBXNAVSTATUS->automaticFlags.callbackPointer = NULL;
  packetUBXESFSTATUS->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushESFSTATUS()
{
  if (packetUBXESFSTATUS == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXESFSTATUS->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logESFSTATUS(boolean enabled)
{
  if (packetUBXESFSTATUS == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXESFSTATUS->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** ESF INS automatic support

boolean SFE_UBLOX_GPS::getEsfIns(uint16_t maxWait)
{
  if (packetUBXESFINS == NULL) initPacketUBXESFINS(); //Check that RAM has been allocated for the ESF INS data
  if (packetUBXESFINS == NULL) //Only attempt this if RAM allocation was successful
    return false;

  if (packetUBXESFINS->automaticFlags.flags.bits.automatic && packetUBXESFINS->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfIns: Autoreporting"));
    // }
    checkUbloxInternal(&packetCfg, UBX_CLASS_ESF, UBX_ESF_INS);
    return packetUBXESFINS->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXESFINS->automaticFlags.flags.bits.automatic && !packetUBXESFINS->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfIns: Exit immediately"));
    // }
    return (false);
  }
  else
  {
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfIns: Polling"));
    // }

    //The GPS is not automatically reporting HNR PVT so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_ESF;
    packetCfg.id = UBX_ESF_INS;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      // if (_printDebug == true)
      // {
      //   _debugSerial->println(F("getEsfIns: data in packetCfg was OVERWRITTEN by another message (but that's OK)"));
      // }
      return (true);
    }

    // if (_printDebug == true)
    // {
    //   _debugSerial->print(F("getEsfIns retVal: "));
    //   _debugSerial->println(statusString(retVal));
    // }
    return (false);
  }

  return (false); // Trap. We should never get here...
}

//Enable or disable automatic ESF INS message generation by the GNSS. This changes the way getESFIns
//works.
boolean SFE_UBLOX_GPS::setAutoESFINS(boolean enable, uint16_t maxWait)
{
  return setAutoESFINS(enable, true, maxWait);
}

//Enable or disable automatic ESF INS message generation by the GNSS. This changes the way getESFIns
//works.
boolean SFE_UBLOX_GPS::setAutoESFINS(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXESFINS == NULL) initPacketUBXESFINS(); //Check that RAM has been allocated for the data
  if (packetUBXESFINS == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_ESF;
  payloadCfg[1] = UBX_ESF_INS;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXESFINS->automaticFlags.flags.bits.automatic = enable;
    packetUBXESFINS->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXESFINS->moduleQueried.moduleQueried.bits.all = false; // Mark data as stale
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXESFINScopy.
boolean SFE_UBLOX_GPS::setAutoESFINScallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoESFINS(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXESFINScopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXESFINScopy = new UBX_ESF_INS_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXESFINScopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoESFINScallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXESFINS->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and ESF INS is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoESFINS(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXESFINS == NULL) initPacketUBXESFINS(); //Check that RAM has been allocated for the ESF INS data
  if (packetUBXESFINS == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXESFINS->automaticFlags.flags.bits.automatic != enabled || packetUBXESFINS->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXESFINS->automaticFlags.flags.bits.automatic = enabled;
    packetUBXESFINS->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXESFINS and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXESFINS()
{
  packetUBXESFINS = new UBX_ESF_INS_t; //Allocate RAM for the main struct
  if (packetUBXESFINS == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXESFINS: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXESFINS->automaticFlags.flags.all = 0;
  packetUBXESFINS->automaticFlags.callbackPointer = NULL;
  packetUBXESFINS->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushESFINS()
{
  if (packetUBXESFINS == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXESFINS->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logESFINS(boolean enabled)
{
  if (packetUBXESFINS == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXESFINS->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** ESF MEAS automatic support

boolean SFE_UBLOX_GPS::getEsfDataInfo(uint16_t maxWait)
{
  if (packetUBXESFMEAS == NULL) initPacketUBXESFMEAS(); //Check that RAM has been allocated for the ESF MEAS data
  if (packetUBXESFMEAS == NULL) //Only attempt this if RAM allocation was successful
    return false;

  if (packetUBXESFMEAS->automaticFlags.flags.bits.automatic && packetUBXESFMEAS->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfDataInfo: Autoreporting"));
    // }
    checkUbloxInternal(&packetCfg, UBX_CLASS_ESF, UBX_ESF_MEAS);
    return packetUBXESFMEAS->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXESFMEAS->automaticFlags.flags.bits.automatic && !packetUBXESFMEAS->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfDataInfo: Exit immediately"));
    // }
    return (false);
  }
  else
  {
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfDataInfo: Polling"));
    // }

    //The GPS is not automatically reporting HNR PVT so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_ESF;
    packetCfg.id = UBX_ESF_MEAS;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      // if (_printDebug == true)
      // {
      //   _debugSerial->println(F("getEsfDataInfo: data in packetCfg was OVERWRITTEN by another message (but that's OK)"));
      // }
      return (true);
    }

    // if (_printDebug == true)
    // {
    //   _debugSerial->print(F("getEsfDataInfo retVal: "));
    //   _debugSerial->println(statusString(retVal));
    // }
    return (false);
  }

  return (false); // Trap. We should never get here...
}

//Enable or disable automatic ESF MEAS message generation by the GNSS. This changes the way getESFDataInfo
//works.
boolean SFE_UBLOX_GPS::setAutoESFMEAS(boolean enable, uint16_t maxWait)
{
  return setAutoESFMEAS(enable, true, maxWait);
}

//Enable or disable automatic ESF MEAS message generation by the GNSS. This changes the way getESFDataInfo
//works.
boolean SFE_UBLOX_GPS::setAutoESFMEAS(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXESFMEAS == NULL) initPacketUBXESFMEAS(); //Check that RAM has been allocated for the data
  if (packetUBXESFMEAS == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_ESF;
  payloadCfg[1] = UBX_ESF_MEAS;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXESFMEAS->automaticFlags.flags.bits.automatic = enable;
    packetUBXESFMEAS->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXESFMEAS->moduleQueried.moduleQueried.bits.all = false; // Mark data as stale
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXESFMEAScopy.
boolean SFE_UBLOX_GPS::setAutoESFMEAScallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoESFMEAS(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXESFMEAScopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXESFMEAScopy = new UBX_ESF_MEAS_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXESFMEAScopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoESFMEAScallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXESFMEAS->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and ESF MEAS is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoESFMEAS(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXESFMEAS == NULL) initPacketUBXESFMEAS(); //Check that RAM has been allocated for the ESF MEAS data
  if (packetUBXESFMEAS == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXESFMEAS->automaticFlags.flags.bits.automatic != enabled || packetUBXESFMEAS->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXESFMEAS->automaticFlags.flags.bits.automatic = enabled;
    packetUBXESFMEAS->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXESFMEAS and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXESFMEAS()
{
  packetUBXESFMEAS = new UBX_ESF_MEAS_t; //Allocate RAM for the main struct
  if (packetUBXESFMEAS == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXESFMEAS: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXESFMEAS->automaticFlags.flags.all = 0;
  packetUBXESFMEAS->automaticFlags.callbackPointer = NULL;
  packetUBXESFMEAS->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushESFMEAS()
{
  if (packetUBXESFMEAS == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXESFMEAS->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logESFMEAS(boolean enabled)
{
  if (packetUBXESFMEAS == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXESFMEAS->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** ESF RAW automatic support

boolean SFE_UBLOX_GPS::getEsfRawDataInfo(uint16_t maxWait)
{
  if (packetUBXESFRAW == NULL) initPacketUBXESFRAW(); //Check that RAM has been allocated for the ESF RAW data
  if (packetUBXESFRAW == NULL) //Only attempt this if RAM allocation was successful
    return false;

  if (packetUBXESFRAW->automaticFlags.flags.bits.automatic && packetUBXESFRAW->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfRawDataInfo: Autoreporting"));
    // }
    checkUbloxInternal(&packetCfg, UBX_CLASS_ESF, UBX_ESF_RAW);
    return packetUBXESFRAW->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXESFRAW->automaticFlags.flags.bits.automatic && !packetUBXESFRAW->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfRawDataInfo: Exit immediately"));
    // }
    return (false);
  }
  else
  {
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getEsfRawDataInfo: Polling"));
    // }

    //The GPS is not automatically reporting HNR PVT so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_ESF;
    packetCfg.id = UBX_ESF_RAW;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      // if (_printDebug == true)
      // {
      //   _debugSerial->println(F("getEsfRawDataInfo: data in packetCfg was OVERWRITTEN by another message (but that's OK)"));
      // }
      return (true);
    }

    // if (_printDebug == true)
    // {
    //   _debugSerial->print(F("getEsfRawDataInfo retVal: "));
    //   _debugSerial->println(statusString(retVal));
    // }
    return (false);
  }

  return (false); // Trap. We should never get here...
}

//Enable or disable automatic ESF RAW message generation by the GNSS. This changes the way getESFRawDataInfo
//works.
boolean SFE_UBLOX_GPS::setAutoESFRAW(boolean enable, uint16_t maxWait)
{
  return setAutoESFRAW(enable, true, maxWait);
}

//Enable or disable automatic ESF RAW message generation by the GNSS. This changes the way getESFRawDataInfo
//works.
boolean SFE_UBLOX_GPS::setAutoESFRAW(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXESFRAW == NULL) initPacketUBXESFRAW(); //Check that RAM has been allocated for the data
  if (packetUBXESFRAW == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_ESF;
  payloadCfg[1] = UBX_ESF_RAW;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXESFRAW->automaticFlags.flags.bits.automatic = enable;
    packetUBXESFRAW->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXESFRAW->moduleQueried.moduleQueried.bits.all = false; // Mark data as stale
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXESFRAWcopy.
boolean SFE_UBLOX_GPS::setAutoESFRAWcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoESFRAW(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXESFRAWcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXESFRAWcopy = new UBX_ESF_RAW_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXESFRAWcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoESFRAWcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXESFRAW->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and ESF RAW is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoESFRAW(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXESFRAW == NULL) initPacketUBXESFRAW(); //Check that RAM has been allocated for the ESF RAW data
  if (packetUBXESFRAW == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXESFRAW->automaticFlags.flags.bits.automatic != enabled || packetUBXESFRAW->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXESFRAW->automaticFlags.flags.bits.automatic = enabled;
    packetUBXESFRAW->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXESFRAW and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXESFRAW()
{
  packetUBXESFRAW = new UBX_ESF_RAW_t; //Allocate RAM for the main struct
  if (packetUBXESFRAW == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXESFRAW: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXESFRAW->automaticFlags.flags.all = 0;
  packetUBXESFRAW->automaticFlags.callbackPointer = NULL;
  packetUBXESFRAW->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushESFRAW()
{
  if (packetUBXESFRAW == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXESFRAW->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logESFRAW(boolean enabled)
{
  if (packetUBXESFRAW == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXESFRAW->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** HNR ATT automatic support

//Get the HNR Attitude data
// Returns true if the get HNR attitude is successful. Data is returned in hnrAtt
// Note: if hnrAttQueried is true, it gets set to false by this function since we assume
//       that the user will read hnrAtt immediately after this. I.e. this function will
//       only return true _once_ after each auto HNR Att is processed
boolean SFE_UBLOX_GPS::getHNRAtt(uint16_t maxWait)
{
  if (packetUBXHNRATT == NULL) initPacketUBXHNRATT(); //Check that RAM has been allocated for the data
  if (packetUBXHNRATT == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXHNRATT->automaticFlags.flags.bits.automatic && packetUBXHNRATT->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getHNRAtt: Autoreporting"));
    // }
    checkUbloxInternal(&packetCfg, UBX_CLASS_HNR, UBX_HNR_ATT);
    return packetUBXHNRATT->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXHNRATT->automaticFlags.flags.bits.automatic && !packetUBXHNRATT->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getHNRAtt: Exit immediately"));
    // }
    return (false);
  }
  else
  {
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getHNRAtt: Polling"));
    // }

    //The GPS is not automatically reporting HNR attitude so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_HNR;
    packetCfg.id = UBX_HNR_ATT;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      // if (_printDebug == true)
      // {
      //   _debugSerial->println(F("getHNRAtt: data in packetCfg was OVERWRITTEN by another message (but that's OK)"));
      // }
      return (true);
    }

    // if (_printDebug == true)
    // {
    //   _debugSerial->print(F("getHNRAtt retVal: "));
    //   _debugSerial->println(statusString(retVal));
    // }
    return (false);
  }

  return (false); // Trap. We should never get here...
}

//Enable or disable automatic HNR attitude message generation by the GNSS. This changes the way getHNRAtt
//works.
boolean SFE_UBLOX_GPS::setAutoHNRAtt(boolean enable, uint16_t maxWait)
{
  return setAutoHNRAtt(enable, true, maxWait);
}

//Enable or disable automatic HNR attitude message generation by the GNSS. This changes the way getHNRAtt
//works.
boolean SFE_UBLOX_GPS::setAutoHNRAtt(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXHNRATT == NULL) initPacketUBXHNRATT(); //Check that RAM has been allocated for the data
  if (packetUBXHNRATT == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_HNR;
  payloadCfg[1] = UBX_HNR_ATT;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXHNRATT->automaticFlags.flags.bits.automatic = enable;
    packetUBXHNRATT->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXHNRATT->moduleQueried.moduleQueried.bits.all = false; // Mark data as stale
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXHNRATTcopy.
boolean SFE_UBLOX_GPS::setAutoHNRAttcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoHNRAtt(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXHNRATTcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXHNRATTcopy = new UBX_HNR_ATT_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXHNRATTcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoHNRAttcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXHNRATT->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and HNR attitude is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoHNRAtt(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXHNRATT == NULL) initPacketUBXHNRATT(); //Check that RAM has been allocated for the data
  if (packetUBXHNRATT == NULL) //Bail if the RAM allocation failed
    return (false);

  boolean changes = packetUBXHNRATT->automaticFlags.flags.bits.automatic != enabled || packetUBXHNRATT->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXHNRATT->automaticFlags.flags.bits.automatic = enabled;
    packetUBXHNRATT->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXHNRATT and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXHNRATT()
{
  packetUBXHNRATT = new UBX_HNR_ATT_t; //Allocate RAM for the main struct
  if (packetUBXHNRATT == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXHNRATT: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXHNRATT->automaticFlags.flags.all = 0;
  packetUBXHNRATT->automaticFlags.callbackPointer = NULL;
  packetUBXHNRATT->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushHNRATT()
{
  if (packetUBXHNRATT == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXHNRATT->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}


//Log this data in file buffer
void SFE_UBLOX_GPS::logHNRATT(boolean enabled)
{
  if (packetUBXHNRATT == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXHNRATT->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** HNR DYN automatic support

//Get the HNR vehicle dynamics data
// Returns true if the get HNR vehicle dynamics is successful. Data is returned in hnrVehDyn
// Note: if hnrDynQueried is true, it gets set to false by this function since we assume
//       that the user will read hnrVehDyn immediately after this. I.e. this function will
//       only return true _once_ after each auto HNR Dyn is processed
boolean SFE_UBLOX_GPS::getHNRDyn(uint16_t maxWait)
{
  if (packetUBXHNRINS == NULL) initPacketUBXHNRINS(); //Check that RAM has been allocated for the data
  if (packetUBXHNRINS == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXHNRINS->automaticFlags.flags.bits.automatic && packetUBXHNRINS->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getHNRDyn: Autoreporting"));
    // }
    checkUbloxInternal(&packetCfg, UBX_CLASS_HNR, UBX_HNR_INS);
    return packetUBXHNRINS->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXHNRINS->automaticFlags.flags.bits.automatic && !packetUBXHNRINS->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getHNRDyn: Exit immediately"));
    // }
    return (false);
  }
  else
  {
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getHNRDyn: Polling"));
    // }

    //The GPS is not automatically reporting HNR vehicle dynamics so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_HNR;
    packetCfg.id = UBX_HNR_INS;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      // if (_printDebug == true)
      // {
      //   _debugSerial->println(F("getHNRDyn: data in packetCfg was OVERWRITTEN by another message (but that's OK)"));
      // }
      return (true);
    }

    // if (_printDebug == true)
    // {
    //   _debugSerial->print(F("getHNRDyn retVal: "));
    //   _debugSerial->println(statusString(retVal));
    // }
    return (false);
  }

  return (false); // Trap. We should never get here...
}

//Enable or disable automatic HNR vehicle dynamics message generation by the GNSS. This changes the way getHNRDyn
//works.
boolean SFE_UBLOX_GPS::setAutoHNRDyn(boolean enable, uint16_t maxWait)
{
  return setAutoHNRDyn(enable, true, maxWait);
}

//Enable or disable automatic HNR vehicle dynamics message generation by the GNSS. This changes the way getHNRDyn
//works.
boolean SFE_UBLOX_GPS::setAutoHNRDyn(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXHNRINS == NULL) initPacketUBXHNRINS(); //Check that RAM has been allocated for the data
  if (packetUBXHNRINS == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_HNR;
  payloadCfg[1] = UBX_HNR_INS;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXHNRINS->automaticFlags.flags.bits.automatic = enable;
    packetUBXHNRINS->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXHNRINS->moduleQueried.moduleQueried.bits.all = false; // Mark data as stale
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXHNRINScopy.
boolean SFE_UBLOX_GPS::setAutoHNRDyncallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoHNRDyn(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXHNRINScopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXHNRINScopy = new UBX_HNR_INS_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXHNRINScopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoHNRDyncallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXHNRINS->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and HNR vehicle dynamics is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoHNRDyn(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXHNRINS == NULL) initPacketUBXHNRINS(); //Check that RAM has been allocated for the data
  if (packetUBXHNRINS == NULL) //Bail if the RAM allocation failed
    return (false);

  boolean changes = packetUBXHNRINS->automaticFlags.flags.bits.automatic != enabled || packetUBXHNRINS->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXHNRINS->automaticFlags.flags.bits.automatic = enabled;
    packetUBXHNRINS->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXHNRINS and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXHNRINS()
{
  packetUBXHNRINS = new UBX_HNR_INS_t; //Allocate RAM for the main struct
  if (packetUBXHNRINS == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXHNRINS: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXHNRINS->automaticFlags.flags.all = 0;
  packetUBXHNRINS->automaticFlags.callbackPointer = NULL;
  packetUBXHNRINS->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushHNRINS()
{
  if (packetUBXHNRINS == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXHNRINS->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logHNRINS(boolean enabled)
{
  if (packetUBXHNRINS == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXHNRINS->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** HNR PVT automatic support

//Get the HNR PVT data
// Returns true if the get HNR PVT is successful. Data is returned in hnrPVT
// Note: if hnrPVTQueried is true, it gets set to false by this function since we assume
//       that the user will read hnrPVT immediately after this. I.e. this function will
//       only return true _once_ after each auto HNR PVT is processed
boolean SFE_UBLOX_GPS::getHNRPVT(uint16_t maxWait)
{
  if (packetUBXHNRPVT == NULL) initPacketUBXHNRPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXHNRPVT == NULL) //Only attempt this if RAM allocation was successful
    return false;

  if (packetUBXHNRPVT->automaticFlags.flags.bits.automatic && packetUBXHNRPVT->automaticFlags.flags.bits.implicitUpdate)
  {
    //The GPS is automatically reporting, we just check whether we got unread data
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getHNRPVT: Autoreporting"));
    // }
    checkUbloxInternal(&packetCfg, UBX_CLASS_HNR, UBX_HNR_PVT);
    return packetUBXHNRPVT->moduleQueried.moduleQueried.bits.all;
  }
  else if (packetUBXHNRPVT->automaticFlags.flags.bits.automatic && !packetUBXHNRPVT->automaticFlags.flags.bits.implicitUpdate)
  {
    //Someone else has to call checkUblox for us...
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getHNRPVT: Exit immediately"));
    // }
    return (false);
  }
  else
  {
    // if (_printDebug == true)
    // {
    //   _debugSerial->println(F("getHNRPVT: Polling"));
    // }

    //The GPS is not automatically reporting HNR PVT so we have to poll explicitly
    packetCfg.cls = UBX_CLASS_HNR;
    packetCfg.id = UBX_HNR_PVT;
    packetCfg.len = 0;
    packetCfg.startingSpot = 0;

    //The data is parsed as part of processing the response
    sfe_ublox_status_e retVal = sendCommand(&packetCfg, maxWait);

    if (retVal == SFE_UBLOX_STATUS_DATA_RECEIVED)
      return (true);

    if (retVal == SFE_UBLOX_STATUS_DATA_OVERWRITTEN)
    {
      // if (_printDebug == true)
      // {
      //   _debugSerial->println(F("getHNRPVT: data in packetCfg was OVERWRITTEN by another message (but that's OK)"));
      // }
      return (true);
    }

    // if (_printDebug == true)
    // {
    //   _debugSerial->print(F("getHNRPVT retVal: "));
    //   _debugSerial->println(statusString(retVal));
    // }
    return (false);
  }

  return (false); // Trap. We should never get here...
}

//Enable or disable automatic HNR PVT message generation by the GNSS. This changes the way getHNRPVT
//works.
boolean SFE_UBLOX_GPS::setAutoHNRPVT(boolean enable, uint16_t maxWait)
{
  return setAutoHNRPVT(enable, true, maxWait);
}

//Enable or disable automatic HNR PVT message generation by the GNSS. This changes the way getHNRPVT
//works.
boolean SFE_UBLOX_GPS::setAutoHNRPVT(boolean enable, boolean implicitUpdate, uint16_t maxWait)
{
  if (packetUBXHNRPVT == NULL) initPacketUBXHNRPVT(); //Check that RAM has been allocated for the data
  if (packetUBXHNRPVT == NULL) //Only attempt this if RAM allocation was successful
    return false;

  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_MSG;
  packetCfg.len = 3;
  packetCfg.startingSpot = 0;
  payloadCfg[0] = UBX_CLASS_HNR;
  payloadCfg[1] = UBX_HNR_PVT;
  payloadCfg[2] = enable ? 1 : 0; // rate relative to navigation freq.

  boolean ok = ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
  if (ok)
  {
    packetUBXHNRPVT->automaticFlags.flags.bits.automatic = enable;
    packetUBXHNRPVT->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  packetUBXHNRPVT->moduleQueried.moduleQueried.bits.all = false; // Mark data as stale
  return ok;
}

//Enable automatic navigation message generation by the GNSS.
//Data is passed to the callback in packetUBXHNRPVTcopy.
boolean SFE_UBLOX_GPS::setAutoHNRPVTcallback(void (*callbackPointer)(), uint16_t maxWait)
{
  // Enable auto messages. Set implicitUpdate to false as we expect the user to call checkUblox manually.
  boolean result = setAutoHNRPVT(true, false, maxWait);
  if (!result)
    return (result); // Bail if setAuto failed

  if (packetUBXHNRPVTcopy == NULL) //Check if RAM has been allocated for the callback copy
  {
    packetUBXHNRPVTcopy = new UBX_HNR_PVT_data_t; //Allocate RAM for the main struct
  }

  if (packetUBXHNRPVTcopy == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("setAutoHNRPVTcallback: PANIC! RAM allocation failed!"));
    return (false);
  }

  packetUBXHNRPVT->automaticFlags.callbackPointer = callbackPointer;
  return (true);
}

//In case no config access to the GNSS is possible and HNR PVT is send cyclically already
//set config to suitable parameters
boolean SFE_UBLOX_GPS::assumeAutoHNRPVT(boolean enabled, boolean implicitUpdate)
{
  if (packetUBXHNRPVT == NULL) initPacketUBXHNRPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXHNRPVT == NULL) //Only attempt this if RAM allocation was successful
    return false;

  boolean changes = packetUBXHNRPVT->automaticFlags.flags.bits.automatic != enabled || packetUBXHNRPVT->automaticFlags.flags.bits.implicitUpdate != implicitUpdate;
  if (changes)
  {
    packetUBXHNRPVT->automaticFlags.flags.bits.automatic = enabled;
    packetUBXHNRPVT->automaticFlags.flags.bits.implicitUpdate = implicitUpdate;
  }
  return changes;
}

// Allocate RAM for packetUBXHNRPVT and initialize it
boolean SFE_UBLOX_GPS::initPacketUBXHNRPVT()
{
  packetUBXHNRPVT = new UBX_HNR_PVT_t; //Allocate RAM for the main struct
  if (packetUBXHNRPVT == NULL)
  {
    if ((_printDebug == true) || (_printLimitedDebug == true)) // This is important. Print this if doing limited debugging
      _debugSerial->println(F("initPacketUBXHNRPVT: PANIC! RAM allocation failed! This will end _very_ badly..."));
    return (false);
  }
  packetUBXHNRPVT->automaticFlags.flags.all = 0;
  packetUBXHNRPVT->automaticFlags.callbackPointer = NULL;
  packetUBXHNRPVT->moduleQueried.moduleQueried.all = 0;
  return (true);
}

//Mark all the data as read/stale
void SFE_UBLOX_GPS::flushHNRPVT()
{
  if (packetUBXHNRPVT == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXHNRPVT->moduleQueried.moduleQueried.all = 0; //Mark all datums as stale (read before)
}

//Log this data in file buffer
void SFE_UBLOX_GPS::logHNRPVT(boolean enabled)
{
  if (packetUBXHNRPVT == NULL) return; // Bail if RAM has not been allocated (otherwise we could be writing anywhere!)
  packetUBXHNRPVT->automaticFlags.flags.bits.addToFileBuffer = (uint8_t)enabled;
}

// ***** CFG RATE Helper Functions

//Set the rate at which the module will give us an updated navigation solution
//Expects a number that is the updates per second. For example 1 = 1Hz, 2 = 2Hz, etc.
//Max is 40Hz(?!)
boolean SFE_UBLOX_GPS::setNavigationFrequency(uint8_t navFreq, uint16_t maxWait)
{
  //if(updateRate > 40) updateRate = 40; //Not needed: module will correct out of bounds values

  //Adjust the I2C polling timeout based on update rate
  i2cPollingWait = 1000 / (((int)navFreq) * 4); //This is the number of ms to wait between checks for new I2C data

  //Query the module for the latest lat/long
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_RATE;
  packetCfg.len = 0;
  packetCfg.startingSpot = 0;

  //This will load the payloadCfg array with current settings of the given register
  if (sendCommand(&packetCfg, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED) // We are expecting data and an ACK
    return (false);                                                       //If command send fails then bail

  uint16_t measurementRate = 1000 / navFreq;

  //payloadCfg is now loaded with current bytes. Change only the ones we need to
  payloadCfg[0] = measurementRate & 0xFF; //measRate LSB
  payloadCfg[1] = measurementRate >> 8;   //measRate MSB

  return ((sendCommand(&packetCfg, maxWait)) == SFE_UBLOX_STATUS_DATA_SENT); // We are only expecting an ACK
}

//Get the rate at which the module is outputting nav solutions
uint8_t SFE_UBLOX_GPS::getNavigationFrequency(uint16_t maxWait)
{
  if (packetUBXCFGRATE == NULL) initPacketUBXCFGRATE(); //Check that RAM has been allocated for the RATE data
  if (packetUBXCFGRATE == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXCFGRATE->moduleQueried.moduleQueried.bits.measRate == false)
    getNavigationFrequencyInternal(maxWait);
  packetUBXCFGRATE->moduleQueried.moduleQueried.bits.measRate = false; //Since we are about to give this to user, mark this data as stale
  packetUBXCFGRATE->moduleQueried.moduleQueried.bits.all = false;

  uint16_t measurementRate = packetUBXCFGRATE->data.measRate;

  measurementRate = 1000 / measurementRate; //This may return an int when it's a float, but I'd rather not return 4 bytes
  return (measurementRate);
}

// ***** DOP Helper Functions

uint16_t SFE_UBLOX_GPS::getGeometricDOP(uint16_t maxWait)
{
  if (packetUBXNAVDOP == NULL) initPacketUBXNAVDOP(); //Check that RAM has been allocated for the DOP data
  if (packetUBXNAVDOP == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVDOP->moduleQueried.moduleQueried.bits.gDOP == false)
    getDOP(maxWait);
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.gDOP = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVDOP->data.gDOP);
}

uint16_t SFE_UBLOX_GPS::getPositionDOP(uint16_t maxWait)
{
  if (packetUBXNAVDOP == NULL) initPacketUBXNAVDOP(); //Check that RAM has been allocated for the DOP data
  if (packetUBXNAVDOP == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVDOP->moduleQueried.moduleQueried.bits.pDOP == false)
    getDOP(maxWait);
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.pDOP = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVDOP->data.pDOP);
}

uint16_t SFE_UBLOX_GPS::getTimeDOP(uint16_t maxWait)
{
  if (packetUBXNAVDOP == NULL) initPacketUBXNAVDOP(); //Check that RAM has been allocated for the DOP data
  if (packetUBXNAVDOP == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVDOP->moduleQueried.moduleQueried.bits.tDOP == false)
    getDOP(maxWait);
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.tDOP = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVDOP->data.tDOP);
}

uint16_t SFE_UBLOX_GPS::getVerticalDOP(uint16_t maxWait)
{
  if (packetUBXNAVDOP == NULL) initPacketUBXNAVDOP(); //Check that RAM has been allocated for the DOP data
  if (packetUBXNAVDOP == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVDOP->moduleQueried.moduleQueried.bits.vDOP == false)
    getDOP(maxWait);
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.vDOP = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVDOP->data.vDOP);
}

uint16_t SFE_UBLOX_GPS::getHorizontalDOP(uint16_t maxWait)
{
  if (packetUBXNAVDOP == NULL) initPacketUBXNAVDOP(); //Check that RAM has been allocated for the DOP data
  if (packetUBXNAVDOP == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVDOP->moduleQueried.moduleQueried.bits.hDOP == false)
    getDOP(maxWait);
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.hDOP = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVDOP->data.hDOP);
}

uint16_t SFE_UBLOX_GPS::getNorthingDOP(uint16_t maxWait)
{
  if (packetUBXNAVDOP == NULL) initPacketUBXNAVDOP(); //Check that RAM has been allocated for the DOP data
  if (packetUBXNAVDOP == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVDOP->moduleQueried.moduleQueried.bits.nDOP == false)
    getDOP(maxWait);
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.nDOP = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVDOP->data.nDOP);
}

uint16_t SFE_UBLOX_GPS::getEastingDOP(uint16_t maxWait)
{
  if (packetUBXNAVDOP == NULL) initPacketUBXNAVDOP(); //Check that RAM has been allocated for the DOP data
  if (packetUBXNAVDOP == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVDOP->moduleQueried.moduleQueried.bits.eDOP == false)
    getDOP(maxWait);
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.eDOP = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVDOP->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVDOP->data.eDOP);
}

// ***** PVT Helper Functions

uint32_t SFE_UBLOX_GPS::getTimeOfWeek(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.iTOW == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.iTOW = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.iTOW);
}

//Get the current year
uint16_t SFE_UBLOX_GPS::getYear(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.year == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.year = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.year);
}

//Get the current month
uint8_t SFE_UBLOX_GPS::getMonth(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.month == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.month = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.month);
}

//Get the current day
uint8_t SFE_UBLOX_GPS::getDay(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.day == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.day = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.day);
}

//Get the current hour
uint8_t SFE_UBLOX_GPS::getHour(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.hour == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.hour = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.hour);
}

//Get the current minute
uint8_t SFE_UBLOX_GPS::getMinute(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.min == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.min = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.min);
}

//Get the current second
uint8_t SFE_UBLOX_GPS::getSecond(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.sec == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.sec = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.sec);
}

//Get the current millisecond
uint16_t SFE_UBLOX_GPS::getMillisecond(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.iTOW == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.iTOW = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.iTOW % 1000);
}

//Get the current nanoseconds - includes milliseconds
int32_t SFE_UBLOX_GPS::getNanosecond(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.nano == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.nano = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.nano);
}

//Get the current date validity
bool SFE_UBLOX_GPS::getDateValid(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.validDate == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.validDate = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return ((bool)packetUBXNAVPVT->data.valid.bits.validDate);
}

//Get the current time validity
bool SFE_UBLOX_GPS::getTimeValid(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.validTime == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.validTime = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return ((bool)packetUBXNAVPVT->data.valid.bits.validTime);
}

//Get the current fix type
//0=no fix, 1=dead reckoning, 2=2D, 3=3D, 4=GNSS, 5=Time fix
uint8_t SFE_UBLOX_GPS::getFixType(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.fixType == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.fixType = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.fixType);
}

//Get whether we have a valid fix (i.e within DOP & accuracy masks)
bool SFE_UBLOX_GPS::getGnssFixOk(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.gnssFixOK == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.gnssFixOK = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.flags.bits.gnssFixOK);
}

//Get whether differential corrections were applied
bool SFE_UBLOX_GPS::getDiffSoln(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.diffSoln == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.diffSoln = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.flags.bits.diffSoln);
}

//Get whether head vehicle valid or not
bool SFE_UBLOX_GPS::getHeadVehValid(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.headVehValid == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.headVehValid = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.flags.bits.headVehValid);
}

//Get the carrier phase range solution status
//Useful when querying module to see if it has high-precision RTK fix
//0=No solution, 1=Float solution, 2=Fixed solution
uint8_t SFE_UBLOX_GPS::getCarrierSolutionType(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.carrSoln == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.carrSoln = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.flags.bits.carrSoln);
}

//Get the number of satellites used in fix
uint8_t SFE_UBLOX_GPS::getSIV(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.numSV == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.numSV = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.numSV);
}

//Get the current longitude in degrees
//Returns a long representing the number of degrees *10^-7
int32_t SFE_UBLOX_GPS::getLongitude(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.lon == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.lon = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.lon);
}

//Get the current latitude in degrees
//Returns a long representing the number of degrees *10^-7
int32_t SFE_UBLOX_GPS::getLatitude(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.lat == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.lat = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.lat);
}

//Get the current altitude in mm according to ellipsoid model
int32_t SFE_UBLOX_GPS::getAltitude(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.height == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.height = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.height);
}

//Get the current altitude in mm according to mean sea level
//Ellipsoid model: https://www.esri.com/news/arcuser/0703/geoid1of3.html
//Difference between Ellipsoid Model and Mean Sea Level: https://eos-gnss.com/elevation-for-beginners/
int32_t SFE_UBLOX_GPS::getAltitudeMSL(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.hMSL == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.hMSL = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.hMSL);
}

int32_t SFE_UBLOX_GPS::getHorizontalAccEst(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.hAcc == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.hAcc = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.hAcc);
}

int32_t SFE_UBLOX_GPS::getVerticalAccEst(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.vAcc == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.vAcc = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.vAcc);
}

int32_t SFE_UBLOX_GPS::getNedNorthVel(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.velN == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.velN = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.velN);
}

int32_t SFE_UBLOX_GPS::getNedEastVel(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.velE == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.velE = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.velE);
}

int32_t SFE_UBLOX_GPS::getNedDownVel(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.velD == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.velD = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.velD);
}

//Get the ground speed in mm/s
int32_t SFE_UBLOX_GPS::getGroundSpeed(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.gSpeed == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.gSpeed = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.gSpeed);
}

//Get the heading of motion (as opposed to heading of car) in degrees * 10^-5
int32_t SFE_UBLOX_GPS::getHeading(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.headMot == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.headMot = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.headMot);
}

uint32_t SFE_UBLOX_GPS::getSpeedAccEst(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.sAcc == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.sAcc = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.sAcc);
}

uint32_t SFE_UBLOX_GPS::getHeadingAccEst(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.headAcc == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.headAcc = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.headAcc);
}

//Get the positional dillution of precision * 10^-2 (dimensionless)
uint16_t SFE_UBLOX_GPS::getPDOP(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.pDOP == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.pDOP = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.pDOP);
}

bool SFE_UBLOX_GPS::getInvalidLlh(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.invalidLlh == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.invalidLlh = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return ((bool)packetUBXNAVPVT->data.flags3.bits.invalidLlh);
}

int32_t SFE_UBLOX_GPS::getHeadVeh(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.headVeh == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.headVeh = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.headVeh);
}

int16_t SFE_UBLOX_GPS::getMagDec(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.magDec == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.magDec = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.magDec);
}

uint16_t SFE_UBLOX_GPS::getMagAcc(uint16_t maxWait)
{
  if (packetUBXNAVPVT == NULL) initPacketUBXNAVPVT(); //Check that RAM has been allocated for the PVT data
  if (packetUBXNAVPVT == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.magAcc == false)
    getPVT(maxWait);
  packetUBXNAVPVT->moduleQueried.moduleQueried2.bits.magAcc = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVPVT->moduleQueried.moduleQueried1.bits.all = false;
  return (packetUBXNAVPVT->data.magAcc);
}

// getGeoidSeparation is currently redundant. The geoid separation seems to only be provided in NMEA GGA and GNS messages.
int32_t SFE_UBLOX_GPS::getGeoidSeparation(uint16_t maxWait)
{
  return (0);
}

// ***** HPPOSECEF Helper Functions

//Get the current 3D high precision positional accuracy - a fun thing to watch
//Returns a long representing the 3D accuracy in millimeters
uint32_t SFE_UBLOX_GPS::getPositionAccuracy(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSECEF == NULL) initPacketUBXNAVHPPOSECEF(); //Check that RAM has been allocated for the HPPOSECEF data
  if (packetUBXNAVHPPOSECEF == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVHPPOSECEF->moduleQueried.moduleQueried.bits.pAcc == false)
    getNAVHPPOSECEF(maxWait);
  packetUBXNAVHPPOSECEF->moduleQueried.moduleQueried.bits.pAcc = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVHPPOSECEF->moduleQueried.moduleQueried.bits.all = false;

  uint32_t tempAccuracy = packetUBXNAVHPPOSECEF->data.pAcc;

  if ((tempAccuracy % 10) >= 5)
    tempAccuracy += 5; //Round fraction of mm up to next mm if .5 or above
  tempAccuracy /= 10;  //Convert 0.1mm units to mm

  return (tempAccuracy);
}

// ***** HPPOSLLH Helper Functions

uint32_t SFE_UBLOX_GPS::getTimeOfWeekFromHPPOSLLH(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the HPPOSLLH data
  if (packetUBXNAVHPPOSLLH == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.iTOW == false)
    getHPPOSLLH(maxWait);
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.iTOW = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVHPPOSLLH->data.iTOW);
}

int32_t SFE_UBLOX_GPS::getHighResLongitude(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the HPPOSLLH data
  if (packetUBXNAVHPPOSLLH == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.lon == false)
    getHPPOSLLH(maxWait);
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.lon = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVHPPOSLLH->data.lon);
}

int32_t SFE_UBLOX_GPS::getHighResLatitude(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the HPPOSLLH data
  if (packetUBXNAVHPPOSLLH == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.lat == false)
    getHPPOSLLH(maxWait);
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.lat = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVHPPOSLLH->data.lat);
}

int32_t SFE_UBLOX_GPS::getElipsoid(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the HPPOSLLH data
  if (packetUBXNAVHPPOSLLH == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.height == false)
    getHPPOSLLH(maxWait);
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.height = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVHPPOSLLH->data.height);
}

int32_t SFE_UBLOX_GPS::getMeanSeaLevel(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the HPPOSLLH data
  if (packetUBXNAVHPPOSLLH == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.hMSL == false)
    getHPPOSLLH(maxWait);
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.hMSL = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVHPPOSLLH->data.hMSL);
}

int8_t SFE_UBLOX_GPS::getHighResLongitudeHp(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the HPPOSLLH data
  if (packetUBXNAVHPPOSLLH == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.lonHp == false)
    getHPPOSLLH(maxWait);
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.lonHp = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVHPPOSLLH->data.lonHp);
}

int8_t SFE_UBLOX_GPS::getHighResLatitudeHp(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the HPPOSLLH data
  if (packetUBXNAVHPPOSLLH == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.latHp == false)
    getHPPOSLLH(maxWait);
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.latHp = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVHPPOSLLH->data.latHp);
}

int8_t SFE_UBLOX_GPS::getElipsoidHp(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the HPPOSLLH data
  if (packetUBXNAVHPPOSLLH == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.heightHp == false)
    getHPPOSLLH(maxWait);
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.heightHp = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVHPPOSLLH->data.heightHp);
}

int8_t SFE_UBLOX_GPS::getMeanSeaLevelHp(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the HPPOSLLH data
  if (packetUBXNAVHPPOSLLH == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.hMSLHp == false)
    getHPPOSLLH(maxWait);
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.hMSLHp = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVHPPOSLLH->data.hMSLHp);
}

uint32_t SFE_UBLOX_GPS::getHorizontalAccuracy(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the HPPOSLLH data
  if (packetUBXNAVHPPOSLLH == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.hAcc == false)
    getHPPOSLLH(maxWait);
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.hAcc = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVHPPOSLLH->data.hAcc);
}

uint32_t SFE_UBLOX_GPS::getVerticalAccuracy(uint16_t maxWait)
{
  if (packetUBXNAVHPPOSLLH == NULL) initPacketUBXNAVHPPOSLLH(); //Check that RAM has been allocated for the HPPOSLLH data
  if (packetUBXNAVHPPOSLLH == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.vAcc == false)
    getHPPOSLLH(maxWait);
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.vAcc = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVHPPOSLLH->moduleQueried.moduleQueried.bits.all = false;
  return (packetUBXNAVHPPOSLLH->data.vAcc);
}

// ***** SVIN Helper Functions

boolean SFE_UBLOX_GPS::getSurveyInActive(uint16_t maxWait)
{
  if (packetUBXNAVSVIN == NULL) initPacketUBXNAVSVIN(); //Check that RAM has been allocated for the SVIN data
  if (packetUBXNAVSVIN == NULL) //Bail if the RAM allocation failed
    return false;

  if (packetUBXNAVSVIN->moduleQueried.moduleQueried.bits.active == false)
    getSurveyStatus(maxWait);
  packetUBXNAVSVIN->moduleQueried.moduleQueried.bits.active = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVSVIN->moduleQueried.moduleQueried.bits.all = false;
  return ((boolean)packetUBXNAVSVIN->data.active);
}

boolean SFE_UBLOX_GPS::getSurveyInValid(uint16_t maxWait)
{
  if (packetUBXNAVSVIN == NULL) initPacketUBXNAVSVIN(); //Check that RAM has been allocated for the SVIN data
  if (packetUBXNAVSVIN == NULL) //Bail if the RAM allocation failed
    return false;

  if (packetUBXNAVSVIN->moduleQueried.moduleQueried.bits.valid == false)
    getSurveyStatus(maxWait);
  packetUBXNAVSVIN->moduleQueried.moduleQueried.bits.valid = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVSVIN->moduleQueried.moduleQueried.bits.all = false;
  return ((boolean)packetUBXNAVSVIN->data.valid);
}

uint16_t SFE_UBLOX_GPS::getSurveyInObservationTime(uint16_t maxWait) // Truncated to 65535 seconds
{
  if (packetUBXNAVSVIN == NULL) initPacketUBXNAVSVIN(); //Check that RAM has been allocated for the SVIN data
  if (packetUBXNAVSVIN == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVSVIN->moduleQueried.moduleQueried.bits.dur == false)
    getSurveyStatus(maxWait);
  packetUBXNAVSVIN->moduleQueried.moduleQueried.bits.dur = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVSVIN->moduleQueried.moduleQueried.bits.all = false;

  //dur (Passed survey-in observation time) is U4 (uint32_t) seconds. We truncate to 16 bits
  //(waiting more than 65535 seconds (18.2 hours) seems excessive!)
  uint32_t tmpObsTime = packetUBXNAVSVIN->data.dur;
  if (tmpObsTime <= 0xFFFF)
  {
    return((uint16_t)tmpObsTime);
  }
  else
  {
    return(0xFFFF);
  }
}

float SFE_UBLOX_GPS::getSurveyInMeanAccuracy(uint16_t maxWait) // Returned as m
{
  if (packetUBXNAVSVIN == NULL) initPacketUBXNAVSVIN(); //Check that RAM has been allocated for the SVIN data
  if (packetUBXNAVSVIN == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVSVIN->moduleQueried.moduleQueried.bits.meanAcc == false)
    getSurveyStatus(maxWait);
  packetUBXNAVSVIN->moduleQueried.moduleQueried.bits.meanAcc = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVSVIN->moduleQueried.moduleQueried.bits.all = false;

  // meanAcc is U4 (uint32_t) in 0.1mm. We convert this to float.
  uint32_t tempFloat = packetUBXNAVSVIN->data.dur;
  return (((float)tempFloat) / 10000.0); //Convert 0.1mm to m
}

// ***** RELPOSNED Helper Functions and automatic support

float SFE_UBLOX_GPS::getRelPosN(uint16_t maxWait) // Returned as m
{
  if (packetUBXNAVRELPOSNED == NULL) initPacketUBXNAVRELPOSNED(); //Check that RAM has been allocated for the RELPOSNED data
  if (packetUBXNAVRELPOSNED == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.relPosN == false)
    getRELPOSNED(maxWait);
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.relPosN = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.all = false;
  return (((float)packetUBXNAVRELPOSNED->data.relPosN) / 100.0); // Convert to m
}

float SFE_UBLOX_GPS::getRelPosE(uint16_t maxWait) // Returned as m
{
  if (packetUBXNAVRELPOSNED == NULL) initPacketUBXNAVRELPOSNED(); //Check that RAM has been allocated for the RELPOSNED data
  if (packetUBXNAVRELPOSNED == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.relPosE == false)
    getRELPOSNED(maxWait);
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.relPosE = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.all = false;
  return (((float)packetUBXNAVRELPOSNED->data.relPosE) / 100.0); // Convert to m
}

float SFE_UBLOX_GPS::getRelPosD(uint16_t maxWait) // Returned as m
{
  if (packetUBXNAVRELPOSNED == NULL) initPacketUBXNAVRELPOSNED(); //Check that RAM has been allocated for the RELPOSNED data
  if (packetUBXNAVRELPOSNED == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.relPosD == false)
    getRELPOSNED(maxWait);
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.relPosD = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.all = false;
  return (((float)packetUBXNAVRELPOSNED->data.relPosD) / 100.0); // Convert to m
}

float SFE_UBLOX_GPS::getRelPosAccN(uint16_t maxWait) // Returned as m
{
  if (packetUBXNAVRELPOSNED == NULL) initPacketUBXNAVRELPOSNED(); //Check that RAM has been allocated for the RELPOSNED data
  if (packetUBXNAVRELPOSNED == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.accN == false)
    getRELPOSNED(maxWait);
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.accN = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.all = false;
  return (((float)packetUBXNAVRELPOSNED->data.accN) / 10000.0); // Convert to m
}

float SFE_UBLOX_GPS::getRelPosAccE(uint16_t maxWait) // Returned as m
{
  if (packetUBXNAVRELPOSNED == NULL) initPacketUBXNAVRELPOSNED(); //Check that RAM has been allocated for the RELPOSNED data
  if (packetUBXNAVRELPOSNED == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.accE == false)
    getRELPOSNED(maxWait);
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.accE = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.all = false;
  return (((float)packetUBXNAVRELPOSNED->data.accE) / 10000.0); // Convert to m
}

float SFE_UBLOX_GPS::getRelPosAccD(uint16_t maxWait) // Returned as m
{
  if (packetUBXNAVRELPOSNED == NULL) initPacketUBXNAVRELPOSNED(); //Check that RAM has been allocated for the RELPOSNED data
  if (packetUBXNAVRELPOSNED == NULL) //Bail if the RAM allocation failed
    return 0;

  if (packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.accD == false)
    getRELPOSNED(maxWait);
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.accD = false; //Since we are about to give this to user, mark this data as stale
  packetUBXNAVRELPOSNED->moduleQueried.moduleQueried.bits.all = false;
  return (((float)packetUBXNAVRELPOSNED->data.accD) / 10000.0); // Convert to m
}

// ***** ESF Helper Functions

boolean SFE_UBLOX_GPS::getSensorFusionMeasurement(UBX_ESF_MEAS_sensorData_t *sensorData, uint8_t sensor, uint16_t maxWait)
{
  if (packetUBXESFMEAS == NULL) initPacketUBXESFMEAS(); //Check that RAM has been allocated for the ESF MEAS data
  if (packetUBXESFMEAS == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXESFMEAS->moduleQueried.moduleQueried.bits.data & (1 << sensor) == 0)
    getEsfDataInfo(maxWait);
  packetUBXESFMEAS->moduleQueried.moduleQueried.bits.data &= ~(1 << sensor); //Since we are about to give this to user, mark this data as stale
  packetUBXESFMEAS->moduleQueried.moduleQueried.bits.all = false;
  sensorData->data.all = packetUBXESFMEAS->data.data[sensor].data.all;
  return (true);
}

boolean SFE_UBLOX_GPS::getRawSensorMeasurement(UBX_ESF_RAW_sensorData_t *sensorData, uint8_t sensor, uint16_t maxWait)
{
  if (packetUBXESFRAW == NULL) initPacketUBXESFRAW(); //Check that RAM has been allocated for the ESF RAW data
  if (packetUBXESFRAW == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXESFRAW->moduleQueried.moduleQueried.bits.data & (1 << sensor) == 0)
    getEsfRawDataInfo(maxWait);
  packetUBXESFRAW->moduleQueried.moduleQueried.bits.data &= ~(1 << sensor); //Since we are about to give this to user, mark this data as stale
  packetUBXESFRAW->moduleQueried.moduleQueried.bits.all = false;
  sensorData->data.all = packetUBXESFRAW->data.data[sensor].data.all;
  sensorData->sTag = packetUBXESFRAW->data.data[sensor].sTag;
  return (true);
}

boolean SFE_UBLOX_GPS::getSensorFusionStatus(UBX_ESF_STATUS_sensorStatus_t *sensorStatus, uint8_t sensor, uint16_t maxWait)
{
  if (packetUBXESFSTATUS == NULL) initPacketUBXESFSTATUS(); //Check that RAM has been allocated for the ESF STATUS data
  if (packetUBXESFSTATUS == NULL) //Bail if the RAM allocation failed
    return (false);

  if (packetUBXESFSTATUS->moduleQueried.moduleQueried.bits.status & (1 << sensor) == 0)
    getEsfInfo(maxWait);
  packetUBXESFSTATUS->moduleQueried.moduleQueried.bits.status &= ~(1 << sensor); //Since we are about to give this to user, mark this data as stale
  packetUBXESFSTATUS->moduleQueried.moduleQueried.bits.all = false;
  sensorStatus->sensStatus1.all = packetUBXESFSTATUS->data.status[sensor].sensStatus1.all;
  sensorStatus->sensStatus2.all = packetUBXESFSTATUS->data.status[sensor].sensStatus2.all;
  sensorStatus->freq = packetUBXESFSTATUS->data.status[sensor].freq;
  sensorStatus->faults.all = packetUBXESFSTATUS->data.status[sensor].faults.all;
  return (true);
}

// ***** HNR Helper Functions

// Set the High Navigation Rate
// Returns true if the setHNRNavigationRate is successful
boolean SFE_UBLOX_GPS::setHNRNavigationRate(uint8_t rate, uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_HNR;
  packetCfg.len = 0;
  packetCfg.startingSpot = 0;

  //Ask module for the current HNR settings. Loads into payloadCfg.
  if (sendCommand(&packetCfg, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED)
    return (false);

  //Load the new navigation rate into payloadCfg
  payloadCfg[0] = rate;

  //Update the navigation rate
  sfe_ublox_status_e result = sendCommand(&packetCfg, maxWait); // We are only expecting an ACK

  //Adjust the I2C polling timeout based on update rate
  if (result == SFE_UBLOX_STATUS_DATA_SENT)
    i2cPollingWait = 1000 / (((int)rate) * 4); //This is the number of ms to wait between checks for new I2C data

  return (result == SFE_UBLOX_STATUS_DATA_SENT);
}

// Get the High Navigation Rate
// Returns 0 if the getHNRNavigationRate fails
uint8_t SFE_UBLOX_GPS::getHNRNavigationRate(uint16_t maxWait)
{
  packetCfg.cls = UBX_CLASS_CFG;
  packetCfg.id = UBX_CFG_HNR;
  packetCfg.len = 0;
  packetCfg.startingSpot = 0;

  //Ask module for the current HNR settings. Loads into payloadCfg.
  if (sendCommand(&packetCfg, maxWait) != SFE_UBLOX_STATUS_DATA_RECEIVED)
    return (0);

  //Return the navigation rate
  return (payloadCfg[0]);
}

// Functions to extract signed and unsigned 8/16/32-bit data from a ubxPacket
// From v2.0: These are public. The user can call these to extract data from custom packets

//Given a spot in the payload array, extract four bytes and build a long
uint32_t SFE_UBLOX_GPS::extractLong(ubxPacket *msg, uint8_t spotToStart)
{
  uint32_t val = 0;
  val |= (uint32_t)msg->payload[spotToStart + 0] << 8 * 0;
  val |= (uint32_t)msg->payload[spotToStart + 1] << 8 * 1;
  val |= (uint32_t)msg->payload[spotToStart + 2] << 8 * 2;
  val |= (uint32_t)msg->payload[spotToStart + 3] << 8 * 3;
  return (val);
}

//Just so there is no ambiguity about whether a uint32_t will cast to a int32_t correctly...
int32_t SFE_UBLOX_GPS::extractSignedLong(ubxPacket *msg, uint8_t spotToStart)
{
  union // Use a union to convert from uint32_t to int32_t
  {
      uint32_t unsignedLong;
      int32_t signedLong;
  } unsignedSigned;

  unsignedSigned.unsignedLong = extractLong(msg, spotToStart);
  return (unsignedSigned.signedLong);
}

//Given a spot in the payload array, extract two bytes and build an int
uint16_t SFE_UBLOX_GPS::extractInt(ubxPacket *msg, uint8_t spotToStart)
{
  uint16_t val = 0;
  val |= (uint16_t)msg->payload[spotToStart + 0] << 8 * 0;
  val |= (uint16_t)msg->payload[spotToStart + 1] << 8 * 1;
  return (val);
}

//Just so there is no ambiguity about whether a uint16_t will cast to a int16_t correctly...
int16_t SFE_UBLOX_GPS::extractSignedInt(ubxPacket *msg, int8_t spotToStart)
{
  union // Use a union to convert from uint16_t to int16_t
  {
      uint16_t unsignedInt;
      int16_t signedInt;
  } stSignedInt;

  stSignedInt.unsignedInt = extractInt(msg, spotToStart);
  return (stSignedInt.signedInt);
}

//Given a spot, extract a byte from the payload
uint8_t SFE_UBLOX_GPS::extractByte(ubxPacket *msg, uint8_t spotToStart)
{
  return (msg->payload[spotToStart]);
}

//Given a spot, extract a signed 8-bit value from the payload
int8_t SFE_UBLOX_GPS::extractSignedChar(ubxPacket *msg, uint8_t spotToStart)
{
  union // Use a union to convert from uint8_t to int8_t
  {
      uint8_t unsignedByte;
      int8_t signedByte;
  } stSignedByte;

  stSignedByte.unsignedByte = extractByte(msg, spotToStart);
  return (stSignedByte.signedByte);
}
