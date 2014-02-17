#include <wixel.h>
#include <usb.h>
#include <usb_com.h>
#include <stdio.h>

void updateBitBuffer(void);

#define LED_DMA (dmaConfig._4)
#define DMA_CHANNEL_LED 4

//#define INVERT // invert output to drive NMOS gate (level-shift to 5 V)

// units of 1/24 us
#define PERIOD 30
#define LED_BIT_L 10
#define LED_BIT_H 19

#define LED_BIT(v) ((v) ? LED_BIT_H : LED_BIT_L)

// Half speed, we've halfed the prescaler so all good
#define T0H 6
#define T1H 14


#define LED_COUNT 16
#define LED_DATA_BITS  (LED_COUNT*24)

uint8 XDATA bitBuffer[LED_DATA_BITS+2];

// NOTE: This code produces a 1-period glitch (high pulse) at the end of the data transfer.
// It will probably be hard to fix that glitch, because it will require a new interrupt to be used.

/**
We run two timers. T3CC0 is set to 30/24, or 1.25us. This is equal to the time to send one bit to the strip.
30/12 = 2.5
T3CC0 is setup so it outputs on P1_4 as soon as it hits the end of a 1.25us period, giving us the high portion of the output.

Meanwhile T3CC1 is setup to clear the output once it hits its setpoint. It will drive high once it rolls over to 0.
The timer is started.

The DMA controller is setup to take values from an array of GRB values; but they are timing values.
It's set to single shot mode and triggers when T3CC0 rolls over (every 1.25us).

It takes a value from the array and puts it into T3CC1, which sets the compare period of CC1.

Each byte in the array is set to either 10 or 19, which is either 0.42us or 0.791us.
A zero bit is represented by 0.4us high + 0.85us low.
A 1 bit is 0.8us high + 0.45 low.
So if CC1 is set to 10, the CC1 timer overflows at 0.42us and drag low.
If CC1 is set to 19, we overflow at 0.791us and drag low.
Once T3CC0 rolls over, we trigger the DMA to move to the next byte and do the next transfer.
*/

void ledStripInit() // Would disable interrupts -> __critical
{
    // uint16 i;
    // for(i = 1; i < sizeof(bitBuffer)-1; i ++)
    // {
    //     bitBuffer[i] = LED_BIT_L;
    // }
    // bitBuffer[0] = 255;
    // bitBuffer[sizeof(bitBuffer)-1] = 0;

    P1_4 = 0;
    P1DIR |= (1<<4); // 0001 -> 10000, bitmask add // P1_4 output
    P1SEL |= (1<<4);  // Assign P1_4 to a peripheral function instead of GPIO.
	P2SEL |= (1<<5); // Give timer3 precendence over uart

    // Set Timer 3 modulo mode (period is set by T3CC0)
    // Set Timer 3 Channel 1 (P1_4) to output compare mode.
	
    T3CC0 = PERIOD - 1;   // Set the period; we want the counter to reset to 0 on the tick after it reaches (PERIOD - 1)
	// T3CC0 will now reset every 1.25us
    T3CC1 = 0;            // Set the duty cycle.
    T3CCTL0 = 0b00000100; // Enable the channel 0 compare, which triggers the DMA at the end of every period.
	// 0 -> not used, 0 -> interrupts off, 000 -> set output on compare, 1 -> enable output on compare, 00 -> reserved

    T3CCTL1 = 0b00100100; // T3CH1: Interrupt disabled, clear output on compare up, set on 0.
	
    T3CTL = 0b00110010;   // Start the timer with Prescaler 1:2, modulo mode (counts from 0 to T3CC0).
	
	// 1.25us, T3CCTL0 goes high, 

    LED_DMA.SRCADDRH = (unsigned int)bitBuffer >> 8;
    LED_DMA.SRCADDRL = (unsigned int)bitBuffer;
    LED_DMA.DESTADDRH = XDATA_SFR_ADDRESS(T3CC1) >> 8;
    LED_DMA.DESTADDRL = XDATA_SFR_ADDRESS(T3CC1);
	
    LED_DMA.LENL =  (uint8)sizeof(bitBuffer);
	LED_DMA.VLEN_LENH = (uint8)(sizeof(bitBuffer) >> 8);
	
    //LED_DMA.VLEN_LENH = (uint8)(sizeof(bitBuffer) >> 8); // 10000010 -> 00000001
	// 000 -> Use LEN for xfer count, 0000001 -> High bits of LEN
	// LEN = 386
	
	// 386 -> 9 bits
	// 770 -> 1100000010, 10 bits
	
    LED_DMA.DC6 = 0b00000111; // WORDSIZE = 0 (8-bit), TMODE = 0 (Single), TRIG = 7 (Timer 3, compare, channel 0)
    LED_DMA.DC7 = 0b01000010; // SRCINC = 1, DESTINC = 0, IRQMASK = 0, M8 = 0, PRIORITY = 2 (High)

    // We found that a priority of 1 (equal to the CPU) also works.

}

