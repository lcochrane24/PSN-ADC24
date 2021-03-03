/* TestSdr24.cpp - This program is used as an example on how to configure and then receive data 
   from the 24-Bit PSN-ADC24 Analog to Digital Board */

#include "TestSdr24.h"
#include "display.h"

#define PACKETS_PER_SECOND		10			// Number of packets sent out by the A/D board per second

BYTE timeRefType = TIME_REF_GARMIN;			// set this to one of time reference types

BOOL invertedPPSSignal = 0;					// Set to TRUE if high to low PPS signal
BOOL noLedPPS = FALSE;						// Set to TRUE to stop the LED on the ADC board from 
											// blinking on and off

/* Each packet from the A/D board has these 4 bytes at the beginning of the packet */
BYTE hdrStr[] = { 0xaa, 0x55, 0x88, 0x44 };

/* Input and output packet arrays */
BYTE newPacket[2048], outDataPacket[128];

/* Other variables... */
int goodData = 0, maxInQue = 0, crcErrors = 0;
int firstPacketCount = 2, sendStatusCount = 2;
int port, baudOrPort, numChan, sps;
WORD vcoOnOffTime = VCO_ON_OFF_TIME;
char portStr[ 128 ];

/*  Used to keep track of incoming packets */
ULONG currentPacketID = 0;

/* A/D data is placed in this array when a data packet is received */
long demuxData[ MAX_CHANNELS ][ MAX_SPS ];

BOOL needRet = 0, gpsSend = 0;
int gpsPackets = 0, dataPackets = 0;

int numConverters = 0, modeSwitch = 0;

BOOL newLine = 0, logData = 0, displayData = 0, dumpSamples = 0;

CDisplay display;

ChanStats chanStats[ MAX_CHANNELS ];
ChanConfig chanConfig[ MAX_CHANNELS ];
double adcRefVolts = 2.5;

time_t lastSendTime;

BOOL WINAPI CtrlHandler( DWORD dwCtrlType )
{
	display.MoveCursor( display.nrow-2, 0 );
	display.Close();
	ClosePort();						// Close the Comm port
	return 0;	
}

void CheckSendTime()
{
	static int sent = 0;
	time_t now;
	
	time( &now );
	if( ( now - lastSendTime ) >= 30 )  {
		SendPacket( 'R' );
		++sent;
	}
}

/* Program starts here..... */
void main( int argc, char *argv[] )
{
	strcpy( portStr, "COMM_PORT" );
	sps = SAMPLE_RATE;
	numChan = NUMBER_CHANNELS;
		
	ReadConfig();
	
	if( argc == 2 )  {
		strcpy( portStr, argv[1] );
	}
	
	SetConsoleCtrlHandler( CtrlHandler, TRUE );
	
	display.Init();
	display.MoveCursor( 0, 0 );
	printf("TestSdr24: Port=%s Baud/Port=%d Channels=%d SPS=%d\nPress Esc to quit....", 
		portStr, baudOrPort, numChan, sps );	
	
	/* Open the comm port */
	if( !OpenPort( portStr, baudOrPort ) )  {
		printf( "Port Open Error\n" );
		return;
	}
	
	for( int c = 0; c != MAX_CHANNELS; c++ )  {
		chanStats[c].highCount = -99999999;
		chanStats[c].lowCount = 99999999;
		chanStats[c].average = 0.0;
		chanStats[c].avgCount = 0;
	}
	
	/* Main loop */
	while( TRUE )  {					// Loop till ESC key pressed
		if( GetCommData() )	 {			// Process incoming data
			ProcessNewPacket();			// Process new packet
			CheckSendTime();
		}
		else if( kbhit() )				// Check for user input
			if( GetUser() )
				break;
		else  {
			_sleep(10);					// If nothing to do sleep awhile
		}
	}
	CtrlHandler( 0 );	
}

