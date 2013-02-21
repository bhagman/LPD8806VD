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
*/

// Meh... Dunno if it works with Arduino, but hey, we'll put this in here.
#if defined(WIRING)
 #include <Wiring.h>
#elif ARDUINO >= 100
 #include <Arduino.h>
#else
 #include <WProgram.h>
#endif

class LPD8806VD
{
  public:

    // Constructors using Hardware SPI
    // Use SPI hardware; specific pins only
    LPD8806VD(uint16_t n = 0, uint8_t depth = 3);  // Buffer not set
    LPD8806VD(uint16_t n, uint8_t *buf, uint8_t depth = 3);

    // Constructors using bit-bang'd SPI
    // Configurable pins
    LPD8806VD(uint16_t n, uint8_t dpin, uint8_t cpin, uint8_t depth = 3);  // Buffer not set
    LPD8806VD(uint16_t n, uint8_t dpin, uint8_t cpin, uint8_t *buf, uint8_t depth = 3);

    void begin(void);
    void clear(void);                             // Clear the pixel buffer
    void show(void);                              // Show all pixels
    void setPixelColor(uint16_t n, uint8_t r, uint8_t g, uint8_t b);
    void setPixelColor(uint16_t n, uint32_t c);   // Sets pixel to color (c is 8, 16, or 24 bit color)
//    void setPixelColor24(uint16_t n, uint32_t c); // 24 bit color (packed RGB)
//    void setPixelColor16(uint16_t n, uint16_t c); // 16 bit color
//    void setPixelColor8(uint16_t n, uint8_t c);   // 8 bit color
    void updatePins(uint8_t dpin, uint8_t cpin);  // Change pins, configurable
    void updatePins(void);                        // Change pins, hardware SPI
    void updateLength(uint16_t n);                // Change strip length
    void setBufferPointer(uint8_t *buf);          // Change the buffer
    void setColorDepth(uint8_t depth);            // Change the color depth

    uint8_t getColorDepth(void) { return colorDepth; };
    uint16_t numPixels(void) { return numLEDs; };
    uint32_t Color(uint32_t color);               // Convert a 24 RGB to a packed color
    uint32_t Color(uint8_t, uint8_t, uint8_t);    // Convert RGB components to packed color
    uint32_t getPixelColor(uint16_t n);

    uint16_t Color8To16(uint8_t c8);

//    uint32_t Color8ToGRB(uint8_t c8);
//    uint32_t Color16ToGRB(uint16_t c16);
//    uint32_t Color24ToGRB(uint32_t c24);

    // Note: These should really be private. Exposed for debugging.
    uint8_t getRed8(uint8_t c8);
    uint8_t getGreen8(uint8_t c8);
    uint8_t getBlue8(uint8_t c8);

    uint8_t getRed16(uint16_t c16);
    uint8_t getGreen16(uint16_t c16);
    uint8_t getBlue16(uint16_t c16);

  private:

    uint8_t colorDepth;                           // 1 = 8 bit, 2 = 16 bit, 3 = 24/32 bit
    uint16_t numLEDs;                             // Number of RGB LEDs in strip
    uint8_t latchBytes;                           // Bytes to clear "latch"
    uint8_t *pixels;                              // Holds LED color values
    uint8_t clkpin, datapin;                      // Clock & data pin numbers
    uint8_t clkpinmask, datapinmask;              // Clock & data PORT bitmasks
    volatile uint8_t *clkport, *dataport;         // Clock & data PORT registers

    void sendBitBangByte(uint8_t data);
    void startBitbang(void);
    void startSPI(void);

    boolean hardwareSPI; // If 'true', using hardware SPI
    boolean begun;       // If 'true', begin() method was previously invoked
};

