//******************************************************************************
//	snakelib.h  (07/13/2015)
//
//  Author:			Paul Roper, Brigham Young University
//  Revisions:		1.0		04/06/2015	Tones
//
//******************************************************************************
//
#ifndef SNAKELIB_H_
#define SNAKELIB_H_

typedef struct
{
	uint16 tone;
	uint16 duration;
} TONE;

//------------------------------------------------------------------------------
// INITIALIZE CONSTANTS FOR TONES
//
#define PITCH	12

#define TONE_C    myCLOCK/1308/12*PITCH
#define TONE_Cs   myCLOCK/1386/12*PITCH
#define TONE_D    myCLOCK/1468/12*PITCH
#define TONE_Eb   myCLOCK/1556/12*PITCH
#define TONE_E    myCLOCK/1648/12*PITCH
#define TONE_F    myCLOCK/1746/12*PITCH
#define TONE_Fs   myCLOCK/1850/12*PITCH
#define TONE_G    myCLOCK/1950/12*PITCH
#define TONE_Ab   myCLOCK/2077/12*PITCH
#define TONE_A    myCLOCK/2200/12*PITCH
#define TONE_Bb   myCLOCK/2331/12*PITCH
#define TONE_B    myCLOCK/2469/12*PITCH
#define TONE_C1   myCLOCK/2616/12*PITCH
#define TONE_Cs1  myCLOCK/2772/12*PITCH
#define TONE_D1   myCLOCK/2937/12*PITCH
#define TONE_Eb1  myCLOCK/3111/12*PITCH
#define TONE_E1   myCLOCK/3296/12*PITCH
#define TONE_F1   myCLOCK/3492/12*PITCH
#define TONE_Fs1  myCLOCK/3700/12*PITCH
#define TONE_G1   myCLOCK/3920/12*PITCH
#define TONE_Ab1  myCLOCK/4150/12*PITCH
#define TONE_A1   myCLOCK/4400/12*PITCH
#define TONE_Bb1  myCLOCK/4662/12*PITCH
#define TONE_B1   myCLOCK/4939/12*PITCH

#define TONE_Db		TONE_Cs
#define TONE_Ds		TONE_Eb
#define TONE_Gb		TONE_Fs
#define TONE_Gs		TONE_Ab
#define TONE_As		TONE_Bb
#define TONE_Bs		TONE_C1
#define TONE_Cb1	TONE_B
#define TONE_Db1	TONE_Cs1
#define TONE_Ds1	TONE_Eb1
#define TONE_Es1	TONE_F1
#define TONE_Fb1	TONE_E1
#define TONE_Gb1	TONE_Fs1
#define TONE_Gs1	TONE_Ab1
#define TONE_As1	TONE_Bb1

#define TONE_REST	0

//------------------------------------------------------------------------------
// INITIALIZE CONSTANTS FOR TONES
//
#define DO1		myCLOCK*PITCH/2616			// Middle C
#define RE		myCLOCK*PITCH/2937			// D
#define MI		myCLOCK*PITCH/3296			// E
#define FA		myCLOCK*PITCH/3492			// F
#define SOL		myCLOCK*PITCH/3920			// G
#define LA		myCLOCK*PITCH/4400			// A
#define TI		myCLOCK*PITCH/4939			// B
#define DO2		myCLOCK*PITCH/5232			// C

#define BEEP_COUNT	(400*(myCLOCK/1200000))
#define BLINK_COUNT	(3*(myCLOCK/1200000))

int timerB_init(void);
void beep(void);
void outTone(unsigned int tone, unsigned int duration);
void rasberry(void);
void doDitty(int tones, const TONE* ditty);
void imperial_march(void);
void charge(void);
void blink(void);

#endif /* SNAKELIB_H_ */
