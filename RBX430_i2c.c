//	RBX430_i2c.c - RBX430-1 REV D board system functions
//******************************************************************************
//******************************************************************************
//
//	Author:			Paul Roper, Brigham Young University
//	Revision:		1.0		02/01/2012
//
//	Built with CCSv5.1 w/cgt 3.0.0
//*******************************************************************************
//
//	                         MSP430F2274
//	                .-----------------------------.
//	          SW1-->|P1.0^                    P2.0|<->LCD_DB0
//	          SW2-->|P1.1^                    P2.1|<->LCD_DB1
//	          SW3-->|P1.2^                    P2.2|<->LCD_DB2
//	          SW4-->|P1.3^                    P2.3|<->LCD_DB3
//	     ADXL_INT-->|P1.4                     P2.4|<->LCD_DB4
//	      AUX INT<--|P1.5                     P2.5|<->LCD_DB5
//	      SERVO_1<--|P1.6 (TA1)               P2.6|<->LCD_DB6
//	      SERVO_2<--|P1.7 (TA2)               P2.7|<->LCD_DB7
//	                |                             |
//	       LCD_A0<--|P3.0                     P4.0|-->LED_1 (Green)
//	      i2c_SDA<->|P3.1 (UCB0SDA)     (TB1) P4.1|-->LED_2 (Orange) / SERVO_3
//	      i2c_SCL<--|P3.2 (UCB0SCL)     (TB2) P4.2|-->LED_3 (Yellow) / SERVO_4
//	       LCD_RW<--|P3.3                     P4.3|-->LED_4 (Red)
//	 TX/LED_5 (G)<--|P3.4 (UCA0TXD)     (TB1) P4.4|-->LCD_BL
//	           RX-->|P3.5 (UCA0RXD)     (TB2) P4.5|-->SPEAKER
//	         RPOT-->|P3.6 (A6)          (A15) P4.6|-->LED 6 (R)
//	         LPOT-->|P3.7 (A7)                P4.7|-->LCD_E
//	                '-----------------------------'
//
//******************************************************************************
//******************************************************************************

#include <setjmp.h>
#include "msp430x22x4.h"
#include "RBX430-1.h"
#include "RBX430_i2c.h"
#include "RBX430_lcd.h"

void i2c_clocklow(void);
void i2c_clockhigh(void);
void i2c_out_bit(uint8 bit);
void i2c_start_address(uint16 address, uint8 rwFlag);
void i2c_out_stop(void);

//******************************************************************************
//******************************************************************************
//	LCD Global variables (NOT ZERO'D!!!)
//
jmp_buf i2c_context;							// error context
volatile uint16 i2c_delay;
volatile uint16 FRAM_adr;
volatile uint8 FRAM_data;
volatile uint8 FRAM_mask;


extern volatile uint16 i2c_fSCL;				// i2c timing constant

//******************************************************************************

#define I2C_CLOCK_LOW		P3OUT &= ~SCL		// put clock low
#define I2C_CLOCK_HIGH		P3OUT |= SCL		// put clock high

#define I2C_DATA_LOW		P3DIR |= SDA		// put data low
#define I2C_DATA_HIGH		P3DIR &= ~SDA		// put data high (pull-up)

#define I2C_DATA_READ		P3DIR &= ~SDA		// put data input

//******************************************************************************
#define I2C_DELAY	0

// 1.2 MHz		i2c_fSCL = (1200/I2C_FSCL) = 12 / 30 = 0
// 8 MHz		i2c_fSCL = (8000/I2C_FSCL) = 80 / 30 = 2
// 12 MHz		i2c_fSCL = (12000/I2C_FSCL) = 120 / 30 = 4
// 16 MHz		i2c_fSCL = (16000/I2C_FSCL) = 160 / 30 = 5

