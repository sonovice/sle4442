#include <avr/io.h>
#include <avr/interrupt.h>
#include <stdbool.h>

// Global variables
volatile unsigned char mode;
volatile unsigned char unlocked = 0;
volatile unsigned char in_mode = 0;
volatile unsigned char mode_temp=0;

// Pin/Port configuration
#define PIN_CLK		PD2	// INT0
#define PIN_RST		PD3 // INT1
#define PIN_IO		PD4
#define PORT		PORTD
#define PIN			PIND
#define DDR 		DDRD

#define READ_IO     PIN & (1 << PIN_IO)

// Some helpful functions
static inline void setOutput() {
	DDR |= (1 << PIN_IO);
    in_mode = 0;
}
static inline void setInput() {
	DDR &= ~(1 << PIN_IO);
    in_mode = 1;
}
static inline void setIO(bool b) {
	if (b)
		PORT |= (1 << PIN_IO);
	else
		PORT &= ~(1 << PIN_IO);
}
static inline void setMode(unsigned char m) {
	mode = m;
}
static inline void waitCycles(unsigned char c) {
	for (unsigned char i = 0; i < c; i++) {
		loop_until_bit_is_set(PIN,PIN_CLK);
		loop_until_bit_is_clear(PIN,PIN_CLK);
	}
}

// Mode constants
#define MODE_ATR	1
#define MODE_CMD	2
#define MODE_DATA	3
#define MODE_PROC	4
#define MODE_IDLE	5
#define MODE_WAIT_START	6
#define MODE_WAIT_STOP	7
#define MODE_LAST_BIT 8

// Memory pointers
volatile unsigned int pointerByte = 0;
volatile signed char pointerBit = 0;


// Command bytes
unsigned char command[3];

#ifdef DEBUG
// DEBUG
static inline void initializeDebug(){
	DDRD |= (1 << PD1);
}
static inline void debugOn(){
	PORTD |= (1 << PD1);
}
static inline void debugOff(){
	PORTD &= ~(1 << PD1);
}
static inline void debugToggle(){
	PORTD ^= (1 << PD1);
}
#endif

