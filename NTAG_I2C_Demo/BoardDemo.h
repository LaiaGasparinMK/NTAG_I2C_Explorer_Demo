#ifndef BoardDemo_H
#define BoardDemo_H

//TODO: ENUM // My LED color identifiers
#define RED		1
#define BLUE	2
#define GREEN	3
#define OFF     0

typedef struct Versions
{
	char	Board_Version[18];
	char	Board_FW_Version[18];
}Versions_t;

static ref class BoardDemo
{
public:
	static BYTE* prevTempC;
	static BYTE* prevTempF;
	static BYTE* prevVolt;
	static VOID initialize();
	static VOID destroy();
	static LONG Run(char ledColor, BOOL enTemp, BOOL enLCD, BOOL dispNDEF, BOOL* redButton, BOOL* greenButton, BOOL* blueButton, float* tempC, float* tempF, float* voltage, float* version);
	static LONG ReadVersions(Versions_t* versions);
};

#endif