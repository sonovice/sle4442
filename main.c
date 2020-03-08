// cpu: ATMega8
// speed: 8 MHz

//#define DEBUG
#define BACKDOOR
//#define WRITE_PSC

#define F_CPU 8000000UL
#include "sle4442.h"
#include "memory.h"

volatile char is_start = 0;
volatile char is_stop = 0;

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

static inline void edgeFalling() {
    switch (mode) {
        case MODE_LAST_BIT:
            setMode(MODE_WAIT_STOP); 
        break;   
        case MODE_ATR:
            // Answer-to-Reset
            if (pointerByte <= 3)
            {
                setOutput();
                setIO(memoryMain[pointerByte] & (1 << pointerBit));
            } 
            else 
            { // ATR finished
                setInput();
                setMode(MODE_WAIT_START);
            }
        break;
        case MODE_DATA:
        // Outgoing Data Mode
        switch (command[0]) 
        {
            case 0x30: // Read Main Memory
                if (pointerByte <= 255) 
                {
                    setIO(memoryMain[pointerByte] & (1 << pointerBit));
                    setOutput();
                } 
                else 
                { // Reading finished
                    setInput();
                    setMode(MODE_WAIT_START);
                }
            break;
            case 0x34: // Read Protection Memory
                if (pointerByte <= 3) 
                {
                    setIO(memoryProtected[pointerByte * 8 + pointerBit]);
                    setOutput();
                } 
                else 
                { // Reading finished
                    setInput();
                    setMode(MODE_WAIT_START);
                }
            break;
            case 0x31: // Read Security Memory
                if (pointerByte <= 3) 
                {
                    if ((unlocked == 3) | (pointerByte == 0x00))
                        setIO(memorySecurity[pointerByte] & (1 << pointerBit));
                    else
                        setIO(0);
                        setOutput();
                } 
                else 
                { // Reading finished
                    setInput();
                    setMode(MODE_WAIT_START);
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
                if (unlocked == 3) 
                {
                    memorySecurity[command[1]] = command[2];
                    memorySecurity[0x00] &= 0x07;
                    waitCycles(124);
                } 
                else if ((command[1] == 0x00) & (command[2] < memorySecurity[0x00]))
                {
                    memorySecurity[0x00] = command[2];
                    waitCycles(124);
                }   
                else if (((command[1] == 0x00) & (command[2] > memorySecurity[0x00])) | (command[1] != 0x00))
                {
                    waitCycles(2);
                }        
                setInput();
                setMode(MODE_WAIT_START);
            break;
            case 0x33: // Compare Verification Data
                setIO(0);
                setOutput();
                #ifdef BACKDOOR
                    memorySecurity[command[1]] = command[2];
                    unlocked++;
                    if (unlocked >= 3)
                    {
                        unlocked = 3;
                        memorySecurity[0x00] = 0x07;
                        #ifdef WRITE_PSC
                            store_psc();
                        #endif                          
                    }
                #else
                    if ((memorySecurity[command[1]] == command[2]) & (memorySecurity[0x00] != 0x00))
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
                #endif
                waitCycles(2);
                setInput();
                setMode(MODE_WAIT_START);
            break;
            case 0x3C: // Write Protection Memory     
                setIO(0);
                setOutput();           
                if ((unlocked == 3) & (memoryMain[command[1]] == command[2])) 
                {
                    if (memoryProtected[command[1]] == 0)
                        waitCycles(2);
                    else
                    {
                        memoryProtected[command[1]] = 0;
                        waitCycles(124);    
                    }     
                }   
                else
                    waitCycles(2);
                setInput();
                setMode(MODE_WAIT_START);
            break;
            case 0x38: // Update Main Memory
                setIO(0);
                setOutput();
                if (unlocked == 3)
                {
                    if ((command[1] <= 0x1f) & (memoryProtected[command[1]] == 0))
                        waitCycles(2);     
                    else
                    {
                        memoryMain[command[1]] = command[2];
                        waitCycles(124);    
                    }    
                }
                else
                    waitCycles(2);                
                setInput();
                setMode(MODE_WAIT_START);
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
        break;
        case MODE_CMD:
            // Command Mode
            if (pointerByte <= 2) 
            {
                if (bit)
                    command[pointerByte] |= (1 << pointerBit);
                else
                    command[pointerByte] &= ~(1 << pointerBit);
                if (pointerByte==2 && pointerBit==7)
                    setMode(MODE_LAST_BIT);    
            }  
        break;
        case MODE_WAIT_STOP:
            pointerByte = 0;
            pointerBit = 0;
            switch (command[0]) {
                case 0x30: // Read Main Memory
                    pointerByte = command[1];
                case 0x34: // Read Protection Memory
                case 0x31: // Read Security Memory
                    mode_temp = MODE_DATA;
                break;
                case 0x39: // Update Security Memory
                case 0x33: // Compare Verification Data
                case 0x3C: // Write Protection Memory
                case 0x38: // Update Main Memory
                    mode_temp = MODE_PROC;
                break;
            }           
        break;
    }
}

// Clock interrupt
ISR(INT0_vect)
{
    if (!(PIN & (1 << PIN_CLK)))
    {
        if (in_mode && mode==MODE_WAIT_START && is_start)
        {     
            if ((READ_IO)==0)
            {
                pointerByte = 0;
                pointerBit = -1;
                setMode(MODE_CMD);
            }                
            is_start = 0;  
        }  
        else if (in_mode && mode==MODE_WAIT_STOP && is_stop)
        {
            if (READ_IO)
                setMode(mode_temp);
            is_stop=0;
        }
             
        edgeFalling();
    }    
    else
    { 
        if (PIN & (1 << PIN_RST))
            setMode(MODE_ATR);
        else if (in_mode && mode==MODE_WAIT_START && !is_start)
        {
            if (READ_IO)
                is_start = 1;               
        }
        else if (in_mode && mode==MODE_WAIT_STOP && !is_stop)
        {
            if ((READ_IO)==0)
                is_stop = 1;
        }
        edgeRising(READ_IO);         
             
    }    
}

// Reset interrupt
ISR(INT1_vect)
{
    setInput();
    setMode(MODE_WAIT_START);
    pointerByte = 0;
    pointerBit = -1;
}

int main(void) {
    unlocked = 0;
    
    // Activate interrupts
    MCUCR |= (1 << ISC00) | (1 << ISC11) | (1 << ISC10);
    GICR |= (1 << INT0) | (1 << INT1);
    sei();

    // Initialize port
    PORT = 0x00;
    DDR = 0x00;
    PIN = 0x00;
    setInput();
    setMode(MODE_WAIT_START);
    pointerByte = 0;
    pointerBit = -1;

    #ifdef DEBUG
    initializeDebug();
    #endif

    for (;;) {
        ;
    }
}
