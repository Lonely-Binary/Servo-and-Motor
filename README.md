# Servo and Motor

Hardware reference and example code for the Lonely Binary boards that drive
servos and motors.

Each board gets a directory under `boards/` with the same shape:

```
boards/<board>/
  README.md    what the board is, how to wire it, a first sketch that moves something
  docs/        hardware reference, power rules, addressing, troubleshooting
  examples/    code, one directory per sketch
  hardware/    BOM, pick-and-place, Gerbers, net list
  images/      renders of the assembled board
  3d/          GLB model
```

Notes that are not specific to one board live in `docs/`.

## Boards

| Board | Outputs | Interface | |
| --- | --- | --- | --- |
| [PCA9685 16-Channel 12-Bit PWM Servo Driver](boards/pca9685-16ch-servo-driver) | 16 PWM | I²C, `0x40`–`0x7F` | USB-C or screw terminal, 3.3–6 V, reverse-polarity protected, cascadable |

## Shared notes

- [How a hobby servo reads a pulse](docs/servo-signal.md) — pulse widths, why
  the frequency matters less than you think, continuous rotation servos, wire
  colours.
- [Choosing a supply](docs/choosing-a-supply.md) — sizing for stall rather than
  for the data sheet, and the three wiring mistakes that cause most of the
  trouble.

## What is in the hardware directories

The bill of materials, pick-and-place file and Gerbers come straight from the
fabrication package. The net list is extracted from the same package, so the
hardware documentation describes the board that was actually built rather than a
reference design it resembles.

## Licence

MIT. See [LICENSE](LICENSE).
