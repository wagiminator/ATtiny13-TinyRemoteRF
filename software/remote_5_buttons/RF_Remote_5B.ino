// tinyRemoteRF for ATtiny13A - 5 Buttons
// 
// RF remote control using an ATtiny 13A. This demo code implements a
// simple but robust and DC-free protocol which is easy to decode and
// does not require precise timing. It operates with ASK/OOK (Amplitude
// Shift Keying / On-Off Keyed).
//
// Pulse lengths are derived from an adjustable error width (RF_ERR).
// A "0" bit is a 2 * RF_ERR long burst and an equally long space, a
// "1" bit is a 4 * RF_ERR long burst and an equally long space. A
// start bit is a 6 * RF_ERR long burst and an equally long space.
//
//  +-------+       +---+   +-----+     +---+   +-  ON
//  |       |       |   |   |     |     |   |   |        start:  6 * ERR
//  |   6   |   6   | 2 | 2 |  4  |  4  | 2 | 2 |   ...  bit0:   2 * ERR
//  |       |       |   |   |     |     |   |   |        bit1:   4 * ERR
// -+       +-------+   +---+     +-----+   +---+   OFF
//
//  |<--- start --->|<-"0"->|<---"1"--->|<-"0"->|
//
// An RF telegram starts with the preamble in which a defined number
// of "0" bits are transmitted to wake up the receiver and allow it to
// set its automatic gain. The following start bits signify the
// start of the transmission. Afterwards three data bytes are transmitted,
// most significant bit first. The three data bytes are in order:
// - the 8-bit address of the device,
// - the 8-bit key-dependent command and
// - the 8-bit logical inverse of the command.
// After the last bit a space of at least 8 * RF_ERR signifies the end
// of the transmission. The transmission can be repeated several times.
//
// The code utilizes the sleep mode power down function. The device will
// work several months on a CR2032 battery.
//
//                        +-\/-+
// KEY5 --- A0 (D5) PB5  1|    |8  Vcc
// KEY3 --- A3 (D3) PB3  2|    |7  PB2 (D2) A1 --- KEY2
// KEY4 --- A2 (D4) PB4  3|    |6  PB1 (D1) ------ KEY1
//                  GND  4|    |5  PB0 (D0) ------ DATA
//                        +----+    
//
// Controller: ATtiny13
// Core:       MicroCore (https://github.com/MCUdude/MicroCore)
// Clockspeed: 1.2 MHz internal
// BOD:        BOD disabled (energy saving)
// Timing:     Micros disabled
//
// Reset pin must be disabled by writing respective fuse after uploading the code:
// avrdude -p attiny13 -c usbasp -U lfuse:w:0x2a:m -U hfuse:w:0xfe:m
// Warning: You will need a high voltage fuse resetter to undo this change!
//
// Note: The internal oscillator may need to be calibrated for the device
//       to function properly.
//
// 2020 by Stefan Wagner 
// Project Files (EasyEDA): https://easyeda.com/wagiminator
// Project Files (Github):  https://github.com/wagiminator
// License: http://creativecommons.org/licenses/by-sa/3.0/


// oscillator calibration value (uncomment and set if necessary)
//#define OSCCAL_VAL  0x4B

// libraries
#include <avr/io.h>
#include <avr/sleep.h>
#include <avr/interrupt.h>
#include <util/delay.h>

// pin definitions
#define DATA    PB0
#define KEY1    PB1
#define KEY2    PB2
#define KEY3    PB3
#define KEY4    PB4
#define KEY5    PB5

// RF Codes
#define ADDR    0x55  // address of the device
#define CMD1    0x01  // command KEY1
#define CMD2    0x02  // command KEY2
#define CMD3    0x03  // command KEY3
#define CMD4    0x04  // command KEY4
#define CMD5    0x05  // command KEY5

// button mask
#define BT_MASK (1<<KEY1)|(1<<KEY2)|(1<<KEY3)|(1<<KEY4)|(1<<KEY5)

// define RF error width in microseconds; must be the same in the receiver code;
// higher values reduce the error rate, but lengthen the transmission time
#define RF_ERR    150

// define number of preamble bits
#define RF_PRE    32

// define number of start bits; must be the same in the receiver code
#define RF_START  4

// define number of transmission repeats
#define RF_REP    3

// macros ASK/OOK
#define RF_on()   PORTB |=  (1<<DATA)   // key-on
#define RF_off()  PORTB &= ~(1<<DATA)   // key-off

// macros to modulate the signals with compensated timings
#define startPulse()  {RF_on(); _delay_us(6*RF_ERR); RF_off(); _delay_us(6*RF_ERR-5);}
#define bit0Pulse()   {RF_on(); _delay_us(2*RF_ERR); RF_off(); _delay_us(2*RF_ERR-5);}
#define bit1Pulse()   {RF_on(); _delay_us(4*RF_ERR); RF_off(); _delay_us(4*RF_ERR-5);}
#define stopPause()   _delay_us(6*RF_ERR) // = min 8 * RF_ERR incl. delay of last bit


// send a single byte via RF
void sendByte(uint8_t value) {
  for (uint8_t i=8; i; i--, value<<=1) {            // send 8 bits, MSB first
    (value & 0x80) ? (bit1Pulse()) : (bit0Pulse()); // send the bit
  }
}

// send complete telegram (preamble + start frame + address + command) via RF
void sendCode(uint8_t cmd) {
  uint8_t i, j;                           // countig variables
  for(i=RF_PRE;i;i--) bit0Pulse();        // send preamble
  for(i=RF_REP;i;i--) {                   // transmission repeat counter
    for(j=RF_START;j;j--) startPulse();   // signify start of transmission
    sendByte(ADDR);                       // send address byte
    sendByte(cmd);                        // send command byte
    sendByte(~cmd);                       // send inverse of command byte
    stopPause();                          // signify end of transmission
  }
}

// main function
int main(void) {
  // set oscillator calibration value
  #ifdef OSCCAL_VAL
    OSCCAL = OSCCAL_VAL;                // set the value if defined above
  #endif

  // setup pins
  DDRB  = (1<<DATA);                      // DATA pin as output
  PORTB = (BT_MASK);                      // pull-up for button pins

  // setup pin change interrupt
  GIMSK = (1<<PCIE);                      // turn on pin change interrupts
  PCMSK = (BT_MASK);                      // turn on interrupt on button pins
  sei();                                  // enable global interrupts

  // disable unused peripherals and set sleep mode to save power
  ADCSRA = 0;                             // disable ADC
  ACSR   = (1<<ACD);                      // disable analog comperator
  PRR    = (1<<PRTIM0) | (1<<PRADC);      // shut down ADC and timer0
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);    // set sleep mode to power down

  // main loop
  while(1) {
    sleep_mode();                         // sleep until button is pressed
    _delay_ms(1);                         // debounce
    uint8_t buttons = ~PINB & (BT_MASK);  // read button pins
    switch (buttons) {                    // send corresponding IR code
      case (1<<KEY1): sendCode(CMD1); break;
      case (1<<KEY2): sendCode(CMD2); break;
      case (1<<KEY3): sendCode(CMD3); break;
      case (1<<KEY4): sendCode(CMD4); break;
      case (1<<KEY5): sendCode(CMD5); break;
      default: break;
    }
  }
}

// pin change interrupt service routine
EMPTY_INTERRUPT (PCINT0_vect);            // nothing to be done here, just wake up from sleep
