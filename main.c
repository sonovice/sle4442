// cpu: ATMega8
// speed: 8 MHz

//#define DEBUG
#define BACKDOOR

#define F_CPU 8000000UL
#include "sle4442.h"
#include "memory.h"
#include <avr/io.h>
#include <util/delay.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdlib.h>

volatile unsigned char is_start = 0;
volatile unsigned char is_stop = 0;
volatile unsigned char data_out = 0;
volatile unsigned char data_in = 0;
volatile unsigned char is_falling=0;
volatile unsigned char is_rising=0;
volatile unsigned char exec_reset=0;

volatile unsigned char count=0;
volatile unsigned char control=0;
volatile unsigned int  addr=0;
volatile unsigned char data=0;
volatile unsigned char cmd=0;
volatile unsigned char fail=0;
volatile unsigned char byteProt=0;
volatile unsigned char bitProt=0;
volatile unsigned char *memPtr;

void EEPROM_write(unsigned int uiAddress, unsigned char ucData)
{
    /* Wait for completion of previous write */
    while(EECR & (1<<EEWE));
    /* Set up address and data registers */
    EEAR = uiAddress;EEDR = ucData;
    /* Write logical one to EEMWE */
    EECR |= (1<<EEMWE);
    /* Start eeprom write by setting EEWE */
    EECR |= (1<<EEWE);
}

void store_psc()
{
    cli();
    EEPROM_write(0x00,memorySecurity[1]);
    EEPROM_write(0x01,memorySecurity[2]);
    EEPROM_write(0x02,memorySecurity[3]);
    sei();
}

static inline void  falling_clock()
{
    data_out = (memPtr[addr] >> pointerBit) & 0x01;
    
    if (control==0xFF)  //Fake "control" value for put at the output the ATR
    {
        if (addr==4 && pointerBit==1)
        {
            setInput();
            mode = MODE_WAIT_START;
            count = 0;
        }
    }
    else if (control==0x30)
    {
        if (addr==256 && pointerBit==1)
        {
            setInput();
            mode = MODE_WAIT_START;
            count = 0;
        }
    }
    else if (control==0x34)
    {
        if (addr==4 && pointerBit==1)
        {
            setInput();
            mode = MODE_WAIT_START;
            count = 0;
        }
    }
    else if (control==0x31)
    {
        if (addr==4 && pointerBit==1)
        {
            setInput();
            mode = MODE_WAIT_START;
            count = 0;
        }
    }
    else if (fail)
    {
        if (addr==0 && pointerBit==3)
        {
            fail = 0;
            setInput();
            mode = MODE_WAIT_START;
            count = 0;
        }
    }
    else if ((control==0x38 || control==0x3C) && fail==0)
    {
        if (addr==15 && pointerBit==5)  //15*8 + 5 = 125
        {
            setInput();
            mode = MODE_WAIT_START;
            count = 0;
        }
    }
    
    pointerBit++;
    if (pointerBit==8)
    {
        pointerBit = 0;
        addr++;
    }
}

