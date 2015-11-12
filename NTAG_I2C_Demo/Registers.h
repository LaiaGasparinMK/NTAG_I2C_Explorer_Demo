#ifndef Ntag_I2C_Registers_H
#define Ntag_I2C_Registers_H

typedef struct Ntag_I2C_Registers
{
	char	REG_Manufacture[20];
	int		REG_Mem_size;
	BOOL	REG_I2C_RST_ON_OFF;
	char	REG_FD_OFF[3];
	char	REG_FD_ON[3];
	int		REG_LAST_NDEF_PAGE;
	BOOL	REG_NDEF_DATA_READ;
	BOOL	REG_RF_FIELD_PRESENT;
	BOOL	REG_PTHRU_ON_OFF;
	BOOL	REG_I2C_LOCKED;
	BOOL	REG_RF_LOCKED;
	BOOL	REG_SRAM_I2C_READY;
	BOOL	REG_SRAM_RF_READY;
	BOOL	REG_EEPROM_WR_BUSY;
	BOOL	REG_PTHRU_DIR;
	int		REG_SM_Reg;
	int		REG_WD_LS_Reg;
	int		REG_WD_MS_Reg;
	//	String	REG_NDEF_Message;
	BOOL	REG_SRAM_MIRROR_ON_OFF;
	BOOL	REG_I2C_CLOCK_STR;
} Ntag_I2C_Registers_t;

static ref class Registers
{
public:
	static LONG ReadSessionRegisters(Ntag_I2C_Registers_t* sessionReg);

	static LONG ReadConfigRegisters(Ntag_I2C_Registers_t* configReg);
	static LONG WriteConfigRegisters(Ntag_I2C_Registers_t configReg);

	static void createConfigRegisters(Ntag_I2C_Registers regs, BYTE* NC_Reg, BYTE* LD_Reg, BYTE* SM_Reg, BYTE* WD_LS_Reg, BYTE* WD_MS_Reg, BYTE* I2C_CLOCK_STR_Reg);
	static LONG GetConfigRegisters(Ntag_I2C_Registers_t* configReg);
	static LONG SetConfigRegisters(Ntag_I2C_Registers_t configReg);

	static void parseRegisters(Ntag_I2C_Registers *regs, BYTE input[16]);

};


#endif