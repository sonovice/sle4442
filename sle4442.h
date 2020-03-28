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
#define PIN_DBG  	PD7
#define PORT		PORTD
#define PIN			PIND
#define DDR 		DDRD

#define READ_IO     PIN & (1 << PIN_IO)
#define SET_DBG     PORT |= (1 << PIN_DBG);
#define RESET_DBG   PORT &= ~(1 << PIN_DBG);

// Some helpful functions
static inline void setOutput() {
	DDR |= (1 << PIN_IO);
    in_mode = 0;
}
static inline void setInput() {
	DDR &= ~(1 << PIN_IO);
    PORT &= ~(1 << PIN_IO);
    in_mode = 1;
}
static inline void setIO(unsigned char b) {
	if (b==0)
		PORT &= ~(1 << PIN_IO);
	else
        PORT |= (1 << PIN_IO);
}
static inline void setMode(unsigned char m) {
	mode = m;
}

// Mode constants
#define MODE_ATR	1
#define MODE_CMD	2
#define MODE_WAIT_START	3
#define MODE_WAIT_STOP	4
#define MODE_OUT	5

// Memory pointers
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

