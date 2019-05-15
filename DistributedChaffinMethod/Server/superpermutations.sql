-- phpMyAdmin SQL Dump
-- version 4.8.3
-- https://www.phpmyadmin.net/
--
-- Host: localhost:3306
-- Generation Time: May 13, 2019 at 03:43 PM
-- Server version: 5.6.41-log
-- PHP Version: 7.2.7

SET SQL_MODE = "NO_AUTO_VALUE_ON_ZERO";
SET AUTOCOMMIT = 0;
START TRANSACTION;
SET time_zone = "+00:00";


/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8mb4 */;

--
-- Database: `grgr2554_superpermutations`
--

-- --------------------------------------------------------

--
-- Table structure for table `finished_tasks`
--

CREATE TABLE `finished_tasks` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `original_task_id` int(10) unsigned NOT NULL,
  `access` int(10) unsigned NOT NULL,
  `ts` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `n` int(10) unsigned NOT NULL,
  `waste` int(10) unsigned NOT NULL,
  `prefix` varchar(6000) NOT NULL,
  `perm_to_exceed` int(10) unsigned NOT NULL,
  `iteration` int(10) unsigned NOT NULL DEFAULT '0',
  `prev_perm_ruled_out` int(10) unsigned NOT NULL DEFAULT '1000000000',
  `perm_ruled_out` int(10) unsigned NOT NULL DEFAULT '1000000000',
  `excl_witness` varchar(6000) DEFAULT NULL,
  `status` char(1) NOT NULL DEFAULT 'U',
  `ts_allocated` timestamp NULL DEFAULT NULL,
  `client_id` int(10) unsigned NOT NULL DEFAULT '0',
  `checkin_count` int(10) unsigned NOT NULL DEFAULT '0',
  `ts_finished` timestamp NULL DEFAULT NULL,
  `team` varchar(32) NOT NULL DEFAULT 'anonymous',
  `nodeCount` bigint(20) NOT NULL DEFAULT '0',
  `redundant` char(1) NOT NULL DEFAULT 'N',
  `parent_id` int(10) unsigned NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `total_nodeCount`
--

