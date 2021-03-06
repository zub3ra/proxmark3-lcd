//-----------------------------------------------------------------------------
// This code is licensed to you under the terms of the GNU GPL, version 2 or,
// at your option, any later version. See the LICENSE.txt file for the text of
// the license.
//-----------------------------------------------------------------------------
// LCD code
//-----------------------------------------------------------------------------

#include "proxmark3.h"
#include "apps.h"
#include "LCD.h"
#include "printf.h"
#include "fonts.h"

//32-bit variable to hold LCD identifier
static unsigned int LCD_id;

//Get LCD ID to be able to work with Philips/Leadlis/Epson LCDs
void LCDGetId (void)
{
unsigned char command;

//LCD ID is got over MOSI line, so handle I/O lines directly for this routine

  // Set as outputs
	AT91C_BASE_PIOA->PIO_OER = GPIO_NCS0	|
			GPIO_NCS1 	|
			GPIO_NCS2 	|
			GPIO_NCS3 	|
			GPIO_MOSI	|
			GPIO_SPCK;
			
	// Set high
	AT91C_BASE_PIOA->PIO_SODR = GPIO_NCS0	|
			GPIO_NCS1 	|
			GPIO_NCS2 	|
			GPIO_NCS3 	|
			GPIO_MOSI;
			
	// Make sure SPI is disabled
	AT91C_BASE_SPI->SPI_CR = AT91C_SPI_SPIDIS;

  // Ensure relevant lines are set as general purpose
	AT91C_BASE_PIOA->PIO_PER = GPIO_NCS0	|
			GPIO_NCS1 	|
			GPIO_NCS2 	|
			GPIO_NCS3 	|
			GPIO_MOSI	|
			GPIO_SPCK;
			
  // Ensure LCD HW is ready (at least 5ms has passed)
  SpinDelay(5);

	// SPCK must be low		
	LOW(GPIO_SPCK);
  SpinDelay(1);

  //Send command 0x04
	LOW(GPIO_NCS2);      // enable LCD, PA10 goes low
  SpinDelay(1);
  LOW(GPIO_MOSI);      // output low on data out (9th bit low = command)
  SpinDelay(1);

  HIGH(GPIO_SPCK);      // send clock pulse
  SpinDelay(1);
  LOW(GPIO_SPCK);
  SpinDelay(1);

  command = 0x04;
  
  for (char j = 0; j < 8; j++)
  {
     if ((command & 0x80) == 0x80) HIGH(GPIO_MOSI);
     else LOW(GPIO_MOSI);

     HIGH(GPIO_SPCK);      // send clock pulse
     SpinDelay(1);
     // After the last bit of the command the LCD
     // sets data pin as an output, so set MOSI as input
     // before falling edge of clock to avoid HW contention
     if (j==7)
     {	
       // Set MOSI as input
       AT91C_BASE_PIOA->PIO_ODR = GPIO_MOSI;
	     // Enable pull-ups
	     AT91C_BASE_PIOA->PIO_PPUER = GPIO_MOSI;
       SpinDelay(1);
     }
     LOW(GPIO_SPCK);
     SpinDelay(1);

     command <<= 1;
  }
			
  //Read ID
  LCD_id = 0;
  HIGH(GPIO_SPCK);      // send dummy clock pulse
  SpinDelay(1);
  LOW(GPIO_SPCK);
  SpinDelay(2);
  for (char j = 0; j < 24; j++)
  {
     LCD_id <<= 1;
     if (AT91C_BASE_PIOA->PIO_PDSR & GPIO_MOSI) LCD_id|=1;
     HIGH(GPIO_SPCK);      // send clock pulse
     SpinDelay(1);
     LOW(GPIO_SPCK);
     SpinDelay(2);

  }

  HIGH(GPIO_NCS2);    		// disable LCD
  SpinDelay(1);
  
  // Leave MOSI as output
  AT91C_BASE_PIOA->PIO_OER = GPIO_MOSI;
  SpinDelay(1);
  
  // Assign lines again to SPI controller
	AT91C_BASE_PIOA->PIO_PDR = GPIO_NCS0	|
			GPIO_NCS1 	|
			GPIO_NCS2 	|
			GPIO_NCS3 	|
			GPIO_MOSI	|
			GPIO_SPCK;
  SpinDelay(1);

}

