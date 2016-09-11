//	RBX430_lcd.c	08/06/2014
//******************************************************************************
//******************************************************************************
//	LL        CCCCC     DDCDDD
//	LL       CC   CC    DD   DD
//	LL      CC          DD    DD
//	LL      CC          DD    DD
//	LL      CC          DD    DD
//	LL       CC   CC    DD   DD
//	LLLLLL    CCCCC     DDDDDD
//******************************************************************************
//******************************************************************************
///	Author:			Paul Roper, Brigham Young University
//	Revision:		1.0		03/05/2012	RBX430-1
//					1.1		06/01/2012	divu8, image1, image2
//					1.2		09/17/2012	fill fixes
//					1.3		11/07/2012	lcd_bitImage, lcd_wordImage
//					1.4		11/25/2012	lcd_points 0-15
//					1.5		04/08/2013	_printfi minimal routine
//										lcd_printf / printf
//										lcd_sprintf / sprintf
//										override fputc, fputs in stdio library
//							07/23/2013	string.h included
//							08/08/2013	%2d fixed
//							11/19/2013	hex length limited to fieldSize
//					1.6					lcd_rectange uses int
//					1.7		08/06/2014	lcd_init initializes ports 2-4
//
//	Description:	Controller firmware for YM160160C/ST7529 LCD
//
//	Built with CCSv5.2 w/cgt 4.2.0
//******************************************************************************
//
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "msp430x22x4.h"
#include "RBX430-1.h"
#include "RBX430_lcd.h"

static volatile uint8 lcd_dmode;			// lcd mode
static volatile uint8 lcd_y;				// row (0-159)
static volatile uint8 lcd_x;				// column (0-159)
static volatile uint8 char_cnt;				// character counter
static volatile char* buffer_ptr;			// sprintf buffer ptr

static void WriteCmd(uint8 c);
static uint8 ReadData(void);
static uint16 ReadDataWord(void);
static void WriteData(uint8 c);
static void WriteDataWord(uint16 data);
static void DelayMs(uint16 time);

//******************************************************************************
//******************************************************************************
//	Function: void lcd_init()
//
//	PreCondition: none
//	Input: none
//	Output: none
//	Side Effects: none
//	Overview: resets LCD, initializes PMP
//
//	Note: Sitronix	ST7529 controller drive
//					1/160 Duty, 1/13 Bias
//
#define LCD_DELAY	50				// 50 ms

uint8 lcd_init(void)
{
	int i;

	// configure P2
	P2SEL = 0x00;				// GPIO
	P2OUT = 0x00;				// turn off all output pins
	P2REN = 0x00;				// no pull-ups
	P2DIR = 0xff;				// P2.0-7 output

	// configure P3
	P3SEL &= ~(LCD_A0|LCD_RW);		// P3.0=LCD_A0, P3.3=LCD_RW
	P3REN &= ~(LCD_A0|LCD_RW);		// no pull-ups
	P3OUT &= ~(LCD_A0|LCD_RW);
	P3DIR |= (LCD_A0|LCD_RW);

	// configure P4
	P4SEL &= ~(LCD_E|BK_LGT);		// P4.4=BK_LGT, P4.7=LCD_E
	P4REN &= ~(LCD_E|BK_LGT);		// no pull-ups
	P4OUT &= ~(LCD_E|BK_LGT);
	P4DIR |= (LCD_E|BK_LGT);

	LCD_A0_H;					// set A0 high (data)
	LCD_RW_H;					// set RW high (read)
	DelayMs(LCD_DELAY);

	// Hold in reset
	LCD_A0_L;					// set A0 low (command) RESET
	DelayMs(LCD_DELAY);

	// Release from reset
	LCD_A0_H;					// set A0 high (data)
	DelayMs(LCD_DELAY);

	WriteCmd(0x30);				// Ext = 0

	WriteCmd(0x94);				// Sleep Out

	WriteCmd(0xd1);				// OSC On
	WriteCmd(0x20);				// Power Control Set
		WriteData(0x08);		// Booster Must Be On First
	DelayMs(2);

	WriteCmd(0x20);				// Power Control Set
		WriteData(0x0b);		// Booster, Regulator, Follower ON

	WriteCmd(0x81);				// Electronic Control
		WriteData(VOP_CODE & 0x3f);
		WriteData(VOP_CODE >> 6);

	WriteCmd(0xca);				// Display Control
		WriteData(0x00);		// CLD=0
		WriteData(0x27);		// Duty=(160/4-1)=39
		WriteData(0x00);		// FR Inverse-Set Value ???

	WriteCmd(0xa6);				// Normal Display

	WriteCmd(0xbb);				// COM Scan Direction
		WriteData(0x01);		// 0->79 159->80

	WriteCmd(0xbc);				// Data Scan Direction
		WriteData(0x01);		// CI=0, LI=1
		WriteData(0x01);		// CLR=1 (P3/P2/P1)
		WriteData(0x01);		// 2B3P

	WriteCmd(0x31);				// Ext = 1

	WriteCmd(0x20);				// set gray level for odd frames
	i = 0;						// for (i = 0; i < 32; i += 2) WriteData(i);
	do
	{
		WriteData(i);
	} while ((i += 2) < 32);

	WriteCmd(0x21);				// set gray level for even frames
	i = 1;						// for (i = 1; i < 32; i += 2) WriteData(i);
	do
	{
		WriteData(i);
	} while ((i += 2) < 32);

	WriteCmd(0x32);				// Analog Circuit Set
		WriteData(0x00);		// OSC Frequency =000 (Default)
		WriteData(0x01);		// Booster Efficiency=01(Default)
		WriteData(0x01);		// Bias=1/13

	WriteCmd(0x34);				// Software init

	WriteCmd(0x30);				// Ext = 0
	WriteCmd(0xaf);				// Display On

	lcd_dmode = 0;
	lcd_y = 0;
	lcd_x = 0;					// column (0-159)
	return 0;
} // end  lcd_init


//******************************************************************************
//	Sitronix ST7529 controller functions
//
//	void WriteCmd(uint8 c)
//	int ReadData(void)
//	void WriteData(uint8 c)
//	void WriteDataWord(uint16 w)
//
//		A0	RW	A0 + ~RW	Function
//		--	--	--------	-----------------
//		0	0	   1		Control Write
//		0	1	   0		Control Read (Reset)
//		1	0      1		Display Write
//		1	1	   1		Display Read
//
//
void WriteCmd(uint8 c)
{
	P2DIR = 0xff;		// output to P2
	P2OUT = c;			// set data on output lines
	LCD_RW_L;			// set RW low (write)
	LCD_A0_L;			// set A0 low (command)
	LCD_E_H;			// toggle E
	LCD_E_L;
	return;
} // end WriteCmd


uint8 ReadData(void)
{
	uint8 data;

	P2DIR = 0x00;		// input from P2
	LCD_A0_H;			// set A0 high (data)
	LCD_RW_H;			// set RW high (read)
	LCD_E_H;			// toggle E
	_no_operation();	// nop
	data = P2IN;		// read data
	LCD_E_L;
	return data;
} // end ReadData


void WriteData(uint8 c)
{
	P2DIR = 0xff;		// output to P2
	P2OUT = c;			// set data on output lines
	LCD_A0_H;			// set A0 high (data)
	LCD_RW_L;			// set RW low (write)
	LCD_E_H;			// toggle E
	LCD_E_L;
	return;
} // end WriteData


