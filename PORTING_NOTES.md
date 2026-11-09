# WSPR Sked Qt port - notes

## mainwindow.ui

`mainwindow.ui` is edited by hand in Qt Designer (G4GCI's copy of 2026-11-02 is the master). Code changes
must not depend on widgets Designer may drop or rename: the clock line, two-month calendar, dry-run box
and ppm box are built in code, and the clock labels are found by name.

## Versions

The Qt port is numbered from **0.2.1** (the 0.1.x numbers were working builds during the port). The version is
set once, in `CMakeLists.txt` (`project(WsprSked VERSION ...)`), shown in the main window title and sent to
wsprnet.org as `WS-<version>`.

- 0.2.2 (2026-11-05): Non Rigctl TX tab (web/serial transmitters instead of rigctld); Test Arduino; rigctld
  "Test connection"; rigctld address/port editable (rigctld on another PC); sunrise/sunset from
  sunrise-sunset.org v2; NTP clock check/sync; Gaussian alpha saved; Save Settings; exit/save prompts;
  database password change; type 2/3 as two transmissions (default); Test Encoding with plain-English
  descriptions; Stop Internet; wider test tones never key the rig; RX window error pop-ups; layout fixes.
- 0.2.1 (2026-11-02): slots, receive window and wsprnet upload, Rigctl rigs, Freq&Ant, Switches&Tuners,
  transmit with dry run, test TX buttons, rig frequency correction (ppm), C#-style TX/RX button and countdown.

## Layout
- `mainwindow.ui` - generated from Form1.Designer.cs (`tools/wf2qt.py Form1.Designer.cs . --ui-only`)
- `mainwindow.h/.cpp` - generated once, now hand-maintained (handler stubs for tabs not yet ported)
- `mainwindow_slots.cpp` - Slots tab
- `slotstore.*` - all slot/antenna/config/sunrise SQL (QMYSQL)
- `repeatplanner.*` - which date/times a repeating slot is written to (pure logic)
- `slottablemodel.*` - the slot grid
- `tests/` - `slots_test` (needs scratch MySQL + `fixture.sql`), `encode_test`, `message_test`, `maidenhead_test`, `modulator_test`, `rigctl_test` (needs rigctld), `arduino_test`, `w410a_test` (Linux, socat), `othertx_test` (Linux), `txlog_test` (cmake -DWSPRSKED_TESTS=ON)

## Database
Same schema and same database names as the C# app, so both can run against one server.
Password is read from `~/DBcredential`, the same file the C# app writes (`credentials.*`: port of
Encryption.cs, AES-128-CBC, zero IV, PKCS7, Base64 - self-contained, no OpenSSL). Anything set in
QSettings (`db/host`, `db/port`, `db/user`, `db/password`) overrides it. Defaults: 127.0.0.1 / admin / wspr.

