# Making the Windows installer

The result is `installer\output\WSPR-Scheduler-2-<version>-setup.exe`, a normal Windows setup program
for SourceForge. The version comes from `project(VERSION ...)` in `CMakeLists.txt`, so change it there
and rebuild before making a new release.

## Once only

1. Install **Inno Setup** (free, version 7, or 6.3 or later): https://jrsoftware.org/isdl.php
2. Make the folder `installer\thirdparty` and copy into it the MySQL files that WSPR Scheduler 2
   already runs with (from your build folder):
   ```
   installer\thirdparty\sqldrivers\qsqlmysql.dll
   installer\thirdparty\libmysql.dll
   installer\thirdparty\libcrypto-3-x64.dll
   installer\thirdparty\libssl-3-x64.dll
   ```
3. Optional: copy `wspr_sked2_help.rtf` (the help shown by F1) into `installer\thirdparty`. It is
   installed in `C:\WSPR_Sked`, but never over a copy already there, so edits on a PC are kept.
   To give an existing PC a new help file, copy it to `C:\WSPR_Sked` by hand.
4. Optional: copy `wspr_db.sql` (the blank databases) into `installer\thirdparty` too. The installer then
   puts it in `C:\WSPR_Sked`, as the C# installer does. It never replaces a `wspr_db.sql` that is
   already there.

`thirdparty`, `stage` and `output` are in `.gitignore`, so none of these go to GitHub.

## Each release

1. In Qt Creator, choose the **Release** build (the computer icon at the bottom left), then **Build**.
2. Double-click `installer\make_release.bat`. It:
   - finds the newest Release build;
   - copies `WsprSked.exe` into `installer\stage` and runs `windeployqt` there (Qt DLLs, plugins, the
     TLS plugins for the Remote tab, the MinGW run-time);
   - adds the MySQL files from `thirdparty`;
   - compiles `wspr-scheduler-2.iss` with Inno Setup.
3. Test `installer\output\WSPR-Scheduler-2-<version>-setup.exe`, ideally on a PC without Qt.
4. On SourceForge (https://sourceforge.net/projects/wspr-scheduler/files/), upload it with **Add File**,
   e.g. in a "WSPR Scheduler 2" folder.

## What the installer does

- Installs into `C:\Program Files\WSPR Scheduler 2`, with a Start menu folder (program, requirements,
  GitHub page, uninstall) and, if ticked, a desktop icon. 64-bit Windows 10 or 11 only.
- Shows the GPLv3 licence, then a page listing what else is needed (MySQL and the WSPR databases,
  WSJT-X, Hamlib). The same text is installed as `requirements.txt`.
- Makes `C:\WSPR_Sked` if it isn't there (recordings, wsprd files), writable by all users, and leaves it
  when uninstalling.
- Optional task (unticked): a Windows Firewall rule letting WsprSked.exe accept connections on private
  networks, for the Remote tab. Uninstalling removes the rule.
- **Never touches the databases**, the settings or `DBcredential`. Installing again (an upgrade, or a
  repair) replaces only the program files. Database setup stays a separate step (XAMPP, or
  `wspr_mysql_setup.exe` from SourceForge).
- Asks to close WSPR Scheduler 2 if it is running.