/* Process user input */
BOOL GetUser()
{
	char ch;
	switch( (ch = getch() ) )  {
		case 0x1b:					// ESC key
				SendPacket( 'x' );	// Send exit command to A/D board
				_sleep(100);		// Wait awhile
				return TRUE;		// return true to end program
		
		case 't':
		case 'T':
				DoTest();
				return FALSE;
		
		case 'b':					// Send goto bootloader command
				SendPacket( 'b' );
				printf("\nGo to Bootloader command sent\n");
				return TRUE;		// return true to end program
		
		case 'R':
				ResetBoard();		// Reset the ADC board
				return FALSE;
		case 'p':					// Turn on or off the sending of GPS data
		case 'P':					// with the normal data packets. P = on, p = off
				if( ch == 'p' )
					gpsSend = FALSE;
				else  {
					gpsPackets = dataPackets = 0;
					gpsSend = TRUE;
				}
				SendPacket( ch );
				return FALSE;
		case 'g':					// Reset and initilize the GPS receiver
		case 'G':					// Initilize the GPS receiver, does not reset the receiver.
				SendPacket( ch );
				return FALSE;
		case 'e':					// Forces the ADC board into the GPS echo mode
		case 'E':
				EchoMode();
				return FALSE;
		case 'r':
				SendPacket( 'r' );
//				gpsPackets = dataPackets = 0;
				return FALSE;
		case 'a':
		case 'd':
				SendPacket( ch );
				return FALSE;
		
		case 'C':
				SendPacket('c');
				return FALSE;
		
		case 'l':
				SendPacket('l');
				return FALSE;
		
		case 'L':
				SendPacket('L');
				return FALSE;
		
		case 'c':  {
					for( int c = 0; c != MAX_CHANNELS; c++ )  {
						chanStats[c].highCount = -99999999;
						chanStats[c].lowCount = 99999999;
						chanStats[c].average = 0.0;
						chanStats[c].avgCount = 0;
					}
				}
				return FALSE;
		
//		case 'l':
//				dumpSamples = !dumpSamples;
//				return FALSE;
		      
		case 'v':
				if( vcoOnOffTime >= 100 )
					return FALSE;
				vcoOnOffTime += 5;
				SetVco( vcoOnOffTime );
				return FALSE;
		
		case 'V':
				if( !vcoOnOffTime )
					return FALSE;
				vcoOnOffTime -= 5;
				SetVco( vcoOnOffTime );
				return FALSE;
									
	}
	return FALSE;
}

/* ------------------------------------------------------------------------------ */
/* The code below is used to receive data from the ADC board */
/* ------------------------------------------------------------------------------ */

/* Process new packet from A/D board */
void ProcessNewPacket()
{
	PreHdr *preHdr = (PreHdr *)newPacket;
		
//	printf("New Packet %c\n", preHdr->type );
	
	/* Check for proper A/D board */
	if( preHdr->flags != 0xc0 )  {
		printf( "Wrong A/D Board!!!\n" );
		return;
	}
	
	// process the different incoming packet types
	switch( preHdr->type )  {
		case 'C':		ProcessConfig(); break;
		case 'g':		ProcessGpsData(); break;
		case 'D':		ProcessDataPacket(); break;
		case 'L':		LogFileMessage(); break;
		case 'S':		ProcessStatusPacket(); break;
		case 'a':		AckPacket(); break;
		case 'r':		AdcInfoPacket(); break;
		default:		printf("\nUnknown packet type %c (0x%x)\n", preHdr->type, preHdr->type );
	}
}

void ProcessConfig()
{
	BoardInfoSdr24 *bi = (BoardInfoSdr24 *)&newPacket[ sizeof(PreHdr) ];
	numConverters = bi->modeNumConverters & 0x0f;
	if( bi->modeNumConverters & 0x80 )
		modeSwitch = TRUE;
	else
		modeSwitch = 0;
	display.MoveCursor( 1, 0 );
	display.ClearLine( 1 );
	printf( "Sending Configuration Information Number Converters=%d GoodFlags=0x%02x Mode=%d\n", 
		numConverters, bi->goodConverterFlags, modeSwitch );
	SendConfigPacket(); 
}