uint16 ReadDataWord(void)
{
	uint16 data;

	P2DIR = 0x00;		// input from P2
	LCD_A0_H;			// set A0 high (data)
	LCD_RW_H;			// set RW high (read)
	LCD_E_H;			// toggle E
	_no_operation();	// nop
	data = P2IN;		// read high data
	LCD_E_L;
	LCD_E_H;			// toggle E
	_no_operation();	// nop
	data = (data << 8) + P2IN;		// read low data
	LCD_E_L;
	return data;
} // end ReadDataWord


void WriteDataWord(uint16 data)
{
	P2DIR = 0xff;		// output to P2
	P2OUT = data >> 8;	// set data on output lines
	LCD_A0_H;			// set A0 high (data)
	LCD_RW_L;			// set RW low (write)
	LCD_E_H;			// toggle E
	LCD_E_L;
	_no_operation();	// nop
	P2OUT = data;		// set data on output lines
	LCD_E_H;			// toggle E
	LCD_E_L;
	return;
} // end WriteDataWord


//******************************************************************************
//	Function: void DelayMs(WORD time)
//
//	PreCondition: none
//	Input: time - delay in ms
//	Output: none
//	Side Effects: none
//	Overview: delays execution on time specified in ms
//
//	Note: none
//
//******************************************************************************
//
#define DELAY_1MS	100

void DelayMs(uint16 time)
{
	volatile uint16 delay;

	// save current clock speed
	volatile uint8 saved_BCSCTL1 = BCSCTL1;
	volatile uint8 saved_DCOCTL = DCOCTL;

	// select 1MHz
	BCSCTL1 = CALBC1_1MHZ;			// Set range 1MHz
	DCOCTL = CALDCO_1MHZ;			// Set DCO step + modulation

	// delay (time) ms
	do
	{
		for (delay = DELAY_1MS; delay > 0;) --delay;
	} while (time--);

	// restore clocks
	BCSCTL1 = saved_BCSCTL1;
	DCOCTL = saved_DCOCTL;
	return; 
} // end DelayMs


//******************************************************************************
//	fast uint8 / 3
//
//	q = (n >> 2) + (n >> 4);		// q = n*0.0101 (approx).
//	q += (q >> 4);					// q = n*0.01010101.
//	q += (q >> 8);					// (not needed for uint8/3)
//	r = n - q * 3;					// 0 <= r <= 15.
//	return q + ((11 * r) >> 5);		// Returning q + r/3.
//
//	See Hacker's Delight, Henry S. Warren, Jr., 10-3
//
#if 0
unsigned divu3(unsigned n)
{
	unsigned q, r, t;
	q = (n >> 2);					// q = n*0.01
	q += (q >> 2);					// q = n*0.0101
	q += (q >> 4);					// q = n*0.01010101
	r = n - ((q << 1) + q);			// 0 <= r <= 15.
	t = r << 3;						// Returning q + r/3.
	return q + ((r + r + r + t) >> 5);
} // end divu3
#else
const char dv3[] = {
		0,0,0, 1,1,1, 2,2,2, 3,3,3, 4,4,4, 5,5,5, 6,6,6, 7,7,7, 8,8,8,
		9,9,9, 10,10,10, 11,11,11, 12,12,12, 13,13,13, 14,14,14, 15,15,15,
		16,16,16, 17,17,17, 18,18,18, 19,19,19, 20,20,20, 21,21,21,
		22,22,22, 23,23,23, 24,24,24, 25,25,25, 26,26,26, 27,27,27,
		28,28,28, 29,29,29, 30,30,30, 31,31,31, 32,32,32, 33,33,33,
		34,34,34, 35,35,35, 36,36,36, 37,37,37, 38,38,38, 39,39,39,
		40,40,40, 41,41,41, 42,42,42, 43,43,43, 44,44,44, 45,45,45,
		46,46,46, 47,47,47, 48,48,48, 49,49,49, 50,50,50, 51,51,51,
		52,52,52, 53,53,53 };

#define divu3(n)	dv3[n]
#endif


//******************************************************************************
//	set lcd x, y
//
void lcd_set_x_y(uint8 x, uint8 y)
{
	WriteCmd(0x75);					// set line address
		WriteData(y);				// from line 0 - 159
		WriteData(0x9f);

	WriteCmd(0x15);					// set column address
		WriteData(divu3(x));		// from col 0 - 160/3
		WriteData(0x35);
	return;
} // end lcd_set_x_y


//******************************************************************************
//	lcd read word
//
uint16 lcd_read_word(int16 column, int16 row)
{
	WriteCmd(0x75);					// set line address
		WriteData(row);				// from line 0 - 159
		WriteData(0x9f);

	WriteCmd(0x15);					// set column address
		WriteData(column);			// from col 0 - 160/3
		WriteData(0x35);
	WriteCmd(0x5d);					// RAMRD - read from memory
	ReadData();						// Dummy read
//	return (ReadData() << 8) + ReadData();
	return ReadDataWord();
} // end lcd_read_word


//******************************************************************************
//	lcd write word
//
void lcd_write_word(int16 column, int16 row, uint16 data)
{
	WriteCmd(0x75);					// set line address
		WriteData(row);				// from line 0 - 159
		WriteData(0x9f);

	WriteCmd(0x15);					// set column address
		WriteData(column);			// from col 0 - 160/3
		WriteData(0x35);
	WriteCmd(0x5c);					// RAMWR - write to memory
	WriteData(data >> 8);			// write high byte
	WriteData(data & 0x00ff);		// write low byte
	return;
} // end lcd_write_word


//******************************************************************************
//	clear lcd screen
//
void lcd_clear()
{
	lcd_set(0xffdf);				// clear lcd
} // end lcd_clear


//******************************************************************************
//	set lcd screen
//
void lcd_set(uint16 value)
{ 
	int i; 

	lcd_set_x_y(0, 0);			// upper right corner
	WriteCmd(0x5c);				// start write

	// whole screen - rows x columns
//	for (i = 0; i < 160 * (divu3((160*2))); i++)
//	for (i = 160 * (divu3((160*2))); i > 0; --i)
//	for (i = 160 * (160 * 2 / 3); i > 0; --i)

	i = 160 * (160 * 2 / 3);
	do
	{
		WriteDataWord(value);
	} while (--i);
	lcd_dmode = 0;				// reset mode
	lcd_y = 0;					// lower left hand corner
	lcd_x = 0;
	return;
} // end  lcd_set


//******************************************************************************
//	R/W lcd point
//
//	IN:		x = column coordinate
//			y = row coordinate
//			flag =	0000 0000
//			         \\\\ \\\\
//			          \\\\ \\\\_ 0=erase, 1=draw
//			           \\\\ \\\_ \
//			            \\\\ \\_  Point size
//			             \\\\ \_ /
//			              \\\\
//			               \\\\_ 0=no fill, 1=fill
//			                \\\_
//			                 \\_
//			                  \_ 1 = read
//
//																oooooo
//						-ooo-							ooooo	oooooo
//						ooooo					oooo	ooooo	oooooo
//				-o-		ooXoo			ooo		oooo	ooXoo	ooXooo
//				oXo		ooooo	oo		oXo		oXoo	ooooo	oooooo
//		o		-o-		-ooo-	Xo		ooo		oooo	ooooo	oooooo
//
//		0/1		2/3		4/5		6/7		8/9		10/11	12/13	14/15
//		SINGLE	TRIPLE	PENT	DOUBLEX	TRIPLEX	QUADX	PENTX	SEXTX
//
//	return results
//
const unsigned int px_on[] = { 0xffc0, 0xf81f, 0x07ff };
const unsigned int px_off[] = { 0x001f, 0x07c0, 0xf800 };

