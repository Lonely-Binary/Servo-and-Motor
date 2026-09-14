# Choosing a supply

Servo projects fail at the power supply more often than anywhere else, and the
symptom is rarely a servo that does not move. It is the microcontroller
resetting, or the I²C bus locking up, or a servo that twitches for no reason.
All three are the supply collapsing.

## Size for stall, not for the data sheet

Data sheets quote a running current. That number is measured with no load. The
one that matters is stall current, which is what a servo draws when it is
holding something against gravity — the normal state of a robot arm.

| Servo class | Running | Stall |
| --- | --- | --- |
| SG90, 9 g | 100–250 mA | 550–700 mA |
| MG90S, metal gear 9 g | 200–400 mA | 800 mA–1.2 A |
| MG996R, 55 g | 500–900 mA | 2.5 A |

Multiply by the number of servos that can plausibly be loaded at once. Four
MG996Rs on an arm is 10 A of worst case, which is a bench supply, not a USB
charger.

Two things reduce the number honestly:

- **Not everything moves at once.** A pan-tilt head has two servos and only one
  of them is fighting gravity.
- **Not everything is loaded.** A gripper that has closed on nothing draws
  almost nothing.

Two things that do not:

- Hoping. A supply that browns out under load does so at the worst moment.
- A bigger capacitor. Bulk capacitance covers the millisecond when several
  servos start together. It does not cover a second of stall.

## Voltage

Most hobby servos are specified for 4.8–6.0 V. Some are happy at 7.4 V from a
2S lithium pack; most are not, and running a 6 V servo at 7.4 V cooks it slowly.

Higher voltage inside the rating means more torque and more speed, and more
current at stall. If a project is marginal on torque, going from 5 V to 6 V is
usually easier than going up a servo size.

Check the board's own limit too. The
[PCA9685 driver](../boards/pca9685-16ch-servo-driver) tops out at 6 V because of
its regulators.

## Wiring

**Never power servos from a computer's USB port.** A USB-A port offers 500 mA.
One 9 g servo can exceed that on its own, and the computer's response to an
overcurrent is not always graceful.

**Never power servos from the microcontroller's 5 V pin.** That pin comes from a
small regulator on the microcontroller board, which is sized for the
microcontroller.

**Share a ground.** If the servos have their own supply and the logic has
another, the two grounds must be joined or the pulse has no reference.

**Watch what the current flows through.** A 2.54 mm header pin is usually rated
around 3 A. Chaining four driver boards from one adapter puts every servo's
current through the first board's header pins. Feed each board its own supply
instead.

## A starting point

For a desk-sized project with up to about six small servos: a 5 V / 3 A USB-C
adapter, into the board's own power inlet, with the microcontroller powered
separately over its own USB cable and the grounds joined through the control
header.

For anything with metal-gear servos under load: a bench supply or a purpose-made
5–6 V switching supply rated at several amps, with the current limit set
somewhere you will notice before the smoke.