void LCDSend(unsigned int data)
{
//	// 9th bit set for data, clear for command
//	while ((AT91C_BASE_SPI->SPI_SR & AT91C_SPI_TXEMPTY) == 0);	// wait for the transfer to complete
//	// For clarity's sake we pass data with 9th bit clear and commands with 9th
//	// bit set since they're implemented as defines, se we need to invert bit
//	AT91C_BASE_SPI->SPI_TDR = data^0x100;							// Send the data/command

	// 9th bit set for data, clear for command
	spi_com(SPI_LCD_MODE, data^0x100,0);

}

void LCDSetXY(unsigned char x, unsigned char y)
{

	if ((LCD_id & 0xFFFFFF) == 0xFFFFFF) //No ID read -> EPSON
  {
  	LCDSend(ECASET);			// column start/end ram
  	LCDSend(x);					// Start Column to display to
  	LCDSend(131);				// End Column to display to	
	
  	LCDSend(EPASET);			// page start/end ram
  	LCDSend(y);					// Start Page to display to
  	LCDSend(131);				// End Page to display to
  }
  else {	
  	LCDSend(PCASET);			// column start/end ram
  	LCDSend(x);					// Start Column to display to
  	LCDSend(131);				// End Column to display to	
	
  	LCDSend(PPASET);			// page start/end ram
  	LCDSend(y);					// Start Page to display to
  	LCDSend(131);				// End Page to display to
  }	
}

void LCDSetPixel(unsigned char x, unsigned char y, unsigned char color)
{
	LCDSetXY(x,y);				// Set position
	if ((LCD_id & 0xFFFFFF) == 0xFFFFFF) //No ID read -> EPSON
    LCDSend(ERAMWR);			// Now write the pixel to the display
  else  LCDSend(PRAMWR);			// Now write the pixel to the display
	LCDSend(color);				// Write the data in the specified Color
}

void LCDFill (unsigned char xs,unsigned char ys,unsigned char width,unsigned char height, unsigned char color)
{
    unsigned char i,j;

    for (i=0;i < height;i++)	// Number of horizontal lines
    {
		LCDSetXY(xs,ys+i);		// Goto start of fill area (Top Left)
	if ((LCD_id & 0xFFFFFF) == 0xFFFFFF) //No ID read -> EPSON
    LCDSend(ERAMWR);			// Write to displa
  else LCDSend(PRAMWR);		// Write to display

		for (j=0;j < width;j++)	// pixels per line
			LCDSend(color);
    }
}

void LCDString (char *lcd_string, const char *font_style,unsigned char x, unsigned char y, unsigned char fcolor, unsigned char bcolor)
{
	unsigned int  i;
	unsigned char mask=0, px, py, xme, yme, offset;
	const char *data;

	data = font_style;			// point to the start of the font table

	xme = *data;				// get font x width
	data++;
	yme = *data;				// get font y length
	data++;
	offset = *data;				// get data bytes per font

	do
	{
		// point to data in table to be loaded
		data =  (font_style + offset) + (offset * (int)(*lcd_string - 32));

		for (i=0;i < yme;i++) {
			mask |=0x80;

			for (px=x; px < (x + xme); px++) {
				py= y + i;

				if (*data & mask)	LCDSetPixel (px,py,fcolor);
				else				LCDSetPixel (px,py,bcolor);

				mask>>=1;
			}
			data++;
		}
		x+=xme;

		lcd_string++;						// next character in string

	} while(*lcd_string !='\0');			// keep spitting chars out until end of string
}



//********************************************************************
//Usage: LCDWriteBMP(myImg,10,10);
//Arguments: unsigned rom char *bmp, unsigned char x, unsigned char y
//Return: None
//Description: This function will read the first 2 bytes in the image
//             which should WIDTH then HEIGHT.
//             Then it will set the working area on LCD and 
//             loop a calculated amount of times grabbing data from image
//             and placing it on LCD from LEFT to RIGHT then TOP to BOTTOM
//********************************************************************
void LCDWriteBMP(unsigned char *bmp, unsigned char x, unsigned char y)
{
unsigned char height, width;
unsigned int maxPix;
unsigned int xmin, xmax, ymin, ymax;

    maxPix = 0;

    //maxPix = (width * height);
    width = *bmp++;
    height = *bmp++;

    maxPix = height;
    maxPix = maxPix * width ;

    xmin = x;
    xmax = x + width;
    ymin = y;
    ymax = y + height;

	if ((LCD_id & 0xFFFFFF) == 0xFFFFFF) //No ID read -> EPSON
  {
  	LCDSend(ECASET);			// column start/end ram
  	LCDSend(xmin);				// Start Column to display to
  	LCDSend(xmax);				// End Column to display to	
	
  	LCDSend(EPASET);			// page start/end ram
  	LCDSend(ymin);				// Start Page to display to
  	LCDSend(ymax);				// End Page to display to

    LCDSend(ERAMWR);
  }
  else {	
  	LCDSend(PCASET);			// column start/end ram
  	LCDSend(xmin);				// Start Column to display to
  	LCDSend(xmax);				// End Column to display to	
	
  	LCDSend(PPASET);			// page start/end ram
  	LCDSend(ymin);				// Start Page to display to
  	LCDSend(ymax);				// End Page to display to

    LCDSend(PRAMWR);
  }	

#ifdef BIT8
unsigned int j;
    for(j = 0; j < maxPix-1; j++) {
        LCDSend(*bmp++);
    }
#endif
}


