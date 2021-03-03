/* sdr.h */

#include <string.h>

#define BOARD_TYPE			5				// SDR24 Based A/D board
#define MAJOR_VER			2
#define MINOR_VER			5

#define MAX_CHANNELS		4

#define SLONG 				long
#define SINT 				short

#define ULONG				unsigned long
#define WORD				unsigned short
#define BYTE				unsigned char
#define UINT 				unsigned int

#define TICKS_PER_DAY		138240000L

#define NO_HOST_DATA_TIMER	900			// 90 sec

#define FG_PPS_HIGH_TO_LOW	0x0001
#define FG_DISABLE_LED		0x0002
#define FG_WATCHDOG_CHECK	0x0004
#define FG_WAAS_ON			0x0008
#define FG_2D_ONLY			0x0010

#define GF_GAIN_MASK		0x07
#define GF_UNIPOLAR_INPUT	0x08
#define GF_VREF_GT25		0x10

#define MAX_DATA_LEN 		( ( ( 200 * 3 * MAX_CHANNELS ) / 10 ) + 10 )
#define DATA_PACKET_LEN		( sizeof( PreHdr ) + sizeof( Sdr24DataHdr ) + MAX_DATA_LEN )

#define IN_BUFFER_SIZE 		48
#define GPS_IN_QUEUE		32
#define GPS_BUFFER_LEN		120
#define GPS_OUT_LEN			52
#define MAX_GPS_OUT_LEN		128
#define AUX_PACKET_LEN		128

#define TRUE				1
#define FALSE				0

#define TIME_SDR24_USEPC	0		// PC's clock as reference
#define TIME_SDR24_GARMIN	1		// Garmin 16/18
#define TIME_SDR24_SKG		2		// SKG/Sure Elec. Board
#define TIME_SDR24_4800		3		// GPS OEM Receiver @ 4800 baud
#define TIME_SDR24_9600		4		// GPS OEM Receiver @ 9600 baud

#define USB_RST_PIN			LATBbits.LATB12
#define USB_TRIS_BIT		TRISBbits.TRISB12

#define SW1_BIT				PORTBbits.RB9 
#define SW2_BIT				PORTBbits.RB10 
#define SW3_BIT				PORTBbits.RB11 

#define PPS_PIN				PORTDbits.RD8

#define LED_PIN				LATDbits.LATD2
#define TEST_PIN			LATDbits.LATD9

#define SPI_CLK				LATBbits.LATB8
#define SPI_OUT				LATDbits.LATD0
	
#define VCO_OUT				LATDbits.LATD3

#define SPI_LONG_DELAY		5
#define SPI_SHORT_DELAY		3

typedef struct  {
	WORD rate;
	WORD sampleTimer;
	BYTE packetsPerSec;
	BYTE frsOn;
	BYTE bits;
} SpsInfo;
	
typedef struct  {
	BYTE modeNumConverters;
	BYTE goodConverters;
	BYTE notUsed1;
	BYTE notUsed2;
} BoardInfoSdr24;

typedef struct  {						// serial data i/o header
	BYTE hdr[4];
	WORD len;
	BYTE type, flags;
} PreHdr;
	
typedef struct  {						// A/D packet header
	ULONG packetID, sampleTick, ppsTick, gpsTOD, notUsed;
	BYTE  loopError, gpsLockSts, gpsSatNum, gpsMonth, gpsDay, gpsYear;
} Sdr24DataHdr;

typedef struct  {
	ULONG offset;
	ULONG gain;
} AdcGainOffset;

typedef struct  {						// main system config info packet
	WORD flags;
	WORD sps;
	WORD vcoOnTime;
	WORD vcoOffTime;
	ULONG timeTick;
	BYTE numChannels;
	BYTE timeRefType;
	BYTE gainFlags[ MAX_CHANNELS ];
} Sdr24ConfigInfo;

typedef struct  {
	BYTE boardType, majVer, minVer, numChannels; 
	WORD sps;
	WORD test;
} StatusInfo;

typedef struct  {
	ULONG timeTick;
	BYTE reset;
} TimeInfo;
	
typedef struct  {
	SLONG data;
	WORD msCount;
} SendTimeInfo; 

typedef struct  {
	SLONG timeAdjust;
	BYTE reset;
} TimeDiffInfo;

typedef struct  {
	long gain;
	long offset;
} AdcChanInfo;

typedef struct  {
	WORD onTime;
	WORD offTime;
} VcoInfo; 

void Init();
void Restart();
void SendDataPacket();
void MakeHdr();
void CollectData();
void CheckTime();
void NewGpsTimeNMEA();
void NewGpsTimeMot();
void NewCommand();
void WaitForConfig();
void NewConfig();
void TimeTest();
void SetBaudRate( int startInt );
BYTE SendGpsLine();
BYTE GpsCommCheck();
BYTE ParseGpsLine();
BYTE TestGpsLine();
BYTE ProcInData();
BYTE ReadBaudModeSwitch();
BYTE WiFiTest();
SINT ReadAD();
void GetGpsLine( BYTE line );
void CheckSendGps();
void EchoGps();
BYTE ProcMotBinChar( BYTE chr );
void SendMotBin( BYTE type, BYTE num );
void NewGpsTimeBin();
void InitGps();
void SendAllGps();
void CheckSendGps();
void ClearGpsOut();
void QueueGpsPacket();
void SendTime();
void SendTimeDiff();
void SetTimeInfo();
void SetGpsTime();
void SaveAdjInfo();
void SendAck();
void GotoBootLdr();
void SwapWord( WORD *in );
void DelayMs( int ms );
void SendHost();
void SetGpsBaud( int startInt );
void SendDummyPacket();
void Check100Ms();
void NewSample();
void SetVco();
void GetAdcGainOffsets();
void SendAdcInfo();
void DoCS5532OffsetGainCal( BYTE chan, BYTE type );
