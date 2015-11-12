#include "stdafx.h"
#include "NTAG_I2C_Logic.h"
#include "Reader.h"
#include "BoardDemo.h"


VOID NTAG_I2C_Logic::Init(){
	Reader::setContext();
	Reader::initReader();
	BoardDemo::initialize();
}



