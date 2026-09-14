/*
 * PCA9685 16-Channel 12-Bit PWM Servo Driver - two boards, hot plug.
 *
 * Sweeps every channel of every board that is on the bus, and keeps sweeping
 * while boards are plugged and unplugged. A board appearing is picked up within
 * about a second and joins the sweep at the current angle; a board disappearing
 * is dropped without stalling the others.
 *
 * Up to two boards, 32 channels, addresses anywhere in 0x40-0x7F.
 *
 * Each board is written in a single I2C transaction: register pointer 0x06
 * followed by 64 bytes, with the PCA9685's auto-increment doing the walking.
 * That is what keeps 32 servos in a smooth sweep instead of a ripple.
 *
 * Wiring, ESP32-S3:
 *   GND -> GND        SDA -> GPIO 8
 *   VCC -> 3V3        SCL -> GPIO 9
 *   Each board needs its own address. See docs/i2c-addressing.md.
 *   Power the servos from a dedicated adapter, not from a computer.
 *
 * Serial commands: status | scan | help
 *
 * Arduino IDE:
 *   Board          ESP32S3 Dev Module
 *   USB CDC On Boot Enabled
 *   Serial Monitor 115200
 *   Library        Adafruit PWM Servo Driver Library
 */

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

static const int SDA_PIN = 8;
static const int SCL_PIN = 9;
static const uint32_t I2C_CLOCK_HZ = 100000;

static const uint8_t ADDR_MIN = 0x40;
static const uint8_t ADDR_MAX = 0x7F;
static const uint8_t MAX_BOARDS = 2;
static const uint8_t CH_PER_BOARD = 16;
static const uint8_t TOTAL_CH = MAX_BOARDS * CH_PER_BOARD;
static const uint8_t MODE1_REG = 0x00;
static const uint32_t PCA9685_OSC_HZ = 27000000UL;   // see docs/registers.md

static const uint16_t SERVO_FREQ_HZ = 50;
static const uint16_t PULSE_MIN_US = 500;
static const uint16_t PULSE_MAX_US = 2500;

static const uint16_t SWEEP_STEP_DEG = 10;
static const uint32_t SWEEP_INTERVAL_MS = 15;
static const uint32_t RESCAN_INTERVAL_MS = 1000;

struct Board {
  bool online;
  uint8_t addr;
  Adafruit_PWMServoDriver* drv;
  uint16_t prescalePlus1;   // cached so we do not read a register every frame
};

Board boards[MAX_BOARDS];
uint16_t angles[TOTAL_CH];

int16_t sweepAngle = 0;
int8_t sweepDir = 1;
uint32_t lastSweepMs = 0;
uint32_t lastRescanMs = 0;

void detach(uint8_t index);
bool attach(uint8_t index, uint8_t addr);
bool flush(uint8_t index);

bool devicePresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool readReg(uint8_t addr, uint8_t reg, uint8_t* value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom((int)addr, 1) != 1) return false;
  *value = Wire.read();
  return true;
}

/* A device in the PCA9685 range whose MODE1 register reads back. */
bool looksLikePca9685(uint8_t addr) {
  uint8_t mode1 = 0;
  return readReg(addr, MODE1_REG, &mode1);
}

void printJumpers(uint8_t addr) {
  uint8_t p = addr & 0x3F;
  Serial.printf("A5..A0=%d%d%d%d%d%d",
                (p >> 5) & 1, (p >> 4) & 1, (p >> 3) & 1,
                (p >> 2) & 1, (p >> 1) & 1, p & 1);
}

uint8_t onlineCount() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < MAX_BOARDS; i++) if (boards[i].online) n++;
  return n;
}

/*
 * count = us * osc / (1e6 * (prescale + 1)), because one counter tick is
 * (prescale + 1) / osc seconds. See docs/registers.md.
 */
uint16_t angleToCount(const Board& board, uint16_t angle) {
  angle = constrain(angle, (uint16_t)0, (uint16_t)180);
  uint16_t us = map(angle, 0, 180, PULSE_MIN_US, PULSE_MAX_US);
  double count = (double)us * (double)PCA9685_OSC_HZ /
                 (1000000.0 * (double)board.prescalePlus1);
  if (count > 4095.0) count = 4095.0;
  if (count < 0.0) count = 0.0;
  return (uint16_t)count;
}

