# Examples

All four Arduino sketches compile clean for `esp32:esp32:esp32s3` with the ESP32
core 3.3.x and Adafruit PWM Servo Driver Library 3.0.x. They assume `SDA` on
GPIO 8 and `SCL` on GPIO 9 — change the two constants at the top of each file
for a different board.

| | |
| --- | --- |
| [arduino/01-single-servo](arduino/01-single-servo) | One servo on channel 0, two positions. Start here. |
| [arduino/02-sweep-all](arduino/02-sweep-all) | Scans the bus, then sweeps all sixteen channels together. |
| [arduino/03-serial-console](arduino/03-serial-console) | `5 120` sends channel 5 to 120°. Also `scan`, `pca` and `addr` for working out what is on the bus. |
| [arduino/04-multi-board-hotplug](arduino/04-multi-board-hotplug) | Two boards, 32 channels, detected as they are plugged in. Writes each board in one 64-byte transaction. |
| [platformio](platformio) | A `platformio.ini` that builds any of the above. |
| [micropython](micropython) | A small driver with no library dependency, and the same sweep. |

## Arduino IDE

Install **Adafruit PWM Servo Driver Library** from the Library Manager; it will
pull in Adafruit BusIO. Then:

| Setting | Value |
| --- | --- |
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| Serial Monitor | 115200 |

Arduino board settings live in the IDE and not in the file, so each sketch
repeats them in its opening comment.

## PlatformIO

[`platformio/platformio.ini`](platformio/platformio.ini) points `src_dir` at one
of the Arduino sketches, so there is only ever one copy of each example. Edit
that line to build a different one.

```bash
cd examples/platformio
pio run -t upload && pio device monitor
```

## MicroPython

Copy both files to the board:

```bash
mpremote cp micropython/pca9685.py :
mpremote cp micropython/main.py :
mpremote run micropython/main.py
```

[`pca9685.py`](micropython/pca9685.py) is about a hundred lines and talks to the
registers directly, so it doubles as a readable account of what the chip
actually wants. [`registers.md`](../docs/registers.md) explains the arithmetic.

## A note on the oscillator constant

Every example declares:

```cpp
static const uint32_t PCA9685_OSC_HZ = 27000000UL;
```

The data sheet calls the internal oscillator 25 MHz *typical* and publishes no
tolerance. Real parts land a few percent away, which is a few degrees of servo
error and a nominal 50 Hz that measures closer to 52. 27 MHz is the value
Adafruit's own example uses and what these boards measure nearest to.

If you want it exact, measure one channel's period with a scope and work
backwards — the procedure is in
[docs/registers.md](../docs/registers.md#trimming-the-oscillator).
