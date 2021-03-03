// display.h

#pragma once

#define	VFCHG	0x0001			/* Changed.			*/
#define	VFHBAD	0x0002			/* Hash and cost are bad.	*/

#define	COLOR_NORM_ATTR		0x1F
#define	COLOR_COLOR_ATTR	0x30
#define COLOR_ML_ATTR		0x1e
#define MONO_NORM_ATTR		0x0f
#define MONO_COLOR_ATTR		0x70

#define NCOL				256

class CDisplay
{
public:

	CDisplay();
	~CDisplay();
	
	void Init();
	void Close();
	void ClearScreen(WORD);
	void BigCursor();
	void LittleCursor();
	void SetRowCol();
	void MoveCursor(int row, int col);
	void ClearLine( int row );
	
	int	nrow;				/* Virtual cursor row.		*/
	int	ncol;				/* Virtual cursor column.	*/
	int	tthue;				/* Current color.		*/
	int	ttrow;				/* Physical cursor row.		*/
	int	ttcol;				/* Physical cursor column.	*/
	int	tceeol;
	
	HANDLE hDsp;
	
	COORD coordSize, coordDest;
	CHAR_INFO charInfo[NCOL+1];
	WORD chr_attr, norm_attr, color_attr, ml_attr; 

	CONSOLE_SCREEN_BUFFER_INFO csbiInfo; 
};

