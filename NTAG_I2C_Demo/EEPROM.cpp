#include "stdafx.h"
#include "EEPROM.h"
#include "Reader.h"
#include "SRAM.h"
#include "NTAG_I2C_RF.h"
#include "Utils.h"
#include "NDEFmsg.h"
#include "NDEFRecord.h"
#include "Registers.h"


namespace NTAG_I2C_Demo {

	using namespace System;

	static ref class EEPROM
	{};
}

LONG EEPROM::EEPROMSpeedTest(int chMultiplier, BYTE* text, int* nWrittenBytes, float* writeTime, int* nReadBytes, float* readTime)
{
	LONG lReturn;
	BYTE message[MAX_MEM_SIZE];

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

		// Check memory size
		BYTE NtagVersion;
		int memSize = 0;
		lReturn = NTAG_I2C_RF::GET_VERSION(&NtagVersion);
		if (lReturn == SCARD_S_SUCCESS)
		{
			if (NtagVersion == NTAG_VERSION_1K)
				memSize = (NTAG_I2C_1k_MemSize + 16) / 4;
			else if (NtagVersion == NTAG_VERSION_2K)
				memSize = (NTAG_I2C_2k_MemSize + 16) / 4;

			// Build the NDEF message
			// NDEF payload
			int textSize = strlen((char*)text);
			int ndefPayloadSize = textSize*chMultiplier;
			BYTE* messageText = (BYTE*)malloc(ndefPayloadSize*sizeof(BYTE));
			BYTE* pnt;
			for (pnt = messageText; pnt < messageText + ndefPayloadSize; pnt += textSize)
				memcpy(pnt, text, textSize);
			// NDEF header (long NDEF)
			BYTE ndeftlv_header[] = { 0x03, 0xFF, ((ndefPayloadSize + 10) >> 8) & 0xFF, (ndefPayloadSize + 10) & 0xFF, 0xC1, 0x01, 0x00, 0x00, ((ndefPayloadSize + 3) >> 8) & 0xFF,
				(ndefPayloadSize + 3) & 0xFF, 0x54, 0x02, 0x65, 0x6E };
			int ndeftlvHeaderSize = ARRAY_SIZE(ndeftlv_header) - 2; // We rest -2 because the type text includes de language "en"
			// NDEF message
			int ndefMessageSize = ndeftlvHeaderSize + ndefPayloadSize;
			if (ndefMessageSize % 4 == 1)
				ndefMessageSize = ndefMessageSize + 3;
			if (ndefMessageSize % 4 == 2)
				ndefMessageSize = ndefMessageSize + 2;
			if (ndefMessageSize % 4 == 3)
				ndefMessageSize = ndefMessageSize + 1;
			memcpy(message, ndeftlv_header, ndeftlvHeaderSize);
			memcpy(message + ndeftlvHeaderSize, messageText, ndefPayloadSize);
			message[ndefMessageSize] = 0xFE;
			message[ndefMessageSize] = 0x00;
			message[ndefMessageSize] = 0x00;
			message[ndefMessageSize] = 0x00;
			//	ndefMessageSize += 4 - ndefMessageSize % 4;

			// Check if the message fits in the NTAG memory
			if (ndefMessageSize + 32 < memSize * 4)	// The NDEF message starts at page 4
			{
				BYTE wPage[4];
				BYTE *p_ndef = message, *p_endndef = message + ndefMessageSize;

				// Execute write test
				*writeTime = (float)clock();
				// Write the bytes in the first sector
				int i = User_memory_Begin;
				while (i < 0x100 && p_ndef < p_endndef && lReturn == SCARD_S_SUCCESS)
				{
					memcpy(wPage, p_ndef, 4);
					lReturn = NTAG_I2C_RF::WRITE(i, wPage);
					i++;
					p_ndef += 4;
				}
				// Check if there are more sectors
				if (p_ndef < p_endndef && lReturn == SCARD_S_SUCCESS)
				{
					lReturn = NTAG_I2C_RF::SECTOR_SELECT(1);
					if (lReturn == SCARD_S_SUCCESS)
					{
						i = 0;
						while (p_ndef < p_endndef && lReturn == SCARD_S_SUCCESS)
						{
							memcpy(wPage, p_ndef, 4);
							lReturn = NTAG_I2C_RF::WRITE(i, wPage);
							i++;
							p_ndef += 4;
						}
					}
					NTAG_I2C_RF::END_SECTOR_SELECT();	// Necessary to enable the polling again and to go back to sector 0 when the SECTOR_SELECT Function has been executed
				}
				*writeTime = (float)clock() - *writeTime;
				if (lReturn == SCARD_S_SUCCESS)
				{
					int max_fast_read = (MAX_TRANSCEIVE_LENGTH - 2) / 4;
					int fetch_start = 4;
					int fetch_end;

					// Execute read test
					*readTime = (float)clock();
					// There is a limitation when trying to read more than 256 bytes 
					// I split the fast reading in as many reads as I need
					while (fetch_start < (ndefMessageSize / 4) + 4 && lReturn == SCARD_S_SUCCESS)
					{
						fetch_end = fetch_start + max_fast_read - 1;
						// check for last read, fetch only rest
						if (fetch_end >= (ndefMessageSize / 4) + 3)
							fetch_end = (ndefMessageSize / 4) + 3;
						// check for sector change in between and reduce fast_read to stay within sector
						if ((fetch_start & 0xFF00) != (fetch_end & 0xFF00))
							fetch_end = (fetch_start & 0xFF00) + 0xFF;
						// fetch data
						lReturn = NTAG_I2C_RF::FAST_READ(fetch_start & 0x00FF, fetch_end & 0x00FF, message);	// I don't care what I am reading, so I use the message variable because I won't use it anymore
						if (lReturn == SCARD_S_SUCCESS)
						{
							// calculate next fetch_start
							fetch_start = fetch_end + 1;
							// check for sector select needed
							if ((fetch_start & 0xFF00) != (fetch_end & 0xFF00))
								lReturn = NTAG_I2C_RF::SECTOR_SELECT(1);
						}
					}
					*readTime = (float)clock() - *readTime;
					// Check if we have gone to sector 1
					if ((fetch_start & 0xFF00) != 0)
						NTAG_I2C_RF::END_SECTOR_SELECT();	// Necessary to enable the polling again and to go back to sector 0 when the SECTOR_SELECT Function has been executed
					if (lReturn == SCARD_S_SUCCESS)
					{
						*nWrittenBytes = ndefMessageSize;
						*nReadBytes = ndefMessageSize;
					}
				}
			}
			else
				lReturn = SCARD_E_INVALID_VALUE;
		}
		free(Reader::ppioSendPci);
		SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
	}
	free(Reader::phCardHandle);

	return lReturn;
};

