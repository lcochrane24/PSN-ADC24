/* Low level comm functions */

#include <stdio.h>
#include <conio.h>
#include <winsock.h>
#include "TestSdr24.h"

static HANDLE hPort;		// serial port handle
static int soc;				// if TCP/IP

/* The following are used to process incoming packets */
static BYTE inBuff[4096], packet[4096], *curPtr, *currPkt;
static int inHdr, packLen, curCnt, hdrState, dataLen, tcpipMode;

extern BYTE hdrStr[], newPacket[];
extern int goodData, crcErrors, maxInQue;
extern time_t lastSendTime;

/* Process the incoming serial data. Returns TRUE when a good packet is found. The 
   new packet is placed in the newPacket byte array.
 
   Packets from the ADC board have the following format:

	4 Byte Header = 0xaa, 0x55, 0x88, 0x44
	2 Byte Packet Length - Length of the data and Checksum byte
	1 Byte Packet Type
	1 Byte Flags
	Optional Data, length dependent on packet type
	1 Byte Checksum
*/	
BOOL GetCommData()
{
	COMSTAT ComStat;
	DWORD dwErrFlags;
	DWORD rcvd, numRead;
	BYTE crc, data;
	struct timeval timeout;
	int tmp = 0;
	short *sptr;
	fd_set fds;
	int sts;
	
//	printf("GetCommData tcp=%d soc=%d\n", tcpipMode, soc );
	if( !curCnt )  {	// if no outstanding local data read more from the serial port
		if( tcpipMode )  {
			FD_ZERO(&fds);
			FD_SET(soc, &fds);
			memset(&timeout, 0, sizeof(timeout));
			if((sts = select(0, &fds, NULL, NULL, &timeout)) == SOCKET_ERROR)
				return FALSE;
			if(!sts)
				return FALSE;
			rcvd = recv( soc, (char *)inBuff, sizeof( inBuff ), 0 );
			if( rcvd <= 0 )
				return FALSE;
			curPtr = inBuff;
			curCnt = rcvd;
		}
		else  {
			ClearCommError( hPort, &dwErrFlags, &ComStat );
			if( !ComStat.cbInQue )
				return FALSE;
			if( (int)ComStat.cbInQue > maxInQue )
				maxInQue = ComStat.cbInQue;	
			if( ComStat.cbInQue > 2048 )
				numRead = 2048;
			else
				numRead = ComStat.cbInQue;
			ReadFile( hPort, inBuff, numRead, &rcvd, NULL );
			curPtr = inBuff;
			curCnt = rcvd;
		}
	}
	while( curCnt )  {				// process each stored byte
		data = *curPtr++;
		if( inHdr )  {
			if( hdrState == 7 )   {		// get flags
				sptr = (short *)&packet[4];
				dataLen = *sptr;
//				printf("\nDataLen=%d\n", dataLen );
				if( dataLen >= 0 && dataLen <= 4000 )  {
					*currPkt++ = data;
					++packLen;
					inHdr = 0;
				}
				else  {
					currPkt = packet;
					packLen = hdrState = 0;
				}
			}
			else if( hdrState == 4 || hdrState == 5 || hdrState == 6 )  {	// get data len and type 
				*currPkt++ = data;
				++packLen;
				++hdrState;
			}  
			else if( data == hdrStr[hdrState] )  {		// see if hdr signature
				*currPkt++ = data;
				++packLen;
				++hdrState;
			}
			else  {
				currPkt = packet;
				packLen = hdrState = 0;
			}
		}
		else  {					// store the data after the header
			*currPkt++ = data;
			++packLen;
			--dataLen;
			if( !dataLen )  {		// packet done
				crc = CalcCRC(&packet[4], (short)(packLen - 5));
				if( crc != packet[packLen-1] )  {	// test the crc	
					logWrite("CRC Error %x %x", crc, packet[packLen-1]);
					++crcErrors;					// count errors
					packLen = 0;
				}
				else
					memcpy( newPacket, packet, packLen ); // save the new packet
				inHdr = TRUE;				// reset for next packet
				currPkt = packet;
				packLen = 0;
				hdrState = 0;
				--curCnt;
				return TRUE;
			}
		}
		--curCnt;
	}
	return FALSE;
}

