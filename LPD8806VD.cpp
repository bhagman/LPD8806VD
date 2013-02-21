/*
||
|| @author         Brett Hagman <bhagman@roguerobotics.com>
|| @url            http://roguerobotics.com/
|| @url            https://github.com/bhagman/LPD8806VD
|| @contribution   Adafruit Industries (LPD8806 Library and Example Code)
||
|| @description
|| | Wiring library to control LPD8806-based RGB LED strips.
|| | Based on LPD8806 library by Adafruit Industries.
|| | Changes:
|| | - 8 or 16 bit limited palette (less SRAM usage)
|| | - 24 bit palette still available
|| | - dumped malloc'd heap storage, now specify buffer at compile time
|| | 
|| | Original Source:
|| | Arduino library to control LPD8806-based RGB LED Strips
|| | https://github.com/adafruit/LPD8806
|| |
|| #
||
|| @license BSD License.
||
|| @notes
|| |
|| | It should be noted that in "24 bit mode", the maximum range for any
|| | of the R, G, or B components is only 7 bits, so technically, there
|| | are only 21 bits of resolution in the color depth (in 3 byte or
|| | "24 bit mode").
|| |
|| | 16 bit color is really only 15 bit (High color).
|| | The color sample layout is 15-bit (5:5:5).
|| | 0b0rrrrrgg gggbbbbb
|| #
||
*/

/*
||
|| TODOOZE:
||
|| - Re-add malloc() for Crazy People(tm) that want to use the Heap.
|| - Use shiftOut() for bit-bang.
|| - Converter for 16 bit -> 24 bit colors.
||
*/


/*
Original text from LPD8806 library by Adafruit Industries.

Copyright (C) Adafruit Industries
MIT license

Clearing up some misconceptions about how the LPD8806 drivers work:

The LPD8806 is not a FIFO shift register.  The first data out controls the
LED *closest* to the processor (unlike a typical shift register, where the
first data out winds up at the *furthest* LED).  Each LED driver 'fills up'
with data and then passes through all subsequent bytes until a latch
condition takes place.  This is actually pretty common among LED drivers.

All color data bytes have the high bit (128) set, with the remaining
seven bits containing a brightness value (0-127).  A byte with the high
bit clear has special meaning (explained later).

The rest gets bizarre...

The LPD8806 does not perform an in-unison latch (which would display the
newly-transmitted data all at once).  Rather, each individual byte (even
the separate G, R, B components of each LED) is latched AS IT ARRIVES...
or more accurately, as the first bit of the subsequent byte arrives and
is passed through.  So the strip actually refreshes at the speed the data
is issued, not instantaneously (this can be observed by greatly reducing
the data rate).  This has implications for POV displays and light painting
applications.  The 'subsequent' rule also means that at least one extra
byte must follow the last pixel, in order for the final blue LED to latch.

To reset the pass-through behavior and begin sending new data to the start
of the strip, a number of zero bytes must be issued (remember, all color
data bytes have the high bit set, thus are in the range 128 to 255, so the
zero is 'special').  This should be done before each full payload of color
values to the strip.  Curiously, zero bytes can only travel one meter (32
LEDs) down the line before needing backup; the next meter requires an
extra zero byte, and so forth.  Longer strips will require progressively
more zeros.  *(see note below)

In the interest of efficiency, it's possible to combine the former EOD
extra latch byte and the latter zero reset...the same data can do double
duty, latching the last blue LED while also resetting the strip for the
next payload.

So: reset byte(s) of suitable length are issued once at startup to 'prime'
the strip to a known ready state.  After each subsequent LED color payload,
these reset byte(s) are then issued at the END of each payload, both to
latch the last LED and to prep the strip for the start of the next payload
(even if that data does not arrive immediately).  This avoids a tiny bit
of latency as the new color payload can begin issuing immediately on some
signal, such as a timer or GPIO trigger.

Technically these zero byte(s) are not a latch, as the color data (save
for the last byte) is already latched.  It's a start-of-data marker, or
an indicator to clear the thing-that's-not-a-shift-register.  But for
conversational consistency with other LED drivers, we'll refer to it as
a 'latch' anyway.

* This has been validated independently with multiple customers'
  hardware.  Please do not report as a bug or issue pull requests for
  this.  Fewer zeros sometimes gives the *illusion* of working, the first
  payload will correctly load and latch, but subsequent frames will drop
  data at the end.  The data shortfall won't always be visually apparent
  depending on the color data loaded on the prior and subsequent frames.
  Tested.  Confirmed.  Fact.
*/

