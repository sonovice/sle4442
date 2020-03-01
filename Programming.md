# Programming instruction
For Windows, download WinAVR from https://sourceforge.net/projects/winavr/  
For Linux, install the required files using this command:
```
sudo apt-get install gcc-avr avr-libc avrdude
```

## Compile the code
Put all the files (**main.c memory.h sle4442.h**) in one folder. Execute this command:
```
avr-gcc -Wall -g -Os -mmcu=atmega8 -std=gnu99 -o main.bin main.c
```

## Generate .hex file
```
avr-objcopy -j .text -j .data -O ihex main.bin main.hex
```

<br/>The following commands are for USBasp programmer. For other programmers, look at the avrdude documentation.  

## Writing fuses
Execute those commands only one time, no need to write fuses every time you burn the flash.
```
avrdude -p atmega8 -c usbasp -U lfuse:w:0xC4:m
avrdude -p atmega8 -c usbasp -U hfuse:w:0xD9:m
```

## Burning
```
avrdude -p atmega8 -c usbasp -U flash:w:main.hex:i -F -P usb
```

## Read EEPROM
Once the PSC is grabbed, you can read the microcontroller EEPROM using this command:
```
avrdude -p atmega8 -c usbasp -U eeprom:r:eeprom.hex:i
```

Open **eeprom.hex** with a text editor, PSC is present here:
![](https://i.imgur.com/WLMl7ar.png)
