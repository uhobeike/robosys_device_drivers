#ifndef _I2C_LCD_H
#define _I2C_LCD_H 1

#define	LCD_RS_CMD_WRITE	0x00
#define LCD_RS_DATA_WRITE	0x40

#define LCD_CLEARDISPLAY	  0x01
#define LCD_RETURNHOME		  0x02
#define LCD_ENTRYMODESET	  0x04
#define LCD_DISPLAYCONTROL  0x08
#define LCD_CURSORSHIFT		  0x10
#define LCD_FUNCTIONSET		  0x20
#define LCD_SETCGRAMADDR	  0x40
#define LCD_SETDDRAMADDR	  0x80

#define LCD_ENTRYRIGHT    0x00
#define LCD_ENTRYLEFT			0x02
#define LCD_ENTRYSHIFTINCREMENT	0x01
#define LCD_ENTRYSHIFTDECREMENT	0x00

#define LCD_DISP_ON			  0x04
#define LCD_DISP_OFF		  0x00
#define LCD_DISP_CURON		0x02
#define LCD_DISP_CUROFF		0x00
#define LCD_DISP_BLINK		0x01
#define LCD_DISP_NOBLINK	0x00

#define LCD_DISPLAYMOVE		0x08
#define LCD_CURSORMOVE		0x00
#define LCD_MOVERIGHT		  0x04
#define LCD_MOVELEFT		  0x00

// Functions
#define LCD_FUNC_8BITMODE	0x10
#define LCD_FUNC_4BITMODE	0x00
#define LCD_FUNC_INSTABLE	0x01
#define LCD_FUNC_2LINE		0x08
#define LCD_FUNC_1LINE		0x00
#define LCD_FUNC_5x10DOTS	0x04
#define LCD_FUNC_5x8DOTS	0x00

#define LCD_IS_OSC        0x10
#define LCD_IS_OSC_BSQ		0x08
#define LCD_IS_OSC_BSP		0x00
#define LCD_IS_CONTSET1		0x70
#define LCD_IS_CONTSET2		0x5C
#define LCD_IS_FOLLOWER		0x60
#define LCD_IS_FOLLOWER_ON	0x08

#define contrast_upper(x)	((x >> 4) & 0x03)
#define contrast_lower(x)	(x & 0x0F)


#endif
