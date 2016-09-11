//******************************************************************************
//	snake_interrupts.c  (07/13/2015)
//
//  Author:			Paul Roper, Brigham Young University
//  Revisions:		1.0		11/25/2012	RBX430-1
//					1.1		05/06/2015	Unused interrupt vectors
//
//******************************************************************************
//
#include "msp430.h"
#include <stdlib.h>
#include "RBX430-1.h"
#include "RBX430_lcd.h"
#include "snake.h"
#include "pthreads.h"

volatile uint16 WDT_cps_cnt;			// WDT count per second
volatile uint16 WDT_move_cnt;			// counter to move snake event
volatile uint16 WDT_debounce_cnt;		// switch debounce counter
volatile uint16 backlight_cnt;			// LCD backlight
volatile uint32 WDT_delay;				// WDT delay counter
volatile uint16 TB0_tone_on;			// tone WDT count

extern volatile uint16 sys_event;		// pending events

extern volatile uint16 move_cnt;		// snake speed
extern volatile enum MODE game_mode;	// 0=idle, 1=play, 2=next
extern volatile uint8 level;			// current level (1-4)
extern volatile uint16 seconds;
extern sem_t WDT_move;
extern sem_t LCD_update;

//------------------------------------------------------------------------------
//-- watchdogtimer_init --------------------------------------------------------
//
int watchdog_init(uint16 wd_ctl)
{
	WDTCTL = wd_ctl;					// set Watchdog interval
	IE1 |= WDTIE;						// enable WDT interrupt
	WDT_cps_cnt = WDT_CPS;				// set WD 1 second counter
	WDT_move_cnt = WDT_MOVE1;			// move update rate
	WDT_debounce_cnt = 0;				// switch debounce
	WDT_delay = 0;						// init time counter
	return 0;
} // end watchdogtimer_init


//------------------------------------------------------------------------------
//-- port1_init ----------------------------------------------------------------
//
int port1_init(void)
{
	// configure P1 switches and ADXL345 INT1 for interrupt
	P1SEL &= ~0x0f;						// select GPIO
	P1DIR &= ~0x0f;						// Configure P1.0-3 as Inputs
	P1OUT |= 0x0f;						// use pull-ups
	P1IES |= 0x0f;						// high to low transition
	P1REN |= 0x0f;						// Enable pull-ups
	P1IE |= 0x0f;						// P1.0-3 interrupt enabled
	P1IFG &= ~0x0f;						// P1.0-3 IFG cleared
	return 0;
} // end port1_init


//------------------------------------------------------------------------------
//-- Port 1 ISR ----------------------------------------------------------------
//
#pragma vector=PORT1_VECTOR
__interrupt void Port_1_ISR(void)
{
	if (P1IFG & 0x0f)
	{
		P1IFG &= ~0x0f;						// P1.0-3 IFG cleared
		WDT_debounce_cnt = DEBOUNCE_CNT;	// enable debounce
	}
	return;
} // end Port_1_ISR


//------------------------------------------------------------------------------
//-- Watchdog Timer ISR --------------------------------------------------------
//
#pragma vector = WDT_VECTOR
__interrupt void WDT_ISR(void)
{
	// 1 second counter
	if (--WDT_cps_cnt == 0)
	{
		LED_GREEN_TOGGLE;				// toggle GREEN led
		WDT_cps_cnt = WDT_CPS;
		seconds++;						// seconds counter
		//sys_event |= LCD_UPDATE;		// update LCD event
		sem_signal(&LCD_update);
		__bic_SR_register_on_exit(LPM0_bits);
	}

	// decrement tone counter - turn off tone when 0
	if (TB0_tone_on && (--TB0_tone_on == 0))
	{
		TBCCR0 = 0;						// turn tone off
	}

	// turn backlight off when backlight_cnt is non-zero
	if (backlight_cnt)
	{
		lcd_backlight(0);				// turn off backlight
		if (--backlight_cnt == 0) lcd_backlight(1);
	}

	if (WDT_delay && (--WDT_delay == 0));

	// update move counter
	if ((game_mode == PLAY) && (--WDT_move_cnt == 0))
	{
		WDT_move_cnt = move_cnt;
		//sys_event |= MOVE_SNAKE;		// MOVE SNAKE event
		sem_signal(&WDT_move);
		__bic_SR_register_on_exit(LPM0_bits);
	}

	// check for switch debounce
	if (WDT_debounce_cnt && (--WDT_debounce_cnt == 0))
	{
		sys_event |= (P1IN ^ 0x0f) & 0x0f;
	}

	// exit low power mode if pending event
	if (sys_event) __bic_SR_register_on_exit(LPM0_bits);
	return;
} // end WDT_ISR
