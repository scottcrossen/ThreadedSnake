//	RBX430-1.c - RBX430 REV D board system functions
//*******************************************************************************
//*******************************************************************************
//	RRRRRR   BBBBBB   XX    XX    44    3333      0000          11
//	RR   RR  BB   BB   XX  XX    444   33  33    00  00        111
//	RR   RR  BB   BB    XXXX    4444        33  00    00        11          ccccc
//	RRRRRR   BBBBBB      XX    44 44     3333   00    00  XXXX  11         cc   cc
//	RR RR    BB   BB    XXXX   444444       33  00    00  XXXX  11         cc
//	RR  RR   BB   BB   XX  XX     44   33  33    00  00         11   ooo   cc   cc
//	RR   RR  BBBBBB   XX    XX    44    3333      0000         1111  ooo    ccccc
//
//	Author:			Paul Roper, Brigham Young University
//	Revision:		1.0		02/15/2012
//					1.1		11/25/2012	ERROR2 blinks error #
//					1.2		03/22/2013	Removed USCI isr vector
//					1.3		06/05/2015	New ERROR, ERROR2 protocols
//
//	Description:	Initialization firmware for RBX430-1 Rev D Development Board
//
//	Built with CCSv5.1 w/cgt 3.0.0
//*******************************************************************************
//
//	                          MSP430F2274
//                  .-----------------------------.
//            SW1-->|P1.0^                    P2.0|<->LCD_DB0
//            SW2-->|P1.1^                    P2.1|<->LCD_DB1
//            SW3-->|P1.2^                    P2.2|<->LCD_DB2
//            SW4-->|P1.3^                    P2.3|<->LCD_DB3
//       ADXL_INT-->|P1.4                     P2.4|<->LCD_DB4
//        AUX INT-->|P1.5                     P2.5|<->LCD_DB5
//        SERVO_1<--|P1.6 (TA1)               P2.6|<->LCD_DB6
//        SERVO_2<--|P1.7 (TA2)               P2.7|<->LCD_DB7
//                  |                             |
//         LCD_A0<--|P3.0                     P4.0|-->LED_1 (Green)
//        i2c_SDA<->|P3.1 (UCB0SDA)     (TB1) P4.1|-->LED_2 (Orange) / SERVO_3
//        i2c_SCL<--|P3.2 (UCB0SCL)     (TB2) P4.2|-->LED_3 (Yellow) / SERVO_4
//         LCD_RW<--|P3.3                     P4.3|-->LED_4 (Red)
//   TX/LED_5 (G)<--|P3.4 (UCA0TXD)     (TB1) P4.4|-->LCD_BL
//             RX-->|P3.5 (UCA0RXD)     (TB2) P4.5|-->SPEAKER
//           RPOT-->|P3.6 (A6)          (A15) P4.6|-->LED 6 (R)
//           LPOT-->|P3.7 (A7)                P4.7|-->LCD_E
//                  '-----------------------------'
//
//******************************************************************************
//******************************************************************************
#include <setjmp.h>
#include "msp430x22x4.h"
#include "RBX430-1.h"

uint16 i2c_fSCL;				// i2c timing constant

//******************************************************************************
//	Initialization sequence for eZ430X MSP430F2274
//
#define BINARY(a,b,c,d,e,f,g,h)	((((((((a<<1)+b<<1)+c<<1)+d<<1)+e<<1)+f<<1)+g<<1)+h)

