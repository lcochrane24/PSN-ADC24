/* Header file for TestPicSdr.cpp */

#include <stdio.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <conio.h>
#include <time.h>
#include <math.h>

#define	MAX_CHANNELS				4			// Maximum number of channels
#define MAX_SPS						200			// Maximum sample rate

#define BYTE						unsigned char
#define WORD						unsigned short
#define ULONG						unsigned long
#define LONG						long
#define BOOL						int

#define TIME_CONFIG_FILE			"time.dat"	// name of the file used to save adjust time info

#define COMM_PORT					"1"			// Comm port to open
#define PORT_SPEED					38400		// Comm port baud rate
#define SAMPLE_RATE					100			// Set the sample rate.
#define NUMBER_CHANNELS				1			// Set the number of channels. Can be 1 to 4

#define VCO_ON_OFF_TIME				50			// Set start vco voltage percentage

#define TIME_REF_USEPC				0			// PC's clock as reference
#define TIME_REF_GARMIN				1			// Garmin 16/18
#define TIME_REF_SKG				2			// SKG / Sure Elect. Board
#define TIME_REF_4800				3			// 4800 baud OEM receiver
#define TIME_REF_9600				4			// 9600 baud OEM receiver

#define GPS_NOT_LOCKED				0			// GPS lock statues
#define GPS_WAS_LOCKED				1
#define GPS_LOCKED					2

#define FG_PPS_HIGH_TO_LOW			0x0001
#define FG_DISABLE_LED				0x0002
#define FG_WATCHDOG_CHECK			0x0004
#define FG_WAAS_ON					0x0008
#define FG_2D_ONLY					0x0010

#define GF_GAIN_MASK				0x07
#define GF_UNIPOLAR_INPUT			0x08
#define GF_VREF_GT25				0x10

#define	HIGH_TO_LOW_PPS_FLAG        0x01		// Config flags sent to the ADC board 
#define	NO_LED_PPS_FLAG        		0x02

#define PC_LOCK_STATUS				1
#define MAX_LOC_AVG					4
#define MAX_AVG_DIFF				10

#pragma pack(1)
	
/* Packet header information sent from the ADC board */
typedef struct  {
	BYTE hdr[4];
	WORD len;
	BYTE type, flags;
} PreHdr;
	
/* Data packet header information sent from the ADC board */
typedef struct  {						// A/D packet header
	ULONG packetID, sampleTick, ppsTick, gpsTOD, notUsed;
	BYTE  loopError, gpsLockSts, gpsSatNum, gpsMonth, gpsDay, gpsYear;
} Sdr24DataHdr;

/* Status information sent from the ADC board */
typedef struct  {
	BYTE boardType, majorVersion, minorVersion, numChannels; 
	WORD sps;
	WORD test;
} StatusInfo;

/* Time information sent from the ADC board */
typedef struct  {
	long data;
	WORD msCount;
} SendTimeInfo;

/*  The following structures are used to send data to the ADC board */

/* Used to save the add/drop time interval and add time flag in the ADC's EEProm */
typedef struct  {
	BYTE modeNumConverters;
	BYTE goodConverterFlags;
	BYTE notUsed1;
	BYTE notUsed2;
} BoardInfoSdr24;

/* Used to adjust the 1 ms time accumulator on the ADC board */
typedef struct  {
	LONG adjustTime;
	BYTE reset;
} TimeInfo;

typedef struct  {
	WORD onTime;
	WORD offTime;
} VcoInfo;

typedef struct {
	ULONG offset;
	ULONG gain;
} AdcGainOffset;

/* Configuration information sent to the ADC board at startup */
typedef struct  {	// main system config info packet
	WORD flags;
	WORD sps;
	WORD vcoOnTime;
	WORD vcoOffTime;
	ULONG timeTick;
	BYTE numChannels;
	BYTE timeRefType;
	BYTE gainFlags[ MAX_CHANNELS ];
} Sdr24ConfigInfo;

#pragma pack()

typedef struct {
	int gain;
	int gainBits;
	int adcBits;
	int adcBitsShift;
	int unipolarFlag;
} ChanConfig;

typedef struct {
	int highCount;
	int lowCount;
	double average;
	int avgCount;	
} ChanStats;

BOOL GetCommData();
BOOL OpenPort( char *portStr, int speed );
BOOL TcpIpConnect( char *portStr, int speedOrPort );
BOOL GetUser();
BYTE CalcCRC( BYTE *cp, short cnt );
BYTE GetGainBits( int gain );
BOOL NewGpsTimeSec();
int GetEchoChar();
void ClosePort();
void SendPacket( char chr );
void ProcessNewPacket();
void ResetBoard();
void EchoMode();
void ProcessGpsData();
void ProcessDataPacket();
void LogFileMessage();
void ProcessStatusPacket();
void SendConfigPacket();
void logWrite( char *pszFormat, ...);
void SendCommData( BYTE *data, int len, int fast );
void SaveTimeInfo();
void ReadTimeInfo();
void ProcessConfig();
void ReadConfig();
void TimeRefGps( BYTE *newPacket, char *timeStatusStr );
void DoTest();
void SetVco( WORD onOffTime );
void AdcInfoPacket();
void AckPacket();
void StartStopGps(BOOL);
void PutEchoChar( char chr );
void DoStats();
void AddStats();
void InitGpsRef();
void CheckLockSts();
void MakeStatusStr( char *str );
void fLogWrite( char *pszFormat, ...);
void SendGpsCmd( char *cmd );
