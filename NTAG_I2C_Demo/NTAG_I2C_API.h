#ifndef NTAG_I2C_API_H
#define NTAG_I2C_API_H
ref class NTAG_I2C_API
{
public:
	static VOID Read_EEPROM();
	static VOID WriteEEPROM();

	static VOID Read_SRAM();
	static VOID Write_SRAM();

	static VOID GetSessionRegisters();

	static VOID GetAllConfigRegisters();
	static VOID SetAllConfigRegisters();

	static VOID GetConfigRegister();
	static VOID SetConfigRegister();
};

#endif