uint8 lcd_point(int16 x, int16 y, int8 flag)
{
	uint16 pixel, xd3;

	// return 1 if out of range
	if ((x < 0) || (x >= HD_X_MAX)) return 1;
	if ((y < 0) || (y >= HD_Y_MAX)) return 1;

	if (flag & 0x80)
	{
		flag = 0x80;						// set flag = 0x80 to read
	}
	else
	{
		uint16 on_off = flag & 0x01;
		flag &= 0x000f;
		switch (flag)
		{
			case 0:								// single
			case 1:
				flag &= 0x0001;
				break;

			case 4:								// penta
			case 5:
				lcd_point(x-1, y+2, on_off);	//	-ooo-
				lcd_point(x, y+2, on_off);		//	ooooo
				lcd_point(x+1, y+2, on_off);	//	ooXoo
												//	ooooo
				lcd_point(x-2, y+1, on_off);	//	-ooo-
				lcd_point(x+2, y+1, on_off);
				lcd_point(x-2, y, on_off);
				lcd_point(x+2, y, on_off);
				lcd_point(x-2, y-1, on_off);
				lcd_point(x+2, y-1, on_off);

				lcd_point(x-1, y-2, on_off);
				lcd_point(x, y-2, on_off);
				lcd_point(x+1, y-2, on_off);

				lcd_point(x-1, y+1, on_off);	//	ooo
				lcd_point(x+1, y+1, on_off);	//	oXo
				lcd_point(x-1, y-1, on_off);	//	ooo
				lcd_point(x+1, y-1, on_off);

			case 2:								// triple
			case 3:
				lcd_point(x, y+1, on_off);		//	-o-
				lcd_point(x-1, y, on_off);		//	oXo
				lcd_point(x, y, on_off);		//	-o-
				lcd_point(x+1, y, on_off);
				lcd_point(x, y-1, on_off);
				return 0;

			default:							// square points
			{
				int i,j;
				int size = (flag >> 1) - 1;
				int end = (size >> 1);
				int start = end - size + 1;
				for (i = start; i <= end; ++i)
				{
					for (j = start; j <= end; ++j)
					{
						lcd_point(x + i, y + j, on_off);
					}
				}
				return 0;
			}
		}
	}

	// process point
	x = 159 - x;						// translate to lcd coordinates
	lcd_set_x_y(x, y);					// upper right corner

	// read 3 points from lcd
	WriteCmd(0xe0);						// RMWIN - read and modify write
	ReadData();							// Dummy read
	pixel = ReadDataWord();				// Start read cycle for pixel 2/1/0

	// turn point on/off
	xd3 = divu3((unsigned int)x);
	xd3 = x - xd3 - xd3 - xd3;			// x % 3 = (x - divu3(x) * 3)
	switch (flag)
	{
		case 0:							// turn point off
			pixel |= px_off[xd3];
			break;

		case 1:							// turn point on
			pixel &= px_on[xd3];
			break;

		default:
		case 128:						// read point
			WriteCmd(0xee);				// RMWOUT - cancel read modify write mode
//			return (pixel & px_off[xd3]) ? 0 : 1;
			return ((pixel & px_off[xd3]) == px_off[xd3]) ? 0 : 1;
	}
	// write pixels back to lcd
	WriteDataWord(pixel);				// Write pixels back
	WriteCmd(0xee);						// RMWOUT - cancel read modify write mode
	return 0;							// return success
} // end lcd_point


//******************************************************************************
//	Display Image Functions:
//
//	uint8 lcd_image(const uint8* image, int16 x, int16 y)
//	uint8 lcd_bitImage(const uint8* image, int16 x, int16 y, uint8 flag)
//	uint8 lcd_wordImage(const uint16* image, int16 x, int16 y, uint8 flag)
//
//******************************************************************************
//	uint8 lcd_image(const uint8* image, int16 x, int16 y)
//
//	IN:		const char* image ->	uint8 width
//									uint8 height
//									(8-bit column value) x width
//									...
//									... height % 8 rows.
//
//	OUT:	Return 0
//
uint8 lcd_image(const uint8* image, int16 x, int16 y)
{
	int16 x1, y1, data, mask;
	int16 right = x + *image++;			// stop at right side of image
	int16 bottom = y;					// finish at bottom
	y += *image++;						// get top of image

	while (y > bottom)					// display from top down
	{
		for (x1 = x; x1 < right; ++x1)	// display from left to right
		{
			data = *image++;			// get image byte
			y1 = y;
//			for (mask = 0x80; mask; mask >>= 1)
			mask = 0x80;
			do
			{
				if (data & mask) lcd_point(x1, --y1, 1);
				else if (1) lcd_point(x1, --y1, 0);
			} while (mask >>= 1);
		}
		y -= 8;							// next row
	}	
	return 0;
} // end lcd_image


//******************************************************************************
//	output LCD B/W bit image
//
//	flag = 0	blank image
//	       1	output LCD RAM image
//	       2	fill image area
//
//	IN:		const char* image ->	uint8 width
//									uint8 height
//									(8-bit row value) x (width % 8)
//									...
//									... height rows.
//
//			x position must be divisible by 3
//
//	OUT:	Return 0
//
uint8 lcd_bitImage(const uint8* image, int16 x, int16 y, uint8 flag)
{
	int16 i, data, index;
	uint8 bits, mask;
	int16 width = *image++;				// get width/height
	int16 height = *image++;
	int16 bottom = y;

	x += width - 1;						// move to top, left (make 0 based)
	y += height;

	while (y > bottom)					// display from top down
	{
		lcd_set_x_y(159 - x, y);		// upper right corner
		WriteCmd(0x5c);					// write to memory

		data = 0x0000;					// fill
		switch (flag)
		{
			case 0:
				data = 0xffdf;			// erase

			case 2:
			{
				for (i = width; i > 0; i -= 3)	// display from right to left
				{
					WriteDataWord(data);
				}
			}
			break;

			default:
			case 1:
			{
				image += (width >> 3);		// point to end of image line
				mask = 0x80;
				data = 0xffdf;				// assume all off
				index = 0;

				for (i = width; i > 0; --i)		// display from right to left
				{
					mask <<= 1;					// adjust mask
					if (mask == 0)
					{
						mask = 0x01;			// reset mask
						bits = *--image;		// get next data byte
					}
					if (bits & mask)
					{
						if (index == 0) data &= 0xffe0;
						else if (index == 1) data &= 0xf83f;
						else data &= 0x07df;
					}
					if (++index == 3)
					{
						WriteDataWord(data);
						data = 0xffdf;			// assume all off
						index = 0;
					}
				}
				if (index) WriteDataWord(data);	// flush data
				image += (width >> 3);			// point to end of image line
			}
			break;
		}
		y--;									// next row
	}
	return 0;
} // end lcd_bitImage


//******************************************************************************
//	output 4-bit gray-scale LCD ram image (3 4-bit pixels / word)
//
//	flag = 0	blank image
//	       1	output LCD RAM image
//	       2	fill image area
//	       3	reverse output LCD RAM image
//
//	IN:		const uint16* image ->	uint16 width
//									uint16 height
//									(16-bit LCD RAM value) x (width / 3) R to L order
//									...
//									... height rows.
//
//	OUT:	Return 0;
//
//	Note:	1. x position must be divisible by 3
//			2. 0xccFF used to compress 0 values
//			3. RAM value = 3P/2B (1111 1222 2203 3333)
//			4. ccf- = special run of 3 pixels
//					ccff = run of 3 pixels off
//					ccfe = run of 3 pixels on
//					ccf0,pppp = run of pppp pixels
//
#define M2B3P(P0,P1,P2) ((((P0^0xff)&0x1f)<<5)+((P1^0xff)&0x1f))<<6)+((P2^0xff)&0x1f)

