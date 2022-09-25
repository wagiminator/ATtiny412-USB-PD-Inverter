// ===================================================================================
// Project:   USB Power Delivery Inverter based on ATtiny212/412
// Version:   v1.0
// Year:      2022
// Author:    Stefan Wagner
// Github:    https://github.com/wagiminator
// EasyEDA:   https://easyeda.com/wagiminator
// License:   http://creativecommons.org/licenses/by-sa/3.0/
// ===================================================================================
//
// Description:
// ------------
// The USB Power Delivery Inverter converts direct current from USB PD power supplies
// into alternating current with selectable voltage and adjustable frequency.
// For this purpose, the ATtiny's powerful Timer/Counter D (TCD) controls an H-Bridge
// with a subsequent low-pass filter.
// 
// References:
// -----------
// Sine look up table generator calculator:
// https://www.daycounter.com/Calculators/Sine-Generator-Calculator.phtml
//
// Wiring:
// -------
//                     +-\/-+
//               Vdd  1|°   |8  GND
//   WOA --- TXD PA6  2|    |7  PA3 AIN3 -------- FRQ
//   WOB --- RXD PA7  3|    |6  PA0 AIN0 UPDI --- UPDI
//       --- SDA PA1  4|    |5  PA2 AIN2 SCL ---- 
//                     +----+
//
// Compilation Settings:
// ---------------------
// Core:    megaTinyCore (https://github.com/SpenceKonde/megaTinyCore)
// Board:   ATtiny412/402/212/202
// Chip:    ATtiny212 or ATtiny412
// Clock:   20 MHz internal
//
// Leave the rest on default settings. Don't forget to "Burn bootloader". 
// Compile and upload the code.
//
// No Arduino core functions or libraries are used. To compile and upload without
// Arduino IDE download AVR 8-bit toolchain at:
// https://www.microchip.com/mplab/avr-support/avr-and-arm-toolchains-c-compilers
// and extract to tools/avr-gcc. Use the makefile to compile and upload.
//
// Fuse Settings: 0:0x00 1:0x00 2:0x02 4:0x00 5:0xC5 6:0x04 7:0x00 8:0x00
//
// Operating Instructions:
// -----------------------
// Use the DIP switches on the device to select the output voltage and AC frequency.
// Connect the device to a USB PD power adapter. As soon as the LED lights up, the
// correct output values are present. The switch positions can also be changed
// during operation.


// ===================================================================================
// Libraries, Definitions and Macros
// ===================================================================================

// Libraries
#include <avr/io.h>                 // for GPIO
#include <avr/interrupt.h>          // for interrupts

// Pin assignments
#define PIN_FRQ       PA3           // frequency selection switch
#define PIN_WOA       PA6           // H-Bridge control A
#define PIN_WOB       PA7           // H-Bridge control B

// Pin manipulation macros
enum {PA0, PA1, PA2, PA3, PA4, PA5, PA6, PA7};  // enumerate pin designators
#define pinOutput(x)  VPORTA.DIR |= (1<<(x))    // set pin to OUTPUT
#define pinRead(x)    (VPORTA.IN &  (1<<(x)))   // READ pin
#define pinPullup(x)  (&PORTA.PIN0CTRL)[x] |= PORT_PULLUPEN_bm

// ===================================================================================
// Sine Wave Table (386 values for 50Hz)
// ===================================================================================

// Calculator: https://www.daycounter.com/Calculators/Sine-Generator-Calculator.phtml
// TCD cycle length = CMPASET + CPACLR + CMPBSET + CMPBCLR + 4 = 259
// Number of values = TCD clock frequency / (TCD cycle length * output frequency)
// Number of values = 5000000Hz / (259 * frequency)
// Frequency        = 5000000Hz / (259 * number of values) 