//******************************************************************************
//	Init Universal Synchronous Controller
//
uint8 i2c_init()
{
	int i;

//	i2c_delay = i2c_fSCL / 30;
	i2c_delay = I2C_DELAY;

	P3SEL &= ~(SDA | SCL);			// select GPIO

	P3OUT &= ~SDA;					// setup SDA for low
	P3DIR &= ~SDA;					// set SDA as input (high)

	P3DIR |= SCL;					// set SCL as output
	P3OUT |= SCL;					// set SCL high

	// output 9 clocks with SDA high
	for (i = 9; i > 0; --i)
	{
		i2c_clocklow();				// clock SCL
		i2c_clockhigh();
	}

	// send stop condition
	i2c_out_stop();
	FRAM_adr = 0xffff;				// reset FRAM cache
	return 0;
} // init_i2c


//******************************************************************************
//
void i2c_clocklow()
{
	volatile int delay = i2c_delay;
	I2C_CLOCK_LOW;					// put clock low
	while (delay--);				// delay
	return;
} // end clocklow

void i2c_clockhigh()
{
	volatile int delay = i2c_delay;
	I2C_CLOCK_HIGH;					// put clock high
	while (delay--);				// delay
	return;
} // end clockhigh


//******************************************************************************
//
//	.         .__delay__.
//	|         |
//	|__delay__|
//	 ^
//	 ^--> 0/1 SDA
//
void i2c_out_bit(uint8 bit)
{
	i2c_clocklow();					// drop clock
	if (bit) I2C_DATA_HIGH;			// set SDA high
	else I2C_DATA_LOW;				// or SDA low
	i2c_clockhigh();				// raise clock
	return;
} // end i2c_out_bit


//	.         .__delay__.      .          .__delay__.
//	|         |            x8  |          |^
//	|__delay__|                |__delay __|^
//	 ^                          ^          ^--> check for ack
//	 ^                          ^
//	 ^--> 0/1 SDA               ^--> 0/1 SDA
//
//	exit high impedance
//
int i2c_out8bits(uint8 c)
{
	uint8 shift = 0x80;
	volatile int delay;

	// output 8 bits during SDA low
	while (shift)
	{
		i2c_out_bit(c & shift);
		shift >>= 1;				// adjust mask
	}
	// look for slave ack, if not low, then error

	//	i2c_clocklow();
	I2C_CLOCK_LOW;					// put clock low
	I2C_DATA_READ;					// turn SDA to input (high impedance)
	delay = i2c_delay;
	while (delay--);				// delay

	i2c_clockhigh();				// put clock high
	return 0;
} // end out8bits


//******************************************************************************
//
//	exit w/high impedance SDA
//
void i2c_start_address(uint16 address, uint8 rwFlag)
{
	volatile uint16 delay = i2c_delay;
	int error;

	// output start
	I2C_CLOCK_HIGH;					// w/SCL & SDA high
	I2C_DATA_HIGH;

	I2C_DATA_LOW;					// output start (SDA high to low while SCL high)
	while (delay--);				// delay

	// output (address * 2 + read/write bit)
	if (error = i2c_out8bits((address << 1) + rwFlag))
	{
		i2c_out_stop();					// output stop 1st
		longjmp(i2c_context, error);	//return error;
	}
	return;
} // end i2c_out_address


//******************************************************************************
//
void i2c_out_stop()
{
	volatile uint16 delay = i2c_delay;

	i2c_clocklow();					// put clock low
	I2C_DATA_LOW;					// make sure SDA is low
	i2c_clockhigh();				// clock high
	I2C_DATA_HIGH;					// stop = low to high
	while (delay--);
	return;
} // end i2c_out_stop


//******************************************************************************
//
uint8 i2c_write(uint16 address, uint8* data, int16 bytes)
{
	int error;

	i2c_start_address(address, 0);	// output write address
	while (bytes--)					// write 8 bits
	{
		if (error = i2c_out8bits(*data++)) longjmp(i2c_context, error);	//return error;
	}
	i2c_out_stop();					// output stop
	return 0;						// return success
} // end i2c_write


