#include "TestSdr24.h"

#define CURR_LOCK_WAIT		( 15 * 10 )
#define UNLOCK_WAIT			2 * 60 * 60 * 10 		// 2 hours
#define WAS_LOCK_WAIT		UNLOCK_WAIT / 2
#define PTIME_5MIN          3000

#define GPS_NOT_LOCKED		0
#define GPS_WAS_LOCKED		1
#define GPS_LOCKED			2

void AdjustVco();
void CheckUnlockSts();
void SetUnlockCounter();

int gpsLockStatus;
int debug = 0;

static BOOL firstGpsTime = TRUE;

static int goodGpsData, goodPPS, noDataFlag, highNotLockedFlag, firstResetTime;	
static int timeState, currPacketTime, skipPackets, notLockedTimer,lockOffCount;
static int currOnCount, currLockSts, wasLocked, currPPSDiff;
static int currPPSDiffAbs, logStrCount, noGpsDataCount, noGpsPPSCount, currSecDiff;
static int badSecCount, waitSecTime, badTimeCount, resetTimeFlag, resetTimeStart;
static int waitTimeCount, lockStartTime, gpsStatus;
static int vcoOnOffPercent, vcoSetTime, vcoLastSetTime; 
static int vcoDirection, vcoHighError, vco0to1Percent;
static ULONG lastGpsTime, lastPPSTime;
static Sdr24DataHdr currGpsHdr;

/* Called every packet when the GPS time reference is enabled */
void TimeRefGps( BYTE *newPacket, char *timeStsStr )
{
	int ppsMs, err = 0;
	Sdr24DataHdr *dataHdr = (Sdr24DataHdr *)&newPacket[ sizeof( PreHdr ) ];
		
	if( firstGpsTime )  {
		InitGpsRef();
		firstGpsTime = 0;
	}
	
	++currPacketTime;
	
	currGpsHdr = *dataHdr;

	if( goodGpsData )
		CheckLockSts();			// Check for GPS time lock
	else
		CheckUnlockSts();
	
	if( lastPPSTime == -1 || ( ++noGpsPPSCount >= 30 ) )  {
		if( currGpsHdr.gpsLockSts == '0' || currGpsHdr.gpsSatNum < 2 )  {
			goodPPS = 0;
			noGpsPPSCount = 0;
			return;
		}
		if( noGpsPPSCount >= 30 )
			printf("GPSRef: No 1PPS Signal\n" );
		gpsStatus = timeState = goodPPS = noGpsPPSCount = 0;
		gpsLockStatus = GPS_NOT_LOCKED;
		lastPPSTime = dataHdr->ppsTick;
		SetUnlockCounter();
		return;
	}
	
	if( lastPPSTime != dataHdr->ppsTick )  {
		goodPPS = TRUE;
		noGpsPPSCount = 0;
		lastPPSTime = dataHdr->ppsTick;
		lastGpsTime = dataHdr->gpsTOD;
		return;
	}
		
	/* Check to see if the time of day header field is changing */
	if( lastGpsTime == -1 || ( lastGpsTime == (int)dataHdr->gpsTOD ) )  {
		if( ++noGpsDataCount >= 300 )  {			// wait 30 sec before displaying error
			printf("GPSRef: No Data from GPS Receiver\n" );
			noGpsDataCount = 0;
			gpsStatus = timeState = 0;
			gpsLockStatus = GPS_NOT_LOCKED;
		}
		lastGpsTime = dataHdr->gpsTOD;
		SetUnlockCounter();
		return;
	}
		
	if( lastGpsTime != (int)dataHdr->gpsTOD )  {
		noGpsDataCount = 0;
		ppsMs = dataHdr->ppsTick % 1000;
		currSecDiff = (long)dataHdr->gpsTOD - ( (long)dataHdr->sampleTick / 1000 );
		if( ppsMs >= 500 && ppsMs <= 999 )  {
			currPPSDiffAbs = (1000 - ppsMs);
			currPPSDiff = -currPPSDiffAbs;
		}
		else
			currPPSDiffAbs = currPPSDiff = ppsMs;
		lastGpsTime = dataHdr->gpsTOD;
		goodGpsData = TRUE;
		
		NewGpsTimeSec();		
	}
	
	MakeStatusStr( timeStsStr );
}

/* Called once per second when the GPS time of day changes. */
BOOL NewGpsTimeSec()
{		
	double tm;
	char rstType;
			
	if( waitSecTime )  {
		--waitSecTime;
		return 0;
	}
	
	if( !currLockSts )  {
		badTimeCount = 0;
		return 1;
	}		
	
	if( timeState && ( currPPSDiffAbs >= 5 ) )  {
		if( ++badTimeCount >= 5 )  {
			printf( "GPSRef: Reset A/D Board Time - Sts:%d 1PPS Diff:%d\n", 
				timeState, currPPSDiff );
			InitGpsRef();
		}
		return 1;
	}
	badTimeCount = 0;	
	
	if( timeState >= 1 )   {
		if( waitTimeCount )
			return 1;	
		AdjustVco();	
		return 1;
	}
		
	if( !timeState )  {
		tm = ( currSecDiff * 1000 ) + currPPSDiff;
		if( firstResetTime )  {
			firstResetTime = 0;
			printf( "GPSRef: First Reset - Time Diff:%-6.3f\n", tm / 1000.0 );
			rstType = 'i';
		}
		else  {
			printf( "GPSRef: Reset - Time Diff:%-6.3f\n", tm / 1000.0 );
			rstType = 't';
		}
		if( currPPSDiffAbs > 1 )
			SendPacket( rstType );
		waitSecTime = 30;
		timeState = 1;
		lockStartTime = currPacketTime; 
		vcoSetTime = currPacketTime;
	}
	
	return TRUE;
}

