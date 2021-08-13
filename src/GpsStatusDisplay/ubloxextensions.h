#ifndef __ubloxextensions_h__
#define __ubloxextensions_h__

#define CFG_NMEA_PROTVER 0x20930001 // NMEA protocol version. 21 = v2.1, 23=v2.3, 40=v4.0, 41=v4.10 42=4.11
#define CFG_NMEA_PROTVER_V21 21 // NMEA protocol version 2.1
#define CFG_NMEA_PROTVER_V23 23 // NMEA protocol version 2.3
#define CFG_NMEA_PROTVER_V40 40 // NMEA protocol version 4.0
#define CFG_NMEA_PROTVER_V41 41 // NMEA protocol version 4.10
#define CFG_NMEA_PROTVER_V411 42 // NMEA protocol version 4.11

#define CFG_SBAS_USE_DIFFCORR 0x10360004

#define CFG_USBOUTPROT_NMEA 0x10780002 // Flag to indicate if NMEA should be an outputprotocol on USB
#define CFG_USBOUTPROT_RTCM3X 0x10780004 // Flag to indicate if RTCM3X should be an outputprotocol on USB
#define CFG_UART2OUTPROT_NMEA 0x10760002 // Flag to indicate if NMEA should be an outputprotocol on UART2
#define CFG_UART2OUTPROT_RTCM3X 0x10760004 // Flag to indicate if RTCM3X should be an outputprotocol on UART2

#define CFG_NMEA_HIGHPREC 0x10930006 // Enable high precision mode -- This flag cannot be set in conjunction with either CFG-NMEA-COMPAT or CFG-NMEA-LIMIT82 mode.
#define CFG_NMEA_COMPAT 0x10930003   // NMEA Compat mode
#define CFG_NMEA_LIMIT82 0x10930005  // Enable strict limit to 82 characters maximum NMEA message length

uint8_t customPayload[MAX_PAYLOAD_SIZE]; // This array holds the payload data bytes
ubxPacket customCfg = {0, 0, 0, 0, 0, customPayload, 0, 0, SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED, SFE_UBLOX_PACKET_VALIDITY_NOT_DEFINED};
struct minfoStructure // Structure to hold the module info (uses 341 bytes of RAM)
    {
        char swVersion[30];
        char hwVersion[10];
        uint8_t extensionNo = 0;
        char extension[10][30];
    } minfo;
    
bool sendCommand(SFE_UBLOX_GNSS *gps, uint8_t cls, uint8_t id, uint8_t len, uint8_t *payload)
{
  // Get USB Port settings:
  customCfg.cls = cls;  // This is the message Class
  customCfg.id = id;    // This is the message ID
  customCfg.len = len;  // Setting the len (length) to zero let's us poll the current settings
  customCfg.startingSpot = 0;  // Always set the startingSpot to zero (unless you really know what you are doing)
  for(int8_t i = 0;i<len;i++)
    customPayload[i] = payload[i];
  return gps->sendCommand(&customCfg, 1100);
}
/*
uint32_t getval32(int offset)
{
  return customPayload[offset] | (uint32_t)customPayload[offset+1] << 8 | (uint32_t)customPayload[offset+2] << 16 | (uint32_t)customPayload[offset+3] << 24;
}
uint8_t getval8(int offset)
{
  return customPayload[offset];
}
*/
boolean getModuleInfo(SFE_UBLOX_GNSS *gps, uint16_t maxWait)
{
    minfo.hwVersion[0] = 0;
    minfo.swVersion[0] = 0;
    for (int i = 0; i < 10; i++)
        minfo.extension[i][0] = 0;
    minfo.extensionNo = 0;

    // Now let's send the command. The module info is returned in customPayload

    if(sendCommand(gps, UBX_CLASS_MON, UBX_MON_VER, 0, nullptr) != SFE_UBLOX_STATUS_DATA_RECEIVED)
      return false; // If command send fails then bail

    // Now let's extract the module info from customPayload

    uint16_t position = 0;
    for (int i = 0; i < 30; i++)
    {
        minfo.swVersion[i] = customPayload[position];
        position++;
    }
    for (int i = 0; i < 10; i++)
    {
        minfo.hwVersion[i] = customPayload[position];
        position++;
    }

    while (customCfg.len >= position + 30)
    {
        for (int i = 0; i < 30; i++)
        {
            minfo.extension[minfo.extensionNo][i] = customPayload[position];
            position++;
        }
        minfo.extensionNo++;
        if (minfo.extensionNo > 9)
            break;
    }

    return (true); //Success!
}
#endif