uint8 lcd_wordImage(const uint16* image, int16 x, int16 y, uint8 flag)
{
	int16 x1, reverse = 0;
	int16 runCnt = 0;
	uint16 runPixels;

	int16 width = *image++;				// get width/height
	int16 height = *image++;
	int16 bottom = y;

	x += width - 1;						// move to top, left (make 0 based)
	y += height;
//	width = divu3((width + 2));			// 3 pixels per 2 bytes (round up)

	if (flag == 3)
	{
		flag = 1;
		reverse = 1;
	}
	while (y > bottom)					// display from top down
	{
		lcd_set_x_y(159 - x, y);		// upper right corner
		WriteCmd(0x5c);					// write to memory

		for (x1 = width; x1 > 0; x1 -= 3)			// display from right to left
		{
			switch (flag)						// switch on mode
			{
				case 0:
					WriteDataWord(0xffdf);		// write 3 pixels off
					break;

				case 1:
					// check for run of 3 pixels
					if (runCnt)
					{
						--runCnt;
						WriteDataWord(reverse ? ~runPixels : runPixels);		// output 3 run pixels
						break;
					}

					// check for special code (ccfx)
					if (*image & 0x0020)
					{
						runCnt = *image++;				// get special code
						switch (runCnt & 0x00ff)		// switch to special case
						{
							case 0x00ff:
								runPixels = 0xffdf;		// 3 pixels off
								break;

							case 0x00fe:
								runPixels = 0x0000;		// 3 pixels on
								break;

							case 0x00f0:
							default:
								runPixels = ~*image++;	// run of 3 pixels
								break;
						}
						runCnt >>= 8;					// get run count
//						++x1;						// setup 1st output
						x1 += 3;						// setup 1st output
						break;
					}

					WriteDataWord(reverse ? *image++ : ~*image++);
					break;

				default:
				case 2:
					WriteDataWord(0x0000);		// write 3 pixels on (fill)
					break;
			}
		}
		y--;									// next row
	}
	return 0;
} // end lcd_wordImage


//******************************************************************************
//	Fill Image
//
//	IN:		x, y			lower right coordinates
//			width,height	area to blank
//			flag = 0		blank image
//	    		   1		(unused)
//	    		   2		fill image area
//
//	OUT:	return 0;
//
uint8 lcd_fillImage(int16 x, int16 y, uint16 width, uint16 height, uint8 flag)
{
	uint16 area[2];
	area[0] = width;
	area[1] = height;
	return lcd_wordImage(area, x, y, flag);
}


//******************************************************************************
//	Blank Image
//
//	IN:		x, y			lower right coordinates
//			width,height	area to blank
//
//	OUT:	return 0;
//
uint8 lcd_blank(int16 x, int16 y, uint16 width, uint16 height)
{
	int16 x0, y0;

	for (x0 = x + width - 1; x0 >= x; --x0)
	{
		for (y0 = y + height - 1; y0 >= y; --y0)
		{
			lcd_point(x0, y0, 0);
		}
	}
	return 0;
} // end lcd_blank


//******************************************************************************
//	change lcd volume (brightness)
//
//		VOP = 3.6 + (volume x 0.04)
//
//		default	3.6 + (257 x 0.04) = 13.88v
//				3.6 + (335 x 0.04) = 17.0v
//				3.6 + (360 x 0.04) = 18.0v
//		max		3.6 + (410 x 0.04) = 20.0v	(DO NOT EXCEED!)
//
void lcd_volume(uint16 volume)
{
	if (volume > 410) volume = 410;		// limit to 20.0v
	WriteCmd(0x81);						// Electronic Control
		WriteData(volume & 0x3f);
		WriteData(volume >> 6);
	return;
} // end lcd_volume


//******************************************************************************
//	Turn ON/OFF LCD backlight
//
void lcd_backlight(uint8 backlight)
{
	if (backlight)
	{
		BACKLIGHT_ON;					// turn on backlight
	}
	else
	{
		BACKLIGHT_OFF;					// turn off backlight
	}
	return;
} // end lcd_backlight


//******************************************************************************
//	Display Mode
//
//	IN:		mode
//	OUT:	old mode
//
//	xxxx xxxx
//	 \\\\ \\\\___ LCD_PROPORTIONAL		proportional font
//	  \\\\ \\\___ LCD_REVERSE_FONT		reverse font
//	   \\\\ \\___ LCD_2X_FONT			2x font
//	    \\\\ \___ LCD_OR_CHAR			write char in OR mode
//	     \\\\____
//	      \\\____
//	       \\____
//	        \____ clear bis
//
//	~mode = Turn OFF mode bit(s)
//
uint8 lcd_mode(int8 mode)
{
	if (mode)
	{
		// set/reset mode bits
		if (mode & 0x80)
		{
			lcd_dmode &= mode;			// reset bits
		}
		else
		{
			lcd_dmode |= mode;			// set mode bits
		}
	}
	else
	{
		lcd_dmode = 0;
	}
	return lcd_dmode;
} // end lcd_mode


//******************************************************************************
//	draw circle of radius r0 and center x0,y0
//
void lcd_circle(int16 x0, int16 y0, uint16 r0, uint8 pen)
{
	int16 x, y, d;
	int16 i, j;

	x = x0;
	y = y0 + r0;
	d =  3 - r0 * 2;

	do
	{
		if (pen & FILL)
		{
			for (i = x0 - (x - x0); i <= x; ++i)
    		{
        		lcd_point(i, y, pen);
    			lcd_point(i,  y0 - (y - y0), pen);
    		}
    		for (j = y0 - (x - x0); j <= y0 + (x - x0); ++j)
    		{
    			for (i = x0 - (y - y0); i <= x0 + (y - y0); ++i)
    			{
    				lcd_point(i, j, pen);
    			}
    		}
		}
		else
		{
			lcd_point(x, y, pen);
    		lcd_point(x, y0 - (y - y0), pen);
    		lcd_point(x0 - (x - x0), y, pen);
    		lcd_point(x0 - (x - x0), y0 - (y - y0), pen);

    		lcd_point(x0 + (y - y0), y0 + (x - x0), pen);
    		lcd_point(x0 + (y - y0), y0 - (x - x0), pen);
    		lcd_point(x0 - (y - y0), y0 + (x - x0), pen);
    		lcd_point(x0 - (y - y0), y0 - (x - x0), pen);
		}
		if (d < 0)
		{
			d = d +  ((x - x0) << 2) + 6;
		}
		else
		{
			d = d + (((x - x0) - (y - y0)) << 2) + 10;
			--y;
		}
		++x;
	} while ((x - x0) <= (y - y0));
	return;
} // end lcd_circle


//******************************************************************************
//	draw square of radius r0 and center x0,y0
//
//	pen =	0000 0000
//	         \\\\ \\\\
//	          \\\\ \\\\_ 0=erase, 1=draw
//	           \\\\ \\\_ 0=single, 1=double
//	            \\\\ \\_ 0=no fill, 1=fill
//
void lcd_square(int16 x0, int16 y0, uint16 r0, uint8 pen)
{
	lcd_rectangle(x0 - r0, y0 - r0, r0 + r0 + 1, r0 + r0 + 1, pen);
	return;
} // end lcd_square


