# WSPR Scheduler 2

A WSPR beacon scheduler and receiver for amateur radio, written in C++ with Qt 6. It is a port of the
original C# WinForms *WSPR Scheduler* by Neville, G4GCI.

Plan WSPR transmit and receive slots by date, time, band and antenna, and the program switches antennas
and filters, tunes the rig, keys it and sends the WSPR audio at the right moment. Between transmissions
it receives and decodes WSPR spots (with WSJT-X's `wsprd`) and can upload them to wsprnet.org.

## Features

- **Slots:** a two-month calendar and hourly slot grid, repeating slots, day/night-only slots based on
  sunrise and sunset, type 1 and type 2/3 messages (compound calls, 6-character locators).
- **Rig control:** Hamlib `rigctld` (local or on another PC), with a ppm correction for the rig's
  reference. Rigs without rigctld are supported through HTTP or serial commands (the "Non Rigctl TX"
  tab), and a rig tuned by hand with VOX keying also works.
- **Antennas and filters:** WiFi Arduino switches, W-410A serial switches and filter boards.
- **Receive:** recording, `wsprd` decoding, a spots table with filters, a spectrum view and wsprnet
  uploads.
- **Testing:** test transmissions, a dry-run mode (on by default) and wider test tones.
- **Clock and sun:** a daily NTP clock check and sunrise/sunset times from sunrise-sunset.org.
- **Remote control:** an HTTPS page for a phone or browser. It can enable or disable TX and the
  schedule, or pick a frequency and antenna for the next slot.

## Requirements

- Qt 6.4 or later: Widgets, Sql, Multimedia, Network and SerialPort, plus the QMYSQL driver.
- MySQL or MariaDB with the WSPR Scheduler databases.
- WSJT-X, for `wsprd`.
- Hamlib, for `rigctld`, if a rig is controlled through it.

Windows build and set-up steps are in [DEPLOY_WINDOWS.md](DEPLOY_WINDOWS.md). Notes on the port and on
where it differs from the C# version are in [PORTING_NOTES.md](PORTING_NOTES.md).

## Building

```
cmake -S . -B build
cmake --build build
```

Tests need a scratch database, never the real one. Configure with `-DWSPRSKED_TESTS=ON`.

## Linux

WSPR Scheduler 2 runs on Linux too (Ubuntu 24.04 tested; Debian and Raspberry Pi OS should work).
To install, from this folder:

```
bash linux/install.sh
```

It installs everything needed (Qt 6, Hamlib, WSJT-X's wsprd, MySQL), builds and installs the program
with a menu entry, gives your user access to the serial port and sound card, and sets up the database
user and, if they aren't there yet, the databases. See [linux/README.md](linux/README.md).

## Licence

GNU General Public License v3.0. See [LICENSE](LICENSE).