// Converts the tick time to milliseconds 
void ConvertTimeToMs( ULONG *tick )
{
	ULONG ms = *tick % 1600;
	ULONG sec = ( *tick / 1600 ) * 1000;
	sec += (ULONG)( (double)ms * 0.625   );
	*tick = sec;
}

/* Process the status packet. Currently displays the firmware version number. */
void ProcessStatusPacket()
{
	StatusInfo *sts = (StatusInfo *)&newPacket[sizeof(PreHdr)];
	display.MoveCursor( 2, 0 );
	printf( "Status Packet - A/D Board Firmware: %d.%d Channels: %d SPS: %d  Test: %d", 
		sts->majorVersion, sts->minorVersion, sts->numChannels, sts->sps, sts->test );
}

/* Display the GPS data */
void ProcessGpsData()
{
	PreHdr *preHdr = (PreHdr *)newPacket;
	newPacket[ sizeof( PreHdr ) + ( preHdr->len-1 ) ] = 0;
	++gpsPackets;
//	printf("GPS Len=%d  GPS Packets: %d   Data Packets: %d\n", preHdr->len-1, gpsPackets, dataPackets );
	printf("%s", &newPacket[ sizeof( PreHdr ) ] );
}

/* Print debug or error messages from the A/D board */
void LogFileMessage()
{
	printf( "\nA/D Board Msg: %s\n", &newPacket[ sizeof( PreHdr ) ] );	// text starts after the header
}

/* Some commands to the ADC board are acknowledged by sending out the type 'a' packet.
   These commands are acknowledged: a, s, x, b, d and j
*/
void AckPacket()
{
	
}

/* ------------------------------------------------------------------------------ */
/* The code below is used to send commands and data to the ADC board */
/* ------------------------------------------------------------------------------ */
/* Packets to the ADC board have the following format:
   Byte 1 = 0x02 Start of text character
   Byte 2 = Packet Type
   Byte 3 = first byte of data or Checksum if no data
   Next Byte after data = Checksum
   Next Byte = 0x03 End of text character
*/
   
/* Send configuration information to the A/D board. */
void SendConfigPacket()
{
	int len = sizeof( Sdr24ConfigInfo );
	Sdr24ConfigInfo *cfg = (Sdr24ConfigInfo *)&outDataPacket[ 2 ];
	BYTE crc;
	time_t ltime;
	struct tm *nt;
	SYSTEMTIME sysTm;
	
	GetSystemTime( &sysTm );
	time( &ltime );
	nt = gmtime( &ltime );
	memset( cfg, 0, sizeof( Sdr24ConfigInfo ) );
	outDataPacket[0] = 0x02;
	outDataPacket[1] = 'C';
	cfg->numChannels = numChan;
	cfg->sps = sps;
	cfg->timeTick = ( ( sysTm.wHour * 3600 ) + ( sysTm.wMinute * 60 ) + sysTm.wSecond ) * 1600; 
	cfg->timeTick += ( int )( ( (double)sysTm.wMilliseconds * 1.6 ) + 0.5 ); 
	cfg->timeRefType = timeRefType;
	
	if( invertedPPSSignal )
		cfg->flags |= FG_PPS_HIGH_TO_LOW;
	if( noLedPPS )			
		cfg->flags |= FG_DISABLE_LED;
	
	for( int c = 0; c != numConverters; c++ )  {
		cfg->gainFlags[ c ] = chanConfig[c].gainBits;
		if( adcRefVolts > 2.5 )
			cfg->gainFlags[ c ] |= GF_VREF_GT25; 
		if( chanConfig[c].unipolarFlag )
			cfg->gainFlags[ c ] |= GF_UNIPOLAR_INPUT; 
		logWrite("gain=%d %x", c, cfg->gainFlags[ c ] );
	}
		
	cfg->vcoOnTime = vcoOnOffTime * 2;
	cfg->vcoOffTime = 200 - cfg->vcoOnTime;
	
	crc = CalcCRC( &outDataPacket[1], (short)( len + 1 ) );
	outDataPacket[ len + 2 ] = crc;
	outDataPacket[ len + 3 ] = 0x03;
	SendCommData( outDataPacket, len + 4, 0 );
}

