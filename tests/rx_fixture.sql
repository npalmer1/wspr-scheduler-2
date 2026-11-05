-- Receive-side tables for testing against a scratch MySQL/MariaDB. Do NOT run this against your real server.
-- wspr_rpt.received matches the real table (phpMyAdmin structure from G4GCI's server): primary key
-- (datetime, tx_sign, frequency) so one station heard on two frequencies (e.g. an image) is kept twice,
-- plus indexes on datetime and frequency. rxconfig is inferred from RXForm.cs.
CREATE DATABASE IF NOT EXISTS wspr_rpt_selftest;
DROP TABLE IF EXISTS wspr_rpt_selftest.received;
CREATE TABLE wspr_rpt_selftest.received (
  datetime DATETIME NOT NULL,
  band SMALLINT(11) NOT NULL,
  tx_sign VARCHAR(15) NOT NULL,
  tx_loc VARCHAR(10) NOT NULL,
  frequency DOUBLE NOT NULL,
  power SMALLINT(11) NOT NULL,
  snr MEDIUMINT(11) NOT NULL,
  drift SMALLINT(6) NOT NULL,
  distance INT(11) NOT NULL,
  azimuth SMALLINT(11) NOT NULL,
  reporter VARCHAR(20) NOT NULL,
  reporter_loc VARCHAR(10) NOT NULL,
  dt FLOAT NOT NULL,
  PRIMARY KEY (datetime, tx_sign, frequency),
  INDEX (datetime),
  INDEX (frequency)
) DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;
DROP TABLE IF EXISTS wspr_rpt_selftest.rxconfig;
CREATE TABLE wspr_rpt_selftest.rxconfig (
  id INT NOT NULL PRIMARY KEY, deep TINYINT(1) NOT NULL, quick TINYINT(1) NOT NULL, osd INT NOT NULL,
  upload TINYINT(1) NOT NULL);

CREATE DATABASE IF NOT EXISTS wspr_configs;
DROP TABLE IF EXISTS wspr_configs.rxsettings;
CREATE TABLE wspr_configs.rxsettings (
  id INT NOT NULL PRIMARY KEY, outputname TEXT, outputdevice INT, inputname TEXT, inputdevice INT, outlevel INT, inlevel INT,
  samedev TINYINT(1), wsprdpath TEXT);
INSERT INTO wspr_configs.rxsettings VALUES
  (0, '0: Speakers (3- USB Audio Device)', 0, '1: Microphone (3- USB Audio Devi', 1, 80, 150, 0, 'C:/WSJT/wsjtx/bin');
