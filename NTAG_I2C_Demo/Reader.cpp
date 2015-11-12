#include "stdafx.h"
#include "Reader.h"
#include "BoardDemo.h"

namespace NTAG_I2C_Demo {

	using namespace System;

	static ref class Reader
	{
	public:
		SCARDCONTEXT* Reader::phContext;
		LPCTSTR szReader;
		SCARDHANDLE* phCardHandle;
		SCARD_IO_REQUEST* ppioSendPci;
		event System::EventHandler^ TagEvent;
		event System::EventHandler^ TagRemovedEvent;
		bool isReader = false;
		bool isLedDemo = false;
		bool endWait; //TODO: See if refactoring. Where to manage threads, not here.

		bool killThread = false;

	};
}
	void Reader::setContext(){
		Reader::phContext = (SCARDCONTEXT*)malloc(sizeof(SCARDCONTEXT));
	};

	void Reader::releaseContext(){
			SCardReleaseContext(*phContext);
			free(phContext);
		};

	void Reader::initReader(){ //TODO CHANGE NAME	
		if (Reader::szReader == nullptr){
			szReader = L"Identiv CLOUD 3700 F Contactless Reader 0";
		}
		if (SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, Reader::phContext) == SCARD_S_SUCCESS)
		{
			// Check if the reader is connected
			LPTSTR pmszReaders = NULL;
			DWORD cch = SCARD_AUTOALLOCATE;
			if (SCardListReaders(*phContext, NULL, (LPTSTR)&pmszReaders, &cch) == SCARD_S_SUCCESS)
			{
				LPTSTR pReader = pmszReaders;
				while ('\0' != *pReader)	// A double-null terminates the list of values
				{
					if (!wcscmp(pReader, Reader::szReader))
						isReader = true;
					pReader = pReader + wcslen(pReader) + 1;	// Advance to the next string
				}
			}
		}
	};
	

		//Returns the identiv FW version
LONG Reader::readIdentivFWversion(char* IdentivReaderFWversion)
{
	ReaderInfoExtended strReaderInfo;
	BYTE InByte, i;
	DWORD BytesRead, ActiveProtocol;
	ULONG ret;
	BYTE byMajorVersion;
	BYTE byMinorVersion;
			

	ret = SCardEstablishContext(SCARD_SCOPE_USER, NULL, NULL, Reader::phContext);
	if (ret == SCARD_S_SUCCESS)
	{
		ret = SCardConnect(*Reader::phContext, szReader, SCARD_SHARE_DIRECT, SCARD_PROTOCOL_UNDEFINED, Reader::phCardHandle, &ActiveProtocol);

		if (ret == SCARD_S_SUCCESS)
		{
			InByte = 0x1E;
			ret = SCardControl(*Reader::phCardHandle, IOCTL_CCID_ESCAPE, &InByte, 1, &strReaderInfo, sizeof(strReaderInfo), &BytesRead);
			if (SCARD_S_SUCCESS == ret)
			{
				IdentivReaderFWversion[0] = '0' + strReaderInfo.byMajorVersion;
				IdentivReaderFWversion[1] = '.';
				IdentivReaderFWversion[2] = '0' + strReaderInfo.byMinorVersion;
			}
		}
	}
	SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
	return ret;
};

LONG Reader::cardConnect_DIRECT_Mode(){}

LONG Reader::cardConnect_SHARED_Mode(){}

LONG Reader::cardConnect_EXCLUSIVE_Mode(){}

	/*
	VOID cardDetection(){
	// PC/SC variables
	SCARD_READERSTATE sReaderState;
	memset(&sReaderState, 0, sizeof(SCARD_READERSTATE));
	sReaderState.szReader = Reader::szReader;

	while (this->killThread == false)
	{
	this->endWait = false;

	// Wait until a tag is detected
	do
	{
	System::Threading::Thread::Sleep(100);
	SCardGetStatusChange(*Reader::phContext, INFINITE, &sReaderState, 1);
	} while ((sReaderState.dwEventState & SCARD_STATE_PRESENT) != SCARD_STATE_PRESENT && this->killThread == false);
	if (this->killThread == false)
	{

	TagEvent(this, gcnew EventArgs());

	bool isTagPresent = keepOnTagReading();

	if (!isTagPresent)
	TagRemovedEvent(this, gcnew EventArgs());
	}
	}
	};

	bool keepOnTagReading(){
	bool isTag = true;
	do
	{
	System::Threading::Thread::Sleep(100);
	isTag = checkIfTag();
	if (!this->killThread && this->isLedDemo)	// If we are in the Demo tab, we need to keep on reading the tag
	{
	TagEvent(this, gcnew EventArgs());

	}
	} while (isTag && !this->endWait && !this->killThread);
	return isTag;
	}

	// Checks if there is a tag present
	bool checkIfTag()
	{
	LONG lReturn;
	bool isTag = false;

	// Connect to card
	Reader::phCardHandle = (SCARDHANDLE*)malloc(sizeof(SCARDHANDLE));
	DWORD dwAP; //Active protocol
	lReturn = SCardConnect(*Reader::phContext, Reader::szReader, SCARD_SHARE_SHARED, SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1, Reader::phCardHandle, &dwAP);
	if (lReturn == SCARD_S_SUCCESS)
	{
	isTag = true;
	SCardDisconnect(*Reader::phCardHandle, SCARD_LEAVE_CARD);
	}

	return isTag;
	}*/