const uint8_t SIN[] = {
  0x80,0x82,0x84,0x86,0x88,0x8a,0x8c,0x8e,0x90,0x92,0x94,0x96,0x98,0x9a,0x9c,0x9e,
  0xa0,0xa2,0xa4,0xa6,0xa8,0xaa,0xac,0xae,0xb0,0xb2,0xb4,0xb6,0xb8,0xb9,0xbb,0xbd,
  0xbf,0xc1,0xc3,0xc4,0xc6,0xc8,0xc9,0xcb,0xcd,0xce,0xd0,0xd2,0xd3,0xd5,0xd6,0xd8,
  0xd9,0xdb,0xdc,0xde,0xdf,0xe0,0xe2,0xe3,0xe4,0xe6,0xe7,0xe8,0xe9,0xea,0xeb,0xed,
  0xee,0xef,0xf0,0xf1,0xf2,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf7,0xf8,0xf9,0xf9,0xfa,
  0xfa,0xfb,0xfb,0xfc,0xfc,0xfd,0xfd,0xfd,0xfe,0xfe,0xfe,0xfe,0xff,0xff,0xff,0xff,
  0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xfe,0xfe,0xfe,0xfd,0xfd,0xfd,0xfc,0xfc,0xfb,
  0xfb,0xfa,0xfa,0xf9,0xf9,0xf8,0xf7,0xf7,0xf6,0xf5,0xf4,0xf3,0xf2,0xf2,0xf1,0xf0,
  0xef,0xee,0xed,0xeb,0xea,0xe9,0xe8,0xe7,0xe6,0xe4,0xe3,0xe2,0xe0,0xdf,0xde,0xdc,
  0xdb,0xd9,0xd8,0xd6,0xd5,0xd3,0xd2,0xd0,0xce,0xcd,0xcb,0xc9,0xc8,0xc6,0xc4,0xc3,
  0xc1,0xbf,0xbd,0xbb,0xb9,0xb8,0xb6,0xb4,0xb2,0xb0,0xae,0xac,0xaa,0xa8,0xa6,0xa4,
  0xa2,0xa0,0x9e,0x9c,0x9a,0x98,0x96,0x94,0x92,0x90,0x8e,0x8c,0x8a,0x88,0x86,0x84,
  0x82,0x80,0x7d,0x7b,0x79,0x77,0x75,0x73,0x71,0x6f,0x6d,0x6b,0x69,0x67,0x65,0x63,
  0x61,0x5f,0x5d,0x5b,0x59,0x57,0x55,0x53,0x51,0x4f,0x4d,0x4b,0x49,0x47,0x46,0x44,
  0x42,0x40,0x3e,0x3c,0x3b,0x39,0x37,0x36,0x34,0x32,0x31,0x2f,0x2d,0x2c,0x2a,0x29,
  0x27,0x26,0x24,0x23,0x21,0x20,0x1f,0x1d,0x1c,0x1b,0x19,0x18,0x17,0x16,0x15,0x14,
  0x12,0x11,0x10,0x0f,0x0e,0x0d,0x0d,0x0c,0x0b,0x0a,0x09,0x08,0x08,0x07,0x06,0x06,
  0x05,0x05,0x04,0x04,0x03,0x03,0x02,0x02,0x02,0x01,0x01,0x01,0x01,0x00,0x00,0x00,
  0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x01,0x01,0x01,0x01,0x02,0x02,0x02,0x03,0x03,
  0x04,0x04,0x05,0x05,0x06,0x06,0x07,0x08,0x08,0x09,0x0a,0x0b,0x0c,0x0d,0x0d,0x0e,
  0x0f,0x10,0x11,0x12,0x14,0x15,0x16,0x17,0x18,0x19,0x1b,0x1c,0x1d,0x1f,0x20,0x21,
  0x23,0x24,0x26,0x27,0x29,0x2a,0x2c,0x2d,0x2f,0x31,0x32,0x34,0x36,0x37,0x39,0x3b,
  0x3c,0x3e,0x40,0x42,0x44,0x46,0x47,0x49,0x4b,0x4d,0x4f,0x51,0x53,0x55,0x57,0x59,
  0x5b,0x5d,0x5f,0x61,0x63,0x65,0x67,0x69,0x6b,0x6d,0x6f,0x71,0x73,0x75,0x77,0x79,
  0x7b,0x7d
};

// ===================================================================================
// Main Function
// ===================================================================================

uint16_t SIN_ptr = 0;                             // sine wave table pointer
uint8_t  SIN_val = 0x80;                          // sine wave value
uint8_t  cnt60   = 5;                             // 60Hz correction counter

int main(void) {
  _PROTECTED_WRITE(CLKCTRL.MCLKCTRLB, 0);         // set clock frequency to 20 MHz
  pinOutput(PIN_WOA); pinOutput(PIN_WOB);         // enable output on WOA/WOB pin
  pinPullup(PIN_FRQ);                             // pullup on frequency selection pin
  TCD0.CTRLB     = TCD_WGMODE_FOURRAMP_gc;        // set four ramp mode
  TCD0.CMPASET   = 0x03;                          //   200ns dead time
  TCD0.CMPACLR   = 0x80;                          // 25800ns on-time WOA
  TCD0.CMPBSET   = 0x03;                          //   200ns dead time
  TCD0.CMPBCLR   = 0x7f;                          // 25600ns on-time WOB
  CPU_CCP        = CCP_IOREG_gc;                  // protected write is coming
  TCD0.FAULTCTRL = TCD_CMPAEN_bm                  // enable WOA output channel
                 | TCD_CMPBEN_bm;                 // enable WOB output channel
  TCD0.INTCTRL   = TCD_OVF_bm;                    // enable overflow interrupt
  while(~TCD0.STATUS & TCD_ENRDY_bm);             // wait for synchronization
  TCD0.CTRLA     = TCD_CLKSEL_20MHZ_gc            // select 20MHz base clock
                 | TCD_CNTPRES_DIV4_gc            // prescaler 4 -> 5MHz timer clock
                 | TCD_ENABLE_bm;                 // enable timer
  sei();                                          // enable interrupts
  while(1);                                       // everything is done via interrupt
}

// ===================================================================================
// TCD Interrupt Service Routine
// ===================================================================================

ISR(TCD0_OVF_vect) {
  TCD0.CMPACLRL =  SIN_val;                       // set next sine value for WOA
  TCD0.CMPBCLRL = -SIN_val;                       // inverse value for WOB
  TCD0.CTRLE    = TCD_SYNCEOC_bm;                 // sync registers at TCD cycle end
  TCD0.INTFLAGS = TCD_OVF_bm;                     // clear interrupt flag
  if(++SIN_ptr >= sizeof(SIN)) SIN_ptr = 0;       // increase sine pointer
  if(!pinRead(PIN_FRQ) && !--cnt60) {             // correction for 60Hz?
    if(++SIN_ptr >= sizeof(SIN)) SIN_ptr = 0;     // -> skip one sine value
    cnt60 = 5;                                    // reset counter
  }
  SIN_val = SIN[SIN_ptr];                         // get next sine value
}