#include <SPI.h>
#include "LPD8806VD.h"

/*****************************************************************************/

// Constructor for use with hardware SPI.
// Pixel buffer NOT set.
LPD8806VD::LPD8806VD(uint16_t n, uint8_t depth)
{
  LPD8806VD(n, (uint8_t *)NULL, depth);
}

// Constructor for use with hardware SPI.
LPD8806VD::LPD8806VD(uint16_t n, uint8_t *buf, uint8_t depth)
{
  pixels = buf;
  begun  = false;
  setColorDepth(depth);
  updateLength(n);
  updatePins();
}


// Constructor for use with arbitrary clock/data pins:
// Pixel buffer NOT set.
LPD8806VD::LPD8806VD(uint16_t n, uint8_t dpin, uint8_t cpin, uint8_t depth)
{
  LPD8806VD(n, dpin, cpin, (uint8_t *)NULL, depth);
}


// Constructor for use with arbitrary clock/data pins:
LPD8806VD::LPD8806VD(uint16_t n, uint8_t dpin, uint8_t cpin, uint8_t *buf, uint8_t depth)
{
  pixels = buf;
  begun  = false;
  setColorDepth(depth);
  updateLength(n);
  updatePins(dpin, cpin);
}


// Sets the color depth.
// Accepted color depths: 1, 2, 3 (or 8, 15/16, 21/24)
void LPD8806VD::setColorDepth(uint8_t depth)
{
  switch (depth)
  {
    case 1:
    case 2:
    case 3:
      colorDepth = depth;
      break;
    case 8:
      colorDepth = 1;
      break;
    case 15:
    case 16:
      colorDepth = 2;
      break;
    case 21:
    case 24:
      colorDepth = 3;
      break;
    default:
      colorDepth = 0;
      break;
  }
}


// Activate hard/soft SPI as appropriate.
void LPD8806VD::begin(void)
{
  if (hardwareSPI == true)
    startSPI();
  else
    startBitbang();
  begun = true;
}


// Change pin assignments post-constructor, switching to hardware SPI.
void LPD8806VD::updatePins(void)
{
  hardwareSPI = true;
  datapin     = clkpin = 0;
  // If begin() was previously invoked, init the SPI hardware now:
  if (begun == true)
    startSPI();
  // Otherwise, SPI is NOT initted until begin() is explicitly called.

  // Note: any prior clock/data pin directions are left as-is and are
  // NOT restored as inputs!
}


// Change pin assignments post-constructor, using arbitrary pins.
void LPD8806VD::updatePins(uint8_t dpin, uint8_t cpin)
{
  datapin     = dpin;
  clkpin      = cpin;
  clkport     = dataport = 0;
  clkpinmask  = datapinmask = 0;

#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328P__) || defined (__AVR_ATmega328__) || defined(__AVR_ATmega8__) || (__AVR_ATmega1281__) || defined(__AVR_ATmega2561__) || defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
  clkport     = portOutputRegister(digitalPinToPort(cpin));
  clkpinmask  = digitalPinToBitMask(cpin);
  dataport    = portOutputRegister(digitalPinToPort(dpin));
  datapinmask = digitalPinToBitMask(dpin);
#endif

  if (begun == true)  // If begin() was previously invoked...
  {
    // If previously using hardware SPI, turn that off:
    if (hardwareSPI == true)
      SPI.end();
    startBitbang(); // Regardless, now enable 'soft' SPI outputs
  } // Otherwise, pins are not set to outputs until begin() is called.

  // Note: any prior clock/data pin directions are left as-is and are
  // NOT restored as inputs!

  hardwareSPI = false;
}


