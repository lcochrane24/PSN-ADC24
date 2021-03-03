#include "term.h"
#include <string.h>

#define PORT		"COM1"
#define BAUD		38400

#define SPS			100

HANDLE hPort = (HANDLE)-1;
BYTE buff[8192];

void ResetBoard();

void main(int argc, char *argv[])
{
	int len, idx, dsp = 0;
	COMSTAT ComStat;
	DWORD dwErrFlags, count;
	char chr, *ptr;
	int crCount = 0;
	long l, hi, low;
				
	if(!OpenPort( PORT, BAUD, &hPort))  {
		printf("Open port error\n");
		exit(1);
	}

	if( argc == 2 )
		ResetBoard();
		
	idx = 0;
	
	hi = -999999999;
	low = 999999999;
	while(1)  {
		if(kbhit())  {
			if((chr = getch()) == 0x1b)
				break;
			if( chr == 'r' || chr == 'R' )
				ResetBoard();
			else if( chr == 'c' || chr == 'C' )  {
				hi = -999999999;
				low = 999999999;
			}
			else if( chr == '0' )
				SendControl( 0x00 );
			else if( chr == '1' )
				SendControl( 0x01 );
			else if( chr == '2' )
				SendControl( 0x02 );
			else if( chr == '3' )
				SendControl( 0x04 );
			else if( chr == '4' )
				SendControl( 0x08 );
			else if( chr == '5' )
				SendControl( 0x10 );
			else if( chr == '6' )
				SendControl( 0x20 );
			else if( chr == '7' )
				SendControl( 0x40 );
			else if( chr == '8' )
				SendControl( 0x80 );
			else
				SendCommand( chr );
		}
		ClearCommError(hPort, &dwErrFlags, &ComStat);
		if(!(len = ComStat.cbInQue))  {
			_sleep(10);
			continue;
		}
		printf("len=%d\n", ComStat.cbInQue );
		ReadFile(hPort, &buff[ idx ], (DWORD)len, &count, NULL);
		if((DWORD)len != count)  {
			printf("Read error\n");
			break;
		}
		idx += len;
		if( ptr = strchr( (char *)buff, '\n') )  {
			ptr = 0;
			sscanf( (char *)buff, "%d", &l );
			l >>= 3;
			if( l > hi )
				hi = l;
			if( l < low )
				low = l;
			if( ++dsp >= SPS )  {
				printf("%d Hi=%d Low=%d Diff=%d\n", l, hi, low, abs( hi-low ) );
				dsp = 0;
			}
			idx = 0;
			memset( buff, 0, 16 );
		}
	}
	ClosePort();
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

void SendCommand( char cmd )
{	
	BYTE crc, outDataPacket[32];
	DWORD count;
					
	outDataPacket[0] = 0x02;
	outDataPacket[1] = cmd;
	crc = CalcChecksum( &outDataPacket[1], (WORD)1 );
	outDataPacket[2] = crc;
	outDataPacket[3] = 0x03;
	WriteFile(hPort, outDataPacket, (DWORD)4, &count, NULL);
}

void SendControl( BYTE data )
{	
	BYTE crc, outDataPacket[32];
	DWORD count;
					
	outDataPacket[0] = 0x02;
	outDataPacket[1] = 'C';
	outDataPacket[2] = data;
	crc = CalcChecksum( &outDataPacket[1], (WORD)2 );
	outDataPacket[3] = crc;
	outDataPacket[4] = 0x03;
	WriteFile(hPort, outDataPacket, (DWORD)5, &count, NULL);
}

int OpenPort(char *portStr, int speed, HANDLE *port)
{
	int ret;
	DCB dcb;
	COMMTIMEOUTS CommTimeOuts;
	HANDLE hPort;
		  
	hPort = CreateFile(portStr, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL);
	if(hPort == (HANDLE)-1)  {
		printf("Comm Port Open Error.\n");
		return(FALSE);
	}
	SetupComm(hPort, 8192, 8192);
	CommTimeOuts.ReadIntervalTimeout = 0xFFFFFFFF;
	CommTimeOuts.ReadTotalTimeoutMultiplier = 0;
	CommTimeOuts.ReadTotalTimeoutConstant = 0;
	CommTimeOuts.WriteTotalTimeoutMultiplier = 0;
	CommTimeOuts.WriteTotalTimeoutConstant = 5000;
	SetCommTimeouts(hPort, &CommTimeOuts);
	
	dcb.DCBlength = sizeof(DCB);
	GetCommState(hPort, &dcb);
	dcb.BaudRate = speed;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fOutxDsrFlow = 0;
	dcb.fDtrControl = DTR_CONTROL_ENABLE;
	dcb.fOutxCtsFlow = 0;
	dcb.fRtsControl = RTS_CONTROL_ENABLE;
	dcb.fInX = dcb.fOutX = 0;
	dcb.fBinary = TRUE;
	dcb.fParity = TRUE;
	dcb.fDsrSensitivity = 0;
	ret = SetCommState(hPort, &dcb);
	*port = hPort;
	EscapeCommFunction( hPort, CLRDTR );
	return(ret);
}

void ClosePort()
{
	if(hPort == (HANDLE)-1)
		return;
	PurgeComm(hPort, PURGE_TXABORT|PURGE_RXABORT|PURGE_TXCLEAR|PURGE_RXCLEAR);
	CloseHandle(hPort);
	hPort = (HANDLE)-1;
}

int SetBaud(int speed)
{
	int ret;
	DCB dcb;
	dcb.DCBlength = sizeof(DCB);
	GetCommState(hPort, &dcb);
	dcb.BaudRate = speed;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fOutxDsrFlow = 0;
	dcb.fDtrControl = DTR_CONTROL_ENABLE;
	dcb.fOutxCtsFlow = 0;
	dcb.fRtsControl = RTS_CONTROL_ENABLE;
	dcb.fInX = dcb.fOutX = 0;
	dcb.fBinary = TRUE;
	dcb.fParity = TRUE;
	dcb.fDsrSensitivity = 0;
	ret = SetCommState(hPort, &dcb);
	return(ret);
}
	
int GetCommStr(char *string, int wait)
{
	while(1)  {
		if(!GetCommChar(string, wait))
			return(0);
		if(*string == 0x0a || *string == 0x0d)  {
			*string = 0;
			return(1);
		}
		++string;
	}
}

int GetCommChar(char *chr, int wait)
{
	COMSTAT ComStat;
	DWORD dwErrFlags, count;
	long startTime = time(0);
		
	while(1)  {
		ClearCommError(hPort, &dwErrFlags, &ComStat);
		if(!ComStat.cbInQue)  {
			_sleep(1);
			if((time(0) - startTime) >= wait)
				return(0);
			continue;
		}
		ReadFile(hPort, chr, (DWORD)1, &count, NULL);
		return(1);
	}
}

void ResetBoard()
{
	printf("Reset....\n");
	EscapeCommFunction( hPort, SETDTR );
	_sleep( 100 );	
	EscapeCommFunction( hPort, CLRDTR );
}
