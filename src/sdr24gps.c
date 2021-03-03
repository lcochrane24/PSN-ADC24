/* sdr gps.c */

char *gps18All[] = { "$PGRMO,GPGSA,1", "$PGRMO,GPGSA,1", "$PGRMO,GPRMC,1", 
					 "$PGRMO,GPGSV,1", "$PGRMO,PGRMT,1", 0 };

char *gps18Only2d[] = { "$PGRMI,,,,,,,R", "$PGRMI,,,,,,,R", "$PGRMO,,2", "$PGRMO,GPGGA,1", 
                      "$PGRMO,GPRMC,1", "$PGRMC,2,400,,,,,,,,,0,2,3,1", "$PGRMC1,1,1,1,,,,1,N,N,2,1", 0 };

char *gps18NormNoWAAS[] = { "$PGRMI,,,,,,,R", "$PGRMI,,,,,,,R", "$PGRMO,,2", "$PGRMO,GPGGA,1", 
                      "$PGRMO,GPRMC,1", "$PGRMC,,,,,,,,,,,,2,3,", "$PGRMC1,,,,,,,,N,,,,,", 0 };

char *gps18NormWAAS[] = { "$PGRMI,,,,,,,R", "$PGRMI,,,,,,,R", "$PGRMO,,2", "$PGRMO,GPGGA,1", 
                      "$PGRMO,GPRMC,1", "$PGRMC,,,,,,,,,,,,2,3,", "$PGRMC1,,,,,,,,W,,,,,", 0 };

char *gpsSKGNorm[] = { "$PMTK313,0", "$PMTK301,0", "$PMTK314,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0", 0 };

char *gpsSKGAll[] = { "$PMTK313,1", "$PMTK301,2", "$PMTK314,0,1,0,1,1,1,0,0,0,0,0,0,0,0,0,0,0", 0 };

void NewGpsTimeNMEA()
{
	if( gpsSendState )
		return;
		
	if( gpsWaitLines )  {
		--gpsWaitLines;
		return;
	}
		
	if( !TestGpsLine() )
		return;
		
	if( !ParseGpsLine() )  {
		if( !sendGpsData && ( ++gpsBadCount >= 3 ) )  {
			gpsBadCount = 0;
			gpsReset = gpsSendState = 1;
			strcpy( (char *)logStr, "GPS Parse Error");
			sendLog = 1;
		}
		return;
	}
	
	gpsTimeOfDay = ( (long)gpsHour * 3600 ) + ( (long)gpsMin * 60 ) + (long)gpsSec;
	
	if( setGpsTimeFlag && ppsTick )
		SetGpsTime();
}
	
void SetGpsTime()
{
	ULONG gtime, adiff; 
	SLONG diff;
						
	while( XmitCount() );
	
	gtime = gpsTimeOfDay * 1600;
	
	setGpsTimeFlag = 0;
	
	IEC0bits.T1IE = 0;
	
	diff = (SLONG)gtime - (SLONG)ppsTick;
		
	if( diff < 0 ) {
		adiff = -diff;
		if( adiff > timeTick )
			timeTick = TICKS_PER_DAY - ( adiff - timeTick );
		else
			timeTick -= adiff;
	}
	else  {
		timeTick += diff;
		if( timeTick >= TICKS_PER_DAY )
			timeTick -= TICKS_PER_DAY;
	}
	
	secTick = 1600 - ( timeTick % 1600 );
	if( resetTime )  {
		sampleCount = resetTime = 0;
	}

	IEC0bits.T1IE = 1;
}

