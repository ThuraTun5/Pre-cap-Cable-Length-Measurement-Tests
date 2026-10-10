// =============================================================================
// T01 - ESP32 board bring-up
// -----------------------------------------------------------------------------
// Goal: prove the board, the USB cable, the PC driver and the toolchain work.
//   1. Blue LED (GPIO 2) blinks            -> the program is running
//   2. RGB LED (GPIO 16) cycles colours    -> the on-board WS2812 works
//   3. Chip info is printed                -> serial monitor works
//   4. Wi-Fi networks are listed           -> the radio works
//
// Wiring: NONE. Only the USB-C cable to the PC.
//
// Serial monitor commands (type the letter, press Enter):
//   h = help   i = chip info   s = scan Wi-Fi   b = blink on/off   r = RGB on/off
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>

// ---- Pins (from the Freenove pinout PDF) ------------------------------------
constexpr uint8_t PIN_LED_BLUE = 2;    // on-board LED marked "IO2"
constexpr uint8_t PIN_LED_RGB  = 16;   // on-board WS2812 RGB LED marked "16"

// ---- Timing -----------------------------------------------------------------
constexpr uint32_t BLINK_INTERVAL_MS = 500;   // blue LED toggles every 0.5 s
constexpr uint32_t RGB_INTERVAL_MS   = 1000;  // RGB LED changes colour every 1 s
constexpr uint8_t  RGB_LEVEL         = 20;    // 0..255, kept low (it is very bright)
                                       // (not named RGB_BRIGHTNESS: the ESP32 core already
                                       //  defines a macro with that name -> compile error)

// ---- State ------------------------------------------------------------------
bool blinkEnabled = true;
bool rgbEnabled   = true;
bool blueLedOn    = false;
uint8_t rgbStep   = 0;                 // 0 = red, 1 = green, 2 = blue, 3 = off

uint32_t lastBlinkMs = 0;
uint32_t lastRgbMs   = 0;

// -----------------------------------------------------------------------------
void printHelp() {
  Serial.println();
  Serial.println(F("Commands: h = help | i = chip info | s = scan Wi-Fi | "
                   "b = blink on/off | r = RGB on/off"));
}

void printChipInfo() {
  Serial.println();
  Serial.println(F("----- Chip information -----"));
  Serial.printf("Chip model     : %s (revision %d)\n", ESP.getChipModel(), ESP.getChipRevision());
  Serial.printf("CPU cores      : %d @ %lu MHz\n", ESP.getChipCores(), (unsigned long)ESP.getCpuFreqMHz());
  Serial.printf("Flash size     : %lu MB\n", (unsigned long)(ESP.getFlashChipSize() / (1024 * 1024)));
  Serial.printf("Free heap (RAM): %lu bytes\n", (unsigned long)ESP.getFreeHeap());
  Serial.printf("Wi-Fi MAC      : %s\n", WiFi.macAddress().c_str());
  Serial.printf("SDK version    : %s\n", ESP.getSdkVersion());
  Serial.println(F("----------------------------"));
}

void scanWifi() {
  Serial.println();
  Serial.println(F("Scanning Wi-Fi (takes ~3 s)..."));
  int count = WiFi.scanNetworks();     // blocking: waits until the scan is done

  if (count <= 0) {
    Serial.println(F("No networks found. (Try near a router or phone hotspot.)"));
  } else {
    Serial.printf("Found %d network(s):\n", count);
    for (int i = 0; i < count; i++) {
      // RSSI = signal strength in dBm: -40 is excellent, -80 is weak
      Serial.printf("  %2d) %-32s  %4d dBm  channel %2d\n",
                    i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i), WiFi.channel(i));
    }
  }
  WiFi.scanDelete();                   // free the memory used by the results
}

void setRgb(uint8_t step) {
  // neopixelWrite(pin, red, green, blue) is built into Arduino-ESP32 2.0.x
  switch (step) {
    case 0:  neopixelWrite(PIN_LED_RGB, RGB_LEVEL, 0, 0); break;   // red
    case 1:  neopixelWrite(PIN_LED_RGB, 0, RGB_LEVEL, 0); break;   // green
    case 2:  neopixelWrite(PIN_LED_RGB, 0, 0, RGB_LEVEL); break;   // blue
    default: neopixelWrite(PIN_LED_RGB, 0, 0, 0);              break;   // off
  }
}

void handleSerialCommand() {
  if (!Serial.available()) return;

  char c = (char)Serial.read();
  switch (c) {
    case 'h': printHelp();     break;
    case 'i': printChipInfo(); break;
    case 's': scanWifi();      break;
    case 'b':
      blinkEnabled = !blinkEnabled;
      if (!blinkEnabled) digitalWrite(PIN_LED_BLUE, LOW);
      Serial.printf("\nBlue LED blinking: %s\n", blinkEnabled ? "ON" : "OFF");
      break;
    case 'r':
      rgbEnabled = !rgbEnabled;
      if (!rgbEnabled) setRgb(3);
      Serial.printf("\nRGB LED cycling: %s\n", rgbEnabled ? "ON" : "OFF");
      break;
    case '\r':
    case '\n':
      break;                           // ignore the Enter key itself
    default:
      Serial.printf("\nUnknown command '%c'. ", c);
      printHelp();
      break;
  }
}

// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(500);                          // give the serial monitor time to connect

  pinMode(PIN_LED_BLUE, OUTPUT);
  digitalWrite(PIN_LED_BLUE, LOW);
  setRgb(3);                           // RGB off at start

  WiFi.mode(WIFI_STA);                 // station mode: needed for scanning
  WiFi.disconnect();                   // we only scan, we do not connect

  Serial.println();
  Serial.println(F("=== T01 ESP32 board bring-up ==="));
  printChipInfo();
  scanWifi();
  printHelp();
}

void loop() {
  uint32_t now = millis();

  // Non-blocking blink: act only when enough time has passed.
  // (No delay() here, so the board can still react to serial commands.)
  if (blinkEnabled && now - lastBlinkMs >= BLINK_INTERVAL_MS) {
    lastBlinkMs = now;
    blueLedOn = !blueLedOn;
    digitalWrite(PIN_LED_BLUE, blueLedOn ? HIGH : LOW);
  }

  if (rgbEnabled && now - lastRgbMs >= RGB_INTERVAL_MS) {
    lastRgbMs = now;
    setRgb(rgbStep);
    rgbStep = (rgbStep + 1) % 4;
  }

  handleSerialCommand();
}