LONG EEPROM::FlashMemory(BYTE* newFW, int newFWLength, DWORD* flashBytes, float* flashTime, System::Windows::Forms::ProgressBar^ progressBar1)
		{
			LONG lReturn;
			BYTE sectorFW[SECTOR_SIZE];
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
				*flashBytes = (int)ceil((float)(newFWLength + 2) / 4) * 4;
				*flashTime = (float)clock();
				Reader::ppioSendPci = (SCARD_IO_REQUEST*)malloc(sizeof(SCARD_IO_REQUEST));
				Reader::ppioSendPci->dwProtocol = dwAP;
				Reader::ppioSendPci->cbPciLength = 8;

				// We divide the firmware in bunches of 4096 bytes, that will be written in one of the sectors of the microcontroller
				int nFlashSectors = newFWLength / SECTOR_SIZE + ((newFWLength % SECTOR_SIZE) == 0 ? 0 : 1);
				int h = 0;
				int j = 0;
				do
				{
					// Write a whole sector onto the SRAM
					// Check if it is the last sector
					if (newFWLength - (h + 1) * SECTOR_SIZE < 0)
					{
						// If it is the last sector, we copy the remaining bytes of the firmware to the sectorFW array, and we fill the rest of the array with 0s
						memset(sectorFW, 0, SECTOR_SIZE);
						memcpy(sectorFW, newFW + h*SECTOR_SIZE, newFWLength % SECTOR_SIZE);
					}
					else
					{	// If it is not the last sector, we copy the corresponding sector of the firmware to the sectorFW arraw
						memcpy(sectorFW, newFW + h*SECTOR_SIZE, SECTOR_SIZE);
					}
					// Send SRAM command indicating that a whole new sector will be written
					int flashAddr = 0x4000 + h * SECTOR_SIZE;	// We begin to write on sector 4
					memset(cmd, 0, NTAG_MEM_SIZE_SRAM);
					cmd[52] = flashAddr >> 24 & 0xFF;
					cmd[53] = flashAddr >> 16 & 0xFF;
					cmd[54] = flashAddr >> 8 & 0xFF;
					cmd[55] = flashAddr & 0xFF;
					cmd[56] = SECTOR_SIZE >> 24 & 0xFF;
					cmd[57] = SECTOR_SIZE >> 16 & 0xFF;
					cmd[58] = SECTOR_SIZE >> 8 & 0xFF;
					cmd[59] = SECTOR_SIZE & 0xFF;
					cmd[60] = 'F';
					cmd[61] = 'P';
					 lReturn = SRAM::WriteSram(cmd);
					// Send several SRAM commands with the firmware of the sector indicated previously
					BYTE* pSectorFW = sectorFW;

					while (pSectorFW < sectorFW + SECTOR_SIZE && lReturn == SCARD_S_SUCCESS)
					{
						lReturn = Utils::WaitForI2CRead();
						if (lReturn == SCARD_S_SUCCESS)
						{
							lReturn = SRAM::WriteSram(pSectorFW);
							progressBar1->PerformStep();
						}
						pSectorFW += NTAG_MEM_SIZE_SRAM;
					}
					if (lReturn == SCARD_S_SUCCESS)
					{
						// Wait for the microcontroller to process the information and to write the response into the SRAM
						Sleep(2000);
						lReturn = Utils::WaitForI2CWrite();
						if (lReturn == SCARD_S_SUCCESS)
						{
							lReturn = SRAM::ReadSram(resp);
							// Check if an error has occurred even though lReturn indicates success
							if (lReturn == SCARD_S_SUCCESS && (resp[60] != 'A' || resp[61] != 'C' || resp[62] != 'K'))
								lReturn = SCARD_E_COMM_DATA_LOST;
						}
					}

					h++;
				} while (h < nFlashSectors && lReturn == SCARD_S_SUCCESS);
				if (lReturn == SCARD_S_SUCCESS)
				{
					// If the writting has been ccompleted successfully, we indicate it to the board
					memset(cmd, 0, NTAG_MEM_SIZE_SRAM);
					cmd[60] = 'F';
					cmd[61] = 'S';
					lReturn = SRAM::WriteSram(cmd);
					if (lReturn == SCARD_S_SUCCESS)
						lReturn = Utils::WaitForI2CRead();
				}
				else
				{
					// If the writting has not been completed successfully, we indicate it to the board (we don't update lReturn)
					memset(cmd, 0, NTAG_MEM_SIZE_SRAM);
					cmd[60] = 'F';
					cmd[61] = 'F';
					SRAM::WriteSram(cmd);
					Utils::WaitForI2CRead();
				}
				*flashTime = (float)clock() - *flashTime;

				free(Reader::ppioSendPci);
				SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
			}
			free(Reader::phCardHandle);

			return lReturn;
		};

		// Reads the full memory of the NTAG (the first four pages, the user memory and the dynamic lock bytes)
