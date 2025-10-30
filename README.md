# BATTLESHIP GAME ON ESP32 

A simple battleship game built using esp32DOIT as the processor, SPI OLED as display and pushbutton as the input

## DISPLAY SPECIFICATIONS
The program runs using ST7789 chipset on the SPI OLED
The configs for this program is not in the codes, so it needs to be sets up firsthand
Below are the changes made on the `User_Setup.h`
#define ST7789_DRIVER 

#define TFT_WIDTH  240
#define TFT_HEIGHT 240

#define TFT_MOSI 21  // Your "SDA" pin
#define TFT_SCLK 18  // Your "SCL" pin
#define TFT_CS   -1  
#define TFT_DC    2  // Your DC pin
#define TFT_RST   4  // Your RES pin
// #define TFT_BL   25  // Your BLK pin

#define LOAD_GLCD   // Load standard font (for println)
#define LOAD_FONT2  // Load a medium font
#define LOAD_FONT4  // Load a large font
#define LOAD_GFXFF  // Load custom FreeFonts