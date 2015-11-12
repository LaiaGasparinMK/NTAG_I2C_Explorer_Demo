#include "stdafx.h"
#include "NDEFmsg.h"
#include "Reader.h"
#include "NDEFRecord.h"
#include "URI.h"
#include "SRAM.h"
#include "NTAG_I2C_RF.h"

LONG NDEFmsg::ReadNdef(int* NdefType, BYTE* payload, int* payloadSize, BOOL *AARPresent, BYTE* AAR, int*AARSize, DWORD* readBytes, float* readTime)
		{
			LONG lReturn;
			BYTE data[MAX_MEM_SIZE];
			int length;
			*payloadSize = 0;

			// Connect to card (no need for exclusive access, as everything is done in sector 0)
			Reader::phCardHandle = (SCARDHANDLE*)malloc(sizeof(SCARDHANDLE));
			DWORD dwAP; //Active protocol
			lReturn = SCardConnect(*Reader::phContext, Reader::szReader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, Reader::phCardHandle, &dwAP);
			if (lReturn == SCARD_S_SUCCESS)
			{
				Reader::ppioSendPci = (SCARD_IO_REQUEST*)malloc(sizeof(SCARD_IO_REQUEST));
				Reader::ppioSendPci->dwProtocol = dwAP;
				Reader::ppioSendPci->cbPciLength = 8;

				*readTime = (float)clock();
				// Get the NDEF message
				lReturn = ReadNdefMessage(data, &length);
				*readBytes = (int)ceil((float)(length + 6) / 16) * 16;	// Bytes are read 16 by 16
				if (lReturn == SCARD_S_SUCCESS)
				{
					// The TNF is defined in the last three bits of the first byte of the NDEF Message
					BYTE TNF = data[0] & 0x07;
					BYTE typeLength = data[1];
					BYTE payloadLength = data[2];
					// From byte 3, the NDEF type is defined as an string of length typeLength
					BYTE* Type = (BYTE*)malloc((typeLength + 1)*sizeof(BYTE));
					memcpy(Type, data + 3, typeLength);
					Type[typeLength] = '\0';
					// Get the message and message type
					if (TNF == TNF_WELL_KNOWN)
					{
						// Check if it is a text-type NDEF message
						int comp = strcmp((char*)Type, RTD_TEXT);
						if (comp == 0)
						{
							*NdefType = TYPE_NDEF_TEXT;
							// We ignore the language information bytes in the payload (3bytes)
							*payloadSize = payloadLength - 3;
							memcpy(payload, data + 3 + typeLength + 3, *payloadSize);
						}
						else
						{
							// Check if it is an URI-type NDEF message
							comp = strcmp((char*)Type, RTD_URI);
							if (comp == 0)
							{
								*NdefType = TYPE_NDEF_URI;
								// The first byte in the URI payload indicates the type of URI stored
								char uriType[30];
								BYTE uriLength;
								
								lReturn = URI::getUriType((BYTE)uriType, (char*)data[3 + typeLength], &uriLength);

								if (lReturn == SCARD_S_SUCCESS)
								{
									*payloadSize = uriLength + payloadLength - 1;
									memcpy(payload, uriType, uriLength);
									memcpy(payload + uriLength, data + 3 + typeLength + 1, payloadLength - 1);
								}
							}
							else
							{
								// Check if it is a Bluetooth handover-type NDEF message
								comp = strcmp((char*)Type, RTD_HS);
								if (comp == 0)
								{
									*NdefType = TYPE_NDEF_BTPAIR;
									// Skip the HS Record header
									char* btRecord = (char*)data + 5 + data[2];
									payload[0] = btRecord[1];
									btRecord += 3;
									memcpy(payload + 1, btRecord, payload[0]);
									if (strncmp(btRecord, MIME_BTPAIR, strlen(MIME_BTPAIR)) == 0)
									{
										// It is a Bluetooth pairing record
										NDEFRecord::parseBTPairRecord(btRecord + strlen(MIME_BTPAIR), (char*)payload + 1 + strlen(MIME_BTPAIR), payloadSize);	// It always returns SCARD_S_SUCCESS
										*payloadSize += 1 + strlen(MIME_BTPAIR);
									}
									else
									{
										payload[1 + strlen(MIME_BTPAIR)] = 0;
										*payloadSize = 1 + strlen(MIME_BTPAIR);
									}
								}
								else
								{
									// Check if it is a Smart Poster-type NDEF message
									comp = strcmp((char*)Type, RTD_SP);
									if (comp == 0)
									{
										*NdefType = TYPE_NDEF_SP;
										// The first element in the Smart Poster should be Text-type
										if (data[3 + typeLength] == 0x91 && data[3 + typeLength + 1] == 0x01 && data[3 + typeLength + 3] == 'T')
										{
											// We ignore the language information bytes in the payload (3bytes)
											payload[0] = data[3 + typeLength + 2] - 3;
											*payloadSize = payload[0] + 1;
											memcpy(payload + 1, data + 3 + typeLength + 4 + 3, payload[0]);

											// The second element in the Smart Poster should be URI type
											if (data[3 + typeLength + 4 + 3 + payload[0]] == 0x51 && data[3 + typeLength + 4 + 3 + payload[0] + 1] == 0x01 && data[3 + typeLength + 4 + 3 + payload[0] + 3] == 'U')
											{
												// The first byte in the URI payload indicates the type of URI stored
												char uriType[30];
												BYTE uriLength;

												lReturn = URI::getUriType((BYTE)uriType, (char*)data[3 + typeLength + 4 + 3 + payload[0] + 4], &uriLength);

												if (lReturn == SCARD_S_SUCCESS)
												{
													payload[*payloadSize] = uriLength + data[3 + typeLength + 4 + 3 + payload[0] + 2] - 1;
													memcpy(payload + *payloadSize + 1, uriType, uriLength);
													memcpy(payload + *payloadSize + 1 + uriLength, data + 3 + typeLength + 4 + 3 + payload[0] + 5, data[3 + typeLength + 4 + 3 + payload[0] + 2] - 1);
													*payloadSize += uriLength + data[3 + typeLength + 4 + 3 + payload[0] + 2] - 1 + 1;
												}
											}
											else
												lReturn = SCARD_E_INVALID_TARGET;
										}
										else
											lReturn = SCARD_E_INVALID_TARGET;
									}
									else
										lReturn = SCARD_E_INVALID_TARGET;
								}
							}
						}
					}
					// This way it can also be a Bluetooth handover type NDEF message
					else if (TNF == TNF_MIME_TYPE)
					{
						*NdefType = TYPE_NDEF_BTPAIR;
						// Skip the HS Record header
						char* btRecord = (char*)data + 5 + data[2];
						payload[0] = btRecord[1];
						btRecord += 3;
						memcpy(payload + 1, btRecord, payload[0]);
						if (strncmp(btRecord, MIME_BTPAIR, strlen(MIME_BTPAIR)) == 0)
						{
							// It is a Bluetooth pairing record
							NDEFRecord::parseBTPairRecord(btRecord + strlen(MIME_BTPAIR), (char*)payload + 1 + strlen(MIME_BTPAIR), payloadSize);	// It always returns SCARD_S_SUCCESS
							*payloadSize += 1 + strlen(MIME_BTPAIR);
						}
						else
						{
							payload[1 + strlen(MIME_BTPAIR)] = 0;
							*payloadSize = 1 + strlen(MIME_BTPAIR);
						}
					}
					else
						lReturn = SCARD_E_INVALID_TARGET;

					//Check if the NDEF message contains an AAR at the end
					// For text, URI and Smart Poster type NDEFs, if we store them without an AAR, data[0] is 0xD1 (one record), while if we store them with an AAR, data[0] is 0x91 (two records)
					// For BT Pairing type NDEFs, data[0] is 0x91 in both cases (two records without AAR and three records with AAR)
					if (data[0] == 0x91 && lReturn == SCARD_S_SUCCESS)
					{
						char* aarRecord = NULL;
						if (*NdefType == TYPE_NDEF_BTPAIR)
						{
							if (length > 3 + typeLength + payloadLength + 3 + data[3 + typeLength + payloadLength + 1] + data[3 + typeLength + payloadLength + 2])
								aarRecord = strstr((char*)(data + 3 + typeLength + payloadLength + 3 + data[3 + typeLength + payloadLength + 1] + data[3 + typeLength + payloadLength + 2] + 3), "android.com:pkg");
						}
						else
							aarRecord = strstr((char*)(data + 3 + typeLength + payloadLength + 3), "android.com:pkg");
						if (aarRecord != NULL)
						{
							*AARPresent = TRUE;
							*AARSize = data - (BYTE*)aarRecord + length - strlen("android.com:pkg");
							memcpy(AAR, aarRecord + strlen("android.com:pkg"), *AARSize);
						}
					}
					*readTime = (float)clock() - *readTime;

					free(Type);
				}
				free(Reader::ppioSendPci);
				SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
			}
			free(Reader::phCardHandle);

			return lReturn;
		}

		// Writes the NDEF type indicated with the content indicated
		LONG NDEFmsg::WriteNdef(int NdefType, BYTE* payload, int payloadSize, BOOL addAAR, DWORD* writeBytes, float* writeTime)
		{
			LONG lReturn;
			BYTE message[MAX_MEM_SIZE];
			int length;

			// Connect to card (no need for exclusive access, as everything is done in sector 0)
			Reader::phCardHandle = (SCARDHANDLE*)malloc(sizeof(SCARDHANDLE));
			DWORD dwAP; //Active protocol
			lReturn = SCardConnect(*Reader::phContext, Reader::szReader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, Reader::phCardHandle, &dwAP);
			if (lReturn == SCARD_S_SUCCESS)
			{
				Reader::ppioSendPci = (SCARD_IO_REQUEST*)malloc(sizeof(SCARD_IO_REQUEST));
				Reader::ppioSendPci->dwProtocol = dwAP;
				Reader::ppioSendPci->cbPciLength = 8;

				*writeTime = (float)clock();
				// Create the NDEF message
				switch (NdefType)
				{
				case TYPE_NDEF_TEXT:
					lReturn = NDEFRecord::createTextRecord(payload, payloadSize, message, &length);
					break;
				case TYPE_NDEF_URI:
					lReturn = NDEFRecord::createUriRecord(payload, payloadSize, message, &length);
					break;
				case TYPE_NDEF_BTPAIR:
					lReturn = NDEFRecord::createBTPairRecord(payload, payloadSize, message, &length);
					break;
				case TYPE_NDEF_SP:
					lReturn = NDEFRecord::createSPRecord(payload, payloadSize, message, &length);
					break;
				}
				if (lReturn == SCARD_S_SUCCESS)
				{
					// If required, append the AAR to the NDEF message
					if (addAAR)
						lReturn = AppendAAR(message, &length);
					if (lReturn == SCARD_S_SUCCESS)
					{
						// Write the NDEF message to the tag
						lReturn = WriteNdefMessage(message, length);
						*writeTime = (float)clock() - *writeTime;
						*writeBytes = (int)ceil((float)(length + 2) / 4) * 4; // Bytes are writen 4 by 4
						if (lReturn == SCARD_S_SUCCESS)
						{
							// Inform the BoardDemo about the new NDEF message that has been stored
							BYTE wData[NTAG_MEM_SIZE_SRAM] = { 0 };
							wData[60] = 'N';
							SRAM::WriteSram(wData);
						}
					}
				}
				free(Reader::ppioSendPci);
				SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
			}
			free(Reader::phCardHandle);

			return lReturn;
		}

		// Reads an NDEF message from the tag
		LONG NDEFmsg::ReadNdefMessage(BYTE* data, int* length)
		{
			LONG lReturn;
			BYTE cc[16];

			// Check that in page 3 there is the NDEF Magic Number, that in page 4 the tag field is an NDEF message, and the length of the NDEF message
			lReturn = NTAG_I2C_RF::READ(3, cc);
			if (lReturn == SCARD_S_SUCCESS && cc[0] == 0xE1 && cc[4] == 0x03)
			{
				*length = cc[5];
				// Copy into data the bytes of the NDEF message that I already have
				memcpy(data, cc + 6, 10);
				// Send read commands until I have read the whole NDEF
				int i = (int)ceil(((float)(*length - 10 + 1)) / 16);
				BYTE* pos = data + 10;
				BYTE page = 7;
				while (i > 0 && lReturn == SCARD_S_SUCCESS)
				{
					lReturn = NTAG_I2C_RF::READ(page, pos);
					i--;
					pos += 16;
					page += 4;
				}
			}
			else if (lReturn == SCARD_S_SUCCESS)	// If the tag is not NDEF formatted
				lReturn = SCARD_E_INVALID_TARGET;

			return lReturn;
		}

		// Writes an NDEF message into the tag
		LONG NDEFmsg::WriteNdefMessage(BYTE* data, int length)
		{
			LONG lReturn;
			BYTE cc[16];

			// Check that in page 3 there is the NDEF Magic Number
			lReturn = NTAG_I2C_RF::READ(3, cc);
			if (lReturn == SCARD_S_SUCCESS && cc[0] == 0xE1)
			{
				// Send write commands until I have written the whole NDEF
				int i = (int)ceil(((float)(length)) / 4);
				BYTE* pos = data;
				BYTE page = 4;
				data[length] = 0x00;
				data[length + 1] = 0x00;
				data[length + 2] = 0x00;
				while (i > 0 && lReturn == SCARD_S_SUCCESS)
				{
					lReturn = NTAG_I2C_RF::WRITE(page, pos);
					i--;
					pos += 4;
					page++;
				}
			}
			else if (lReturn == SCARD_S_SUCCESS)	// If the tag is not NDEF formatted
				lReturn = SCARD_E_INVALID_TARGET;

			return lReturn;
		}

		BYTE* NDEFmsg::CreateDefaultNDEFmessage(BYTE message[], int length){
			BYTE payload[MAX_MEM_SIZE];
			payload[0] = strlen("NTAG I2C EXPLORER");
			memcpy(payload + 1, "NTAG I2C EXPLORER", strlen("NTAG I2C EXPLORER"));
			payload[1 + strlen("NTAG I2C EXPLORER")] = strlen("http://www.nxp.com/demoboard/OM5569");
			memcpy(payload + 1 + strlen("NTAG I2C EXPLORER") + 1, "http://www.nxp.com/demoboard/OM5569", strlen("http://www.nxp.com/demoboard/OM5569"));
			payload[1 + strlen("NTAG I2C EXPLORER") + 1 + strlen("http://www.nxp.com/demoboard/OM5569")] = '\0'; // Title Length + Title + Link Length + Link
			return payload;
		}

		// Appends an AAR to an NDEF message
		LONG NDEFmsg::AppendAAR(BYTE* message, int* length)
		{
			// AAR header
			BYTE AARheader[] = { 0x54, 0x0F, 0x17 };
			int AARheaderSize = ARRAY_SIZE(AARheader);

			// AAR text
			BYTE AARtext[] = "android.com:pkgcom.nxp.ntagi2cdemo_dev";
			int AARtextSize = ARRAY_SIZE(AARtext) - 1;	// We don't count here the '\0' at the end

			// NDEF message
			memcpy(message + *length - 1, AARheader, AARheaderSize);
			memcpy(message + *length - 1 + AARheaderSize, AARtext, AARtextSize);
			message[*length - 1 + AARheaderSize + AARtextSize] = 0xFE;
			*length += AARheaderSize + AARtextSize;
			message[1] += AARheaderSize + AARtextSize;
			message[2] = 0x91;

			return SCARD_S_SUCCESS;
		};