//******************************************************************************
//	read bytes into buffer using i2c
//
//	IN:		address	i2c address
//			buffer	pointer to input buffer
//			bytes	# of bytes to read
//	OUT:	last byte read
//
uint8 i2c_read(uint16 address, uint8* buffer, int16 bytes)
{
	uint16 i, data;

	i2c_start_address(address, 1);	// output read address

	while (bytes--)					// read 8 bits
	{
		for (i = 8; i > 0; --i)
		{
			i2c_clocklow();			// I2C_CLOCK_LOW;
			I2C_DATA_READ;			// high impedance
			i2c_clockhigh();		// I2C_CLOCK_HIGH;
			data <<= 1;				// assume 0
			if (P3IN & SDA) data++;
		}
		// save data
		*buffer++ = data;

		// output ack or nack
		i2c_clocklow();				// I2C_CLOCK_LOW;
		if (bytes) I2C_DATA_LOW;	// ack (0)
		else I2C_DATA_HIGH;			// nack (1)
		i2c_clockhigh();			// I2C_CLOCK_HIGH;
	}
	i2c_out_stop();					// output stop
	return data;
} // end i2c_read


//******************************************************************************
//******************************************************************************
//	initialize TMP102
//
uint8 TMP102_init()
{
	int error = 0;
	if (error = setjmp(i2c_context)) return error;

	i2c_write(TMP102_ADR, (uint8 *)&error, 1);	// write/read TMP102 register 0
	i2c_read(TMP102_ADR, (uint8 *)&error, 2);
	return 0;
} // end TMP102_init


//******************************************************************************
//	read from TMP102
//
uint8 TMP102_read(uint8 address)
{
	uint8 datum;
	int error;
	if (error = setjmp(i2c_context)) return error;

	i2c_write(TMP102_ADR, (uint8 *)&address, 1);
	i2c_read(TMP102_ADR, &datum, 1);
	return datum;
} // end TMP102_read


//******************************************************************************
//	read word from TMP102
//
uint16 TMP102_read_word(uint8 address)
{
	uint16 datum;
	int error;
	if (error = setjmp(i2c_context)) return error;

	i2c_write(TMP102_ADR, (uint8 *)&address, 1);
	i2c_read(TMP102_ADR, (uint8 *)&datum, 2);
	return (datum << 8) + (datum >> 8);		// make little endian
} // end TMP102_read_word


//******************************************************************************
//
uint8 TMP102_write(uint8 address, uint8 datum)
{
	int error;
	uint8 TXData[2];					// i2c tx buffer
	if (error = setjmp(i2c_context)) return error;

	TXData[0] = address;				// address MSB
	TXData[1] = datum;					// data byte
	i2c_write(TMP102_ADR, TXData, 2);
	return 1;
} // end TMP102_write


//******************************************************************************
//******************************************************************************
//	initialize BQ3200 RTC
//
uint8 RTC_init()
{
	int error = 0;
	if (error = setjmp(i2c_context)) return error;

	i2c_write(RTC_ADR, (uint8 *)&error, 1);	// write/read BQ3200 register 0
	i2c_read(RTC_ADR, (uint8 *)&error, 1);
	return 0;
} // end RTC_init


//******************************************************************************
//	read from BQ3200 RTC
//
uint8 RTC_read(uint8 address)
{
	uint8 datum;
	int error;
	if (error = setjmp(i2c_context)) return error;

	i2c_write(RTC_ADR, (uint8 *)&address, 1);
	i2c_read(RTC_ADR, &datum, 1);
	return datum;
} // end RTC_read


//******************************************************************************
//	read from BQ3200 RTC
//
uint8 RTC_read_time(RTC_TIME* time)
{
	int error = 0;
	if (error = setjmp(i2c_context)) return error;

	i2c_write(RTC_ADR, (uint8 *)&error, 1);
	i2c_read(RTC_ADR, (uint8 *)time, sizeof(RTC_TIME));

	//	convert BCD to decimal
	//	hours = ((myTime.hours >> 4) & 0x03) * 10 + (myTime.hours & 0x0f);
	//	minutes = ((myTime.minutes >> 4) & 0x7) * 10 + (myTime.minutes & 0x0f);
	//	seconds = ((myTime.seconds >> 4) & 0x7) * 10 + (myTime.seconds & 0x0f);
	return 0;
} // end RTC_read_time


//******************************************************************************
//	read from BQ3200 RTC
//
#define BCD(x)	(((x/10)<<4)+(x%10))

