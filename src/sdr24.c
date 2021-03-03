/* sdr24.c */

#ifdef CPU4013
#include "p30F4013.h"
#else
#include "p30F3014.h"
#endif

_FOSC( CSW_FSCM_OFF & XT_PLL8 );
_FWDT(WDT_OFF & WDTPSA_64 & WDTPSB_4);
_FBORPOR( PBOR_ON & BORV_42 & PWRT_16 & MCLR_EN );
_FGS( CODE_PROT_OFF );

#include <uart.h>
#include <stdio.h>
#include <stdlib.h>
#include "sdr24.h"

BYTE ppsLowHiFlag, setGpsTimeFlag, outBuffer, samplesPerPacket;
BYTE newInData, sendLog, exitLoop, sendStatus, goodConfig;
BYTE ppsHiToLowFlag, ledPPS, newCommandFlag, checkErr, sendTime;
BYTE echoMode, newEchoChr, resetTime;
BYTE sendAckFlag, packetsPerSec, sendAdcInfoFlag, calcFlag;

BYTE inBuffer[IN_BUFFER_SIZE], inBuffQueue[ IN_BUFFER_SIZE ];
volatile BYTE *inPtrQue;
 
BYTE dataPacket[DATA_PACKET_LEN], dataPacket1[DATA_PACKET_LEN], auxPacket[AUX_PACKET_LEN], hdrStr[4], *txOutPtr;
BYTE *adDataPtr;

char gpsInBuffer[ GPS_BUFFER_LEN ], gpsInQueue[ GPS_IN_QUEUE], lastGpsChar;
WORD gpsInCount, gpsQueueInCount, gpsQueueOutCount, newGpsData;

WORD secTick, secTickValue, secOffValue, gpsTimer;
ULONG packetID, gpsTimeOfDay, timeTick, ppsTick, timeStamp;

Sdr24DataHdr *currHdr;

WORD txOutCount, sampleDataLen, sampleOffset;

BYTE sampleCount, numChannels, timeRefType, noWaitFlag, goodChanFlags;
WORD spsRate, noDataCounter;

Sdr24ConfigInfo config;
BYTE baudSwitch, modeSwitch;

BYTE gpsHour, gpsMin, gpsSec, sendHour, sendMin, sendSec;
BYTE gpsSatNum, gpsWaitLines, gpsMonth, gpsDay, gpsYear;
char gpsLockChar, ggaLockChar, needTimeFlag;

volatile BYTE hdrState, commTimer, inDataLen, inPacketLen;
BYTE inSum, echoChar;

BYTE gpsSendState, gpsBadCount, gpsSendWait, gpsSendLine, gpsLineLen;
BYTE gpsReset, gpsOutLine[GPS_OUT_LEN], *tx2OutPtr, tx2OutCount;

BYTE gpsOutBuffer[ MAX_GPS_OUT_LEN+sizeof(PreHdr)+4 ], gpsOutBuffer1[ MAX_GPS_OUT_LEN+sizeof(PreHdr)+4 ];
BYTE gpsOutSendLen, gpsOutLen, gpsOutSum, *gpsOutBuffPtr, *gpsOutSendPtr, skipGpsSend;
BYTE gpsOutIdx, sendGpsData, numConverters, numChanError, spsError, errWait;

char logStr[AUX_PACKET_LEN];

SendTimeInfo sendTimeInfo;

WORD vcoOnTime, vcoOffTime, vcoTime, newVcoOnTime, newVcoOffTime;
BYTE vcoPinOn, watchdogCheck, noDataFlag;

AdcGainOffset gainOffsets[ MAX_CHANNELS ];

BYTE restartFlag;
WORD notRdyCount;

#include "sdr24utils.c"
#include "sdr24gps.c"
#include "sdr24int.c"
#include "sdr24adc.c"