/* Send a one character command to the A/D Board */
void SendPacket( char chr )
{
	BYTE crc;
					
	lastSendTime = time(0);
	outDataPacket[0] = 0x02;
	outDataPacket[1] = chr;
	crc = CalcCRC( &outDataPacket[1], (short)1 );
	outDataPacket[2] = crc;
	outDataPacket[3] = 0x03;
	SendCommData( outDataPacket, 4, 0 );
}

/* Send a status request packet. When the A/D board receives this packet it will
   send a status packet (type S) back to the host */
void SendStatusRequest()
{
	PreHdr *pHdr = (PreHdr *)outDataPacket;
	BYTE crc;

	lastSendTime = time(0);
	memcpy(pHdr->hdr, hdrStr, 4);
	pHdr->len = 1;
	pHdr->type = 'S';
	pHdr->flags = 0;
	crc = CalcCRC(&outDataPacket[4], (short)4);
	outDataPacket[sizeof(PreHdr)] = crc;
	SendCommData(outDataPacket, sizeof(PreHdr) + 1, 0);
}

/* Send the current time to the A/D board */
void SendCurrentTime( BOOL reset )
{
	BYTE outDataPacket[ 128 ], crc;
	TimeInfo *tm = (TimeInfo *)&outDataPacket[ 2 ];
	int len = sizeof( TimeInfo );
	SYSTEMTIME sTm;
	HANDLE process;
	
	if( reset )
		tm->reset = 1;
	else
		tm->reset = 0;
	
	lastSendTime = time(0);
	outDataPacket[0] = 0x02;
	outDataPacket[1] = 'T';
	process = GetCurrentProcess();
	SetPriorityClass( process, REALTIME_PRIORITY_CLASS );
	GetSystemTime( &sTm );
	tm->adjustTime = ( ( sTm.wHour * 3600 ) + ( sTm.wMinute * 60 ) + sTm.wSecond ) * 1600;
	tm->adjustTime += ( int )( ( (double)sTm.wMilliseconds * 1.6 ) + 0.5 );
	
	crc = CalcCRC( &outDataPacket[1], (short)(len + 1));
	outDataPacket[len + 2] = crc;
	outDataPacket[len + 3] = 0x03;
	SendCommData(outDataPacket, len + 4, 1 );
	SetPriorityClass( process, NORMAL_PRIORITY_CLASS );
}

/* Sends the current time to the A/D board. The board then sends back a 
   type 'w' packet with the time difference between the time sent and 
   the time accumulator on the A/D board. */
void SendTimeDiffRequest( BOOL reset )
{
	BYTE outDataPacket[ 128 ], crc;
	TimeInfo *tm = (TimeInfo *)&outDataPacket[ 2 ];
	int len = sizeof( TimeInfo );
	SYSTEMTIME sTm;
	HANDLE process;
	
	lastSendTime = time(0);
	tm->reset = 0;
	outDataPacket[0] = 0x02;
	outDataPacket[1] = 'D';
	process = GetCurrentProcess();
	SetPriorityClass( process, REALTIME_PRIORITY_CLASS );
	GetSystemTime( &sTm );
	tm->adjustTime = ( ( ( sTm.wHour * 3600 ) + ( sTm.wMinute * 60 ) + sTm.wSecond ) * 1600 );
	tm->adjustTime += ( int )( ( (double)sTm.wMilliseconds * 1.6 ) + 0.5 );
	crc = CalcCRC( &outDataPacket[1], (short)(len + 1));
	outDataPacket[len + 2] = crc;
	outDataPacket[len + 3] = 0x03;
	SendCommData(outDataPacket, len + 4, 1 );
	SetPriorityClass( process, NORMAL_PRIORITY_CLASS );
}