/* Open the comm port */
BOOL OpenPort( char *portStr, int speedOrPort )
{
	DCB dcb;
	COMMTIMEOUTS CommTimeOuts;
	char openStr[16];
	int ret, port;
	
	tcpipMode = FALSE;
	hPort = (HANDLE)-1;
	soc = -1;
		
	if( strchr( portStr, '.' ) )  {
		tcpipMode = TRUE;
		return TcpIpConnect( portStr, speedOrPort );
	}
	
	port = atoi( portStr );
	
	if( port < 1 || port > 255 )
		return FALSE;
	
	if( port >= 10 )
		sprintf( openStr, "\\\\.\\COM%d", port );
	else
		sprintf( openStr, "COM%d", port );
	
	hPort = CreateFile( openStr, GENERIC_READ|GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL );
	if( hPort == (HANDLE)-1 )
		return FALSE;
	SetupComm( hPort, 32768, 1024 );
	CommTimeOuts.ReadIntervalTimeout = 0xFFFFFFFF;
	CommTimeOuts.ReadTotalTimeoutMultiplier = 0;
	CommTimeOuts.ReadTotalTimeoutConstant = 0;
	CommTimeOuts.WriteTotalTimeoutMultiplier = 0;
	CommTimeOuts.WriteTotalTimeoutConstant = 5000;
	SetCommTimeouts( hPort, &CommTimeOuts );
	dcb.DCBlength = sizeof( DCB );
	GetCommState( hPort, &dcb );
	dcb.BaudRate = speedOrPort;
	dcb.ByteSize = 8;
	dcb.Parity = NOPARITY;
	dcb.StopBits = ONESTOPBIT;
	dcb.fOutxDsrFlow = 0;
	dcb.fDtrControl = DTR_CONTROL_DISABLE;
	dcb.fOutxCtsFlow = 0;
	dcb.fRtsControl = RTS_CONTROL_DISABLE;
	dcb.fInX = dcb.fOutX = 0;
	dcb.fBinary = TRUE;
	dcb.fParity = TRUE;
	dcb.fDsrSensitivity = 0;
	ret = SetCommState( hPort, &dcb );
	EscapeCommFunction( hPort, CLRDTR );
	
	/* init some variables used to unpack the incoming data */
	goodData = packLen = curCnt = crcErrors = hdrState = 0;
	currPkt = packet;
	inHdr = TRUE;
	return(ret);
}

/* Close the comm port */
void ClosePort()
{
	if( tcpipMode )  {
		if(soc == -1)
			return;
		closesocket(soc);
		soc = -1;
	}
	else  {
		if( hPort == (HANDLE)-1 )
			return;
		PurgeComm( hPort, PURGE_TXABORT|PURGE_RXABORT|PURGE_TXCLEAR|PURGE_RXCLEAR );
		CloseHandle( hPort );
		hPort = (HANDLE)-1;
	}
}

/* Calculate the crc of a buffer */
BYTE CalcCRC( BYTE *cp, short cnt )
{
	BYTE crc = 0;
	while( cnt-- )
		crc ^= *cp++;
	return crc;
}

/* Send data to the A/D Board */
void SendCommData( BYTE *data, int len, int fast )
{
	DWORD sent;
	int ret;
			
	if( tcpipMode )  {
		ret = send( soc, (char *)data, len, 0 );
		if( ret != len )  {
			printf("Send Error RET=%d\n", ret );
		}
	}
	while( len-- )  {
		WriteFile( hPort, data, 1, &sent, NULL );
		++data;
		if( !fast )		// throttle data if not fast
			_sleep( 5 );
	}
	time( &lastSendTime );
}