uint8 RTC_write_time(RTC_TIME* time)
{
	int error;
	uint8 TXData[8];					// i2c tx buffer
	if (error = setjmp(i2c_context)) return error;

	TXData[0] = 0;						// register 0
	TXData[1] = BCD(time->seconds);
	TXData[2] = BCD(time->minutes);
	TXData[3] = BCD(time->hours);
	TXData[4] = BCD(time->day);
	TXData[5] = BCD(time->date);
	TXData[6] = BCD(time->month);
	TXData[7] = BCD(time->year);
	i2c_write(RTC_ADR, TXData, 8);
	return 0;
} // end RTC_write_time


//******************************************************************************
//******************************************************************************
//	initialize IO8
//
uint8 IO8_init()
{
	int error = 0;
	if (error = setjmp(i2c_context)) return error;

	i2c_write(IO8_ADR, (uint8 *)&error, 1);	// write/read IO8 register 0
	i2c_read(IO8_ADR, (uint8 *)&error, 1);
	return 0;
} // end IO_init


//******************************************************************************
//	read from IO8
//
uint8 IO8_read(uint8 address)
{
	uint8 datum;
	int error = 0;
	if (error = setjmp(i2c_context)) return error;

	i2c_write(IO8_ADR, (uint8 *)&error, 1);
	i2c_read(IO8_ADR, &datum, 1);
	return datum;
} // end IO_read


//******************************************************************************
//
uint8 IO8_write(uint8 address, uint8 datum)
{
	int error;
	uint8 TXData[2];					// i2c tx buffer
	if (error = setjmp(i2c_context)) return error;

	TXData[0] = address;				// address MSB
	TXData[1] = datum;					// data byte
	i2c_write(IO8_ADR, TXData, 2);
	return 1;
} // end IO8_write


//******************************************************************************
//******************************************************************************
//	initialize FRAM
//
uint8 FRAM_init(void)
{
	int error;
	if (error = setjmp(i2c_context)) return error;

	i2c_write(FRAM_ADR, (uint8 *)&error, 2);
	i2c_read(FRAM_ADR, (uint8 *)&error, 1);
	return 0;
} // end FRAM_init


//******************************************************************************
//	initialize FRAM
//
uint8 FRAM_set(uint16 size, uint8 data)
{
	unsigned int i;

	for (i = 0; i < size; i++) FRAM_write(i, data);

	return 0;
} // end FRAM_set


void FRAM_init_stream()
{
	FRAM_adr = 0x0000;
	FRAM_mask = 0x00;
	FRAM_data = 0x00;
	return;
}

uint8 FRAM_stream_read(void)
{
	// move to next byte in FRAM
	FRAM_mask <<= 1;
	if (FRAM_mask == 0x00)
	{
		FRAM_data = FRAM_read(FRAM_adr++);
		FRAM_mask = 0x01;
	}
	return (FRAM_data & FRAM_mask) ? 1 : 0;
}

void FRAM_stream_write(uint8 data)
{
	if (FRAM_mask == 0x00) FRAM_mask = 0x01;

	// add data to FRAM bit stream
	if (data) FRAM_data |= FRAM_mask;

	// flush byte on 8 bits of data
	FRAM_mask <<= 1;
	if (FRAM_mask == 0x00)
	{
		FRAM_write(FRAM_adr++, FRAM_data);
		FRAM_data = 0x00;
	}
	return;
}

//******************************************************************************
//	read from FRAM
//
uint8 FRAM_read(uint16 address)
{
	uint8 datum;
	int error;
	uint8 TXData[2];					// i2c tx buffer
	if (error = setjmp(i2c_context)) return error;

	TXData[0] = address >> 8;			// address MSB
	TXData[1] = address & 0x00ff;		// address LSB
	i2c_write(FRAM_ADR, TXData, 2);
	return i2c_read(FRAM_ADR, &datum, 1);
} // end FRAM_read


//******************************************************************************
//	write to FRAM
//
uint8 FRAM_write(uint16 address, uint8 datum)
{
	int error;
	uint8 TXData[4];					// i2c tx buffer
	if (error = setjmp(i2c_context)) return error;

	TXData[0] = address >> 8;			// address MSB
	TXData[1] = address & 0x00ff;		// address LSB
	TXData[2] = datum;					// data byte
	i2c_write(FRAM_ADR, TXData, 3);
	return 0;
} // end FRAM_write