BYTE GpsCommCheck()
{
	BYTE chr;
	
	if( gpsQueueInCount == gpsQueueOutCount )
		return 0;
	
	IEC1bits.U2RXIE = 0;
	chr = gpsInQueue[ gpsQueueOutCount ];
	if( ++gpsQueueOutCount >= GPS_IN_QUEUE )
		gpsQueueOutCount = 0;
	IEC1bits.U2RXIE = 1;
	
	lastGpsChar = chr;
	gpsTimer = 0;
	
	if( sendGpsData && !gpsSendState )  {
		if( ++gpsOutLen >= (MAX_GPS_OUT_LEN-1) )  {
			if( gpsOutSendLen )  {
				ClearGpsOut();
				strcpy( logStr, "GPS Send Overflow Error");
				sendLog = 1;
				return 0;
			}
			else
				QueueGpsPacket();
		}
		*gpsOutBuffPtr++ = chr;
		gpsOutSum ^= chr;
	}

	if( chr == 0x0d )
		return 0;
		
	if( !gpsInCount )  {
		if( chr == '$' )  {
			gpsInCount = 1;
			gpsInBuffer[0] = chr;
		}
		return 0;
	}
	if( gpsInCount >= ( GPS_BUFFER_LEN-1 ) )  {
		gpsInCount = 0;
		strcpy( logStr, "GPS Input Overflow Error");
		sendLog = 1;
		return 0;
	}
	if( chr == 0x0a )  {
		gpsInBuffer[ gpsInCount ] = 0;
		gpsLineLen = gpsInCount+1;
		gpsInCount = 0;
		return 1;
	}
	gpsInBuffer[ gpsInCount++ ] = chr;
	return 0;
}

void QueueGpsPacket()
{
	BYTE *buff;
	PreHdr *pHdr;
	int len;

	if( gpsOutIdx )
		buff = gpsOutBuffer1; 
	else
		buff = gpsOutBuffer;
	
	pHdr = (PreHdr *)buff;
	memcpy( pHdr->hdr, hdrStr, 4);	// make the header
	len = gpsOutLen;
	pHdr->len = len + 1;			// 1 more for checksum
	pHdr->type = 'g';
	pHdr->flags = 0xc0;
	gpsOutSum ^= buff[4];
	gpsOutSum ^= buff[5];
	gpsOutSum ^= buff[6];
	gpsOutSum ^= buff[7];
	buff[ len + sizeof(PreHdr) ] = gpsOutSum; 
	gpsOutSendPtr = buff;
	if( gpsOutIdx )  {
		gpsOutIdx = 0;
		gpsOutBuffPtr = &gpsOutBuffer[ sizeof( PreHdr ) ];	
	}
	else  {
		gpsOutIdx = 1;
		gpsOutBuffPtr = &gpsOutBuffer1[ sizeof( PreHdr ) ];	
	}
	gpsOutSum = gpsOutLen = 0;
	
	if( skipGpsSend )
		--skipGpsSend;
	if( !skipGpsSend )
		gpsOutSendLen = len + ( sizeof( PreHdr ) + 1 );		// one more for the checksum
}
	
BYTE TestGpsLine()
{
	char *ptr, *end, chr, *line;
	BYTE sum, num;
	WORD len;
	
	line = &gpsInBuffer[1]; 
	sum = 0;
	if(!( end = strchr( line, '*' ) ) )  {
		strcpy(logStr, "GPS Error No *");
		sendLog = 1;
		return( 0 );
	}
	*end++ = 0;
	if( ( len = strlen(line) ) < 2 )  {
		strcpy(logStr, "GPS Length Error");
		sendLog = 1;
		return( 0 );
	}
	ptr = line;
	while(len--)
		sum = sum ^ *ptr++;
	num = (sum >> 4) & 0x0f;
	if( num > 9 )
		chr = ( num-10 ) + 'A';
	else
		chr = num + '0';
	if( chr != *end++ )
		return( 0 );
	num = sum & 0x0f;
	if( num > 9 )
		chr = ( num-10 ) + 'A';
	else
		chr = num + '0';
	if( chr != *end )  {
		strcpy(logStr, "GPS Checksum Error");
		sendLog = 1;
		return(0);
	}
	return(1);
}