// Enable SPI hardware and set up protocol details.
// TODO: Hardware SPI - remove #if defined's
void LPD8806VD::startSPI(void)
{
  uint8_t i = latchBytes;

  SPI.begin();
  SPI.setBitOrder(MSBFIRST);
  SPI.setDataMode(SPI_MODE0);

  // Go as fast as you can go!!! :)
  // 16MHz / 2 = 8 MHz -- go like sn*t!

  SPI.setClockDivider(SPI_CLOCK_DIV4);

  // Although the LPD8806 should, in theory, work up to 20MHz, the unshielded
  // wiring from the microcontroller can be more susceptible to interference.
  // Experiment and see what you get.

  if (latchBytes == 0) return;
  
  // Issue initial latch/reset to strip:

#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328P__) || defined (__AVR_ATmega328__) || defined(__AVR_ATmega8__) || (__AVR_ATmega1281__) || defined(__AVR_ATmega2561__) || defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)

  while (i--)
  {
    SPDR = 0;
    while (!(SPSR & (1 << SPIF))); // Wait for prior byte out
  }
#else
  while (i--)
  {
    SPI.transfer(0);
  }
#endif
}


// Send off a byte via bit bang.
// TODO: use shiftOut()
void LPD8806VD::sendBitBangByte(uint8_t data)
{
  // MSBFIRST implied
  uint8_t bit;

  for (bit = 0x80; bit; bit >>= 1)
  {
    // use low level bitbanging when we can
    if (dataport != 0)
    {
      if (data & bit)
        *dataport |=  datapinmask;
      else
        *dataport &= ~datapinmask;
      *clkport |=  clkpinmask;
      *clkport &= ~clkpinmask;
    }
    else
    {
      // can't do low level bitbanging, revert to digitalWrite
      if (data & bit)
        digitalWrite(datapin, HIGH);
      else
        digitalWrite(datapin, LOW);
      digitalWrite(clkpin, HIGH);
      digitalWrite(clkpin, LOW);
    }
  }
}


// Enable software SPI pins and issue initial latch.
void LPD8806VD::startBitbang()
{
  uint8_t i = latchBytes;

  pinMode(datapin, OUTPUT);
  pinMode(clkpin , OUTPUT);
  
  if (latchBytes == 0) return;

  // send a "latch" clear
  while (i--)
  {
    sendBitBangByte(0);
  }
}


// Update the length of the strip.
void LPD8806VD::updateLength(uint16_t n)
{
  latchBytes = (n + 31) / 32;  // 1 latch byte every 32 "pixels"

  numLEDs    = n;

  clear();

  // 'begun' state does not change -- pins retain prior modes
}

// Clear the pixel array.
void LPD8806VD::clear(void)
{
  if (pixels != NULL)
  {
    memset(pixels, 0, numLEDs * colorDepth);  // Clear the array
  }
}


