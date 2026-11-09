# WSPR Scheduler 2 on Linux

Tested on Ubuntu 24.04. Debian 12 and Raspberry Pi OS (64-bit, Bookworm) should work the same way
(Ubuntu 22.04 too, with its older Qt 6).

## Install

1. Get the source: either

   ```
   sudo apt install git
   git clone https://github.com/npalmer1/wspr-scheduler-2.git
   ```

   or download the ZIP from GitHub (**Code → Download ZIP**) and unpack it.

2. Run the install script from that folder, as your normal user (it asks for your password itself):

   ```
   cd wspr-scheduler-2
   bash linux/install.sh
   ```

3. **Log out and back in** (or restart) - the script gives your user access to the serial port and the
   sound card, and that only applies to a new login.

4. Start **WSPR Scheduler 2** from the applications menu (or type `WsprSked`).

### What the script does

- Installs what WSPR Scheduler 2 needs: Qt 6 and its MySQL driver, Hamlib (`rigctld`), WSJT-X (`wsprd`,
  for decoding), MySQL, the Liberation fonts, and the compiler and CMake to build it.
- Builds WSPR Scheduler 2 and installs it in `/usr/local`, with a menu entry.
- Adds your user to the `dialout` (serial ports) and `audio` (sound card) groups.
- Starts MySQL and sets it to start with the PC, adds the `admin` database user (it asks for the password
  - press Enter for the default, `wspr`), and makes the WSPR Scheduler databases from `wspr_db.sql`
  **only if they aren't there already**. Existing databases are never changed.
- Makes `~/WSPR_Sked` (wsprd's files and the F1 help file - `C:\WSPR_Sked` on Windows) and turns on
  network time (WSPR needs the clock right to within a second).

Options: `--no-wsjtx` (no WSJT-X: no receiving), `--no-mysql` (the databases are on another PC),
`--uninstall`.

### wspr_db.sql

The script looks for `wspr_db.sql` (the blank WSPR Scheduler databases) in `linux/`, then
`~/Downloads`. If it isn't found, everything else is installed and it tells you; put the file in one of
those places and run the script again, or import it yourself:

```
sudo mysql < wspr_db.sql
```

### Moving from Windows

To bring your slots and settings across, export the databases on Windows (e.g. with
`mysqldump -u admin -p --databases wspr wspr_configs wspr_grey wspr_rpt wspr_rx > wspr_backup.sql`)
and import them on Linux with `sudo mysql < wspr_backup.sql` **instead of** wspr_db.sql. Then, in WSPR
Scheduler 2, choose the rig's port again on the Rigctl rigs tab (Linux ports are `ttyUSB0`, `ttyACM0`...,
not `COM3`) and the sound card on the RX & Sound config tab.

## Update

```
cd wspr-scheduler-2
git pull                 # or download and unpack the new ZIP over the old folder
bash linux/install.sh
```

It rebuilds and reinstalls the program; the databases and settings are kept.

## First run

- **Rigctl rigs** tab: choose your rig, its serial port (`ttyUSB0` for most USB CAT leads) and baud rate,
  **Save**, then **Start RigCtlD**. If it says the port can't be used, check you logged out and back in
  after installing (`groups` should list `dialout`).
- **RX & Sound config** tab: **Update list**, then choose the rig's sound card for output (TX) and input
  (RX). If the lists say "No sound devices found", see below.

## Troubleshooting

| Problem | Check |
|---|---|
| Port "No permission" | `groups` must show `dialout`; log out and in again |
| Port "not found" | `ls /dev/ttyUSB* /dev/ttyACM*` with the rig's lead plugged in |
| No sound devices | `aplay -l` should list cards; if only `sudo aplay -l` does, you're not in the `audio` group yet |
| MySQL not running | `sudo systemctl status mysql` (or `mariadb`); `sudo systemctl enable --now mysql` |
| "Error whilst loading" / can't log in | the `admin` password: `sudo mysql -e "ALTER USER 'admin'@'localhost' IDENTIFIED BY 'wspr'; ALTER USER 'admin'@'127.0.0.1' IDENTIFIED BY 'wspr';"` |
| Clock | `timedatectl` should say `System clock synchronized: yes` |

## Building by hand

```
sudo apt install build-essential cmake qt6-base-dev qt6-tools-dev qt6-multimedia-dev qt6-serialport-dev \
                 libqt6sql6-mysql libhamlib-utils wsjtx fonts-liberation
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
sudo cmake --install build          # or just run build/WsprSked
```
