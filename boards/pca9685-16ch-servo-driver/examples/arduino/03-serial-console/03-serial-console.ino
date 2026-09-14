/*
 * PCA9685 16-Channel 12-Bit PWM Servo Driver - serial console.
 *
 * Drive channels by hand from the Serial Monitor, and decode what is actually
 * on the bus. The 'pca' command is the fast way to check you soldered the
 * address jumpers you meant to: it walks 0x40-0x7F and prints the A5..A0
 * pattern for every address that answers.
 *
 * Commands (115200, newline ending):
 *   <channel> <angle>   e.g. "0 90"  or  "5,120"
 *   all <angle>         e.g. "all 0"
 *   scan                every device on the bus, 0x08-0x77
 *   pca                 PCA9685 address table with jumper decoding
 *   addr <address>      e.g. "addr 0x42" - talk to a different board
 *   status              current angles and address
 *   help
 *
 * Wiring, ESP32-S3:
 *   GND -> GND        SDA -> GPIO 8
 *   VCC -> 3V3        SCL -> GPIO 9
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

static const uint8_t ADDR_MIN = 0x40;
static const uint8_t ADDR_MAX = 0x7F;
static const uint8_t MODE1_REG = 0x00;

static const uint8_t CHANNELS = 16;
static const uint16_t SERVO_FREQ_HZ = 50;
static const uint16_t PULSE_MIN_US = 500;
static const uint16_t PULSE_MAX_US = 2500;
static const uint32_t PCA9685_OSC_HZ = 27000000UL;   // see docs/registers.md
static const uint16_t DEFAULT_ANGLE = 0;

Adafruit_PWMServoDriver* drv = nullptr;
uint8_t boardAddr = 0;
bool ready = false;
uint16_t angles[CHANNELS];

bool devicePresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

bool readReg(uint8_t addr, uint8_t reg, uint8_t* value) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return false;
  if (Wire.requestFrom(addr, (uint8_t)1) != 1) return false;
  *value = Wire.read();
  return true;
}

/* Low six bits of the 7-bit address are the A5..A0 jumpers. */
void printJumpers(uint8_t addr) {
  uint8_t p = addr & 0x3F;
  Serial.printf("A5A4A3A2A1A0=%d%d%d%d%d%d",
                (p >> 5) & 1, (p >> 4) & 1, (p >> 3) & 1,
                (p >> 2) & 1, (p >> 1) & 1, p & 1);
}

void printHelp() {
  Serial.println("--- commands ---");
  Serial.println("  <channel> <angle>   0 90   or  5,120");
  Serial.println("  all <angle>         all 0");
  Serial.println("  scan                I2C bus 0x08-0x77");
  Serial.println("  pca                 PCA9685 table 0x40-0x7F with jumpers");
  Serial.println("  addr <address>      addr 0x42");
  Serial.println("  status              angles and current address");
  Serial.println("  help");
  Serial.println("--- addressing ---");
  Serial.println("  7-bit = 0x40 | (A5..A0). Jumper bridged = 1, open = 0.");
  Serial.println("  Avoid 0x70: it is the All Call address and is on by default.");
}

void scanBus() {
  Serial.println("I2C bus scan (0x08-0x77)...");
  uint8_t count = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    if (devicePresent(addr)) {
      bool inRange = (addr >= ADDR_MIN && addr <= ADDR_MAX);
      Serial.printf("  0x%02X%s\n", addr, inRange ? "  (PCA9685 range)" : "");
      count++;
    }
    delay(1);
  }
  if (count == 0) Serial.println("  nothing found");
  else Serial.printf("  %d device(s)\n", count);
}

uint8_t scanPca9685() {
  Serial.println("PCA9685 address table (jumper bridged = 1, open = 0)");
  Serial.println("  addr   A5A4A3A2A1A0   state");
  uint8_t count = 0;

  for (uint8_t addr = ADDR_MIN; addr <= ADDR_MAX; addr++) {
    bool online = devicePresent(addr);
    uint8_t mode1 = 0;
    bool readable = online && readReg(addr, MODE1_REG, &mode1);

    Serial.printf("  0x%02X   ", addr);
    printJumpers(addr);
    if (addr == ADDR_MIN) Serial.print(" (default)");
    if (addr == 0x70) Serial.print(" (All Call - avoid)");

    if (online) {
      Serial.printf("   [online MODE1=0x%02X]\n", readable ? mode1 : 0xFF);
      count++;
    } else {
      Serial.println("   [-]");
    }
    delay(1);
  }

  Serial.printf("%d online. Re-run 'pca' after changing jumpers.\n", count);
  return count;
}

uint8_t firstOnline() {
  for (uint8_t addr = ADDR_MIN; addr <= ADDR_MAX; addr++) {
    if (devicePresent(addr)) return addr;
  }
  return 0;
}

