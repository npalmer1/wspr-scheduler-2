# Building and running on the WSPR PC (Windows)

1. Qt Online Installer (open source): https://www.qt.io/download-qt-installer-oss
   Tick: Qt 6.11.1 > MinGW 64-bit; Qt 6.11.1 > Additional Libraries > Qt Multimedia, Qt Serial Port;
   Developer and Designer Tools > the MinGW listed for 6.11.1, CMake, Ninja (Qt Creator is included).
2. Copy the `wspr_sked_qt` folder across (no build folder needed). In Qt Creator: File > Open File or
   Project > `CMakeLists.txt`, choose the Desktop Qt 6.11.1 MinGW 64-bit kit, Build.
   The build runs windeployqt, so the Qt DLLs are copied next to the programs.
3. MySQL driver (not part of Qt) - in the build folder:
   - `sqldrivers\qsqlmysql.dll` built for Qt 6.11.1 MinGW 64-bit
     (https://github.com/thecodemonkey86/qt_mysql_driver/releases), the same file as on the other PC;
   - `libmysql.dll`, `libcrypto-3-x64.dll`, `libssl-3-x64.dll` next to `WsprSked.exe`.
   If the app says the driver is missing: set QT_DEBUG_PLUGINS=1 in Projects > Run > Environment and look
   for qsqlmysql in the output ("version" 396032 = Qt 6.11; anything else is the wrong build).
4. Also needed on that PC: MySQL/MariaDB with the WSPR Sked databases, ~\DBcredential (as for the C# app),
   WSJT-X (wsprd.exe, default C:\WSJT\wsjtx\bin) and Hamlib (rigctld.exe) if rigctld is used.
4a. Remote tab: windeployqt copies the TLS plugins (`tls\qschannelbackend.dll` etc.) - keep that folder
    next to WsprSked.exe or the remote page can't start. Windows Firewall asks the first time the port is
    opened: allow Private networks. The certificate is made with PowerShell the first time Save is pressed.
5. Close the C# WSPR Sked first (both apps would use the same rig COM port, sound card and database).
6. Tests (optional, need a scratch database, NOT the real one): configure with -DWSPRSKED_TESTS=ON.
