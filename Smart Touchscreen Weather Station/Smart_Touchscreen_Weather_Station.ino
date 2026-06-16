#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <SD.h>
#include <SPI.h>
#include "secrets.h"

// --- Display Configuration ---
#define TFT_BL 1
#define QSPI_CS 45
#define QSPI_SCK 47
#define QSPI_D0 21
#define QSPI_D1 48
#define QSPI_D2 40
#define QSPI_D3 39

// --- Touch Configuration ---
#define TOUCH_I2C_ADD 0x3B
#define TOUCH_SDA 4
#define TOUCH_SCL 8
#define TOUCH_INT 3
#define TOUCH_RST 38

// --- SD Card (SPI) ---
#define SD_CS 10   // <-- CHANGE THIS TO YOUR SD CARD CS PIN

// --- WiFi Credentials ---
const char* ssid     = SECRET_SSID;
const char* password = SECRET_PASSWORD;
String apiKey        = SECRET_API_KEY;

// --- Location ---
String cityName = "London";
String country  = "UK";

// --- Time / NTP ---
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 0;
const int daylightOffset_sec = 0;

// --- Display Objects ---
Arduino_DataBus *bus = new Arduino_ESP32QSPI(QSPI_CS, QSPI_SCK, QSPI_D0, QSPI_D1, QSPI_D2, QSPI_D3);
Arduino_GFX *g = new Arduino_AXS15231B(bus, -1, 2, false, 320, 480); // rotation = 2 (180°)
Arduino_Canvas *gfx = new Arduino_Canvas(320, 480, g);

// --- Weather Data ---
float currentTemp = 0.0;
int currentHumidity = 0;
String currentDesc = "Loading...";
bool hasData = false;

// --- Last Saved Weather (for change detection) ---
float lastSavedTemp = NAN;
int lastSavedHumidity = -1;
String lastSavedDesc = "";

// --- SD Card Status ---
bool sdAvailable = false;

// --- Minute tracking ---
int lastMinute = -1;

// --- Touch Function (ROTATED) ---
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

      if (tx <= 2 || ty <= 2 || tx >= 318 || ty >= 478) return false;

      // Rotate touch 180°
      x = 320 - tx;
      y = 480 - ty;

      return true;
    }
  }
  return false;
}

// --- Fetch Weather Data ---
void fetchWeather() {
  Serial.println("[FETCH] Starting weather fetch...");

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[FETCH] WiFi NOT connected!");
    hasData = false;
    currentDesc = "NO WIFI";
    return;
  }

  String url =
    "https://api.openweathermap.org/data/2.5/weather?q=" +
    cityName + "," + country +
    "&appid=" + apiKey +
    "&units=metric";

  Serial.print("[FETCH] URL: ");
  Serial.println(url);

  HTTPClient http;
  http.begin(url);
  int code = http.GET();

  Serial.print("[FETCH] HTTP code: ");
  Serial.println(code);

  if (code == 200) {
    String payload = http.getString();
    Serial.println("[FETCH] Payload received:");
    Serial.println(payload);

    DynamicJsonDocument doc(2048);
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
      Serial.print("[FETCH] JSON error: ");
      Serial.println(err.c_str());
      hasData = false;
      currentDesc = "JSON ERR";
    } else {
      currentTemp = doc["main"]["temp"];
      currentHumidity = doc["main"]["humidity"];
      currentDesc = String(doc["weather"][0]["description"]);
      currentDesc.toUpperCase();
      hasData = true;
      Serial.println("[FETCH] Parsed OK.");
    }
  } else {
    Serial.println("[FETCH] API ERROR!");
    hasData = false;
    currentDesc = "API ERR";
  }

  http.end();
}

// --- Get Current Time String (for logging) ---
String getTimeString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "NO_TIME";
  }

  char buffer[20];
  // Format: YYYY-MM-DD HH:MM:SS
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// --- Get Clock String (for display) ---
String getClockString() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "--:--:--";
  }

  char buffer[9];
  strftime(buffer, sizeof(buffer), "%H:%M:%S", &timeinfo);
  return String(buffer);
}

