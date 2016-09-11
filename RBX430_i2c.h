#ifndef I2C_H_
#define I2C_H_

//******************************************************************************
//	i2c equates
//
#define ADXL345_ADR			0x1d				// ADXL345 accelerometer
#define FRAM_ADR			0x50				// F-RAM
#define TMP102_ADR			0x48				// Digital Temperature Sensor
#define RTC_ADR				0x68				// Real-time Clock
#define IO8_ADR				0x38				// I/O Expander
//#define IO8_ADR				0x20				// I/O Expander

uint8 i2c_init(void);
uint8 i2c_write(uint16 address, uint8* data, int16 bytes);
uint8 i2c_read(uint16 address, uint8* buffer, int16 bytes);

void wait(uint16 time);


//******************************************************************************
//	TMP102 prototypes
//
#define TMP102_ADR_READ		((TMP102_ADR<<1)|0x01)
#define TMP102_ADR_WRITE	(TMP102_ADR<<1)

uint8 TMP102_init(void);
uint8 TMP102_read(uint8 address);
uint16 TMP102_read_word(uint8 address);
uint8 TMP102_write(uint8 address, uint8 datum);


//******************************************************************************
//	BQ3200 RTC prototypes
//
typedef struct time_struct
{
	uint8	seconds;
	uint8	minutes;
	uint8	hours;
	uint8	day;
	uint8	date;
	uint8	month;
	uint8	year;
	uint8	cal_cfg1;
} RTC_TIME;

uint8 RTC_init(void);
uint8 RTC_read(uint8 address);
uint8 RTC_read_time(RTC_TIME* time);
uint8 RTC_write_time(RTC_TIME* time);


//******************************************************************************
//	IO8 prototypes
//
uint8 IO8_init(void);
uint8 IO8_read(uint8 address);
uint8 IO8_write(uint8 address, uint8 datum);


//******************************************************************************
//	FRAM prototypes
//
#define FRAM_SIZE			8192
#define FRAM_LCD_SIZE		3200		// 160 x 160 / 8

uint8 FRAM_init(void);
uint8 FRAM_set(uint16 size, uint8 data);
uint8 FRAM_read(uint16 address);
uint8 FRAM_write(uint16 address, uint8 datum);

void FRAM_init_stream();
uint8 FRAM_stream_read(void);
void FRAM_stream_write(uint8 data);


//******************************************************************************
//	ADXL345 Prototypes
//
#define ADXL345_ADR_READ	((ADXL345_ADR<<1)|0x01)
#define ADXL345_ADR_WRITE	(ADXL345_ADR<<1)

typedef union myXYZ			// X, Y, Z struct
{
	unsigned char axis_data[6];
	struct
	{
		int16 XX;			// little endian will swap axis_data[0 & 1]
		int16 YY;			// little endian will swap axis_data[2 & 3]
		int16 ZZ;			// little endian will swap axis_data[4 & 5]
	} xyz;
} XYZ;

uint8 ADXL345_init(void);
uint8 ADXL345_read(uint8 regaddr, uint8 *buf, uint8 count);
uint8 ADXL345_write(uint8 regaddr, uint8 *data, uint8 count);

//******************************************************************************
// ADXL345 Register names
#define XL345_DEVID				0x00
#define XL345_RESERVED1			0x01
#define XL345_THRESH_TAP		0x1d
#define XL345_OFSX				0x1e
#define XL345_OFSY				0x1f
#define XL345_OFSZ				0x20
#define XL345_DUR				0x21
#define XL345_LATENT			0x22
#define XL345_WINDOW			0x23
#define XL345_THRESH_ACT		0x24
#define XL345_THRESH_INACT		0x25
#define XL345_TIME_INACT		0x26
#define XL345_ACT_INACT_CTL		0x27
#define XL345_THRESH_FF			0x28
#define XL345_TIME_FF			0x29
#define XL345_TAP_AXES			0x2a
#define XL345_ACT_TAP_STATUS	0x2b
#define XL345_BW_RATE			0x2c
#define XL345_POWER_CTL			0x2d
#define XL345_INT_ENABLE		0x2e
#define XL345_INT_MAP			0x2f
#define XL345_INT_SOURCE		0x30
#define XL345_DATA_FORMAT		0x31
#define XL345_DATAX0			0x32
#define XL345_DATAX1			0x33
#define XL345_DATAY0			0x34
#define XL345_DATAY1			0x35
#define XL345_DATAZ0			0x36
#define XL345_DATAZ1			0x37
#define XL345_FIFO_CTL			0x38
#define XL345_FIFO_STATUS		0x39

//----------------------------------------------------------------------
//	Bit field definitions and register values
//----------------------------------------------------------------------

#define XL345_ID				0xe5		// defive id
#define XL345_SOFT_RESET		0x52		// soft reset

//	Registers THRESH_TAP through TIME_INACT, THRESH_FF and TIME_FF
//	take only 8-bit values - no specific bit field values

//	ACT_INACT_CTL
#define XL345_INACT_Z_ENABLE	0x01
#define XL345_INACT_Z_DISABLE	0x00
#define XL345_INACT_Y_ENABLE	0x02
#define XL345_INACT_Y_DISABLE	0x00
#define XL345_INACT_X_ENABLE	0x04
#define XL345_INACT_X_DISABLE	0x00
#define XL345_INACT_AC			0x08
#define XL345_INACT_DC			0x00
#define XL345_ACT_Z_ENABLE		0x10
#define XL345_ACT_Z_DISABLE		0x00
#define XL345_ACT_Y_ENABLE		0x20
#define XL345_ACT_Y_DISABLE		0x00
#define XL345_ACT_X_ENABLE		0x40
#define XL345_ACT_X_DISABLE		0x00
#define XL345_ACT_AC			0x80
#define XL345_ACT_DC			0x00