//******************************************************************************
//******************************************************************************

const unsigned char adxl345_init_data[] = {
		XL345_THRESH_TAP,     THRESH_TAP,      // 0x1d: 62.5mg/LSB, 0x08=0.5g
		XL345_OFSX,           0x00,            // 0x1e: 0xff
		XL345_OFSY,           0x00,            // 0x1f: 0x05
		XL345_OFSZ,           0x00,            // 0x20: 0xff
		XL345_DUR,            TAP_DUR,         // 0x21: 625 us/LSB, 0x10=10ms
		XL345_LATENT,         0x00,            // 0x22: no double tap
		XL345_WINDOW,         0x00,            // 0x23: no double tap
		XL345_THRESH_ACT,     0x20,            // 0x24: 62.5mg/LSB, 0x20=2g
		XL345_THRESH_INACT,   0x03,            // 0x25: 62.5mg/LSB, 0x03=0.1875g
		XL345_TIME_INACT,     0x02,            // 0x26: 1s/LSB, 0x02=2s
		XL345_ACT_INACT_CTL,  0x00,            // 0x27: no activity interrupt
		XL345_THRESH_FF,      0x00,            // 0x28: no free fall
		XL345_TIME_FF,        0x00,            // 0x29: no free fall
		XL345_TAP_AXES,       TAP_AXES,        // 0x2a: TAP_Z enable
		XL345_BW_RATE,        XL345_RATE_100,  // 0x2c:
		XL345_POWER_CTL,      XL345_STANDBY,   // 0x2d: standby while changing int
		XL345_INT_ENABLE,     0x00,            // 0x2e: disable
		XL345_INT_MAP,        0x00,            // 0x2f: all interrupts to INT1
		XL345_DATA_FORMAT,    XL345_FORMAT,    // 0x31:
		XL345_FIFO_CTL,       0x00,            // 0x38:

		XL345_INT_ENABLE,     XL345_SINGLETAP, // 0x2e: single tap enable
		XL345_POWER_CTL,      XL345_MEASURE,   // 0x2d: measure
		0x00                                   // end of configuration
	};


//******************************************************************************
// initialize ADXL345 accelerometer
//
uint8 ADXL345_init()						// ADXL345 initialization
{
	unsigned char* data = (unsigned char*)adxl345_init_data;
	unsigned char reg;
	int error;
	if (error = setjmp(i2c_context)) return error;

	reg = XL345_DEVID;					// ADXL345 register
	i2c_write(ADXL345_ADR, &reg, 1);
	i2c_read(ADXL345_ADR, &reg, 1);


	// initialize adxl345
	while (*data)
	{
		reg = *data++;					// get register
		ADXL345_write(reg, data++, 1);	// write data
	}
	return 0;							// success
} // end ADXL345_init


//******************************************************************************
//	read multiple bytes from adxl345 into buffer
//
//		regaddr = xl345 register
//		    buf = pointer to buffer
//		  count = # of bytes to read
//
//		returns last byte read
//
uint8 ADXL345_read(uint8 regaddr, uint8 *buf, uint8 count)
{
	int error;
	if (error = setjmp(i2c_context)) return error;

	i2c_write(ADXL345_ADR, &regaddr, 1);
	return i2c_read(ADXL345_ADR, buf, count);
} // end ADXL345_read


//******************************************************************************
//	write multiple bytes to adxl345 accelerometer
//
//		regaddr = xl345 register
//		   data = pointer to data
//		  count = # of bytes to write
//
//		return 0 = success
//
#define MAX_ADXL345_COUNT	10
uint8 ADXL345_write(uint8 regaddr, uint8 *data, uint8 count)
{
	uint16 i;
	uint8 error;
	uint8 TXData[MAX_ADXL345_COUNT];	// i2c tx buffer

	if (error = setjmp(i2c_context)) return error;
	if (count >= MAX_ADXL345_COUNT) return 1;

	TXData[0] = regaddr;				// ADXL345 register
	for (i = 1; i <= count; ++i)
	{
		TXData[i] = *data++;			// retrieve data
	}
	i2c_write(ADXL345_ADR, TXData, count+1);
	return 0;
} // end ADXL345_write