LONG EEPROM::ReadMemory(BYTE* data, DWORD* readBytes, float* readTime)
		{
			LONG lReturn;

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

				// Initialize variables
				*readBytes = 0;
				int max_fast_read = (MAX_TRANSCEIVE_LENGTH - 2) / 4;
				int fetch_start = 0;
				int fetch_end = 0;
				// Check memory size
				BYTE NtagVersion;
				int memSize = 0;
				lReturn = NTAG_I2C_RF::GET_VERSION(&NtagVersion);
				if (lReturn == SCARD_S_SUCCESS)
				{
					if (NtagVersion == NTAG_VERSION_1K)
						memSize = (NTAG_I2C_1k_MemSize + 16) / 4;
					else if (NtagVersion == NTAG_VERSION_2K)
						memSize = (NTAG_I2C_2k_MemSize + 16) / 4;

					*readTime = (float)clock();
					// There is a limitation when trying to read more than 256 bytes 
					// I split the fast reading in as many reads as I need
					do
					{
						fetch_end = fetch_start + max_fast_read - 1;
						// check for last read, fetch only rest
						if (fetch_end >= memSize)
							fetch_end = memSize;
						// check for sector change in between and reduce fast_read to stay within sector
						if ((fetch_start & 0xFF00) != (fetch_end & 0xFF00))
							fetch_end = (fetch_start & 0xFF00) + 0xFF;
						// fetch data
						lReturn = NTAG_I2C_RF::FAST_READ(fetch_start & 0x00FF, fetch_end & 0x00FF, data + *readBytes);
						if (lReturn == SCARD_S_SUCCESS)
						{
							*readBytes += (fetch_end - fetch_start + 1) * 4;
							// calculate next fetch_start
							fetch_start = fetch_end + 1;
							// check for sector select needed
							if ((fetch_start & 0xFF00) != (fetch_end & 0xFF00))
								lReturn = NTAG_I2C_RF::SECTOR_SELECT(1);
						}
					} while (fetch_start <= memSize && lReturn == SCARD_S_SUCCESS);
					*readTime = (float)clock() - *readTime;
					// Check if we have gone to sector 1
					if ((fetch_start & 0xFF00) != 0)
						NTAG_I2C_RF::END_SECTOR_SELECT();	// Necessary to enable the polling again and to go back to sector 0 when the SECTOR_SELECT Function has been executed
				}
				free(Reader::ppioSendPci);
				SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
			}
			free(Reader::phCardHandle);

			return lReturn;
		}