## WSPR encoder (`wsprencode.*`, port of WsprSharp Encode.cs)
Symbols identical to the C# version (checked against output of the original C# in `tests/encode_test.cpp`).
Differences: errors are reported instead of thrown/swallowed. The C# `SanitizeCallsign` threw and then
silently carried on for calls like 2E0ABC (which happened to encode correctly) and for G4GCI/P (which then
crashed in WsprCode); the port checks the actual packing layout, so 2E0ABC encodes and /P calls get a clear
"use type 2/3" error. `SanitizeLocation2` now really pads to 4 (C# discarded the PadRight result).

## Type 2/3 messages (`wsprencode` encodeType2/encodeType3 + `wsprmessage.*`, port of WsprTransmission.cs)
- Encoded natively, so **wspr_enc.exe, the WsprMsg class and the "path to wspr_enc.exe" setting are not needed**.
- Symbols identical to WSJT-X `wsprcode` for type 1/2/3 (prefixes 1-3 chars, 1-char and 2-digit suffixes,
  hashed calls, power rounding). Checked end to end in WSJT-X `wsprd`: type 3 alone decodes as `<...>`,
  after the type 2 it resolves to `<G4GCI/P> IO90GU 23`. A 4-char prefix (VP2E/) is refused; wsprcode
  silently sends VP2/ instead.
- Slot rules in `planMessage()` (see wsprmessage.h). Two cases where C# relied on what wspr_enc.exe did with
  '*'-padded locators, now made explicit - **please confirm**:
  1. "Send type 2/3 as one tx'n": slot 1 sends type 2 for a compound call, else type 1 with 4-char locator;
     slot 2 is not used. (A single WSPR message cannot hold a compound call plus a 6-char locator.)
  2. Type 3 with only a 4-char locator (C# padded to "IO90**"): refused - type 3 needs 6 characters.

## WSPR audio (`wsprmodulator.*` + `wspraudioout.*`, port of FSK.cs)
- `WsprModulator` makes the samples; `WsprAudioOut` plays them with QAudioSink (Qt Multimedia, replaces NAudio).
- Default shaping **Legacy** = the FSK.cs algorithm. Output matches the C# code within 1 LSB of 16 bits
  (float vs double rounding) - checked against a WAV produced by the original C#.
- Optional **ContinuousPhase**: exact WSPR timing (110.59 s vs 110.65 s, spacing 1.4648 vs 1.46413 Hz),
  constant amplitude, 10 ms fade at each end, click-free test tones.
- Measured on air-equivalent files: both shapings have the same out-of-band power (within 0.3 dB) and
  decode in WSJT-X `wsprd` down to about -30 dB SNR (24 trials each at -26..-30 dB). The C# global-time
  sine does not cause phase jumps in practice: spacing x symbol length = 1.000001 (except in 100 Hz test mode).
- Playback differences: samples generated on demand at the sound card's own rate/format; setVolume() applies
  during a transmission (C# MonitorFSK's copy to waveOut.Volume was commented out); no busy-wait loop
  (C# MonitorFSK spun a CPU core for the whole transmission); finished() at the end or on stop().
- Note for the TX timer port: FSKMod started playing in its constructor, so the "wait for second 1" loop
  after it in WSPR_Transmit did not delay the audio. Start timing will be handled in the timer port.
- `tools/wsprwav` writes a 120 s 12 kHz WAV of any call/locator/power for checking with `wsprd`.

## Maidenhead (`maidenhead.*`, port of Maidenhead.cs)
Same lat/long, distance, azimuth and locator results as the C# (checked against output of the original).
Invalid locators give "no position" instead of C#'s (91, 181) point, which Distance/Azimuth treated as real
(due north, past the pole) - the likely cause of DP0POL plotting at the North Pole in WSPR Scheduler Map 2.
Also: no NaN for very close points, lat 90/long 180 stay inside the grid, N/S/E/W labels the right way round.

## Rig control (`rigctl.*`, port of RigCtlD.cs + Form1 sendRigCommand/sendTXRigCommand)
Talks to Hamlib rigctld over TCP like the C# version. Differences: the `RPRT n` reply is checked - C#
sendTXRigCommand returned "ok" for any reply except a dropped connection, so a refused frequency or PTT
(`RPRT -1`) was treated as success and the audio played unkeyed or on the wrong frequency; every wait has a
timeout (C# could hang on Stream.Read); connection refused is reported, not thrown (C# created the TcpClient
outside its try block); replies are read to the end of the line; one connection is kept and re-opened if
rigctld restarts. Tested against Hamlib 4.5 rigctld (dummy rig): frequency, PTT, refused PTT, restart, no reply.
Starting/stopping rigctld itself (Form1 startRigCtlD/stopRigCtlD) comes with the TX port.

## Arduino antenna switch / filter (`arduinoswitch.*`, port of Arduino.cs)
Same TCP protocol (`set-pin <n> high|low`, `filter:<freq>`, port 5000). Differences: failures are reported -
C# set `ok = (response != "error" || response != "")`, which is always true, so a switch that didn't answer
was treated as switched and the TX went ahead on the previous antenna; the set-pin reply must confirm the
pin and level asked for; timeouts on connect and reply. The sketch in Arduino.cs reads one digit of the pin,
so pins 10-13 select pin 1 - caught by the reply check; `tools/arduino_switch_fixed.ino` is a corrected sketch
(full pin number, clears pins 0-13, doesn't drive LED_BUILTIN when it is pin 13). Tested against a fake board.

## W-410A serial antenna switch (`w410a.*`, port of W410A.cs + Form1 W410A_setAnt/W410A_setMode)
Same commands (`W410A0+P<n>`, `W410A0+M<m>`, LF-terminated) and serial settings ("8,none,1,none").
Differences: failures are reported - the C# class put exception text ("The port 'COM3' does not exist") into
`reply` and Form1 set ok = true for any reply, so a missing switch was treated as switched; replies ending in
CR, LF or CR+LF are accepted (C# ReadLine needed LF); the settings string is validated. Any non-empty reply
counts as success because the W-410A's reply format isn't in the C# code. Uses Qt Serial Port (add it in the
Qt Maintenance Tool). Tested over a socat virtual serial pair with a fake switch.

## Non-rigctl rigs (`othertx.*`, port of OtherTX.cs + Form1.sendOtherTXRigCommand)
Same command strings (several commands separated by ';'), HTTP GET <base url><command> with spaces as '+',
or one line on a serial port. Differences:
- HTTP commands are sent in order and each is waited for; C# started them without awaiting, so they could
  arrive in any order and the result was always "ok". A failing command stops the rest (no PTT after a failed
  frequency change).
- Serial settings are read as data,parity,stop,flow (the order the rig settings save them); C# read
  parity,data,... , threw, and silently used 8N1.
- Serial rigs skip the HTTP reachability check that made C# build http://ip:COM3/... and give up.
- Serial replies are optional and ';'-terminated replies are kept (C# needed an LF line, else "error").
Tested against a fake HTTP rig (python) and a fake Yaesu-style CAT rig on a socat serial pair.
SerialComm.cs (Serial.cs) is not used by Form1 - not ported.

## Message boxes (`msg.*`, replaces MessageClass.cs + MessageForm)
Same look as MessageForm (OldLace, fixed size, centred) and the same calls: timed, OK, Yes/No, Add/Edit/Del.
Differences: timed boxes are created on the UI thread (C# ShowDialog from Task.Run); no busy-wait after the
dialog closes (C# `while (!mForm.reply) {}`); closing with X = OK / No / nothing. Yes/No defaults to No.
IsUrlReachable / IsFtpReachable move to the network code that uses them.

## Encoder test on a real sound card: `wsprplay`
`wsprplay --list`, then `wsprplay "G4GCI IO90 23" --device "..."` plays the message at the next even
minute + 1 s for WSJT-X to decode (audio only, no PTT). Tested here: played through PulseAudio, recorded
from the output, decoded by wsprd at -21 dB SNR.

## Transmission log (`txlog.*`, port of Logging.cs)
Same file (~/WSPR_Log) and same line format as WSPR_Transmit. Differences: failures are reported (C# swallowed
them); the start time is passed in (C# logged "now - 1 minute", which is only the start minute if the log is
written before the minute after the transmission ends); Qt handles path separators, so no OpSystem argument.

## Slots tab: where behaviour differs from Form1.cs
- Grid page loads with one query (C#: 30 queries, one connection each). Saves run in one transaction.
- "Time now" at hh:59:53+ goes to (hh+1):00; C# went to hh:00 of the same hour.
- Type 2/3 pair at hh:58: second slot is (hh+1):00; C# wrote hh:00 of the same hour (except at 23:58).
- Answering "No" to "Save as message type 2/3?" saves type 1; C# re-checked the call when writing and saved type 2 anyway.
- Day/Night repeat with power below 1 W now saves; C# SaveSlot_Sun converted watts with ToInt32 and failed.
- Antenna names containing an apostrophe save correctly (all SQL values bound).
- "Delete all repeating slots" on a slot with no parent deletes just that slot; C# deleted every parent-less slot.
- Day/Night windows use each day's sunrise/sunset (C#: refreshed every 7 days) and, in local-time mode, local sun times.
- Main/test slot database choice is saved to wspr_configs.settings.SlotDB (C# had SaveDBName but never called it).
- Lost database connections reconnect on the next query (C#: WSPR Live needed a restart after a power cut - same pattern).
- Date range delete: click first and last dates (QCalendarWidget has no drag-select).
- Kept as C#: stored SwitchPort = shown port + 1; parent string "yyyy-MM-dd HH:mm" ("yyyy-MM-dd00:00" for 24 h sets);
  hour-granular day/night windows; "1hr" label for daily repeats.

## Fixed in this port: resampler

`Resampler12k` (receive, 48/44.1 kHz -> 12 kHz) removed more filtered samples than it had when a block
ended part-way through a step; a debug build asserted, a release build read past the buffer. Now bounded.

## Receive window (`rxform.*`, `rxcycle.*`, `wspraudioin.*`, `rxspot.*`, `rxstore.*`, port of RXForm.cs)
Same window, controls and names (`rxform.ui`, made by `tools/mkrxui.py` with layouts so the grid grows with
the window). Same cycle: start at odd:59 / even:00-01, record until second 110 to temp1..3.wav, run
`wsprd -a <data> -f <dial> [-d|-q] [-o n]`, save to wspr_rpt.received (one "nil rcvd" row when nothing is heard),
put new rows at the top, upload to http://wsprnet.org/post/ with the same fields. Data folder C:/WSPR_Sked on
Windows (as C#), ~/WSPR_Sked elsewhere; debug log C:/Users/Public/wspr_debug.txt (~/wspr_debug.txt elsewhere).

Differences:
- Records whatever format the sound card offers and converts to 12 kHz (C# required 12 kHz from the device,
  which Qt on Windows usually won't give). Length is counted in samples, not a Task.Delay.
- Each file runs from exactly even:00 to even:110 s: audio from :59 is dropped and a slow-starting sound
  card is padded with silence. C# files started whenever recording began (:59, :00 or :01), so the dt
  sent to wsprnet was off by up to a second.
- A recording that fails or delivers nothing is reported; C# decoded an empty file or nothing silently.
- wsprd runs through QProcess with an argument list (paths with spaces work; C# built a cmd.exe string),
  with a 100 s limit. wsprd is looked for in the configured folder, then a `wsprd` folder next to the
  program, then next to the program, then on PATH.
- The dial frequency for `-f`, the nil row and the upload is the one when recording ends (C# mixed the
  value at decode time and the value at :48).
- 2m spots get band code 15 (C# tested "14" before "144" and stored them as 20m).
- Filters use bound values and frequency ranges: the band filter no longer mixes 20m with 2m, LF now finds
  spots, an apostrophe in the call box no longer breaks the query, and the call filter also finds type 3
  decodes ("<G4GCI/P>"). Date filter boxes are UTC and default to the last 24 h.
- "Allow upload own call" off now also blocks own type 3 ("<G4GCI/P>") and base-call decodes.
- Changing the CW/SSB comparison power updates every row, not just the headings. Columns sort by value.
- Deep / Quick / OSD / Upload are saved to rxconfig when changed (C# only in Form1's settings save).
- The RX clock and cycle start always use UTC (C# used local time when "LT" was ticked).
- OSD, band and power lists are drop-downs (the C# ListBoxes were one row high). Close is ignored, as C#
  (CS_NOCLOSE); the main window closes it on exit.
- Not ported yet, needs Form1 parts: RX & Sound tab (device lists, gain slider, wsprd browser - values are
  read from wspr_configs.rxsettings), rig frequency readback (getRigF; RX uses the default frequency until
  then), blockDecodes during transmit.
- `tests/rx_fixture.sql`: wspr_rpt.received matches the real table (key datetime + tx_sign + frequency, so a
  station heard on two frequencies at once is kept twice); rxconfig is inferred from the C# code.

Receiver test on a real sound card: `wsprrx --list`, then
`wsprrx --device "<input>" --wsprd "C:/WSJT/wsjtx/bin" --dial 14.0956 --cycles 2` (no database, no upload).
`wsprrx --file C:/WSPR_Sked/temp1.wav --wsprd ...` decodes a saved recording.

## Spectrum window (`spectrumform.*`, `wsprspectrum.*`, port of SpectrumForm.cs)
Same window: 1300-1700 Hz waterfall of the file being recorded, read every 0.5 s, one line per 16384
samples (Hann window, same dB and colour scale as the NAudio version), scale with red lines at 50/100 Hz,
white line + "HH:MM started on: <MHz> ^^" at each new cycle, gain -40..+40 dB, Hz/MHz under the mouse,
close = hide, paused while hidden. Differences: MHz under the mouse in double precision (C# float was ~1 Hz
coarse at 14 MHz, 16 Hz at 144 MHz); a narrow window shows the strongest bin under each pixel instead of
one sampled bin; resizing keeps the picture; the label uses the recording's cycle time (C# rounded the
clock); the recording is written unbuffered so the waterfall keeps up with it. NAudio FFT replaced by a
plain radix-2 FFT (same 1/N scaling).

## Rigctl rigs tab (`rigctld.*`, `mainwindow_rig.cpp`, port of Form1 rigctld code)
Same settings (wspr.rigctl: Radio, COMport, Baud, IPv4, Port, norigctld, VOX; Hamlib folder in
wspr_configs.settings.HamlibPath) and buttons: Refresh (COM ports + `rigctl -l`), Search, Start/Stop,
Save RigCtl Config (saves and restarts rigctld), "..."/Save Path, "Do not use rigctld", VOX. At start-up the
app starts rigctld unless it is already answering or "Do not use rigctld" is ticked, and reads the dial
frequency every 30 s (seconds 2 and 38) for the receiver.
Differences:
- rigctld gets only the model number (`-m 1046`); C# passed the whole rig-list line.
- "rigctld running" means it answers on its TCP port. C# looked for any process called rigctld, so a copy
  that couldn't open the COM port still showed as running; now you get rigctld's own error message.
- Stop ends the copy this app started (on exit too); if it didn't start one, every rigctld is ended, as C#.
- Save Path saves just the Hamlib folder (C# SaveAll saved every setting).
- Save RigCtl Config is one INSERT ... ON DUPLICATE KEY UPDATE with bound values (C# INSERT, then an UPDATE
  with the values pasted into the SQL).
- The radio, COM and baud lists are drop-downs (C# swallowed typed keys); a saved COM port that isn't
  plugged in stays in the list so it isn't lost.
- Frequency reads use a 0.7 s timeout so a hung rigctld can't freeze the window.

## Freq&Ant tab (`mainwindow_freqant.cpp`)
Antennas: Add / Edit / Del / double-click, name, description, switch + port, second (remote) switch + port,
tuner, saved to wspr.antennas (ports stored as the list index, as C#). Renaming offers to update slots from
today, as C#. Frequencies: Edit / double-click sets the TX audio level (0-1) for a frequency in
wspr.frequencies; the slider also changes the level of a transmission in progress (C# Volume); 22 m and 8 m
are receive only. Differences:
- a new antenna takes the next free number (C# used the list length and could overwrite another antenna);
- a deleted antenna also leaves the antenna drop-downs (C# left it there);
- frequencies you added come back after a restart (C# only refreshed the standard ones);
- bound SQL values (apostrophes in names save); RXAudioLevel gets 1.0 (C# wrote Slot.Switch2 there);
- list columns are padded with spaces under the headings (Qt lists don't expand tabs);
- the slot editor's frequency list includes added frequencies too.

## Font size
The app font is 8 pt (Qt's Windows default is 9 pt), so text fits the C# form's fixed button and label
sizes. Setting ui/fontPointSize (QSettings) changes it; 0 = Qt default.

## Switches&Tuners tab (`mainwindow_hw.cpp`)
Same lists (row = Id in wspr.switches / tuners / filters, 0 = "No ..."), Edit / double-click with the
"Edit ... settings (Y/N)?" question, name, protocol (Serial: port, baud, data/parity/stop/flow; TCP/UDP: IP
and port), type and antenna-port count; names appear in the Freq&Ant switch/tuner lists. Clicking a filter
asks whether to make it the filter in use.
Differences: one bound INSERT ... ON DUPLICATE KEY UPDATE; choosing a switch type no longer overwrites the
port count with another switch's; the filter choice is saved at once (settings.selectedFilter); all lists
are re-enabled after Cancel/Save (C# left the filter list greyed out); drop-down lists, not typeable boxes.

## Transmit (`mainwindow_tx.cpp`, `txplan.*`, port of findSlot / WSPRtimer / StartTX / PTT / wsprTXtimer)

Each 2-minute cycle (UTC): odd :54 look up the next slot; odd :55 switch the antenna (both switches),
select the filter, set the rig frequency and tell the receive window; even :00 key PTT (rigctld `T 1`)
unless VOX is ticked; audio from even :01 at the level saved for that frequency on Freq&Ant; PTT off when
the audio has played (110.6 s), with a backstop at 114 s. Inactive slots still change antenna and
frequency, and the station receives there (as C#).

- **Dry run** (tab with "Enable TX", ticked until you untick it, remembered): everything except PTT.
  The audio still goes to the TX sound card so the level can be checked; with VOX ticked the audio is
  not played either (VOX would key the rig). Receive carries on during a dry-run cycle.
- A switch, filter board or rig that doesn't answer cancels that transmission (C# transmitted on the
  previous antenna / frequency). A rigctld that isn't answering skips the slot, as C#.
- Unticking Enable TX during a transmission stops it. Closing the program unkeys the rig.
- Status goes to the status bar and the debug log (`wspr_debug.txt`); errors also pop up for 4 s.
- **Frequency correction (ppm)** on the Rigctl rigs tab (new, column `ppm` added to wspr.rigctl): the rig is
  tuned that many ppm high to make up for its reference oscillator, and the dial frequency read back is
  divided by the same factor, so receive spots and uploads use the true frequency. For the FT-450D here
  wsprnet showed 12 Hz low on 20 m and 42 Hz low on 6 m: +0.85 ppm.
- Choosing a default antenna (TX config tab) switches to it at once, and clicking a frequency in that
  tab's list tunes the rig at once (C#: activateAntSwitch / testFreq); neither happens during a transmission.
- The TX/RX buttons on the main page and the TX config page show the frequency: "TX 14.0956 MHz" while
  transmitting, "TX next ..." / "RX next ..." before a slot, "RX ..." (the rig's dial) otherwise.
  Colours as C#: olive = receiving with no slot, royal blue = all through a receive slot,
  orange-red / orange flashing = transmitting.
  Countdown as C#: "TX in N seconds" / "RX in: N seconds" to the next slot, then "TX time: n" / "RX time: n"
  0..110 through it. The next slot is read early for this display (C# counted only from :54). Pressing
  either stops a transmission at once (C# asked first) or cancels a queued Wspr TX Test.
- Test buttons (frequency from the list / test box, default antenna, offset and power; refused on 22 m / 8 m):
  **Wspr TX Test** queues a full WSPR transmission for the next even minute (logged like a slot);
  **Test TX/PTT** switches, tunes and transmits at once for 30 s; **Test wspr** plays 30 s of WSPR audio
  with no PTT (refused with VOX ticked). Dry run applies to the first two. Stop Transmit ends any of them.
- Not ported yet: the Non Rigctl TX tab (rigs without Hamlib), tuners.

## Sunrise/sunset, clock sync, Gaussian alpha (`mainwindow_sun.cpp`, `suntimes.*`, `ntpcheck.*`)

- Sunrise/sunset: one request to sunrise-sunset.org API v2 for the whole year (C#: 365 requests to v1), in
  the PC's time zone, stored as local "HH:mm" in wspr_grey.sunrise_sunset as before. Run by the button, at
  start-up when the locator has fewer than 360 days, and every 30 days at 02:11:20 UTC (as C#). The API's
  terms require "Sun data by Sunrise-Sunset.org" to be shown: it is in the sun label.
- Clock: "sync clock" button and daily at 03:15:53 UTC (between the end of a 03:14 transmission and the
  :54 slot look-up; C# used 03:15:00 and 12:15:00, during a transmission). Offset from all four NTP
  timestamps; if over 1 s, Windows is asked to re-sync (net start w32time, w32tm /resync /force - needs
  Administrator) and the clock is checked again. Runs on a worker thread.
- Gaussian alpha: read from settings.Alpha at start-up (it showed the first list entry, 0.01), saved when
  changed, used from the next transmission. 0.1 = 5 % amplitude dip at symbol edges, 0.14 dB less power.

## TX Configuration defaults and Save Settings (`mainwindow_config.cpp`)

- A new slot takes the default frequency, offset, power and antenna shown on the tab, saved or not
  (C# used the values from the last Save Settings). Watts are shown beside the default dBm.
- Callsign and base call are capitalised as typed; the base call follows the call (C#: findBaseCall).
- Save Settings (C#: SaveAll/SaveConfig) checks call and locator, saves to wspr_configs.settings, updates
  the receive window, fetches new sunrise/sunset times if the 4-character square changed, and says
  "Settings saved".

## Database password (TX Configuration, `savePasswordbutton_Click`, `SlotStore::changeOwnPassword`)

Shown as dots; Show ticks it visible. Save password (C#: updatePassword) checks 4+ characters, the
re-entered copy and the allowed characters, asks first, then changes the password of the account the
program logs in as: ALTER USER CURRENT_USER(), else SET PASSWORD = PASSWORD() (MariaDB, no privilege
needed), else SET PASSWORD = '..' (MySQL 8). C# ran ALTER USER 'admin'@'%' six times and ignored errors,
which does nothing for an admin@localhost account and on MariaDB needs CREATE USER rights. A fresh login
with the new password is tried before anything is saved; then ~/DBcredential is rewritten (same format as
C#, with the db_call line) so the C# program picks it up too. If the saved password no longer works, the
typed one can be tried instead (C#: "use this to recover").

## Non Rigctl TX tab (`mainwindow_otherrig.cpp`, `othertx.*`, wspr.rigs)

For transmitters that are not driven through rigctld (a web-controlled beacon, or CAT commands on a COM
port). Used when "Do not use rigctld" is ticked on the Rigctl rigs tab and a rig is chosen here (double
click). Before each slot (odd :55) the TX command is sent with freq (Hz: dial + 1400 + offset), call, grid,
pwr and offset filled in; at the start of the transmission the start-TX command, at the end the RX
command. TCP/UDP rigs are sent http(s)://ip:port/<API URL><command> per ;-separated command (as C#),
serial rigs one line. A command that fails cancels that transmission (C# carried on). Right click deletes
a rig. The rigs table is created if missing, with the C# columns. New, optional "Reply to TX command" per rig (columns
command1/reply1, unused by C#): when filled in, a reply to the TX command must contain it (any case), e.g.
"ok" or "band set to", or that transmission is cancelled. Empty = no reply expected: TCP commands then
only need an HTTP 2xx answer, serial commands none (Yaesu/Kenwood CAT set commands don't reply).

With rigctld off and no rig chosen here, the rig is tuned by hand: the frequency picked in the list on
the Slots tab (or TX Configuration) is what the receiver and wsprnet uploads use, and slot frequencies are
ignored ("slot freq will be ignored"); PTT by VOX. A reminder is shown if none has been picked.

## Remote tab (`mainwindow_remote.cpp`, `remoteserver.*`, C#: RemoteTabWiring + Remote* callbacks)

An HTTPS page (https://<PC address>:<port>, default 5029) for a phone, tablet or PC browser: Status,
Enable/Disable TX, Disable/Enable sked and Log out; with "Allow remote TX/RX control" also a frequency
(the TX Configuration list) and antenna ("AntNo: name") and "Go next slot". Go next slot disables the
schedule and uses the next 2-minute slot (the one after if fewer than 4 s are left) with the default
offset and power: it transmits if TX is enabled (a dry run if Dry run is ticked), otherwise - or on the
22 m / 8 m receive-only frequencies - it switches the antenna, tunes and receives. With rigctld off the
rig can't be tuned from here, which the reply says. The transmission is logged like a scheduled one.
- Settings are in <home>\WSPRSkedRemote\remote.ini (not the database). The password and passcode are kept
  only as salted PBKDF2-SHA256 hashes, so their boxes are empty after Save ("(unchanged)"); leave them
  empty to keep them. Password at least 8 characters, passcode at least 4. Port 1024-65535.
- Certificate: self-signed, made once as remote.pfx in the same folder - by PowerShell
  (New-SelfSignedCertificate, Windows 10/11, no admin needed) or by openssl if that is on the PATH.
  Delete remote.pfx and press Save to make a new one. The browser warns about it the first time.
- Log-in needs all three. 5 failed log-ins from one address lock that address out for 15 minutes; each
  failure is answered after 1 s. Sessions end after 15 minutes idle, on Log out, and whenever Save restarts
  the server (so a new password logs everyone out). Forms carry a token, the cookie is Secure, HttpOnly,
  SameSite=Strict. Remote actions go in the status bar, the debug log and the list on the tab.
- Reaching it from outside the house means forwarding the port on the router; a VPN to the home network
  is safer if you have one.

## Help (`mainwindow_help.cpp`, `rtfhtml.*`, C#: ShowHelp + HelpForm)

F1 (anywhere in the main window) or a click on "F1 Help" opens C:\WSPR_Sked\wspr_sked2_help.rtf in a
window of its own, which stays open beside the main window. Its own name keeps it apart from the C#
version's main_help.rtf in the same folder; it is there (not in Program Files) so it can be edited in
WordPad without administrator rights, and the installer never replaces a copy already there. A copy next
to the program is used if C:\WSPR_Sked has none; wspr_sked2_help.html is used if there is no .rtf.
Qt can't show RTF, so the file is converted to HTML each time (rtfhtml.*): formatting, colours, fonts,
alignment and indents, bullets/numbering, tables, hyperlinks (open in the browser) and PNG/JPEG
pictures. Metafile pictures and Word drawing objects are not shown. Edits show the next time F1 is pressed.

## Database values
Empty text is stored as '' rather than NULL (a null QString binds as NULL in Qt; the C# tables have
NOT NULL text columns, so e.g. a TCP switch with no baud rate failed to save).

## Start-up without MySQL
- main.cpp: before the window opens, if the database is on this PC and port 3306 doesn't answer,
  MySqlStarter tries to start it (Windows service MySQL80/MySQL84/MySQL/mysql/MariaDB, then XAMPP's
  mysqld --standalone) and waits up to 20 s, showing "MySQL isn't running - starting it ...".
- If it isn't installed / won't start, the LoadErrorLabel in the main window says so and why.
- DbBackoff (shared by all stores): after a failure, no new connection attempt for 30 s, and every
  attempt is preceded by a 0.3 s port check - so a missing server no longer freezes the window.

## Linux
- Serial ports: listed as /dev/ttyUSB0 (QSerialPortInfo::systemLocation), and a bare "ttyUSB0" is
  given to rigctld as /dev/ttyUSB0. Before starting rigctld the port is checked: a Windows name
  (COM3 from a copied database), a missing device, or no permission (the dialout group) is reported
  in words instead of rigctld just exiting.
- Hamlib folder: /usr/bin by default (libhamlib-utils); a Windows path from a copied database is ignored.
- wsprd: /usr/bin (wsjtx package) when the saved path is a Windows one or empty.
- MySQL start: the mysql / mariadb systemd service, with pkexec asking for the password (the program
  never runs as root); messages give the apt / systemctl commands instead of XAMPP.
- Fonts: Segoe UI is mapped to Selawik / Liberation Sans / Arimo / Noto Sans / DejaVu Sans, whichever
  is there (appfont.h, used by main.cpp and the window test).
- The Slots tab's "run as administrator" hint becomes the dialout-group hint.
