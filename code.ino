/*
 * ===============================================
 * SPROAC - Smart Produce Rotting & Oxidation Alert Controller
 * Version 3.2 - SEN0376 I2C Alcohol Sensor Fix
 * WiFi Dashboard + Google Sheets + OLED Display
 * ===============================================
 *
 * ============================================================
 * WIRING GUIDE - SEN0376 Alcohol Sensor (I2C) to XIAO ESP32-C3
 * ============================================================
 *
 *  SEN0376 Pin  |  XIAO ESP32-C3 Pin  |  Notes
 * --------------|---------------------|------------------------
 *   VCC         |  3.3V (or 5V)       |  Sensor works 3.3–5.5V
 *   GND         |  GND                |  Common ground
 *   SDA         |  D4 (GPIO 6)        |  I2C Data (shared bus)
 *   SCL         |  D5 (GPIO 7)        |  I2C Clock (shared bus
 *
 *  SEN0376 DIP Switch default: A0=1, A1=1 → I2C Address = 0x75
 *  (ADDRESS_3 in DFRobot library)
 *
 *  NOTE: SDA and SCL are SHARED with the OLED display on the
 *  same Wire bus. No separate pins needed — just connect both
 *  the OLED and the SEN0376 to the same SDA/SCL lines.
 *
 *  H2S Sensor (Fermion) → GPIO 3 (D1) — ANALOG, unchanged
 *  MQ-135               → GPIO 4 (D2) — ANALOG, unchanged
 *  DHT11                → GPIO 2 (D0) — Digital, unchanged
 *
 * Library Required: DFRobot_Alcohol
 *   Install via Arduino Library Manager:
 *   Search "DFRobot_Alcohol" → Install
 *   OR from: https://github.com/DFRobot/DFRobot_Alcohol
 * ============================================================
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ArduinoJson.h>
#include "FS.h"
#include "SPIFFS.h"
#include "time.h"

// *** NEW: DFRobot Alcohol Sensor I2C Library ***
#include "DFRobot_Alcohol.h"

// ===============================================
// WiFi Configuration
// ===============================================
const char* wifi_ssid     = "Ashwin's iPhone";
const char* wifi_password = "ashwin3907";
const char* ap_ssid       = "SPROAC-Monitor";
const char* ap_password   = "sproac123";

// ===============================================
// Google Apps Script URL
// ===============================================
String GOOGLE_SCRIPT_URL = "https://script.google.com/macros/s/AKfycbzDMA3MQfWI9yOvNhE5pFj74-a5ckRa2FAxOByzUVhnaK2z_7RHiW-taCjA8jQALdUukg/exec";

// ===============================================
// Display Settings
// ===============================================
#define SCREEN_WIDTH    128
#define SCREEN_HEIGHT   64
#define OLED_RESET      -1
#define SCREEN_ADDRESS  0x3C   // OLED I2C address

// ===============================================
// XIAO ESP32-C3 Pin Configuration
// ===============================================
#define DHT_PIN     2    // D0 - DHT11
#define H2S_PIN     3    // D1 - Fermion H2S (analog)
#define MQ135_PIN   4    // D2 - MQ-135 (analog)
// ALCOHOL_PIN removed — now using I2C via DFRobot_Alcohol library

// Button Pins
#define BTN_UP      8    // D8
#define BTN_DOWN    9    // D9
#define BTN_ENTER   10   // D10
#define BTN_BACK    20   // D6

// I2C Pins for XIAO ESP32-C3
#define I2C_SDA     6    // D4
#define I2C_SCL     7    // D5

// ===============================================
// SEN0376 I2C Alcohol Sensor Setup
// ===============================================
// DIP Switch → I2C Address mapping:
//   A0=0, A1=0  → 0x72
//   A0=1, A1=0  → 0x73
//   A0=0, A1=1  → 0x74
//   A0=1, A1=1  → 0x75  ← DEFAULT (factory setting)
#define ALCOHOL_I2C_ADDRESS  0x75       // Change if your DIP switch differs
#define ALCOHOL_COLLECT_NUM  5          // Averaging samples (1–100)

DFRobot_Alcohol_I2C alcoholSensor(&Wire, ALCOHOL_I2C_ADDRESS);

// ===============================================
// DHT11 Configuration
// ===============================================
#define DHTTYPE DHT11
DHT dht(DHT_PIN, DHTTYPE);

// ===============================================
// Config & Logging
// ===============================================
#define CONFIG_FILE      "/config.json"
#define LOG_FILE         "/system.log"
#define MAX_LOG_SIZE     10000

// Alert thresholds
#define EARLY_DETECTION_THRESHOLD    0.6f
#define MODERATE_SPOILAGE_THRESHOLD  0.8f
#define SPOILAGE_STARTS_THRESHOLD    1.0f

// Timing
#define DEBOUNCE_DELAY          50
#define LONG_PRESS_TIME         1000
#define SENSOR_READ_INTERVAL    2000
#define ALERT_CHECK_INTERVAL    5000
#define DISPLAY_UPDATE_INTERVAL 33
#define ALERT_DISPLAY_TIME      10000
#define WATCHDOG_TIMEOUT        30000
#define DATA_SEND_INTERVAL      5000

// Error codes
#define ERROR_NONE               0
#define ERROR_DHT_READ           1
#define ERROR_DISPLAY            2
#define ERROR_SPIFFS             3
#define ERROR_SENSOR_CALIBRATION 4
#define ERROR_CONFIG_LOAD        5

// ===============================================
// Objects
// ===============================================
WebServer server(80);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// NTP
const char* ntpServer       = "pool.ntp.org";
const long  gmtOffset_sec   = 19800;   // GMT+5:30
const int   daylightOffset_sec = 0;

// ===============================================
// Enums
// ===============================================
enum State {
  STATE_MAIN_MENU, STATE_ITEM_MENU, STATE_MONITORING,
  STATE_PARAM_MENU, STATE_PARAM_ADJUST, STATE_ALERT_DISPLAY,
  STATE_ERROR_DISPLAY, STATE_TEST_MODE
};
enum Category    { CAT_VEGETABLES = 0, CAT_FRUITS = 1, CAT_COUNT = 2 };
enum VegetableItem { VEG_ONION = 0, VEG_GARLIC = 1, VEG_POTATO = 2, VEG_COUNT = 3 };
enum FruitItem   { FRUIT_TOMATO = 0, FRUIT_BANANA = 1, FRUIT_COUNT = 2 };
enum Parameter   { PARAM_TEMP = 0, PARAM_HUMIDITY = 1, PARAM_GAS = 2, PARAM_MONITOR = 3, PARAM_COUNT = 4 };
enum AlertLevel  { ALERT_NONE = 0, ALERT_EARLY = 1, ALERT_MODERATE = 2, ALERT_SPOILAGE = 3 };

// ===============================================
// Structures
// ===============================================
struct ItemThresholds {
  float tempMax, tempMin;
  float humidityMax, humidityMin;
  int   gasMax;
  bool  notifyEnabled;
};
struct ButtonState {
  bool pressed, lastState;
  unsigned long lastDebounceTime, pressStartTime;
  bool longPressDetected;
};
struct SensorHealth {
  bool dhtOk, h2sOk, mq135Ok, alcoholOk;
  unsigned long lastSuccessfulRead;
  int consecutiveFailures;
};

// ===============================================
// Global Variables
// ===============================================
State      currentState    = STATE_MAIN_MENU;
int        selectedCategory = 0;
int        selectedItem    = 0;
int        selectedParameter = 0;
int        menuCursor      = 0;
int        adjustCursor    = 0;
AlertLevel currentAlert    = ALERT_NONE;
bool       isMonitoring    = false;
int        currentError    = ERROR_NONE;
bool       alcoholReady    = false;   // tracks I2C init success

ButtonState btnUp, btnDown, btnEnter, btnBack;

unsigned long lastSensorRead   = 0;
unsigned long lastAlertCheck   = 0;
unsigned long lastDisplayUpdate = 0;
unsigned long alertDisplayTime = 0;
unsigned long systemStartTime  = 0;
unsigned long lastWatchdogReset = 0;
unsigned long lastSendTime     = 0;

const char* categoryNames[]  = {"VEGETABLES", "FRUITS"};
const char* vegetableNames[] = {"Onion", "Garlic", "Potato"};
const char* fruitNames[]     = {"Tomato", "Banana"};
const char* paramNames[]     = {"Temperature", "Humidity", "Gas Level", "START MONITOR"};

ItemThresholds itemThresholds[5];   // [0..2] vegs, [3..4] fruits

float currentTemperature = 0.0f;
float currentHumidity    = 0.0f;
int   currentGasLevel    = 0;
int   currentH2SValue    = 0;
int   currentMQ135Value  = 0;
float currentAlcoholPPM  = 0.0f;   // float for I2C sensor (0–5 ppm)
int   currentAlcoholValue = 0;     // integer PPM x 100 for display/sheets

SensorHealth sensorHealth = {true, true, true, true, 0, 0};

float h2sBaselineVoltage   = 0.0f;
float mq135BaselineVoltage = 0.0f;

unsigned long totalAlerts   = 0;
unsigned long totalReadings = 0;

// ===============================================
// Forward Declarations
// ===============================================
void initButtonState(ButtonState* btn);
void handleButtons();
void handleButton(int pin, ButtonState* btn, void (*callback)());
void onButtonUp(); void onButtonDown(); void onButtonEnter(); void onButtonBack();
void showSplashScreen();
bool calibrateSensors();
void readAllSensors();
int  readH2S();
int  readMQ135();
float readAlcohol_I2C();
void checkSpoilageAlerts();
int  getItemIndex();
const char* getCurrentItemName();
void adjustValueUp(); void adjustValueDown();
void updateDisplay();
void displayMainMenu(); void displayItemMenu(); void displayParamMenu();
void displayParamAdjust(); void displayMonitoring(); void displayAlert(); void displayError();
bool loadConfiguration(); bool saveConfiguration();
void logMessage(const char* level, const char* message);
void checkSensorHealth(); void performSystemTest(); void resetWatchdog();
float readDHTTemperature(); float readDHTHumidity();
bool validateSensorReading(float value, float minV, float maxV);
void setupWebDashboard(); void updateWebDashboard();
String getChildFriendlyHTML();
void handleRoot(); void handleAPI(); void handleNotFound();
String getCurrentItemForWeb(); String getAlertStatusEmoji();
void setupGoogleSheetsIntegration(); void updateGoogleSheets();
bool sendDataToGoogleSheets(float temperature, float humidity, int h2s, int mq135, int alcohol);
void connectToWiFi();
String getCurrentCategoryName();

// ===============================================
// SETUP
// ===============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n========================================");
  Serial.println("SPROAC v3.2 - I2C Alcohol Sensor");
  Serial.println("========================================\n");

  systemStartTime   = millis();
  lastWatchdogReset = millis();

  // Buttons
  pinMode(BTN_UP,    INPUT_PULLUP);
  pinMode(BTN_DOWN,  INPUT_PULLUP);
  pinMode(BTN_ENTER, INPUT_PULLUP);
  pinMode(BTN_BACK,  INPUT_PULLUP);

  // Analog sensor pins
  pinMode(H2S_PIN,   INPUT);
  pinMode(MQ135_PIN, INPUT);

  initButtonState(&btnUp);
  initButtonState(&btnDown);
  initButtonState(&btnEnter);
  initButtonState(&btnBack);

  // SPIFFS
  Serial.print("Initializing SPIFFS... ");
  if (!SPIFFS.begin(true)) {
    Serial.println("FAILED!");
    currentError = ERROR_SPIFFS;
  } else {
    Serial.println("OK");
    logMessage("INFO", "System starting");
  }

  // I2C — shared bus: OLED + SEN0376
  Serial.print("Initializing I2C bus (SDA=6, SCL=7)... ");
  Wire.begin(I2C_SDA, I2C_SCL);
  Serial.println("OK");

  // OLED Display
  Serial.print("Initializing OLED... ");
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("FAILED!");
    currentError = ERROR_DISPLAY;
  } else {
    Serial.println("OK");
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.display();
  }

  showSplashScreen();
  delay(2000);

  // SEN0376 Alcohol Sensor (I2C)
  Serial.print("Initializing SEN0376 Alcohol Sensor (I2C 0x75)... ");
  int alcoholRetry = 0;
  // NOTE: DFRobot library calls Wire.begin() internally — re-init with
  // custom SDA/SCL pins immediately after to restore XIAO C3 pin mapping.
  while (!alcoholSensor.begin() && alcoholRetry < 5) {
    Wire.begin(I2C_SDA, I2C_SCL);  // restore custom pins after lib resets them
    Serial.print(".");
    delay(1000);
    alcoholRetry++;
  }
  Wire.begin(I2C_SDA, I2C_SCL);   // always restore after final begin() call
  if (alcoholRetry < 5) {
    alcoholReady = true;
    sensorHealth.alcoholOk = true;
    Serial.println(" OK!");
    Serial.println("  Preheating sensor (3 min recommended for accuracy)...");
    // Show preheat on OLED
    if (currentError != ERROR_DISPLAY) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(5, 10); display.println("Alcohol Sensor");
      display.setCursor(5, 22); display.println("Preheating...");
      display.setCursor(5, 34); display.println("~3 min for full");
      display.setCursor(5, 46); display.println("accuracy");
      display.display();
    }
    delay(3000); // Brief wait — full 3-min preheat happens passively
  } else {
    alcoholReady = false;
    sensorHealth.alcoholOk = false;
    Serial.println(" FAILED! Check wiring:");
    Serial.println("  SEN0376 SDA -> XIAO D4 (GPIO6)");
    Serial.println("  SEN0376 SCL -> XIAO D5 (GPIO7)");
    Serial.println("  SEN0376 VCC -> 3.3V");
    Serial.println("  SEN0376 GND -> GND");
    Serial.println("  DIP Switch: A0=1, A1=1 (default 0x75)");
  }

  // DHT11
  Serial.print("Initializing DHT11... ");
  dht.begin();
  delay(1000);
  float testTemp = readDHTTemperature();
  if (isnan(testTemp)) {
    Serial.println("FAILED!");
    sensorHealth.dhtOk = false;
  } else {
    Serial.print("OK ("); Serial.print(testTemp); Serial.println("°C)");
  }

  // Configuration
  Serial.print("Loading configuration... ");
  if (loadConfiguration()) {
    Serial.println("OK");
  } else {
    Serial.println("Using defaults");
  }

  // Calibrate analog sensors
  Serial.println("\nCalibrating analog sensors...");
  if (!calibrateSensors()) {
    Serial.println("WARNING: Calibration had errors");
  } else {
    Serial.println("Calibration complete!");
  }

  performSystemTest();
  setupGoogleSheetsIntegration();
  setupWebDashboard();

  currentState = STATE_MAIN_MENU;
  Serial.println("\n========================================");
  Serial.println("System Ready!");
  Serial.println("========================================\n");
  logMessage("INFO", "System initialized successfully");
}

// ===============================================
// MAIN LOOP
// ===============================================
void loop() {
  resetWatchdog();
  handleButtons();

  if (millis() - lastSensorRead > SENSOR_READ_INTERVAL) {
    if (isMonitoring) {
      readAllSensors();
      checkSensorHealth();
    }
    lastSensorRead = millis();
  }

  if (isMonitoring && (millis() - lastAlertCheck > ALERT_CHECK_INTERVAL)) {
    checkSpoilageAlerts();
    lastAlertCheck = millis();
  }

  if (millis() - lastDisplayUpdate > DISPLAY_UPDATE_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = millis();
  }

  if (currentState == STATE_ALERT_DISPLAY &&
      (millis() - alertDisplayTime > ALERT_DISPLAY_TIME)) {
    currentState = STATE_MONITORING;
  }

  updateWebDashboard();
  updateGoogleSheets();
  delay(10);
}

// ===============================================
// ALCOHOL SENSOR — I2C READ (SEN0376)
// ===============================================
float readAlcohol_I2C() {
  if (!alcoholReady || !sensorHealth.alcoholOk) {
    return 0.0f;
  }
  // readAlcoholData() returns concentration in PPM (0–5 ppm range)
  // ALCOHOL_COLLECT_NUM samples are averaged internally by the library
  float ppm = alcoholSensor.readAlcoholData(ALCOHOL_COLLECT_NUM);
  if (ppm < 0.0f) ppm = 0.0f;
  if (ppm > 5.0f) ppm = 5.0f;
  return ppm;
}

// ===============================================
// SENSOR READING
// ===============================================
float readDHTTemperature() {
  float temp = dht.readTemperature();
  return validateSensorReading(temp, -40, 80) ? temp : NAN;
}

float readDHTHumidity() {
  float h = dht.readHumidity();
  return validateSensorReading(h, 0, 100) ? h : NAN;
}

bool validateSensorReading(float value, float minV, float maxV) {
  if (isnan(value)) return false;
  return (value >= minV && value <= maxV);
}

void readAllSensors() {
  totalReadings++;

  // DHT11
  currentTemperature = readDHTTemperature();
  currentHumidity    = readDHTHumidity();
  if (isnan(currentTemperature) || isnan(currentHumidity)) {
    sensorHealth.dhtOk = false;
    sensorHealth.consecutiveFailures++;
    currentTemperature = 0.0f;
    currentHumidity    = 0.0f;
  } else {
    sensorHealth.dhtOk = true;
    sensorHealth.lastSuccessfulRead = millis();
    sensorHealth.consecutiveFailures = 0;
  }

  // Analog gas sensors
  currentH2SValue  = readH2S();
  currentMQ135Value = readMQ135();

  // I2C Alcohol sensor
  currentAlcoholPPM   = readAlcohol_I2C();
  // Scale to integer 0–500 for threshold comparison (x100 of 0–5 ppm)
  currentAlcoholValue = (int)(currentAlcoholPPM * 100.0f);

  // Select gas level for current item
  if (selectedCategory == CAT_VEGETABLES) {
    switch (selectedItem) {
      case VEG_ONION:
      case VEG_GARLIC: currentGasLevel = currentH2SValue;   break;
      case VEG_POTATO: currentGasLevel = currentMQ135Value; break;
    }
  } else {
    // Fruits: use alcohol (scaled to 0–500 for threshold comparison)
    currentGasLevel = currentAlcoholValue;
  }

  if (totalReadings % 30 == 0) {
    char logMsg[120];
    snprintf(logMsg, sizeof(logMsg),
             "T=%.1f H=%.0f H2S=%d MQ135=%d Alc=%.2fppm",
             currentTemperature, currentHumidity,
             currentH2SValue, currentMQ135Value, currentAlcoholPPM);
    logMessage("DATA", logMsg);
  }

  Serial.printf("T:%.1fC H:%.0f%% H2S:%dppm MQ135:%dppm Alc:%.2fppm\n",
                currentTemperature, currentHumidity,
                currentH2SValue, currentMQ135Value, currentAlcoholPPM);
}

int readH2S() {
  if (!sensorHealth.h2sOk) return 0;
  int adc = analogRead(H2S_PIN);
  float voltage = adc * (3.3f / 4095.0f);
  float adjusted = voltage - h2sBaselineVoltage;
  if (adjusted < 0) adjusted = 0;
  int ppm = (int)(adjusted / 0.020f);
  return constrain(ppm, 0, 100);
}

int readMQ135() {
  if (!sensorHealth.mq135Ok) return 0;
  int raw = analogRead(MQ135_PIN);
  int ppm = map(raw, 0, 4095, 0, 500);
  static int readings[5] = {0};
  static int idx = 0;
  readings[idx] = ppm;
  idx = (idx + 1) % 5;
  int sum = 0;
  for (int i = 0; i < 5; i++) sum += readings[i];
  return sum / 5;
}

// ===============================================
// CALIBRATION — Analog sensors only
// ===============================================
bool calibrateSensors() {
  if (currentError != ERROR_DISPLAY) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(5, 15); display.println("Calibrating...");
    display.setCursor(5, 28); display.println("Keep in clean air");
    display.setCursor(5, 41); display.println("Please wait...");
    display.display();
  }
  delay(3000);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  bool ok = true;

  // H2S
  Serial.print("  H2S sensor... ");
  float sum = 0; int valid = 0;
  for (int i = 0; i < 100; i++) {
    int adc = analogRead(H2S_PIN);
    if (adc >= 0 && adc <= 4095) { sum += adc * (3.3f / 4095.0f); valid++; }
    delay(50);
  }
  if (valid > 50) {
    h2sBaselineVoltage = sum / valid;
    Serial.printf("OK (%.3fV)\n", h2sBaselineVoltage);
  } else { Serial.println("FAILED"); sensorHealth.h2sOk = false; ok = false; }

  // MQ135
  Serial.print("  MQ135 sensor... ");
  sum = 0; valid = 0;
  for (int i = 0; i < 100; i++) {
    int adc = analogRead(MQ135_PIN);
    if (adc >= 0 && adc <= 4095) { sum += adc * (3.3f / 4095.0f); valid++; }
    delay(50);
  }
  if (valid > 50) {
    mq135BaselineVoltage = sum / valid;
    Serial.printf("OK (%.3fV)\n", mq135BaselineVoltage);
  } else { Serial.println("FAILED"); sensorHealth.mq135Ok = false; ok = false; }

  if (currentError != ERROR_DISPLAY) {
    display.clearDisplay();
    display.setCursor(10, 28);
    display.println(ok ? "Calibration Done!" : "Partial Calib!");
    display.display();
    delay(1500);
  }
  return ok;
}

// ===============================================
// ALERT CHECKING
// ===============================================
void checkSpoilageAlerts() {
  int itemIndex = getItemIndex();
  ItemThresholds* thresh = &itemThresholds[itemIndex];
  AlertLevel newAlert = ALERT_NONE;

  if (currentTemperature > thresh->tempMax || currentTemperature < thresh->tempMin)
    newAlert = ALERT_MODERATE;

  if (currentHumidity > thresh->humidityMax || currentHumidity < thresh->humidityMin)
    if (newAlert < ALERT_MODERATE) newAlert = ALERT_EARLY;

  if (thresh->gasMax > 0) {
    float ratio = (float)currentGasLevel / (float)thresh->gasMax;
    if      (ratio >= SPOILAGE_STARTS_THRESHOLD)   newAlert = ALERT_SPOILAGE;
    else if (ratio >= MODERATE_SPOILAGE_THRESHOLD && newAlert < ALERT_MODERATE) newAlert = ALERT_MODERATE;
    else if (ratio >= EARLY_DETECTION_THRESHOLD   && newAlert < ALERT_EARLY)    newAlert = ALERT_EARLY;
  }

  if (newAlert > currentAlert && thresh->notifyEnabled) {
    currentAlert = newAlert;
    currentState = STATE_ALERT_DISPLAY;
    alertDisplayTime = millis();
    totalAlerts++;
    char msg[50]; snprintf(msg, sizeof(msg), "Alert Level %d triggered", newAlert);
    logMessage("ALERT", msg);
  } else if (newAlert < currentAlert) {
    currentAlert = newAlert;
  }
}

// ===============================================
// GOOGLE SHEETS
// ===============================================
void connectToWiFi() {
  Serial.println("\n========================================");
  Serial.printf("Connecting to WiFi: %s\n", wifi_ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);
  Serial.print("Connecting");
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500); Serial.print("."); attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n✓ WiFi Connected! IP: %s\n", WiFi.localIP().toString().c_str());
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    Serial.println("✓ NTP time synchronized");
  } else {
    Serial.println("\n✗ WiFi Connection Failed!");
  }
  Serial.println("========================================\n");
}

bool sendDataToGoogleSheets(float temperature, float humidity, int h2s, int mq135, int alcohol) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.begin(GOOGLE_SCRIPT_URL);
  http.addHeader("Content-Type", "application/json");

  // alcohol here is currentAlcoholValue (0–500, scaled x100 of PPM)
  // Send as float PPM for Google Sheets readability
  float alcoholFloat = alcohol / 100.0f;

  String json = "{";
  json += "\"temperature\":" + String(temperature, 1) + ",";
  json += "\"humidity\":"    + String(humidity, 1)    + ",";
  json += "\"h2s\":"         + String(h2s)            + ",";
  json += "\"mq135\":"       + String(mq135)          + ",";
  json += "\"alcohol\":"     + String(alcoholFloat, 2);
  json += "}";

  Serial.printf("Sending → T:%.1f H:%.1f H2S:%d MQ135:%d Alc:%.2fppm\n",
                temperature, humidity, h2s, mq135, alcoholFloat);

  int code = http.POST(json);
  bool ok = (code == 200 || code == 302);
  if (ok) Serial.println("✓ Sheets updated");
  else    Serial.printf("✗ HTTP %d\n", code);
  http.end();
  return ok;
}

void setupGoogleSheetsIntegration() {
  Serial.println("╔════════════════════════════╗");
  Serial.println("║ Google Sheets Integration  ║");
  Serial.println("╚════════════════════════════╝");
  connectToWiFi();
}

void updateGoogleSheets() {
  if (!isMonitoring) return;
  if (millis() - lastSendTime >= DATA_SEND_INTERVAL) {
    sendDataToGoogleSheets(currentTemperature, currentHumidity,
                           currentH2SValue, currentMQ135Value,
                           currentAlcoholValue);
    lastSendTime = millis();
  }
}

// ===============================================
// WEB DASHBOARD
// ===============================================
String getCurrentItemForWeb() {
  return (selectedCategory == 0) ? String(vegetableNames[selectedItem])
                                 : String(fruitNames[selectedItem]);
}

String getAlertStatusEmoji() {
  switch (currentAlert) {
    case 0: return "😊 All Good!";
    case 1: return "⚠️ Early Warning";
    case 2: return "🟡 Moderate Risk";
    case 3: return "🚨 Spoilage Alert!";
    default: return "❓ Unknown";
  }
}

String getChildFriendlyHTML() {
  String alc = String(currentAlcoholPPM, 2);
  String html = R"rawliteral(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta http-equiv="refresh" content="5">
<title>🌈 SPROAC Dashboard</title>
<style>
body{font-family:Arial,sans-serif;background:#f0f8ff;margin:0;padding:10px}
h1{text-align:center;color:#333}
.banner{background:#4CAF50;color:#fff;padding:8px;border-radius:8px;text-align:center;margin:8px 0;font-weight:bold}
.section-title{background:#2196F3;color:#fff;padding:6px 12px;border-radius:6px;margin-top:12px}
.grid{display:flex;flex-wrap:wrap;gap:10px;margin:10px 0}
.card{background:#fff;border-radius:10px;padding:12px;min-width:140px;flex:1;box-shadow:0 2px 6px rgba(0,0,0,.15)}
.card h3{margin:4px 0;font-size:1.1em}
.card p{margin:3px 0;font-size:.9em}
.footer{text-align:center;color:#888;font-size:.8em;margin-top:16px}
</style></head><body>
<h1>🌈 SPROAC Dashboard</h1>
)rawliteral";

  if (isMonitoring) {
    html += "<div class='banner'>🟢 MONITORING: " + getCurrentItemForWeb() + " — " + getAlertStatusEmoji() + "</div>\n";
  }

  html += "<div class='section-title'>🥬 VEGETABLES</div>\n<div class='grid'>\n";

  // Onion
  html += "<div class='card'>🧅<h3>Onion</h3>";
  html += "<p>🌡️ " + String(currentTemperature, 1) + "°C</p>";
  html += "<p>💧 " + String((int)currentHumidity) + "%</p>";
  html += "<p>☁️ H2S: " + String(currentH2SValue) + " ppm</p>";
  html += "<p>📊 Max: " + String(itemThresholds[0].gasMax) + " ppm</p></div>\n";

  // Garlic
  html += "<div class='card'>🧄<h3>Garlic</h3>";
  html += "<p>🌡️ " + String(currentTemperature, 1) + "°C</p>";
  html += "<p>💧 " + String((int)currentHumidity) + "%</p>";
  html += "<p>☁️ H2S: " + String(currentH2SValue) + " ppm</p>";
  html += "<p>📊 Max: " + String(itemThresholds[1].gasMax) + " ppm</p></div>\n";

  // Potato
  html += "<div class='card'>🥔<h3>Potato</h3>";
  html += "<p>🌡️ " + String(currentTemperature, 1) + "°C</p>";
  html += "<p>💧 " + String((int)currentHumidity) + "%</p>";
  html += "<p>☁️ MQ135: " + String(currentMQ135Value) + " ppm</p>";
  html += "<p>📊 Max: " + String(itemThresholds[2].gasMax) + " ppm</p></div>\n";
  html += "</div>\n";

  html += "<div class='section-title'>🍎 FRUITS</div>\n<div class='grid'>\n";

  // Tomato
  html += "<div class='card'>🍅<h3>Tomato</h3>";
  html += "<p>🌡️ " + String(currentTemperature, 1) + "°C</p>";
  html += "<p>💧 " + String((int)currentHumidity) + "%</p>";
  html += "<p>☁️ Alcohol: " + alc + " ppm</p>";
  html += "<p>📊 Max: " + String(itemThresholds[3].gasMax / 100.0f, 2) + " ppm</p></div>\n";

  // Banana
  html += "<div class='card'>🍌<h3>Banana</h3>";
  html += "<p>🌡️ " + String(currentTemperature, 1) + "°C</p>";
  html += "<p>💧 " + String((int)currentHumidity) + "%</p>";
  html += "<p>☁️ Alcohol: " + alc + " ppm</p>";
  html += "<p>📊 Max: " + String(itemThresholds[4].gasMax / 100.0f, 2) + " ppm</p></div>\n";
  html += "</div>\n";

  html += "<div class='footer'>⏰ Auto-refresh 5s | IP: " + WiFi.localIP().toString();
  html += " | 🌟 SPROAC v3.2</div>\n</body></html>";
  return html;
}

void handleRoot()     { server.send(200, "text/html", getChildFriendlyHTML()); }
void handleNotFound() { server.send(404, "text/plain", "404 Not Found"); }
void handleAPI() {
  String json = "{";
  json += "\"temperature\":"  + String(currentTemperature, 1) + ",";
  json += "\"humidity\":"     + String(currentHumidity, 1)    + ",";
  json += "\"h2s\":"          + String(currentH2SValue)       + ",";
  json += "\"mq135\":"        + String(currentMQ135Value)     + ",";
  json += "\"alcohol_ppm\":"  + String(currentAlcoholPPM, 2) + ",";
  json += "\"item\":\""       + getCurrentItemForWeb()        + "\",";
  json += "\"isMonitoring\":" + String(isMonitoring ? "true" : "false") + ",";
  json += "\"alertLevel\":"   + String(currentAlert);
  json += "}";
  server.send(200, "application/json", json);
}

void setupWebDashboard() {
  Serial.println("╔════════════════════════════╗");
  Serial.println("║  SPROAC Web Dashboard      ║");
  Serial.println("╚════════════════════════════╝");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("📱 Dashboard: http://%s\n", WiFi.localIP().toString().c_str());
    if (MDNS.begin("sproac")) Serial.println("📡 Also: http://sproac.local");
  } else {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ap_ssid, ap_password);
    Serial.printf("📡 AP: %s  PW: %s  IP: %s\n",
                  ap_ssid, ap_password, WiFi.softAPIP().toString().c_str());
  }
  server.on("/",         handleRoot);
  server.on("/api/data", handleAPI);
  server.onNotFound(handleNotFound);
  server.begin();
  Serial.println("✅ Web Server Started!\n");
}

void updateWebDashboard() { server.handleClient(); }

// ===============================================
// BUTTON HANDLING
// ===============================================
void initButtonState(ButtonState* btn) {
  btn->pressed = false; btn->lastState = true;
  btn->lastDebounceTime = 0; btn->pressStartTime = 0;
  btn->longPressDetected = false;
}

void handleButtons() {
  handleButton(BTN_UP,    &btnUp,    onButtonUp);
  handleButton(BTN_DOWN,  &btnDown,  onButtonDown);
  handleButton(BTN_ENTER, &btnEnter, onButtonEnter);
  handleButton(BTN_BACK,  &btnBack,  onButtonBack);
}

void handleButton(int pin, ButtonState* btn, void (*callback)()) {
  bool reading = digitalRead(pin);
  if (reading != btn->lastState) btn->lastDebounceTime = millis();
  if ((millis() - btn->lastDebounceTime) > DEBOUNCE_DELAY) {
    if (reading != btn->pressed) {
      btn->pressed = reading;
      if (btn->pressed == LOW) {
        btn->pressStartTime = millis();
        btn->longPressDetected = false;
        callback();
      }
    }
  }
  btn->lastState = reading;
}

void onButtonUp() {
  switch (currentState) {
    case STATE_MAIN_MENU:   menuCursor = (menuCursor - 1 + CAT_COUNT) % CAT_COUNT; break;
    case STATE_ITEM_MENU: {
      int cnt = (selectedCategory == CAT_VEGETABLES) ? VEG_COUNT : FRUIT_COUNT;
      menuCursor = (menuCursor - 1 + cnt) % cnt; break;
    }
    case STATE_PARAM_MENU:  menuCursor = (menuCursor - 1 + PARAM_COUNT) % PARAM_COUNT; break;
    case STATE_PARAM_ADJUST: adjustValueUp(); break;
    default: break;
  }
}

void onButtonDown() {
  switch (currentState) {
    case STATE_MAIN_MENU:   menuCursor = (menuCursor + 1) % CAT_COUNT; break;
    case STATE_ITEM_MENU: {
      int cnt = (selectedCategory == CAT_VEGETABLES) ? VEG_COUNT : FRUIT_COUNT;
      menuCursor = (menuCursor + 1) % cnt; break;
    }
    case STATE_PARAM_MENU:  menuCursor = (menuCursor + 1) % PARAM_COUNT; break;
    case STATE_PARAM_ADJUST: adjustValueDown(); break;
    default: break;
  }
}

void onButtonEnter() {
  switch (currentState) {
    case STATE_MAIN_MENU:
      selectedCategory = menuCursor; menuCursor = 0;
      currentState = STATE_ITEM_MENU; break;
    case STATE_ITEM_MENU:
      selectedItem = menuCursor; menuCursor = 0;
      currentState = STATE_PARAM_MENU; break;
    case STATE_PARAM_MENU:
      if (menuCursor == PARAM_MONITOR) {
        isMonitoring = true; currentState = STATE_MONITORING;
        readAllSensors(); logMessage("INFO", "Monitoring started");
      } else {
        selectedParameter = menuCursor; adjustCursor = 0;
        currentState = STATE_PARAM_ADJUST;
      }
      break;
    case STATE_PARAM_ADJUST:
      if (selectedParameter == PARAM_GAS) {
        saveConfiguration(); currentState = STATE_PARAM_MENU;
      } else {
        adjustCursor = (adjustCursor == 0) ? 1 : 0;
      }
      break;
    case STATE_MONITORING:
      isMonitoring = false; currentState = STATE_PARAM_MENU;
      logMessage("INFO", "Monitoring stopped"); break;
    case STATE_ALERT_DISPLAY:
    case STATE_ERROR_DISPLAY:
      currentState = STATE_MONITORING; break;
    default: break;
  }
}

void onButtonBack() {
  switch (currentState) {
    case STATE_ITEM_MENU:   menuCursor = selectedCategory; currentState = STATE_MAIN_MENU; break;
    case STATE_PARAM_MENU:  menuCursor = selectedItem;    currentState = STATE_ITEM_MENU; break;
    case STATE_PARAM_ADJUST: saveConfiguration(); currentState = STATE_PARAM_MENU; break;
    case STATE_MONITORING:  isMonitoring = false; currentState = STATE_PARAM_MENU; break;
    case STATE_ALERT_DISPLAY:
    case STATE_ERROR_DISPLAY: currentState = STATE_MONITORING; break;
    default: break;
  }
}

// ===============================================
// DISPLAY
// ===============================================
void showSplashScreen() {
  if (currentError == ERROR_DISPLAY) return;
  display.clearDisplay();
  display.drawRect(0, 0, 128, 64, SSD1306_WHITE);
  display.drawRect(2, 2, 124, 60, SSD1306_WHITE);
  display.setTextSize(2); display.setCursor(20, 10); display.println("SPROAC");
  display.setTextSize(1); display.setCursor(5,  32); display.println("Smart Freshness");
  display.setCursor(18, 44); display.println("Monitor v3.2");
  display.setCursor(8,  54); display.println("I2C Alcohol Fix");
  display.display();
}

void updateDisplay() {
  if (currentError == ERROR_DISPLAY) return;
  display.clearDisplay();
  switch (currentState) {
    case STATE_MAIN_MENU:    displayMainMenu();   break;
    case STATE_ITEM_MENU:    displayItemMenu();   break;
    case STATE_PARAM_MENU:   displayParamMenu();  break;
    case STATE_PARAM_ADJUST: displayParamAdjust(); break;
    case STATE_MONITORING:   displayMonitoring(); break;
    case STATE_ALERT_DISPLAY: displayAlert();     break;
    case STATE_ERROR_DISPLAY: displayError();     break;
    default: break;
  }
  display.display();
}

void displayMainMenu() {
  display.setTextSize(1);
  display.setCursor(25, 0); display.println("MAIN MENU");
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  int y = 20;
  for (int i = 0; i < CAT_COUNT; i++) {
    if (i == menuCursor) {
      display.fillRect(0, y-2, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(15, y); display.print("> "); display.println(categoryNames[i]);
      display.setTextColor(SSD1306_WHITE);
    } else {
      display.setCursor(15, y); display.print("  "); display.println(categoryNames[i]);
    }
    y += 15;
  }
  display.drawLine(0, 54, 128, 54, SSD1306_WHITE);
  display.setCursor(5, 56); display.print("UP/DN");
  display.setCursor(68, 56); display.print("ENTER");
}

void displayItemMenu() {
  display.setTextSize(1);
  display.setCursor(0, 0); display.print("< "); display.println(categoryNames[selectedCategory]);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  int itemCount = (selectedCategory == CAT_VEGETABLES) ? VEG_COUNT : FRUIT_COUNT;
  const char** names = (selectedCategory == CAT_VEGETABLES) ? vegetableNames : fruitNames;
  int y = 18;
  for (int i = 0; i < itemCount; i++) {
    if (i == menuCursor) {
      display.fillRect(0, y-2, 128, 12, SSD1306_WHITE);
      display.setTextColor(SSD1306_BLACK);
      display.setCursor(15, y); display.print("> "); display.println(names[i]);
      display.setTextColor(SSD1306_WHITE);
    } else {
      display.setCursor(15, y); display.print("  "); display.println(names[i]);
    }
    y += 13;
  }
  display.drawLine(0, 54, 128, 54, SSD1306_WHITE);
  display.setCursor(5, 56); display.print("BACK");
  display.setCursor(68, 56); display.print("ENTER");
}

void displayParamMenu() {
  int idx = getItemIndex();
  ItemThresholds* t = &itemThresholds[idx];
  display.setTextSize(1);
  display.setCursor(0, 0); display.print("< "); display.println(getCurrentItemName());
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  int y = 15;
  for (int i = 0; i < PARAM_COUNT; i++) {
    bool sel = (i == menuCursor);
    if (sel) { display.fillRect(0, y-1, 128, 10, SSD1306_WHITE); display.setTextColor(SSD1306_BLACK); }
    display.setCursor(2, y);
    if (i == PARAM_MONITOR) { display.print("> "); display.println(paramNames[i]); }
    else {
      display.println(paramNames[i]);
      display.setCursor(70, y);
      switch (i) {
        case PARAM_TEMP:
          display.print(t->tempMin,1); display.print("-"); display.print(t->tempMax,1); display.print("C"); break;
        case PARAM_HUMIDITY:
          display.print((int)t->humidityMin); display.print("-"); display.print((int)t->humidityMax); display.print("%"); break;
        case PARAM_GAS:
          // For fruits, display as decimal PPM
          if (selectedCategory == CAT_FRUITS)
            display.print(t->gasMax / 100.0f, 1);
          else
            display.print(t->gasMax);
          display.print("ppm"); break;
      }
    }
    if (sel) display.setTextColor(SSD1306_WHITE);
    y += 11;
  }
  display.drawLine(0, 54, 128, 54, SSD1306_WHITE);
  display.setCursor(5, 56); display.print("BACK");
  display.setCursor(68, 56); display.print("ENTER");
}

void displayParamAdjust() {
  int idx = getItemIndex();
  ItemThresholds* t = &itemThresholds[idx];
  display.setTextSize(1);
  display.setCursor(0, 0); display.print("ADJUST "); display.println(paramNames[selectedParameter]);
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  if (selectedParameter == PARAM_TEMP || selectedParameter == PARAM_HUMIDITY) {
    display.setCursor(5, 18);
    display.print(adjustCursor == 0 ? "> " : "  ");
    display.print("Max: "); display.setTextSize(2);
    if (selectedParameter == PARAM_TEMP) { display.print(t->tempMax, 1); display.println("C"); }
    else { display.print((int)t->humidityMax); display.println("%"); }
    display.setTextSize(1);
    display.setCursor(5, 38);
    display.print(adjustCursor == 1 ? "> " : "  ");
    display.print("Min: "); display.setTextSize(2);
    if (selectedParameter == PARAM_TEMP) { display.print(t->tempMin, 1); display.println("C"); }
    else { display.print((int)t->humidityMin); display.println("%"); }
  } else if (selectedParameter == PARAM_GAS) {
    display.setCursor(5, 20); display.println("Max Gas Threshold:");
    display.setTextSize(2); display.setCursor(10, 35);
    if (selectedCategory == CAT_FRUITS) { display.print(t->gasMax / 100.0f, 2); display.println("ppm"); }
    else { display.print(t->gasMax); display.println("ppm"); }
  }
  display.setTextSize(1);
  display.drawLine(0, 54, 128, 54, SSD1306_WHITE);
  display.setCursor(5, 56); display.print(selectedParameter == PARAM_GAS ? "BACK=SAVE" : "ENTER=NEXT");
}

void displayMonitoring() {
  int idx = getItemIndex();
  ItemThresholds* t = &itemThresholds[idx];
  display.setTextSize(1);
  display.setCursor(0, 0); display.print("MON: "); display.println(getCurrentItemName());
  display.drawLine(0, 10, 128, 10, SSD1306_WHITE);
  display.setCursor(0, 14);
  display.print("T:"); display.print(currentTemperature, 1); display.print("C");
  if (currentTemperature > t->tempMax || currentTemperature < t->tempMin) display.print("!");
  display.setCursor(0, 24);
  display.print("H:"); display.print((int)currentHumidity); display.print("%");
  if (currentHumidity > t->humidityMax || currentHumidity < t->humidityMin) display.print("!");
  display.setCursor(0, 34);
  if (selectedCategory == CAT_FRUITS) {
    display.print("Alc:"); display.print(currentAlcoholPPM, 2); display.print("ppm");
  } else if (selectedItem == VEG_POTATO) {
    display.print("CO2:"); display.print(currentMQ135Value); display.print("ppm");
  } else {
    display.print("H2S:"); display.print(currentH2SValue); display.print("ppm");
  }
  display.setCursor(0, 44);
  display.print("Max:"); 
  if (selectedCategory == CAT_FRUITS) display.print(t->gasMax / 100.0f, 2);
  else display.print(t->gasMax);
  display.print("ppm");
  display.setCursor(0, 54);
  const char* statuses[] = {"OK", "EARLY WARN", "MOD RISK", "SPOILAGE!"};
  display.print(statuses[currentAlert]);
}

void displayAlert() {
  display.fillRect(0, 0, 128, 64, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK);
  display.setTextSize(2); display.setCursor(15, 5); display.println("! ALERT !");
  display.setTextSize(1);
  display.setCursor(5, 25);
  switch (currentAlert) {
    case ALERT_EARLY:
      display.println("Early Detection"); display.setCursor(5,38); display.println("Spoilage Starting"); break;
    case ALERT_MODERATE:
      display.println("Moderate Spoilage"); display.setCursor(5,38); display.println("Check Storage!"); break;
    case ALERT_SPOILAGE:
      display.println("SPOILAGE DETECTED"); display.setCursor(5,38); display.println("Action Required!"); break;
    default: break;
  }
  display.setCursor(5, 52);
  display.print(getCurrentItemName()); display.print(": ");
  if (selectedCategory == CAT_FRUITS) display.print(currentAlcoholPPM, 2);
  else display.print(currentGasLevel);
  display.print("ppm");
  display.setTextColor(SSD1306_WHITE);
}

void displayError() {
  display.setTextSize(1);
  display.setCursor(20, 10); display.println("ERROR");
  display.setCursor(5, 25);
  switch (currentError) {
    case ERROR_DHT_READ:           display.println("DHT Sensor Failure"); break;
    case ERROR_DISPLAY:            display.println("Display Error"); break;
    case ERROR_SPIFFS:             display.println("Storage Error"); break;
    case ERROR_SENSOR_CALIBRATION: display.println("Calibration Error"); break;
    case ERROR_CONFIG_LOAD:        display.println("Config Error"); break;
    default:                       display.println("Unknown Error"); break;
  }
  display.setCursor(5, 50); display.println("Press BACK");
}

// ===============================================
// CONFIGURATION
// ===============================================
bool loadConfiguration() {
  // Default values
  for (int i = 0; i < 5; i++) {
    itemThresholds[i] = {30.0f, 10.0f, 80.0f, 40.0f, 0, true};
  }
  // Gas thresholds
  // H2S in ppm (0–100 scale): Onion, Garlic
  itemThresholds[VEG_ONION].gasMax  = 50;
  itemThresholds[VEG_GARLIC].gasMax = 50;
  // MQ135 in ppm (0–500 scale): Potato
  itemThresholds[VEG_POTATO].gasMax = 200;
  // Alcohol in 0.01 ppm units (x100): Fruits
  // 20 = 0.20 ppm, 25 = 0.25 ppm
  itemThresholds[VEG_COUNT + FRUIT_TOMATO].gasMax  = 20;
  itemThresholds[VEG_COUNT + FRUIT_BANANA].gasMax  = 25;

  if (currentError == ERROR_SPIFFS) return false;
  if (!SPIFFS.exists(CONFIG_FILE)) { saveConfiguration(); return false; }

  File file = SPIFFS.open(CONFIG_FILE, "r");
  if (!file) return false;
  StaticJsonDocument<1024> doc;
  if (deserializeJson(doc, file) != DeserializationError::Ok) { file.close(); return false; }
  file.close();

  for (int i = 0; i < VEG_COUNT; i++) {
    const char* k = vegetableNames[i];
    if (doc.containsKey(k)) {
      itemThresholds[i].tempMax      = doc[k]["tempMax"] | 30.0f;
      itemThresholds[i].tempMin      = doc[k]["tempMin"] | 10.0f;
      itemThresholds[i].humidityMax  = doc[k]["humMax"]  | 80.0f;
      itemThresholds[i].humidityMin  = doc[k]["humMin"]  | 40.0f;
      itemThresholds[i].gasMax       = doc[k]["gas"]     | 100;
      itemThresholds[i].notifyEnabled= doc[k]["notify"]  | true;
    }
  }
  for (int i = 0; i < FRUIT_COUNT; i++) {
    const char* k = fruitNames[i];
    int idx = VEG_COUNT + i;
    if (doc.containsKey(k)) {
      itemThresholds[idx].tempMax      = doc[k]["tempMax"] | 30.0f;
      itemThresholds[idx].tempMin      = doc[k]["tempMin"] | 10.0f;
      itemThresholds[idx].humidityMax  = doc[k]["humMax"]  | 80.0f;
      itemThresholds[idx].humidityMin  = doc[k]["humMin"]  | 40.0f;
      itemThresholds[idx].gasMax       = doc[k]["gas"]     | 25;
      itemThresholds[idx].notifyEnabled= doc[k]["notify"]  | true;
    }
  }
  return true;
}

bool saveConfiguration() {
  if (currentError == ERROR_SPIFFS) return false;
  StaticJsonDocument<1024> doc;
  for (int i = 0; i < VEG_COUNT; i++) {
    const char* k = vegetableNames[i];
    doc[k]["tempMax"] = itemThresholds[i].tempMax;
    doc[k]["tempMin"] = itemThresholds[i].tempMin;
    doc[k]["humMax"]  = itemThresholds[i].humidityMax;
    doc[k]["humMin"]  = itemThresholds[i].humidityMin;
    doc[k]["gas"]     = itemThresholds[i].gasMax;
    doc[k]["notify"]  = itemThresholds[i].notifyEnabled;
  }
  for (int i = 0; i < FRUIT_COUNT; i++) {
    const char* k = fruitNames[i];
    int idx = VEG_COUNT + i;
    doc[k]["tempMax"] = itemThresholds[idx].tempMax;
    doc[k]["tempMin"] = itemThresholds[idx].tempMin;
    doc[k]["humMax"]  = itemThresholds[idx].humidityMax;
    doc[k]["humMin"]  = itemThresholds[idx].humidityMin;
    doc[k]["gas"]     = itemThresholds[idx].gasMax;
    doc[k]["notify"]  = itemThresholds[idx].notifyEnabled;
  }
  File file = SPIFFS.open(CONFIG_FILE, "w");
  if (!file) return false;
  serializeJson(doc, file);
  file.close();
  return true;
}

// ===============================================
// HELPERS
// ===============================================
int getItemIndex() {
  return (selectedCategory == CAT_VEGETABLES) ? selectedItem : (VEG_COUNT + selectedItem);
}
const char* getCurrentItemName() {
  return (selectedCategory == CAT_VEGETABLES) ? vegetableNames[selectedItem] : fruitNames[selectedItem];
}
String getCurrentCategoryName() { return String(categoryNames[selectedCategory]); }

void adjustValueUp() {
  int idx = getItemIndex();
  ItemThresholds* t = &itemThresholds[idx];
  switch (selectedParameter) {
    case PARAM_TEMP:
      if (adjustCursor == 0) t->tempMax = min(t->tempMax + 0.5f, 50.0f);
      else { t->tempMin += 0.5f; if (t->tempMin >= t->tempMax) t->tempMin = t->tempMax - 0.5f; }
      break;
    case PARAM_HUMIDITY:
      if (adjustCursor == 0) t->humidityMax = min(t->humidityMax + 1.0f, 100.0f);
      else { t->humidityMin += 1.0f; if (t->humidityMin >= t->humidityMax) t->humidityMin = t->humidityMax - 1.0f; }
      break;
    case PARAM_GAS:
      if (selectedCategory == CAT_FRUITS) {
        t->gasMax += 1; // +0.01 ppm steps
        if (t->gasMax > 500) t->gasMax = 500; // 5.00 ppm max
      } else if (selectedItem == VEG_POTATO) {
        t->gasMax += 5; if (t->gasMax > 500) t->gasMax = 500;
      } else {
        t->gasMax += 5; if (t->gasMax > 100) t->gasMax = 100;
      }
      break;
  }
}

void adjustValueDown() {
  int idx = getItemIndex();
  ItemThresholds* t = &itemThresholds[idx];
  switch (selectedParameter) {
    case PARAM_TEMP:
      if (adjustCursor == 0) { t->tempMax -= 0.5f; if (t->tempMax <= t->tempMin) t->tempMax = t->tempMin + 0.5f; }
      else t->tempMin = max(t->tempMin - 0.5f, 0.0f);
      break;
    case PARAM_HUMIDITY:
      if (adjustCursor == 0) { t->humidityMax -= 1.0f; if (t->humidityMax <= t->humidityMin) t->humidityMax = t->humidityMin + 1.0f; }
      else t->humidityMin = max(t->humidityMin - 1.0f, 0.0f);
      break;
    case PARAM_GAS:
      if (selectedCategory == CAT_FRUITS) {
        t->gasMax -= 1; if (t->gasMax < 0) t->gasMax = 0;
      } else {
        t->gasMax -= 5; if (t->gasMax < 0) t->gasMax = 0;
      }
      break;
  }
}

// ===============================================
// LOGGING & SYSTEM MONITORING
// ===============================================
void logMessage(const char* level, const char* message) {
  if (currentError == ERROR_SPIFFS) return;
  if (SPIFFS.exists(LOG_FILE)) {
    File f = SPIFFS.open(LOG_FILE, "r");
    if (f && f.size() > MAX_LOG_SIZE) { f.close(); SPIFFS.remove(LOG_FILE); }
    else if (f) f.close();
  }
  File f = SPIFFS.open(LOG_FILE, "a");
  if (f) {
    char buf[20]; snprintf(buf, sizeof(buf), "[%lu] ", (millis() - systemStartTime) / 1000);
    f.print(buf); f.print(level); f.print(": "); f.println(message);
    f.close();
  }
}

void checkSensorHealth() {
  if (sensorHealth.consecutiveFailures > 5)
    logMessage("WARNING", "Sensor health degraded");
  if (isMonitoring && (millis() - sensorHealth.lastSuccessfulRead > 30000))
    Serial.println("WARNING: No successful reads in 30s");
}

void resetWatchdog() {
  if (millis() - lastWatchdogReset > WATCHDOG_TIMEOUT)
    logMessage("WARNING", "Watchdog timeout");
  lastWatchdogReset = millis();
}

void performSystemTest() {
  Serial.println("\n--- System Test ---");
  float t = readDHTTemperature(), h = readDHTHumidity();
  if (!isnan(t)) Serial.printf("  DHT11: OK (T=%.1fC, H=%.0f%%)\n", t, h);
  else           Serial.println("  DHT11: FAILED");

  Serial.printf("  H2S raw ADC:   %d (baseline %.3fV)\n", analogRead(H2S_PIN),   h2sBaselineVoltage);
  Serial.printf("  MQ135 raw ADC: %d (baseline %.3fV)\n", analogRead(MQ135_PIN), mq135BaselineVoltage);

  if (alcoholReady) {
    float alc = readAlcohol_I2C();
    Serial.printf("  Alcohol (I2C): %.2f ppm\n", alc);
  } else {
    Serial.println("  Alcohol (I2C): Not initialized — check wiring");
  }

  if (currentError != ERROR_SPIFFS)
    Serial.printf("  SPIFFS: %u total, %u used bytes\n", SPIFFS.totalBytes(), SPIFFS.usedBytes());
  else
    Serial.println("  SPIFFS: FAILED");
  Serial.println("-------------------\n");
}