LONG EEPROM::ResetMemory(DWORD* resetBytes, float* resetTime)
		{
			LONG lReturn;
			BYTE wPage[4] = { 0 };
			BYTE rPage[16];
			BYTE message[MAX_MEM_SIZE];
			int length;
			
			int payloadSize;

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

				// Check memory size
				BYTE NtagVersion;
				lReturn = NTAG_I2C_RF::GET_VERSION(&NtagVersion);
				if (lReturn == SCARD_S_SUCCESS)
				{
					if (NtagVersion == NTAG_VERSION_1K)
						*resetBytes = NTAG_I2C_1k_MemSize;
					else if (NtagVersion == NTAG_VERSION_2K)
						*resetBytes = NTAG_I2C_2k_MemSize;
					int memSize = (*resetBytes + 16) / 4;

					*resetTime = (float)clock();
					// Check static lock bytes
					lReturn = NTAG_I2C_RF::READ(2, rPage);
					if (lReturn == SCARD_S_SUCCESS)
					{
						lReturn = SCARD_P_STATIC_LOCKED;
						if (rPage[2] == 0 && rPage[3] == 0)
						{
							// Check dynamic lock bytes
							if (NtagVersion == NTAG_VERSION_1K)
								lReturn = NTAG_I2C_RF::READ(0xE2, rPage);
							else
							{
								lReturn = NTAG_I2C_RF::SECTOR_SELECT(1);
								if (lReturn == SCARD_S_SUCCESS)
									lReturn = NTAG_I2C_RF::READ(0xE0, rPage);
								NTAG_I2C_RF::END_SECTOR_SELECT();	// Necessary to enable the polling again and to go back to sector 0 when the SECTOR_SELECT Function has been executed
							}
							if (lReturn == SCARD_S_SUCCESS)
							{
								lReturn = SCARD_P_DYNAMIC_LOCKED;
								if (rPage[0] == 0 && rPage[1] == 0 && rPage[2] == 0)
								{
									// Try writing the Capability Container
									if (NtagVersion == NTAG_VERSION_1K)
									{
										// CC for NTAG 1k
										wPage[0] = 0xE1;
										wPage[1] = 0x10;
										wPage[2] = 0x6D;
										wPage[3] = 0x00;
									}
									else
									{
										// CC for NTAG 2k
										wPage[0] = 0xE1;
										wPage[1] = 0x10;
										wPage[2] = 0xEA;
										wPage[3] = 0x00;
									}
									lReturn = NTAG_I2C_RF::WRITE(3, wPage);
									if (lReturn == SCARD_S_SUCCESS)
									{
										// Check if it has been written succesfully
										lReturn = NTAG_I2C_RF::READ(3, rPage);
										if (lReturn == SCARD_S_SUCCESS)
										{
											lReturn = SCARD_P_CC_WRONG;
											if (rPage[0] == wPage[0] && rPage[1] == wPage[1] && rPage[2] == wPage[2] && rPage[3] == wPage[3])
											{
												// Write the whole first sector
												int i = User_memory_Begin;
												wPage[0] = 0;
												wPage[1] = 0;
												wPage[2] = 0;
												wPage[3] = 0;
												do
												{
													lReturn = NTAG_I2C_RF::WRITE(i, wPage);
													i++;
												} while (i < 0x100 && i < memSize && lReturn == SCARD_S_SUCCESS);
												// Check if there are more sectors
												if (memSize > 0x100 && lReturn == SCARD_S_SUCCESS)
												{
													lReturn = NTAG_I2C_RF::SECTOR_SELECT(1);
													i = 0;
													while (i < memSize - 0x100 && lReturn == SCARD_S_SUCCESS)
													{
														lReturn = NTAG_I2C_RF::WRITE(i, wPage);
														i++;
													}
													NTAG_I2C_RF::END_SECTOR_SELECT();	// Necessary to enable the polling again and to go back to sector 0 when the SECTOR_SELECT Function has been executed
												}
												if (lReturn == SCARD_S_SUCCESS)
												{
													//TODO: Refactor: Create function in NDEF. GetDefaultNDEFMessage!
													// Create the default NDEF message to be written into the tag memory

													BYTE* payload = NDEFmsg::CreateDefaultNDEFmessage(message, length);
													payloadSize = 1 + strlen("NTAG I2C EXPLORER") + 1 + strlen("http://www.nxp.com/demoboard/OM5569");
													lReturn = NDEFRecord::createSPRecord(payload, payloadSize, message, &length);
													
													if (lReturn == SCARD_S_SUCCESS)
													{
														// Append the AAR to the NDEF message
														lReturn = NDEFmsg::AppendAAR(message, &length);
														if (lReturn == SCARD_S_SUCCESS)
														{
															// Write the NDEF message to the tag
															lReturn = NDEFmsg:: WriteNdefMessage(message, length);
															if (lReturn == SCARD_S_SUCCESS)
															{
																// Check configuration lock bit via RF
																Ntag_I2C_Registers_t configReg;
																lReturn = Registers::GetConfigRegisters(&configReg);
																if (lReturn == SCARD_S_SUCCESS)
																{
																	lReturn = SCARD_P_CONFIG_LOCKED;
																	if (configReg.REG_EEPROM_WR_BUSY == FALSE)	// It is the same bit in the session registers
																	{
																		// Change the config registers to their default value
																		configReg.REG_I2C_RST_ON_OFF = FALSE;
																		memcpy(configReg.REG_FD_OFF, "00b", 3);
																		memcpy(configReg.REG_FD_ON, "00b", 3);
																		configReg.REG_PTHRU_DIR = TRUE;
																		configReg.REG_LAST_NDEF_PAGE = 0;
																		configReg.REG_SM_Reg = 0xF8;
																		configReg.REG_WD_LS_Reg = 0x48;
																		configReg.REG_WD_MS_Reg = 0x08;
																		configReg.REG_I2C_CLOCK_STR = TRUE;
																		configReg.REG_I2C_LOCKED = FALSE;
																		configReg.REG_RF_LOCKED = FALSE;
																		lReturn = Registers::SetConfigRegisters(configReg);

																		*resetTime = (float)clock() - *resetTime;
																	}
																}
															}
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
				//free(Reader::ppioSendPci);
				SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
			}
			//free(Reader::phCardHandle);

			return lReturn;
		}
