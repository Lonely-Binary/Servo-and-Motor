# Registers and timing

You do not need any of this to move a servo — the Adafruit library covers it.
You need it when the angle is off by a few degrees and you want to know why, or
when you are writing your own driver.

Everything here is from the
[PCA9685 data sheet, rev. 4](https://www.nxp.com/docs/en/data-sheet/PCA9685.pdf).

## Register map

| Address | Name | Function |
| --- | --- | --- |
| `0x00` | MODE1 | Sleep, auto-increment, address response |
| `0x01` | MODE2 | Output structure, invert, update timing |
| `0x02`–`0x04` | SUBADR1–3 | Sub-addresses, disabled by default |
| `0x05` | ALLCALLADR | All Call address, `0xE0` (7-bit `0x70`), enabled by default |
| `0x06`–`0x09` | LED0_ON_L … LED0_OFF_H | Channel 0 |
| … | | Four bytes per channel, sixteen channels |
| `0x42`–`0x45` | LED15_ON_L … LED15_OFF_H | Channel 15 |
| `0xFA`–`0xFD` | ALL_LED_ON_L … ALL_LED_OFF_H | Write all sixteen at once |
| `0xFE` | PRE_SCALE | PWM frequency, default `0x1E` |

### MODE1, `0x00`, defaults to `0x11`

| Bit | Name | Default | Meaning |
| --- | --- | --- | --- |
| 7 | RESTART | 0 | Write 1 to restart PWM after waking from sleep |
| 6 | EXTCLK | 0 | Use the EXTCLK pin. **Sticky — only a power cycle or software reset clears it.** On this board EXTCLK is grounded, so setting this bit stops the PWM |
| 5 | AI | 0 | Auto-increment the register pointer. Set it before block writes |
| 4 | SLEEP | **1** | Oscillator off. The chip powers up asleep |
| 3–1 | SUB1–3 | 0 | Respond to sub-addresses |
| 0 | ALLCALL | **1** | Respond to the All Call address. This is why `0x70` is unusable |

The chip wakes up asleep. Clear SLEEP, then wait 500 µs for the oscillator
before writing PWM values — the data sheet does not guarantee output timing
inside that window.

### MODE2, `0x01`, defaults to `0x04`

| Bit | Name | Default | Meaning |
| --- | --- | --- | --- |
| 4 | INVRT | 0 | Invert the output logic |
| 3 | OCH | 0 | 0: outputs change on STOP. 1: on ACK |
| 2 | OUTDRV | **1** | 1: totem pole. 0: open drain |
| 1–0 | OUTNE | 00 | What the outputs do while `OE` is high. `00` = driven low |

OCH is worth knowing about. Left at 0, every channel you wrote before the STOP
condition changes at the same instant — which is how you make sixteen servos
start a move together instead of in sequence.

## Frequency

One prescaler serves all sixteen channels, so they all run at the same
frequency. Only the duty cycle is per channel.

```
prescale = round( osc_clock / (4096 × frequency) ) - 1
```

| | |
| --- | --- |
| Oscillator | 25 MHz typical, internal |
| Prescale range | `0x03` to `0xFF`, forced by hardware |
| Frequency range | 1526 Hz down to 24 Hz |
| Default | `0x1E` = 30, giving 200 Hz |

PRE_SCALE can only be written while SLEEP is 1. The sequence is: set SLEEP,
write PRE_SCALE, clear SLEEP, wait 500 µs, write RESTART.

For servos at 50 Hz:

```
prescale = round( 25 000 000 / (4096 × 50) ) - 1 = 122 - 1 = 121
```

which actually gives 25 000 000 / (4096 × 122) = **50.03 Hz**, a period of
19.99 ms and a tick of 4.88 µs.

## From microseconds to counts

Each channel has a 12-bit on-count and a 12-bit off-count against a counter that
runs 0 to 4095 every period. For a servo, leave the on-count at 0 and put the
pulse width in the off-count:

```
count = pulse_us × osc_clock / (1 000 000 × (prescale + 1))
```

At 50 Hz with a 25 MHz oscillator, one count is 4.88 µs:

| Pulse | Count | Typical meaning |
| --- | --- | --- |
| 500 µs | 102 | Hard end stop on a wide-range servo |
| 1000 µs | 205 | 0° on most hobby servos |
| 1500 µs | 307 | Centre |
| 2000 µs | 410 | 180° on most hobby servos |
| 2500 µs | 512 | The other hard end stop |

`Adafruit_PWMServoDriver::writeMicroseconds()` does this arithmetic for you and
is the call to reach for. `setPWM(channel, 0, count)` writes counts directly.

A warning about the old `SERVOMIN 150` / `SERVOMAX 600` pair that appears in
every PCA9685 tutorial: at 50 Hz those are 732 µs and 2928 µs. Plenty of servos
will buzz, or grind against an internal stop, at 2928 µs. Work in microseconds
and start at 1000–2000; widen only after you have found your servo's real range
by hand.

## Trimming the oscillator

The data sheet calls the internal oscillator 25 MHz *typical*. Real parts come
out a few percent away, and a few percent of 1500 µs is tens of microseconds —
enough to see as a couple of degrees of error, and enough for a nominal 50 Hz to
land nearer 52 Hz.

The fix is to measure it once per board and tell the library:

1. `setOscillatorFrequency(25000000)` and `setPWMFreq(50)`.
2. Put a scope or logic analyser on any channel driven to mid-scale and measure
   the actual period.
3. `osc_actual = measured_frequency × 4096 × (prescale + 1)`, where prescale is
   whatever `readPrescale()` returns.
4. Pass that number to `setOscillatorFrequency()` from then on.

The examples in this repository declare the value as a named constant near the
top of the file for exactly this reason.

## Writing all sixteen channels at once

Set the AI bit in MODE1, then write one I²C transaction: the register pointer
`0x06`, followed by 64 bytes. The pointer walks itself and every channel lands
in one bus transaction, which at 100 kHz is about 6 ms instead of sixteen
separate transactions with their own addressing overhead.

[`examples/arduino/04-multi-board-hotplug`](../examples/arduino/04-multi-board-hotplug)
does this, and it is why it can hold thirty-two servos in a smooth sweep.

## Software reset

Writing `0x06` to the general call address `0x00` resets every PCA9685 on the
bus to its power-up state. It is the only way to clear a stuck EXTCLK bit
without cycling power.