CREATE TABLE `total_nodeCount` (
  `id` int(10) UNSIGNED NOT NULL DEFAULT '0',
  `nodeCount` bigint(20) UNSIGNED NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
-- --------------------------------------------------------

--
-- Table structure for table `num_finished_tasks`
--

CREATE TABLE `num_finished_tasks` (
  `id` int(10) UNSIGNED NOT NULL DEFAULT '0',
  `num_finished` int(10) UNSIGNED NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `num_redundant_tasks`
--

CREATE TABLE `num_redundant_tasks` (
  `id` int(10) UNSIGNED NOT NULL DEFAULT '0',
  `num_redundant` int(10) NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `superperms`
--

CREATE TABLE `superperms` (
  `id` int(10) UNSIGNED NOT NULL,
  `ts` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `n` int(10) UNSIGNED NOT NULL,
  `waste` int(10) UNSIGNED NOT NULL,
  `perms` int(10) UNSIGNED NOT NULL,
  `str` varchar(6000) NOT NULL,
  `IP` varchar(100) DEFAULT NULL,
  `team` varchar(32) NOT NULL DEFAULT 'anonymous'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `tasks`
--

CREATE TABLE `tasks` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `access` int(10) unsigned NOT NULL,
  `ts` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `n` int(10) unsigned NOT NULL,
  `waste` int(10) unsigned NOT NULL,
  `prefix` varchar(6000) NOT NULL,
  `branch_order` varchar(6000) DEFAULT NULL,
  `perm_to_exceed` int(10) unsigned NOT NULL,
  `iteration` int(10) unsigned NOT NULL DEFAULT '0',
  `prev_perm_ruled_out` int(10) unsigned NOT NULL DEFAULT '1000000000',
  `perm_ruled_out` int(10) unsigned NOT NULL DEFAULT '1000000000',
  `excl_witness` varchar(6000) DEFAULT NULL,
  `status` char(1) NOT NULL DEFAULT 'U',
  `ts_allocated` timestamp NULL DEFAULT NULL,
  `client_id` int(10) unsigned NOT NULL DEFAULT '0',
  `checkin_count` int(10) unsigned NOT NULL DEFAULT '0',
  `ts_finished` timestamp NULL DEFAULT NULL,
  `team` varchar(32) NOT NULL DEFAULT 'anonymous',
  `redundant` char(1) NOT NULL DEFAULT 'N',
  `parent_id` int(10) unsigned NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `teams`
--

CREATE TABLE `teams` (
  `team` varchar(32) NOT NULL DEFAULT 'anonymous',
  `tasks_completed` int(10) unsigned NOT NULL DEFAULT '0',
  `nodeCount` bigint(20) NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `witness_strings`
--

CREATE TABLE `witness_strings` (
  `id` int(10) UNSIGNED NOT NULL,
  `ts` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `n` int(10) UNSIGNED NOT NULL,
  `waste` int(10) UNSIGNED NOT NULL,
  `perms` int(10) UNSIGNED NOT NULL,
  `str` varchar(6000) NOT NULL,
  `excl_perms` int(10) UNSIGNED NOT NULL DEFAULT '1000000000',
  `final` char(1) NOT NULL DEFAULT 'N',
  `team` varchar(32) NOT NULL DEFAULT 'anonymous'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

-- --------------------------------------------------------

--
-- Table structure for table `workers`
--

CREATE TABLE `workers` (
  `id` int(10) UNSIGNED NOT NULL,
  `IP` varchar(100) DEFAULT NULL,
  `instance_num` int(10) UNSIGNED NOT NULL DEFAULT '0',
  `ts` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `ts_registered` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `checkin_count` int(10) UNSIGNED NOT NULL DEFAULT '0',
  `current_task` int(10) UNSIGNED NOT NULL DEFAULT '0',
  `team` varchar(32) NOT NULL DEFAULT 'anonymous'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

--
-- Indexes for dumped tables
--

--
-- Indexes for table `finished_tasks`
--
ALTER TABLE `finished_tasks`
  ADD PRIMARY KEY (`id`) USING BTREE,
  ADD KEY `n_waste` (`n`,`waste`) USING BTREE,
  ADD KEY `branch_order` (`branch_order`(767));

--
-- Indexes for table `num_finished_tasks`
--
ALTER TABLE `num_finished_tasks`
  ADD PRIMARY KEY (`id`);

--
-- Indexes for table `num_redundant_tasks`
--
ALTER TABLE `num_redundant_tasks`
  ADD PRIMARY KEY (`id`);

--
-- Indexes for table `superperms`
--
ALTER TABLE `superperms`
  ADD PRIMARY KEY (`id`);

--
-- Indexes for table `tasks`
--
ALTER TABLE `tasks`
  ADD PRIMARY KEY (`id`) USING BTREE,
  ADD KEY `n_waste` (`n`,`waste`) USING BTREE,
  ADD KEY `branch_order` (`branch_order`(767));

--
-- Indexes for table `teams`
--
ALTER TABLE `teams`
  ADD PRIMARY KEY (`team`);

--
-- Indexes for table `witness_strings`
--
ALTER TABLE `witness_strings`
  ADD PRIMARY KEY (`id`),
  ADD UNIQUE KEY `n_waste` (`n`,`waste`);

--
-- Indexes for table `workers`
--
ALTER TABLE `workers`
  ADD PRIMARY KEY (`id`);

--
-- AUTO_INCREMENT for dumped tables
--

--
-- AUTO_INCREMENT for table `finished_tasks`
--
ALTER TABLE `finished_tasks`
  MODIFY `id` int(10) UNSIGNED NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `num_finished_tasks`
--
ALTER TABLE `num_finished_tasks`
  MODIFY `id` int(10) UNSIGNED NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `num_redundant_tasks`
--
ALTER TABLE `num_redundant_tasks`
  MODIFY `id` int(10) UNSIGNED NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `superperms`
--
ALTER TABLE `superperms`
  MODIFY `id` int(10) UNSIGNED NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `tasks`
--
ALTER TABLE `tasks`
  MODIFY `id` int(10) UNSIGNED NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `witness_strings`
--
ALTER TABLE `witness_strings`
  MODIFY `id` int(10) UNSIGNED NOT NULL AUTO_INCREMENT;

--
-- AUTO_INCREMENT for table `workers`
--
ALTER TABLE `workers`
  MODIFY `id` int(10) UNSIGNED NOT NULL AUTO_INCREMENT;
COMMIT;

/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
