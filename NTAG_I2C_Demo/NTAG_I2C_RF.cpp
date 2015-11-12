#include "stdafx.h"
#include "Ntag_I2C.h"
#include "NTAG_I2C_RF.h"
#include "ProxAPI.h"
#include "Reader.h"



	
		// Gets the version of the NTAG I2C IC
	LONG NTAG_I2C_RF::GET_VERSION(BYTE* version)
		{
			LONG lReturn;
			BYTE cmdGetVersion[] = { 0xFF, 0xEF, 0x00, 0x00, 0x01, 0x60 };
			DWORD cmdLength = 6;
			BYTE respGetVersion[10];
			DWORD respLength = 10;

			lReturn = SCardTransmit(*Reader::phCardHandle, Reader::ppioSendPci, cmdGetVersion, cmdLength, NULL, respGetVersion, &respLength);
			if (lReturn == SCARD_S_SUCCESS && respLength == 10) // Should also check CRC
			{
				if (respGetVersion[6] == 0x13)
					*version = NTAG_VERSION_1K;
				else if (respGetVersion[6] == 0x15)
					*version = NTAG_VERSION_2K;
				else
					lReturn = SCARD_E_UNKNOWN_CARD;	// The version is unknown by this function
			}
			// Check if an error has occurred even though lReturn indicates success
			else if (lReturn == SCARD_S_SUCCESS)
				lReturn = SCARD_E_COMM_DATA_LOST;

			return lReturn;
		};

		// Disables the polling and selects the sector passed as parameter
		// Note that it is necessary to execute the END_SECTOR_SELECT function later, to enable the polling again and to go back to sector 0
	LONG NTAG_I2C_RF::SECTOR_SELECT(BYTE nb_sector)
		{
			LONG lReturn;
			BYTE cmdDisPoll[] = { 0xFF, 0xCC, 0x00, 0x00, 0x02, 0xAC, 0x01 };
			DWORD cmdDisLength = 7;
			BYTE respDisPoll[2];
			DWORD respDisLength = 2;
			BYTE cmdSectorSelect0[] = { 0xFF, 0xEF, 0x00, 0x00, 0x02, 0xC2, 0xFF };
			DWORD cmdSS0Length = 7;
			BYTE respSectorSelect0[1];
			DWORD respSS0Length = 1;
			BYTE cmdSectorSelect1[] = { 0xFF, 0xEF, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00 };
			DWORD cmdSS1Length = 9;
			BYTE respSectorSelect1[1];
			DWORD respSS1Length = 1;

			// Disable polling
			lReturn = SCardTransmit(*Reader::phCardHandle, Reader::ppioSendPci, cmdDisPoll, cmdDisLength, NULL, respDisPoll, &respDisLength);
			if (lReturn == SCARD_S_SUCCESS && respDisLength == 2 && respDisPoll[0] == 0x90 && respDisPoll[1] == 0x00)
			{
				// Send the first command of Sector Select
				lReturn = SCardTransmit(*Reader::phCardHandle, Reader::ppioSendPci, cmdSectorSelect0, cmdSS0Length, NULL, respSectorSelect0, &respSS0Length);
				if (lReturn == SCARD_S_SUCCESS && respSS0Length == 1 && respSectorSelect0[0] == 0x0A)
				{
					// Send the second command of the sector select, indicating the sector
					cmdSectorSelect1[5] = nb_sector;
					lReturn = SCardTransmit(*Reader::phCardHandle, Reader::ppioSendPci, cmdSectorSelect1, cmdSS1Length, NULL, respSectorSelect1, &respSS1Length);

					// Check if an error has occurred even though lReturn indicates success
					if (lReturn == SCARD_S_SUCCESS && respSS1Length != 0)
						lReturn = SCARD_E_COMM_DATA_LOST;
				}
				else if (lReturn == SCARD_S_SUCCESS)
					lReturn = SCARD_E_COMM_DATA_LOST;
			}
			else if (lReturn == SCARD_S_SUCCESS)
				lReturn = SCARD_E_COMM_DATA_LOST;

			return lReturn;
		}

		// Enables the polling and goes back to sector 0
	LONG NTAG_I2C_RF::END_SECTOR_SELECT()
		{
			LONG lReturn;
			BYTE cmdEnPoll[] = { 0xFF, 0xCC, 0x00, 0x00, 0x02, 0xAC, 0x00 };
			DWORD cmdEnLength = 7;
			BYTE respEnPoll[2];
			DWORD respEnLength = 2;
			BYTE cmdError[] = { 0xFF, 0xEF, 0x00, 0x00, 0x01, 0x00 };
			DWORD cmdErrorLength = 6;
			BYTE respError[1];
			DWORD respErrorLength = 1;

			// Enable polling
			lReturn = SCardTransmit(*Reader::phCardHandle, Reader::ppioSendPci, cmdEnPoll, cmdEnLength, NULL, respEnPoll, &respEnLength);
			// Check if an error has occurred even though lReturn indicates success
			if (lReturn == SCARD_S_SUCCESS && (respEnLength != 2 || respEnPoll[0] != 0x90 || respEnPoll[1] != 0x00))
				lReturn = SCARD_E_COMM_DATA_LOST;

			// Send a command that causes an error. This way we ensure that the polling is working again and that we are in sector 0
			SCardTransmit(*Reader::phCardHandle, Reader::ppioSendPci, cmdError, cmdErrorLength, NULL, respError, &respErrorLength);

			return lReturn;
		}

		// Sends a Read command and stores the result in the variable data
	LONG NTAG_I2C_RF::READ(BYTE page, BYTE* data)
		{
			LONG lReturn;
			BYTE cmdRead[] = { 0xFF, 0xEF, 0x00, 0x00, 0x02, 0x30, 0x00 };
			DWORD cmdReadLength = 7;
			BYTE respRead[18];
			DWORD respReadLength = 18;

			// Send Read command
			cmdRead[6] = page;
			lReturn = SCardTransmit(*Reader::phCardHandle, Reader::ppioSendPci, cmdRead, cmdReadLength, NULL, respRead, &respReadLength);
			if (lReturn == SCARD_S_SUCCESS && respReadLength == 18) // Should also check CRC
			{
				memcpy(data, respRead, respReadLength - 2);
			}
			// Check if an error has occurred even though lReturn indicates success
			else if (lReturn == SCARD_S_SUCCESS)
				lReturn = SCARD_E_COMM_DATA_LOST;

			return lReturn;
		}

		// Sends a Fast Read command and stores the result in the variable data
	LONG NTAG_I2C_RF::FAST_READ(BYTE startPage, BYTE endPage, BYTE* data)
		{
			LONG lReturn;
			BYTE cmdFastRead[] = { 0xFF, 0xEF, 0x00, 0x00, 0x03, 0x3A, 0x00, 0x00 };
			DWORD cmdFastReadLength = 8;
			BYTE respFastRead[1026];
			DWORD respFastReadLength = 1026;

			// Send Fast Read command
			cmdFastRead[6] = startPage;
			cmdFastRead[7] = endPage;
			lReturn = SCardTransmit(*Reader::phCardHandle, Reader::ppioSendPci, cmdFastRead, cmdFastReadLength, NULL, respFastRead, &respFastReadLength);
			if (lReturn == SCARD_S_SUCCESS && respFastReadLength == (endPage - startPage + 1) * 4 + 2) // Should also check CRC
			{
				memcpy(data, respFastRead, respFastReadLength - 2);
			}
			// Check if an error has occurred even though lReturn indicates success
			else if (lReturn == SCARD_S_SUCCESS)
				lReturn = SCARD_E_COMM_DATA_LOST;

			return lReturn;
		}

		// Sends a Write command with the content of the variable data
	LONG NTAG_I2C_RF::WRITE(BYTE page, BYTE* data)
		{
			LONG lReturn;
			BYTE cmdWrite[] = { 0xFF, 0xEF, 0x00, 0x00, 0x06, 0xA2, 0x00, 0x00, 0x00, 0x00, 0x00 };
			DWORD cmdWriteLength = 11;
			BYTE respWrite[1];
			DWORD respWriteLength = 1;

			// Send Write command
			cmdWrite[6] = page;
			cmdWrite[7] = data[0];
			cmdWrite[8] = data[1];
			cmdWrite[9] = data[2];
			cmdWrite[10] = data[3];
			lReturn = SCardTransmit(*Reader::phCardHandle, Reader::ppioSendPci, cmdWrite, cmdWriteLength, NULL, respWrite, &respWriteLength);
			// Check if an error has occurred even though lReturn indicates success
			if (lReturn == SCARD_S_SUCCESS && (respWriteLength != 1 || respWrite[0] != 0x0A))
				lReturn = SCARD_E_COMM_DATA_LOST;

			return lReturn;
		}