bool connect(uint8_t addr) {
  if (addr < ADDR_MIN || addr > ADDR_MAX) {
    Serial.println("[err] address must be 0x40-0x7F");
    return false;
  }
  if (!devicePresent(addr)) {
    Serial.printf("[err] no answer at 0x%02X - check jumpers and power\n", addr);
    return false;
  }

  delete drv;
  drv = new Adafruit_PWMServoDriver(addr);
  if (!drv->begin()) {
    delete drv;
    drv = nullptr;
    ready = false;
    Serial.printf("[err] begin() failed at 0x%02X\n", addr);
    return false;
  }

  drv->setOscillatorFrequency(PCA9685_OSC_HZ);
  drv->setPWMFreq(SERVO_FREQ_HZ);
  boardAddr = addr;
  ready = true;

  for (uint8_t ch = 0; ch < CHANNELS; ch++) {
    drv->writeMicroseconds(ch, map(angles[ch], 0, 180, PULSE_MIN_US, PULSE_MAX_US));
  }

  Serial.printf("[ok] connected 0x%02X (", addr);
  printJumpers(addr);
  Serial.println(")");
  return true;
}

void setAngle(uint8_t channel, uint16_t angle) {
  if (channel >= CHANNELS) return;
  angle = constrain(angle, (uint16_t)0, (uint16_t)180);
  angles[channel] = angle;
  if (!ready) return;
  drv->writeMicroseconds(channel, map(angle, 0, 180, PULSE_MIN_US, PULSE_MAX_US));
}

void setAll(uint16_t angle) {
  for (uint8_t ch = 0; ch < CHANNELS; ch++) setAngle(ch, angle);
}

void printStatus() {
  Serial.printf("--- address 0x%02X ", boardAddr);
  if (ready) {
    printJumpers(boardAddr);
    Serial.println();
  } else {
    Serial.println("(not connected)");
  }
  for (uint8_t ch = 0; ch < CHANNELS; ch++) {
    Serial.printf("  CH%02d: %3d deg\n", ch, angles[ch]);
  }
}

void handleLine(const String& line) {
  if (line.length() == 0) return;

  if (line == "?" || line.equalsIgnoreCase("help")) { printHelp(); return; }
  if (line.equalsIgnoreCase("status"))              { printStatus(); return; }
  if (line.equalsIgnoreCase("scan"))                { scanBus(); return; }
  if (line.equalsIgnoreCase("pca"))                 { scanPca9685(); return; }

  if (line.startsWith("addr ") || line.startsWith("ADDR ")) {
    String arg = line.substring(5);
    arg.trim();
    uint8_t addr = arg.startsWith("0x") || arg.startsWith("0X")
                       ? (uint8_t)strtol(arg.c_str(), nullptr, 16)
                       : (uint8_t)arg.toInt();
    connect(addr);
    return;
  }

  if (!ready) {
    Serial.println("[err] not connected - run 'pca', then 'addr <address>'");
    return;
  }

  if (line.startsWith("all ") || line.startsWith("ALL ")) {
    setAll((uint16_t)line.substring(4).toInt());
    Serial.printf("[ok] CH0-CH15 -> %d deg\n", angles[0]);
    return;
  }

  int sep = line.indexOf(',');
  if (sep < 0) sep = line.indexOf(' ');
  if (sep < 0) sep = line.indexOf(':');

  if (sep > 0) {
    int ch = line.substring(0, sep).toInt();
    int angle = line.substring(sep + 1).toInt();
    if (ch < 0 || ch >= CHANNELS) { Serial.println("[err] channel 0-15"); return; }
    if (angle < 0 || angle > 180) { Serial.println("[err] angle 0-180"); return; }
    setAngle((uint8_t)ch, (uint16_t)angle);
    Serial.printf("[ok] CH%02d -> %d deg\n", ch, angles[ch]);
    return;
  }

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
    } else if (buf.length() < 64) {
      buf += c;
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);

  for (uint8_t ch = 0; ch < CHANNELS; ch++) angles[ch] = DEFAULT_ANGLE;

  Serial.println("\n===== PCA9685 serial console =====");
  Serial.printf("I2C SDA=GPIO%d SCL=GPIO%d 400 kHz\n", SDA_PIN, SCL_PIN);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(400000);

  uint8_t addr = firstOnline();
  if (addr == 0) {
    Serial.println("[err] no PCA9685 found. Check:");
    Serial.printf("  1. SDA/SCL on GPIO%d/GPIO%d\n", SDA_PIN, SCL_PIN);
    Serial.println("  2. VCC and a shared ground");
    Serial.println("  3. address jumpers - run 'pca' once connected");
    Serial.println("  rescanning in 5 s...");
    delay(5000);
    addr = firstOnline();
  }

  if (addr != 0) {
    connect(addr);
    setAll(DEFAULT_ANGLE);
    Serial.printf("[ok] %d Hz, %d-%d us, all channels at %d deg\n",
                  SERVO_FREQ_HZ, PULSE_MIN_US, PULSE_MAX_US, DEFAULT_ANGLE);
  } else {
    Serial.println("[warn] still nothing - console is live, try 'pca'");
  }

  printHelp();
  Serial.println();
}

void loop() {
  processSerial();
}
