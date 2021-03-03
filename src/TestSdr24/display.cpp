// display.cpp

#include "windows.h"
#include "display.h"
#include "stdio.h"

CDisplay::CDisplay()
{
    hDsp = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleScreenBufferInfo(hDsp, &csbiInfo);
	nrow = csbiInfo.dwSize.Y-1;
	ncol = csbiInfo.dwSize.X - 1;
}

CDisplay::~CDisplay()
{
	Close();
}

void CDisplay::Init()
{
 	coordSize.Y = nrow;
	coordSize.X = ncol+1;
	LittleCursor();
    ClearScreen(BACKGROUND_BLUE|FOREGROUND_BLUE|FOREGROUND_BLUE|FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY);
}

void CDisplay::Close()
{
    CloseHandle( hDsp );
}

void CDisplay::BigCursor()
{
	CONSOLE_CURSOR_INFO curInfo;
	curInfo.dwSize = 80;
	curInfo.bVisible = TRUE;
	SetConsoleCursorInfo(hDsp, &curInfo);
}

void CDisplay::LittleCursor()
{
	CONSOLE_CURSOR_INFO curInfo;
	curInfo.dwSize = 30;
	curInfo.bVisible = TRUE;
	SetConsoleCursorInfo(hDsp, &curInfo);
}

void CDisplay::MoveCursor(int row, int col)
{
	COORD home;
	home.X = ttcol = col;
	home.Y = ttrow = row;
	SetConsoleCursorPosition(hDsp, home);
}

void CDisplay::ClearScreen(WORD color)
{
	DWORD	dummy;
	COORD	Home = {0, 0};
	FillConsoleOutputAttribute(hDsp, color, 
		csbiInfo.dwSize.X * csbiInfo.dwSize.Y, Home, &dummy);
	FillConsoleOutputCharacter(hDsp, ' ', csbiInfo.dwSize.X * csbiInfo.dwSize.Y, Home, &dummy );
}

void CDisplay::ClearLine( int row )
{
	static char line[ 1024 ];
	static int first = 1;
	COORD cord;
	DWORD written;
	
	if( first )  {
		first = 0;
		char *to = line;
		int cnt = ncol;
		while( cnt-- )
			*to++ = ' ';
		*to = 0;
	}
	
	cord.X = 0;
	cord.Y = row;
	
	WriteConsoleOutputCharacter( hDsp, line, ncol-1, cord, &written );
}