/* Used to check if the GPS receiver is lock and giving out good time info */
void CheckLockSts()
{
	/* Check for GPS lock */
	if( !goodPPS || currGpsHdr.gpsLockSts == '0' || currGpsHdr.gpsSatNum < 2 )  {
		if( ++notLockedTimer >= PTIME_5MIN )  {
			if( !highNotLockedFlag )  {
				highNotLockedFlag = TRUE;
				if( debug )
					printf( "GPSRef: Setting High Not Locked Flag\n" );
			}
		}
		if( !currLockSts && !lockOffCount )
			return;
			
		if( lockOffCount )  {
			--lockOffCount;
			if( !lockOffCount )  {
				if( debug && ( gpsLockStatus != GPS_NOT_LOCKED ) )
					printf( "GPSRef: Reference Not Locked\n" );
				gpsLockStatus = GPS_NOT_LOCKED;
				InitGpsRef();
			}
			else if( ( gpsLockStatus != GPS_WAS_LOCKED ) && ( lockOffCount < WAS_LOCK_WAIT ) )  {
				if( debug && (gpsLockStatus != GPS_WAS_LOCKED ) )
					printf( "GPSRef: Reference Was Locked\n" );
				gpsLockStatus = GPS_WAS_LOCKED;
			}
		}
		return;
	}
	
	if( currLockSts && ( gpsLockStatus == GPS_LOCKED ) && !currOnCount )  {
		lockOffCount = UNLOCK_WAIT;
		return;
	}
			
	if( currLockSts && ( gpsLockStatus == GPS_WAS_LOCKED ) )  {
		if( debug && !currLockSts )
			printf( "GPSRef: Reference Locked\n" );
		gpsLockStatus = GPS_LOCKED;
		lockOffCount = UNLOCK_WAIT;
		return;
	}
		
	if( currOnCount )  {
		--currOnCount;
		if( !currOnCount )  {
			if( debug && !currLockSts )
				printf( "GPSRef: Reference Locked\n" );
			wasLocked = currLockSts = TRUE;
			gpsLockStatus = GPS_LOCKED;
			lockOffCount = UNLOCK_WAIT;
			notLockedTimer = 0;
		}
	}
}

void CheckUnlockSts()
{
	if( gpsLockStatus == GPS_NOT_LOCKED || !lockOffCount )
		return;
		
	--lockOffCount;
	if( !lockOffCount )  {
		if( debug && ( gpsLockStatus != GPS_NOT_LOCKED ) )
			printf( "GPSRef: Reference Not Locked (No Data)\n" );
		gpsLockStatus = GPS_NOT_LOCKED;
	}
	else if( ( gpsLockStatus != GPS_WAS_LOCKED ) && ( lockOffCount < WAS_LOCK_WAIT ) )  {
		if( debug )
			printf( "GPSRef: Reference Was Locked (No Data)\n" );
		gpsLockStatus = GPS_WAS_LOCKED;
	}
}

void SetUnlockCounter()
{			
	if( !goodGpsData || gpsLockStatus == GPS_NOT_LOCKED )
		return;
	
	noDataFlag = TRUE;
	lockOffCount = UNLOCK_WAIT;
	if( debug )
		printf( "GPSRef: No Data Lock Off Count = %d seconds\n", lockOffCount / 10 );
}

/* Init the GPS time keeping process */
void InitGpsRef()
{
	goodGpsData = goodPPS = noDataFlag = highNotLockedFlag = gpsStatus = 0;	
	currLockSts = wasLocked = currPPSDiff = waitTimeCount = lockStartTime = 0;
	currPPSDiffAbs = logStrCount = noGpsDataCount = noGpsPPSCount = currSecDiff = 0;
	badSecCount = waitSecTime = badTimeCount = resetTimeFlag = resetTimeStart = 0;
	vcoSetTime = vcoLastSetTime = vcoDirection = vcoHighError = vco0to1Percent = 0;
	timeState = currPacketTime = notLockedTimer = lockOffCount = 0;
	
	firstResetTime = TRUE;
	lastGpsTime = lastPPSTime = -1;
	skipPackets = 10;
	vcoOnOffPercent = 50;
	currOnCount = CURR_LOCK_WAIT;
	gpsLockStatus = GPS_NOT_LOCKED;
	ReadTimeInfo();
}

