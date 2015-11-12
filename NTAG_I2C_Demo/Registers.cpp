#include "stdafx.h"
#include "Registers.h"
#include "NTAG_I2C_RF.h"
#include "Reader.h"



		LONG Registers::ReadSessionRegisters(Ntag_I2C_Registers_t* sessionReg)
		{
			LONG lReturn;
			BYTE data[16];

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

				// Get all the session registers. The Session Registers are located in Sector 3
				lReturn = NTAG_I2C_RF::SECTOR_SELECT(3);
				if (lReturn == SCARD_S_SUCCESS)
				{
					// Read session registers
					lReturn = NTAG_I2C_RF::READ(SessionPage, data);
					if (lReturn == SCARD_S_SUCCESS)
						parseRegisters(sessionReg, data);
				}
				NTAG_I2C_RF::END_SECTOR_SELECT();	// Necessary to enable the polling again and to go back to sector 0 when the SECTOR_SELECT Function has been executed
				free(Reader::ppioSendPci);
				SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
			}
			free(Reader::phCardHandle);

			return lReturn;
		}

		LONG Registers::ReadConfigRegisters(Ntag_I2C_Registers_t* configReg)
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

				// Get all the config registers
				lReturn = GetConfigRegisters(configReg);

				free(Reader::ppioSendPci);
				SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
			}
			free(Reader::phCardHandle);

			return lReturn;
		}

		LONG Registers::WriteConfigRegisters(Ntag_I2C_Registers_t configReg)
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

				// Set all the config registers
				lReturn = SetConfigRegisters(configReg);

				free(Reader::ppioSendPci);
				SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
			}
			free(Reader::phCardHandle);

			return lReturn;
		};

		LONG Registers::SetConfigRegisters(Ntag_I2C_Registers_t configReg)
		{
			LONG lReturn;
			BYTE data[4];

			// Get the bytes in the right form to be written into the tag
			BYTE NC_Reg = 0;
			BYTE LD_Reg = 0;
			BYTE SM_Reg = 0;
			BYTE WD_LS_Reg = 0;
			BYTE WD_MS_Reg = 0;
			BYTE I2C_CLOCK_STR_Reg = 0;
			createConfigRegisters(configReg, &NC_Reg, &LD_Reg, &SM_Reg, &WD_LS_Reg, &WD_MS_Reg, &I2C_CLOCK_STR_Reg);

			// The Configuration Registers may be located in sector 0 or sector 1
			BYTE NtagVersion;
			lReturn = NTAG_I2C_RF::GET_VERSION(&NtagVersion);
			if (lReturn == SCARD_S_SUCCESS)
			{
				if (NtagVersion == NTAG_VERSION_2K)
					lReturn = NTAG_I2C_RF::SECTOR_SELECT(1);

				// Write the Config Regs
				data[0] = NC_Reg;
				data[1] = LD_Reg;
				data[2] = SM_Reg;
				data[3] = WD_LS_Reg;
				lReturn = NTAG_I2C_RF::WRITE(ConfigurationPage, data);
				if (lReturn == SCARD_S_SUCCESS)
				{
					data[0] = WD_MS_Reg;
					data[1] = I2C_CLOCK_STR_Reg;
					data[2] = 0x00;
					data[3] = 0x00;
					lReturn = NTAG_I2C_RF::WRITE(ConfigurationPage + 1, data);
				}
				if (NtagVersion == NTAG_VERSION_2K)
					NTAG_I2C_RF::END_SECTOR_SELECT();	// Necessary to enable the polling again and to go back to sector 0 when the SECTOR_SELECT Function has been executed
			}

			return lReturn;
		}

		void Registers::parseRegisters(Ntag_I2C_Registers *regs, BYTE input[16])
		{
			BYTE NtagVersion = 0;
			if (NTAG_I2C_RF::GET_VERSION(&NtagVersion) == SCARD_S_SUCCESS)
			{
				if (NtagVersion == NTAG_VERSION_1K) {
					memcpy(regs->REG_Manufacture, "NXP NTAG I2C 1K", strlen("NXP NTAG I2C 1K") + 1);
					regs->REG_Mem_size = NTAG_I2C_1k_MemSize;
				}
				else if (NtagVersion == NTAG_VERSION_2K) {
					memcpy(regs->REG_Manufacture, "NXP NTAG I2C 2K", strlen("NXP NTAG I2C 2K") + 1);
					regs->REG_Mem_size = NTAG_I2C_2k_MemSize;
				}
				else
				{
					memcpy(regs->REG_Manufacture, "", strlen("") + 1);
					regs->REG_Mem_size = 0;
				}
			}
			else
			{
				memcpy(regs->REG_Manufacture, "", strlen("") + 1);
				regs->REG_Mem_size = 0;
			}

			BYTE NC_Reg = input[NC_REG];

			// check I2C_RST_ON_OFF
			//// more sense to do: regs->REG_I2C_RST_ON_OFF = (NC_Reg & I2C_RST_ON_OFF == I2C_RST_ON_OFF) , here and in all the rest
			if ((NC_Reg & I2C_RST_ON_OFF) == I2C_RST_ON_OFF)
				regs->REG_I2C_RST_ON_OFF = TRUE;
			else
				regs->REG_I2C_RST_ON_OFF = FALSE;

			// check FD_OFF
			BYTE tmpReg = (NC_Reg & FD_OFF);
			//// change to switch
			if (tmpReg == (0x30))
				memcpy(regs->REG_FD_OFF, "11b", 3);
			if (tmpReg == (0x20))
				memcpy(regs->REG_FD_OFF, "10b", 3);
			if (tmpReg == (0x10))
				memcpy(regs->REG_FD_OFF, "01b", 3);
			if (tmpReg == (0x00))
				memcpy(regs->REG_FD_OFF, "00b", 3);;

			// check FD_ON
			tmpReg = (NC_Reg & FD_ON);
			//// change to switch
			if (tmpReg == (0x0c))
				memcpy(regs->REG_FD_ON, "11b", 3);
			if (tmpReg == (0x08))
				memcpy(regs->REG_FD_ON, "10b", 3);
			if (tmpReg == (0x04))
				memcpy(regs->REG_FD_ON, "01b", 3);
			if (tmpReg == (0x00))
				memcpy(regs->REG_FD_ON, "00b", 3);

			// Last NDEF Page
			regs->REG_LAST_NDEF_PAGE = (0x00000FF & input[LAST_NDEF_PAGE]);

			BYTE NS_Reg = input[NS_REG];

			// check NDEF_DATA_READ
			if ((NS_Reg & NDEF_DATA_READ) == NDEF_DATA_READ)
				regs->REG_NDEF_DATA_READ = TRUE;
			else
				regs->REG_NDEF_DATA_READ = FALSE;

			// check RF_FIELD
			if ((NS_Reg & RF_FIELD_PRESENT) == RF_FIELD_PRESENT)
				regs->REG_RF_FIELD_PRESENT = TRUE;
			else
				regs->REG_RF_FIELD_PRESENT = FALSE;

			// check PTHRU_ON_OFF
			if ((NC_Reg & PTHRU_ON_OFF) == PTHRU_ON_OFF)
				regs->REG_PTHRU_ON_OFF = TRUE;
			else
				regs->REG_PTHRU_ON_OFF = FALSE;

			// check I2C_LOCKED
			if ((NS_Reg & I2C_LOCKED) == I2C_LOCKED)
				regs->REG_I2C_LOCKED = TRUE;
			else
				regs->REG_I2C_LOCKED = FALSE;

			// check RF_LOCK
			if ((NS_Reg & RF_LOCKED) == RF_LOCKED)
				regs->REG_RF_LOCKED = TRUE;
			else
				regs->REG_RF_LOCKED = FALSE;

			// check check SRAM_I2C_Ready
			if ((NS_Reg & SRAM_I2C_READY) == SRAM_I2C_READY)
				regs->REG_SRAM_I2C_READY = TRUE;
			else
				regs->REG_SRAM_I2C_READY = FALSE;

			// check SRAM_RF_READY
			tmpReg = (BYTE)(NS_Reg & SRAM_RF_READY);
			if ((NS_Reg & SRAM_RF_READY) == SRAM_RF_READY)
				regs->REG_SRAM_RF_READY = TRUE;
			else
				regs->REG_SRAM_RF_READY = FALSE;

			// check SRAM_RF_READY
			tmpReg = (BYTE)(NS_Reg & EEPROM_WR_BUSY);
			if ((NS_Reg & EEPROM_WR_BUSY) == EEPROM_WR_BUSY)
				regs->REG_EEPROM_WR_BUSY = TRUE;
			else
				regs->REG_EEPROM_WR_BUSY = FALSE;

			// check PTHRU_DIR
			tmpReg = (BYTE)(NC_Reg & (BYTE)0x01);
			if (tmpReg == (0x01))
				regs->REG_PTHRU_DIR = TRUE;
			else
				regs->REG_PTHRU_DIR = FALSE;

			// SM_Reg
			regs->REG_SM_Reg = (0x00000FF & input[SM_REG]);

			// WD_LS_Reg
			regs->REG_WD_LS_Reg = (0x00000FF & input[WDT_LS]);

			// WD_MS_Reg
			regs->REG_WD_MS_Reg = (0x00000FF & input[WDT_MS]);

			// check SRAM_MIRROR_ON_OFF
			if ((NC_Reg & SRAM_MIRROR_ON_OFF) == SRAM_MIRROR_ON_OFF)
				regs->REG_SRAM_MIRROR_ON_OFF = TRUE;
			else
				regs->REG_SRAM_MIRROR_ON_OFF = FALSE;

			// I2C_CLOCK_STR
			if (input[I2C_CLOCK_STR] == 1)
				regs->REG_I2C_CLOCK_STR = TRUE;
			else
				regs->REG_I2C_CLOCK_STR = FALSE;
		}

		// Gets an NTAG_I2C_Registers variable and leaves them in the right form to be written into the tag
		void Registers::createConfigRegisters(Ntag_I2C_Registers regs, BYTE* NC_Reg, BYTE* LD_Reg, BYTE* SM_Reg, BYTE* WD_LS_Reg, BYTE* WD_MS_Reg, BYTE* I2C_CLOCK_STR_Reg)
		{
			char* FD_OFF_Value = regs.REG_FD_OFF;
			char* FD_ON_Value = regs.REG_FD_ON;

			if (strncmp(FD_OFF_Value, "11b", 3) == 0)
				*NC_Reg = (*NC_Reg | 0x30);

			if (strncmp(FD_OFF_Value, "10b", 3) == 0) {
				*NC_Reg = (*NC_Reg & 0xcf);
				*NC_Reg = (*NC_Reg | 0x20);
			}

			if (strncmp(FD_OFF_Value, "01b", 3) == 0) {
				*NC_Reg = (*NC_Reg & 0xcf);
				*NC_Reg = (*NC_Reg | 0x10);
			}

			if (strncmp(FD_OFF_Value, "00b", 3) == 0)
				*NC_Reg = (*NC_Reg & 0xcf);

			if (strncmp(FD_ON_Value, "11b", 3) == 0)
				*NC_Reg = (*NC_Reg | 0x0c);

			if (strncmp(FD_ON_Value, "10b", 3) == 0)  {
				*NC_Reg = (*NC_Reg & 0xf3);
				*NC_Reg = (*NC_Reg | 0x08);
			}

			if (strncmp(FD_ON_Value, "01b", 3) == 0) {
				*NC_Reg = (*NC_Reg & 0xf3);
				*NC_Reg = (*NC_Reg | 0x04);
			}

			if (strncmp(FD_ON_Value, "00b", 3) == 0)
				*NC_Reg = (*NC_Reg & 0xf3);

			if (regs.REG_PTHRU_DIR == TRUE)
				*NC_Reg = (*NC_Reg | 0x01);
			else
				*NC_Reg = (*NC_Reg & 0xfe);

			*LD_Reg = regs.REG_LAST_NDEF_PAGE;
			*SM_Reg = regs.REG_SM_Reg;
			*WD_LS_Reg = regs.REG_WD_LS_Reg;
			*WD_MS_Reg = regs.REG_WD_MS_Reg;

			if (regs.REG_I2C_CLOCK_STR == TRUE)
				*I2C_CLOCK_STR_Reg = 1;
			else
				*I2C_CLOCK_STR_Reg = 0;

			if (regs.REG_I2C_RST_ON_OFF == TRUE)
				*NC_Reg = (*NC_Reg | 0x80);
			else
				*NC_Reg = (*NC_Reg & 0x7f);
		}

		// Gets the config registers of the NTAG
		LONG Registers::GetConfigRegisters(Ntag_I2C_Registers_t* configReg)
		{
			LONG lReturn;
			BYTE data[16];

			// Get all the config registers. The Configuration Registers may be located in sector 0 or sector 1
			BYTE NtagVersion;
			lReturn = NTAG_I2C_RF::GET_VERSION(&NtagVersion);
			if (lReturn == SCARD_S_SUCCESS)
			{
				if (NtagVersion == NTAG_VERSION_2K)
					lReturn = NTAG_I2C_RF::SECTOR_SELECT(1);
				// Read configuration registers
				lReturn = NTAG_I2C_RF::READ(ConfigurationPage, data);
				if (lReturn == SCARD_S_SUCCESS)
					parseRegisters(configReg, data);
				if (NtagVersion == NTAG_VERSION_2K)
					NTAG_I2C_RF::END_SECTOR_SELECT();	// Necessary to enable the polling again and to go back to sector 0 when the SECTOR_SELECT Function has been executed
			}
			return lReturn;
		}