void ledStripService()
{
        // Start the transfer of data to the LED strip.
        // It will finish in about LED_DATA_BITS * PERIOD / 24 microseconds.
        DMAARM |= (1 << DMA_CHANNEL_LED);
}


void updateHalfBitBuffer()
{
    //uint8 time = (getMs() >> 5) & 0xFF;
    uint8 i, j;

    for(i = 0; i < LED_COUNT; i++)
    {
        for(j = 0; j < 8; j++)
        {
            bitBuffer[1 + 24*i + j] = T1H; // G
            bitBuffer[1 + 24*i + 8 + j] = T0H; // R
            bitBuffer[1 + 24*i + 16 + j] = T0H; // B
        }
    }
	
	bitBuffer[0] = 255;
	bitBuffer[sizeof(bitBuffer)-1] = 0;
}

void updateBitBuffer()
{
    //uint8 time = (getMs() >> 5) & 0xFF;
    uint8 i, j;

    for(i = 0; i < LED_COUNT; i++)
    {
        for(j = 0; j < 8; j++)
        {
            bitBuffer[1 + 24*i + j] = LED_BIT(1); // G
            bitBuffer[1 + 24*i + 8 + j] = LED_BIT(0); // R
            bitBuffer[1 + 24*i + 16 + j] = LED_BIT(0); // B
        }
    }
	
	bitBuffer[sizeof(bitBuffer)-1] = 0;
}

void updateBitBufferRed()
{
    //uint8 time = (getMs() >> 5) & 0xFF;
    uint8 i, j;

    for(i = 0; i < LED_COUNT; i++)
    {
        for(j = 0; j < 8; j++)
        {
            bitBuffer[1 + 24*i + j] = LED_BIT(0); // G
            bitBuffer[1 + 24*i + 8 + j] = LED_BIT(1); // R
            bitBuffer[1 + 24*i + 16 + j] = LED_BIT(0); // B
        }
    }
	
		bitBuffer[sizeof(bitBuffer)-1] = 0;
}

void updateBitBufferBlue()
{
    //uint8 time = (getMs() >> 5) & 0xFF;
    uint8 i, j;

    for(i = 0; i < LED_COUNT; i++)
    {
        for(j = 0; j < 8; j++)
        {
            bitBuffer[1 + 24*i + j] = LED_BIT(0); // G
            bitBuffer[1 + 24*i + 8 + j] = LED_BIT(0); // R
            bitBuffer[1 + 24*i + 16 + j] = LED_BIT(1); // B
        }
    }
	
	bitBuffer[sizeof(bitBuffer)-1] = 0;
}

void main()
{
    systemInit();
    usbInit();
	
	LED_RED(1);
	
	updateHalfBitBuffer();
	ledStripInit();

	ledStripService();

    
    while(1)
    {
		boardService();
        usbComService();
		
        usbShowStatusWithGreenLed();
		
		LED_YELLOW_TOGGLE();
		
		//ledStripInit();
		//updateHalfBitBuffer();
		//ledStripService();
    }
}
