// cpu: ATMega8
// speed: 8 MHz

//#define DEBUG

#include "sle4442.h"
#include "memory.h"

static inline void edgeFalling() {
	switch (mode) {
		case MODE_ATR:
			// Answer-to-Reset
			if (pointerByte <= 3) {
				setOutput();
				setIO(memoryMain[pointerByte] & (1 << pointerBit));
			} else { // ATR finished
				setInput();
				setMode(MODE_IDLE);
			}
			break;
		case MODE_DATA:
			// Outgoing Data Mode
			switch (command[0]) {
				case 0x30: // Read Main Memory
					if (pointerByte <= 255) {
						setIO(memoryMain[pointerByte] & (1 << pointerBit));
						setOutput();
					} else { // Reading finished
						setInput();
						setMode(MODE_IDLE);
					}
					break;
				case 0x34: // Read Protection Memory
					if (pointerByte <= 3) {
						setIO(memoryProtected[pointerByte * 8 + pointerBit]);
						setOutput();
					} else { // Reading finished
						setInput();
						setMode(MODE_IDLE);
					}
					break;
				case 0x31: // Read Security Memory
					if (pointerByte <= 3) {
						if ((unlocked == 3) | (pointerByte == 0x00))
							setIO(memorySecurity[pointerByte] & (1 << pointerBit));
						else
							setIO(0);
						setOutput();
					} else { // Reading finished
						setInput();
						setMode(MODE_IDLE);
					}
					break;
			}
			break;
		case MODE_PROC:
			// Processing Mode
			switch (command[0]) {
				case 0x39: // Update Security Memory
					setIO(0);
					setOutput();
					if (unlocked == 3) {
						memorySecurity[command[1]] = command[2];
						memorySecurity[0x00] &= 0x07;
					} else if ((command[1] == 0x00) & (command[2] < memorySecurity[0x00]))
						memorySecurity[0x00] = command[2];
					waitCycles(124);
					setInput();
					setMode(MODE_IDLE);
					break;
				case 0x33: // Compare Verification Data
					setIO(0);
					setOutput();
					if ((memorySecurity[command[1]] == command[2]) & (memorySecurity[0x00] != 0x00)) {
						unlocked++;
						if (unlocked >= 3) {
							unlocked = 3;
							memorySecurity[0x00] = 0x07;
						}
					} else
						unlocked = 0;
					waitCycles(2);
					setInput();
					setMode(MODE_IDLE);
					break;
				case 0x3C: // Write Protection Memory
					setIO(0);
					setOutput();
					if ((unlocked == 3) & (memoryMain[command[1]] == command[2])) {
						memoryProtected[command[1]] = 1;
					}
					waitCycles(124);
					setInput();
					setMode(MODE_IDLE);
					break;
				case 0x38: // Update Main Memory
					setIO(0);
					setOutput();
					if (unlocked == 3)
						memoryMain[command[1]] = command[2];
					waitCycles(124);
					setInput();
					setMode(MODE_IDLE);
					break;
			}
			break;
	}
}

static inline void edgeRising(bool bit) {
	// Increase pointers
	pointerBit++;
	if (pointerBit > 7) {
		pointerByte++;
		pointerBit = 0;
	}

	switch (mode) {
		case MODE_IDLE:
			// Idle Mode - Waiting for command
			loop_until_bit_is_clear(PIN,PIN_IO);
			pointerByte = 0;
			pointerBit = -1;
			setMode(MODE_CMD);
			break;
		case MODE_CMD:
			// Command Mode
			if (pointerByte <= 2) {
				if (bit)
					command[pointerByte] |= (1 << pointerBit);
				else
					command[pointerByte] &= ~(1 << pointerBit);
			} else {
				loop_until_bit_is_set(PIN,PIN_IO);
				pointerByte = 0;
				pointerBit = 0;
				switch (command[0]) {
					case 0x30: // Read Main Memory
						pointerByte = command[1];
					case 0x34: // Read Protection Memory
					case 0x31: // Read Security Memory
						setMode(MODE_DATA);
						break;
					case 0x39: // Update Security Memory
					case 0x33: // Compare Verification Data
					case 0x3C: // Write Protection Memory
					case 0x38: // Update Main Memory
						setMode(MODE_PROC);
						break;
				}
			}
			break;
	}
}

// Clock interrupt
ISR(INT0_vect)
{
	if (!(PIN & (1 << PIN_CLK)))
		edgeFalling();
	else
		edgeRising(PIN & (1 << PIN_IO));
}

// Reset interrupt
ISR(INT1_vect)
{
	setMode(MODE_ATR);
	pointerByte = 0;
	pointerBit = 0;
	unlocked = 0;
	edgeFalling();
}

int main(void) {
	// Activate interrupts
	MCUCR |= (1 << ISC00) | (1 << ISC11);
	GICR |= (1 << INT0) | (1 << INT1);
	sei();

	// Initialize port
	PORT = 0x00;
	DDR = 0x00;
	PIN = 0x00;
	setMode(MODE_ATR);

#ifdef DEBUG
	initializeDebug();
#endif

	for (;;) {
		;
	}
}
