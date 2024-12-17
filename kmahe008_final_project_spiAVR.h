#ifndef SPIAVR_H
#define SPIAVR_H

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "helper.h"
#include <avr/pgmspace.h>
#include "characters.h"
#include <avr/eeprom.h>


//B5 should always be SCK(spi clock) and B3 should always be MOSI. If you are using an
//SPI peripheral that sends data back to the arduino, you will need to use B4 as the MISO pin.
//The SS pin can be any digital pin on the arduino. Right before sending an 8 bit value with
//the SPI_SEND() funtion, you will need to set your SS pin to low. If you have multiple SPI
//devices, they will share the SCK, MOSI and MISO pins but should have different SS pins.
//To send a value to a specific device, set it's SS pin to low and all other SS pins to high.

// Outputs, pin definitions
#define PIN_SCK                   PORTB5//SHOULD ALWAYS BE B5 ON THE ARDUINO
#define PIN_MOSI                  PORTB3//SHOULD ALWAYS BE B3 ON THE ARDUINO
#define PIN_SS                    PORTD7    //was B1
#define PIN_RESET                 PORTB0
#define PIN_A0                    PORTB2    //was B2 

#define SWRESET 0x01 // software reset
#define SLPOUT 0x11 // sleep out
#define DISPOFF 0x28 // display off
#define DISPON 0x29 // display on
#define CASET 0x2A // column address set
#define RASET 0x2B // row address set
#define RAMWR 0x2C // RAM write
#define MADCTL 0x36 // axis control
#define COLMOD 0x3A // color mode
// 1.44" TFT display constants
#define XSIZE 130
#define YSIZE 160
#define XMAX XSIZE-1
#define YMAX YSIZE-1

// Define colors
#define COLOR_GREEN 0x07E0
#define COLOR_WHITE 0xFFFF
#define COLOR_BLACK 0x0000
#define COLOR_RED 0x00FF
#define YELLOW 0xFFE0

uint8_t currX;
uint8_t currY;


void HardwareReset() {
    PORTB = SetBit(PORTB, 0, 0);
    _delay_ms(200);
    PORTB = SetBit(PORTB, 0, 1);
    _delay_ms(200);
}


//If SS is on a different port, make sure to change the init to take that into account.
void SPI_INIT() {
    DDRB |= (1 << PIN_SCK) | (1 << PIN_MOSI) /*| (1 << PIN_SS)*/ | (1 << PIN_RESET) | (1 << PIN_A0);
    DDRD |= (1 << PIN_SS);
    PORTB = SetBit(PORTB, 0, 0);
    SPCR |= (1 << SPE) | (1 << MSTR) | (0 << SPR0) | (0 << SPR1) ; //initialize SPI comunication
    SPSR |= (1 << SPI2X);
}

void SPI_SEND(int data)
{
    SPDR = data;//set data that you want to transmit
    while (!(SPSR & (1 << SPIF)));// wait until done transmitting
}

void Send_Command(char data) {
    PORTB = SetBit(PORTB, 2, 0);        //WAS BIT 2
    PORTD = SetBit(PORTD, 7, 0);    //was set bit 1 of portb 0
    SPI_SEND(data);
    PORTD = SetBit(PORTD, 7, 1);    //was set bit 1 of portb 1
}

void Send_Data(char data) {
    PORTB = SetBit(PORTB, 2, 1);    //WAS BIT 2
    PORTD = SetBit(PORTD, 7, 0);    //was set bit 1 OF PORT B 0
    SPI_SEND(data);
    PORTD = SetBit(PORTD, 7, 1);    //was set bit 1 0F PORT B 1
}

void WriteWord (char w)
{
 Send_Data(w >> 8); // write upper 8 bits
 Send_Data(w & 0xFF); // write lower 8 bits
}


void Init_Display() {
    HardwareReset();
    Send_Command(SWRESET);
    _delay_ms(150);
    Send_Command(SLPOUT);
    _delay_ms(200);
    Send_Command(COLMOD);
    Send_Data(0x05); //for 18 bit color mode. You can pick any color mode you want
    _delay_ms(10);
    Send_Command(DISPON);
    _delay_ms(200);
}

