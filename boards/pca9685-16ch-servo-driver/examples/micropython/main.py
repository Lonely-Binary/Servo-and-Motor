"""Sweep every channel of a PCA9685 servo driver.

Wiring, ESP32-S3:
    GND -> GND        SDA -> GPIO 8
    VCC -> 3V3        SCL -> GPIO 9
    OE  -> leave unconnected
    V+  -> from the USB-C socket or the screw terminal, not from the ESP32

Copy pca9685.py to the board alongside this file.
"""

import time
from machine import I2C, Pin

from pca9685 import PCA9685, scan

SDA_PIN = 8
SCL_PIN = 9

CHANNELS = 16
STEP_DEG = 3
STEP_MS = 15

i2c = I2C(0, sda=Pin(SDA_PIN), scl=Pin(SCL_PIN), freq=100_000)

boards = scan(i2c)
if not boards:
    raise SystemExit(
        "No PCA9685 found. Check VCC, a shared ground, and SDA/SCL."
    )

for address, pattern in boards:
    print("0x{:02X}  A5..A0={}".format(address, pattern))

pwm = PCA9685(i2c, address=boards[0][0])
print("using 0x{:02X} at {:.2f} Hz".format(boards[0][0], pwm.freq(50)))

try:
    while True:
        for degrees in range(0, 181, STEP_DEG):
            for channel in range(CHANNELS):
                pwm.angle(channel, degrees)
            time.sleep_ms(STEP_MS)
        time.sleep_ms(300)

        for degrees in range(180, -1, -STEP_DEG):
            for channel in range(CHANNELS):
                pwm.angle(channel, degrees)
            time.sleep_ms(STEP_MS)
        time.sleep_ms(800)
except KeyboardInterrupt:
    pwm.all_off()
    print("stopped")