/* Sends a command to adjust the time accumulator on the A/D board */
void SetTimeDiff( long adjust, BOOL reset )
{
	BYTE outDataPacket[ 128 ], crc;
	TimeInfo *tm = ( TimeInfo * )&outDataPacket[ 2 ];
	int len = sizeof( TimeInfo );
	
	lastSendTime = time(0);
	tm->adjustTime = adjust;
	if( reset )
		tm->reset = 1;
	else
		tm->reset = 0;
	outDataPacket[0] = 0x02;
	outDataPacket[1] = 'd';
	crc = CalcCRC( &outDataPacket[1], (short)( len + 1 ) );
	outDataPacket[len + 2] = crc;
	outDataPacket[len + 3] = 0x03;
	SendCommData(outDataPacket, len + 4, 0 );
}

/* Sends a command to adjust the VCO voltage */
void SetVco( WORD onOffTime )
{
	BYTE outDataPacket[ 128 ], crc;
	VcoInfo *vi = ( VcoInfo * )&outDataPacket[ 2 ];
	int len = sizeof( VcoInfo );
	
	lastSendTime = time(0);
	vi->onTime = onOffTime * 2;
	vi->offTime = 200 - vi->onTime;
	outDataPacket[0] = 0x02;
	outDataPacket[1] = 'V';
	crc = CalcCRC( &outDataPacket[1], (short)( len + 1 ) );
	outDataPacket[len + 2] = crc;
	outDataPacket[len + 3] = 0x03;
	SendCommData(outDataPacket, len + 4, 0 );

	display.ClearLine( 21 );
	display.MoveCursor( 21, 0 );
	printf("Vco OnOff %% = %d %d/%d      ", vcoOnOffTime, vi->onTime, vi->offTime );

}

/* Make a time string based on the packet time  */
void MakeTimeStr( char *str, time_t utc, ULONG tick )
{
	int msTick, ms, sec, min, hour;
	struct tm *nt;
	
	nt = gmtime( &utc );
	msTick = tick;
	ms = (int)( msTick % 1000L ); 
	msTick /= 1000L;
	hour = (int)( msTick / 3600L ); 
	msTick %= 3600L;
	min = (int)( msTick / 60L );
	sec = (int)( msTick % 60L );
	sprintf( str, "%02d/%02d/%02d %02d:%02d:%02d.%03d", nt->tm_mon+1, nt->tm_mday, nt->tm_year % 100, 
		hour, min, sec, ms );	
}

/* Make a time string based on the packet time  */
void MakeTickTimeStr( char *str, ULONG tick )
{
	int msTick, ms, sec, min, hour;
	
	msTick = tick;
	ms = (int)( msTick % 1000L ); 
	msTick /= 1000L;
	hour = (int)( msTick / 3600L ); 
	msTick %= 3600L;
	min = (int)( msTick / 60L );
	sec = (int)( msTick % 60L );
	sprintf( str, "%02d:%02d:%02d.%03d", hour, min, sec, ms );	
}

/* Make a time string based on the packet time  */
void MakeTODStr( char *str, ULONG tod )
{
	int sec, min, hour;
	
	hour = (int)( tod / 3600L ); 
	tod %= 3600L;
	min = (int)( tod / 60L );
	sec = (int)( tod % 60L );
	sprintf( str, "%02d:%02d:%02d", hour, min, sec );	
}

/* Configure the A/D board into the GPS echo more. In this mode all data sent 
   to the A/D Comm port is sent to the GPS receiver and all data from the GPS 
   receiver is set out the Comm port. */ 
void EchoMode()
{
	char chr;
	int stop = 0;
	
	SendPacket( 'E' );
	_sleep(200);
	SendPacket( 'E' );
	ClosePort();
	if( timeRefType == TIME_REF_SKG || timeRefType == TIME_REF_9600 )	
		OpenPort( portStr, 9600 );
	else
		OpenPort( portStr, 4800 );
	while( 1 )  {
		if( chr = GetEchoChar() )
			printf("%c", chr );
		if( kbhit() )  {
			chr = getch();
			if( chr == 0x1b )  {
				PutEchoChar( 0x1b );
				PutEchoChar( 0x1b );
				PutEchoChar( 0x1b );
				break;
			}
			if( chr == 'l' )  {
				SendGpsCmd("$PGRMC1E");
			}
			if( chr == 's' )  {
				if( !stop )  {
					StartStopGps( FALSE );
					stop = TRUE;
				}
				else  {
					StartStopGps( TRUE );
					stop = FALSE;
				}
			}
		}
	}
	ResetBoard();
	ClosePort();
	OpenPort( portStr, baudOrPort );
}

