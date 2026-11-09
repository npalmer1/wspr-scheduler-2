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

## Linux (Ubuntu 24.04)

WSPR Scheduler 2 runs on Linux too. On Ubuntu:

```
sudo apt install build-essential cmake git qtcreator qt6-base-dev qt6-tools-dev qt6-multimedia-dev \
                 qt6-serialport-dev libqt6sql6-mysql mysql-server libhamlib-utils wsjtx fonts-liberation
sudo usermod -aG dialout $USER      # use the rig's serial port (log out and in again afterwards)
```

- `libhamlib-utils` gives rigctld / rigctl (in /usr/bin), `wsjtx` gives wsprd (in /usr/bin).
- Serial ports are /dev/ttyUSB0, /dev/ttyACM0 ... (a database copied from Windows still says COM3:
  choose the port again on the Rig control tab and Save).
- Files that are in C:\WSPR_Sked on Windows (wsprd's working files, the help file
  wspr_sked2_help.rtf) are in ~/WSPR_Sked.
- If MySQL isn't running, WSPR Scheduler offers to start it and Ubuntu asks for your password
  (`sudo systemctl enable mysql` makes it start with the PC). The program itself never runs as root.
- The PC clock is kept right by Ubuntu's own time service; WSPR Scheduler checks it and reports.
- The windows are laid out for Windows' Segoe UI font: Liberation Sans (fonts-liberation) stands in.

## Licence

GNU General Public License v3.0. See [LICENSE](LICENSE).
