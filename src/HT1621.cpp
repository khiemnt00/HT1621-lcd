/*******************************************************************************
Copyright 2016-2018 anxzhu (github.com/anxzhu)
Copyright 2018 Valerio Nappi (github.com/5N44P) (changes)
Based on segment-lcd-with-ht1621 from anxzhu (2016-2018)
(https://github.com/anxzhu/segment-lcd-with-ht1621)

Partially rewritten and extended by Valerio Nappi (github.com/5N44P) in 2018
https://github.com/5N44P/ht1621-7-seg

Refactored. Removed dependency on any MCU hardware by Viacheslav Balandin
https://github.com/hedgehogV/HT1621-lcd

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

*******************************************************************************/

#include "HT1621.hpp"
#include "math.h"
#include "stdio.h"

/**
 * @brief CALCULATION DEFINES BLOCK
 */
#define MAX_NUM     999999
#define MIN_NUM     -99999

#define MAX_POSITIVE_PRECISION 3
#define MAX_NEGATIVE_PRECISION 2

/**
 * @brief DISPLAY HARDWARE DEFINES BLOCK
 */
#define  BIAS     0x52             //0b1000 0101 0010  1/3duty 4com
#define  SYSDIS   0x00             //0b1000 0000 0000  Turn off both system oscillator and LCD bias generator
#define  SYSEN    0x02             //0b1000 0000 0010  Turn on system oscillator
#define  LCDOFF   0x04             //0b1000 0000 0100  Turn off LCD bias generator
#define  LCDON    0x06             //0b1000 0000 0110  Turn on LCD bias generator
#define  XTAL     0x28             //0b1000 0010 1000  System clock source, crystal oscillator
#define  RC256    0x30             //0b1000 0011 0000  System clock source, on-chip RC oscillator
#define  TONEON   0x12             //0b1000 0001 0010  Turn on tone outputs
#define  TONEOFF  0x10             //0b1000 0001 0000  Turn off tone outputs
#define  WDTDIS1  0x0A             //0b1000 0000 1010  Disable WDT time-out flag output

#define MODE_CMD  0x80
#define MODE_WR   0xa0

#define BATTERY_SEG_ADDR    0x80
#define SEPARATOR_SEG_ADDR  0x80




// TODO: replace magic numbers
// TODO: give wrapper example for GPIO toggle in README and in hpp
HT1621::HT1621(pPinSet *pCs, pPinSet *pSck, pPinSet *pMosi, pPinSet *pBacklight)
{
    pCsPin = pCs;
    pSckPin = pSck;
    pMosiPin = pMosi;
    pBacklightPin = pBacklight;

    wrCmd(BIAS);
    wrCmd(RC256);
    wrCmd(SYSDIS);
    wrCmd(WDTDIS1);
    wrCmd(SYSEN);
    wrCmd(LCDON);
}


void HT1621::backlightOn()
{
    if (pBacklightPin)
        pBacklightPin(HIGH);
}

void HT1621::backlightOff()
{
    if (pBacklightPin)
        pBacklightPin(LOW);
}

void HT1621::displayOn()
{
    wrCmd(LCDON);
}

void HT1621::displayOff()
{
    wrCmd(LCDOFF);
}

void HT1621::wrBits(uint8_t bitField, uint8_t cnt)
{
    if (!pSckPin || !pMosiPin)
        return;

    for (int i = 0; i < cnt; i++)
    {
        pSckPin(LOW);
        pMosiPin((bitField & 0x80)? HIGH : LOW);
        pSckPin(HIGH);
        bitField <<= 1;
    }
}

void HT1621::wrByte(uint8_t addr, uint8_t byte)
{
    if (!pCsPin)
        return;

    addr <<= 2;

    pCsPin(LOW);
    wrBits(MODE_WR, 3);
    wrBits(addr, 6);
    wrBits(byte, sizeof(byte));
    pCsPin(HIGH);
}


void HT1621::wrCmd(uint8_t cmd) // TODO: think about va_args
{
    if (!pCsPin)
        return;

    pCsPin(LOW);
    wrBits(MODE_CMD, 4);
    wrBits(cmd, sizeof(cmd));
    pCsPin(HIGH);
}