int main()
{
	Init();
	restartFlag = 0;
	notRdyCount = 0;
			
LoopIn:
	
	Restart();

	restartFlag = 1;
	
	while( 1 )  {
		numConverters = InitConverters();
		if( !numConverters )  {
			strcpy( logStr, "No Converters");
			SendLogString();
			DelayMs( 100 );
			goto LoopIn;
		}
		
		if( noWaitFlag )  {
			noWaitFlag = 0;
			NewConfig();
		}
		else
			WaitForConfig();
		
		CalcAllAdc();
		
		StartAllCS5532();
				
		exitLoop = 0;

		while( !exitLoop )  {

			if( IsCS5532DataRdy() )  {
				NewSample();
				notRdyCount = 0;
			}
			else if( ++notRdyCount > 50000 )  {
				notRdyCount = 0;
				strcpy( logStr, "ADC Not Rdy");
				sendLog = 1;
			}
				
			if( newCommandFlag )  {
				newCommandFlag = 0;
				noDataCounter = 0;
				noDataFlag = 0;
				NewCommand();
				continue;
			}
				
			if( timeRefType )  {
				if( GpsCommCheck() )  {
					NewGpsTimeNMEA();
					continue;
				}		
				else if( gpsSendState )
					CheckSendGps();
			}
		
			if( !txOutCount )  {
				if( restartFlag )  {
					restartFlag = 0;
					strcpy( logStr, "Restart Flag");
					sendLog = 1;
				}
				if( sendTime )
					SendTime();
				else if( sendStatus ) 
					SendStats();
				else if( sendLog )
					SendLogString();
				else if( sendAckFlag )
					SendAck();
				else if( sendAdcInfoFlag )  {
					StopAllCS5532();
					if( calcFlag )  {
						CalcAllAdc();
						calcFlag = 0;	
					}
					GetAdcGainOffsets();
					StartAllCS5532();
					SendAdcInfo();
				}
			}
		}
	}
	
	goto LoopIn;
}

void SendError()
{
	if( errWait )
		--errWait;
	if( !errWait )  {
		if( numChanError )
			strcpy( logStr, "Config Error: Num Converters");
		else if( spsError )
			strcpy( logStr, "Config Error: Wrong SPS");
		else
			strcpy( logStr, "Config Error" );
		SendLogString();
		errWait = 10;
	}
	sampleCount = 0;
	sampleDataLen = 0;
}

void NewSample()
{
	ClrWdt();
	
	if( !sampleCount )
		MakeHdr();
		
	TEST_PIN = 1;
	ReadCS5532DataAll();
	TEST_PIN = 0;
	
	if( ++sampleCount >= samplesPerPacket )  {
		if( spsError || numChanError )
			SendError();
		else  {
			SendDataPacket();
			Check100Ms();
		}
	}
}

void MakeHdr()
{
	if( outBuffer )  {
		currHdr = (Sdr24DataHdr *)&dataPacket1[ sizeof( PreHdr ) ];
		adDataPtr = (BYTE *)( (BYTE *)&dataPacket1[ sizeof( PreHdr ) + sizeof( Sdr24DataHdr ) ] );
	}
	else  {
		currHdr = (Sdr24DataHdr *)&dataPacket[ sizeof( PreHdr ) ];
		adDataPtr = (BYTE *)( (BYTE *)&dataPacket[ sizeof( PreHdr ) + sizeof( Sdr24DataHdr ) ] );
	}
	sampleDataLen = 0;
	IEC0bits.T1IE = 0;
	currHdr->sampleTick = timeTick;
	currHdr->ppsTick = ppsTick;
	IEC0bits.T1IE = 1;
	currHdr->gpsMonth = gpsMonth;
	currHdr->gpsDay = gpsDay;
	currHdr->gpsYear = gpsYear;
	currHdr->gpsTOD = gpsTimeOfDay;
	currHdr->gpsSatNum = gpsSatNum;
	currHdr->gpsLockSts = gpsLockChar;
	currHdr->packetID = packetID + 1;
	currHdr->loopError = 0;
	currHdr->notUsed = 0;
}

void SendDataPacket()
{
	PreHdr *phdr;
	BYTE sum;
	WORD len;
		
	while( XmitCount() );
	
	len = sampleDataLen + sizeof( Sdr24DataHdr );
		
	if( outBuffer )  {
		memcpy( dataPacket1, hdrStr, 4 );
		phdr = (PreHdr *)dataPacket1;
		phdr->len = len + 1;			// one more for checksum
		phdr->type = 'D';
		phdr->flags = 0xc0;
		sum = CalcChecksum( &dataPacket1[ 4 ], len + 4 );
		dataPacket1[ len + sizeof(PreHdr) ] = sum; 
		txOutPtr = dataPacket1;
		outBuffer = 0;
	}
	else  {
		memcpy( dataPacket, hdrStr, 4 );
		phdr = (PreHdr *)dataPacket;
		phdr->len = len + 1;			// one more for checksum
		phdr->type = 'D';
		phdr->flags = 0xc0;
		sum = CalcChecksum( &dataPacket[ 4 ], len + 4 );
		dataPacket[ len + sizeof(PreHdr) ] = sum; 
		txOutPtr = dataPacket;
		outBuffer = 1;
	}
	txOutCount = len + sizeof( PreHdr ) + 1;	// one more for the checksum
	SendHost();
	sampleCount = 0;
	sampleDataLen = 0;
	++packetID;
}

