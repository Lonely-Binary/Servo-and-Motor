# I²C addressing

The PCA9685's 7-bit address is `0x40` OR'd with the six address pins:

```
address = 0x40 | (A5<<5 | A4<<4 | A3<<3 | A2<<2 | A1<<1 | A0)
```

On this board each address pin has a 10 kΩ pull-down and a solder jumper up to
`VCC`. **Open is 0. Bridged is 1.** A board out of the packet, with nothing
bridged, answers on `0x40`.

The jumper pads are on the underside, under the label `I2C ADDR`, in order `A0`
at the top through `A5` at the bottom. Bridge one by dragging a bead of solder
across the two pads.

## The table

| Address | A5 | A4 | A3 | A2 | A1 | A0 |
| --- | --- | --- | --- | --- | --- | --- |
| `0x40` | | | | | | |
| `0x41` | | | | | | ● |
| `0x42` | | | | | ● | |
| `0x43` | | | | | ● | ● |
| `0x44` | | | | ● | | |
| `0x45` | | | | ● | | ● |
| `0x46` | | | | ● | ● | |
| `0x47` | | | | ● | ● | ● |
| `0x48` | | | ● | | | |
| `0x50` | | ● | | | | |
| `0x60` | ● | | | | | |
| `0x7F` | ● | ● | ● | ● | ● | ● |

● = bridged. Any combination in between works the same way; the pattern is plain
binary.

## The address you must not use

**Skip `0x70`** — A5 and A4 bridged, everything else open.

`0x70` is the PCA9685's LED All Call address, and the `ALLCALL` bit in MODE1
defaults to 1. A board set to `0x70` answers to its own address *and* to every
All Call broadcast on the bus, which means it also moves whenever another board
is addressed by All Call. NXP counts 62 usable addresses out of the 64 for this
reason.

You can clear `ALLCALL` in software and reclaim the address, but there are 63
other ones.

## Finding boards on the bus

Any I²C scanner works. A board that is powered and correctly wired shows up
immediately; one that does not appear is almost always a `VCC` or ground
problem, not an address problem.

[`examples/arduino/02-sweep-all`](../examples/arduino/02-sweep-all) scans on
startup and uses the lowest address it finds.
[`examples/arduino/03-serial-console`](../examples/arduino/03-serial-console)
has a `pca` command that scans `0x40`–`0x7F` and decodes each hit back into
jumper positions, which is the fast way to check you soldered what you meant to.

## Cascading

Boards on one bus need different addresses and nothing else. Chain `GND`, `SDA`,
`SCL` and `VCC` from board to board — the right-edge socket accepts the next
board's left-edge pins directly — and give each board a unique jumper pattern.

Sixty-three boards is 1008 channels, so the real limits arrive long before the
address space does: bus capacitance, pull-up strength, and the current to run
that many servos. See [Power](power.md#cascading).

`OE` is also on the header, so a chain shares one output-enable line. Pull it
high and every channel on every board stops together.