// --- Save Weather to SD (only if changed, CSV format) ---
void saveWeatherIfChanged() {
  if (!sdAvailable) {
    Serial.println("[SD] Not available, skipping save.");
    return;
  }
  if (!hasData) {
    Serial.println("[SD] No valid data, skipping save.");
    return;
  }

  bool changed = false;

  if (isnan(lastSavedTemp) || abs(currentTemp - lastSavedTemp) > 0.01) changed = true;
  if (currentHumidity != lastSavedHumidity) changed = true;
  if (currentDesc != lastSavedDesc) changed = true;

  if (!changed) {
    Serial.println("[SD] Weather unchanged, not writing.");
    return;
  }

  String timestamp = getTimeString();

  // CSV FORMAT: time,temp,humidity,description
  String line = timestamp + "," +
                String(currentTemp, 1) + "," +
                String(currentHumidity) + "," +
                currentDesc + "\n";

  Serial.print("[SD] Writing CSV line: ");
  Serial.println(line);

  File logFile = SD.open("/weather_log.csv", FILE_APPEND);
  if (!logFile) {
    Serial.println("[SD] Failed to open file for append.");
    return;
  }

  logFile.print(line);
  logFile.close();

  lastSavedTemp = currentTemp;
  lastSavedHumidity = currentHumidity;
  lastSavedDesc = currentDesc;

  Serial.println("[SD] Weather saved.");
}

// --- Draw UI ---
void drawUI() {
  Serial.println("[UI] Drawing UI...");
  gfx->fillScreen(0x0000);

  // Time at top
  String clockStr = getClockString();
  gfx->setCursor(10, 10);
  gfx->setTextColor(0xFFFF);
  gfx->setTextSize(2);
  gfx->print(clockStr);

  // Location
  gfx->setCursor(10, 40);
  gfx->setTextColor(0xFFFF);
  gfx->setTextSize(2);
  gfx->print(cityName);
  gfx->print(", ");
  gfx->print(country);

  // Temperature
  gfx->setCursor(10, 110);
  gfx->setTextSize(6);
  gfx->setTextColor(0xFFE0);
  if (hasData) {
    gfx->print(currentTemp, 1);
    gfx->print(" C");
  } else {
    gfx->print("--.- C");
  }

  // Description
  gfx->setCursor(10, 210);
  gfx->setTextSize(2);
  gfx->setTextColor(0x07E0);
  gfx->print("COND: ");
  gfx->print(currentDesc);

  // Humidity
  gfx->setCursor(10, 250);
  gfx->setTextSize(2);
  gfx->setTextColor(0x001F);
  gfx->print("HUMIDITY: ");
  gfx->print(currentHumidity);
  gfx->print("%");

  gfx->flush();
  Serial.println("[UI] UI drawn.");
}

// --- Setup ---
void setup() {
  Serial.begin(115200);
  Serial.println("\n[BOOT] Starting setup...");

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);
  Serial.println("[BOOT] Backlight ON");

  pinMode(TOUCH_RST, OUTPUT);
  digitalWrite(TOUCH_RST, HIGH);
  Serial.println("[BOOT] Touch reset HIGH");

  delay(100);

  Serial.println("[BOOT] Starting I2C...");
  Wire.begin(TOUCH_SDA, TOUCH_SCL);

  Serial.println("[BOOT] Starting display...");
  if (!gfx->begin()) {
    Serial.println("[ERROR] gfx->begin() FAILED!");
    while (1);
  }
  Serial.println("[BOOT] Display OK");

  // WiFi
  Serial.print("[WIFI] Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\n[WIFI] Connected!");

  // NTP Time
  Serial.println("[TIME] Configuring NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("[TIME] Failed to get time.");
  } else {
    Serial.println("[TIME] Time OK.");
  }

  // SD Card
  Serial.println("[SD] Initialising SD card (SPI)...");
  if (!SD.begin(SD_CS)) {
    Serial.println("[SD] SD.begin FAILED. SD logging disabled.");
    sdAvailable = false;
  } else {
    Serial.println("[SD] SD card OK.");
    sdAvailable = true;
  }

  // Initial fetch and draw
  fetchWeather();
  saveWeatherIfChanged();
  drawUI();

  // Initialise lastMinute to current minute
  if (getLocalTime(&timeinfo)) {
    lastMinute = timeinfo.tm_min;
    Serial.print("[TIME] Initial minute: ");
    Serial.println(lastMinute);
  } else {
    lastMinute = -1;
  }

  Serial.println("[BOOT] Setup complete.");
}

// --- Loop ---
void loop() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("[TIME] Failed to read time.");
    delay(500);
    return;
  }

  int currentMinute = timeinfo.tm_min;

  // Trigger update when the minute changes
  if (currentMinute != lastMinute) {
    Serial.print("[TIME] Minute changed: ");
    Serial.println(currentMinute);

    fetchWeather();
    saveWeatherIfChanged();
    drawUI();

    lastMinute = currentMinute;
  }

  // Optional touch debug
  int tx, ty;
  if (getTouch(tx, ty)) {
    Serial.print("[TOUCH] ");
    Serial.print(tx);
    Serial.print(", ");
    Serial.println(ty);
  }

  delay(200);
}