//******************************************************************************
//	draw star of radius r0 and center x0,y0
//
//	pen =	0000 0000
//	         \\\\ \\\\
//	          \\\\ \\\\_ 0=erase, 1=draw
//	           \\\\ \\\_ 0=single, 1=double
//	            \\\\ \\_ 0=no fill, 1=fill
//
void lcd_star(int16 x0, int16 y0, uint16 r0, uint8 pen)
{
	int16  r = 0;

	do
	{
		uint16 y1 = y0 + r;
		uint16 y2 = y0 - r;

		lcd_point(x0 - r0, y1, pen);
		lcd_point(x0 + r0, y1, pen);
		lcd_point(x0 - r0, y2, pen);
		lcd_point(x0 + r0, y2, pen);
		if (r0)
		{
			lcd_point(x0 - r0 + 1, y1, pen);
			lcd_point(x0 + r0 - 1, y1, pen);
			lcd_point(x0 - r0 + 1, y2, pen);
			lcd_point(x0 + r0 - 1, y2, pen);
		}

		++r;
	} while (r0--);
	return;
} // end lcd_star


//******************************************************************************
//	draw triangle of radius r0 and center x0,y0
//
//	pen =	0000 0000
//	         \\\\ \\\\
//	          \\\\ \\\\_ 0=erase, 1=draw
//	           \\\\ \\\_ 0=single, 1=double
//	            \\\\ \\_ 0=no fill, 1=fill
//			     \\\\ \_ 0=
//
void lcd_triangle(int16 x0, int16 y0, uint16 r0, uint8 pen)
{
	int16  x, y;
	uint8 fill = 1;						// always fill bottom
	y = y0 - r0;						// start at bottom
	r0 <<= 1;							// radius * 2
	do
	{
		lcd_point(x0 - (r0 >> 1), y, pen);	// draw end points
		lcd_point(x0 + (r0 >> 1), y, pen);
		if (fill)						// if bottom or fill
		{
			for (x = (x0 - (r0 >> 1) + 1); x <= (x0 + (r0 >> 1) - 1); ++x)
			{
				lcd_point(x, y, pen);
			}
		}
		++y;
		fill = pen & FILL;
	} while (r0--);
	return;
} // end lcd_triangle


//******************************************************************************
//	draw diamond of radius r and center x,y
//
//	pen =	0000 0000
//	         \\\\ \\\\
//	          \\\\ \\\\_ 0=erase, 1=draw
//	           \\\\ \\\_ 0=single, 1=double
//	            \\\\ \\_ 0=no fill, 1=fill
//			     \\\\ \_ 0=
//
void lcd_diamond(int16 x, int16 y, uint16 r, uint8 pen)
{
	int16  x0, y0, y1;
	uint8 fill = pen & FILL;			// always fill bottom
	y0 = y1 = y;						// start in middle
	do
	{
		lcd_point(x - r, y0, pen);		// draw middle points
		lcd_point(x + r, y0, pen);
		lcd_point(x - r, y1, pen);		// draw middle points
		lcd_point(x + r, y1, pen);
		if (fill)						// if bottom or fill
		{
			for (x0 = (x - r + 1); x0 <= (x + r - 1); ++x0)
			{
				lcd_point(x0, y0, pen);
				lcd_point(x0, y1, pen);
			}
		}
		++y0;
		--y1;
	} while (r--);
	return;
} // end lcd_diamond


//******************************************************************************
//	draw rectangle at lower left (x0,y0) of width w, height h
//
//	pen =	0000 0000
//	         \\\\ \\\\
//	          \\\\ \\\\_ 0=erase, 1=draw
//	           \\\\ \\\_ \
//	            \\\\ \\_  pen size
//	             \\\\ \_ /
//	              \\\\
//	               \\\\_ 0=no fill, 1=fill
//
#if 0
void lcd_rectangle(int16 x, int16 y, uint16 w, uint16 h, uint8 pen)
{
	int16  x0, y0;
	uint8 fill_flag = (pen & FILL) ? 1 : 0;

	if (w-- == 0) return;
	for (y0 = y; y0 < y + h; ++y0)
	{
		lcd_point(x, y0, pen);
		if ((y0 == y) || (y0 == y + h - 1) || fill_flag)
		{
			for (x0 = x + 1; x0 < x + w; ++x0)
			{
				lcd_point(x0, y0, pen);
			}
		}
		lcd_point(x + w, y0, pen);
	}
	return;
} // end lcd_rectangle
#else
void lcd_rectangle(int16 x, int16 y, int16 w, int16 h, uint8 pen)
{
	uint8 fill_flag = (pen & FILL) ? 1 : 0;
	if (w-- == 0) return;
	int y0 = y;
	do
	{
		lcd_point(x, y0, pen);
		if ((y0 == y) || (y0 == y + h - 1) || fill_flag)
		{
			//for (x0 = x + 1; x0 < x + w; ++x0)
			int x0 = x + 1;
			do
			{
				lcd_point(x0, y0, pen);
			} while (++x0 < x + w);
		}
		lcd_point(x + w, y0, pen);
	} while (++y0 < y + h);
	return;
} // end lcd_rectangle
#endif


