#include <wixel.h>
#include <usb.h>
#include <usb_com.h>
#include <stdio.h>

void updateBitBuffer(void);

#define LED_DMA (dmaConfig._2)
#define DMA_CHANNEL_LED 2

//#define INVERT // invert output to drive NMOS gate (level-shift to 5 V)

// units of 1/24 us
#define PERIOD 30
#define LED_BIT_L 10
#define LED_BIT_H 19

#define LED_BIT(v) ((v) ? LED_BIT_H : LED_BIT_L)

#define LED_COUNT 17
#define LED_DATA_BITS  (LED_COUNT*24)



volatile uint8 XDATA bitBuffer[LED_DATA_BITS+2];

// NOTE: This code produces a 1-period glitch (high pulse) at the end of the data transfer.
// It will probably be hard to fix that glitch, because it will require a new interrupt to be used.
void ledStripInit()
{
    uint16 i;
    for(i = 1; i < sizeof(bitBuffer)-1; i ++)
    {
        bitBuffer[i] = LED_BIT_L;
    }
    bitBuffer[0] = 255;
    bitBuffer[sizeof(bitBuffer)-1] = 0;

    P1_4 = 0;
    P1DIR |= (1<<4);
    P1SEL |= (1<<4);  // Assign P1_4 to a peripheral function instead of GPIO.

    // Set Timer 3 modulo mode (period is set by T3CC0)
    // Set Timer 3 Channel 1 (P1_4) to output compare mode.
    T3CC0 = PERIOD - 1;   // Set the period; we want the counter to reset to 0 on the tick after it reaches (PERIOD - 1)
    T3CC1 = 0;            // Set the duty cycle.
    T3CCTL0 = 0b00000100; // Enable the channel 0 compare, which triggers the DMA at the end of every period.
#ifdef INVERT
    T3CCTL1 = 0b00011100; // T3CH1: Interrupt disabled, set output on compare up, clear on 0.
#else
    T3CCTL1 = 0b00100100; // T3CH1: Interrupt disabled, clear output on compare up, set on 0.
#endif
    T3CTL = 0b00010010;   // Start the timer with Prescaler 1:1, modulo mode (counts from 0 to T3CC0).

    LED_DMA.SRCADDRH = (unsigned int)bitBuffer >> 8;
    LED_DMA.SRCADDRL = (unsigned int)bitBuffer;
    LED_DMA.DESTADDRH = XDATA_SFR_ADDRESS(T3CC1) >> 8;
    LED_DMA.DESTADDRL = XDATA_SFR_ADDRESS(T3CC1);
    LED_DMA.LENL = (uint8)sizeof(bitBuffer);
    LED_DMA.VLEN_LENH = (uint8)(sizeof(bitBuffer) >> 8);
    LED_DMA.DC6 = 0b00000111; // WORDSIZE = 0 (8-bit), TMODE = 0 (Single), TRIG = 7 (Timer 3, compare, channel 0)
    LED_DMA.DC7 = 0b01000001; // SRCINC = 1, DESTINC = 0, IRQMASK = 0, M8 = 0, PRIORITY = 2 (High)

    // We found that a priority of 1 (equal to the CPU) also works.
	LED_YELLOW(1);

}

void ledStripService()
{
    static uint32 lastTime = 0;

    //if (getMs() - lastTime >= 30)
    {
        lastTime = getMs();
        updateBitBuffer();

        // Start the transfer of data to the LED strip.
        // It will finish in about LED_DATA_BITS * PERIOD / 24 microseconds.
        DMAARM |= (1 << DMA_CHANNEL_LED);
    }
}

void updateBitBuffer()
{
    uint8 time = (getMs() >> 5) & 0xFF;
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
}

void main()
{
    systemInit();
    usbInit();
    ledStripInit();

    ledStripService();
    
    while(1)
    {
        usbComService();
        usbShowStatusWithGreenLed();
        //LED_YELLOW(getMs() >> 9 & 1);
        boardService();
    }
}
