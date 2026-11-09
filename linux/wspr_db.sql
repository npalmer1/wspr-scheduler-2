-- phpMyAdmin SQL Dump
-- version 5.2.1
-- https://www.phpmyadmin.net/
--
-- Host: 127.0.0.1
-- Generation Time: Apr 06, 2026 at 04:02 PM
-- Server version: 10.4.32-MariaDB
-- PHP Version: 8.2.12

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `wspr`
--
CREATE DATABASE IF NOT EXISTS `wspr` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE `wspr`;

-- --------------------------------------------------------

--
-- Table structure for table `antennas`
--

CREATE TABLE `antennas` (
  `AntNo` int(11) NOT NULL,
  `Antenna` varchar(50) NOT NULL,
  `Description` varchar(250) NOT NULL,
  `Switch` int(11) NOT NULL,
  `Tuner` int(11) NOT NULL,
  `Rotator` tinyint(1) NOT NULL,
  `SwitchPort` int(11) NOT NULL,
  `TunerPort` int(11) NOT NULL,
  `Azimuth` int(11) NOT NULL,
  `Switch2` int(11) NOT NULL DEFAULT 0,
  `SwitchPort2` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `antennas`
--

INSERT INTO `antennas` (`AntNo`, `Antenna`, `Description`, `Switch`, `Tuner`, `Rotator`, `SwitchPort`, `TunerPort`, `Azimuth`, `Switch2`, `SwitchPort2`) VALUES
(0, 'Dipole 80-10m', 'Dipole for 80 to 10m', 0, 0, 0, 0, 0, 0, 0, 0),
(1, 'Cobwebb 20-6m', 'Cobwebb antenna 20-6m', 0, 0, 0, 0, 0, 0, 0, 0);

-- --------------------------------------------------------

--
-- Table structure for table `filters`
--

CREATE TABLE `filters` (
  `Id` int(11) NOT NULL,
  `Name` varchar(50) NOT NULL,
  `Protocol` varchar(50) NOT NULL,
  `Port` varchar(50) NOT NULL,
  `IP` varchar(50) NOT NULL,
  `Baud` varchar(50) NOT NULL,
  `Serial` varchar(50) NOT NULL,
  `Channels` int(11) NOT NULL,
  `ChannelType` varchar(50) NOT NULL,
  `Type` varchar(50) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `frequencies`
--

CREATE TABLE `frequencies` (
  `Frequency` double NOT NULL,
  `AudioLevel` double NOT NULL,
  `RXAudioLevel` double NOT NULL,
  `Region` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `rigctl`
--

CREATE TABLE `rigctl` (
  `RigctlID` smallint(3) NOT NULL,
  `Radio` varchar(500) NOT NULL,
  `COMport` varchar(50) NOT NULL,
  `Baud` varchar(50) NOT NULL,
  `IPv4` varchar(50) NOT NULL,
  `Port` varchar(50) NOT NULL,
  `norigctld` tinyint(1) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `rigctl`
--

INSERT INTO `rigctl` (`RigctlID`, `Radio`, `COMport`, `Baud`, `IPv4`, `Port`, `norigctld`) VALUES
(0, '  1003  Yaesu                  FT-1000D                20240228.0      Stable      RIG_MODEL_FT1000D', 'COM1', '9600', '127.0.0.1', '4532', 0);

-- --------------------------------------------------------

--
-- Table structure for table `rigs`
--

CREATE TABLE `rigs` (
  `id` int(11) NOT NULL,
  `rigname` varchar(500) NOT NULL,
  `type` int(11) NOT NULL,
  `selected` int(11) NOT NULL,
  `TXcommand` varchar(500) NOT NULL,
  `RXcommand` varchar(500) NOT NULL,
  `TXptt` varchar(500) DEFAULT NULL,
  `command1` varchar(500) DEFAULT NULL,
  `command2` varchar(500) DEFAULT NULL,
  `command3` varchar(500) DEFAULT NULL,
  `command4` varchar(500) DEFAULT NULL,
  `reply1` varchar(500) NOT NULL,
  `reply2` varchar(500) NOT NULL,
  `reply3` varchar(500) NOT NULL,
  `reply4` varchar(500) NOT NULL,
  `Protocol` varchar(50) DEFAULT NULL,
  `Port` varchar(50) DEFAULT NULL,
  `IP` varchar(50) DEFAULT NULL,
  `Baud` varchar(50) DEFAULT NULL,
  `Serial` varchar(50) DEFAULT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `switches`
--

CREATE TABLE `switches` (
  `Id` int(11) NOT NULL,
  `Name` varchar(50) NOT NULL,
  `Protocol` varchar(50) NOT NULL,
  `Port` varchar(50) NOT NULL,
  `IP` varchar(50) NOT NULL,
  `Baud` varchar(50) NOT NULL,
  `Serial` varchar(50) NOT NULL,
  `Channels` int(11) NOT NULL,
  `ChannelType` varchar(50) NOT NULL,
  `Type` varchar(50) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `switches`
--

INSERT INTO `switches` (`Id`, `Name`, `Protocol`, `Port`, `IP`, `Baud`, `Serial`, `Channels`, `ChannelType`, `Type`) VALUES
(1, 'switch1', 'TCP', '5000', '192.168.1.2', '', '', 4, '0', 'Arduino (antenna switchable on data pins)'),
(2, 'switch2', 'Serial', 'COM1', '', '9600', '8,none,1,none', 4, '0', 'W-410A (antenna selectable by commands)');

-- --------------------------------------------------------

--
-- Table structure for table `tuners`
--

CREATE TABLE `tuners` (
  `Id` int(11) NOT NULL,
  `Name` varchar(50) NOT NULL,
  `Protocol` varchar(50) NOT NULL,
  `Port` varchar(50) NOT NULL,
  `IP` varchar(50) NOT NULL,
  `Baud` varchar(50) NOT NULL,
  `Serial` varchar(50) NOT NULL,
  `Channels` int(11) NOT NULL,
  `ChannelType` varchar(50) NOT NULL,
  `Type` varchar(50) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `antennas`
--
ALTER TABLE `antennas`
  ADD PRIMARY KEY (`AntNo`);

--
-- Indexes for table `filters`
--
ALTER TABLE `filters`
  ADD PRIMARY KEY (`Id`);

--
-- Indexes for table `frequencies`
--
ALTER TABLE `frequencies`
  ADD PRIMARY KEY (`Frequency`);

--
-- Indexes for table `rigctl`
--
ALTER TABLE `rigctl`
  ADD PRIMARY KEY (`RigctlID`);

--
-- Indexes for table `rigs`
--
ALTER TABLE `rigs`
  ADD PRIMARY KEY (`id`);

--
-- Indexes for table `switches`
--
ALTER TABLE `switches`
  ADD PRIMARY KEY (`Id`);

--
-- Indexes for table `tuners`
--
ALTER TABLE `tuners`
  ADD PRIMARY KEY (`Id`);
--
-- Database: `wspr_configs`
--
CREATE DATABASE IF NOT EXISTS `wspr_configs` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE `wspr_configs`;

-- --------------------------------------------------------

--
-- Table structure for table `rxsettings`
--

CREATE TABLE `rxsettings` (
  `id` int(11) NOT NULL,
  `outputname` varchar(100) NOT NULL,
  `outputdevice` int(11) NOT NULL,
  `inputname` varchar(100) NOT NULL,
  `inputdevice` int(11) NOT NULL,
  `outlevel` int(11) NOT NULL,
  `inlevel` int(11) NOT NULL,
  `wsprdpath` varchar(500) NOT NULL,
  `samedev` tinyint(1) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `rxsettings`
--

INSERT INTO `rxsettings` (`id`, `outputname`, `outputdevice`, `inputname`, `inputdevice`, `outlevel`, `inlevel`, `wsprdpath`, `samedev`) VALUES
(0, '0: Speakers (High Definition Audio', 0, '0: Microphone (3- USB Audio Device', 0, 1, 175, 'C:/WSPR_Sked', 0);

-- --------------------------------------------------------

--
-- Table structure for table `settings`
--

CREATE TABLE `settings` (
  `ConfigID` int(3) NOT NULL,
  `Callsign` varchar(250) NOT NULL,
  `BaseCall` varchar(250) NOT NULL,
  `Offset` int(4) NOT NULL,
  `DefaultF` double NOT NULL,
  `Power` int(10) NOT NULL,
  `PowerW` int(5) NOT NULL,
  `FList` varchar(1024) NOT NULL,
  `Locator` varchar(50) NOT NULL,
  `LocatorLong` tinyint(1) NOT NULL,
  `DefaultAnt` varchar(250) NOT NULL,
  `Alpha` double NOT NULL,
  `DefaultAudio` int(11) NOT NULL,
  `HamlibPath` varchar(500) NOT NULL,
  `MsgType` int(11) NOT NULL,
  `TimeZone` varchar(250) NOT NULL,
  `AllowType2` tinyint(1) NOT NULL,
  `oneMsg` tinyint(1) NOT NULL,
  `WsprmsgPath` varchar(500) NOT NULL,
  `VolumeLevel` int(11) NOT NULL,
  `stopsolar` tinyint(1) NOT NULL,
  `stopRX` tinyint(1) NOT NULL,
  `Riseoff` int(11) NOT NULL,
  `Setoff` int(11) NOT NULL,
  `RXonly` tinyint(1) NOT NULL,
  `SlotDB` varchar(250) NOT NULL,
  `selectedFilter` int(11) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `settings`
--

INSERT INTO `settings` (`ConfigID`, `Callsign`, `BaseCall`, `Offset`, `DefaultF`, `Power`, `PowerW`, `FList`, `Locator`, `LocatorLong`, `DefaultAnt`, `Alpha`, `DefaultAudio`, `HamlibPath`, `MsgType`, `TimeZone`, `AllowType2`, `oneMsg`, `WsprmsgPath`, `VolumeLevel`, `stopsolar`, `stopRX`, `Riseoff`, `Setoff`, `RXonly`, `SlotDB`, `selectedFilter`) VALUES
(0, 'G4GCI', 'G4GCI', 50, 7.0386, 37, 5, '', 'IO90', 0, 'Dipole 80-10m', 0.1, 1, 'C:/WSPR_Sked', 1, 'UTC', 0, 1, '', 0, 0, 0, 0, 0, 0, 'wspr_slots', 0);

--
-- Indexes for dumped tables
--

--
-- Indexes for table `rxsettings`
--
ALTER TABLE `rxsettings`
  ADD PRIMARY KEY (`id`);

--
-- Indexes for table `settings`
--
ALTER TABLE `settings`
  ADD PRIMARY KEY (`ConfigID`);
--
-- Database: `wspr_grey`
--
CREATE DATABASE IF NOT EXISTS `wspr_grey` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE `wspr_grey`;

-- --------------------------------------------------------

--
-- Table structure for table `sunrise_sunset`
--

CREATE TABLE `sunrise_sunset` (
  `locator` varchar(8) NOT NULL,
  `date` varchar(10) NOT NULL,
  `sunrise` varchar(6) NOT NULL,
  `sunset` varchar(6) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `sunrise_sunset`
--

INSERT INTO `sunrise_sunset` (`locator`, `date`, `sunrise`, `sunset`) VALUES
('IO90', '01-01', '08:02', '16:12'),
('IO90', '01-02', '08:02', '16:13'),
('IO90', '01-03', '08:02', '16:14'),
('IO90', '01-04', '08:02', '16:15'),
('IO90', '01-05', '08:01', '16:16'),
('IO90', '01-06', '08:01', '16:18'),
('IO90', '01-07', '08:01', '16:19'),
('IO90', '01-08', '08:00', '16:20'),
('IO90', '01-09', '08:00', '16:21'),
('IO90', '01-10', '07:59', '16:23'),
('IO90', '01-11', '07:59', '16:24'),
('IO90', '01-12', '07:58', '16:25'),
('IO90', '01-13', '07:57', '16:27'),
('IO90', '01-14', '07:57', '16:28'),
('IO90', '01-15', '07:56', '16:30'),
('IO90', '01-16', '07:55', '16:31'),
('IO90', '01-17', '07:54', '16:33'),
('IO90', '01-18', '07:53', '16:34'),
('IO90', '01-19', '07:52', '16:36'),
('IO90', '01-20', '07:51', '16:38'),
('IO90', '01-21', '07:50', '16:39'),
('IO90', '01-22', '07:49', '16:41'),
('IO90', '01-23', '07:48', '16:42'),
('IO90', '01-24', '07:47', '16:44'),
('IO90', '01-25', '07:46', '16:46'),
('IO90', '01-26', '07:45', '16:47'),
('IO90', '01-27', '07:43', '16:49'),
('IO90', '01-28', '07:42', '16:51'),
('IO90', '01-29', '07:41', '16:53'),
('IO90', '01-30', '07:39', '16:54'),
('IO90', '01-31', '07:38', '16:56'),
('IO90', '02-01', '07:36', '16:58'),
('IO90', '02-02', '07:35', '16:59'),
('IO90', '02-03', '07:33', '17:01'),
('IO90', '02-04', '07:32', '17:03'),
('IO90', '02-05', '07:30', '17:05'),
('IO90', '02-06', '07:29', '17:06'),
('IO90', '02-07', '07:27', '17:08'),
('IO90', '02-08', '07:25', '17:10'),
('IO90', '02-09', '07:24', '17:12'),
('IO90', '02-10', '07:22', '17:13'),
('IO90', '02-11', '07:20', '17:15'),
('IO90', '02-12', '07:18', '17:17'),
('IO90', '02-13', '07:17', '17:19'),
('IO90', '02-14', '07:15', '17:20'),
('IO90', '02-15', '07:13', '17:22'),
('IO90', '02-16', '07:11', '17:24'),
('IO90', '02-17', '07:09', '17:26'),
('IO90', '02-18', '07:07', '17:27'),
('IO90', '02-19', '07:05', '17:29'),
('IO90', '02-20', '07:04', '17:31'),
('IO90', '02-21', '07:02', '17:33'),
('IO90', '02-22', '07:00', '17:34'),
('IO90', '02-23', '06:58', '17:36'),
('IO90', '02-24', '06:56', '17:38'),
('IO90', '02-25', '06:54', '17:39'),
('IO90', '02-26', '06:52', '17:41'),
('IO90', '02-27', '06:49', '17:43'),
('IO90', '02-28', '06:47', '17:45'),
('IO90', '03-01', '06:45', '17:46'),
('IO90', '03-02', '06:43', '17:48'),
('IO90', '03-03', '06:41', '17:50'),
('IO90', '03-04', '06:39', '17:51'),
('IO90', '03-05', '06:37', '17:53'),
('IO90', '03-06', '06:35', '17:55'),
('IO90', '03-07', '06:33', '17:56'),
('IO90', '03-08', '06:31', '17:58'),
('IO90', '03-09', '06:28', '18:00'),
('IO90', '03-10', '06:26', '18:01'),
('IO90', '03-11', '06:24', '18:03'),
('IO90', '03-12', '06:22', '18:05'),
('IO90', '03-13', '06:20', '18:06'),
('IO90', '03-14', '06:17', '18:08'),
('IO90', '03-15', '06:15', '18:10'),
('IO90', '03-16', '06:13', '18:11'),
('IO90', '03-17', '06:11', '18:13'),
('IO90', '03-18', '06:09', '18:14'),
('IO90', '03-19', '06:06', '18:16'),
('IO90', '03-20', '06:04', '18:18'),
('IO90', '03-21', '06:02', '18:19'),
('IO90', '03-22', '06:00', '18:21'),
('IO90', '03-23', '05:58', '18:23'),
('IO90', '03-24', '05:55', '18:24'),
('IO90', '03-25', '05:53', '18:26'),
('IO90', '03-26', '05:51', '18:27'),
('IO90', '03-27', '05:49', '18:29'),
('IO90', '03-28', '05:47', '18:31'),
('IO90', '03-29', '06:44', '19:32'),
('IO90', '03-30', '06:42', '19:34'),
('IO90', '03-31', '06:40', '19:35'),
('IO90', '04-01', '06:38', '19:37'),
('IO90', '04-02', '06:35', '19:39'),
('IO90', '04-03', '06:33', '19:40'),
('IO90', '04-04', '06:31', '19:42'),
('IO90', '04-05', '06:29', '19:43'),
('IO90', '04-06', '06:27', '19:45'),
('IO90', '04-07', '06:25', '19:47'),
('IO90', '04-08', '06:22', '19:48'),
('IO90', '04-09', '06:20', '19:50'),
('IO90', '04-10', '06:18', '19:51'),
('IO90', '04-11', '06:16', '19:53'),
('IO90', '04-12', '06:14', '19:55'),
('IO90', '04-13', '06:12', '19:56'),
('IO90', '04-14', '06:10', '19:58'),
('IO90', '04-15', '06:08', '20:00'),
('IO90', '04-16', '06:05', '20:01'),
('IO90', '04-17', '06:03', '20:03'),
('IO90', '04-18', '06:01', '20:04'),
('IO90', '04-19', '05:59', '20:06'),
('IO90', '04-20', '05:57', '20:08'),
('IO90', '04-21', '05:55', '20:09'),
('IO90', '04-22', '05:53', '20:11'),
('IO90', '04-23', '05:51', '20:12'),
('IO90', '04-24', '05:49', '20:14'),
('IO90', '04-25', '05:47', '20:16'),
('IO90', '04-26', '05:45', '20:17'),
('IO90', '04-27', '05:44', '20:19'),
('IO90', '04-28', '05:42', '20:20'),
('IO90', '04-29', '05:40', '20:22'),
('IO90', '04-30', '05:38', '20:23'),
('IO90', '05-01', '05:36', '20:25'),
('IO90', '05-02', '05:34', '20:27'),
('IO90', '05-03', '05:33', '20:28'),
('IO90', '05-04', '05:31', '20:30'),
('IO90', '05-05', '05:29', '20:31'),
('IO90', '05-06', '05:27', '20:33'),
('IO90', '05-07', '05:26', '20:34'),
('IO90', '05-08', '05:24', '20:36'),
('IO90', '05-09', '05:22', '20:37'),
('IO90', '05-10', '05:21', '20:39'),
('IO90', '05-11', '05:19', '20:40'),
('IO90', '05-12', '05:18', '20:42'),
('IO90', '05-13', '05:16', '20:43'),
('IO90', '05-14', '05:15', '20:45'),
('IO90', '05-15', '05:13', '20:46'),
('IO90', '05-16', '05:12', '20:48'),
('IO90', '05-17', '05:11', '20:49'),
('IO90', '05-18', '05:09', '20:51'),
('IO90', '05-19', '05:08', '20:52'),
('IO90', '05-20', '05:07', '20:53'),
('IO90', '05-21', '05:06', '20:55'),
('IO90', '05-22', '05:04', '20:56'),
('IO90', '05-23', '05:03', '20:57'),
('IO90', '05-24', '05:02', '20:59'),
('IO90', '05-25', '05:01', '21:00'),
('IO90', '05-26', '05:00', '21:01'),
('IO90', '05-27', '04:59', '21:02'),
('IO90', '05-28', '04:58', '21:04'),
('IO90', '05-29', '04:57', '21:05'),
('IO90', '05-30', '04:56', '21:06'),
('IO90', '05-31', '04:55', '21:07'),
('IO90', '06-01', '04:55', '21:08'),
('IO90', '06-02', '04:54', '21:09'),
('IO90', '06-03', '04:53', '21:10'),
('IO90', '06-04', '04:53', '21:11'),
('IO90', '06-05', '04:52', '21:12'),
('IO90', '06-06', '04:52', '21:13'),
('IO90', '06-07', '04:51', '21:14'),
('IO90', '06-08', '04:51', '21:15'),
('IO90', '06-09', '04:50', '21:15'),
('IO90', '06-10', '04:50', '21:16'),
('IO90', '06-11', '04:50', '21:17'),
('IO90', '06-12', '04:49', '21:17'),
('IO90', '06-13', '04:49', '21:18'),
('IO90', '06-14', '04:49', '21:19'),
('IO90', '06-15', '04:49', '21:19'),
('IO90', '06-16', '04:49', '21:20'),
('IO90', '06-17', '04:49', '21:20'),
('IO90', '06-18', '04:49', '21:20'),
('IO90', '06-19', '04:49', '21:21'),
('IO90', '06-20', '04:49', '21:21'),
('IO90', '06-21', '04:49', '21:21'),
('IO90', '06-22', '04:50', '21:21'),
('IO90', '06-23', '04:50', '21:21'),
('IO90', '06-24', '04:50', '21:22'),
('IO90', '06-25', '04:51', '21:22'),
('IO90', '06-26', '04:51', '21:22'),
('IO90', '06-27', '04:52', '21:22'),
('IO90', '06-28', '04:52', '21:21'),
('IO90', '06-29', '04:53', '21:21'),
('IO90', '06-30', '04:53', '21:21'),
('IO90', '07-01', '04:54', '21:21'),
('IO90', '07-02', '04:55', '21:20'),
('IO90', '07-03', '04:55', '21:20'),
('IO90', '07-04', '04:56', '21:20'),
('IO90', '07-05', '04:57', '21:19'),
('IO90', '07-06', '04:58', '21:19'),
('IO90', '07-07', '04:59', '21:18'),
('IO90', '07-08', '05:00', '21:18'),
('IO90', '07-09', '05:01', '21:17'),
('IO90', '07-10', '05:02', '21:16'),
('IO90', '07-11', '05:03', '21:15'),
('IO90', '07-12', '05:04', '21:15'),
('IO90', '07-13', '05:05', '21:14'),
('IO90', '07-14', '05:06', '21:13'),
('IO90', '07-15', '05:07', '21:12'),
('IO90', '07-16', '05:08', '21:11'),
('IO90', '07-17', '05:09', '21:10'),
('IO90', '07-18', '05:11', '21:09'),
('IO90', '07-19', '05:12', '21:08'),
('IO90', '07-20', '05:13', '21:07'),
('IO90', '07-21', '05:14', '21:06'),
('IO90', '07-22', '05:16', '21:04'),
('IO90', '07-23', '05:17', '21:03'),
('IO90', '07-24', '05:18', '21:02'),
('IO90', '07-25', '05:20', '21:00'),
('IO90', '07-26', '05:21', '20:59'),
('IO90', '07-27', '05:22', '20:58'),
('IO90', '07-28', '05:24', '20:56'),
('IO90', '07-29', '05:25', '20:55'),
('IO90', '07-30', '05:27', '20:53'),
('IO90', '07-31', '05:28', '20:52'),
('IO90', '08-01', '05:29', '20:50'),
('IO90', '08-02', '05:31', '20:49'),
('IO90', '08-03', '05:32', '20:47'),
('IO90', '08-04', '05:34', '20:45'),
('IO90', '08-05', '05:35', '20:44'),
('IO90', '08-06', '05:37', '20:42'),
('IO90', '08-07', '05:38', '20:40'),
('IO90', '08-08', '05:40', '20:39'),
('IO90', '08-09', '05:41', '20:37'),
('IO90', '08-10', '05:43', '20:35'),
('IO90', '08-11', '05:44', '20:33'),
('IO90', '08-12', '05:46', '20:31'),
('IO90', '08-13', '05:47', '20:29'),
('IO90', '08-14', '05:49', '20:28'),
('IO90', '08-15', '05:50', '20:26'),
('IO90', '08-16', '05:52', '20:24'),
('IO90', '08-17', '05:53', '20:22'),
('IO90', '08-18', '05:55', '20:20'),
('IO90', '08-19', '05:56', '20:18'),
('IO90', '08-20', '05:58', '20:16'),
('IO90', '08-21', '06:00', '20:14'),
('IO90', '08-22', '06:01', '20:12'),
('IO90', '08-23', '06:03', '20:10'),
('IO90', '08-24', '06:04', '20:08'),
('IO90', '08-25', '06:06', '20:06'),
('IO90', '08-26', '06:07', '20:03'),
('IO90', '08-27', '06:09', '20:01'),
('IO90', '08-28', '06:10', '19:59'),
('IO90', '08-29', '06:12', '19:57'),
('IO90', '08-30', '06:13', '19:55'),
('IO90', '08-31', '06:15', '19:53'),
('IO90', '09-01', '06:16', '19:51'),
('IO90', '09-02', '06:18', '19:49'),
('IO90', '09-03', '06:19', '19:46'),
('IO90', '09-04', '06:21', '19:44'),
('IO90', '09-05', '06:22', '19:42'),
('IO90', '09-06', '06:24', '19:40'),
('IO90', '09-07', '06:25', '19:38'),
('IO90', '09-08', '06:27', '19:35'),
('IO90', '09-09', '06:29', '19:33'),
('IO90', '09-10', '06:30', '19:31'),
('IO90', '09-11', '06:32', '19:29'),
('IO90', '09-12', '06:33', '19:26'),
('IO90', '09-13', '06:35', '19:24'),
('IO90', '09-14', '06:36', '19:22'),
('IO90', '09-15', '06:38', '19:20'),
('IO90', '09-16', '06:39', '19:18'),
('IO90', '09-17', '06:41', '19:15'),
('IO90', '09-18', '06:42', '19:13'),
('IO90', '09-19', '06:44', '19:11'),
('IO90', '09-20', '06:45', '19:09'),
('IO90', '09-21', '06:47', '19:06'),
('IO90', '09-22', '06:48', '19:04'),
('IO90', '09-23', '06:50', '19:02'),
('IO90', '09-24', '06:51', '19:00'),
('IO90', '09-25', '06:53', '18:57'),
('IO90', '09-26', '06:55', '18:55'),
('IO90', '09-27', '06:56', '18:53'),
('IO90', '09-28', '06:58', '18:51'),
('IO90', '09-29', '06:59', '18:48'),
('IO90', '09-30', '07:01', '18:46'),
('IO90', '10-01', '07:02', '18:44'),
('IO90', '10-02', '07:04', '18:42'),
('IO90', '10-03', '07:05', '18:40'),
('IO90', '10-04', '07:07', '18:37'),
('IO90', '10-05', '07:09', '18:35'),
('IO90', '10-06', '07:10', '18:33'),
('IO90', '10-07', '07:12', '18:31'),
('IO90', '10-08', '07:13', '18:29'),
('IO90', '10-09', '07:15', '18:27'),
('IO90', '10-10', '07:17', '18:24'),
('IO90', '10-11', '07:18', '18:22'),
('IO90', '10-12', '07:20', '18:20'),
('IO90', '10-13', '07:21', '18:18'),
('IO90', '10-14', '07:23', '18:16'),
('IO90', '10-15', '07:25', '18:14'),
('IO90', '10-16', '07:26', '18:12'),
('IO90', '10-17', '07:28', '18:10'),
('IO90', '10-18', '07:29', '18:08'),
('IO90', '10-19', '07:31', '18:06'),
('IO90', '10-20', '07:33', '18:04'),
('IO90', '10-21', '07:34', '18:02'),
('IO90', '10-22', '07:36', '18:00'),
('IO90', '10-23', '07:38', '17:58'),
('IO90', '10-24', '07:39', '17:56'),
('IO90', '10-25', '06:41', '16:54'),
('IO90', '10-26', '06:43', '16:52'),
('IO90', '10-27', '06:44', '16:50'),
('IO90', '10-28', '06:46', '16:48'),
('IO90', '10-29', '06:48', '16:47'),
('IO90', '10-30', '06:49', '16:45'),
('IO90', '10-31', '06:51', '16:43'),
('IO90', '11-01', '06:53', '16:41'),
('IO90', '11-02', '06:55', '16:40'),
('IO90', '11-03', '06:56', '16:38'),
('IO90', '11-04', '06:58', '16:36'),
('IO90', '11-05', '07:00', '16:35'),
('IO90', '11-06', '07:01', '16:33'),
('IO90', '11-07', '07:03', '16:31'),
('IO90', '11-08', '07:05', '16:30'),
('IO90', '11-09', '07:06', '16:28'),
('IO90', '11-10', '07:08', '16:27'),
('IO90', '11-11', '07:10', '16:25'),
('IO90', '11-12', '07:11', '16:24'),
('IO90', '11-13', '07:13', '16:23'),
('IO90', '11-14', '07:15', '16:21'),
('IO90', '11-15', '07:16', '16:20'),
('IO90', '11-16', '07:18', '16:19'),
('IO90', '11-17', '07:19', '16:17'),
('IO90', '11-18', '07:21', '16:16'),
('IO90', '11-19', '07:23', '16:15'),
('IO90', '11-20', '07:24', '16:14'),
('IO90', '11-21', '07:26', '16:13'),
('IO90', '11-22', '07:27', '16:12'),
('IO90', '11-23', '07:29', '16:11'),
('IO90', '11-24', '07:30', '16:10'),
('IO90', '11-25', '07:32', '16:09'),
('IO90', '11-26', '07:33', '16:08'),
('IO90', '11-27', '07:35', '16:07'),
('IO90', '11-28', '07:36', '16:07'),
('IO90', '11-29', '07:38', '16:06'),
('IO90', '11-30', '07:39', '16:05'),
('IO90', '12-01', '07:40', '16:05'),
('IO90', '12-02', '07:42', '16:04'),
('IO90', '12-03', '07:43', '16:04'),
('IO90', '12-04', '07:44', '16:03'),
('IO90', '12-05', '07:46', '16:03'),
('IO90', '12-06', '07:47', '16:02'),
('IO90', '12-07', '07:48', '16:02'),
('IO90', '12-08', '07:49', '16:02'),
('IO90', '12-09', '07:50', '16:02'),
('IO90', '12-10', '07:51', '16:01'),
('IO90', '12-11', '07:52', '16:01'),
('IO90', '12-12', '07:53', '16:01'),
('IO90', '12-13', '07:54', '16:01'),
('IO90', '12-14', '07:55', '16:01'),
('IO90', '12-15', '07:56', '16:02'),
('IO90', '12-16', '07:56', '16:02'),
('IO90', '12-17', '07:57', '16:02'),
('IO90', '12-18', '07:58', '16:02'),
('IO90', '12-19', '07:59', '16:03'),
('IO90', '12-20', '07:59', '16:03'),
('IO90', '12-21', '08:00', '16:03'),
('IO90', '12-22', '08:00', '16:04'),
('IO90', '12-23', '08:01', '16:05'),
('IO90', '12-24', '08:01', '16:05'),
('IO90', '12-25', '08:01', '16:06'),
('IO90', '12-26', '08:02', '16:06'),
('IO90', '12-27', '08:02', '16:07'),
('IO90', '12-28', '08:02', '16:08'),
('IO90', '12-29', '08:02', '16:09'),
('IO90', '12-30', '08:02', '16:10'),
('IO90', '12-31', '08:02', '16:11');

--
-- Indexes for dumped tables
--

--
-- Indexes for table `sunrise_sunset`
--
ALTER TABLE `sunrise_sunset`
  ADD PRIMARY KEY (`date`);
--
-- Database: `wspr_rpt`
--
CREATE DATABASE IF NOT EXISTS `wspr_rpt` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE `wspr_rpt`;

-- --------------------------------------------------------

--
-- Table structure for table `received`
--

CREATE TABLE `received` (
  `datetime` datetime NOT NULL,
  `band` smallint(11) NOT NULL,
  `tx_sign` varchar(15) NOT NULL,
  `tx_loc` varchar(10) NOT NULL,
  `frequency` double NOT NULL,
  `power` smallint(11) NOT NULL,
  `snr` mediumint(11) NOT NULL,
  `drift` smallint(6) NOT NULL,
  `distance` int(11) NOT NULL,
  `azimuth` smallint(11) NOT NULL,
  `reporter` varchar(20) NOT NULL,
  `reporter_loc` varchar(10) NOT NULL,
  `dt` float NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

-- --------------------------------------------------------

--
-- Table structure for table `rxconfig`
--

CREATE TABLE `rxconfig` (
  `id` int(11) NOT NULL,
  `deep` tinyint(1) NOT NULL,
  `quick` tinyint(1) NOT NULL,
  `osd` int(11) NOT NULL,
  `upload` tinyint(1) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Dumping data for table `rxconfig`
--

INSERT INTO `rxconfig` (`id`, `deep`, `quick`, `osd`, `upload`) VALUES
(0, 0, 0, 0, 0);

--
-- Indexes for dumped tables
--

--
-- Indexes for table `received`
--
ALTER TABLE `received`
  ADD PRIMARY KEY (`datetime`,`tx_sign`,`frequency`),
  ADD KEY `idx_received_datetime` (`datetime`),
  ADD KEY `idx_received_frequency` (`frequency`);

--
-- Indexes for table `rxconfig`
--
ALTER TABLE `rxconfig`
  ADD PRIMARY KEY (`id`);
--
-- Database: `wspr_rx`
--
CREATE DATABASE IF NOT EXISTS `wspr_rx` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE `wspr_rx`;

-- --------------------------------------------------------

--
-- Table structure for table `reported`
--

CREATE TABLE `reported` (
  `id` bigint(11) NOT NULL,
  `time` datetime NOT NULL,
  `band` smallint(9) NOT NULL,
  `rx_sign` varchar(25) NOT NULL,
  `rx_lat` float NOT NULL,
  `rx_lon` float NOT NULL,
  `rx_loc` varchar(8) NOT NULL,
  `tx_sign` varchar(15) NOT NULL,
  `tx_lat` float NOT NULL,
  `tx_lon` float NOT NULL,
  `tx_loc` varchar(8) NOT NULL,
  `distance` mediumint(9) NOT NULL,
  `azimuth` mediumint(9) NOT NULL,
  `rx_azimuth` mediumint(9) NOT NULL,
  `frequency` int(11) NOT NULL,
  `power` smallint(6) NOT NULL,
  `snr` smallint(6) NOT NULL,
  `drift` smallint(6) NOT NULL,
  `version` varchar(20) NOT NULL,
  `code` smallint(6) NOT NULL
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `reported`
--
ALTER TABLE `reported`
  ADD PRIMARY KEY (`id`),
  ADD UNIQUE KEY `id` (`id`),
  ADD KEY `idx_reported_time` (`time`),
  ADD KEY `idx_reported_band` (`band`),
  ADD KEY `idx_reported_time_band` (`time`,`band`),
  ADD KEY `idx_reported_rx_sign` (`rx_sign`),
  ADD KEY `idx_reported_distance` (`distance`),
  ADD KEY `idx_reported_version` (`version`);
--
-- Database: `wspr_slots`
--
CREATE DATABASE IF NOT EXISTS `wspr_slots` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE `wspr_slots`;

-- --------------------------------------------------------

--
-- Table structure for table `slots`
--

CREATE TABLE `slots` (
  `Date` date NOT NULL,
  `Time` time NOT NULL,
  `Frequency` double NOT NULL,
  `Offset` int(11) NOT NULL,
  `Power` int(11) NOT NULL,
  `PowerW` double NOT NULL,
  `Antenna` varchar(250) NOT NULL,
  `Tuner` int(3) NOT NULL,
  `Switch` int(3) NOT NULL,
  `SwitchPort` int(11) NOT NULL,
  `Rotator` varchar(3) NOT NULL,
  `Azimuth` int(11) NOT NULL,
  `Elevation` int(11) NOT NULL,
  `End` date NOT NULL,
  `Active` tinyint(1) NOT NULL,
  `Repeating` tinyint(1) NOT NULL,
  `TimeEnd` time NOT NULL,
  `RptTime` tinyint(1) NOT NULL,
  `Parent` varchar(250) NOT NULL,
  `Audio` int(11) NOT NULL,
  `SlotNo` int(11) NOT NULL,
  `MsgType` int(11) NOT NULL,
  `RptType` tinyint(4) NOT NULL,
  `GreyOffset` int(4) NOT NULL,
  `Switch2` int(11) NOT NULL DEFAULT 0,
  `SwitchPort2` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `slots`
--
ALTER TABLE `slots`
  ADD PRIMARY KEY (`Date`,`Time`),
  ADD KEY `idx_slots_date` (`Date`),
  ADD KEY `idx_slots_datetime` (`Date`,`Time`),
  ADD KEY `idx_slots_parent` (`Parent`);
--
-- Database: `wspr_slots_test`
--
CREATE DATABASE IF NOT EXISTS `wspr_slots_test` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE `wspr_slots_test`;

-- --------------------------------------------------------

--
-- Table structure for table `slots`
--

CREATE TABLE `slots` (
  `date` date NOT NULL,
  `time` time NOT NULL,
  `frequency` double NOT NULL,
  `offset` int(11) NOT NULL,
  `power` int(11) NOT NULL,
  `powerw` double DEFAULT NULL,
  `antenna` varchar(250) NOT NULL,
  `tuner` int(3) NOT NULL,
  `switch` int(3) NOT NULL,
  `switchport` int(11) NOT NULL,
  `rotator` varchar(3) NOT NULL,
  `azimuth` int(11) NOT NULL,
  `elevation` int(11) NOT NULL,
  `end` date DEFAULT NULL,
  `active` tinyint(1) NOT NULL,
  `repeating` tinyint(1) NOT NULL,
  `timeend` time NOT NULL,
  `rpttime` tinyint(1) NOT NULL,
  `parent` varchar(250) NOT NULL,
  `audio` int(11) NOT NULL,
  `slotno` int(11) NOT NULL,
  `msgtype` int(11) NOT NULL,
  `rpttype` tinyint(4) NOT NULL,
  `greyoffset` int(4) NOT NULL,
  `switch2` int(11) NOT NULL DEFAULT 0,
  `switchport2` int(11) NOT NULL DEFAULT 0
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `slots`
--
ALTER TABLE `slots`
  ADD PRIMARY KEY (`date`,`time`);
--
-- Database: `wspr_sol`
--
CREATE DATABASE IF NOT EXISTS `wspr_sol` DEFAULT CHARACTER SET utf8mb4 COLLATE utf8mb4_general_ci;
USE `wspr_sol`;

-- --------------------------------------------------------

--
-- Table structure for table `weather`
--

CREATE TABLE `weather` (
  `datetime` datetime NOT NULL,
  `Ap` int(11) DEFAULT 0,
  `Kp00` float DEFAULT 0,
  `Kp03` float DEFAULT 0,
  `Kp06` float DEFAULT 0,
  `Kp09` float DEFAULT 0,
  `Kp12` float DEFAULT 0,
  `Kp15` float DEFAULT 0,
  `Kp18` float DEFAULT 0,
  `Kp21` float DEFAULT 0,
  `flux` float DEFAULT 0,
  `SSN` int(11) DEFAULT 0,
  `Xray` varchar(10) DEFAULT '',
  `pf00` varchar(25) DEFAULT '',
  `pf03` varchar(25) DEFAULT '',
  `pf06` varchar(25) DEFAULT '',
  `pf09` varchar(25) DEFAULT '',
  `pf12` varchar(25) DEFAULT '',
  `pf15` varchar(25) DEFAULT '',
  `pf18` varchar(25) DEFAULT '',
  `pf21` varchar(25) DEFAULT '',
  `fl00` varchar(25) DEFAULT '',
  `fl03` varchar(25) DEFAULT '',
  `fl06` varchar(25) DEFAULT '',
  `fl09` varchar(25) DEFAULT '',
  `fl12` varchar(25) DEFAULT '',
  `fl15` varchar(25) DEFAULT '',
  `fl18` varchar(25) DEFAULT '',
  `fl21` varchar(25) DEFAULT '',
  `s00` varchar(25) DEFAULT '',
  `s03` varchar(25) DEFAULT '',
  `s06` varchar(25) DEFAULT '',
  `s09` varchar(25) DEFAULT '',
  `s12` varchar(25) DEFAULT '',
  `s15` varchar(25) DEFAULT '',
  `s18` varchar(25) DEFAULT '',
  `s21` varchar(25) DEFAULT ''
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_general_ci;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `weather`
--
ALTER TABLE `weather`
  ADD PRIMARY KEY (`datetime`);
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
