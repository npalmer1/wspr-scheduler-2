-- Minimal copies of the databases the Slots tab reads, for testing against a scratch MySQL/MariaDB.
-- Column names follow what the C# Form1.cs reads. Do NOT run this against your real server.
CREATE DATABASE IF NOT EXISTS wspr;
DROP TABLE IF EXISTS wspr.antennas;
CREATE TABLE wspr.antennas (
  AntNo INT NOT NULL PRIMARY KEY, Antenna TEXT NOT NULL, Description TEXT NOT NULL,
  Switch INT NOT NULL, Tuner INT NOT NULL, Rotator INT NOT NULL DEFAULT 0, SwitchPort INT NOT NULL,
  TunerPort INT NOT NULL DEFAULT 0, Azimuth INT NOT NULL DEFAULT 0,
  Switch2 INT NOT NULL DEFAULT 0, SwitchPort2 INT NOT NULL DEFAULT 0);
INSERT INTO wspr.antennas (AntNo, Antenna, Description, Switch, Tuner, SwitchPort, Switch2, SwitchPort2) VALUES
  (1,'Windom','Windom 40-10m',1,0,0,0,0),
  (2,'Vertical','5/8 vertical',1,1,2,0,0),
  (3,'G4GCI''s loop','Magnetic loop (apostrophe test)',0,0,0,0,0);

-- Freq&Ant tab: TX audio level per frequency (C#: ReadFrequencies / SaveFrequency)
DROP TABLE IF EXISTS wspr.frequencies;
CREATE TABLE wspr.frequencies (
  Frequency DOUBLE NOT NULL PRIMARY KEY, AudioLevel DOUBLE NOT NULL,
  RXAudioLevel DOUBLE NOT NULL DEFAULT 1, Region INT NOT NULL DEFAULT 0);
INSERT INTO wspr.frequencies (Frequency, AudioLevel) VALUES (14.0956, 0.8), (7.0386, 1), (10.1402, 0.5);

CREATE DATABASE IF NOT EXISTS wspr_configs;
CREATE TABLE IF NOT EXISTS wspr_configs.settings (
  ConfigID INT NOT NULL PRIMARY KEY, Callsign TEXT, BaseCall TEXT, DefaultF DOUBLE, `Offset` INT, Power INT,
  Locator TEXT, LocatorLong TINYINT(1), DefaultAnt TEXT, Alpha DOUBLE, DefaultAudio INT, HamlibPath TEXT,
  MsgType INT, TimeZone TEXT, AllowType2 TINYINT(1), oneMsg TINYINT(1), WsprmsgPath TEXT,
  stopsolar TINYINT(1), stopRX TINYINT(1), SlotDB TEXT NOT NULL);
ALTER TABLE wspr_configs.settings DROP COLUMN IF EXISTS selectedFilter;
REPLACE INTO wspr_configs.settings (ConfigID, Callsign, BaseCall, DefaultF, `Offset`, Power, Locator, LocatorLong,
  DefaultAnt, Alpha, DefaultAudio, HamlibPath, MsgType, TimeZone, AllowType2, oneMsg, WsprmsgPath, stopsolar, stopRX, SlotDB) VALUES
  (1,'G4GCI','G4GCI',14.0956,50,23,'IO90GU',0,'Windom',0.1,1,'C:/WSPR_Sked',1,'UTC',0,0,'',0,0,'wspr_slots');

CREATE DATABASE IF NOT EXISTS wspr_grey;
CREATE TABLE IF NOT EXISTS wspr_grey.sunrise_sunset (
  locator VARCHAR(8) NOT NULL, date VARCHAR(5) NOT NULL, sunrise TEXT, sunset TEXT, PRIMARY KEY (locator, date));
REPLACE INTO wspr_grey.sunrise_sunset VALUES
  ('IO90','11-01','06:56','16:37'), ('IO90','11-02','06:58','16:35'), ('IO90','11-03','07:00','16:33'),
  ('IO90','06-21','04:59:00','21:22:00');

-- rigctld settings (Rigctl rigs tab). Column names from Form1.cs SaveRigctl/readRigctl; VOX is added by the app.
DROP TABLE IF EXISTS wspr.rigctl;
CREATE TABLE wspr.rigctl (
  RigctlID INT NOT NULL PRIMARY KEY, Radio TEXT NOT NULL, COMport TEXT NOT NULL, Baud TEXT NOT NULL,
  IPv4 TEXT NOT NULL, Port TEXT NOT NULL, norigctld TINYINT(1) NOT NULL);
REPLACE INTO wspr.rigctl (RigctlID, Radio, COMport, Baud, IPv4, Port, norigctld) VALUES
  (0, '  1046  Yaesu                  FT-450                  20221109.0      Stable      RIG_MODEL_FT450', 'COM4', '38400', '127.0.0.1', '4532', 0);

-- Switches&Tuners tab (C#: ReadHardware / SaveHardware). Id = position in the list, 0 = "No switch".
DROP TABLE IF EXISTS wspr.switches;
DROP TABLE IF EXISTS wspr.tuners;
DROP TABLE IF EXISTS wspr.filters;
CREATE TABLE wspr.switches (Id INT NOT NULL PRIMARY KEY, Name TEXT NOT NULL, Protocol TEXT NOT NULL, Port TEXT NOT NULL,
  IP TEXT NOT NULL, Baud TEXT NOT NULL, Serial TEXT NOT NULL, Type TEXT NOT NULL, Channels INT NOT NULL, ChannelType TEXT NOT NULL);
CREATE TABLE wspr.tuners LIKE wspr.switches;
CREATE TABLE wspr.filters LIKE wspr.switches;
INSERT INTO wspr.switches VALUES
  (1, 'Arduino shack', 'TCP', '5000', '192.168.1.60', '', '', 'Arduino R4 wifi (antenna switchable on data pins)', 6, '0'),
  (2, 'W410A loft', 'Serial', 'COM5', '', '9600', '8,none,1,none', 'W-410A (antenna selectable by commands)', 4, '0');
INSERT INTO wspr.filters VALUES
  (1, 'LPF board', 'TCP', '5000', '192.168.1.61', '', '', 'Arduino - single action on data pin 1', 4, '0');
