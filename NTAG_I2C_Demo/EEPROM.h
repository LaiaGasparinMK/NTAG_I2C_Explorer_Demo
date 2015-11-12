#ifndef EEPROM_H
#define EEPROM_H

static ref class EEPROM
{
public:
	static LONG ReadMemory(BYTE* data, DWORD* readBytes, float* readTime);
	static LONG ResetMemory(DWORD* resetBytes, float* resetTime);
	static LONG FlashMemory(BYTE* newFW, int newFWLength, DWORD* flashBytes, float* flashTime, System::Windows::Forms::ProgressBar^ progressBar1);
	static LONG EEPROMSpeedTest(int chMultiplier, BYTE* text, int* nWrittenBytes, float* writeTime, int* nReadBytes, float* readTime);

};

#endif