uint8 RBX430_init(enum _430clock clock)
{
	WDTCTL = WDTPW | WDTHOLD;				// Stop WDT

	// 	MSP430 Clock - Set DCO to 1-16 MHz:
	switch (clock)
	{
		case _1MHZ:
			BCSCTL1 = CALBC1_1MHZ;			// Set range 1MHz
			DCOCTL = CALDCO_1MHZ;			// Set DCO step + modulation
			i2c_fSCL = (1200/I2C_FSCL);		// fSCL
			break;

		case _8MHZ:
			BCSCTL1 = CALBC1_8MHZ;			// Set range 8MHz
			DCOCTL = CALDCO_8MHZ;			// Set DCO step + modulation
			i2c_fSCL = (8000/I2C_FSCL);		// fSCL
			break;

		case _12MHZ:
			BCSCTL1 = CALBC1_12MHZ;			// Set range 12MHz
			DCOCTL = CALDCO_12MHZ;			// Set DCO step + modulation
			i2c_fSCL = (12000/I2C_FSCL);	// fSCL
			break;

		case _16MHZ:
			BCSCTL1 = CALBC1_16MHZ;			// Set range 16MHz
			DCOCTL = CALDCO_16MHZ;			// Set DCO step + modulation
			i2c_fSCL = (16000/I2C_FSCL);	// fSCL
			break;

		default:
			ERROR2(_SYS, _ERR_430init);		// hard failure!
	}
	BCSCTL3 = LFXT1S_2;						// Select ACLK from VLO (no crystal)

	// configure P1
	P1SEL = 0x00;				// select GPIO
	P1OUT = 0x0f;				// turn off all output pins
	P1REN = 0x0f;				// pull-up P1.0-3
	P1DIR = 0xc0;				// P1.0-5 input, P1.6-7 output

	// configure P2
	P2SEL = 0x00;				// GPIO
	P2OUT = 0x00;				// turn off all output pins
	P2REN = 0x00;				// no pull-ups
	P2DIR = 0xff;				// P2.0-7 output

	// configure P3
	P3SEL = 0x00;				// GPIO
	P3OUT = 0x04;				// turn off all output pins (set SDA/SCL high)
	P3REN = 0x00;				// no pull-ups
	P3DIR = 0x1d;				// P3.0,2-4 output, P3.1,5-7 input

	// configure P4
	P4SEL = 0x00;				// select GPIO
	P4OUT = 0x00;				// turn off all output pins
	P4REN = 0x00;				// no pull-ups
	P4DIR = 0xff;				// P4.0-7 output

	return 0;					// success
} // end RBX430_init

#if 0
//******************************************************************************
//	report hard error
//
void ERROR2(int16 error)
{
	int i, j;

	if (error == 0) return;			// return if no error

	__bic_SR_register(GIE);			// disable interrupts
	RBX430_init(_1MHZ);				// system reset @1 MHz

	while (1)
	{
		BACKLIGHT_OFF;
		for (i = 5; i > 0; --i) for (j = -1; j; --j) LED_RED_OFF;

		// now blink error #
		for (i = error; i > 0; --i)
		{
			BACKLIGHT_ON;
			for (j = 0x7fff; j; --j) LED_RED_ON;
			BACKLIGHT_OFF;
			for (j = 0x7fff; j; --j) LED_RED_OFF;
		}
	}
} // end ERROR2
#endif


//******************************************************************************
 //	report hard error
 //
 void ERROR(int16 error)
 {
 	volatile int i, j;

 	// return if no error
 	if (error == 0) return;

 	__bic_SR_register(GIE);			// disable interrupts
	RBX430_init(_1MHZ);				// system reset @1 MHz

 	while (1)
 	{
 		// pause
 		LED_RED_OFF;
 		for (i = 4; i > 0; --i) for (j = -1; j; --j);

 		// flash LED's 10 times
 		i = 10;
 		while (i--)
 		{
 			LED_RED_TOGGLE;
 			for (j = 8000; j > 0; --j);
 		}

 		// pause
 		LED_RED_OFF;
 		for (i=1; i > 0; --i) for (j = -1; j; --j);
 		for (i=1; i > 0; --i) for (j = -1; j; --j);

 		// now blink error #
 		for (i = error; i > 0; --i)
 		{
 			LED_RED_ON;
 			for (j = -1; j; --j);
 			LED_RED_OFF;
 			for (j = -1; j; --j);
 		}
 	}
 } // end ERROR


 void ERROR2(int class, int16 error)
 {
 	volatile int i, j;

 	// return if no error
 	if (error == 0) return;

	_disable_interrupts();
	RBX430_init(_1MHZ);				// system reset @1 MHz

 	while (1)
 	{
 		// pause
 		LED_RED_OFF;
 		for (i = 4; i > 0; --i) for (j = -1; j; --j);

 		// flash LED's 10 times
 		i = 10;
 		while (i--)
 		{
 			LED_RED_TOGGLE;
 			for (j = 8000; j > 0; --j);
 		}

 		// pause
 		LED_RED_OFF;
 		for (i=1; i > 0; --i) for (j = -1; j; --j);

 		// now blink class
 		for (i = class; i > 0; --i)
 		{
 			LED_RED_ON;
 			for (j = -1; j; --j);
 			LED_RED_OFF;
 			for (j = -1; j; --j);
 		}

 		// pause again
 		LED_RED_OFF;
 		for (i=1; i > 0; --i) for (j = -1; j; --j);
 		for (i=1; i > 0; --i) for (j = -1; j; --j);

 		// now blink error #
 		for (i = error; i > 0; --i)
 		{
 			LED_RED_ON;
 			for (j = -1; j; --j);
 			LED_RED_OFF;
 			for (j = -1; j; --j);
 		}
 	}
 } // end ERROR2


