/* sdr24adc.c */

#define SPI_CONF_REG		0x012d
#define SETUP_REG 			0x0000	// 100/120 sps
	
// rate, sampleTimer, packetsPerSec, frsOn, bits;
SpsInfo spsInfo[] = {
	{   6, 214,  2, 1, 0x04 },		// 6.25 SPS	
	{   7, 214,  2, 0, 0x04 },		// 7.5 SPS	
	{  15, 107,  5, 0, 0x03 },	
	{  25,  64,  5, 1, 0x02 },	
	{  30,  54, 10, 0, 0x02 },	
	{  50,  32, 10, 1, 0x01 },	
	{  60,  27, 10, 0, 0x01 },	
	{ 100,  16, 10, 1, 0x00 },	
	{ 120,  14, 10, 0, 0x00 },
	{ 200,   8, 10, 1, 0x0c },	
	{   0,   0, 0,  0, 0x00 }		// end	
};

#define ALL_CHAN_ON 		0x0e00
#define ALL_CHAN_OFF 		0x0ef0
int chanSelect[ MAX_CHANNELS ] = { 0x1ee0, 0x1ed0, 0x1eb0, 0x1e70 };
int chanMask[ MAX_CHANNELS ] = 	 { 0x0001, 0x0002, 0x0004, 0x0008 };

int IsCS5532DataRdy( )
{
	if( PORTB & chanMask[0] )
		return 0;
	return 1;
}

int IsCS5532ChanRdy( BYTE chan )
{
	if( PORTB & chanMask[ chan ] )
		return 0;
	return 1;
}

// Delay for SPI R/W
void SPIDly( int cnt )
{
	while( cnt-- )
		asm ("NOP");
}

void WriteSPIByte( BYTE data )
{
	int cnt = 8;
		
	while( cnt-- )  {
		if( data & 0x80 )
			SPI_OUT = 1;
		else
			SPI_OUT = 0;
		SPIDly( SPI_LONG_DELAY );
		SPI_CLK = 1;
	    data <<= 1;
		SPIDly( SPI_SHORT_DELAY );
    	SPI_CLK = 0;
	}
}

void WriteRegister( BYTE cmd, ULONG data )
{
	BYTE *p = (BYTE *)&data;
	WriteSPIByte( cmd );
	WriteSPIByte( p[3] );
	WriteSPIByte( p[2] );
	WriteSPIByte( p[1] );
	WriteSPIByte( p[0] );
}

BYTE ReadSPIByte( int chan )
{
	BYTE data = 0;
	int cnt = 8;
	while( cnt-- )  {
		data <<= 1;
		SPI_CLK = 1;
		SPIDly( SPI_LONG_DELAY );
		if( PORTB & chanMask[ chan ] ) 
			data |= 1;
		SPIDly( SPI_SHORT_DELAY );
		SPI_CLK = 0;
		SPIDly( SPI_LONG_DELAY );
  	}
	return data;
}

void ReadRegister( int chan, BYTE cmd, ULONG *data )
{	
	int cnt = 4;
	BYTE *p = (BYTE *)data + sizeof( ULONG ) - 1;
	
	WriteSPIByte( cmd );
	
	while( cnt-- )
    	*p-- = ReadSPIByte( chan );
}

int SetupCS5532( int channel, WORD spsRate, BYTE gainFlags, BYTE calc )
{
	SpsInfo *sps = &spsInfo[ 0 ];
	WORD wGain = gainFlags & GF_GAIN_MASK;
	UINT i, bits;
	ULONG l = 0L;
		
	while( sps->rate )  {
		if( sps->rate == spsRate )
			break;
		++sps;
	}
	if( !sps->rate )		// not found
		return 0;

	PORTB = chanSelect[ channel ];						
	SPIDly( SPI_LONG_DELAY );
	
	if( sps->frsOn )
		l |= 0x00080000L;
	
	if( ! ( gainFlags & GF_VREF_GT25 ) )
		l |= 0x02000000;

	WriteRegister( 0x03, l );
	
	i = SETUP_REG;
	bits = sps->bits;
	bits <<= 7;
	i |= bits;
	
	if( gainFlags & GF_UNIPOLAR_INPUT )
		i |= 0x0040;
	
	wGain <<= 11;
	i |= wGain;
	
	l = (ULONG)i;
	l <<= 16;
	l |= (ULONG)i;
	
	WriteRegister( 0x5, l );
	
	PORTB = ALL_CHAN_OFF;
	
	return sps->rate;
}

void StartAllCS5532()
{
	int c, cnt = numChannels;
	BYTE chMask = 0x01;
	
	for( c = 0; c != MAX_CHANNELS; c++ )  {
		if( goodChanFlags & chMask )  {
			SetupCS5532( c, spsRate, config.gainFlags[ c ], 0 );
			--cnt;
			if( !cnt )
				break;
		}
		chMask <<= 1;
	}

	PORTB = ALL_CHAN_ON;
	SPIDly( SPI_LONG_DELAY );
	WriteSPIByte( 0xc0 );			// start continuious conversion
}

void StopAllCS5532()
{	
	long data[ MAX_CHANNELS ];
	int p, c, cnt = 32;
	
	WriteSPIByte( 0xff );
	
	while( cnt-- )  {
		SPI_CLK = 1;
		SPIDly( SPI_LONG_DELAY );
		p = PORTB;
		for( c = 0; c != MAX_CHANNELS; c++ )  {
			data[ c ] <<= 1;
			if( p & chanMask[ c ] ) 
				data[ c ] |= 1;
		}
		SPI_CLK = 0;
		SPIDly( SPI_SHORT_DELAY );
  	}
}