/* Reads the time interval information from the time.dat file */
void ReadTimeInfo()
{
	FILE *fp;
	char  buff[256];
	int vco, cnt;
				
	if(!(fp = fopen( TIME_CONFIG_FILE, "r")))
		return;
	fgets(buff, 127, fp);
	fclose(fp);
	cnt = sscanf(buff, "%d", &vco );
	if( cnt == 1 && vco > 5 && vco < 95 )
		vcoOnOffPercent = vco;
}

/* Saves the time interval information to the time.dat file */
void SaveTimeInfo()
{
	FILE *fp;
	
	if(!(fp = fopen( TIME_CONFIG_FILE, "w")))
		return;
	fprintf(fp, "%d\n", vcoOnOffPercent );
	fclose(fp);
}

void TimeStrMin( char *str, DWORD time )
{
	int days, hour, min; 
	time /= 10;
	days = time / 86400;
	time %= 86400;
	hour = time / 3600;
	time %= 3600L;
	min = (int)(time / 60L);
	if( days )
		sprintf( str, "%d %02d:%02d", days, hour, min );
	else
		sprintf( str, "%02d:%02d", hour, min );
}

/* Make a string based on the current time status */
void MakeStatusStr( char *str )
{
	char lockStr[ 64 ], lastStr[ 64 ], currentStr[ 64 ];
		
	if( timeState >= 1 )
		TimeStrMin( lockStr, currPacketTime - lockStartTime );
	else
		strcpy(lockStr, "00:00");

	if( vcoLastSetTime )
		TimeStrMin( lastStr, vcoLastSetTime );
	else
		strcpy( lastStr, "00:00");
		
	if( vcoSetTime )
		TimeStrMin( currentStr, currPacketTime - vcoSetTime );
	else
		strcpy(currentStr, "00:00");
	
	sprintf( str, "Sts:%d Lck:%c Sats:%02d LckTm:%s Vco:%d%% VcoChg:%s/%s PPSDif:%d", 
		timeState, currGpsHdr.gpsLockSts, currGpsHdr.gpsSatNum, lockStr, vcoOnOffPercent, currentStr, 
		lastStr, currPPSDiff );
}

/* Used in the Echo GPS mode to send commands to the Garmin GPS receiver */
void SendGpsCmd( char *cmd )
{
	char line[256];

	strcpy(line, cmd);
	strcat(line, "\r\n");
	SendCommData( (BYTE *)line, strlen( line ), TRUE );
}

/* Used in the Echo GPS mode to start or stop GPS data. Used to check the transmit and
   receive RS-232 lines */
void StartStopGps( int start )
{
	if( !start )
		SendGpsCmd("$PGRMO,,2");
	else
		SendGpsCmd("$PGRMO,,4");
}

void AdjustVco()
{		
	int adj, diff, currOnOff = vcoOnOffPercent;
	
	if( !currPPSDiff && vcoDirection )  {
		if( debug) 
			printf( "GPSRef: Reset Direction\n" );
		diff = currOnOff - vco0to1Percent;
		currOnOff = currOnOff - ( diff / 2 );
		vcoDirection = vcoHighError = vco0to1Percent = 0;		
	}
	
	if( currPPSDiffAbs )  {
		if( currPPSDiffAbs >= 1 )  {
			if( currPPSDiffAbs < vcoHighError )  {
				if( debug )
					printf( "GPSRef: Error Now Going Down OnOff=%d 0to1=%d\n", 
						currOnOff, vco0to1Percent );
				vcoDirection = -1;
			}
			else if( currPPSDiffAbs > vcoHighError )  {
				if( !vcoHighError )  {
					vco0to1Percent = currOnOff;
				}
				vcoHighError = currPPSDiffAbs; 
				if( debug )
					printf( "GPSRef: Error Going Up\n" );
				vcoDirection = 1;
			}
		}
	}
	
	if( currPPSDiffAbs >= 4 )
		adj = 3;		
	else if( currPPSDiffAbs >= 3 )
		adj = 2;
	else
		adj = 1;
				
	if( currPPSDiffAbs && ( vcoDirection != -1 ) )  {
		if( currPPSDiff >= 1 )
			currOnOff -= adj;
		else
			currOnOff += adj;
	}
			
	if( currOnOff < 5 )
		currOnOff = 5;
	else if( currOnOff > 95 )
		currOnOff = 95;
		
	if( currOnOff != vcoOnOffPercent )  {
		vcoLastSetTime = currPacketTime - vcoSetTime;
		vcoOnOffPercent = currOnOff;
		SetVco( vcoOnOffPercent );
		SaveTimeInfo();
		vcoSetTime = currPacketTime;
	}
	
	if( debug )
		printf( "GPSRef: Vco PPSDif=%d Cur=%d Dir=%d HiErr=%d\n", currPPSDiff, 
			vcoOnOffPercent, vcoDirection, vcoHighError );
	
	if( currPPSDiffAbs > 4 )
		waitTimeCount = 300;
	else if( currPPSDiffAbs > 1 )
		waitTimeCount = 600;
	else
		waitTimeCount = 1200;
}

