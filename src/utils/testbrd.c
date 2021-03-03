#include "p30F3014.h"

_FOSC( CSW_FSCM_OFF & XT_PLL8 );
_FWDT( WDT_OFF );
_FBORPOR( PBOR_OFF & BORV_27 & PWRT_16 & MCLR_EN );
_FGS( CODE_PROT_OFF );

#include <uart.h>
#include <stdio.h>

#define MAX_CHANNELS		4
#define SPS					200

#define SPI_LONG_DELAY		5
#define SPI_SHORT_DELAY		3

#define SLONG 				long
#define SINT 				short

#define ULONG				unsigned long
#define WORD				unsigned short
#define BYTE				unsigned char
#define UINT 				unsigned int

#define LED_PIN				LATDbits.LATD2
#define TEST_PIN			LATDbits.LATD9

#define SPI_CLK				LATBbits.LATB8
#define SPI_OUT				LATDbits.LATD0

#define FG_UNIPOLAR_INPUT	0x0008

#define ALL_CHAN_ON 		0x1e00
#define ALL_CHAN_OFF 		0x1ef0
int chanSelect[ MAX_CHANNELS ] = { 0x1ee0, 0x1ed0, 0x1eb0, 0x1e70 };
int chanMask[ MAX_CHANNELS ] = 	 { 0x0001, 0x0002, 0x0004, 0x0008 };

#define SPI_CONF_REG		0x012d
#define SETUP_REG 			0x0000	// 100/120 sps
	
typedef struct  {
	WORD rate;
	WORD sampleTimer;
	BYTE packetsPerSec;
	BYTE frsOn;
	BYTE bits;
} SpsInfo;

SpsInfo spsInfoRun[] = {
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

void __attribute__((__interrupt__, no_auto_psv)) _T1Interrupt(void)
{
	IFS0bits.T1IF = 0;		/* clear interrupt flag */
}

void Init()
{
	ADCON1bits.ADON = 0;
	ADPCFG = 0xffff;
	TRISA = TRISC = 0;
	
	TRISB = 0x0e0f;
	
	TRISD = 0x0100;
	TRISF = 0x0010;

	PORTB = ALL_CHAN_OFF;
	PORTF = 0;
	
	TEST_PIN = 0;
	
	TMR1 = 0; 				/* clear timer1 register */
	PR1 = 1535; 			/* set period1 register */
	T1CONbits.TCS = 0; 		/* set internal clock source */
	IPC0bits.T1IP = 0x06;	/* set priority level */
	IFS0bits.T1IF = 0; 		/* clear interrupt flag */
	T1CONbits.TON = 1; 		/* start the timer */
	SRbits.IPL = 3; 		/* enable CPU priority levels 4-7*/
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

void SetBaudRate( int setInt )
{
	WORD div, c1, c2, data;
	
	div = 15;					// 38.4k baud @ 4.912Mhz * 8x PLL
	
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
	U1STA = data;
}

int IsCS5532DataRdy( int chan )
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

void StartCS5532()
{
	PORTB = ALL_CHAN_ON;
	SPIDly( SPI_LONG_DELAY );
	WriteSPIByte( 0xc0 );			// start continuious conversion
}

int SetupCS5532( int channel, WORD spsRate, BYTE flags, BYTE gain )
{
	SpsInfo *sps = &spsInfoRun[ 0 ];
	UINT i, bits;
	ULONG l;
	WORD wGain = gain;
		
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
		WriteRegister( 0x03, 0x00080000 );
	else
		WriteRegister( 0x03, 0x00000000 );
	
	i = SETUP_REG;
	bits = sps->bits;
	bits <<= 7;
	i |= bits;
	
	if( flags & FG_UNIPOLAR_INPUT )
		i |= 0x0040;
	
	wGain = gain & 0x07;	
	wGain <<= 11;
	i |= wGain;
	
	l = (ULONG)i;
	l <<= 16;
	l |= (ULONG)i;
	
	WriteRegister( 0x5, l );
	
	PORTB = ALL_CHAN_OFF;
	
	return sps->rate;
}

int InitCS5532( int chan )
{
	int maxLoop = 5, ok = 0;
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
		DelayMs( 50 );
		printf("loop\n");
	}
	
	PORTB = ALL_CHAN_OFF;
	return ok;
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
	
int NumberChannels()
{
	int i, converters = 0;
	for( i = 0; i != MAX_CHANNELS; i++ )  {
		if( !InitCS5532( i ) )
			break;
		++converters;
	}
	return converters;
}
	

int main()
{
	int sts, cnt, numChannels;
	long data;
		
	Init();
	
	SetBaudRate( 0 );
	
	while( 1 )  {
		sts = InitCS5532( 3 );
		printf("sts=%d\n", sts );
		DelayMs( 200 );
	}
	
	numChannels = NumberChannels();
	printf("Converters = %d\n", numChannels );
		
	SetupCS5532( 0, SPS, 0, 0 );
	StartCS5532();

	cnt = 0;
	
	while( 1 )  {
		while( !IsCS5532DataRdy( 0 ) );
		TEST_PIN = 1;
		ReadCS5532Data( 0, &data );
		TEST_PIN = 0;
		DelayMs( 1 );
		data >>= 5;
		if( ++cnt >= SPS )  {
			printf("%ld\n", data );
			cnt = 0;
		}
	}
				
	return 1;
}