void ReadCS5532Data( int chan, long *data )
{
	int cnt = 3; 
	BYTE *p = (BYTE *)data + sizeof( ULONG ) - 2;
	
	WriteSPIByte( 0 );
	
	while( cnt-- )
    	*p-- = ReadSPIByte( chan );
	ReadSPIByte( chan );
	++p;
	if( p[2] & 0x80 )
		p[ 3 ] = 0xff;
	else
		p[ 3 ] = 0;		
}
	
void ReadCS5532DataAll()
{	
	long data[ MAX_CHANNELS ];
	int p, c, cnt = 32, num = numChannels;
	BYTE *b, chMask = 0x01;
	
	memset( data, 0, sizeof( data ) );
	
	WriteSPIByte( 0 );
	
	while( cnt-- )  {
		SPI_CLK = 1;
		SPIDly( SPI_LONG_DELAY );
		p = PORTB;
		for( c = 0; c != MAX_CHANNELS; c++ )  {
			data[ c ] <<= 1;
			if( p & chanMask[ c ] ) 
				data[ c ] |= 1;
		}
		SPI_CLK = 0;
		SPIDly( SPI_SHORT_DELAY );
  	}
	
	for( c = 0; c != MAX_CHANNELS; c++ )  {
		if( goodChanFlags & chMask )  {
#ifdef ADCSM
			if( c != (MAX_CHANNELS-1) )
				data[c] = -data[c];
#endif
			b = ( (BYTE *)&data[c] ) + 3;
			*adDataPtr++ = *b--;
			*adDataPtr++ = *b--;
			*adDataPtr++ = *b--;
			--num;
			if( !num )
				break;
		}
		chMask <<= 1;
	}
	
	sampleDataLen += sampleOffset;
}

WORD TestSampleRate( WORD spsRate )
{
	SpsInfo *sps = &spsInfo[ 0 ];
		
	while( sps->rate )  {
		if( sps->rate == spsRate )
			return 1;
		++sps;
	}
	return 0;
}

BYTE GetPacketsPerSec( WORD spsRate )
{
	SpsInfo *sps = &spsInfo[ 0 ];
		
	while( sps->rate )  {
		if( sps->rate == spsRate )
			break;
		++sps;
	}
	if( !sps->rate )		// not found
		return 0;
	
	return sps->packetsPerSec;
}

int InitCS5532( int chan )
{
	int maxLoop = 3, ok = 0;
	ULONG ul, off, gain;
	
	PORTB = chanSelect[ chan ];						
	
	SPIDly( SPI_LONG_DELAY );
	
	while( maxLoop-- )  {
		WriteRegister( 0xff, 0xffffffff );
		WriteRegister( 0xff, 0xffffffff );
		WriteRegister( 0xff, 0xffffffff );
		WriteRegister( 0xff, 0xfffffffe );
		DelayMs( 20 );
		WriteRegister( 0x03, 0x20000000ul );
		DelayMs( 20 );
		ReadRegister( chan, 0x0b, &ul );
		if( ul & 0x10000000ul )  {
			WriteRegister( 0x03, 0x00 );
			ReadRegister( chan, 0x0b, &ul );
			ReadRegister( chan, 0x09, &off );
			ReadRegister( chan, 0x0a, &gain );
			if( off == 0 && gain == 0x1000000 )  {
				ok = 1;
				break;
			}
		}
		ClrWdt();
		DelayMs( 50 );
	}
	
	PORTB = ALL_CHAN_OFF;
	return ok;
}

int InitConverters()
{
	int c, converters = 0;
	BYTE chMask = 0x01;
	
	goodChanFlags = 0;
	for( c = 0; c != MAX_CHANNELS; c++ )  {
		if( InitCS5532( c ) )  {
			++converters;
			goodChanFlags |= chMask;
		}
		chMask <<= 1;
	}
	return converters;
}
	
void GetAdcGainOffsets()
{
	int c, cnt = numChannels;
	BYTE chMask = 0x01;
	
	AdcGainOffset *pGO = gainOffsets;

	for( c = 0; c != MAX_CHANNELS; c++ )  {
		if( goodChanFlags & chMask )  {
			ReadRegister( c, 0x09, &pGO->offset );
			ReadRegister( c, 0x0a, &pGO->gain );
			--cnt;
			if( !cnt )
				break;
		}
		chMask <<= 1;
		++pGO;
	}

	sendAdcInfoFlag = 0;
}

void DoCS5532OffsetGainCal( BYTE chan, BYTE type )
{
	long dummy;

	PORTB = chanSelect[ chan ];						
	
	DelayMs( 50 );
	
	WriteSPIByte( type );			// start offset calibration
	
	DelayMs( 10 );
	
	while( !IsCS5532ChanRdy( chan) );
	
	ReadCS5532Data( chan, &dummy );
	
	PORTB = ALL_CHAN_OFF;
}

void CalcAllAdc()
{
	int c, cnt = numChannels;
	BYTE chMask = 0x01;
		
	for( c = 0; c != MAX_CHANNELS; c++ )  {
		if( goodChanFlags & chMask )  {
			SetupCS5532( c, 7, config.gainFlags[c], 1 );
			DelayMs( 20 );
			DoCS5532OffsetGainCal( c, 0x81 );				// Offset Command
			DelayMs( 20 );
			DoCS5532OffsetGainCal( c, 0x82 );				// Gain Command
			DelayMs( 20 );
			ClrWdt();
			--cnt;
			if( !cnt )
				break;
		}
		chMask <<= 1;
	}	
}