// This is how data is pushed to the strip.  Unfortunately, the company
// that makes the chip didnt release the protocol document or you need
// to sign an NDA or something stupid like that, but we reverse engineered
// this from a strip controller and it seems to work very nicely!
void LPD8806VD::show(void)
{
  uint8_t  *ptr = pixels;
  uint16_t i    = numLEDs;
  uint8_t  r, g, b;
  uint16_t color;

  while (i--)
  {
    switch (colorDepth)
    {
      case 1:
        g = getGreen8(*ptr);
        r = getRed8(*ptr);
        b = getBlue8(*ptr);
        break;
      case 2:
        color = ((uint16_t)(*ptr) << 8) | (uint16_t)(*(ptr + 1));
        g = getGreen16(color);
        r = getRed16(color);
        b = getBlue16(color);
        break;
      case 3:
        g = *ptr;
        r = *(ptr + 1);
        b = *(ptr + 2);
        break;
    }

    // We need to set the upper bit on all components
    g |= 0x80;
    r |= 0x80;
    b |= 0x80;

    if (hardwareSPI)
    {
#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328P__) || defined (__AVR_ATmega328__) || defined(__AVR_ATmega8__) || (__AVR_ATmega1281__) || defined(__AVR_ATmega2561__) || defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
      SPDR = g;              // Issue new byte
      while (!(SPSR & (1 << SPIF))); // Wait for prior byte out
      SPDR = r;              // Issue new byte
      while (!(SPSR & (1 << SPIF))); // Wait for prior byte out
      SPDR = b;              // Issue new byte
      while (!(SPSR & (1 << SPIF))); // Wait for prior byte out
#else
      SPI.transfer(g);
      SPI.transfer(r);
      SPI.transfer(b);
#endif
    }
    else
    {
      sendBitBangByte(g);
      sendBitBangByte(r);
      sendBitBangByte(b);
    }

    ptr += colorDepth;
  }

  
  // Now send "latch" clear bytes (0)
  i = latchBytes;
  while (i--)
  {
    if (hardwareSPI)
    {
#if defined(__AVR_ATmega168__) || defined(__AVR_ATmega328P__) || defined (__AVR_ATmega328__) || defined(__AVR_ATmega8__) || (__AVR_ATmega1281__) || defined(__AVR_ATmega2561__) || defined(__AVR_ATmega2560__) || defined(__AVR_ATmega1280__)
      SPDR = 0;                      // send "latch" clear bytes (0)
      while (!(SPSR & (1 << SPIF))); // Wait for prior byte out
#else
      SPI.transfer(0);
#endif
    }
    else
    {
      sendBitBangByte(0);
    }
    
  }
}


// The following set of methods get the LPD8806 GRB components.

uint8_t LPD8806VD::getRed8(uint8_t c8)
{
  return ((c8 & 0b11100000) >> 1);
}

uint8_t LPD8806VD::getGreen8(uint8_t c8)
{
  return ((c8 & 0b00011100) << 2);
}

uint8_t LPD8806VD::getBlue8(uint8_t c8)
{
  return ((c8 & 0b00000011) << 5);
}


uint8_t LPD8806VD::getRed16(uint16_t c16)
{
  return ((c16 & 0b0111110000000000) >> (8 + 0));
}

uint8_t LPD8806VD::getGreen16(uint16_t c16)
{
  return ((c16 & 0b0000001111100000) >> (2 + 1));
}

uint8_t LPD8806VD::getBlue16(uint16_t c16)
{
  return ((c16 & 0b0000000000011111) << (3 - 1));
}


// Convert 8 bit -> 16 bit color.
// Utility function for less transfer payload.
uint16_t LPD8806VD::Color8To16(uint8_t c8)
{
  // C8 = rrrgggbb
  // -> C16 = 0rrr00gg g00bb000
  uint8_t r = (c8 & 0b11100000);
  uint8_t g = (c8 & 0b00011100) << 3;
  uint8_t b = (c8 & 0b00000011) << 6;
  
  return (uint16_t)(r) << (8 - 1) |
         (uint16_t)(g) << 2 |
         (uint16_t)(b) >> 3;
}


// TODO: Convert 16 bit -> 24 bit color.

/*
// Convert 8 bit -> GRB
uint32_t LPD8806VD::Color8ToGRB(uint8_t c8)
{
  // GRB = xggggggg xrrrrrrr xbbbbbbb
  // C8 = rrrgggbb
  return ((uint32_t)(c8 & 0b00011100) << (16 + 3 - 1)) |
         ((uint32_t)(c8 & 0b11100000) << (8 - 1)) |
         ((uint32_t)(c8 & 0b00000011) << (6 - 1));
}

// Convert 16 bit -> GRB
uint32_t LPD8806VD::Color16ToGRB(uint16_t c16)
{
  // GRB = xggggggg xrrrrrrr xbbbbbbb
  // C16 = rrrrrrgg gggbbbbb
  uint8_t r = ((c16 & 0b1111110000000000) >> (8 + 1));
  uint8_t g = ((c16 & 0b0000001111100000) >> (5 - 1));
  uint8_t b = ((c16 & 0b0000000000011111) << (3 - 1));
  
  return (uint32_t)(g) << (16) |
         (uint32_t)(r) << (8) |
         (uint32_t)(b);
}

// Convert 24 bit -> GRB
uint32_t LPD8806VD::Color24ToGRB(uint32_t c24)
{
  // GRB = xggggggg xrrrrrrr xbbbbbbb
  // C24 = rrrrrrrr gggggggg bbbbbbbb
  uint8_t r = (c24 >> (16 + 1) & 0xff);
  uint8_t g = (c24 >> (8 + 1) & 0xff);
  uint8_t b = (c24 >> 1 & 0xff);
  
  return (uint32_t)(g) << (16) |
         (uint32_t)(r) << (8) |
         (uint32_t)(b);
}
*/

