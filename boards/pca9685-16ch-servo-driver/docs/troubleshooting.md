# Troubleshooting

## The board does not appear on an I²C scan

Check the two LEDs first. Red is `VCC`, blue is `V+`.

**Neither lit.** No power reaching the board. If you are feeding it from the
adapter, check the adapter and the cable — a charge-only USB-C cable is fine
here, but a dead one is not. If you are feeding `VCC` from the microcontroller,
check that wire.

**Blue lit, red dark.** `V+` is present but `VCC` is not, so the PCA9685 is
unpowered. Either wire `VCC` on the control header to your microcontroller's
3V3 pin, or bridge the `3V3` jumper on the underside to run `VCC` off `V+`
through the on-board regulator. See [Power](power.md).

**Both lit, still nothing on the scan.** In order of likelihood:

- `SDA` and `SCL` swapped.
- No shared ground between the microcontroller and the board.
- The scanner is on the wrong pins. On an ESP32 the I²C pins are whatever you
  passed to `Wire.begin()`, not a fixed pair.
- A solder bead bridging two address jumper pads — the board is answering, just
  not where you are looking. Scan the whole `0x08`–`0x77` range rather than
  probing `0x40` alone.

## It answers, but the servo does not move

**`OE` wired high.** `OE` is active low with a 10 kΩ pull-down, so leaving it
unconnected is correct. Wiring it to 3V3 or 5V disables all sixteen channels in
hardware and the bus keeps working perfectly the whole time.

**`V+` not connected.** The PWM pin gets its signal from `VCC`, but the servo
gets its power from `V+`. A board running on `VCC` alone will happily accept
every command and move nothing. Blue LED dark means `V+` is dead.

**Servo plugged in backwards.** Brown or black to the `GND` row, red to `V+`,
yellow or orange to `PWM`. The row labels are printed between the pin blocks.

**Wrong channel.** Channel numbers are printed above the `PWM` row, 0 at the
left.

## The servo buzzes, or grinds at the ends of travel

You are commanding a pulse outside its mechanical range. The `SERVOMIN 150` /
`SERVOMAX 600` pair from the common tutorials is 732 µs to 2928 µs at 50 Hz,
which is wider than most hobby servos accept.

Use `writeMicroseconds()` and start conservative at 1000–2000 µs. Widen 50 µs at
a time until you hear it hit a stop, then back off. See
[Registers](registers.md#from-microseconds-to-counts).

A servo that buzzes at rest and never settles, at any pulse width, is usually
under-powered rather than mis-commanded — see below.

## The microcontroller resets when a servo moves

The supply is collapsing. A stalled MG996R pulls around 2.5 A; four of them
briefly pull ten. The board's 1000 µF capacitor covers a millisecond, not a
second of stall.

- Use a dedicated adapter for `V+`, not the computer's USB port, and not the
  microcontroller's 5 V pin.
- If `VCC` comes from the microcontroller, the reset is the microcontroller's
  own supply sagging — which means `V+` current is somehow flowing through it.
  Check that you have not wired `V+` to the microcontroller's 5 V pin.
- Stagger the starts. Sixteen servos commanded in one bus transaction all begin
  in the same millisecond, which is the worst case for the supply. Writing them
  a few at a time costs nothing visually.

## Angles are consistently a few degrees off

The internal oscillator is 25 MHz *typical* and individual chips land a few
percent away. Measure yours once and pass the real number to
`setOscillatorFrequency()`. The procedure is in
[Registers](registers.md#trimming-the-oscillator).

If the error is the same on every channel of one board but different between
boards, this is it. If it differs between channels on one board, it is the
servos.

## Two boards, only one responds

Both are on `0x40`. A board with no jumpers bridged is `0x40`, so a second board
needs at least one address jumper. See [I²C addressing](i2c-addressing.md).

If you did set jumpers and it still fails, scan the bus and decode what you
find — `examples/arduino/03-serial-console` has a `pca` command that prints the
jumper pattern for every address it sees.

## Cascaded boards are unreliable

**Check for `0x70`.** A board at `0x70` also answers the All Call address and
moves when other boards are addressed. Pick another address.

**Count the pull-ups.** Every board fits its own 10 kΩ pair. Eight boards is
1.25 kΩ, which is getting stiff. Past about eight, remove RN6 from all but one.

**Follow the current.** `V+` runs through the control header as well as the
servo rows, so a chain fed from one adapter puts every servo's current through
the first board's header pins. Feed each board its own supply and chain only
`GND`, `SDA` and `SCL`.

## Nothing works after experimenting with MODE1

You probably set the EXTCLK bit. It is sticky — writing zero to it does nothing
— and `EXTCLK` is grounded on this board, so the chip has no clock and the PWM
stops.

Power cycle the board, or write `0x06` to the general call address `0x00` for a
software reset.