BYTE ParseGpsLine()
{
	register char *ptr, lckChr;
	static char lockWait = 0;
	
	if( !memcmp( &gpsInBuffer[1], "GPGGA,", 6 ) )  {
		if( ! ( ptr = SkipComma( gpsInBuffer, 7 ) ) )
			return FALSE;
		ggaLockChar = *ptr;
		gpsSatNum = (char)GetSatNum(ptr + 2);
		return TRUE;
	}
		
	if( !memcmp( &gpsInBuffer[1], "GPRMC,", 6 ) )  {
		if( !needTimeFlag )				// only allow one RMC after 1PPS pulse
			return TRUE;
		needTimeFlag = 0;
		ptr = &gpsInBuffer[ 7 ];
		gpsHour = GetHMS( ptr );
		gpsMin = GetHMS( &ptr[ 2 ] );
		gpsSec = GetHMS( &ptr[ 4 ] );
		if( ! ( ptr = FindComma( ptr ) ) )
			return FALSE;
		lckChr = *ptr;
		if( lckChr == 'A' )  {
			if( lockWait )  {
				--lockWait;			
				gpsLockChar = '0';			// send not locked back to host
			}
			else if( ggaLockChar == '2' )
				gpsLockChar = '2';
			else
				gpsLockChar = '1';
		}	
		else if( lckChr == 'V' )  {
			gpsLockChar = '0';
			lockWait = 3;
		}
		else  {
			strcpy( logStr, "Unknown RMC Chr");
			sendLog = 1;
			return TRUE;
		}
		
		if( ! ( ptr = SkipComma( gpsInBuffer, 10 ) ) )
			return FALSE;

		if( *ptr == ',' )
			gpsDay = gpsMonth = gpsYear = 0;
		else  {
			gpsDay = GetHMS( ptr );
			gpsMonth = GetHMS( &ptr[ 2 ] );
			gpsYear = GetHMS( &ptr[ 4 ] );
		}
		gpsBadCount = 0;
		return TRUE;
	}
	
	if( timeRefType == TIME_SDR24_4800 || timeRefType == TIME_SDR24_9600 )
		return TRUE;
	
	return FALSE;
}

BYTE SendGpsLine()
{
	register BYTE *out, sum, num; 
	WORD len;
	char *str = 0;
		
	if( timeRefType == TIME_SDR24_4800 || timeRefType == TIME_SDR24_9600 )
		return 0;
		
	if( timeRefType == TIME_SDR24_SKG )  {
		if( sendGpsData )
			str = gpsSKGAll[ gpsSendLine ];
		else
			str = gpsSKGNorm[ gpsSendLine ];
	}
	else  {
		if( sendGpsData )
			str = gps18All[ gpsSendLine ];
		else if( config.flags & FG_2D_ONLY )
			str = gps18Only2d[ gpsSendLine ];
		else if( config.flags & FG_WAAS_ON  )
			str = gps18NormWAAS[ gpsSendLine ];
		else
			str = gps18NormNoWAAS[ gpsSendLine ];
	}
			
	if( !str )
		return 0;
	
	strcpy( (char *)gpsOutLine, str );
	len = strlen( (char *)gpsOutLine ) - 1;
	sum = CalcChecksum( &gpsOutLine[1], len );
	out = &gpsOutLine[ len + 1 ];
	*out++ = '*'; 
	num = ( sum >> 4 ) & 0x0f;
	if( num > 9 )
		*out++ = ( num - 10 ) + 'A';
	else
		*out++ = num + '0';
	num = sum & 0x0f;
	if( num > 9 )
		*out++ = ( num - 10 ) + 'A';
	else
		*out++ = num + '0';
	*out++ = 0x0d;
	*out++ = 0x0a;
	*out = 0;
	tx2OutPtr = gpsOutLine;
	tx2OutCount = len + 6;
	SendGPS();
	return 1;
}

