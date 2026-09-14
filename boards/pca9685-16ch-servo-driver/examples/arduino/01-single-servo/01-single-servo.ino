/*
 * PCA9685 16-Channel 12-Bit PWM Servo Driver - one servo, two positions.
 *
 * The smallest sketch that proves the board works. Plug one servo into
 * channel 0 and it swings back and forth once a second.
 *
 * Wiring, ESP32-S3:
 *   GND -> GND        SDA -> GPIO 8
 *   VCC -> 3V3        SCL -> GPIO 9
 *   OE  -> leave unconnected (pulled low on the board)
 *   V+  -> leave unconnected; power it from the USB-C socket or screw terminal
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

// A board with no address jumpers bridged answers here.
static const uint8_t PCA9685_ADDR = 0x40;

// The data sheet calls the internal oscillator 25 MHz typical; real parts land
// a few percent away. 27 MHz is what Adafruit's own example uses and what these
// boards measure closest to. See docs/registers.md for how to trim it yourself.
static const uint32_t PCA9685_OSC_HZ = 27000000UL;

static const uint16_t SERVO_FREQ_HZ = 50;

// Start conservative. Widen only after you have found your servo's real range.
static const uint16_t PULSE_MIN_US = 1000;
static const uint16_t PULSE_MAX_US = 2000;

Adafruit_PWMServoDriver pwm(PCA9685_ADDR);

void setup() {
  Serial.begin(115200);
  delay(300);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  if (!pwm.begin()) {
    Serial.printf("No PCA9685 at 0x%02X. Check VCC, ground and SDA/SCL.\n",
                  PCA9685_ADDR);
    while (true) delay(1000);
  }

  pwm.setOscillatorFrequency(PCA9685_OSC_HZ);
  pwm.setPWMFreq(SERVO_FREQ_HZ);

  Serial.printf("PCA9685 0x%02X ready, %d Hz\n", PCA9685_ADDR, SERVO_FREQ_HZ);
}

void loop() {
  pwm.writeMicroseconds(0, PULSE_MIN_US);
  delay(700);
  pwm.writeMicroseconds(0, PULSE_MAX_US);
  delay(700);
}
