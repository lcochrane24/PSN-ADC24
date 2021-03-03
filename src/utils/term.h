#include <afxwin.h>			// MFC core and standard components
#define NO_VBX_SUPPORT		// to prevent conflict with DDEML
#include <afxext.h> 		// MFC extensions (including VB)
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <conio.h>
#include <ctype.h>
#include <time.h>

int OpenPort(char *, int, HANDLE *);
void ClosePort();
void SendCommand( char chr );
void SendControl( BYTE data );
int SetBaud(int);
int GetCommChar(char *, int);
int GetCommStr(char *, int);
void CheckSaveSample(BYTE *);
void GetDataDump();
void UnpackBlock(BYTE *, int);
BOOL GetSession(int);
BOOL GetNumber(char *, char *, int *);
