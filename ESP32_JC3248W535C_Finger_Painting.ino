#include <Arduino_GFX_Library.h>
#include <Wire.h>

// --- Display Configuration ---
#define TFT_BL 1
#define QSPI_CS 45
#define QSPI_SCK 47
#define QSPI_D0 21
#define QSPI_D1 48
#define QSPI_D2 40
#define QSPI_D3 39

// --- Touch Configuration (I2C) ---
#define TOUCH_I2C_ADD 0x3B
#define TOUCH_SDA 4
#define TOUCH_SCL 8
#define TOUCH_INT 3
#define TOUCH_RST 38

Arduino_DataBus *bus = new Arduino_ESP32QSPI(QSPI_CS, QSPI_SCK, QSPI_D0, QSPI_D1, QSPI_D2, QSPI_D3);
Arduino_GFX *g = new Arduino_AXS15231B(bus, -1, 0, false, 320, 480);
Arduino_Canvas *gfx = new Arduino_Canvas(320, 480, g);

// Helper function to read AXS15231B touch with filter (0,0)
bool getTouch(int &x, int &y) {
  uint8_t read_cmd[] = {0xB5, 0xAB, 0xA5, 0x5A, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00};
  uint8_t data[8] = {0};

  Wire.beginTransmission(TOUCH_I2C_ADD);
  Wire.write(read_cmd, sizeof(read_cmd));
  Wire.endTransmission();

  Wire.requestFrom(TOUCH_I2C_ADD, 8);
  if (Wire.available() == 8) {
    for (int i = 0; i < 8; i++) data[i] = Wire.read();

    if (data[0] == 0 && data[1] != 0) {
      int tx = ((data[2] & 0x0F) << 8) | data[3];
      int ty = ((data[4] & 0x0F) << 8) | data[5];
      
      // Filter invalid readings (AXS often sends 0,0 at start/end)
      if (tx <= 2 || ty <= 2 || tx >= 318 || ty >= 478) return false;

      x = tx;
      y = ty;
      return true;
    }
  }
  return false;
}

void setup() {
  Serial.begin(115200);
  
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, HIGH); 
  delay(100);

  Wire.begin(TOUCH_SDA, TOUCH_SCL);

  if (!gfx->begin()) {
    Serial.println("gfx->begin() error!");
    while(1);
  }

  drawUI();
}

void drawUI() {
  gfx->fillScreen(0x0000); 
  
  // "Clear" button
  gfx->fillRect(220, 5, 90, 40, 0xF800); 
  gfx->setCursor(235, 15);
  gfx->setTextColor(0xFFFF);
  gfx->setTextSize(2);
  gfx->print("CLEAR");

  // Label next to button
  gfx->setCursor(10, 18);
  gfx->setTextSize(1);
  gfx->setTextColor(0x7BEF); // Gray/Light blue
  gfx->print("Screen and touch test");

  // Color palette at bottom (5 squares)
  gfx->fillRect(0, 430, 64, 50, 0xF800); // Red
  gfx->fillRect(64, 430, 64, 50, 0x07E0); // Green
  gfx->fillRect(128, 430, 64, 50, 0x001F); // Blue
  gfx->fillRect(192, 430, 64, 50, 0xFFE0); // Yellow
  gfx->fillRect(256, 430, 64, 50, 0xFFFF); // White
  
  gfx->flush();
}

int lastX = -1, lastY = -1;
uint16_t currentColor = 0x07E0; // Default Green

void loop() {
  int tx, ty;
  if (getTouch(tx, ty)) {
    // Check "Clear" button
    if (tx > 220 && tx < 310 && ty > 5 && ty < 45) {
      static unsigned long clearTimer = 0;
      if (clearTimer == 0) clearTimer = millis();
      if (millis() - clearTimer > 200) {
        drawUI();
        lastX = -1; lastY = -1;
        clearTimer = 0;
        delay(300);
      }
      return;
    }

    // Color selection from bottom palette (ty > 430)
    if (ty > 430) {
      if (tx < 64) currentColor = 0xF800;
      else if (tx < 128) currentColor = 0x07E0;
      else if (tx < 192) currentColor = 0x001F;
      else if (tx < 256) currentColor = 0xFFE0;
      else currentColor = 0xFFFF;
      
      lastX = -1; lastY = -1; // Reset line on color change
      return;
    }

    // Drawing (only in work area: below CLEAR button, above palette)
    if (ty > 60 && ty < 420) {
      if (lastX != -1) {
        gfx->drawLine(lastX, lastY, tx, ty, currentColor);
        gfx->fillCircle(tx, ty, 3, currentColor);
      } else {
        gfx->fillCircle(tx, ty, 3, currentColor);
      }
      lastX = tx;
      lastY = ty;
      gfx->flush();
    }
  } else {
    lastX = -1;
    lastY = -1;
  }
  delay(5);
}
