/*
 * PCA9685 16-Channel 12-Bit PWM Servo Driver - sweep all sixteen channels.
 *
 * Scans the I2C bus on startup, takes the lowest address it finds, then walks
 * every channel from one end of travel to the other and back. This is the
 * factory test: plug in as many servos as you have and watch them move
 * together.
 *
 * Wiring, ESP32-S3:
 *   GND -> GND        SDA -> GPIO 8
 *   VCC -> 3V3        SCL -> GPIO 9
 *
 * Sixteen servos starting in the same millisecond is the worst case for the
 * supply. Use a dedicated 5 V adapter on V+, not a computer's USB port.
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

static const uint32_t PCA9685_OSC_HZ = 27000000UL;   // see docs/registers.md
static const uint16_t SERVO_FREQ_HZ = 50;
static const uint16_t PULSE_MIN_US = 1000;           // 0 degrees
static const uint16_t PULSE_MAX_US = 2000;           // 180 degrees

static const uint8_t CHANNELS = 16;
static const int STEP_DEG = 3;
static const uint32_t STEP_MS = 15;

Adafruit_PWMServoDriver pwm;

void setServoAngle(uint8_t channel, int angle) {
  angle = constrain(angle, 0, 180);
  pwm.writeMicroseconds(channel, map(angle, 0, 180, PULSE_MIN_US, PULSE_MAX_US));
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\nPCA9685 sweep test");

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);
  delay(100);

  Serial.println("Scanning I2C bus...");
  uint8_t lowest = 0;
  uint8_t found = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() != 0) continue;
    Serial.printf("  device at 0x%02X\n", addr);
    found++;
    if (lowest == 0) lowest = addr;
  }

  if (found == 0) {
    Serial.println("No I2C devices. Check VCC, ground and SDA/SCL.");
    while (true) delay(1000);
  }

  Serial.printf("Using 0x%02X\n", lowest);
  pwm = Adafruit_PWMServoDriver(lowest);
  if (!pwm.begin()) {
    Serial.println("begin() failed.");
    while (true) delay(1000);
  }

  pwm.setOscillatorFrequency(PCA9685_OSC_HZ);
  pwm.setPWMFreq(SERVO_FREQ_HZ);
  Serial.printf("Ready: %d channels at %d Hz, %d-%d us\n",
                CHANNELS, SERVO_FREQ_HZ, PULSE_MIN_US, PULSE_MAX_US);
}

void loop() {
  for (int angle = 0; angle <= 180; angle += STEP_DEG) {
    for (uint8_t ch = 0; ch < CHANNELS; ch++) setServoAngle(ch, angle);
    delay(STEP_MS);
  }
  delay(300);

  for (int angle = 180; angle >= 0; angle -= STEP_DEG) {
    for (uint8_t ch = 0; ch < CHANNELS; ch++) setServoAngle(ch, angle);
    delay(STEP_MS);
  }
  delay(800);
}