void Check100Ms()
{
	if( gpsSendWait )
		--gpsSendWait;
		
	if( hdrState && ( ++commTimer >= 30 ) )  {
		hdrState = 0;
		strcpy( logStr, "Input Comm Timeout");
		sendLog = 1;
	}
	if( checkErr )  {
		checkErr = 0;
		strcpy( logStr, "Input Checksum Error");
		sendLog = 1;
	}
		
	if( watchdogCheck && ( ++noDataCounter >= NO_HOST_DATA_TIMER ) )  {
		noWaitFlag = 0;
		exitLoop = noDataFlag = 1;
		Restart();
	}
}

void NewConfig()
{
	while( XmitCount() );
	
	IFS0bits.U1RXIF = 0;		/* disable interrupts */
    IFS0bits.U1TXIF = 0;
	IFS1bits.U2RXIF = 0;
    IFS1bits.U2TXIF = 0;
	IEC0bits.T1IE = 0;
	
	memcpy( &config, &inBuffer[1], sizeof( Sdr24ConfigInfo ) );
	
	numChannels = config.numChannels;
	if( numChannels > numConverters )  {
		numChannels = numConverters;
		numChanError = 1;
	}
	else
		numChanError = 0;
	
	timeRefType = config.timeRefType;
	timeTick = config.timeTick;
	
	if( config.flags & FG_PPS_HIGH_TO_LOW )
		ppsHiToLowFlag = 1;
	else
		ppsHiToLowFlag = 0;

	spsRate = config.sps;
	if( !TestSampleRate( spsRate ) )  {
		spsError = 1;
		spsRate = 50;
	}	
	
	packetsPerSec = GetPacketsPerSec( spsRate );
	samplesPerPacket = spsRate / packetsPerSec;
	sampleOffset = numChannels * 3;
	
	if( config.flags & FG_DISABLE_LED )
		ledPPS = 0;
	else
		ledPPS = 1;
	
	if( config.flags & FG_WATCHDOG_CHECK )  {
		watchdogCheck = 1;
		RCONbits.SWDTEN = 1;
	}
	else  {
		watchdogCheck = 0;
		RCONbits.SWDTEN = 0;
	}
	vcoOnTime = config.vcoOnTime;
	vcoTime = vcoOffTime = config.vcoOffTime;
	vcoPinOn = 0;
	newVcoOnTime = 0;
	newVcoOffTime = 0;
	VCO_OUT = 0;
	
	goodConfig = 1;
	exitLoop = 0;
	
	Restart();
}

void NewCommand()
{
	char cmd;
	
	cmd = inBuffer[0];
	if( cmd == 'T' )
		SetTimeInfo();
	else if( cmd == 'D' )
		SendTimeDiff();
	else if( cmd == 't' )  {
		setGpsTimeFlag = 1;
		resetTime = 0;
	}
	else if( cmd == 'i' )  {
		setGpsTimeFlag = 1;
		resetTime = 1;
	}
	else if( cmd == 'd' )  {
		SetTimeDiff();
		sendAckFlag = 1;
	}
	else if( cmd == 'S' )
		sendStatus = 1;
	else if( cmd == 'x')  {
		noWaitFlag = 0;
		exitLoop = 1;
	}
	else if( cmd == 'C' )  {
		exitLoop = noWaitFlag = 1;
	}
	else if( cmd == 'g' )  {
		gpsReset = 1;
		gpsSendState = 1;
		gpsSendWait = 0;
	}
	else if( cmd == 'G' )  {
		gpsSendState = 1;
		gpsSendWait = 0;
	}
	else if( cmd == 'E' )  {
		EchoGps();
		exitLoop = 1;
	}
	else if( cmd == 'b' )  {
		GotoBootLdr();
	}
	else if( cmd == 'P' && !sendGpsData )  {
		if( timeRefType == TIME_SDR24_4800 || timeRefType == TIME_SDR24_9600 )  {
			sendGpsData = 1;
		}
		else  {
			if( timeRefType == TIME_SDR24_SKG )
				gpsReset = 1;
			sendGpsData = gpsSendState = 1;
			skipGpsSend = 3;
		}
		ClearGpsOut();
	}
	else if( cmd == 'p' && sendGpsData )  {
		sendGpsData = 0;
		if( timeRefType == TIME_SDR24_4800 || timeRefType == TIME_SDR24_9600 )  {
			ClearGpsOut();
		}
		else  {
			if( timeRefType == TIME_SDR24_SKG )
				gpsReset = 1;
			gpsSendState = 1;
			ClearGpsOut();
		}
	}
	else if( cmd == 'V' )  {
		SetVco();
		sendAckFlag = 1;
	}
	else if( cmd == 'r' )  {
		calcFlag = 0;
		sendAdcInfoFlag = 1;
	}
	else if( cmd == 'c' )  {
		calcFlag = 1;
		sendAdcInfoFlag = 1;
	}
	else if( cmd == 'R' )  {
		noDataCounter = 0;
		noDataFlag = 0;
	}
}