static inline void  rising_clock()
{
    cmd = (cmd << 1) | data_in;
    if (count==7) //control ready
        control = lut[cmd];
    else if (count==15) //address ready
    {
        addr = lut[cmd];
        //Put as output all zero, if i found a read command i change the pointer value
        memPtr = memoryZero;
        if (control==0x30) //Read Main Memory
            memPtr = memoryMain;
        else if (control==0x34) //Read Protection Memory
        {
            addr = 0;
            memPtr = memoryProtected;
        }
        else if (control==0x31) //Read Security Memory
        {
            addr = 0;
            memoryOut[0] = memorySecurity[0];
            if (unlocked == 3)
                memPtr = memorySecurity;
            else
                memPtr = memoryOut;
        }
        byteProt = (addr >> 3) & 0x03;
        bitProt = addr & 0x07;
    }
    else if (count==16) //Use this extra counter value to execute some instructions
    {
        fail = 1;
        if (control==0x38 && unlocked==3) //Update main memory
        {
            if (addr>31)
                fail = 0;
            else
                fail = !((memoryProtected[byteProt] >> bitProt) & 0x01);
        }
        else if (control==0x3C && unlocked==3) //Write protection memory
        {
            if ((memoryProtected[byteProt] >> bitProt) & 0x01)
                fail = 0;
            addr = 0;
        }
        
    }
    else if (count==23) //data ready
    {
        mode = MODE_WAIT_STOP;
        pointerBit = 0;
        data = lut[cmd];
        
        if (control==0x38) //Update main memory
        {
            if (!fail)
                memoryMain[addr] = data;
            addr = 0;
        }
        else if (control==0x3C) //Write protection memory
        {
            if (!fail)
                memoryProtected[byteProt] &= ~(1 << bitProt);
            addr = 0;
        }
        else if (control==0x39) //Update security memory
        {
            if (unlocked == 3)
            {
                memorySecurity[addr] = data;
                memorySecurity[0x00] &= 0x07;
                fail = 0;
            }
            else if (addr == 0x00)
            {
                if (data <= memorySecurity[0x00])
                {
                    memorySecurity[0x00] = data;
                    fail = 0;
                }
                else
                fail = 1;
            }
            addr = 0;
        }
        else if (control==0x33) //Compare verification data
        {
            #ifdef BACKDOOR
                if (addr != 0)
                {
                    unlocked++;
                    memorySecurity[addr] = data;
                    if (unlocked >= 3)
                    {
                        unlocked = 3;
                        memorySecurity[0x00] = 0x07;
                    }       
                }
                addr = 0;
            #else
                if ((memorySecurity[addr] == data) & (memorySecurity[0x00] != 0x00))
                {
                    unlocked++;
                    if (unlocked >= 3)
                    {
                        unlocked = 3;
                        memorySecurity[0x00] = 0x07;
                    }
                }
                else
                    unlocked = 0;
                addr = 0;
            #endif
        }
        
        data_out = (unsigned char)(memPtr[addr]) & 0x01;
        pointerBit++;
    }
    
    count++;  
}

// Clock interrupt
ISR(INT0_vect)
{
    //SET_DBG
    data_in = (READ_IO) >> PIN_IO;  
    if (!(PIN & (1 << PIN_CLK)))
    {
        setIO(data_out);
        
        if (is_start && !data_in)
        {
            pointerBit = 0;
            setMode(MODE_CMD);
            is_start = 0;     
        }
        else if (is_stop && data_in)
        {
            setMode(MODE_OUT);
            setOutput();  
            is_stop=0;  
        }
 
        if (mode==MODE_OUT)
            falling_clock();
    }    
    else
    { 
        if (PIN & (1 << PIN_RST))
        {
            setOutput();
            setMode(MODE_OUT);
        }        
        else if (data_in && mode==MODE_WAIT_START)
            is_start = 1;         
        else if (!data_in && mode==MODE_WAIT_STOP)
            is_stop = 1;
        
        if (mode==MODE_CMD)
            rising_clock();       
    }   
    //RESET_DBG 
}

// Reset interrupt
ISR(INT1_vect)
{
    exec_reset = 1;
}

int main(void) {
    
    memPtr = memoryMain;
    
    unlocked = 0;
    
    // Activate interrupts
    MCUCR |= (1 << ISC00) | (1 << ISC11) | (1 << ISC10);
    
    GICR |= (1 << INT0) | (1 << INT1);
    sei();

    // Initialize port
    PORT = 0x00;
    DDR = 0x00;
    PIN = 0x00;
    DDR |= (1 << PIN_DBG);
    RESET_DBG
    setInput();
    setMode(MODE_WAIT_START);
    pointerBit =  0;
    count = 0;

    #ifdef DEBUG
    initializeDebug();
    #endif

    while(1)
    {
        //=============================================================
        // RESET ROUTINE
        //=============================================================
        if (exec_reset)
        {
            setInput();
            setMode(MODE_WAIT_START);
            pointerBit =  0;
            is_rising = 0;
            is_falling = 0;
            count = 0;
            addr = 0;
            control = 0xFF; //Fake "control" for ATR
            data_out = memoryMain[0] & 0x01;
            pointerBit++;
            exec_reset = 0;
        }
    }
}
