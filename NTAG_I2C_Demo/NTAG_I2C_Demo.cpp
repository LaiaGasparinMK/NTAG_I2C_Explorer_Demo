// NTAG_I2C_Demo.cpp : main project file.

#include "stdafx.h"
#include "ProxAPI.h"
#include "MyForms.h"
#include "Form1.h"

using namespace NTAG_I2C_Demo;

[STAThreadAttribute]
int main(array<System::String ^> ^args)
{
	// Enabling Windows XP visual effects before any controls are created
	Application::EnableVisualStyles();
	Application::SetCompatibleTextRenderingDefault(false);

	try{
		// Create the main window and run it
		NTAG_I2C_Logic::Init();
		Application::Run(gcnew Form1());
	}
	catch (ObjectDisposedException^)
	{
	}

	return 0;
}
