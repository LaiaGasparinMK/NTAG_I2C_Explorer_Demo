#ifndef Ntag_I2C_H
#define Ntag_I2C_H

#define TYPE_NDEF_TEXT 0
#define TYPE_NDEF_URI 1
#define TYPE_NDEF_BTPAIR 2
#define TYPE_NDEF_SP 3

#define MIME_BTPAIR "application/vnd.bluetooth.ep.oob"

#define TNF_WELL_KNOWN 1
#define TNF_MIME_TYPE 3

#define RTD_TEXT "T"
#define RTD_URI "U"
#define RTD_HS "Hs"
#define RTD_SP "Sp"

#if !defined(ARRAY_SIZE)
#define ARRAY_SIZE(x) (sizeof((x)) / sizeof((x)[0]))
#endif

// LPC11U24 parameters
#define SECTOR_SIZE	4096

// SRAM memory
#define NTAG_MEM_SIZE_SRAM 64

// User memory
#define NTAG_I2C_1k_MemSize 888
#define NTAG_I2C_2k_MemSize 1904

// Maximum total memory
#define MAX_MEM_SIZE 2048

// Maximum command/response length
#define MAX_TRANSCEIVE_LENGTH 252

// NTAG version identifier
#define NTAG_VERSION_1K 0x01
#define NTAG_VERSION_2K 0x02

// Reference Pages
#define SessionPage 0xF8
#define ConfigurationPage 0xE8
#define SRAM_Begin 0xF0
#define SRAM_Finish 0xFF
#define User_memory_Begin 0x04
#define UID 0x00;

// Reference offsets
#define NC_REG 0x00
#define LAST_NDEF_PAGE 0x01
#define SM_REG 0x02
#define WDT_LS 0x03
#define WDT_MS 0x04
#define I2C_CLOCK_STR 0x05
#define NS_REG 0x06
#define FIXED 0x07

// Bits of the NS_REG Register
#define RF_FIELD_PRESENT (0x01 << 0)
#define EEPROM_WR_BUSY (0x01 << 1)
#define EEPROM_WR_ERR (0x01 << 2)
#define SRAM_RF_READY (0x01 << 3)
#define SRAM_I2C_READY (0x01 << 4)
#define RF_LOCKED (0x01 << 5)
#define I2C_LOCKED (0x01 << 6) 
#define NDEF_DATA_READ (0x01 << 7)

// Bits of the NC_REG Register
#define PTHRU_DIR (0x01 << 0)
#define SRAM_MIRROR_ON_OFF (0x01 << 1)
#define FD_ON (0x03 << 2)
#define FD_OFF (0x03 << 4)
#define PTHRU_ON_OFF (0x01 << 6)
#define I2C_RST_ON_OFF (0x01 << 7)

// Offset of the Config Registers
#define NC_REG 0x00
#define LAST_NDEF_PAGE 0x01
#define SM_REG 0x02
#define WDT_LS 0x03
#define WDT_MS 0x04
#define I2C_CLOCK_STR 0x05
#define REG_LOCK 0x06
#define FIXED 0x07

//EEPROM?
#define SCARD_P_STATIC_LOCKED	0xFF000001
#define SCARD_P_DYNAMIC_LOCKED	0xFF000002
#define SCARD_P_CC_WRONG		0xFF000003
#define SCARD_P_CONFIG_LOCKED	0xFF000004

#endif