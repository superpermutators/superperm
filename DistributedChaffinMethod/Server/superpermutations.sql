CREATE TABLE `tasks` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
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
  `redundant` char(1) NOT NULL DEFAULT 'N',
  `parent_id` int(10) unsigned NOT NULL DEFAULT '0',
  `parent_pl` int(10) unsigned NOT NULL DEFAULT '0',
  `test` char(1) NOT NULL DEFAULT 'N',
  `branch_bin` varbinary(3000) DEFAULT NULL,
  PRIMARY KEY (`id`) USING BTREE,
  KEY `n_waste` (`n`,`waste`) USING BTREE,
  KEY `tsb` (`test`,`status`,`branch_bin`) USING BTREE,
  KEY `parent_id` (`parent_id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

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
  `parent_id` int(10) unsigned NOT NULL DEFAULT '0',
  `parent_pl` int(10) unsigned NOT NULL DEFAULT '0',
  `test` char(1) NOT NULL DEFAULT 'N',
  PRIMARY KEY (`id`) USING BTREE,
  KEY `n_waste` (`n`,`waste`) USING BTREE
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

CREATE TABLE `num_finished_tasks` (
  `num_finished` int(10) unsigned NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

CREATE TABLE `num_redundant_tasks` (
  `num_redundant` int(10) unsigned NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

CREATE TABLE `total_nodeCount` (
  `nodeCount` bigint(20) unsigned NOT NULL DEFAULT '0'
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

CREATE TABLE `witness_strings` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `ts` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `n` int(10) unsigned NOT NULL,
  `waste` int(10) unsigned NOT NULL,
  `perms` int(10) unsigned NOT NULL,
  `str` varchar(6000) NOT NULL,
  `excl_perms` int(10) unsigned NOT NULL DEFAULT '1000000000',
  `final` char(1) NOT NULL DEFAULT 'N',
  `team` varchar(32) NOT NULL DEFAULT 'anonymous',
  PRIMARY KEY (`id`),
  UNIQUE KEY `n_waste` (`n`,`waste`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

CREATE TABLE `superperms` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `ts` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `n` int(10) unsigned NOT NULL,
  `waste` int(10) unsigned NOT NULL,
  `perms` int(10) unsigned NOT NULL,
  `str` varchar(6000) NOT NULL,
  `IP` varchar(100) DEFAULT NULL,
  `team` varchar(32) NOT NULL DEFAULT 'anonymous',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

CREATE TABLE `workers` (
  `id` int(10) unsigned NOT NULL AUTO_INCREMENT,
  `IP` varchar(100) DEFAULT NULL,
  `instance_num` int(10) unsigned NOT NULL DEFAULT '0',
  `ts` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
  `ts_registered` timestamp NOT NULL DEFAULT CURRENT_TIMESTAMP,
  `checkin_count` int(10) unsigned NOT NULL DEFAULT '0',
  `current_task` int(10) unsigned NOT NULL DEFAULT '0',
  `team` varchar(32) NOT NULL DEFAULT 'anonymous',
  PRIMARY KEY (`id`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;

CREATE TABLE `teams` (
  `team` varchar(32) NOT NULL DEFAULT 'anonymous',
  `tasks_completed` int(10) unsigned NOT NULL DEFAULT '0',
  `crashouts` int(10) unsigned NOT NULL DEFAULT '0',
  `nodeCount` bigint(20) NOT NULL DEFAULT '0',
  `visible` char(1) NOT NULL DEFAULT 'N',
  PRIMARY KEY (`team`)
) ENGINE=InnoDB DEFAULT CHARSET=latin1;
