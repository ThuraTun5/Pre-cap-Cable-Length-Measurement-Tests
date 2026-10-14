// =============================================================================
// T06 - USB serial link to the Raspberry Pi (protocol v1, see docs/PROTOCOL.md)
// -----------------------------------------------------------------------------
// Same measurement as T05 (encoder -> mm), but every output line is one JSON
// object ("JSON Lines"), so the Pi program can read it directly.
//
//   ESP32 -> Pi : {"type":"pos",...}  when the slider moves (at most 25 per second)
//                 {"type":"hb",...}   heartbeat, once per second while nothing moves
//                 {"type":"hello",...} after start-up and on request
//                 {"type":"ack",...}  answer to every command
//   Pi -> ESP32 : one command per line: z | o <mm> | c | c <mm> | d | r | i | p | h
//
// Link: USB cable, 115200 baud, 8N1, each line ends with '\n'.
// Wiring: same as T04/T05 (encoder -> 74LVC245 -> GPIO34/35).
// =============================================================================

#include <Arduino.h>
#include <ESP32Encoder.h>
#include <Preferences.h>

// ---- Pins (confirmed in T3/T4) ----------------------------------------------
constexpr uint8_t PIN_ENC_A    = 34;
constexpr uint8_t PIN_ENC_B    = 35;
constexpr uint8_t PIN_LED_BLUE = 2;
constexpr uint8_t PIN_LED_RGB  = 16;

// ---- Protocol ---------------------------------------------------------------
constexpr int      PROTOCOL_VERSION  = 1;
constexpr uint32_t BAUD              = 115200;
constexpr uint32_t POS_INTERVAL_MS   = 40;     // position at most 25x per second
constexpr uint32_t HEARTBEAT_MS      = 1000;   // heartbeat when nothing moves
constexpr size_t   TX_BUFFER_BYTES   = 2048;   // so printing never waits for the UART

// ---- Mechanics (see T05) ----------------------------------------------------
constexpr int32_t COUNTS_PER_TURN      = 2048 * 4;
constexpr double  MM_PER_TURN          = 2.0 * 20;                 // GT2 pitch x 20 teeth
constexpr double  NOMINAL_MM_PER_COUNT = MM_PER_TURN / COUNTS_PER_TURN;
constexpr double  CAL_MAX_DEVIATION    = 0.10;
constexpr int32_t CAL_MIN_COUNTS       = 8192;
constexpr uint16_t PCNT_FILTER         = 100;
constexpr uint32_t BLINK_INTERVAL_MS   = 500;

ESP32Encoder encoder;
Preferences  prefs;

// ---- Saved settings (NVS, shared with nothing else) -------------------------
double mmPerCount = NOMINAL_MM_PER_COUNT;
double offsetMm   = 0.0;
int8_t dirSign    = 1;

// ---- State ------------------------------------------------------------------
bool     zeroed        = false;   // false until 'z' after every start-up
bool     calStarted    = false;
int64_t  calStartCount = 0;
uint32_t seq           = 0;       // message number, +1 for every line sent
int64_t  lastSentCount = INT64_MIN;
int64_t  lastSpeedCount = 0;
uint32_t lastSpeedMs   = 0;
uint32_t lastPosMs     = 0;
uint32_t lastSentMs    = 0;
uint32_t lastBlinkMs   = 0;
bool     blueLedOn     = false;
char     lineBuf[48];
uint8_t  lineLen       = 0;

// -----------------------------------------------------------------------------
int64_t signedCount() { return dirSign * encoder.getCount(); }
double  travelMm(int64_t count) { return (double)count * mmPerCount; }

void saveSettings() {
  prefs.putDouble("mmpc", mmPerCount);
  prefs.putDouble("off", offsetMm);
  prefs.putChar("dir", dirSign);
}

// Every message starts with type, seq and t_ms. 'body' adds the rest (may be empty).
void sendLine(const char* type, const char* body) {
  seq++;
  lastSentMs = millis();
  Serial.printf("{\"type\":\"%s\",\"seq\":%lu,\"t_ms\":%lu%s%s}\n", type, (unsigned long)seq,
                (unsigned long)lastSentMs, body[0] ? "," : "", body);
}

void sendPosition(const char* type, int64_t count, double speed) {
  char body[200];
  double mm = travelMm(count);
  snprintf(body, sizeof(body),
           "\"count\":%lld,\"deg\":%.2f,\"mm\":%.2f,\"len_mm\":%.2f,\"v_mms\":%.1f,\"zeroed\":%s",
           (long long)count, (double)count * 360.0 / COUNTS_PER_TURN, mm, offsetMm + mm, speed,
           zeroed ? "true" : "false");
  sendLine(type, body);
}

void sendHello() {
  char body[200];
  snprintf(body, sizeof(body),
           "\"fw\":\"T06\",\"proto\":%d,\"mm_per_count\":%.7f,\"mm_per_turn\":%.4f,"
           "\"offset_mm\":%.2f,\"dir\":%d,\"zeroed\":%s",
           PROTOCOL_VERSION, mmPerCount, mmPerCount * COUNTS_PER_TURN, offsetMm, dirSign,
           zeroed ? "true" : "false");
  sendLine("hello", body);
}

