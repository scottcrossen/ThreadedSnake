//******************************************************************************
//  Lab 09b - Snake (08/07/2015)
//
//  Description:
//
//	"Write a snake game program in C that scores points by eating randomly
//	placed food on the display. There are four levels of play, each with
//	increasing difficulty. The game ends when the snake runs into an obstacle,
//	itself, or time expires. The direction of the snake’s head is turned to
//	horizontal and vertical paths using the push buttons and the body of the
//	snake follows in the head's path. The snake is always moving and the
//	snake moves faster at each level. The tail grows by one segment every
//	time the snake eats a food."
//
//
//  Author:			Paul Roper, Brigham Young University
//  Revisions:		1.0		11/25/2012		RBX430-1
//					1.1		04/12/2013		START_LEVEL
//					1.2		04/06/2015		functions moved to snakelib
//					1.3		08/07/2015		else if (sys_event)
//
//  Built with Code Composer Studio Version: 5.2.0.00090
//******************************************************************************
//******************************************************************************
//
#include "msp430.h"
#include <stdlib.h>
#include "RBX430-1.h"
#include "RBX430_lcd.h"
#include "snake.h"
#include "snakelib.h"
#include "pthreads.h"

volatile uint16 sys_event;					// pending events
extern volatile enum MODE game_mode;		// 0=idle, 1=play, 2=next
//extern volatile uint16 move_cnt;					// snake speed
sem_t WDT_move;					// sem signal
sem_t WDT_move_exit;					// sem signal
sem_t LCD_update;					// sem signal
pthread_mutex_t LCD_mutex;

//------------------------------------------------------------------------------
//-- main ----------------------------------------------------------------------
//
void main(void)
{
	ERROR2(_SYS, RBX430_init(CLOCK));		// init RBX430-1 board
	ERROR2(_SYS, lcd_init());				// init LCD & interrupts
	ERROR2(_SYS, port1_init());				// init port 1 (switches)
	ERROR2(_SYS, timerB_init());			// init timer B (sound)
	ERROR2(_SYS, watchdog_init(WDT_CTL));	// init watchdog timer
	game_mode = SETUP;						// idle mode
	sys_event = NEW_GAME;					// new game (in idle mode)
	pthread_init(NULL);
	_enable_interrupt();
	semaphore_create(&WDT_move, NULL);
	semaphore_create(&WDT_move_exit, NULL);
	semaphore_create(&LCD_update, NULL);
	sem_trywait(&WDT_move_exit);
	pthread_mutex_init(&LCD_mutex, NULL);
	pthread_create(NULL, NULL, MOVE_SNAKE_event, NULL);
	pthread_create(NULL, NULL, LCD_UPDATE_event, NULL);

	//--------------------------------------------------------------------------
	//	event service routine loop
	//
	while (1)
	{
		// disable interrupts before check sys_event
		_disable_interrupts();

		// if there's something pending, enable interrupts before servicing
		if (sys_event) _enable_interrupt();

		// otherwise, enable interrupts and goto sleep (LPM0)
		else __bis_SR_register(LPM0_bits + GIE);

		//----------------------------------------------------------------------
		//	I'm AWAKE!!!  There must be something to do
		//----------------------------------------------------------------------

		if (sys_event & MOVE_SNAKE)			// move snake event
		{
			sys_event &= ~MOVE_SNAKE;		// clear move event
			if (game_mode == PLAY) sem_signal(&WDT_move);
			//MOVE_SNAKE_event();				// move snake

		}
		else if (sys_event & SWITCH_1)		// switch #1 event
		{
			sys_event &= ~SWITCH_1;			// clear switch #1 event
			SWITCH_1_event();				// process switch #1 event
		}
		else if (sys_event & SWITCH_2)		// switch #2 event
		{
			sys_event &= ~SWITCH_2;			// clear switch #2 event
			SWITCH_2_event();				// process switch #2 event
		}
		else if (sys_event & SWITCH_3)		// switch #3 event
		{
			sys_event &= ~SWITCH_3;			// clear switch #3 event
			SWITCH_3_event();				// process switch #3 event
		}
		else if (sys_event & SWITCH_4)		// switch #4 event
		{
			sys_event &= ~SWITCH_4;			// clear switch #4 event
			SWITCH_4_event();				// process switch #4 event
		}
		else if (sys_event & START_LEVEL)	// start level event
		{
			sys_event &= ~START_LEVEL;		// clear start level event
			START_LEVEL_event();			// start game
		}
		else if (sys_event & LCD_UPDATE)	// LCD event
		{
			sys_event &= ~LCD_UPDATE;		// clear LCD event
			sem_signal(&WDT_move);
			//LCD_UPDATE_event();				// update LCD
		}
		else if (sys_event & END_GAME)		// end game event
		{
			sys_event &= ~END_GAME;			// clear end game event
			END_GAME_event();				// end game
		}
		else if (sys_event & NEW_GAME)		// new game event
		{
			sys_event &= ~NEW_GAME;			// clear new game event
			NEW_GAME_event();				// new game
		}
		else if (sys_event & NEXT_LEVEL)	// next level event
		{
			sys_event &= ~NEXT_LEVEL;		// clear next level event
			NEXT_LEVEL_event();				// next level
		}
		else if (sys_event)					// ????
		{
			ERROR2(_USER, _ERR_EVENT);		// unrecognized event
		}
	}
} // end main
