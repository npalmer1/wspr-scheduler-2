; WSPR Scheduler 2 - Windows installer (Inno Setup 6, https://jrsoftware.org/isinfo.php)
;
; Don't compile this by hand: run make_release.bat, which fills the "stage" folder with the program,
; the Qt files and the MySQL driver, then compiles this script.
; The version comes from WsprSked.exe (project(VERSION) in CMakeLists.txt).

#define AppName   "WSPR Scheduler 2"
#define AppExe    "WsprSked.exe"
#define AppUrl    "https://github.com/npalmer1/wspr-scheduler-2"
#define StageDir  "stage"

#ifndef AppVersion
  #define AppVersion GetStringFileInfo(AddBackslash(SourcePath) + StageDir + "\" + AppExe, "ProductVersion")
#endif
#if AppVersion == ""
  #error stage\WsprSked.exe is missing or has no version - run make_release.bat
#endif

[Setup]
; never change AppId: it is how Windows knows a new version replaces the old one
AppId={{7389BFAA-2E98-4D83-9E93-5D7E864075A6}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=G4GCI
AppPublisherURL={#AppUrl}
AppSupportURL={#AppUrl}/issues
AppUpdatesURL={#AppUrl}
VersionInfoVersion={#AppVersion}
VersionInfoDescription={#AppName} setup
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
LicenseFile=..\LICENSE
InfoBeforeFile=requirements.txt
SetupIconFile=..\wsprsked.ico
UninstallDisplayIcon={app}\{#AppExe}
UninstallDisplayName={#AppName}
OutputDir=output
OutputBaseFilename=WSPR-Scheduler-2-{#AppVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
CloseApplications=yes
RestartApplications=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"
Name: "firewall"; Description: "Allow the Remote tab's web page through Windows Firewall (private networks only)"; GroupDescription: "Remote control:"; Flags: unchecked

[Dirs]
; recordings, wsprd output and (by default) Hamlib - the same folder the C# WSPR Scheduler uses
Name: "C:\WSPR_Sked"; Permissions: users-modify; Flags: uninsneveruninstall

[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\README.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "requirements.txt"; DestDir: "{app}"; Flags: ignoreversion
#ifdef HaveHelp
; the help shown by F1: in C:\WSPR_Sked so it can be edited without administrator rights, and never
; over a copy already there (its own name keeps it apart from the C# version's main_help.rtf)
Source: "thirdparty\wspr_sked2_help.rtf"; DestDir: "C:\WSPR_Sked"; Flags: onlyifdoesntexist uninsneveruninstall
#endif
#ifdef HaveDbSql
; The databases themselves are never touched: this only copies the blank-database file to where the
; C# installer puts it, and never over one already there. Importing it is a separate, deliberate step.
Source: "thirdparty\wspr_db.sql"; DestDir: "C:\WSPR_Sked"; Flags: onlyifdoesntexist uninsneveruninstall
#endif

[INI]
Filename: "{app}\WSPR Scheduler 2 on GitHub.url"; Section: "InternetShortcut"; Key: "URL"; String: "{#AppUrl}"

[Icons]
Name: "{autoprograms}\{#AppName}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{autoprograms}\{#AppName}\Requirements"; Filename: "{app}\requirements.txt"
Name: "{autoprograms}\{#AppName}\WSPR Scheduler 2 on GitHub"; Filename: "{app}\WSPR Scheduler 2 on GitHub.url"
Name: "{autoprograms}\{#AppName}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
; firewall: remove any old rule first so a reinstall doesn't add a second one
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""{#AppName}"""; Flags: runhidden; Tasks: firewall
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall add rule name=""{#AppName}"" dir=in action=allow program=""{app}\{#AppExe}"" enable=yes profile=private"; Flags: runhidden; Tasks: firewall
Filename: "{app}\{#AppExe}"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "{sys}\netsh.exe"; Parameters: "advfirewall firewall delete rule name=""{#AppName}"""; Flags: runhidden; RunOnceId: "DeleteFirewallRule"

[UninstallDelete]
Type: files; Name: "{app}\WSPR Scheduler 2 on GitHub.url"