//******************************************************************************
//******************************************************************************
//	ASCII character set for LCD
//
const unsigned char cs[][5] = {
  { 0x00,0x00,0x00,0x00,0x00 },  // SP ----- -OO-- OO-OO ----- -O--- OO--O -O--- -OO--
  { 0xfa,0xfa,0x00,0x00,0x00 },  // !  ----- -OO-- OO-OO -O-O- -OOO- OO--O O-O-- -OO--
  { 0xe0,0xc0,0x00,0xe0,0xc0 },  // "  ----- -OO-- O--O- OOOOO O---- ---O- O-O-- -----
  { 0x24,0x7e,0x24,0x7e,0x24 },  // #  ----- -OO-- ----- -O-O- -OO-- --O-- -O--- -----
  { 0x24,0xd4,0x56,0x48,0x00 },  // $  ----- -OO-- ----- -O-O- ---O- -O--- O-O-O -----
  { 0xc6,0xc8,0x10,0x26,0xc6 },  // %  ----- ----- ----- OOOOO OOO-- O--OO O--O- -----
  { 0x6c,0x92,0x6a,0x04,0x0a },  // &  ----- -OO-- ----- -O-O- --O-- O--OO -OO-O -----
  { 0xc0,0xc0,0x00,0x00,0x00 },  // '  ----- ----- ----- ----- ----- ----- ----- -----
//
  { 0x7c,0x82,0x00,0x00,0x00 },  // (  ---O- -O--- ----- ----- ----- ----- ----- -----
  { 0x82,0x7c,0x00,0x00,0x00 },  // )  --O-- --O-- -O-O- --O-- ----- ----- ----- ----O
  { 0x10,0x7c,0x38,0x7c,0x10 },  // #  --O-- --O-- -OOO- --O-- ----- ----- ----- ---O-
  { 0x10,0x10,0x7c,0x10,0x10 },  // +  --O-- --O-- OOOOO OOOOO ----- OOOOO ----- --O--
  { 0x06,0x07,0x00,0x00,0x00 },  // ,  --O-- --O-- -OOO- --O-- ----- ----- ----- -O---
  { 0x10,0x10,0x10,0x10,0x10 },  // -  --O-- --O-- -O-O- --O-- -OO-- ----- -OO-- O----
  { 0x06,0x06,0x00,0x00,0x00 },  // .  ---O- -O--- ----- ----- -OO-- ----- -OO-- -----
  { 0x04,0x08,0x10,0x20,0x40 },  // /  ----- ----- ----- ----- --O-- ----- ----- -----
//
//{ 0x42,0xfe,0x02,0x00,0x00 },  // 1
  { 0x7c,0x8a,0x92,0xa2,0x7c },  // 0  -OOO- --O-- -OOO- -OOO- ---O- OOOOO --OOO OOOOO
  { 0x00,0x42,0xfe,0x02,0x00 },  // 1  O---O -OO-- O---O O---O --OO- O---- -O--- ----O
  { 0x46,0x8a,0x92,0x92,0x62 },  // 2  O--OO --O-- ----O ----O -O-O- O---- O---- ---O-
  { 0x44,0x92,0x92,0x92,0x6c },  // 3  O-O-O --O-- --OO- -OOO- O--O- OOOO- OOOO- --O--
  { 0x18,0x28,0x48,0xfe,0x08 },  // 4  OO--O --O-- -O--- ----O OOOOO ----O O---O -O---
  { 0xf4,0x92,0x92,0x92,0x8c },  // 5  O---O --O-- O---- O---O ---O- O---O O---O -O---
  { 0x3c,0x52,0x92,0x92,0x8c },  // 6  -OOO- -OOO- OOOOO -OOO- ---O- -OOO- -OOO- -O---
  { 0x80,0x8e,0x90,0xa0,0xc0 },  // 7  ----- ----- ----- ----- ----- ----- ----- -----
//
  { 0x6c,0x92,0x92,0x92,0x6c },  // 8  -OOO- -OOO- ----- ----- ---O- ----- -O--- -OOO-
  { 0x60,0x92,0x92,0x94,0x78 },  // 9  O---O O---O ----- ----- --O-- ----- --O-- O---O
  { 0x36,0x36,0x00,0x00,0x00 },  // :  O---O O---O -OO-- -OO-- -O--- OOOOO ---O- O---O
  { 0x36,0x37,0x00,0x00,0x00 },  // ;  -OOO- -OOOO -OO-- -OO-- O---- ----- ----O --OO-
  { 0x10,0x28,0x44,0x82,0x00 },  // <  O---O ----O ----- ----- -O--- ----- ---O- --O--
  { 0x24,0x24,0x24,0x24,0x24 },  // =  O---O ---O- -OO-- -OO-- --O-- OOOOO --O-- -----
  { 0x82,0x44,0x28,0x10,0x00 },  // >  -OOO- -OO-- -OO-- -OO-- ---O- ----- -O--- --O--
  { 0x60,0x80,0x9a,0x90,0x60 },  // ?  ----- ----- ----- --O-- ----- ----- ----- -----
//
  { 0x7c,0x82,0xba,0xaa,0x78 },  // @  -OOO- -OOO- OOOO- -OOO- OOOO- OOOOO OOOOO -OOO-
  { 0x7e,0x90,0x90,0x90,0x7e },  // A  O---O O---O O---O O---O O---O O---- O---- O---O
  { 0xfe,0x92,0x92,0x92,0x6c },  // B  O-OOO O---O O---O O---- O---O O---- O---- O----
  { 0x7c,0x82,0x82,0x82,0x44 },  // C  O-O-O OOOOO OOOO- O---- O---O OOOO- OOOO- O-OOO
  { 0xfe,0x82,0x82,0x82,0x7c },  // D  O-OOO O---O O---O O---- O---O O---- O---- O---O
  { 0xfe,0x92,0x92,0x92,0x82 },  // E  O---- O---O O---O O---O O---O O---- O---- O---O
  { 0xfe,0x90,0x90,0x90,0x80 },  // F  -OOO- O---O OOOO- -OOO- OOOO  OOOOO O---- -OOO-
  { 0x7c,0x82,0x92,0x92,0x5c },  // G  ----- ----- ----- ----- ----- ----- ----- -----
//
  { 0xfe,0x10,0x10,0x10,0xfe },  // H  O---O -OOO- ----O O---O O---- O---O O---O -OOO-
  { 0x82,0xfe,0x82,0x00,0x00 },  // I  O---O --O-- ----O O--O- O---- OO-OO OO--O O---O
  { 0x0c,0x02,0x02,0x02,0xfc },  // J  O---O --O-- ----O O-O-- O---- O-O-O O-O-O O---O
  { 0xfe,0x10,0x28,0x44,0x82 },  // K  OOOOO --O-- ----O OO--- O---- O---O O--OO O---O
  { 0xfe,0x02,0x02,0x02,0x02 },  // L  O---O --O-- O---O O-O-- O---- O---O O---O O---O
  { 0xfe,0x40,0x20,0x40,0xfe },  // M  O---O --O-- O---O O--O- O---- O---O O---O O---O
  { 0xfe,0x40,0x20,0x10,0xfe },  // N  O---O -OOO- -OOO- O---O OOOOO O---O O---O -OOO-
  { 0x7c,0x82,0x82,0x82,0x7c },  // O  ----- ----- ----- ----- ----- ----- ----- -----
//
  { 0xfe,0x90,0x90,0x90,0x60 },  // P  OOOO- -OOO- OOOO- -OOO- OOOOO O---O O---O O---O
  { 0x7c,0x82,0x92,0x8c,0x7a },  // Q  O---O O---O O---O O---O --O-- O---O O---O O---O
  { 0xfe,0x90,0x90,0x98,0x66 },  // R  O---O O---O O---O O---- --O-- O---O O---O O-O-O
  { 0x64,0x92,0x92,0x92,0x4c },  // S  OOOO- O-O-O OOOO- -OOO- --O-- O---O O---O O-O-O
  { 0x80,0x80,0xfe,0x80,0x80 },  // T  O---- O--OO O--O- ----O --O-- O---O O---O O-O-O
  { 0xfc,0x02,0x02,0x02,0xfc },  // U  O---- O--O- O---O O---O --O-- O---O -O-O- O-O-O
  { 0xf8,0x04,0x02,0x04,0xf8 },  // V  O---- -OO-O O---O -OOO- --O-- -OOO- --O-- -O-O-
  { 0xfc,0x02,0x3c,0x02,0xfc },  // W  ----- ----- ----- ----- ----- ----- ----- -----
//
  { 0xc6,0x28,0x10,0x28,0xc6 },  // O  O---O O---O OOOOO -OOO- ----- -OOO- --O-- -----
  { 0xe0,0x10,0x0e,0x10,0xe0 },  // Y  O---O O---O ----O -O--- O---- ---O- -O-O- -----
  { 0x86,0x8a,0x92,0xa2,0xc2 },  // Z  -O-O- O---O ---O- -O--- -O--- ---O- O---O -----
  { 0xfe,0x82,0x82,0x00,0x00 },  // [  --O-- -O-O- --O-- -O--- --O-- ---O- ----- -----
  { 0x40,0x20,0x10,0x08,0x04 },  // \  -O-O- --O-- -O--- -O--- ---O- ---O- ----- -----
  { 0x82,0x82,0xfe,0x00,0x00 },  // ]  O---O --O-- O---- -O--- ----O ---O- ----- -----
  { 0x20,0x40,0x80,0x40,0x20 },  // ^  O---O --O-- OOOOO -OOO- ----- -OOO- ----- OOOOO
  { 0x02,0x02,0x02,0x02,0x02 },  // _  ----- ----- ----- ----- ----- ----- ----- -----
//
  { 0xc0,0xe0,0x00,0x00,0x00 },  // `  -OO-- ----- O---- ----- ----O ----- --OOO -----
  { 0x04,0x2a,0x2a,0x2a,0x1e },  // a  -OO-- ----- O---- ----- ----O ----- -O--- -----
  { 0xfe,0x22,0x22,0x22,0x1c },  // b  --O-- -OOO- OOOO- -OOO- -OOOO -OOO- -O--- -OOOO
  { 0x1c,0x22,0x22,0x22,0x14 },  // c  ----- ----O O---O O---O O---O O---O OOOO- O---O
  { 0x1c,0x22,0x22,0x22,0xfc },  // d  ----- -OOOO O---O O---- O---O OOOO- -O--- O---O
  { 0x1c,0x2a,0x2a,0x2a,0x10 },  // e  ----- O---O O---O O---O O---O O---- -O--- -OOOO
  { 0x10,0x7e,0x90,0x90,0x80 },  // f  ----- -OOOO OOOO- -OOO- -OOOO -OOO- -O--- ----O
  { 0x18,0x25,0x25,0x25,0x3e },  // g  ----- ----- ----- ----- ----- ----- ----- -OOO-
//
  { 0xfe,0x10,0x10,0x10,0x0e },  // h  O---- -O--- ----O O---- O---- ----- ----- -----
  { 0xbe,0x02,0x00,0x00,0x00 },  // i  O---- ----- ----- O---- O---- ----- ----- -----
  { 0x02,0x01,0x01,0x21,0xbe },  // j  O---- -O--- ---OO O--O- O---- OO-O- OOOO- -OOO-
  { 0xfe,0x08,0x14,0x22,0x00 },  // k  OOOO- -O--- ----O O-O-- O---- O-O-O O---O O---O
  { 0xfe,0x02,0x00,0x00,0x00 },  // l  O---O -O--- ----O OO--- O---- O-O-O O---O O---O
  { 0x3e,0x20,0x18,0x20,0x1e },  // m  O---O -O--- ----O O-O-- O---- O---O O---O O---O
  { 0x3e,0x20,0x20,0x20,0x1e },  // n  O---O -OO-- O---O O--O- OO--- O---O O---O -OOO-
  { 0x1c,0x22,0x22,0x22,0x1c },  // o  ----- ----- -OOO- ----- ----- ----- ----- -----
//
  { 0x3f,0x22,0x22,0x22,0x1c },  // p  ----- ----- ----- ----- ----- ----- ----- -----
  { 0x1c,0x22,0x22,0x22,0x3f },  // q  ----- ----- ----- ----- -O--- ----- ----- -----
  { 0x22,0x1e,0x22,0x20,0x10 },  // r  OOOO- -OOOO O-OO- -OOO- OOOO- O--O- O---O O---O
  { 0x12,0x2a,0x2a,0x2a,0x04 },  // s  O---O O---O -O--O O---- -O--- O--O- O---O O---O
  { 0x20,0x7c,0x22,0x22,0x04 },  // t  O---O O---O -O--- -OOO- -O--- O--O- O---O O-O-O
  { 0x3c,0x02,0x04,0x3e,0x00 },  // u  O---O O---O -O--- ----O -O--O O-OO- -O-O- OOOOO
  { 0x38,0x04,0x02,0x04,0x38 },  // v  OOOO- -OOOO OOO-- OOOO- --OO- -O-O- --O-- -O-O-
  { 0x3c,0x06,0x0c,0x06,0x3c },  // w  O---- ----O ----- ----- ----- ----- ----- -----
//
  { 0x22,0x14,0x08,0x14,0x22 },  // x  ----- ----- ----- ---OO --O-- OO--- -O-O- -OO--
  { 0x39,0x05,0x06,0x3c,0x00 },  // y  ----- ----- ----- --O-- --O-- --O-- O-O-- O--O-
  { 0x26,0x2a,0x2a,0x32,0x00 },  // z  O---O O--O- OOOO- --O-- --O-- --O-- ----- O--O-
  { 0x10,0x7c,0x82,0x82,0x00 },  // {  -O-O- O--O- ---O- -OO-- ----- --OO- ----- -OO--
  { 0xee,0x00,0x00,0x00,0x00 },  // |  --O-- O--O- -OO-- --O-- --O-- --O-- ----- -----
  { 0x82,0x82,0x7c,0x10,0x00 },  // }  -O-O- -OOO- O---- --O-- --O-- --O-- ----- -----
  { 0x40,0x80,0x40,0x80,0x00 },  // ~  O---O --O-- OOOO- ---OO --O-- OO--- ----- -----
  { 0x60,0x90,0x90,0x60,0x00 }   // _  ----- OO--- ----- ----- ----- ----- ----- -----
//{ 0x02,0x06,0x0a,0x06,0x02 }   // _
};