void HT1621::batteryLevel(tBatteryLevel level)
{
    // zero out stored battery level
    _buffer[0] &= ~BATTERY_SEG_ADDR;
    _buffer[1] &= ~BATTERY_SEG_ADDR;
    _buffer[2] &= ~BATTERY_SEG_ADDR;

    switch(level)
    {
        case 3: // battery on and all 3 segments
            _buffer[0] |= BATTERY_SEG_ADDR;
            // fall through
        case 2: // battery on and 2 segments
            _buffer[1] |= BATTERY_SEG_ADDR;
            // fall through
        case 1: // battery on and 1 segment
            _buffer[2] |= BATTERY_SEG_ADDR;
            break;
        case 0: // battery indication off
            break;
        default:
            break;
    }
    update();
}


void HT1621::clear()
{
    for (int addr = 0; addr < DISPLAY_SIZE * 2; addr += 2)
    {
        wrByte(addr, 0);
    }
}


void HT1621::update()
{
    // TODO: rewrite with a loop
    // the buffer is backwards with respect to the lcd. could be improved
    wrByte(0, _buffer[5]);
    wrByte(2, _buffer[4]);
    wrByte(4, _buffer[3]);
    wrByte(6, _buffer[2]);
    wrByte(8, _buffer[1]);
    wrByte(10,_buffer[0]);
}

void HT1621::print(int32_t num)
{
    if (num > MAX_NUM)
        num = MAX_NUM;
    if (num < MIN_NUM)
        num = MIN_NUM;

    char localbuffer[DISPLAY_SIZE + 1];
    snprintf(localbuffer, sizeof(localbuffer), "%6li", num); // convert the decimal into string

    for (int i = 0; i < DISPLAY_SIZE; i++)
    {
        // TODO: add letters support
        // TODO: speed this up
        _buffer[i] &= 0x80; // mask the first bit, used by batter and decimal point
        switch (localbuffer[i]){ // map the digits to the seg bits
            case '0':
                _buffer[i] |= 0x7D;
                break;
            case '1':
                _buffer[i] |= 0x60;
                break;
            case '2':
                _buffer[i] |= 0x3e;
                break;
            case '3':
                _buffer[i] |= 0x7a;
                break;
            case '4':
                _buffer[i] |= 0x63;
                break;
            case '5':
                _buffer[i] |= 0x5b;
                break;
            case '6':
                _buffer[i] |= 0x5f;
                break;
            case '7':
                _buffer[i] |= 0x70;
                break;
            case '8':
                _buffer[i] |= 0x7f;
                break;
            case '9':
                _buffer[i] |= 0x7b;
                break;
            case '-':
                _buffer[i] |= 0x02;
                break;
            }
        }

        update();
}


void HT1621::print(float num, uint8_t precision)
{
    if (num >= 0 && precision > MAX_POSITIVE_PRECISION)
        precision = MAX_POSITIVE_PRECISION;
    else if (num < 0 && precision > MAX_NEGATIVE_PRECISION)
        precision = MAX_NEGATIVE_PRECISION;

    int32_t integerated = (int32_t)(num * pow(10, precision));

    if (integerated > MAX_NUM)
        integerated = MAX_NUM;
    if (integerated < MIN_NUM)
        integerated = MIN_NUM;

    print(integerated);
    decimalSeparator(precision);

    update();
}

void HT1621::decimalSeparator(uint8_t dpPosition)
{
    // zero out the eight bit
    _buffer[3] &= ~SEPARATOR_SEG_ADDR;
    _buffer[4] &= ~SEPARATOR_SEG_ADDR;
    _buffer[5] &= ~SEPARATOR_SEG_ADDR;

    if (dpPosition == 0 || dpPosition > 3)
        return;

    // 3 is the digit offset
    // the first three eights bits in the buffer are for the battery signs
    // the last three are for the decimal point
    _buffer[DISPLAY_SIZE - dpPosition] |= SEPARATOR_SEG_ADDR;
}
