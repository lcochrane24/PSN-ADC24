/* utils.c */

BYTE XmitCount()
{
	return txOutCount;
}

/* Calculate the checksum of a block of data */
BYTE CalcChecksum(BYTE *cp, WORD cnt)
{
	BYTE sum;

	sum = 0;
	while( cnt-- )
		sum ^= *cp++;
	return( sum );
}

char *SkipComma(char *line, int skip)
{
	while( --skip )  {
		if( *line == ',' )  {
			++line;
			continue;
		}
		if( ! ( line = strchr( line, ',' ) ) )
			return( (char *)0 );
		++line;
	}
	return( line );
}

char *FindComma( char *line )
{
	if( ! ( line = strchr( line, ',' ) ) )
		return( (char *)0 );
	return ++line;
}

BYTE GetSatNum(char *ptr)
{
	BYTE num;
	
	if( ptr[1] == ',' )
		num = ( *ptr - '0' );
	else  {
		num = ( *ptr++ - '0' ) * 10;	
		num += ( *ptr - '0' );
	}
	return( num );
}

BYTE GetHMS(char *ptr)
{
	BYTE num;
	
	num = ( *ptr++ - '0' ) * 10;	
	num += ( *ptr - '0' );
	return( num );
}

void SendGPS()
{
	--tx2OutCount;
	IFS1bits.U2TXIF = 0;
	U2TXREG = *tx2OutPtr++;
	IEC1bits.U2TXIE = 1;
}

void SendHost()
{
	--txOutCount;
	IFS0bits.U1TXIF = 0;
	U1TXREG = *txOutPtr++;
	IEC0bits.U1TXIE = 1;
}

/* Packs and sends a new debug / error message string to the host */
void SendLogString()
{
	PreHdr *phdr;
	BYTE sum, len;
	
	while( XmitCount() );
	
	memcpy( auxPacket, hdrStr, 4 );
	len = strlen( logStr ) + 1;		// 1 more for null
	phdr = (PreHdr *)auxPacket;
	phdr->len = len + 1;			// 1 more for checksum
	phdr->type = 'L';
	phdr->flags = 0xc0;
	memcpy( &auxPacket[ sizeof(PreHdr) ], logStr, len );
	sum = CalcChecksum( &auxPacket[ 4 ], len + 4 );
	auxPacket[ len + sizeof(PreHdr) ] = sum; 
	txOutCount = len + ( sizeof( PreHdr ) + 1 );	// one more for the checksum
	txOutPtr = auxPacket;
	SendHost();
	sendLog = 0;
}

/* Packs and sends the time differece packet */
void SendTime()
{
	PreHdr *phdr;
	BYTE sum, len;
	
	while( XmitCount() );
	
	memcpy( auxPacket, hdrStr, 4 );
	len = sizeof( SendTimeInfo );
	phdr = (PreHdr *)auxPacket;
	phdr->len = len + 1;			// 1 more for checksum
	phdr->type = 'w';
	phdr->flags = 0xc0;
	memcpy( &auxPacket[ sizeof(PreHdr) ], &sendTimeInfo, len );
	sum = CalcChecksum( &auxPacket[ 4 ], len + 4 );
	auxPacket[ len + sizeof(PreHdr) ] = sum; 
	txOutCount = len + ( sizeof( PreHdr ) + 1 );	// one more for the checksum
	txOutPtr = auxPacket;
	SendHost();
	sendTime = 0;
}

/* This "I am here" packet is sent if the A/D board is not communicating with the host. */
void SendConfigReq()
{
	PreHdr *pHdr;
	BYTE sum, len;
	BoardInfoSdr24 *bi;
	
	len = sizeof( BoardInfoSdr24 );
	memcpy( auxPacket, hdrStr, 4 );
	pHdr = (PreHdr *)auxPacket;
	pHdr->len = len + 1;			// 1 more for checksum
	pHdr->type = 'C';
	pHdr->flags = 0xc0;		// new board
	
	bi = (BoardInfoSdr24 *)&auxPacket[ sizeof(PreHdr) ];
	bi->modeNumConverters = numConverters;
	if( modeSwitch )
		bi->modeNumConverters |= 0x80;
	bi->goodConverters = goodChanFlags; 
	bi->notUsed1 = bi->notUsed2 = 0;
	sum = CalcChecksum( &auxPacket[4], len + 4);
	auxPacket[ len + sizeof( PreHdr ) ] = sum;
	txOutCount = len + sizeof( PreHdr ) + 1;	// one more for the checksum
	txOutPtr = auxPacket;
	SendHost();
}

