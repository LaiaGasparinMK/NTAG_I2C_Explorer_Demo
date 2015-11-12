#ifndef SRAM_H
#define SRAM_H

static ref class SRAM
{
public:
	static LONG PerformSramCom(BYTE*cmd, BYTE* resp);
	static LONG ReadSram(BYTE* data);
	static LONG WriteSram(BYTE* data);
	static LONG SRAMSpeedTest(int chMultiplier, BOOL polling, BOOL* integritySent, BOOL* integrityRecv, float* writeTime, float* readTime);
};

#endif