//******************************************************************************
//	set lcd cursor position
//
//	Description: set the position at which the next character will be printed.
//
uint8 lcd_cursor(int16 x, int16 y)
{
	lcd_x = ((x >= 0) && (x < HD_X_MAX)) ? x : HD_X_MAX-1;
	lcd_y = ((y >= 0) && (y < HD_Y_MAX)) ? y : HD_Y_MAX-1;
	return 0;
} // end lcd_cursor


//******************************************************************************
//	write 8-bit data to LCD
//
//	lcd_x = column
//	lcd_y = lower left-hand corner
//
void lcd_WD(uint8 datum)
{
	int i = 0x80;
	uint8 y = lcd_y + CHAR_SIZE;

	if (lcd_dmode & LCD_REVERSE_FONT) datum = ~datum;
	do
	{
		--y;							// move down dolumn
		if (i & datum)
		{
			lcd_point(lcd_x, y, 1);		// always turn point on
		}
		else
		{
			if (!(lcd_dmode & LCD_OR_CHAR))
				lcd_point(lcd_x, y, 0);	// turn off pixel
		}
	} while (i >>= 1);
	if (++lcd_x >= HD_X_MAX) lcd_x = 0;	// handle wrap-around
	return;
} // end lcd_WD


void lcd_WD2(uint16 datum)
{
	uint8 saved_lcd_y = lcd_y;			// save lcd_y

	lcd_y += CHAR_SIZE;
	if (lcd_y >= HD_Y_MAX) lcd_y -= 256 - HD_Y_MAX;
	lcd_WD(datum >> 8);
	lcd_x = lcd_x ? lcd_x - 1 : HD_X_MAX;

	lcd_y = saved_lcd_y;				// restore lcd_y
	lcd_WD(datum & 0x00ff);
	return;
} // end lcd_WD2


//******************************************************************************
//	write character to LCD
//
unsigned char lcd_putchar(unsigned char c)
{
	char_cnt++;							// increment # of characters
	if (c < ' ')
	{
		switch (c)
		{
			case '\a':
			{
				lcd_dmode |= LCD_REVERSE_FONT;
				break;
			}

			case '\b':
			{
				lcd_dmode |= LCD_2X_FONT;
				break;
			}

			case '\t':
			{
				lcd_dmode |= LCD_PROPORTIONAL;
				break;
			}

			case '\n':
			{
				// decrement lcd_y by CHAR_SIZE or CHAR_SIZE*2
				lcd_y -= (lcd_dmode & LCD_2X_FONT ? 2 : 1) << 3;
				if (lcd_y >= HD_Y_MAX) lcd_y -= 256 - HD_Y_MAX;
			}

			case '\r':
			{
				lcd_x = 0;
				break;
			}

			case '\v':
			{
				lcd_dmode |= LCD_OR_CHAR;
				break;
			}

			default:
				break;
		}
	}
	else
	{
		// output character
		if (c <= '~')
		{
			uint16 i = 0;
			unsigned char cc = c - ' ';
			if (lcd_dmode & LCD_2X_FONT)	// 2x Font
			{
				do
				{
					unsigned char ccs = cs[cc][i];
					unsigned char mask1 = 0x01;
					unsigned int mask2 = 0x0003;
					unsigned int data = 0;

					do
					{	// double bits into data
						if (ccs & mask1) data |= mask2;
						mask1 <<= 1;
						mask2 <<= 2;
					} while (mask1);
					lcd_WD2(data);
					lcd_WD2(data);
					if (++i && (lcd_dmode & LCD_PROPORTIONAL) && !cs[cc][i]) break;
				} while (i < 5);
				lcd_WD2(0x0000);			// trailing space
				lcd_WD2(0x0000);
			}
			else							// 1x font
			{
				do							// 5 8-bit columns
				{
					lcd_WD(cs[cc][i]);		// output character
					if (++i && (lcd_dmode & LCD_PROPORTIONAL) && !cs[cc][i]) break;
				} while (i < 5);
				lcd_WD(0x00);				// trailing space
			}
		}
	}
	return c;
} // end my_putchar