void WaitForConfig()
{
	WORD sendCount, ledCount, waitTime;
	BYTE ledOnOff, vcoOnOff;
	BYTE baud, vcoCount = 2;
	
	USB_TRIS_BIT = 1;		// set port mode to input
	USB_RST_PIN = 0;
			
	hdrState = newCommandFlag = ledOnOff = vcoOnOff = 0;
	ledCount = sendCount = 1;
	vcoCount = 1;
	LED_PIN = VCO_OUT = 0;
	txOutCount = 0;
		
	IEC0bits.T1IE = 0;
	IEC0bits.U1RXIE = 1;
		
	waitTime = 6000;
	
	while( 1 )  {
		ClrWdt();
		DelayMs(10);
		baud = ReadBaudModeSwitch();
		if( baud != baudSwitch )  {
			baudSwitch = baud;
			SetBaudRate( TRUE );
		}
		if( newCommandFlag )  {
			newCommandFlag = 0;
			noDataFlag = 0;
			hdrState = 0;
			if( inBuffer[0] == 'b' )
				GotoBootLdr();
			else if( inBuffer[0] == 'C' )  {
				NewConfig();
				return;
			}
		}
		--sendCount;
		if( !sendCount )  {
			SendConfigReq();
			sendCount = 100;
		}

		--ledCount;
		if( !ledCount )  {
			if( ledOnOff )  {
				LED_PIN = 0;
				ledOnOff = 0;
			}
			else  {
				LED_PIN = 1;
				ledOnOff = 1;
			}
			ledCount = 10;
		}
	
		--vcoCount;
		if( !vcoCount )  {
			if( vcoOnOff )  {
				VCO_OUT	= 0;
				vcoOnOff = 0;
			}
			else  {
				VCO_OUT	= 1;
				vcoOnOff = 1;
			}
			vcoCount = 2;
		}
		
		if( waitTime )
			--waitTime;
			
		if( modeSwitch && noDataFlag && !txOutCount && !waitTime)  {
			USB_TRIS_BIT = 0;		// set port mode to output
			DelayMs( 50 );
			USB_TRIS_BIT = 1;		// reset port mode back to input
			waitTime = 12000;
		}
			
//		if( modeSwitch && !txOutCount && !waitTime )  {
//			if( WiFiTest() )
//				waitTime = 12000;
//			else
//				waitTime = 6000;
//		}

	}
}

void SendStats()
{
	BYTE sum, len;
	StatusInfo *s;
	PreHdr *pHdr;
				
	len = sizeof( StatusInfo );
	s = (StatusInfo *)&auxPacket[ sizeof( PreHdr ) ];
	s->boardType = BOARD_TYPE;			// fill in the packet with data
	s->majVer = MAJOR_VER;
	s->minVer = MINOR_VER;
	s->numChannels = numChannels;
	s->sps = spsRate;
	s->test = samplesPerPacket;
	
	memcpy( auxPacket, hdrStr, 4 );
	pHdr = (PreHdr *)auxPacket;
	pHdr->len = len + 1;			// 1 more for checksum
	pHdr->type = 'S';
	pHdr->flags = 0xc0;
	sum = CalcChecksum(&auxPacket[4], len + 4);
	auxPacket[ len + sizeof(PreHdr) ] = sum; 
		
	txOutCount = len + ( sizeof( PreHdr ) + 1 );	// one more for the checksum
	txOutPtr = auxPacket;
	SendHost();
	sendStatus = 0;
}

void GotoBootLdr()
{
	asm ("RESET");
}

void SetTimeInfo()
{
	TimeInfo *ti;
	long time, diff;
	BYTE reset;
		
	memcpy( &inBuffer[0], &inBuffer[1], sizeof( TimeInfo ) );
	ti = (TimeInfo *)&inBuffer;
	time = ti->timeTick;
	reset = ti->reset;
	
	IEC0bits.T1IE = 0;

	diff = timeTick - timeStamp;
	time += diff;
	if( time >= TICKS_PER_DAY )
		time -= TICKS_PER_DAY;
	timeTick = time;
	secTick = 1600 - ( time % 1600 );
	if( reset )
		sampleCount = 0;
	
	IEC0bits.T1IE = 1;
}

void SetTimeDiff()
{
	SLONG diff;
	ULONG adiff;
	TimeDiffInfo *ti;
	BYTE reset;
		
	memcpy( &inBuffer[0], &inBuffer[1], sizeof( TimeDiffInfo ) );
	ti = (TimeDiffInfo *)&inBuffer;
	diff = ti->timeAdjust;
	reset = ti->reset;
	
	IEC0bits.T1IE = 0;
		
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
	if( reset )
		sampleCount = 0;
	
	IEC0bits.T1IE = 1;
}

void SendTimeDiff()
{
	SLONG now, diff;
	TimeInfo *ti;
	
	memcpy( &inBuffer[0], &inBuffer[1], sizeof( TimeInfo ) );
	IEC0bits.T1IE = 0;
	now = timeTick;
	IEC0bits.T1IE = 1;
	diff = now - timeStamp;
	ti = (TimeInfo *)&inBuffer;
	sendTimeInfo.data = now - ( ti->timeTick + diff );
	sendTimeInfo.msCount = (WORD)diff;
	sendTime = 1;
}