/* Reset the board by toggling the DTR line */
void ResetBoard()
{
	if( tcpipMode )
		return;
	EscapeCommFunction( hPort, SETDTR );
	_sleep( 400 );
	EscapeCommFunction( hPort, CLRDTR );
	_sleep( 100 );
	GetCommData();
	goodData = crcErrors = curCnt = packLen = hdrState = 0;
	currPkt = packet;
	inHdr = TRUE;
}

/* Used in the GPS Echo mode to receive one byte from the ADC board */
int GetEchoChar()
{
	COMSTAT ComStat;
	DWORD dwErrFlags, rcvd;
	char ret;
	struct timeval timeout;
	fd_set fds;
	int sts;
		
	if( tcpipMode )  {
		FD_ZERO(&fds);
		FD_SET(soc, &fds);
		memset(&timeout, 0, sizeof(timeout));
		if((sts = select(0, &fds, NULL, NULL, &timeout)) == SOCKET_ERROR)
			return -1;
		if(!sts)
			return 0;
		recv( soc, &ret, 1, 0 );
	}
	else  {
		ClearCommError( hPort, &dwErrFlags, &ComStat );
		if( !ComStat.cbInQue )
			return FALSE;
		ReadFile( hPort, &ret, 1, &rcvd, NULL );
	}
	return ret;
}

/* Used in the GPS Echo mode to send one byte to the ADC board */
void PutEchoChar( char chr )
{
	DWORD sent;
	if( tcpipMode )
		send( soc, &chr, 1, 0 );
	else
		WriteFile( hPort, &chr, 1, &sent, NULL );
}

BOOL TcpIpConnect( char *hostStr, int port )
{
	struct hostent *host;
	int bufferSize;
	SOCKADDR_IN remoteAddr;
	IN_ADDR RemoteIpAddress;
	int timeout = 5000;
	WSADATA WsaData;
	UINT err;
	
	soc = (SOCKET)-1;
		
	printf("TcpIp Connect Start\n");
	
	if(WSAStartup(0x0101, &WsaData) == SOCKET_ERROR)  {
		printf("WSAStartup() failed: %ld\n", GetLastError());
    	return(0);
	}
	
	if(!(host = gethostbyname(hostStr)))  {
		printf("gethostbyname failed\n");
		return(0);
	}		
	DWORD *d = (DWORD *)host->h_addr_list[0];
	RemoteIpAddress.S_un.S_addr	= *d;
	
	// Open a socket using the Internet Address family and TCP
	if((soc = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)  {
		printf("socket open failed: %ld\n", GetLastError());
		return(0);	
	}

	// Set the receive buffer size...
	bufferSize = 2048;
	err = setsockopt(soc, SOL_SOCKET, SO_RCVBUF, (char *)&bufferSize, sizeof(bufferSize));
	if(err == SOCKET_ERROR)  {
    	printf("setsockopt(SO_RCVBUF) failed: %ld", GetLastError());
		return(0);
	}
	ZeroMemory(&remoteAddr, sizeof(remoteAddr));
	remoteAddr.sin_family = AF_INET;
	remoteAddr.sin_port = htons(port);
	remoteAddr.sin_addr = RemoteIpAddress;

	printf("Connecting to %s...\n", hostStr);

	if(connect(soc, (PSOCKADDR)&remoteAddr, sizeof(remoteAddr)) == SOCKET_ERROR)  {
		printf("connect failed: %ld\n", GetLastError());
		return(0);
   	}
	printf("After Connect\n");
	setsockopt(soc, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout));
	setsockopt(soc, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
	
	/* init some variables used to unpack the incoming data */
	goodData = packLen = curCnt = crcErrors = hdrState = 0;
	currPkt = packet;
	inHdr = TRUE;
	
	return(TRUE);
}