void LCDReset(void)
{
//CEX: Removed aliasing of LED names with LCD 
// backlight to avoid flickering, so use the
// correct name (active high).
// RESET line is not conected to LCD, so skip

	SetupSpi(SPI_LCD_MODE); // Init LCD
	SpinDelay(100);
	HIGH(GPIO_LCD_BL); // Turn on backlight
}
void LCDInit(void) 
{
	int i;

	LCDReset();

	if ((LCD_id & 0xFFFFFF) == 0xFFFFFF) //No ID read -> EPSON
	{
    LCDSend(EDISCTL);  	// display control(EPSON)
    LCDSend(0x0C);   		// 12 = 1100 - CL dividing ratio [don't divide] switching period 8H (default)
    LCDSend(0x20);      // (32 + 1) * 4 = 132 lines (of which 130 are visible)
    LCDSend(0x00);      // Do not invert lines
	
    LCDSend(ECOMSCN);  	// common scanning direction(EPSON)
    LCDSend(0x01);
    
    LCDSend(EOSCON);  	// internal oscialltor ON(EPSON)
	
    LCDSend(ESLPOUT);  	// sleep out(EPSON)
    
    LCDSend(EPWRCTR); 	// power ctrl(EPSON)
    LCDSend(0x0F);    		//everything on, no external reference resistors
	
	  LCDSend(EDISINV);  	// invert display mode(EPSON)
    
    LCDSend(EDATCTL);  	// data control(EPSON)
    LCDSend(0x03);	// inverted orientation; connector up-right
    LCDSend(0x00);	// RGB arrangement (RGB all rows/cols)
    LCDSend(0x01);	// 8 bit-color display
	
    LCDSend(EVOLCTR);  	// electronic volume, this is the contrast/brightness(EPSON)
    LCDSend(0x21);   		// volume (contrast) setting - fine tuning, original
    LCDSend(0x03);   		// internal resistor ratio - coarse adjustment
	   
    LCDSend(ENOP);  	  // nop(EPSON)
    
    LCDSend(ERGBSET8);//setup color lookup table 
    LCDSend(0x00);	// 000 RED  
    LCDSend(0x02);	// 001      
    LCDSend(0x04);	// 010      
    LCDSend(0x06);	// 011      
    LCDSend(0x08);	// 100      
    LCDSend(0x0a);	// 101      
    LCDSend(0x0c);	// 110      
    LCDSend(0x0f);	// 111      
    LCDSend(0x00);	// 000 GREEN
    LCDSend(0x02);	// 001      
    LCDSend(0x04);	// 010      
    LCDSend(0x06);	// 011      
    LCDSend(0x08);	// 100      
    LCDSend(0x0a);	// 101      
    LCDSend(0x0c);	// 110      
    LCDSend(0x0f);	// 111      
    LCDSend(0x00);	//  00 BLUE 
    LCDSend(0x06);	//  01      
    LCDSend(0x09);	//  10      
    LCDSend(0x0f);	//  11      
      

    LCDSend(EDISON);   	// display on(EPSON)
	  SpinDelay(100);
    
 	  // clear display
 	  LCDSetXY(0,0);
	  LCDSend(ERAMWR);			// Write to display

  }	
	else if ((LCD_id & 0x298003) == 0x298003) //PHILIPS? 298303 and 298503
  {
  	LCDSend(PSWRESET);			// software reset
	  SpinDelay(100);
	  LCDSend(PSLEEPOUT);			// exit sleep mode
	  LCDSend(PBSTRON);			// booster on
	  LCDSend(PDISPON);			// display on
	  LCDSend(PNORON);			// normal on
	  LCDSend(PMADCTL);			// rotate display 180 deg
	  LCDSend(0xC0);

	  LCDSend(PCOLMOD);			// color mode
	  LCDSend(0x02);				// 8bpp color mode

	  LCDSend(PSETCON);			// set contrast
	  LCDSend(0xBE);                      // SE VE BIEN EN LA PANTALLA VERDE

	  // clear display
 	  LCDSetXY(0,0);
	  LCDSend(PRAMWR);			// Write to display
  }
  else if ((LCD_id & 0xFE8003) == 0xFE8003) //Philips clone? FE8203 
  {
	  LCDSend(PSWRESET);			// software reset
	  SpinDelay(100);
	  LCDSend(PSLEEPOUT);			// exit sleep mode
	  LCDSend(PBSTRON);			// booster on
	  LCDSend(LMADCTR); 
	  LCDSend(PMADCTL);			// rotate display 180 deg
	  LCDSend(0xC0);	
	
	  LCDSend(PCOLMOD);			// color mode
	  LCDSend(0x02);				// 8bpp color mode

	  LCDSend(LRGBSET);			// color mode
	  LCDSend(0x00);
	  LCDSend(0x02);
	  LCDSend(0x04);
	  LCDSend(0x06);
	  LCDSend(0x08);
	  LCDSend(0x0a);
	  LCDSend(0x0c);
	  LCDSend(0x0f);
	  LCDSend(0x00);
	  LCDSend(0x02);
	  LCDSend(0x04);
	  LCDSend(0x06);
	  LCDSend(0x08);
	  LCDSend(0x0a);
	  LCDSend(0x0c);
	  LCDSend(0x0f);
	  LCDSend(0x00);
	  LCDSend(0x04);
	  LCDSend(0x09);
	  LCDSend(0x0f);
	  LCDSend(LNOP);
	  LCDSend(PSETCON);			// set contrast
	  LCDSend(0xBE);                      // SE VE BIEN EN LA PANTALLA
	  LCDSend(PDISPON);			// display on 
	  // clear display
 	   LCDSetXY(0,0);
	  LCDSend(PRAMWR);			// Write to display
	}
  else //if ((LCD_id & 0x458003) == 0x458003) //LEADIS? 458203 and 45FF03
  {
	  LCDSend(PSWRESET);			// software reset
	  SpinDelay(100);
	  LCDSend(PSLEEPOUT);			// exit sleep mode
	  LCDSend(PBSTRON);			// booster on
	  LCDSend(LINVON);
	  LCDSend(LMADCTR); 
	  LCDSend(PMADCTL);			// rotate display 180 deg
	  LCDSend(0x88);	
	
	  LCDSend(PCOLMOD);			// color mode
	  LCDSend(0x02);				// 8bpp color mode

	  LCDSend(LRGBSET);			// color mode
	  LCDSend(0x00);
	  LCDSend(0x02);
	  LCDSend(0x04);
	  LCDSend(0x06);
	  LCDSend(0x08);
	  LCDSend(0x0a);
	  LCDSend(0x0c);
	  LCDSend(0x0f);
	  LCDSend(0x00);
	  LCDSend(0x02);
	  LCDSend(0x04);
	  LCDSend(0x06);
	  LCDSend(0x08);
	  LCDSend(0x0a);
	  LCDSend(0x0c);
	  LCDSend(0x0f);
	  LCDSend(0x00);
	  LCDSend(0x04);
	  LCDSend(0x09);
	  LCDSend(0x0f);
	  LCDSend(LNOP);
	  LCDSend(PSETCON);			// set contrast
	  LCDSend(0xBE);                      // SE VE BIEN EN LA PANTALLA
	  LCDSend(PDISPON);			// display on 
	  // clear display
 	   LCDSetXY(0,0);
	  LCDSend(PRAMWR);			// Write to display
	}
//	else  // EPSON
//	{
//	//falta inicialización EPSON	
//  }		

	i=LCD_XRES*LCD_YRES;
	while(i--) LCDSend(0xFF);

  //********************
  //TEST show LCD id
char texto[24];
sprintf(texto, " LCD id: %X",LCD_id);
LCDString(texto,     (char *)&FONT6x8,1,1+8*15,0x00  ,0xFF );
SpinDelay(10);
	//********************

}

