# SLE4442 Emulator
This is a smartcard emulator for ATMEGA8 microcontroller. It is a fork from https://github.com/sonovice/sle4442, with some bugs fixed.

## Schematic
![Schematic](https://i.imgur.com/YaTSVsc.png)

## Instructions
- Modify the "memory.h" file with the data of the card that you want to emulate
- If you compile the file with "#define BACKDOOR" uncommented, you will emulate a card that will be unlocked using any PSC code. Also, the PSC code sent by the reader will be updated immediately on the Security memory.  
- **WARNING**  
The maximum input clock frequency will be around 35KHz, not the 50KHz specified in the SLE4442 datasheet. I've tried my best to optimize the code but this is the main limitation of this project, you can try to use an external 16MHz clock for a better performance.

## Bugs fixed
- In a real SLE4442, an ATR is sent only if clock is pulsed when reset is high, otherwise a simple reset happens. This behavior is not present in the **sonovice** code, so i change the interrupr logic to reflect this behavior.
- Protection memory behaviour was not correct.
- Inserted also a "wrong" response for all the commands, so the emulation is very similar to a real card 

## To be done
- Support also small AVR MCU like Attiny85 (ATMEGA8 has a lot of unused pins)
- Insert a support for reading/writing to a real EEPROM in order to make the emulator behave like a "real" card.