//******************************************************************************
//	initialize A/D converter
//
uint8 ADC_init(void)
{
//	ADC10CTL0 = SREF0 | ADC10SHT_2 | ADC10ON | ADC10IE;
//	ADC10AE0 = 0x00;					// disable A0-A7
//	ADC10AE1 = 0xb0;					// enable A15, A13, A12
	return 0;
} // end ADC_init


//****************************************************************
//	read A/D converted value
//
//		SREF0		VR+ = VREF+ and VR- = VSS
//		ADC10SHT_2	16 x ADC10CLKs
//		ADC10ON		ADC10 on
//		REFON		Reference on
//		REF2_5V		2.5v internal reference
//
//		channel 6  = right potentiometer
//		channel 7  = left potentiometer
//		channel 10 = internal temperature
//		channel 15 = red LED
//
uint16 ADC_read(uint8 channel)
{
	int result, timeout;

	ADC10CTL0 = SREF0 | ADC10SHT_2 | ADC10ON | REFON | REF2_5V;
	switch (channel)
	{
		case RIGHT_POT:
		{
			// P3.6 -> Right Potentiometer
			P3DIR &= ~0x40;					// A6 = P3.6
			P3SEL |= 0x40;
			ADC10AE0 = 0x40;
			ADC10AE1 = 0x00;				// P3.6 ADC10 function and enable
			break;
		}

		case LEFT_POT:
		{
			// P3.7 -> Left Potentiometer
			P3DIR &= ~0x80;					// A7 - P3.7
			P3SEL |= 0x80;
			ADC10AE0 = 0x80;
			ADC10AE1 = 0x00;				// P3.7 ADC10 function and enable
			break;
		}

		case RED_LED:
		{
			// P4.6 -> Red LED
			P4DIR &= ~0x40;					// A15 = P4.6
			P4SEL |= 0x40;
			ADC10AE0 = 0x00;
			ADC10AE1 = 0x80;				// P4.6 ADC10 function and enable
			break;
		}

		case MSP430_TEMPERATURE:
		{
			// internal temperature, delay 30 us to allow Ref to settle
			for (timeout = 30*8; timeout;) --timeout;
			break;
		}

		case MSP430_VOLTAGE:
		{
			// (Vcc - Vss) / 2
			break;
		}
	}

	ADC10CTL1 = channel << 12;
	ADC10CTL0 |= ENC | ADC10SC;			// Sampling and conversion start
	timeout = 1;
//	while (!(ADC10CTL0 & ADC10IFG) && ++timeout);
	while (++timeout) if (ADC10CTL0 & ADC10IFG) break;
	if (timeout == 0) ERROR2(_SYS, _ERR_ADC_TO);
	result = ADC10MEM;
	if (result < 0) result = 0;

	ADC10AE0 = 0;
	ADC10AE1 = 0;
	P4DIR |= 0x40;						// turn P4.6 to output
	P4SEL &= ~0x40;
	return result;
} // end ADC_sample


//******************************************************************************
//******************************************************************************
// ADC10 interrupt service routine
//
#pragma vector = ADC10_VECTOR
__interrupt void ADC10_ISR(void)
{
  __bic_SR_register_on_exit(CPUOFF);	// Clear CPUOFF bit from 0(SR)
  return;
} // end ADC10_ISR


//------------------------------------------------------------------------------
//--UNITIALIZED MSP430 INTERRUPT VECTORS----------------------------------------

#pragma vector=unused_interrupts
__interrupt void ISR_trap(void)
{
	volatile int* current_SP = (void*) _get_SP_register();
	volatile int PC = *(current_SP + 2);

	ERROR2(_SYS, _ERR_ISR);				// unrecognized interrupt
} // end ISR_trap
