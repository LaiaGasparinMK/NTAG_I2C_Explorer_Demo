#ifndef NTAG_I2C_RF_H
#define NTAG_I2C_RF_H

public ref class NTAG_I2C_RF
{
public:
	static LONG GET_VERSION(BYTE* version);
	static LONG SECTOR_SELECT(BYTE nb_sector);
	static LONG END_SECTOR_SELECT();
	static LONG READ(BYTE page, BYTE* data);
	static LONG FAST_READ(BYTE startPage, BYTE endPage, BYTE* data);
	static LONG WRITE(BYTE page, BYTE* data);
};

#endif