void CheckSendGps()
{
	if( tx2OutCount || gpsSendWait )
		return;
		
	if( timeRefType == TIME_SDR24_4800 || timeRefType == TIME_SDR24_9600 )  {
		gpsSendState = 0;
		gpsWaitLines = 5;
		return;
	}

	if( timeRefType == TIME_SDR24_SKG )  {
		if( gpsTimer > 2000 )  {		// a little over 1 second
			gpsTimer = 0;
			strcpy( logStr, "SKG Init Error");
			sendLog = 1;
		}
		else if( ( lastGpsChar != 0x0a ) || ( gpsTimer < 80 ) )
			return;		
	}
		
	if( gpsSendState == 1 )  {
		gpsSendLine = 0;
		if( sendGpsData )
			SendGpsLine();
		else  {
			if( gpsReset )  {
				gpsReset = 0;
				SendGpsLine();
				gpsSendWait = 20;
			}
		}
		gpsSendLine = 1;
		gpsSendState = 2;
		return;
	}
		
	if( !SendGpsLine() )  {
		gpsWaitLines = 5;
		gpsSendState = 0;
		ClearGpsOut();
	}
	else  {
		++gpsSendLine;
		++gpsSendState;
		gpsSendWait = 20;
	}
}

void EchoGps()
{
	BYTE escCnt, chr;
	WORD c1, c2, data;
	escCnt = 0;
	SetGpsBaud( FALSE );
		
	CloseUART1();
	c1 = UART_EN & UART_IDLE_CON & UART_DIS_WAKE & UART_DIS_LOOPBACK &
	    	UART_DIS_ABAUD & UART_NO_PAR_8BIT &	UART_1STOPBIT;
	c2 = UART_TX_PIN_NORMAL & UART_TX_ENABLE & UART_ADR_DETECT_DIS & 
		UART_RX_OVERRUN_CLEAR;
	
	if( timeRefType == TIME_SDR24_SKG || timeRefType == TIME_SDR24_9600 )
		OpenUART1( c1, c2, 63  );
	else
		OpenUART1( c1, c2, 127  );
	
	data = U1STA;
	data &= 0xff3f;
	U1STA = data ;
		
	while( 1 )  {
		if( DataRdyUART1() )  {
			chr = ReadUART1();
			if( chr == 0x1b )  {
				if( ++escCnt >= 3 )
					break;
			}
			else
				escCnt = 0;
			while( BusyUART2() );
			WriteUART2( chr );
		}
		if( DataRdyUART2() )  {
			chr = ReadUART2();
			while( BusyUART1() );
			WriteUART1( chr );
		}
		ClrWdt();
	}
	echoMode = 0;
	baudSwitch = ReadBaudModeSwitch();
	if( timeRefType )
		InitGps();
}
		
void InitGps()
{
	IEC1bits.U2TXIE = 0;
	IFS1bits.U2TXIF = 0;
	tx2OutCount = 0;
	if( timeRefType == TIME_SDR24_SKG )
		gpsReset = 1;
	gpsSendState = 1;
	gpsInCount = 0;
	SetGpsBaud( TRUE );
	ClrWdt();
}

void ClearGpsOut()
{
	gpsOutIdx = 0;
	gpsOutSendLen = gpsOutLen = gpsOutSum = 0;
	gpsOutBuffPtr = &gpsOutBuffer[ sizeof( PreHdr ) ];	
}

void SetGpsBaud( int startInt )
{
	WORD c1, c2, data;
	
	IEC1bits.U2TXIE = 0;
	IEC1bits.U2RXIE = 0;
	CloseUART2();
	
	c1 = UART_EN & UART_IDLE_CON & UART_DIS_WAKE & UART_DIS_LOOPBACK &
	    	UART_DIS_ABAUD & UART_NO_PAR_8BIT &	UART_1STOPBIT;
	c2 = UART_TX_PIN_NORMAL & UART_TX_ENABLE & UART_ADR_DETECT_DIS & 
		UART_RX_OVERRUN_CLEAR;
	if( timeRefType == TIME_SDR24_SKG || timeRefType == TIME_SDR24_9600 )
		OpenUART2( c1, c2, 63 );
	else
		OpenUART2( c1, c2, 127 );
	
	if ( U2STAbits.OERR )
		U2STAbits.OERR = 0;
	
	data = U2STA;
	data &= 0xff3f;
	U2STA = data ;
	
	gpsQueueInCount = gpsQueueOutCount = 0;
	if( startInt )
		IEC1bits.U2RXIE = 1;
}
