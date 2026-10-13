// =============================================================================
// T02 - Buck converter test, Part B: the ESP32 as a real load
// -----------------------------------------------------------------------------
// Goal: prove the buck converter (set to 5.0 V in Part A) can power the ESP32
// while it does its most power-hungry job: Wi-Fi scanning (datasheet: about
// 240 mA average, 380 mA peak at 3.3 V).
//
// The board has NO serial monitor in Part B (USB is unplugged), so the LEDs
// tell you what happened:
//   At start-up, RGB LED for 2 s:  WHITE  = normal power-on   (good)
//                                  RED    = brown-out reset  (supply too weak / voltage dipped)
//                                  YELLOW = any other reset   (see serial when on USB)
//   Blue LED (GPIO 2) blinks 1x per second  -> program running
//   RGB short flash after each Wi-Fi scan:   GREEN = networks found, RED = none found
//
// Wiring:
//   Upload + first check : USB only (buck NOT connected).
//   Part B               : USB UNPLUGGED. Buck OUT+ -> ESP32 5V pin, OUT- -> ESP32 GND.
// =============================================================================

#include <Arduino.h>
#include <WiFi.h>
#include <esp_system.h>   // esp_reset_reason()

// ---- Pins (from the Freenove pinout PDF) ------------------------------------
constexpr uint8_t PIN_LED_BLUE = 2;    // on-board LED marked "IO2"
constexpr uint8_t PIN_LED_RGB  = 16;   // on-board WS2812 RGB LED marked "16"

// ---- Timing -----------------------------------------------------------------
constexpr uint32_t BLINK_INTERVAL_MS = 500;    // blue LED toggles every 0.5 s -> 1 Hz
constexpr uint32_t SCAN_INTERVAL_MS  = 4000;   // start a new Wi-Fi scan every 4 s
constexpr uint32_t BOOT_SIGNAL_MS    = 2000;   // how long the start-up colour shows
constexpr uint32_t FLASH_MS          = 200;    // length of the green/red scan flash
constexpr uint8_t  RGB_LEVEL         = 20;     // 0..255, kept low (very bright LED)

// ---- State ------------------------------------------------------------------
bool     blueLedOn     = false;
bool     scanRunning   = false;
bool     flashOn       = false;
uint32_t lastBlinkMs   = 0;
uint32_t lastScanMs    = 0;
uint32_t flashStartMs  = 0;
uint32_t scanCount     = 0;

// -----------------------------------------------------------------------------
void setRgb(uint8_t r, uint8_t g, uint8_t b) {
  neopixelWrite(PIN_LED_RGB, r, g, b);
}

// Translate the reset reason into a short text (for the serial monitor on USB)
const char* resetReasonText(esp_reset_reason_t reason) {
  switch (reason) {
    case ESP_RST_POWERON:  return "power-on (or EN button)";
    case ESP_RST_BROWNOUT: return "BROWN-OUT: supply voltage dropped too low";
    case ESP_RST_SW:       return "software restart";
    case ESP_RST_PANIC:    return "crash (panic)";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT:      return "watchdog";
    default:               return "other";
  }
}

void showBootSignal(esp_reset_reason_t reason) {
  if (reason == ESP_RST_POWERON)       setRgb(RGB_LEVEL, RGB_LEVEL, RGB_LEVEL);  // white
  else if (reason == ESP_RST_BROWNOUT) setRgb(RGB_LEVEL, 0, 0);                  // red
  else                                 setRgb(RGB_LEVEL, RGB_LEVEL, 0);          // yellow
  delay(BOOT_SIGNAL_MS);   // only in setup(): the user must see it before scanning starts
  setRgb(0, 0, 0);
}

// -----------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(300);

  pinMode(PIN_LED_BLUE, OUTPUT);
  digitalWrite(PIN_LED_BLUE, LOW);

  esp_reset_reason_t reason = esp_reset_reason();
  Serial.println();
  Serial.println(F("=== T02 Part B: ESP32 as load for the buck converter ==="));
  Serial.printf("Reset reason: %s\n", resetReasonText(reason));
  showBootSignal(reason);

  WiFi.mode(WIFI_STA);     // station mode: needed for scanning
  WiFi.disconnect();       // we only scan, we never connect
}

void loop() {
  uint32_t now = millis();

  // 1) Blue LED heartbeat (non-blocking)
  if (now - lastBlinkMs >= BLINK_INTERVAL_MS) {
    lastBlinkMs = now;
    blueLedOn = !blueLedOn;
    digitalWrite(PIN_LED_BLUE, blueLedOn ? HIGH : LOW);
  }

  // 2) Start a Wi-Fi scan every few seconds. "true" = asynchronous:
  //    the scan runs in the background, so loop() keeps running.
  if (!scanRunning && now - lastScanMs >= SCAN_INTERVAL_MS) {
    lastScanMs = now;
    WiFi.scanNetworks(true);
    scanRunning = true;
  }

  // 3) Check if the background scan has finished.
  //    scanComplete(): -1 = still running, -2 = not started / failed, >= 0 = number found
  if (scanRunning) {
    int16_t result = WiFi.scanComplete();
    if (result >= 0 || result == WIFI_SCAN_FAILED) {
      scanRunning = false;
      scanCount++;
      int found = result > 0 ? result : 0;
      Serial.printf("Scan %lu: %d network(s)\n", (unsigned long)scanCount, found);
      if (found > 0) setRgb(0, RGB_LEVEL, 0);   // green flash
      else           setRgb(RGB_LEVEL, 0, 0);   // red flash
      flashOn = true;
      flashStartMs = now;
      WiFi.scanDelete();                       // free the result memory
    }
  }

  // 4) End the green/red flash after a short time (non-blocking)
  if (flashOn && now - flashStartMs >= FLASH_MS) {
    flashOn = false;
    setRgb(0, 0, 0);
  }
}
