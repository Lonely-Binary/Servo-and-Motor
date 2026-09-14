# How a hobby servo reads a pulse

A hobby servo does not take an angle. It takes a pulse, and it holds whatever
position that pulse width corresponds to, fighting you if you push it.

The convention comes from 1970s radio control gear, where a receiver decoded a
PPM frame and handed each channel a pulse. Everything since has kept it:

| Pulse width | Position |
| --- | --- |
| 1000 µs | One end |
| 1500 µs | Centre |
| 2000 µs | The other end |

Send that pulse again every 20 ms or so and the servo keeps holding. Stop
sending and most servos go limp, though some hold their last position.

## The frequency matters less than you think

50 Hz — a pulse every 20 ms — is the traditional rate, and it is what almost
every example uses. The servo's control loop samples the pulse and updates; it
does not care much whether the gap is 20 ms or 15 ms. Analogue servos generally
tolerate 40–60 Hz. Digital servos often accept several hundred hertz and feel
crisper for it, but only if the data sheet says so.

What matters is the **pulse width**, not the duty cycle. If you change the
frequency on a controller that works in duty cycle, every angle moves. This is
why it is worth working in microseconds and letting the driver do the
conversion.

## The range is not 1000–2000

That is the specification. Real servos overshoot it in both directions, and the
amount varies by model and by unit.

- A 9 g SG90 typically covers about 180° over roughly 500–2400 µs.
- Many standard servos cover 90–120° over 1000–2000 µs and need a wider pulse
  to reach their mechanical limits.
- Some cheap servos hit an internal stop well before 2400 µs and buzz against
  it, drawing stall current the whole time.

So: start at 1000–2000 µs, then widen 50 µs at a time until you hear the servo
strain, and back off from there. Write the numbers you found on the servo with a
marker. Do not assume the next one off the same reel is identical.

The common `SERVOMIN 150` / `SERVOMAX 600` pair from PCA9685 tutorials is 732 µs
to 2928 µs at 50 Hz. That is outside what most hobby servos are built for, and
it is why so many people's first PCA9685 project buzzes.

## Continuous rotation servos

A continuous rotation servo is a normal servo with the feedback potentiometer
disconnected and the stop removed. The pulse no longer sets a position; it sets
a speed and direction.

| Pulse width | Behaviour |
| --- | --- |
| 1500 µs | Stopped |
| Below 1500 µs | One direction, faster the further you go |
| Above 1500 µs | The other direction |

"Stopped" is approximate. Most units creep at exactly 1500 µs and need trimming
— by a pot on the case, or by finding the real stop value and using that.

There is no position feedback. If you need to know where it is, you need an
encoder.

## The wires

| Colour | Signal | Also seen as |
| --- | --- | --- |
| Brown | Ground | Black |
| Red | Power | Red |
| Orange | Signal | Yellow, white |

Brown/red/orange is the Futaba colour order and the common one. Black/red/white
is JR. Either way, **red is in the middle**, which is the property worth
remembering: a connector plugged in backwards puts power where the signal should
be, and the signal where power should be.

Boards in this repository print the row colours on the silkscreen so you can
check without unplugging anything.

## Power

Signal current is nothing. Motor current is everything, and it does not come
from the microcontroller.

Give the servo rail its own supply, share a ground with the controller, and size
the supply for stall rather than for the idle figure in the data sheet. A servo
holding a load against gravity sits near stall indefinitely and does not
complain about it — it just draws the current.

See [Choosing a supply](choosing-a-supply.md).
