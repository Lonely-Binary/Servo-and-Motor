# Power

Two rails, one rule each.

- **`V+` runs the servos.** 3.3 V to 6 V, and 6 V is a hard ceiling.
- **`VCC` runs the PCA9685 and the I²C pull-ups.** 3 V to 5 V, and it must match
  your microcontroller's logic voltage.

They are separate rails. Nothing connects them unless you bridge a `V+ TO VCC`
jumper on the underside.

## Getting power in

`V+` has two inlets and they are the same node: the USB Type-C socket and the
5.0 mm screw terminal. Use one. Both go through the same reverse-polarity
MOSFET, so getting the screw terminal backwards costs you nothing.

The Type-C socket is power only — six pins, no data lines. It exists so you can
use a phone charger, not so you can plug the board into a laptop. Do not. A
computer port will not give you the current, and the board cannot tell it to.

A 5 V / 3 A adapter is the sensible default. Budget by servo:

| Servo | Idle | Moving | Stalled |
| --- | --- | --- | --- |
| SG90 class, 9 g | ~10 mA | 100–250 mA | 550–700 mA |
| MG996R class, 55 g | ~10 mA | 500–900 mA | 2.5 A |

The numbers that matter are the ones on the right. Servos do not take turns, and
a robot arm that puts its whole weight on one joint sits near stall
indefinitely. If four MG996Rs can plausibly stall at once, a 3 A supply is not
enough, and the symptom is the microcontroller resetting rather than the servo
giving up.

C1, the 1000 µF electrolytic on `V+`, covers the millisecond-scale current step
when several servos start together. It does not cover a supply that is simply
too small.

## Getting power to the logic

Three ways, in order of preference:

**From the microcontroller.** Wire `VCC` on the control header to your board's
3V3 or 5V pin — whichever matches its I/O. Leave both `V+ TO VCC` jumpers open.
This is the arrangement that cannot go wrong, and it is what the
[quick start](../README.md#quick-start) uses.

**From `V+`, through the 3.3 V regulator.** Bridge the `3V3` jumper. `VCC`
becomes 3.3 V from U4 (ME6217C33, 800 mA, 100 mV dropout at 300 mA) as long as
`V+` is comfortably above 3.3 V. Off a 5 V supply it has 1.7 V of headroom and
behaves. Now the board powers itself and the header only needs three wires:
`GND`, `SDA`, `SCL`.

**From `V+`, through the 5 V regulator.** Bridge the `5V` jumper. Only do this
if your microcontroller is genuinely 5 V — a classic Uno or Nano. And know that
U3 (ME6212C50) has 100 mV of dropout at 100 mA and only 350 mA of headroom, so
from a 5 V adapter `VCC` will sit somewhat below 5 V and sag under load.

Bridging both jumpers shorts the two regulators together. Bridge one or neither.

## The 5 V trap

The I²C pull-ups on this board go to `VCC`. That means `VCC` decides what voltage
appears on `SDA` and `SCL` — not your microcontroller, and not whatever you
guessed.

Bridge the `5V` jumper, wire an ESP32 to the header, and the board pulls `SDA`
and `SCL` up to 5 V into pins rated for 3.3 V. The ESP32, Pico, STM32 and most
other modern parts are not 5 V tolerant. Match `VCC` to your logic level.

The reverse is fine and often useful: `VCC` at 3.3 V with `V+` at 6 V. The
PCA9685 runs happily at 3.3 V, the servos see their full 6 V, and the bus stays
at 3.3 V.

## Common ground

If `V+` comes from its own adapter and `VCC` comes from the microcontroller, the
two supplies must share a ground. Wire `GND` on the control header to the
microcontroller's ground. Without it the PWM edges have no reference and the
servos twitch or ignore you.

## Cascading

Chained boards share `VCC`, `SDA`, `SCL`, `OE` and `GND` through the headers —
and they share `V+` too, because `V+` is on the control header as well as on the
servo rows.

That last point is easy to miss. If you daisy-chain four boards from one
adapter, every amp for every servo travels through the 2.54 mm header pins on
the first board in the chain. For a handful of micro servos that is fine. For
anything bigger, feed each board its own supply through its own screw terminal
and chain only `GND`, `SDA`, `SCL` — cut or leave unfitted the `V+` pin between
boards.

Each board also adds a 10 kΩ pull-up pair. Four boards gives 2.5 kΩ; eight gives
1.25 kΩ, which at 3.3 V is 2.6 mA through whichever device is pulling the line
low. Still legal, but past about eight boards it is worth removing RN6 from all
but one.

## Limits, collected

| | |
| --- | --- |
| `V+` | 3.3 V to 6.0 V. Above 6.0 V damages the regulators, and the PCA9685 with them if a jumper is bridged |
| `VCC` | 3.0 V to 5.0 V. PCA9685 V<sub>DD</sub> is rated 2.3 V to 5.5 V |
| Channel output | 25 mA sink, 10 mA source, minus a 220 Ω series resistor |
| C1 | 1000 µF, 10 V, 105 °C |
| USB-C | 5 V, power only, no data lines |