/* All sixteen channels of one board in a single transaction. */
bool flush(uint8_t index) {
  if (index >= MAX_BOARDS || !boards[index].online || !boards[index].drv) {
    return false;
  }

  Board& board = boards[index];
  uint8_t base = index * CH_PER_BOARD;
  uint8_t buf[1 + CH_PER_BOARD * 4];
  buf[0] = PCA9685_LED0_ON_L;

  for (uint8_t i = 0; i < CH_PER_BOARD; i++) {
    uint16_t count = angleToCount(board, angles[base + i]);
    buf[1 + i * 4 + 0] = 0;                      // ON_L
    buf[1 + i * 4 + 1] = 0;                      // ON_H
    buf[1 + i * 4 + 2] = count & 0xFF;           // OFF_L
    buf[1 + i * 4 + 3] = (count >> 8) & 0x0F;    // OFF_H
  }

  Wire.beginTransmission(board.addr);
  Wire.write(buf, sizeof(buf));
  if (Wire.endTransmission(true) != 0) {
    Serial.printf("[-] board %d at 0x%02X stopped answering\n", index, board.addr);
    detach(index);
    return false;
  }
  return true;
}

void detach(uint8_t index) {
  if (index >= MAX_BOARDS) return;
  if (boards[index].drv) {
    delete boards[index].drv;
    boards[index].drv = nullptr;
  }
  uint8_t base = index * CH_PER_BOARD;
  for (uint8_t i = 0; i < CH_PER_BOARD; i++) angles[base + i] = 0;
  boards[index].online = false;
  boards[index].addr = 0;
  boards[index].prescalePlus1 = 0;
}

bool attach(uint8_t index, uint8_t addr) {
  if (index >= MAX_BOARDS) return false;
  detach(index);

  Adafruit_PWMServoDriver* drv = new Adafruit_PWMServoDriver(addr);
  if (!drv->begin()) {
    delete drv;
    Serial.printf("[err] begin() failed at 0x%02X\n", addr);
    return false;
  }

  drv->setOscillatorFrequency(PCA9685_OSC_HZ);
  drv->setPWMFreq(SERVO_FREQ_HZ);
  delay(5);

  boards[index].drv = drv;
  boards[index].addr = addr;
  boards[index].online = true;
  boards[index].prescalePlus1 = (uint16_t)drv->readPrescale() + 1;

  uint8_t base = index * CH_PER_BOARD;
  for (uint8_t i = 0; i < CH_PER_BOARD; i++) angles[base + i] = (uint16_t)sweepAngle;

  Serial.printf("[+] board %d at 0x%02X -> CH%d-CH%d (",
                index, addr, base, base + CH_PER_BOARD - 1);
  printJumpers(addr);
  Serial.println(")");
  flush(index);
  return true;
}

int slotOfAddr(uint8_t addr) {
  for (uint8_t i = 0; i < MAX_BOARDS; i++) {
    if (boards[i].online && boards[i].addr == addr) return (int)i;
  }
  return -1;
}

int freeSlot() {
  for (uint8_t i = 0; i < MAX_BOARDS; i++) if (!boards[i].online) return (int)i;
  return -1;
}

void rescan() {
  uint8_t found[MAX_BOARDS];
  uint8_t foundN = 0;

  for (uint8_t addr = ADDR_MIN; addr <= ADDR_MAX; addr++) {
    if (!looksLikePca9685(addr)) continue;
    if (foundN < MAX_BOARDS) found[foundN++] = addr;
  }

  // Gone: online but not in the scan.
  for (uint8_t i = 0; i < MAX_BOARDS; i++) {
    if (!boards[i].online) continue;
    bool stillThere = false;
    for (uint8_t j = 0; j < foundN; j++) {
      if (found[j] == boards[i].addr) { stillThere = true; break; }
    }
    if (!stillThere) {
      Serial.printf("[-] board %d at 0x%02X gone\n", i, boards[i].addr);
      detach(i);
    }
  }

  // New: in the scan but not yet attached.
  for (uint8_t j = 0; j < foundN; j++) {
    if (slotOfAddr(found[j]) >= 0) continue;
    int slot = freeSlot();
    if (slot < 0) {
      Serial.printf("[warn] already holding %d boards, ignoring 0x%02X\n",
                    MAX_BOARDS, found[j]);
      continue;
    }
    attach((uint8_t)slot, found[j]);
  }
}

