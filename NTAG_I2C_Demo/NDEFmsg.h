#ifndef NDEFmsg_H
#define NDEFmsg_H
static ref class  NDEFmsg
{
public:
	static LONG ReadNdef(int* NdefType, BYTE* payload, int* payloadSize, BOOL *AARPresent, BYTE* AAR, int*AARSize, DWORD* readBytes, float* readTime);
	static LONG WriteNdef(int NdefType, BYTE* payload, int payloadSize, BOOL addAAR, DWORD* writeBytes, float* writeTime);
	
	static LONG ReadNdefMessage(BYTE* data, int* length);
	static LONG WriteNdefMessage(BYTE* data, int length);
	
	static BYTE* CreateDefaultNDEFmessage(BYTE message[], int length);
	static LONG AppendAAR(BYTE* message, int* length);
};

#endif