/* Save debug information into a log file */
void logWrite( char *pszFormat, ...)
{
	char *pszArguments;
	FILE *logFile;
	char buff[1024], date[24], tmstr[24];
	static BOOL firstLog = TRUE;

	pszArguments = (char *) &pszFormat + sizeof pszFormat;
	vsprintf( buff, pszFormat, pszArguments );
	if( needRet )  {
		printf("\n");
		needRet = 0;
	}
	if( firstLog )  {
		if( (logFile = fopen( "TestSdr24.log", "w") ) == NULL )
			return;
		firstLog = 0;
	}
	else if( (logFile = fopen( "TestSdr24.log", "a") ) == NULL )
		return;
	_strdate( date );
	_strtime( tmstr );
	fprintf(logFile, "%s %s %s\n", date, tmstr, buff );
	fclose(logFile);
}

/* Save debug information into a log file */
void fLogWrite( char *pszFormat, ...)
{
	char *pszArguments;
	FILE *logFile;
	char buff[1024], date[24], tmstr[24];
	static BOOL firstLog = TRUE;

	pszArguments = (char *) &pszFormat + sizeof pszFormat;
	vsprintf( buff, pszFormat, pszArguments );
	if( firstLog )  {
		if( (logFile = fopen( "TestSdr24.log", "w") ) == NULL )
			return;
		firstLog = 0;
	}
	else if( (logFile = fopen( "TestSdr24.log", "a") ) == NULL )
		return;
	_strdate( date );
	_strtime( tmstr );
	fprintf(logFile, "%s %s %s\n", date, tmstr, buff );
	fclose(logFile);
}

// Process one line of the config file
void GetChanConfig( ChanConfig *pCC, char *line )
{		
	int bits, gain, unipolar;
	
	if( sscanf( line, "%d %d %d", &bits, &gain, &unipolar ) != 3 )  {
		bits = 24;
		gain = 1;
		unipolar = 0;
	}		
	pCC->gain = gain;
	pCC->gainBits = GetGainBits( gain );
	pCC->unipolarFlag = unipolar;
	pCC->adcBits = bits;
	pCC->adcBitsShift = 24 - bits;
}

// Read the config file
void ReadConfig()
{
	FILE *fp;
	char buff[80], *ptr;
		
	memset( chanStats, 0, sizeof( chanStats ) );
	memset( chanConfig, 0, sizeof( chanConfig ) );
	
	if( !( fp = fopen("TestSdr24.ini", "r") ) )
		return;
	while( fgets( buff, 80, fp ) )  {
		if( buff[0] == '#' || buff[0] == '! ')
			continue;
		if( ptr = strchr( buff, 0x0a ) )
			*ptr = 0;
		if( ptr = strchr( buff, 0x0d ) )
			*ptr = 0;
		if( strstr( buff, "port=") )
			strcpy( portStr, &buff[5] );
		else if( strstr( buff, "baud=") )
			baudOrPort = atoi( &buff[5] );
		else if( strstr( buff, "channels=") )
			numChan = atoi( &buff[9] );
		else if( strstr( buff, "sps=") )
			sps = atoi( &buff[4] );
		else if( strstr( buff, "dump=") )
			dumpSamples = atoi( &buff[5] );
		else if( strstr( buff, "log=") )
			logData = atoi( &buff[4] );
		else if( strstr( buff, "newline=") )
			newLine = atoi( &buff[8] );
		else if( strstr( buff, "display=") )
			displayData = atoi( &buff[8] );
		else if( strstr( buff, "reference=") )
			adcRefVolts = atof( &buff[10] );
		else if( strstr( buff, "chan1=") )
			GetChanConfig( &chanConfig[0], &buff[6] );
		else if( strstr( buff, "chan2=") )
			GetChanConfig( &chanConfig[1], &buff[6] );
		else if( strstr( buff, "chan3=") )
			GetChanConfig( &chanConfig[2], &buff[6] );
		else if( strstr( buff, "chan4=") )
			GetChanConfig( &chanConfig[3], &buff[6] );
		else if( strstr( buff, "chan5=") )
			GetChanConfig( &chanConfig[4], &buff[6] );
		else if( strstr( buff, "chan6=") )
			GetChanConfig( &chanConfig[5], &buff[6] );
		else if( strstr( buff, "chan7=") )
			GetChanConfig( &chanConfig[6], &buff[6] );
		else if( strstr( buff, "chan8=") )
			GetChanConfig( &chanConfig[7], &buff[6] );
		else if( strstr( buff, "gps=") )  {
			if( buff[4] == 's' )
				timeRefType = TIME_REF_SKG;
			else if( buff[4] == 'g' )
				timeRefType = TIME_REF_GARMIN;	
			else if( buff[4] == '4' )
				timeRefType = TIME_REF_4800;	
			else if( buff[4] == '9' )
				timeRefType = TIME_REF_9600;	
		}
	}
	fclose(fp);
}

