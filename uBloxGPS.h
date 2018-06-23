/*
uBloxGPS.h - Library for reading UBx NAV-PVT Message from UBlox GPSs
Created by Don Gibson on June 15th, 2018
SquirrelEngineering.com

Released into the public domain.
*/


#ifndef uBloxGPS_h
#define uBloxGPS_h

#include "Arduino.h"


// UBlox Standard Header
#define UBLOX_HEADER1 0xB5
#define UBLOX_HEADER2 0x62

// ID bytes for NAV-PVT
#define CLASSNAV 0x01;
#define NAMEPVT 0x07;

#define NAVPVT_LENGTH 84

// NAV-PVT Message payload
// Bit Masks for Bit flag fields.

#define VALID_DATE 0x01
#define VALID_TIME 0x02
#define VALID_FULLYRESOLVED 0x04

#define FLAGS_GNSSFIXOK 0x01
#define FLAGS_DIFFSOLN 0x02
#define FLAGS_PSMSTATE 0x01C   

#define FIXTYPE_NOFIX 0x00
#define FIXTYPE_DEADREC 0x01
#define FIXTYPE_2D 0x02
#define FIXTYPE_3D 0X03
#define FIXTYPE_GNSS_AND_DEADREC 0x04
#define FIXTYPE_TIMEONLY 0x05


struct NAVPVTMsg
{
	// Header Stuff
	uint8_t Class;  // Message Class
	uint8_t ID;		// Message ID
	uint16_t PayloadLength; // Length of payload 

	// START of Payload data
	uint32_t iTOW;  // GPS time of week (ms)
	uint16_t year;
	uint8_t month;
	uint8_t day;
	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t valid; // Valid status bits, see VALID defines above for bitmasks
	uint32_t tAcc; //Time accurace estimate UTC
	int32_t nano; // Fraction of a sec.
	uint8_t fixType; // Type of fix, see FIXTYPE defines above for values.
	uint8_t flags; // See FLAGS defines above for bitmasks
	uint8_t reserved1;
	uint8_t numSV; // # of Space vehicles (aka Sats)
	int32_t lon; // 1e-7 deg
	int32_t lat; // 1e-7 deg
	int32_t height; // Height above ellipsoid (mm)
	int32_t hMSL; // Height above mean sea level (mm)
	uint32_t hAcc; // Horz Accuracy estimate(mm)
	uint32_t vAcc; // Vert. Accuracy estimate (mm)
	int32_t ve1N; // NED North Velocity (mm/s)
	int32_t ve1E; // NED East Velocity (mm/s)
	int32_t ve1D; // NED down Velocity (mm/s)
	int32_t gSpeed; // ground speed (2-D) (mm/s)
	int32_t heading; // Heading of motion 2-D 1e-5 (deg)
	uint32_t sAcc; // Speed accuracy estimate (mm/s)
	uint32_t headingAcc; // Heading accuracy estimate (deg)
	uint16_t pDOP; //Position DOP
	uint16_t reserved2;
	uint32_t reserved3;
};


//
// Main class
//
class uBloxGPS
{
	// Member data
public:
	struct NAVPVTMsg PVT; // Last good PVT Message data. Safe to access.
	const int PVTPAYLOADLEN = 84;  // Fixed length of this Message message type.
	uint32_t FixNumber;   // Fix Serial number, bumps up by one on each fix.
	uint32_t MessageNumber; // Serial number of valid Messages (CRC Checksum is good), bumps up by one on each successfully decoded Message
	bool CRC_OK = false; // State of the CRC check. 
private:
	struct NAVPVTMsg WorkingPVT;  // Working structure. Not safe to access as it is changing during reception of data.
	int ParserState; // State of the Message parsing process. 0 = begining.
	int PayloadLength = 0; // The received payload length. Not really usefull for this Message type. (Should always be 84)

	uint32_t LastValidPositionAge = 0; //The time in millis() when the last valid position was updated.

	// Working vars
	int PayloadCount = 0;  // Count of bytes loded into payload so far.
	uint8_t *PayloadPtr;  // pointer to the position in the payload to next update.
	
	// Holding vars for length member.
	uint8_t lengthbyte1 = 0;
	uint8_t lengthbye2 = 0;

	// Receieved checksum bytes
	uint8_t CK_A = 0;
	uint8_t CK_B = 0;

	void(*PositionFixCallback)(uBloxGPS *);

//
// Methods
//
public:
	// ctor
	uBloxGPS();
	
	// Public Methods
	void FeedMe(char ch); // Send serial stream one character at a time to this function.
	bool IsValidFixPosition(); // Will be true if a the current PVT position data contains a valid position... 
							   // it may not be accurate, but it is a position
						       // Use other PVT data to assess the quality of the fix. (DPO, hAcc, vAcc, fixType..)
	uint32_t PositionFixAge(); // Return the # of Milis since the last good fix was reported.
	
	void SetPositionUpdateCallbackFunction(void(*PositionFixCallback)(uBloxGPS *));  // Set the call back function. Use NULL pointer to disable.

	// Private methods
private:
	void ParseNAV_PVTMessage(char ch);  // Message parser routine
	uint16_t CalcChecksum(uint8_t *buff, int len); // Calculate checksum value from a buffer + length.
};


#endif