//	TAP_AXES
#define XL345_TAP_Z_ENABLE		0x01
#define XL345_TAP_Z_DISABLE		0x00
#define XL345_TAP_Y_ENABLE		0x02
#define XL345_TAP_Y_DISABLE		0x00
#define XL345_TAP_X_ENABLE		0x04
#define XL345_TAP_X_DISABLE		0x00
#define XL345_TAP_SUPPRESS		0x08

//	ACT_TAP_STATUS
#define XL345_TAP_Z_SOURCE		0x01
#define XL345_TAP_Y_SOURCE		0x02
#define XL345_TAP_X_SOURCE		0x04
#define XL345_STAT_ASLEEP		0x08
#define XL345_ACT_Z_SOURCE		0x10
#define XL345_ACT_Y_SOURCE		0x20
#define XL345_ACT_X_SOURCE		0x40

//	BW_RATE
#define XL345_RATE_3200			0x0f
#define XL345_RATE_1600			0x0e
#define XL345_RATE_800			0x0d
#define XL345_RATE_400			0x0c
#define XL345_RATE_200			0x0b
#define XL345_RATE_100			0x0a
#define XL345_RATE_50			0x09
#define XL345_RATE_25			0x08
#define XL345_RATE_12_5			0x07
#define XL345_RATE_6_25			0x06
#define XL345_RATE_3_125		0x05
#define XL345_RATE_1_563		0x04
#define XL345_RATE__782			0x03
#define XL345_RATE__39			0x02
#define XL345_RATE__195			0x01
#define XL345_RATE__098			0x00

// BW_RATE expressed as bandwidth
#define XL345_BW_1600			0x0f
#define XL345_BW_800			0x0e
#define XL345_BW_400			0x0d
#define XL345_BW_200			0x0c
#define XL345_BW_100			0x0b
#define XL345_BW_50				0x0a
#define XL345_BW_25				0x09
#define XL345_BW_12_5			0x08
#define XL345_BW_6_25			0x07
#define XL345_BW_3_125			0x06
#define XL345_BW_1_563			0x05
#define XL345_BW__782			0x04
#define XL345_BW__39			0x03
#define XL345_BW__195			0x02
#define XL345_BW__098			0x01
#define XL345_BW__048			0x00
#define XL345_LOW_POWER			0x08
#define XL345_LOW_NOISE			0x00

//	POWER_CTL
#define XL345_WAKEUP_8HZ		0x00
#define XL345_WAKEUP_4HZ		0x01
#define XL345_WAKEUP_2HZ		0x02
#define XL345_WAKEUP_1HZ		0x03
#define XL345_SLEEP				0x04
#define XL345_MEASURE			0x08
#define XL345_STANDBY			0x00
#define XL345_AUTO_SLEEP		0x10
#define XL345_ACT_INACT_SERIAL	0x20
#define XL345_ACT_INACT_CONCURRENT	0x00

//	INT_ENABLE, INT_MAP, and INT_SOURCE
#define XL345_OVERRUN			0x01
#define XL345_WATERMARK			0x02
#define XL345_FREEFALL			0x04
#define XL345_INACTIVITY		0x08
#define XL345_ACTIVITY			0x10
#define XL345_DOUBLETAP			0x20
#define XL345_SINGLETAP			0x40
#define XL345_DATAREADY			0x80

//	DATA_FORMAT (use for DATAX0 through DATAZ1)
#define XL345_RANGE_2G			0x00
#define XL345_RANGE_4G			0x01
#define XL345_RANGE_8G			0x02
#define XL345_RANGE_16G			0x03
#define XL345_DATA_JUST_RIGHT	0x00
#define XL345_DATA_JUST_LEFT	0x04
#define XL345_10BIT				0x00
#define XL345_FULL_RESOLUTION	0x08
#define XL345_INT_LOW			0x20
#define XL345_INT_HIGH			0x00
#define XL345_SPI3WIRE			0x40
#define XL345_SPI4WIRE			0x00
#define XL345_SELFTEST			0x80

//	FIFO_CTL - bit valves 0 to 31 used for the watermark
//	or the number of pre-trigger samples when in triggered mode
#define XL345_TRIGGER_INT1		0x00
#define XL345_TRIGGER_INT2		0x20
#define XL345_FIFO_MODE_BYPASS	0x00
#define XL345_FIFO_RESET		0x00
#define XL345_FIFO_MODE_FIFO	0x40
#define XL345_FIFO_MODE_STREAM	0x80
#define XL345_FIFO_MODE_TRIGGER	0xc0

//	FIFO_STATUS (%32 = # of entries currently in FIFO)
#define XL345_FIFO_TRIGGERED	0x80

//#define XL345_INT		XL345_ACTIVITY | XL345_INACTIVITY | XL345_FREEFALL
#define XL345_FORMAT	XL345_FULL_RESOLUTION | XL345_DATA_JUST_RIGHT | XL345_RANGE_16G

#if 1
#define TAP_DUR		0x10		// 625 us/LSB, 0x10=10ms
#define THRESH_TAP	0x08		// 62.5mg/LSB, 0x08=0.5g
#define TAP_AXES	XL345_TAP_Z_ENABLE	// 0x2a: TAP_Z enable
#else
#define TAP_DUR		0x08		// 625 us/LSB, 0x08=5ms
#define THRESH_TAP	0x10		// 62.5mg/LSB, 0x10=1.0g
#define TAP_AXES	(XL345_TAP_X_ENABLE+XL345_TAP_Y_ENABLE+XL345_TAP_Z_ENABLE)
#endif

#endif /*I2C_H_*/
