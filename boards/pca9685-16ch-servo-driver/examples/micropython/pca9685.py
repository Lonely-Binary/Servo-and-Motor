"""Minimal PCA9685 driver for MicroPython.

No dependencies beyond machine.I2C. Enough to drive servos and to see what the
registers are actually doing; see docs/registers.md for the arithmetic.
"""

import struct
import time

_MODE1 = 0x00
_LED0_ON_L = 0x06
_ALL_LED_ON_L = 0xFA
_PRESCALE = 0xFE

_MODE1_RESTART = 0x80
_MODE1_AI = 0x20
_MODE1_SLEEP = 0x10


class PCA9685:
    """One PCA9685 on an I2C bus.

    osc_hz is the chip's internal oscillator. The data sheet calls it 25 MHz
    typical; real parts land a few percent away, which is a few degrees of
    servo error. Measure yours once and pass the real number in.
    """

    def __init__(self, i2c, address=0x40, osc_hz=27_000_000):
        self.i2c = i2c
        self.address = address
        self.osc_hz = osc_hz
        self._counts_per_us = 0.0
        self.reset()

    # --- registers ---------------------------------------------------------

    def _write(self, reg, value):
        self.i2c.writeto_mem(self.address, reg, bytes([value]))

    def _read(self, reg):
        return self.i2c.readfrom_mem(self.address, reg, 1)[0]

    def reset(self):
        """Wake the chip and turn on register auto-increment.

        This also clears the ALLCALL bit, so the board stops answering to
        0x70 as well as to its own address.
        """
        self._write(_MODE1, _MODE1_AI)
        time.sleep_us(500)          # oscillator start-up, data sheet 7.3.1
        self._cache_prescale()

    def _cache_prescale(self):
        prescale_plus_1 = self._read(_PRESCALE) + 1
        self._counts_per_us = self.osc_hz / (1_000_000 * prescale_plus_1)

    # --- frequency ---------------------------------------------------------

    def freq(self, hz=None):
        """Read the output frequency, or set it. All sixteen channels share it."""
        if hz is None:
            return self.osc_hz / (4096 * (self._read(_PRESCALE) + 1))

        prescale = int(self.osc_hz / (4096 * hz) + 0.5) - 1
        prescale = min(255, max(3, prescale))       # hardware limits

        mode1 = self._read(_MODE1)
        self._write(_MODE1, (mode1 & ~_MODE1_RESTART) | _MODE1_SLEEP)
        self._write(_PRESCALE, prescale)            # writable only while asleep
        self._write(_MODE1, mode1 & ~_MODE1_SLEEP)
        time.sleep_us(500)
        self._write(_MODE1, mode1 | _MODE1_RESTART | _MODE1_AI)
        self._cache_prescale()
        return self.freq()

    # --- output ------------------------------------------------------------

    def pwm(self, channel, on, off):
        """Raw 12-bit on and off counts, 0 to 4095."""
        self.i2c.writeto_mem(
            self.address,
            _LED0_ON_L + 4 * channel,
            struct.pack("<HH", on & 0x0FFF, off & 0x0FFF),
        )

    def duty(self, channel, fraction):
        """Duty cycle as 0.0 to 1.0."""
        self.pwm(channel, 0, int(min(1.0, max(0.0, fraction)) * 4095))

    def us(self, channel, microseconds):
        """Pulse width in microseconds. This is the one servos care about."""
        count = int(microseconds * self._counts_per_us)
        self.pwm(channel, 0, min(4095, max(0, count)))

    def angle(self, channel, degrees, min_us=1000, max_us=2000):
        """Degrees, mapped onto a pulse width.

        The 1000-2000 us default is deliberately conservative. Widen it only
        after you have found your servo's real range by hand.
        """
        degrees = min(180, max(0, degrees))
        self.us(channel, min_us + (max_us - min_us) * degrees / 180)

    def all_off(self):
        """Stop driving every channel."""
        self.i2c.writeto_mem(
            self.address, _ALL_LED_ON_L, struct.pack("<HH", 0, 0x1000)
        )


def scan(i2c):
    """Every address in the PCA9685 range that answers, with its jumper pattern."""
    found = []
    for address in i2c.scan():
        if 0x40 <= address <= 0x7F:
            bits = address & 0x3F
            pattern = "".join(str((bits >> b) & 1) for b in range(5, -1, -1))
            found.append((address, pattern))
    return found