void SetVco()
{
	VcoInfo *vi;
	
	memcpy( &inBuffer[0], &inBuffer[1], sizeof( VcoInfo ) );
	vi = (VcoInfo *)&inBuffer;
	newVcoOnTime = vi->onTime;
	newVcoOffTime = vi->offTime;
}

void SendAck()
{
	PreHdr *phdr;
	BYTE sum, len;
	
	memcpy( auxPacket, hdrStr, 4 );
	len = 1;
	
	phdr = (PreHdr *)auxPacket;
	phdr->len = len + 1;			// 1 more for checksum
	phdr->type = 'a';
	phdr->flags = 0xc0;
	
	auxPacket[ sizeof(PreHdr) ] = 0x06;
	sum = CalcChecksum( &auxPacket[ 4 ], len + 4 );
	auxPacket[ len + sizeof(PreHdr) ] = sum; 
	
	txOutCount = len + ( sizeof( PreHdr ) + 1 );	// one more for the checksum
	txOutPtr = auxPacket;
	SendHost();
	sendAckFlag = 0;
}

void DelayMs( int ms )
{
	int i;
	long delay;
	
	for( i = 0; i != ms; i++ )  {
		delay = 1625L;
		while( delay-- );
	}
}

void SetBaudRate( int startInt )
{
	WORD div, c1, c2, data;
	
	if( baudSwitch == 0 )
		div = 63;					// 9600
	else if( baudSwitch == 1 )
		div = 31;					// 19200
	else if( baudSwitch == 2 )
		div = 15;					// 38400
	else 
		div = 10;					// 57600
	
	IEC0bits.U1TXIE = 0;
	IEC0bits.U1RXIE = 0;
	CloseUART1();
	c1 = UART_EN & UART_IDLE_CON & UART_DIS_WAKE & UART_DIS_LOOPBACK &
	    	UART_DIS_ABAUD & UART_NO_PAR_8BIT &	UART_1STOPBIT;
	c2 = UART_TX_PIN_NORMAL & UART_TX_ENABLE & UART_ADR_DETECT_DIS & 
		UART_RX_OVERRUN_CLEAR;
	OpenUART1( c1, c2, div  );
	
	if ( U1STAbits.OERR )
		U1STAbits.OERR = 0;
	
	data = U1STA;
	data &= 0xff3f;
	U1STA = data ;
	
	hdrState = 0;
	if( startInt )
		IEC0bits.U1RXIE = 1;
}

BYTE ReadBaudModeSwitch()
{
	BYTE s1 = 0, s2 = 0;
	
	if( SW3_BIT )
		modeSwitch = 0;
	else
		modeSwitch = 1;
	if( !SW1_BIT )
		s1 = TRUE;
	if( !SW2_BIT )
		s2 = TRUE;
	if( !s1 && !s2 )	
		return 0;
	if( s1 && !s2 )	
		return 1;
	if( !s1 && s2 )	
		return 2;
	return 3;
}

void SendAdcInfo()
{
	BYTE sum, len;
	PreHdr *pHdr;
	AdcGainOffset *pGO;
					
	len = sizeof( gainOffsets );
	
	pGO = (AdcGainOffset *)&auxPacket[ sizeof( PreHdr ) ];
	memcpy( pGO, gainOffsets, len );
	memcpy( auxPacket, hdrStr, 4 );
	pHdr = (PreHdr *)auxPacket;
	pHdr->len = len + 1;			// 1 more for checksum
	pHdr->type = 'r';
	pHdr->flags = 0xc0;
	sum = CalcChecksum(&auxPacket[4], len + 4);
	auxPacket[ len + sizeof(PreHdr) ] = sum; 
		
	txOutCount = len + ( sizeof( PreHdr ) + 1 );	// one more for the checksum
	txOutPtr = auxPacket;
	SendHost();
	sendAdcInfoFlag = 0;
}

BYTE WiFiTest()
{
	BYTE c, ret = 0;
	
	IEC0bits.U1RXIE = 0;
	IEC0bits.U1TXIE = 0;
	
	DelayMs(20);	
	U1TXREG = '\r';
	DelayMs(100);	
	
	if( U1STAbits.OERR )
		U1STAbits.OERR = 0;
					
	while( U1STAbits.URXDA )
		c = U1RXREG;

	U1TXREG = 'Z';
	DelayMs(100);
	if( U1STAbits.URXDA )  {
		c = U1RXREG;
		if( c == 'Z')  {
			U1TXREG = '\r';
			DelayMs(100);
			U1TXREG = 'A';
			DelayMs(20);
			U1TXREG = 'T';
			DelayMs(20);
			U1TXREG = 'A';
			DelayMs(20);
			U1TXREG = '\r';
			DelayMs(20);
			ret = 1;
		}
	}
	
	if( U1STAbits.OERR )
		U1STAbits.OERR = 0;
	
	IEC0bits.U1RXIE = 1;
	IEC0bits.U1TXIE = 1;
	
	return ret;
}
