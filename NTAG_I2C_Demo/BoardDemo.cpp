#include "stdafx.h"
#include "BoardDemo.h"
#include "Reader.h"
#include "SRAM.h"

namespace NTAG_I2C_Demo {

	using namespace System;

	static ref class BoardDemo
	{
	public:
		static BYTE* prevTempC;
		static BYTE* prevTempF;
		static BYTE* prevVolt;
	};
}
VOID BoardDemo::initialize(){
	BoardDemo::prevTempC = (BYTE*)malloc(4 * sizeof(BYTE));
			memset(BoardDemo::prevTempC, 0, 4 * sizeof(BYTE));
			BoardDemo::prevTempF = (BYTE*)malloc(5 * sizeof(BYTE));
			memset(BoardDemo::prevTempF, 0, 5 * sizeof(BYTE));
			BoardDemo::prevVolt = (BYTE*)malloc(2 * sizeof(BYTE));
			memset(BoardDemo::prevVolt, 0, 2 * sizeof(BYTE));
		}

VOID BoardDemo::destroy(){
	free(BoardDemo::prevTempC);
	free(BoardDemo::prevTempF);
	free(BoardDemo::prevVolt);
}

		// Turns on one of the LEDs (by writting the appropriate content into the SRAM) and reads what buttons are pressed and the temperature at the board
