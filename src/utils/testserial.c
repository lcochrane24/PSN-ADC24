#include "p30F3014.h"

_FOSC( CSW_FSCM_OFF & XT_PLL8 );
_FWDT( WDT_OFF );
_FBORPOR( PBOR_OFF & BORV_27 & PWRT_16 & MCLR_EN );
_FGS( CODE_PROT_OFF );

#include <uart.h>
//#include <spi.h>
#include <stdio.h>
#include "sdr24.h"

int numChannels = 1;
Sdr24ConfigInfo config;
BYTE *adDataPtr;
int sampleDataLen;
int sampleOffset;

#include "sdr24adc.c"

void __attribute__((__interrupt__, no_auto_psv)) _T1Interrupt(void)
{
	IFS0bits.T1IF = 0;		/* clear interrupt flag */
}

void Init()
{
	ADCON1bits.ADON = 0;
	ADPCFG = 0xffff;
	TRISA = 0x0000;
	TRISB = 0x0e0f;
	TRISC = 0x0000;
	TRISD = 0x0100;
	TRISF = 0x0010;

	PORTB = 0x1fff;
	PORTF = 0;
	
	SPI_OUT = 0;
	SPI_CLK = 0;
	
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
	
	div = 15;					// 38.4k baud * 8x PLL
	
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

int main()
{
	long l;
	
	Init();
	
	SetBaudRate( 0 );
	
	config.sps =15;
	config.flags = 0;
	config.gain[0] = 0;
	
	while( 1 )  {
		numChannels = InitConverters();
		printf("Num = %d\n", numChannels );
		DelayMs(100);

		StartCS5532();
		
		while( 1 )  {
			if( IsCS5532DataRdy() )  {
				ReadCS5532Data( 0, &l );
				printf("l=%ld\n", l );
			}
			else
				printf("not rdy\n");
		}
	}
				
	return 1;
}
