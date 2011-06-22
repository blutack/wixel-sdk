/* spectrum analyzer:
*
* The receivers(s) (the Wixel(s) loaded with spectrum_analyzer) will
* report the signal strength on all 256 channels.
*
* Average and maximum signal strength is reported in dBm on the USB serial connection.
* This will be a number typically between -100 and -30, indicating the signal strength.
* Heavily modified from test_radio_signal_rx app by S. James Remington
*
*/

#include <wixel.h>
#include <radio_registers.h>
#include <stdio.h>
#include <usb.h>
#include <usb_com.h>

static int16 XDATA rssiAvg[256], rssiMax[256];

void frequentTasks(void);

void updateLeds()
{
    usbShowStatusWithGreenLed();
    // Yellow LED is controlled by checkRadioChannels
    LED_RED(0);
}

void analyzerInit()
{
    radioRegistersInit();

    MCSM0 = 0x14;    // Auto-calibrate  when going from idle to RX or TX.
    MCSM1 = 0x00;    // Disable CCA.  After RX, go to IDLE.  After TX, go to IDLE.
    // We leave MCSM2 at its default value = 0x07
    MDMCFG2 = 0x70;   //disable sync word detection
    RFST = 4; //idle radio
}

void checkRadioChannels()
{
    int8 j;
    uint16 i;
    uint16 channel;

    LED_YELLOW(1);
    for(channel=0; channel<256; channel++)
    {
        rssiAvg[channel] = 0;
        rssiMax[channel] = -999;
    }

    for(j=0; j<10; j++) //ten times through all channels
    {
        for(channel=0; channel<256; channel++)
        {
            int32 rssiSum;
            int16 rssiVal;

            while(MARCSTATE != 1);  //radio should already be idle, but check anyway
            CHANNR = channel;
            RFST = 2;  // radio in RX mode and autocal
            while(MARCSTATE != 13);  //wait for RX mode
            rssiSum = 0;
            for (i=0; i<100; i++)
            {
                if (TCON & 2) //radio byte available?
                {
                    uint8 rfdata = RFD; // read byte
                    TCON &= ~2; //clear ready flag
                }
                rssiSum += radioRssi();
            }
            RFST = 4; //idle radio

            rssiVal = (int16) (rssiSum/100);
            rssiAvg[channel] += rssiVal;
            if (rssiMax[channel] < rssiVal){ rssiMax[channel] = rssiVal; } // save maximum

            frequentTasks();
        }  // the above loop takes about 414 ms on average, so about 1.6 ms/channel
    }

    for (channel=0; channel<256; channel++) rssiAvg[channel] /= 10;

    LED_YELLOW(0);
}

void reportResults()
{
    uint8 reportLength;
    uint8 XDATA report[64];
    uint16 i;
    for (i=0; i<256; i++)
    {
        if (rssiMax[i] > -90) //report activity on channel if maximum is above -90 dBm
        {
            while (usbComTxAvailable() < sizeof(report)){ frequentTasks(); }    //wait for usb TX buffer space
            reportLength = sprintf(report, "%4d, %4d, %4d\r\n", i, rssiAvg[i], rssiMax[i]);
            usbComTxSend(report, reportLength);
        }
    }
}

void frequentTasks()
{
    boardService();
    usbComService();
    updateLeds();
}

void main()
{
    systemInit();
    usbInit();
    analyzerInit();

    while(1)
    {
        frequentTasks();
        checkRadioChannels();
        reportResults();
    }
}