// Convert a 24 bit RGB color to a packed color
uint32_t LPD8806VD::Color(uint32_t color)
{
  return Color(color >> 16, color >> 8, color);
}


// Convert a R,G,B component values into a packed color value based on
// the current settings of the strip.
uint32_t LPD8806VD::Color(uint8_t r, uint8_t g, uint8_t b)
{
  // outputs either a 8, 16, or 24/32 bit value (which can be put into the
  // pixel array.
  // C8 = rrrgggbb
  // C16 = rrrrrrgg gggbbbbb
  // C24 uses the direct color format for the LPD8806 (GRB)
  //     = 0ggggggg 0rrrrrrr 0bbbbbbb
  //     (the upper bit will be set later)

  uint32_t packedColor = 0;
  
  switch (colorDepth)
  {
    case 1:
      packedColor = 0x000000ff &
                    ((r & 0b11100000) |
                     (g & 0b11100000) >> 3 |
                     (b & 0b11000000) >> 6);
      break;
    case 2:
      packedColor = 0x0000ffff &
                    ((uint16_t)(r & 0b11111000) << 7 |
                     (uint16_t)(g & 0b11111000) << 2 |
                     (uint16_t)(b & 0b11111000) >> 3);
      break;
    case 3:
      packedColor = 0x00ffffff &
                    ((uint32_t)(g) << (16 - 1) |
                     (uint32_t)(r) << (8 - 1) |
                     (uint32_t)(b) >> 1);
      break;
  }
  
  return packedColor;
}


// Sets a pixel's color, from R, G, B components.
void LPD8806VD::setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b)
{
  setPixelColor(n, Color(r, g, b));
}


// Sets a pixel's color directly (8 and 16 bit colors are NOT in GRB format).
// color is expected to be in packed format.
void LPD8806VD::setPixelColor(uint16_t n, uint32_t color)
{
  uint8_t *p = &pixels[n * colorDepth];

  switch (colorDepth)
  {
    case 1:
      *p   = color;
      break;
    case 2:
      *p++ = color >> 8;
      *p   = color;
      break;
    case 3:
      // Store in GRB Ready-To-Go(tm) format
      *p++ = (color >> 16);
      *p++ = (color >> 8);
      *p   = color;
      break;
  }
}


// Query color from previously-set pixel.
uint32_t LPD8806VD::getPixelColor(uint16_t n)
{
  uint8_t *ptr;
  uint8_t  r, g, b;
  uint16_t color16;


  if (n < numLEDs)
  {
    // first, let's get our components
    ptr = &pixels[n * colorDepth];

    switch (colorDepth)
    {
      case 1:
        g = getGreen8(*ptr);
        r = getRed8(*ptr);
        b = getBlue8(*ptr);
        break;
      case 2:
        color16 = ((uint16_t)(*ptr) << 8) | (uint16_t)(*(ptr + 1));
        g = getGreen16(color16);
        r = getRed16(color16);
        b = getBlue16(color16);
        break;
      case 3:
        g = *ptr;
        r = *(ptr + 1);
        b = *(ptr + 2);
        break;
    }
    
    // now make them 256 value components
    
    g <<= 1;
    r <<= 1;
    b <<= 1;
    
    // and finally, return the packed value.
    
    return Color(r, g, b);
  }

  return 0; // Pixel # is out of bounds
}