void GotoXY(int x, int y) {
    currX = x;
    currY = y;
}

void SetAddrWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    Send_Command(CASET); // set column range (x0,x1)
    WriteWord(x0);
    WriteWord(x1);
    Send_Command(RASET); // set row range (y0,y1)
    WriteWord(y0);
    WriteWord(y1);
}


void CharDisplay(char ch, uint8_t x, uint8_t y, int color)
{
    int pixel;
    uint8_t row, col, bit, data, mask = 0x01;
    SetAddrWindow(x,y,x+4,y+6);
    Send_Command(RAMWR);

    for (row=0; row<7;row++) {
      for (col=0; col<5;col++) {
        data = pgm_read_byte(&(CHARS[ch-48][col]));
        bit = data & mask;
        if (bit==0) pixel= COLOR_BLACK;
        else pixel=color;
        WriteWord(pixel);
    }
    mask <<= 1;
  }
}

void AdvanceCursor() {
    currX++; 
    if (currX>20) {
        currY++; 
        currX = 0; 
    }
    if (currY>19) {
         currY = 0; 
    }
}

void WriteChar(char ch, int color) {
 CharDisplay(ch,currX*6, currY*8, color);
 AdvanceCursor();
}


void WriteString(char* text, int color) {
    for (;*text;text++) // for all non-nul chars
    WriteChar(*text,color); // write the char
}

void SetScreen(int color)
{
    SetAddrWindow(0,0,XMAX,YMAX);
    Send_Command(RAMWR);
    for (unsigned int i=40960; i>0; --i) {
        Send_Data((color >>8) & 0xFF);
        Send_Data(color &0xFF);
    }

}

void DrawPixel (uint8_t x, uint8_t y, int color) {
  SetAddrWindow(x,y,x,y); // set active region = 1 pixel
  Send_Command(RAMWR); // memory write
  Send_Data((color >> 8) & 0xFF); // Send high byte of color
  Send_Data(color & 0xFF);        // Send low byte of color
}


void DrawBitmap(int16_t x, int16_t y, const unsigned short *image, int16_t w, int16_t h){
  int16_t originalWidth = w;              // save this value; even if not all columns fit on the screen, the image is still this width in ROM
  int i = w*(h - 1);

  SetAddrWindow(x, y-h+1, x+w-1, y);
  Send_Command(RAMWR);

  for(y=0; y < h; y++){
    for(x=0; x < w; x++){                               
      uint16_t pixel = pgm_read_word(image + i);
      i++;  
      WriteWord(pixel);
    }
    i = i - 2*originalWidth;
  }
}

void ClearScreen() {
    SetAddrWindow(0,0,XMAX,YMAX); 
    Send_Command(RAMWR);
    for (unsigned int i = 40960; i > 0; --i) {
        SPDR = 0; // initiate transfer of 0x00
        //SPI_SEND(SPDR);
        //while (!(SPSR & 0x80));
        while (!(SPSR & (1 << SPIF)));// wait until done transmitting

    }
}

void Write565 (int data, unsigned int count)
// note: inlined spi xfer for optimization
{
    for (;count>0;count--)
    {
    SPDR = (data >> 8); // write hi byte
    while (!(SPSR & 0x80)); // wait for transfer to complete
    SPDR = (data & 0xFF); // write lo byte
    while (!(SPSR & 0x80)); // wait for transfer to complete
 }
}

void FillRect (uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, int color)
{
    uint8_t hi = color >> 8;
    uint8_t lo = color;

    uint8_t width = x1-x0; // rectangle width
    uint8_t height = y1-y0; // rectangle height
    
    SetAddrWindow(x0,y0,x0+width-1,y0+height-1); // set active region
    Send_Command(RAMWR);

    for(y0=height;y0>0;y0--)
    {
     for (x0=width;x0>0;x0--)
     {
        Send_Data(hi);
        Send_Data(lo);
     }

    }

     // memory write
    Write565(color,width*height); // set color data for all pixels
}



#endif /* SPIAVR_H */