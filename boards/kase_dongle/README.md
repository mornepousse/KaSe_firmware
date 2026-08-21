# kase_dongle — USB receiver for the Niphargus

USB receiver for the Niphargus split keyboard. It receives **finished HID
reports** over NRF24L01+ and presents them to the host as a USB HID composite
device plus a CDC binary port.

Hardware: `~/Documents/PCB-esp/dongle/dongle/` (KiCad 9 project, M.2 Key B 3042
form factor).

## What it does, and what it does not

The dongle has **no matrix, no keymap, no input engine**. The Niphargus left half
runs its own — it has to anyway, since it must work over USB with no dongle in
sight — and sends nothing but completed HID reports. The dongle pushes them
through unchanged.

That is not an implementation detail: it is what makes **two keymap engines
structurally impossible in this system**, a question that came up three times
before it was settled. The engine, the keyboard half of the CDC protocol and the
board keymap are not compiled for this role; the keymap commands are not
registered and answer `KS_STATUS_ERR_UNKNOWN`, which tells the controller
software plainly that this device does not do that. A silent no-op would be worse
than the missing feature.

It keeps: pairing, RF reception on two slots, HID presentation to the host, CDC,
and supervision (battery, link quality).

Full design:
[`dongle-role-niphargus-design.md`](../../docs/superpowers/specs/2026-08-19-dongle-role-niphargus-design.md).

## The two slots

These are no longer two halves of one keyboard:

| Slot | Radio | Device |
|---|---|---|
| 0x01 | NRF#1 | the keyboard — Niphargus master half |
| 0x02 | NRF#2 | the **Conchodytes** mouse |

Hence the rule locked down by `main/comm/rf/rf_slot.h`: **losing a slot releases
only what that slot was holding.** A mouse going out of range must not wipe the
keystroke in progress.

The pairing keys in NVS keep their original names (`mac_left` / `mac_right`):
renaming them would unpair hardware already in the field for a purely cosmetic
gain.

## Build & flash

```bash
source ~/esp/esp-idf/export.sh
idf.py -B build_kase_dongle -DBOARD=kase_dongle \
       -DSDKCONFIG=build_kase_dongle/sdkconfig build
```

Every board keeps its own build directory **and its own `sdkconfig`**. Never
build two boards in the same `build/` with the root `sdkconfig`: the
configuration leaks from one to the other.

⚠️ **The `/dev/ttyUSBN` numbers of the dongle and the V2D swap between plug-ins.**
Check the MAC before flashing anything — the dongle is `ac:a7:04:18:81:ec` —
rather than trusting the port number.

```bash
idf.py -B build_kase_dongle -p /dev/ttyUSB0 flash
```

## Pinout (ESP32-S3-WROOM-2)

Read off the `dongle.kicad_sch` netlist:

| Signal | GPIO | Notes |
|---|---|---|
| SPI MOSI | GPIO5 | shared, R15 100 Ω series |
| SPI MISO | GPIO6 | shared, R16 100 Ω series |
| SPI SCK | GPIO7 | shared, R17 100 Ω series |
| NRF#1 (keyboard) CSN / CE / IRQ | GPIO13 / GPIO14 / GPIO8 | |
| NRF#2 (mouse) CSN / CE / IRQ | GPIO1 / GPIO4 / GPIO2 | |
| USB D+ / D− | GPIO20 / GPIO19 | native OTG, full-speed |
| Bootstrap IO0 | GPIO0 | flash mode via the CH340 RTS line |

GPIO1 is a UART0 strapping pin that floats low at reset, so both CSN lines are
driven high before either radio is initialised — otherwise NRF#2 latches the SPI
traffic meant for NRF#1 in parallel and corrupts it.

## Status

- **Bring-up** ✅ enumerates as `303a:4001`, CDC PING/FEATURES answer.
- **NRF RX stack** ✅ both radios probe on the real board, channels 76/82,
  `rf_rx_task` starts.
- **HID relay** — written; the bench criterion is `ack ≈ 100 %` with `maxrt = 0`
  under sustained typing. It cannot be checked before the Niphargus boards exist:
  the V2D no longer carries its nRF module.

ESP-NOW went out with the first-generation half firmware.
