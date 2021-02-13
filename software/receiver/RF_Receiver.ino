// Receiver Example Code for TinyRemoteRF
//
// Received codes from TinyRemoteRF are sent via serial interface.
//
// Pulse lengths are derived from an adjustable error width (RF_ERR).
// A "0" bit is a 2 * RF_ERR long burst and an equally long space, a
// "1" bit is a 4 * RF_ERR long burst and an equally long space. The
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
// An RF telegram starts with the preamble byte to wake up the receiver
// and allow it to set its automatic gain. The following start bits
// signifies the start of the transmission. Afterwards three data bytes
// are transmitted, most significant bit first. The three data bytes
// are in order:
// - the 8-bit address of the device,
// - the 8-bit key-dependent command and
// - the 8-bit logical inverse of the command.
// After the last bit a space of at least 8 * RF_ERR signifies the end
// of the transmission.

#define DATA_PIN  10    // pin connected to receiver module
#define RF_ERR    150   // must be the same value as in the transmitter code
#define RF_START  4     // must be the same value as in the transmitter code

uint16_t duration;

void setup() {
  pinMode(DATA_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
}

void loop() {
  // turn off LED
  digitalWrite(LED_BUILTIN, LOW);

  // wait for start bits
  uint8_t counter = RF_START;
  do {
    duration = pulseIn(DATA_PIN, HIGH, 14 * RF_ERR);
    if ((duration < (5 * RF_ERR)) || (duration > (7 * RF_ERR))) counter = 5;
  } while(--counter);
  delayMicroseconds(2 * RF_ERR);

  // turn on LED
  digitalWrite(LED_BUILTIN, HIGH);

  // read bytes
  uint32_t data = 1;
  for(uint8_t i=24; i; i--) {
    duration = pulseIn(DATA_PIN, HIGH, 10 * RF_ERR);
    if ((duration < RF_ERR) || (duration > (5 * RF_ERR))) break;
    data <<= 1;
    if (duration > (3 * RF_ERR)) data |= 1;
  }

  // check reading
  if (data & 0x01000000) {
    Serial.println("Telegram received:");
    Serial.print("Address: "); Serial.println((data >> 16) & 0xFF, HEX);
    Serial.print("Command: "); Serial.println((data >>  8) & 0xFF, HEX);
    Serial.print("Inverse: "); Serial.println( data        & 0xFF, HEX);
    if(((data >> 8) & 0xFF) == (~data & 0xFF)) Serial.println("Valid Code Reading");
    else                                       Serial.println("Invalid Code Reading");
  }
  else Serial.println("Incomplete Reading !");
}