void Restart()
{
	hdrStr[0] = 0xaa; hdrStr[1] = 0x55; hdrStr[2] = 0x88; hdrStr[3] = 0x44;
	
	outBuffer = 0;
	txOutCount = 0;
	packetID = 1;
	
	ppsTick = 0;
	gpsTimeOfDay = 0;
	
	gpsLockChar = ggaLockChar = '0';
	gpsSatNum = 0;
	gpsSendState = 0;
	
	secTick = secTickValue = 1600;
	secOffValue = 800;
	
	sampleCount = 0;
	sampleDataLen = 0;
	
	sendTime = 0;
	gpsInCount = gpsQueueInCount = gpsQueueOutCount = newGpsData = 0;
	hdrState = 0;
	gpsBadCount = tx2OutCount = 0;
	gpsReset = 0;
	setGpsTimeFlag = 0;
	gpsWaitLines = 5;
	gpsSendWait = 0;
	sendGpsData = 0;
	echoMode = newEchoChr = 0;
	resetTime = 0;
	sendAckFlag = 0;
	sendLog = 0;
	sendStatus = 0;
	sendAdcInfoFlag = 0;
	gpsTimer = 0;
	noDataCounter = 0;
	needTimeFlag = 0;
	gpsMonth = gpsDay = gpsYear = 0;
			
	ClearGpsOut();
	
	if( ppsHiToLowFlag )  {
		if( !PPS_PIN )
			ppsLowHiFlag = 0;
		else
			ppsLowHiFlag = 1;
	}
	else  {
		if( PPS_PIN )
			ppsLowHiFlag = 0;
		else
			ppsLowHiFlag = 1;
	}
	
	IEC0bits.T1IE = 1;		/* enable interrupts */
	IEC0bits.U1RXIE = 1;

	if( timeRefType )
		InitGps();
}

void Init()
{
	ADCON1bits.ADON = 0;
	ADPCFG = 0xffff;
	TRISA = TRISC = 0;
	
	TRISB = 0x1e0f;
	TRISD = 0x0100;
	TRISF = 0x0010;

	PORTB = ALL_CHAN_OFF;
	PORTF = 0;
	
	TEST_PIN = 0;
	
	RCONbits.SWDTEN = 0;
	
	txOutCount = 0;
	
	TMR1 = 0; 				/* clear timer1 register */
	PR1 = 6143; 			/* set period1 register for 1600 hz with 4.152Mz and 8x PPL */
	T1CONbits.TCS = 0; 		/* set internal clock source */
	IPC0bits.T1IP = 0x06;	/* set priority level */
	IFS0bits.T1IF = 0; 		/* clear interrupt flag */
	T1CONbits.TON = 1; 		/* start the timer */
	SRbits.IPL = 3; 		/* enable CPU priority levels 4-7*/

	timeTick = 0;
	ledPPS = 0;
	numChannels = 1;
	spsRate = 100;
	
	timeRefType = 0;
	ppsHiToLowFlag = 0;
			
	exitLoop = 0;
	DelayMs( 50 );
	newCommandFlag = checkErr = 0;
	goodConfig = 0;
	noWaitFlag = 0;
	gpsSendLine = 0;
	vcoOnTime = 100;
	vcoTime = vcoOffTime = 100;
	vcoPinOn = 0;
	newVcoOnTime = 0;
	newVcoOffTime = 0;
	errWait = spsError = numChanError = 0;
	VCO_OUT = 0;
	watchdogCheck = 0;
	goodChanFlags = 0;
	lastGpsChar = 0;
	skipGpsSend = 0;
	noDataFlag = 0;
}
