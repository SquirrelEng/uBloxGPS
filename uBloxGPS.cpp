/*
uBloxGPS.cpp - Library for reading UBx NAV-PVT Message from UBlox GPSs
Created by Don Gibson on June 15th, 2018
SquirrelEngineering.com

Released into the public domain.
*/

#include "Arduino.h"
#include "uBloxGPS.h"



// Class Constructor
uBloxGPS::uBloxGPS()
{
	FixNumber = 0;
	MessageNumber = 0;
	LastValidPositionAge = millis();
	ParserState = 0;
	WorkingPVT.Class = CLASSNAV;
	WorkingPVT.ID = NAMEPVT;
	PayloadPtr = NULL;
	CRC_OK = false;
	PositionFixCallback = NULL;
}


//
// Main function to feed serial data into
//
void uBloxGPS::FeedMe(uint8_t ch) // Send Serial stream to this function one char at a time
{
	
		ParseNAV_PVTMessage(ch);

}

//
// Does the PVT structure have a valid fix on the location?
//
bool uBloxGPS::IsValidFixPosition()
{
	bool brc = false;
	if (CRC_OK && PVT.fixType != FIXTYPE_NOFIX && PVT.fixType != FIXTYPE_TIMEONLY)
	{
		brc = true;
	}
	return brc;
}

//
// Age in millis since last successful fix.
//
uint32_t uBloxGPS::PositionFixAge()
{
	uint32_t rc = millis() - LastValidPositionAge;
	return rc;
}

//
// uBx Message parser for NAV-PVT message.
//
// This parser is given one character at a time from the GPS data stream. As the data is fed to this routine, it keep track of the "ParserState" of the
// decoding process. As each part and sub parts of the Message is decoded, the ParserState variable is incremented (i.e. enters the next state) to 
// indicate the parser should expet the next part of the message on the next recieved character.
// If unexpected data arrives, then the ParserState is reset to 0 (Ready for the start of next Message).
// 
// At PaserState==8 The last checksum byte has been recieved and this is the end of the Message. A checksum is calculated from the receievd payload data and
// the compared to the checksum inside the Message. If the two checksums match, the Message was successfully decoded and the PVT Message is moved out
// of a working variable to the PVT class member which is public and can be accessed from the calling application code. If the checksums matched the MessageNumber
// member is incremented.
//
// If the PVT Message is a valid location as determined by the IsValid() function, then the FixNumber member is incremented and the LastValidPositionAge  is set.
//
// NOTE: The PVT structure is overwritten on every successfil Message decode. The last good position fix is not necessarily  the once currently in the PVT strusture.
//       This allows you to monitor the progress of the GPS during acquisition and at other times when a position is not being obtained.
//
//       Your program code can call IsValidFixPosition() to determine if the current PVT structure has a valid posion fix.
//       See the example programs for details.
//
//
//
void uBloxGPS::ParseNAV_PVTMessage(uint8_t ch)
{
	switch (ParserState)
	{
		// State 0 the beginning
	case 0: // 1st byte of Message header
		if (ch == UBLOX_HEADER1) // All uBlox Messages start with this byte value
		{
			ParserState++; // Go to next state.
		}
		break;

	case 1: // 2nd byte of Message header.
		if (ch == UBLOX_HEADER2) // All uBlox Messages have this value as the second type sent.
		{
			ParserState++; // Go to next state.
		}
		else
		{
			ParserState = 0; // not the right character for the start of Message, so go back to the start.
		}
		break;


	case 2: // Class Number
		if (ch == WorkingPVT.Class) // Did we find the expected class type? (i.e. NAV)
		{
			ParserState++;  // Go to next state.			
		}
		else
		{
			ParserState = 0; //Go back to start
		}
		break;

	case 3:
		if (ch == WorkingPVT.ID) // Is tis the expeted message ID?
		{
			ParserState++; // Go to next state.
		}
		else
		{
			ParserState = 0; //Go back to start
		}
		break;

		// State 4 start reading payload length (2 bytes)
	case 4:  // Get the first byte of length
		lengthbyte1 = ch;
		ParserState++; // Go to next state.
		break;

	case 5:  //Get the 2nd byte of length
		lengthbye2 = ch;
		PayloadLength = (lengthbye2 << 8) + lengthbyte1; // Form up a LittleEndinain short from the two bytes
		WorkingPVT.PayloadLength = PayloadLength;

		// Set the payload pointer and count to prepare for the start of filling the payload in the next state.
		PayloadPtr = (uint8_t *)&WorkingPVT.iTOW; // Payload starts with iTOW
		PayloadCount = 0;
		ParserState++;
		break;

		// State 6 start reading the payload data into structure.
	case 6: // Read payload data, untill we have filled the payload with data.
		*PayloadPtr++ = ch;
		PayloadCount++;

		// if payload is filled.
		if (PayloadCount == PVTPAYLOADLEN) // PVT is fixed size, dont count on received PayloadLength because it could be corrupted.
		{
			ParserState++;  // Done reading payload data, go to the next state
		}
		break;

		// State 7 start CRC
	case 7: //CK_A: Get 1st check digit byte
		CK_A = ch;
		ParserState++;
		break;

		// State 8 complete CRC
	case 8: //CK_B: get 2nd check digit byte
		CK_B = ch;
		//Serial.println();

		// Verify checksum
		uint16_t check1 = CalcChecksum((uint8_t *)&WorkingPVT, sizeof(NAVPVTMsg));  // Checksum for the data collected
		uint16_t check2 = (CK_A << 8) + CK_B; // Checksum given in Message

		if (check1 == check2) // Checksum matches, all is good.
		{
			
			// Make PVT data available, update status
							
			memcpy((void *)&PVT, (void *)&WorkingPVT, sizeof(NAVPVTMsg));  // Copy Working PVT to LastGoodPVT
			CRC_OK = true;
			MessageNumber++; // Bump the Message number#
			if (IsValidFixPosition()) //the fix is a valid position
			{				
				LastValidPositionAge = millis(); // Mark the time of this fix.	
				FixNumber++; // Bump the fix#

				// Call the callback
				if (PositionFixCallback != NULL) // if a callback function has been set
				{
					PositionFixCallback(this);
				}
			}
			
		}
		else // checksums do not match, start over
		{
			CRC_OK = false;
		}


		ParserState = 0; // Go back to the beginning state for the next Message.
		break;

	}
}

//
// Set a callback function to be called on each position fix update.
// Set this value to NULL to stop the callback function.
//
void uBloxGPS::SetPositionUpdateCallbackFunction(void(*functptr)(uBloxGPS *))
{
	PositionFixCallback = functptr;
}


//
// Calculate the Checksum
//
uint16_t uBloxGPS::CalcChecksum(uint8_t *buff, int len)
{
	uint16_t Checksum = 0;
	uint8_t CK_A = 0;
	uint8_t CK_B = 0;
	for (int i = 0; i < len; i++)
	{
		CK_A = CK_A + buff[i];
		CK_B = CK_B + CK_A;
	}

	Checksum = (CK_A << 8) + CK_B;
	return Checksum;
}