void DoTest()
{
}

void AdcInfoPacket()
{
	AdcGainOffset *go = (AdcGainOffset *)&newPacket[sizeof(PreHdr)];
	display.MoveCursor( 12, 0 );
	display.ClearLine( 12 );
	printf( "AdcInfo Chan:   Gain:     Offset:" ); 
	for( int c = 0; c != numConverters; c++ )  {
		display.MoveCursor( 13+c, 0 );
		display.ClearLine( 13+c );
		printf( "           %d    %ld  %ld", c+1, go->gain, go->offset ); 
		++go;		
	}
}

BYTE GetGainBits( int gain )
{
	if( gain == 0 || gain == 1 )
		return 0x00;
	if( gain == 2 )
		return 0x01;
	if( gain == 4 )
		return 0x02;
	if( gain == 8 )
		return 0x03;
	if( gain == 16 )
		return 0x04;
	if( gain == 32 )
		return 0x05;
	if( gain == 64 )
		return 0x06;
	return 0;
}

/* Process data packet */
void ProcessDataPacket()
{
	PreHdr *preHdr = (PreHdr *)&newPacket;
	Sdr24DataHdr *dataHdr = (Sdr24DataHdr *)&newPacket[sizeof(PreHdr)];
	ULONG  pktId = dataHdr->packetID;
	BYTE *dataPtr = (BYTE *)&newPacket[ sizeof(PreHdr) + sizeof( Sdr24DataHdr) ];
	char timeStatusStr[256], timeStr[64], buff[1024];
	long packetDiff, lData;
	BYTE *lPtr = (BYTE *)&lData;
	int count, idx, chan, num;
	static int totalSamples = 0;
		
	ConvertTimeToMs( &dataHdr->sampleTick );
	ConvertTimeToMs( &dataHdr->ppsTick );
	
//	char tmStr[64], ppsStr[64], todStr[64];
//	MakeTickTimeStr( tmStr, dataHdr->sampleTick );
//	MakeTickTimeStr( ppsStr, dataHdr->ppsTick );
//	MakeTODStr( todStr, dataHdr->gpsTOD );
//	fLogWrite( "Tod=%s TickTime=%s TickPPS=%s", todStr, tmStr, ppsStr );
	
	/* Send the current time after receiving some data packets. This seeds
	   the 1 millisecond time accumulator on the ADC board. */
	if( firstPacketCount )  {
		--firstPacketCount;
		if( !firstPacketCount )
			SendCurrentTime( TRUE );
		return;
	}
	goodData = TRUE;			// We now have good data
	
	/* Request that the ADC board to send a Status Packet */
	if( sendStatusCount )  {
		--sendStatusCount;
		if( !sendStatusCount )  {
			printf("Send Status Cmd\n");
			SendPacket( 'S' );
		}
	}
	
	/* Check the packet ID to see if there are any lost packets or if the board
	   has been reset */
	if( currentPacketID )  {
		if( pktId <= currentPacketID )
			printf("\nPacket ID Error new = %u old = %u\n", pktId, currentPacketID );
		else  {
			packetDiff = pktId - ( currentPacketID + 1 );
			if( packetDiff )
				printf( "\nPacket Sequence Error - Lost Packets = %d\n", packetDiff );
		}
	}
	currentPacketID = pktId;
	
	int numSamples =  ((preHdr->len - 1 - sizeof( Sdr24DataHdr ) ) / 3 );
	
	timeStatusStr[0] = 0;
	if( timeRefType == TIME_REF_GARMIN || timeRefType == TIME_REF_SKG )
		TimeRefGps( newPacket, timeStatusStr );
	
	MakeTimeStr( timeStr, time(0), dataHdr->sampleTick );
	if(!gpsSend)  {
		sprintf( buff, "ID:%06d %s", pktId, timeStr ); 
		display.MoveCursor( 4, 0 );
		display.ClearLine( 4 );
		puts( buff );
		if( logData || dumpSamples )
			logWrite("%s", buff );
		display.MoveCursor( 5, 0 );
		if( timeStatusStr[0] )  {
			display.ClearLine( 5 );
			sprintf( buff, "%s", timeStatusStr );
			puts( buff );
		}
	}
	needRet = TRUE;
	
	if( !numSamples )
		return;
		
	/* Now demux the data */
	num = count = numSamples / numChan;
//	logWrite("count=%d numSamps=%d numChan=%d", count, numSamples, numChan );
	idx = 0;
	while( count-- )  {
		for( chan = 0; chan != numChan; chan++ )  {
			if( dataPtr[0] & 0x80 )
				lPtr[3] = 0xff;
			else
				lPtr[3] = 0;
			lPtr[2] = dataPtr[0];
			lPtr[1] = dataPtr[1];
			lPtr[0] = dataPtr[2] ;
			dataPtr += 3;
			if( chanConfig[ chan ].adcBitsShift )  {
				if( lData < 0 )  {
					lData = -lData;
					lData >>= chanConfig[ chan ].adcBitsShift;
					lData = -lData;
				}
				else
					lData >>= chanConfig[ chan ].adcBitsShift;
			}
//			logWrite("chan=%d idx=%d lData=%d", chan, idx, lData );
			demuxData[ chan ][ idx ] = lData;
		}
		++idx;
	}
	++dataPackets;	
	
	AddStats();
	
	totalSamples += num;
	
	/* User should add code here to handle the new A/D data from the A/D board */
	
	if( totalSamples >= sps )  {
		DoStats();
		totalSamples = 0;
	}	
}

