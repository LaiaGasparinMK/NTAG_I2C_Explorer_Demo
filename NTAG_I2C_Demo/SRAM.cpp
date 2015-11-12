#include "stdafx.h"
#include "SRAM.h"
#include "Reader.h"
#include "Utils.h"
#include "NTAG_I2C_RF.h"



		// Performs a cycle of writing into the SRAM, waiting for mthe microcontroller to read it and to respond, and reading the response
		LONG SRAM::PerformSramCom(BYTE*cmd, BYTE* resp)
		{
			LONG lReturn;

			// Send SRAM command
			lReturn = WriteSram(cmd);
			if (lReturn == SCARD_S_SUCCESS)
			{
				// Wait for I2C Read
				lReturn = Utils::WaitForI2CRead();
				if (lReturn == SCARD_S_SUCCESS)
				{
					// Wait for I2C Write
					lReturn = Utils::WaitForI2CWrite();
					if (lReturn == SCARD_S_SUCCESS)
					{
						// Read SRAM response
						lReturn = ReadSram(resp);
					}
				}
			}

			return lReturn;
		}


		// Reads the SRAM and stores the result in the variable data
		LONG SRAM::ReadSram(BYTE* data)
		{
			LONG lReturn;

			// In Pass-through mode, the SRAM may be mirrored to sector 0 or sector 1
			BYTE NtagVersion;
			lReturn = NTAG_I2C_RF::GET_VERSION(&NtagVersion);
			if (lReturn == SCARD_S_SUCCESS)
			{
				if (NtagVersion == NTAG_VERSION_2K)
					lReturn = NTAG_I2C_RF::SECTOR_SELECT(1);
				if (lReturn == SCARD_S_SUCCESS)
					lReturn = NTAG_I2C_RF::FAST_READ(SRAM_Begin, SRAM_Finish, data);
				if (NtagVersion == NTAG_VERSION_2K)
					NTAG_I2C_RF::END_SECTOR_SELECT();	// Necessary to enable the polling again and to go back to sector 0 when the SECTOR_SELECT Function has been executed
			}

			return lReturn;
		}

		// Writes the SRAM with the content of the variable data
		LONG SRAM::WriteSram(BYTE* data)
		{
			LONG lReturn;
			BYTE page[4];

			// In Pass-through mode, the SRAM may be mirrored to sector 0 or sector 1
			BYTE NtagVersion;
			lReturn = NTAG_I2C_RF::GET_VERSION(&NtagVersion);
			if (lReturn == SCARD_S_SUCCESS)
			{
				if (NtagVersion == NTAG_VERSION_2K)
					lReturn = NTAG_I2C_RF::SECTOR_SELECT(1);
				if (lReturn == SCARD_S_SUCCESS)
				{
					// The 64-bytes SRAM Blocks are mapped in 16 pages
					int i = 0;
					do
					{
						page[0] = data[(i * 4) + 0];
						page[1] = data[(i * 4) + 1];
						page[2] = data[(i * 4) + 2];
						page[3] = data[(i * 4) + 3];
						lReturn = NTAG_I2C_RF::WRITE(SRAM_Begin + i, page);
						i++;
					} while (i < 16 && lReturn == SCARD_S_SUCCESS);
				}
				if (NtagVersion == NTAG_VERSION_2K)
					NTAG_I2C_RF::END_SECTOR_SELECT();	// Necessary to enable the polling again and to go back to sector 0 when the SECTOR_SELECT Function has been executed
			}

			return lReturn;
		}
		// Performs a test on the speed for writing and reading from the SRAM
		LONG SRAM::SRAMSpeedTest(int chMultiplier, BOOL polling, BOOL* integritySent, BOOL* integrityRecv, float* writeTime, float* readTime)
		{
			LONG lReturn;
			BYTE dataBlock[NTAG_MEM_SIZE_SRAM] = { 0 };

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

				// During the SRAM test we write 64 bytes in the memory space dedicated for it
				// S: reference for the SpeedTest
				dataBlock[60] = 'S';
				lReturn = WriteSram(dataBlock);
				if (lReturn == SCARD_S_SUCCESS)
				{
					// if the Passthrough mode was not activated, wait for it to be activated
					lReturn = Utils::WaitForPassthroughActive();
					if (lReturn == SCARD_S_SUCCESS)
					{
						dataBlock[60] = 0;	// == memset(dataBlock, 0, NTAG_MEM_SIZE_SRAM);
						// Initialize the seed for the CRC32 calculation
						uint32_t seed = 0xFFFFFFFF;
						uint32_t crcTx = 0;

						// Execute write test
						*writeTime = (float)clock();
						int i = chMultiplier;
						while (i > 0 && lReturn == SCARD_S_SUCCESS)
						{
							// The last block contains the "finish_S_" string to notify the LPC board about the message transmission completion
							if (i == 1)
							{
								memcpy(dataBlock, "finish_S_", 9);
								// Update content for the CRC32 calculation (last 4 bytes are the CRC32)
								crcTx = crc32(seed, dataBlock, NTAG_MEM_SIZE_SRAM - 4);
								// Append the CRC32 to the block as this is the last block to be transmitted
								dataBlock[60] = (crcTx >> 0) & 0xFF;
								dataBlock[61] = (crcTx >> 8) & 0xFF;
								dataBlock[62] = (crcTx >> 16) & 0xFF;
								dataBlock[63] = (crcTx >> 24) & 0xFF;
							}
							else
							{
								// Update content for the CRC32 calculation
								crcTx = crc32(seed, dataBlock, NTAG_MEM_SIZE_SRAM);
								seed = crcTx;
							}

							// Write Data to SRAM
							lReturn = WriteSram(dataBlock);

							if (lReturn == SCARD_S_SUCCESS)
							{
								// Check if polling is required
								if (polling == TRUE)
									lReturn = Utils::WaitForI2CRead();
								else
									Sleep(30);
							}

							i--;
						}
						*writeTime = (float)clock() - *writeTime;

						Sleep(10);

						// Reset the seed to its initial value for the CRC32 calculation
						seed = 0xFFFFFFFF;
						uint32_t crcRx = 0;

						// Execute read test
						*readTime = (float)clock();
						i = chMultiplier;
						while (i > 0 && lReturn == SCARD_S_SUCCESS)
						{
							// Check if polling is required
							if (polling == TRUE)
								lReturn = Utils::WaitForI2CWrite();
							else
								Sleep(30);
							if (lReturn == SCARD_S_SUCCESS)
							{
								// data is ready, read from SRAM
								lReturn = ReadSram(dataBlock);
								if (i > 1)
								{
									crcRx = crc32(seed, dataBlock, NTAG_MEM_SIZE_SRAM);
									seed = crcRx;
								}
								else
									crcRx = crc32(seed, dataBlock, NTAG_MEM_SIZE_SRAM - 4);
							}
							i--;
						}
						*readTime = (float)clock() - *readTime;
						if (lReturn == SCARD_S_SUCCESS)
						{
							// Get the CRC values in reception
							uint32_t crcReceived = dataBlock[NTAG_MEM_SIZE_SRAM - 1] << 24 | dataBlock[NTAG_MEM_SIZE_SRAM - 2] << 16 |
								dataBlock[NTAG_MEM_SIZE_SRAM - 3] << 8 | dataBlock[NTAG_MEM_SIZE_SRAM - 4];
							*integrityRecv = (crcReceived == crcRx);

							// Check if the microcontroller detected an error in the CRC
							*integritySent = dataBlock[59] == 0x01 ? FALSE : TRUE;
						}
					}
				}
				free(Reader::ppioSendPci);
				SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
			}
			free(Reader::phCardHandle);

			return lReturn;
		};

