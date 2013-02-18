# Variable Color Depth LPD8806 Library for Wiring #
This is a [Wiring](http://wiring.org.co/) Library for the LPD8806 PWM LED driver
chips, strips, and pixels.

The library is based upon the Adafruit Industries [LPD8806 Library](https://github.com/adafruit/LPD8806).

## What's different? ##
The original [LPD8806 Library](https://github.com/adafruit/LPD8806) only used 24 bit colors,
which requires the use of 4 bytes per pixel.  To save space, at the cost of color resolution,
the LPD8806VD library allows the use of 16 bit and 8 bit color depths.

## Download ##
Click the Downloads Tab in the Tabbar above. 

## Installation ##
* Uncompress the Downloaded Library
* Rename the uncompressed folder to LPD8806VD
* Check that the LPD8806VD folder contains `LPD8806VD.cpp` and `LPD8806VD.h`
* Place the LPD8806VD library folder your `<WiringSketchFolder>/libraries/` folder, 
  if the libraries folder does not exist - create it first!
* (You can find - and also change - the Wiring Sketch Folder within the Wiring IDE under 
  **File -> Preferences**)
* Restart the IDE