void setAllOnline(uint16_t angle) {
  angle = constrain(angle, (uint16_t)0, (uint16_t)180);
  for (uint8_t b = 0; b < MAX_BOARDS; b++) {
    if (!boards[b].online) continue;
    uint8_t base = b * CH_PER_BOARD;
    for (uint8_t i = 0; i < CH_PER_BOARD; i++) angles[base + i] = angle;
    flush(b);
  }
}

void printStatus() {
  Serial.println("--- status ---");
  Serial.printf("sweep %d deg, direction %s, %d board(s) online\n",
                sweepAngle, sweepDir > 0 ? "up" : "down", onlineCount());
  for (uint8_t i = 0; i < MAX_BOARDS; i++) {
    if (!boards[i].online) { Serial.printf("  %d  (empty)\n", i); continue; }
    Serial.printf("  %d  0x%02X ", i, boards[i].addr);
    printJumpers(boards[i].addr);
    Serial.printf(" -> CH%d-CH%d\n",
                  i * CH_PER_BOARD, i * CH_PER_BOARD + CH_PER_BOARD - 1);
  }
}

void printHelp() {
  Serial.println("--- multi-board sweep ---");
  Serial.println("  every channel of every board sweeps together");
  Serial.println("  boards are detected about once a second");
  Serial.println("commands: status | scan | help");
}

void scanBus() {
  Serial.println("I2C scan...");
  uint8_t n = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    if (devicePresent(addr)) {
      Serial.printf("  0x%02X%s\n", addr,
                    (addr >= ADDR_MIN && addr <= ADDR_MAX) ? "  (PCA9685 range)" : "");
      n++;
    }
  }
  Serial.printf("%d device(s)\n", n);
}

void handleLine(const String& line) {
  if (line.length() == 0) return;
  if (line.equalsIgnoreCase("help") || line == "?") { printHelp(); return; }
  if (line.equalsIgnoreCase("status"))              { printStatus(); return; }
  if (line.equalsIgnoreCase("scan"))                { scanBus(); return; }
  Serial.println("[err] unknown command - type help");
}

void processSerial() {
  static String buf;
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\r') continue;
    if (c == '\n') {
      buf.trim();
      handleLine(buf);
      buf = "";
    } else if (buf.length() < 48) {
      buf += c;
    }
  }
}

void sweepTick() {
  if (onlineCount() == 0) return;

  setAllOnline((uint16_t)sweepAngle);

  sweepAngle += (int16_t)SWEEP_STEP_DEG * sweepDir;
  if (sweepAngle >= 180) { sweepAngle = 180; sweepDir = -1; }
  else if (sweepAngle <= 0) { sweepAngle = 0; sweepDir = 1; }
}

void setup() {
  Serial.begin(115200);
  uint32_t t0 = millis();
  while (!Serial && (millis() - t0 < 3000)) delay(10);
  delay(100);

  for (uint8_t i = 0; i < MAX_BOARDS; i++) {
    boards[i].online = false;
    boards[i].addr = 0;
    boards[i].drv = nullptr;
    boards[i].prescalePlus1 = 0;
  }
  for (uint8_t i = 0; i < TOTAL_CH; i++) angles[i] = 0;

  Serial.println("\n===== PCA9685 multi-board sweep =====");
  Serial.printf("I2C SDA=GPIO%d SCL=GPIO%d %lu Hz\n",
                SDA_PIN, SCL_PIN, (unsigned long)I2C_CLOCK_HZ);
  Serial.printf("%d-%d us at %d Hz, %d deg every %lu ms\n",
                PULSE_MIN_US, PULSE_MAX_US, SERVO_FREQ_HZ,
                SWEEP_STEP_DEG, (unsigned long)SWEEP_INTERVAL_MS);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(I2C_CLOCK_HZ);

  rescan();
  if (onlineCount() == 0) {
    Serial.println("[info] no board yet, waiting...");
  } else {
    Serial.printf("[ok] %d board(s), sweeping\n", onlineCount());
  }
  printHelp();
  Serial.println();

  lastSweepMs = millis();
  lastRescanMs = millis();
}

void loop() {
  processSerial();

  uint32_t now = millis();

  if (now - lastRescanMs >= RESCAN_INTERVAL_MS) {
    lastRescanMs = now;
    rescan();
  }

  if (now - lastSweepMs >= SWEEP_INTERVAL_MS) {
    lastSweepMs = now;
    sweepTick();
  }
}
