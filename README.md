Important:
This is a rare display module! If you are looking for "Marauder" or other ready-to-flash "hacking" firmwares, they will NOT work on this specific hardware. Most ESP32-S3 3.5" tools are built for the cheaper "Yellow/Blue" modules (ILI9488/ST7796). This board uses a unique QSPI AXS15231B controller which requires custom drivers and PSRAM. Use this repository to learn how to correctly initialize this display via Arduino/GFX.

Project Overview: Finger Painting Test
This sketch serves as a comprehensive functional test for the JC3248W535C module. It allows you to verify:

Display Integrity: Full-screen rendering via QSPI and PSRAM.
Touch Precision: Multi-point touch tracking and coordinate mapping.
Color Accuracy: A palette of 5 colors to verify RGB565 rendering.
Performance: Fluidity of the Arduino_Canvas buffer at 480x320 resolution.

Features:
Smooth drawing with line interpolation.
"CLEAR" button with a 200ms debounce to prevent accidental resets.
Bottom color palette for easy switching during testing.
Technical Specifications
Module: JC3248W535C (ESP32-S3-WROOM-1).
Display: 3.5" IPS, 480x320 px.
Driver: AXS15231B (QSPI).
Memory: 16MB Flash, 8MB PSRAM (OPI).
Required Libraries
To compile this project, you must install the following library via the Arduino Library Manager:

GFX Library for Arduino (by Moon On Our Nation) - Version 1.6.5 or newer.
Arduino IDE Configuration (Critical)
Standard settings will not work. You must manually configure the following under the Tools menu:

Board: ESP32S3 Dev Module
USB CDC On Boot: Enabled (to see Serial output)
Flash Size: 16MB (128Mb)
PSRAM: OPI PSRAM (Mandatory! Without this, the Canvas allocation will fail and the screen will remain black).
Arduino CLI Configuration
To ensure the code functions correctly via CLI, use the following flags:

arduino-cli compile --fqbn esp32:esp32:esp32s3:CDCOnBoot=cdc,USBMode=hwcdc,PSRAM=opi,FlashSize=16M
Display Pinout (QSPI)
CS: 45
SCLK: 47
D0: 21, D1: 48, D2: 40, D3: 39
Backlight: GPIO 1
Visual Troubleshooting (Know-How)

Canvas Mode: The AXS15231B driver via QSPI requires the use of Arduino_Canvas. Drawing directly to the screen object will cause synchronization errors (white screen or artifacts).
PSRAM: The Canvas buffer must be allocated in PSRAM (hence the PSRAM=opi flag).
Flush: Every screen update requires calling gfx->flush() to push data from the buffer to the display controller.
Colors: If colors appear inverted, toggle the IPS parameter in the Arduino_AXS15231B constructor (currently set to false).

Initialization Code (Stable)
'''
Arduino_DataBus *bus = new Arduino_ESP32QSPI(45, 47, 21, 48, 40, 39);
Arduino_GFX *g = new Arduino_AXS15231B(bus, -1, 0, false, 320, 480);
Arduino_Canvas *gfx = new Arduino_Canvas(320, 480, g);

void setup() {
  pinMode(1, OUTPUT); digitalWrite(1, HIGH); // Backlight
  gfx->begin();
  '''
  gfx->fillScreen(0x0000);
  gfx->flush();
}