//TODO: Change --> Board Status
LONG BoardDemo::Run(char ledColor, BOOL enTemp, BOOL enLCD, BOOL dispNDEF, BOOL* redButton, BOOL* greenButton, BOOL* blueButton, float* tempC, float* tempF, float* voltage, float* version)
		{
			LONG lReturn;
			BYTE cmd[NTAG_MEM_SIZE_SRAM];
			BYTE resp[NTAG_MEM_SIZE_SRAM];

			// Connect to card (we do it in exclusive mode to avoid interferences from other Windows services, e.g., when trying to access to sectors other than 0)
			
			//Reader::cardConnect_EXCLUSIVE_MODE
			Reader::phCardHandle = (SCARDHANDLE*)malloc(sizeof(SCARDHANDLE));
			DWORD dwAP; //Active protocol
			int i = 0;
			do
			{
				lReturn = SCardConnect(*Reader::phContext, Reader::szReader, SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, Reader::phCardHandle, &dwAP);
				Sleep(50);
				i++;
			} while (lReturn == SCARD_E_SHARING_VIOLATION && i < 10);


			if (lReturn == SCARD_S_SUCCESS)
			{
				//Reader__sendPciConfig
				Reader::ppioSendPci = (SCARD_IO_REQUEST*)malloc(sizeof(SCARD_IO_REQUEST));
				Reader::ppioSendPci->dwProtocol = dwAP;
				Reader::ppioSendPci->cbPciLength = 8;

				// Send SRAM command with the LED information and get information about the temperature, voltage and buttons
				
				//prepareCommand
				memset(&cmd, 0, NTAG_MEM_SIZE_SRAM);
				cmd[40] = BoardDemo::prevTempC[0];
				cmd[41] = BoardDemo::prevTempC[1];
				cmd[42] = BoardDemo::prevTempC[2];
				cmd[43] = BoardDemo::prevTempC[3];

				cmd[45] = BoardDemo::prevTempF[0];
				cmd[46] = BoardDemo::prevTempF[1];
				cmd[47] = BoardDemo::prevTempF[2];
				cmd[48] = BoardDemo::prevTempF[3];
				cmd[49] = BoardDemo::prevTempF[4];

				cmd[56] = BoardDemo::prevVolt[0];
				cmd[57] = BoardDemo::prevVolt[1];

				if (dispNDEF == TRUE)
					cmd[53] = 'E';
				if (enLCD == TRUE)
					cmd[54] = 'E';
				if (enTemp == TRUE)
					cmd[55] = 'E';
				cmd[60] = 'L';
				cmd[61] = '0' + ledColor;

				lReturn = SRAM::PerformSramCom(cmd, resp);
				if (lReturn == SCARD_S_SUCCESS)
				{
					// Parse the information from the SRAM
					// Obtain the temperature
					int Temp = 0;
					// Adding first Byte
					Temp = ((resp[59] >> 5) & 0x00000007);
					// Adding second Byte
					Temp |= ((resp[58] << 3) & 0x000007F8);
					// If the 11 Bit is 1 it is negative
					if ((Temp & (1 << 11)) == (1 << 11)) {
						// Mask out the 11 Bit
						Temp &= ~(1 << 11);
					}
					// Obtain the actual value
					*tempC = Temp * 0.125f;
					*tempF = 32 + (1.8f * *tempC);

					// Obtain the voltage
					int Voltage = (int)resp[57] * 0x100 + resp[56];

					// Obtain the actual value
					*voltage = 0;
					if (Voltage != 0)
						*voltage = 0x3FF * 2.048f / Voltage;

					// Obtain the temperature and voltage values to send the next time to the board
					char fString[10];
					if (enTemp == TRUE)
					{
						sprintf_s(fString, 10, "%06.2f", *tempF);
						BoardDemo::prevTempF[0] = fString[0];
						BoardDemo::prevTempF[1] = fString[1];
						BoardDemo::prevTempF[2] = fString[2];
						BoardDemo::prevTempF[3] = fString[4];
						BoardDemo::prevTempF[4] = fString[5];
						sprintf_s(fString, 10, "%05.2f", *tempC);
						BoardDemo::prevTempC[0] = fString[0];
						BoardDemo::prevTempC[1] = fString[1];
						BoardDemo::prevTempC[2] = fString[3];
						BoardDemo::prevTempC[3] = fString[4];
					}
					sprintf_s(fString, 10, "%03.1f", *voltage);
					BoardDemo::prevVolt[0] = fString[0];
					BoardDemo::prevVolt[1] = fString[2];

					// Obtain the buttons
					*redButton = FALSE;
					*greenButton = FALSE;
					*blueButton = FALSE;
					if ((resp[62] & 0x01) == 0x01)
						*redButton = TRUE;
					if ((resp[62] & 0x02) == 0x02)
						*greenButton = TRUE;
					if ((resp[62] & 0x04) == 0x04)
						*blueButton = TRUE;

					// Obtain the firmware version
					*version = 1;
					if (resp[63] != 0)
						*version = ((resp[63] >> 4) & 0x0F) + ((resp[63] & 0x0F) * 0.1f);
				}
				free(Reader::ppioSendPci);
				SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
			}
			free(Reader::phCardHandle);

			return lReturn;
		}

		LONG BoardDemo::ReadVersions(Versions_t* versions)
		{
			LONG lReturn;
			BYTE cmd[NTAG_MEM_SIZE_SRAM];
			BYTE resp[NTAG_MEM_SIZE_SRAM];

			// Connect to card (we do it in exclusive mode to avoid interferences from other Windows services, e.g., when trying to access to sectors other than 0)
			Reader::phCardHandle = (SCARDHANDLE*)malloc(sizeof(SCARDHANDLE));
			DWORD dwAP; //Active protocol
			int i = 0;
			do
			{
				lReturn = SCardConnect(*Reader::phContext, Reader::szReader, SCARD_SHARE_EXCLUSIVE, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, Reader::phCardHandle, &dwAP);
				Sleep(50);
				i++;
			} while (lReturn == SCARD_E_SHARING_VIOLATION && i < 10);
			if (lReturn == SCARD_S_SUCCESS)
			{
				Reader::ppioSendPci = (SCARD_IO_REQUEST*)malloc(sizeof(SCARD_IO_REQUEST));
				Reader::ppioSendPci->dwProtocol = dwAP;
				Reader::ppioSendPci->cbPciLength = 8;

				// Send SRAM command asking for version information
				memset(&cmd, 0, NTAG_MEM_SIZE_SRAM);
				cmd[60] = 'V';
				lReturn = SRAM::PerformSramCom(cmd, resp);
				if (lReturn == SCARD_S_SUCCESS)
				{
					// Parse the information from the SRAM
					// Check if data was sent, otherwise it is a ExplorerBoard FW
					if (resp[12] == 0)
					{
						versions->Board_Version[0] = ((resp[63] >> 4) & (BYTE)0x0F) + 0x30;
						versions->Board_Version[1] = '.';
						versions->Board_Version[2] = (resp[63] & (BYTE)0x0F) + 0x30;
						versions->Board_Version[3] = '\0';
						strcpy_s(versions->Board_FW_Version, 4, versions->Board_Version);
					}
					else
					{
						resp[15] = '\0';
						strcpy_s(versions->Board_Version, 4, (char*)resp + 12);
						resp[31] = '\0';
						strcpy_s(versions->Board_FW_Version, 4, (char*)resp + 28);
					}
				}

				//TODO : refactor call free Reader ???? check how to avoid manually destroyment
				free(Reader::ppioSendPci);
				SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
			}
			free(Reader::phCardHandle);

			return lReturn;
		}