void AddStats()
{
	long lData;
	ChanStats *pCS;
	
	for( int c = 0; c != numChan; c++ )  {
		pCS = &chanStats[c];
		for( int s = 0; s != sps/10; s++ )  {
			lData = demuxData[c][s];
//			if( c == 0 )
//				logWrite("%d=%d", s, lData );
			if( lData > pCS->highCount )
				pCS->highCount = lData;
			if( lData < pCS->lowCount )
				pCS->lowCount = lData;
			pCS->average += lData;
			++pCS->avgCount;
		}
	}
}

// Display incoming data stats
void DoStats()
{
	ChanStats *pCS;
	ChanConfig *pCC;
	double sens;	
		
	for( int c = 0; c != numChan; c++ )  {
		pCS = &chanStats[c];
		pCC = &chanConfig[c];
		display.MoveCursor( 8+c, 0 );
		display.ClearLine( 8+c );
		int diff =  pCS->highCount -  pCS->lowCount;
		sens = ( 5.0 / pow( 2.0, pCC->adcBits ) );
		printf("Ch%d: Avg=%8.1f  High=%-7d Low=%-7d Diff=%-7d %g        ", c+1, 
			pCS->average / (double)pCS->avgCount, pCS->highCount, pCS->lowCount, abs( diff ),
			( sens * fabs( (double)diff ) ) / pCC->gain  );
		pCS->average = 0.0;
		pCS->avgCount = 0;
	}
}