#if LCD_PRINTF
//******************************************************************************
//	formatted output of characters and integers
//
//	format specifier: %[flags][width][.precision][length]specifier
//
//	Supported specifiers:
//		%d or %i	Signed decimal integer
//		%u			Unsigned decimal integer
//		%x or %X	Unsigned hexadecimal integer
//		%c			Character
//		%s			String
//
//	Supported sub-specifier flags/width/length:
//		0			Left-pads numbers with zeros(0) instead of spaces
//		l			Long (32-bits)
//		h			Char (8-bits)
//
//	Supported control characters:
//		'\a'		Reverse font
//		'\b'		2x Font
//		'\t'		Proportional font
//		'\n'		New Line
//		'\r'		Return
//		'\v'		OR character to display
//
//	Possible for future releases:
//		-			Left-justify within field
//		+			+ or - sign
//		sp			- sign
//		#			Minimum number of characters to be printed
//
//	Max sizes:
//		uint32	0xffffffff = 4294967296 = 32 bit unsigned max
//		uint16	0xffff = 65535 = 16 bit unsigned max
//		~ is a degree symbol
//
static int my_printfi(unsigned char (*outc)(unsigned char), char* fmt, char* arg_ptr)
{
	static const char lHexChar[] = "0123456789abcdef";
	static const char uHexChar[] = "0123456789ABCDEF";
	static const unsigned long dv[] =
		{ 1000000000, 100000000, 10000000, 1000000, 100000, 10000, 1000, 100, 10, 1, 0 };
	unsigned char c;

	char_cnt = 0;							// clear character counter
	while (c = *fmt++)						// get character from format string
	{
		if (c == '%')
		{
			int16 length = 4;				// length: 2 = char, 4 = int, 8 = long
			int16 fieldSize = 0;			// field size
//			uint8 padChar = '0';			// width: pad character
			uint8 padChar = ' ';			// width: pad character
			int8 hexCase = 0;				// hexadecimal: 0 = upper, 1 = lower
			while (1)
			{
				switch (c = *fmt++)
				{
					case'0':				// pad w/zeros modifier
						padChar = '0';
						continue;

					case'1':
					case'2':
					case'3':
					case'4':
					case'5':
					case'6':
					case'7':
					case'8':
					case'9':				// field size modifier
					{
						fieldSize = 0;
						do
						{
							fieldSize = fieldSize * 10 + c - '0';
//							fieldSize = (((fieldSize << 2) + fieldSize) << 1) + c - '0';
							c = *fmt++;
						} while (c >= '0' && c <= '9');
						--fmt;
						continue;
					}

					case 's':				// string
					{
						char* _ptr = va_arg(arg_ptr, char*);
						while (strlen(_ptr) < fieldSize--) (*outc)(' ');
						while (*_ptr) (*outc)(*_ptr++);
						break;
					}

					case 'c':				// char
						while (1 < fieldSize--) (*outc)(padChar);	// pad char??
						(*outc)(va_arg(arg_ptr, char));
						break;

					case 'd':				// 16-bit integer
					case 'i':				// 16-bit integer
					case 'u':				// 16-bit unsigned
					{
						unsigned long nn, d;					// temp variables
						unsigned long* dp = (unsigned long*)dv;	// pointer to powers of 10
						int size = 10;							// max size

						// get value as signed long (int32)
						long n = (long)(length > 4 ? va_arg(arg_ptr, long)
													: va_arg(arg_ptr, int));

						if (n == 0)
						{
							while (1 <= --fieldSize) (*outc)(padChar);
							(*outc)('0');
						}
						else
						{
							// if signed and negative, negate, reduce field, and set flag
							if (c != 'u' && n < 0) n = -n, fieldSize--, (*outc)('-');

							// reduce size by # of digits & output padding (if any)
							while ((unsigned long)n < *dp) ++dp, --size;
							while (size <= --fieldSize) (*outc)(padChar);

							nn = (unsigned long)n;				// output #
							do									// output decimal #
							{
								d = *dp++;						// get power of 10
								c = '0';						// start with '0'
								while (nn >= d) ++c, nn -= d;	// get digit
								(*outc)(c);						// output digit
							} while (!(d & 1));					// repeat until 1
						}
						break;
					}

					case 'h':				// char modifier
						length = 2;
						continue;

					case 'l':				// long modifier
						length = 8;
						continue;

					case 'x':				// lower case hexadecimal
						hexCase = 1;

					case 'X':				// upper case hexadecimal
					{
						unsigned long d = (unsigned long)(length > 4 ? va_arg(arg_ptr, long) : va_arg(arg_ptr, int));

						if (fieldSize && (length > fieldSize)) length = fieldSize;
						else

						while (length < fieldSize--) (*outc)(padChar);
						do
						{
							c = (unsigned char)(d >> (--length << 2)) & 0x0f;
							(*outc)(hexCase ? lHexChar[c] : uHexChar[c]);
						} while (length);
						break;
					}

					default:				// undefined conversion
						(*outc)(c);

					case 0:					// single % ????
						break;
				}
				break;						// finished specifier, next
			}
		}
		else (*outc)(c);					// character in format string
	}
	return 0;
} // end my_printfi


//******************************************************************************
//	formatted print to lcd
//
//	return # of characters sent to lcd_putchar
//
int lcd_printf(const char* fmt, ...)
{
	unsigned char (*outchar)(unsigned char c);
	uint8 saved_lcd_dmode = lcd_dmode;		// save font mode
	outchar = lcd_putchar;					// use putchar
	va_list arg_ptr;
	va_start(arg_ptr, fmt);					// create pointer to args
	my_printfi(outchar, (char*)fmt, arg_ptr);
	va_end(arg_ptr);						// destroy arg pointer
	lcd_dmode = saved_lcd_dmode;			// restore font mode
	return char_cnt;						// return character count
} // end lcd_printf


//******************************************************************************
//	formatted printf to buffer
//
//	return # of characters sent to buffer
//
static unsigned char outChar(unsigned char c)
{
	char_cnt++;
	*buffer_ptr++ = c;
	*buffer_ptr = 0;
	return c;
} // end outChar

int lcd_sprintf(char* buffer, const char* fmt, ...)
{
	unsigned char (*outchar)(unsigned char c);
	outchar = outChar;						// use outChar
	buffer_ptr = buffer;
	va_list arg_ptr;
	va_start(arg_ptr, fmt);					// create pointer to args
	my_printfi(outchar, (char*)fmt, arg_ptr);
	va_end(arg_ptr);						// destroy arg pointer
	return char_cnt;						// return character count
} // end lcd_sprintf
#endif

//******************************************************************************
//	override fputs and fputc in stdio library
//
int fputs(const char *_ptr, register FILE *_fp)
{
	while (*_ptr) lcd_putchar(*_ptr++);	// output string
	return 1;
} // end fputs

int fputc(int _c, register FILE *_fp)
{
	lcd_putchar(_c);					// output character
	return _c;
} // end fputc