// ack: ok = command accepted. msg must not contain double quotes.
void sendAck(char cmd, bool ok, const char* msg) {
  char body[200];
  snprintf(body, sizeof(body), "\"cmd\":\"%c\",\"ok\":%s,\"msg\":\"%s\"", cmd, ok ? "true" : "false", msg);
  sendLine("ack", body);
}

void calibrate(double distanceMm) {
  char msg[120];
  if (!calStarted) { sendAck('c', false, "send c alone at the START mark first"); return; }
  int64_t moved = signedCount() - calStartCount;
  int64_t size  = moved < 0 ? -moved : moved;
  if (size < CAL_MIN_COUNTS) { sendAck('c', false, "moved less than one turn (40 mm)"); return; }
  double candidate = distanceMm / (double)size;
  double change = candidate / NOMINAL_MM_PER_COUNT - 1.0;
  if (change > CAL_MAX_DEVIATION || change < -CAL_MAX_DEVIATION) {
    snprintf(msg, sizeof(msg), "rejected: scale change %+.1f percent", change * 100.0);
    sendAck('c', false, msg);
    return;
  }
  mmPerCount = candidate;
  calStarted = false;
  saveSettings();
  snprintf(msg, sizeof(msg), "calibrated: %.4f mm per turn (%+.3f percent)", mmPerCount * COUNTS_PER_TURN,
           change * 100.0);
  sendAck('c', true, msg);
}

void handleLine(char* line) {
  char cmd = line[0];
  char* arg = line + 1;
  while (*arg == ' ') arg++;
  bool hasArg = (*arg != '\0');
  double value = hasArg ? strtod(arg, nullptr) : 0.0;
  char msg[80];

  switch (cmd) {
    case 'z':
      encoder.clearCount();
      lastSpeedCount = 0;
      zeroed = true;
      sendAck('z', true, "zero set");
      break;
    case 'o':
      if (!hasArg) { sendAck('o', false, "usage: o <mm>"); break; }
      offsetMm = value;
      saveSettings();
      snprintf(msg, sizeof(msg), "offset %.2f mm", offsetMm);
      sendAck('o', true, msg);
      break;
    case 'c':
      if (!hasArg) {
        calStartCount = signedCount();
        calStarted = true;
        sendAck('c', true, "start mark saved; move a known distance, then send c <mm>");
      } else if (value > 0) {
        calibrate(value);
      } else {
        sendAck('c', false, "distance must be positive");
      }
      break;
    case 'd':
      dirSign = -dirSign;
      encoder.setCount(-encoder.getCount());
      saveSettings();
      sendAck('d', true, dirSign > 0 ? "direction normal" : "direction flipped");
      break;
    case 'r':
      mmPerCount = NOMINAL_MM_PER_COUNT;
      saveSettings();
      sendAck('r', true, "calibration reset to 40 mm per turn");
      break;
    case 'i':
      sendHello();
      break;
    case 'p':
      sendPosition("pos", signedCount(), 0.0);
      break;
    case 'h':
      sendAck('h', true, "commands: z, o <mm>, c, c <mm>, d, r, i, p, h");
      break;
    default:
      snprintf(msg, sizeof(msg), "unknown command (send h for help)");
      sendAck(cmd, false, msg);
      break;
  }
}

void readSerialLine() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      lineBuf[lineLen] = '\0';
      if (lineLen > 0) handleLine(lineBuf);
      lineLen = 0;
    } else if (lineLen < sizeof(lineBuf) - 1) {
      lineBuf[lineLen++] = c;
    }
  }
}

// -----------------------------------------------------------------------------
void setup() {
  Serial.setTxBufferSize(TX_BUFFER_BYTES);   // must come before begin()
  Serial.begin(BAUD);
  pinMode(PIN_LED_BLUE, OUTPUT);
  neopixelWrite(PIN_LED_RGB, 0, 0, 0);

  prefs.begin("t05", false);                 // same saved settings as T05
  mmPerCount = prefs.getDouble("mmpc", NOMINAL_MM_PER_COUNT);
  offsetMm   = prefs.getDouble("off", 0.0);
  dirSign    = prefs.getChar("dir", 1) < 0 ? -1 : 1;

  ESP32Encoder::useInternalWeakPullResistors = puType::none;
  encoder.attachFullQuad(PIN_ENC_A, PIN_ENC_B);
  encoder.setFilter(PCNT_FILTER);
  encoder.clearCount();

  delay(200);
  Serial.println();   // ends any half line left by the boot messages
  sendHello();
}

void loop() {
  uint32_t now = millis();

  if (now - lastPosMs >= POS_INTERVAL_MS) {
    int64_t count = signedCount();
    double dt = (now - lastSpeedMs) / 1000.0;
    double speed = dt > 0 ? travelMm(count - lastSpeedCount) / dt : 0.0;
    lastSpeedCount = count;
    lastSpeedMs = now;
    lastPosMs = now;
    if (count != lastSentCount) {                       // moved -> position message
      lastSentCount = count;
      sendPosition("pos", count, speed);
    } else if (now - lastSentMs >= HEARTBEAT_MS) {      // quiet -> heartbeat
      sendPosition("hb", count, 0.0);
    }
  }

  if (now - lastBlinkMs >= BLINK_INTERVAL_MS) {
    lastBlinkMs = now;
    blueLedOn = !blueLedOn;
    digitalWrite(PIN_LED_BLUE, blueLedOn ? HIGH : LOW);
  }

  readSerialLine();
}
