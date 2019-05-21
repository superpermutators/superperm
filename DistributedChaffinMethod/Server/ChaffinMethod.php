<?php
//	Version 13.0
//	Last updated: 21 May 2019
//	Authors: Greg Egan, Jay Pantone

include '../inc/dbinfo.inc';

//	Directory where scripts have permission to write

define('PHP_FILES', 'phpFiles/');

//	**********************************
//	*** Emergency slowdown lever 1 ***

//	1 in X requests to getTask() will be told there are no tasks
//	If X is 0, the default, EVERY request is honoured (if tasks are available)

define('ONE_IN_X', 0);		//	Default
//define('ONE_IN_X', 2);	//	Extreme slowdown

//	*** Emergency slowdown lever 2 ***

//	Time in seconds between client checking in with the server
//	The default is 3 minutes, i.e. 180 seconds

define('CLIENT_CHECKIN', 180);		//	Default
//define('CLIENT_CHECKIN', 300);	//	Extreme slowdown

//	*** Emergency slowdown lever 3 ***

//	Time in seconds clients should spend exploring subtrees
//	Normally the client uses a default value of 2 minutes, which the server shrinks to 30 seconds when many clients are idle
//	If the value here is 0, the default behaviour will continue
//	If the value here is positive, it is sent to the clients (whether many clients are idle or not), overriding both
//	the client default and the server behaviour for idle times.

define('MAX_TIME_IN_SUBTREE', 0);		//	Default (not actual time, just means usual values)
//define('MAX_TIME_IN_SUBTREE', 300);	//	Extreme slowdown

//	**********************************

//	Main or fork repo to send people to for upgrade

//define('CODE_REPO','https://github.com/superpermutators/superperm/blob/master/DistributedChaffinMethod/DistributedChaffinMethod.c');
define('CODE_REPO','https://github.com/nagegerg/superperm/blob/master/DistributedChaffinMethod/DistributedChaffinMethod.c [Note this is still just a test fork of the main project.]');

//	Version of the client ABSOLUTELY required.
//  Note that if this is changed while clients are running tasks,
//	those tasks will be disrupted and will need to be cancelled and reallocated
//	with a call to cancelStalledTasks, and the disrupted clients will need to be unregistered
//	with a call to cancelStalledClients.

$versionAbsolutelyRequired = 13;

//	Version of the client required for new registrations and new tasks to be allocated.
//	If this is changed while clients are running tasks, the task will continue uninterrupted;
//	the client will be unregistered and will exit cleanly the next time it asks for a new task.

$versionForNewTasks = 13;

//	Maximum number of clients to register

$maxClients = 5000;

//	Maximum number of times to retry a transaction

$maxRetries = 10;

//	Valid range for $n

$min_n = 3;
$max_n = 7;

//	Range for access codes

$A_LO = 100000000;
$A_HI = 999999999;

//	Debugging

$stage=0;

//	Currently not using instanceCount

/*
//	Record number of instances of this script

function instanceCount($inc,$def,&$didUpdate) {
	$ii=0;
	$fname = "InstanceCount.txt";
	$fp = fopen($fname, "r+");
	if ($fp===FALSE) {
		$fp = fopen($fname, "w");
		if (!($fp===FALSE)) {
			fwrite($fp, $def<10?"0$def":"$def");
			fclose($fp);
		}
	} else {
		$ii=1;
		if (flock($fp, LOCK_EX)) {
			$ic = fgets($fp);
			$ii = intval($ic);
			$iq = $ii+$inc;
			fseek($fp,0,SEEK_SET);
			fwrite($fp, $iq<10?"0$iq":"$iq");
			fflush($fp);
			flock($fp, LOCK_UN);
			$didUpdate=TRUE;
		} else {
			$didUpdate=FALSE;
		}

		fclose($fp);
	}
	return $ii; 
}
*/

function logError($a,$b)
{
$rtime = date('Y-m-d H:i:s').' UTC ';
$fp = fopen(PHP_FILES . 'ChaffinMethodErrors.txt','a');
fwrite($fp,$rtime . $a ."\n");
fwrite($fp,$rtime . $b ."\n\n");
fclose($fp);
}

function handlePDOError0($f, $e) {
	sleep(1);
	logError("PDO ERROR",$f.($e->getMessage()));
}

function handlePDOError($e) {
	print("Error: " . $e->getMessage());
	logError("PDO ERROR", $e->getMessage());
}

function factorial($n) {
	if ($n==1) return 1;
	return $n*factorial($n-1);
}


//	Function to check that a string contains only the characters 0-9 and .

function checkString($str) {
return (ctype_digit(str_replace('.','',$str)));
}

//	Function to check that a string contains only alphanumeric characters and spaces

function checkString2($str) {
return (ctype_alnum(str_replace(' ','',$str)));
}

//	Function to pad odd-length branch string on the right with zero

function bPad($b) {
if (strlen($b) % 2 == 0) return $b;
return ($b . "0");
}

//	Function to check (what should be) a digit string to see if it is valid, and count the number of distinct permutations it visits.

function analyseString($str, $n) {
	if (is_string($str)) {
		$slen = strlen($str);
		if ($slen > 0) {
			$stri = array_map('intval',str_split($str));
			$strmin = min($stri);
			$strmax = max($stri);
			if ($strmin == 1 && $strmax == $n) {
				$perms = array();
				
				//	Loop over all length-n substrings
				for ($i=0; $i <= $slen - $n; $i++) {
					$s = array_slice($stri,$i,$n);
					$u = array_unique($s);
					if (count($u) == $n) {
						$pval = 0;
						$factor = 1;
						for ($j=0; $j<$n; $j++) {
							$pval += $factor * $s[$j];
							$factor *= 10;
						}
						if (!in_array($pval, $perms)) {
							$perms[] = $pval;
						}
					}
				}
				return count($perms);
			}
		}
	}
	return -1;
}

//	Function to check if a supplied string visits more permutations than any string with the same (n,w) in the database;
//	if it does, the database is updated.  In any case, function returns either the [old or new] (n,w,p) for maximum p, or an error message.
//
//	If called with $p=-1, simply returns the current (n,w,p) for maximum p, or an error message, ignoring $str.
//
//	If $pro > 0, it describes a permutation count ruled out for this number of wasted characters.
//
//	If $p = n!, a copy of the string is stored in the separate "superperms" database

function maybeUpdateWitnessStrings($n, $w, $p, $str, $pro, $teamName) {
	global $pdo, $maxRetries;

	if ($pro > 0 && $pro <= $p) return "Error: Trying to set permutations ruled out to $pro while exhibiting a string with $p permutations\n";

	$isSuper = ($p==factorial($n));
	
	//	Transaction #1: Table 'superperms'
	
	if ($isSuper) {
		$ip = $_SERVER['REMOTE_ADDR'];
		
		for ($r=1;$r<=$maxRetries;$r++) {
			try {			
				$pdo->beginTransaction();
				$res = $pdo->prepare("INSERT INTO superperms (n,waste,perms,str,IP,team) VALUES(?, ?, ?, ?, ?, ?)");
				$res->execute([$n, $w, $p, $str, $ip, $teamName]);
				$pdo->commit();
				break;
			} catch (Exception $e) {
				$pdo->rollback();
				if ($r==$maxRetries) handlePDOError($e);
				else handlePDOError0("[retry $r of $maxRetries in maybeUpdateWitnessStrings() / superperms] ", $e);
			}
		}
	};
	
	//	Transaction #2: Table 'witness_strings'
	
	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pexcl = 1000000000;
			$haveData = FALSE;
		
			$pdo->beginTransaction();
			$res = $pdo->prepare("SELECT perms, excl_perms FROM witness_strings WHERE n=? AND waste=?" . ($p>=0 ? " FOR UPDATE" : ""));
			$res->execute([$n, $w]);

			if (!($row = $res->fetch(PDO::FETCH_NUM))) {
			
				//	No data at all for this (n,w) pair
				
				if ($p >= 0) {
					
					//	Try to update the database
					
					if ($pro > 0) {
						$final = ($pro == $p+1) ? "Y" : "N";

						$res = $pdo->prepare("INSERT INTO witness_strings (n,waste,perms,str,excl_perms,final,team) VALUES(?, ?, ?, ?, ?, ?, ?)");
						$res->execute([$n, $w, $p, $str, $pro, $final, $teamName]);
						$result = "($n, $w, $p)\n";
						$pexcl = $pro;
					} else {
						$res = $pdo->prepare("INSERT INTO witness_strings (n,waste,perms,str,team) VALUES(?, ?, ?, ?, ?)");
						$res->execute([$n, $w, $p, $str, $teamName]);
						$result = "($n, $w, $p)\n";
					}
					
					$haveData = TRUE;

				} else {
					//	If we are just querying, return -1 in lieu of maximum
					$result = "($n, $w, -1)\n";
				}
			} else {
				//	There is existing data for this (n,w) pair, so check to see if we have a greater permutation count
				
				$haveData = TRUE;
				$p0 = intval($row[0]);
				$pexcl = intval($row[1]);
				
				if ($p > $p0) {
				
					//	Our new data has a greater permutation count, so update the entry
				
					if ($pro > 0 && $pro < $pexcl) {
						$final = ($pro == $p+1) ? "Y" : "N";
						$res = $pdo->prepare("REPLACE INTO witness_strings (n,waste,perms,str,excl_perms,final,team) VALUES(?, ?, ?, ?, ?, ?, ?)");
						$res->execute([$n, $w, $p, $str, $pro, $final, $teamName]);
						$result = "($n, $w, $p)\n";
						$pexcl = $pro;
					} else {
						$res = $pdo->prepare("REPLACE INTO witness_strings (n,waste,perms,str,team) VALUES(?, ?, ?, ?, ?)");
						$res->execute([$n, $w, $p, $str, $teamName]);
						$result = "($n, $w, $p)\n";
					}
				} else {
					//	Our new data does not have a greater permutation count (or might just be a query with $p=-1), so return existing maximum
					
					$result = "($n, $w, $p0)\n";
				}
			}
			
		//	If there is a finalised value for one less waste, ensure that the value ruled out in our current waste reflects that
		
			if ($haveData) {
				$res = $pdo->prepare("SELECT perms FROM witness_strings WHERE n=? AND waste=? AND final='Y'");
				$res->execute([$n, $w-1]);
				if ($row = $res->fetch(PDO::FETCH_NUM)) {
					$pprev = intval($row[0]);
					$pexclFromPrev = $pprev + $n+1;
					if ($pexclFromPrev < $pexcl) {
						$final = ($pexclFromPrev == $p+1) ? "Y" : "N";
						$res = $pdo->prepare("UPDATE witness_strings SET excl_perms = ?, final = ? WHERE n=? AND waste=?");
						$res->execute([$pexclFromPrev,$final,$n,$w]);
					}
				}
			}

			$pdo->commit();
			return $result;
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in maybeUpdateWitnessStrings() / witness_strings] ", $e);
		}
	}
}

//	Function to make a new task record
//
//	Returns "Task id: ... " or "Error: ... "

function makeTask($n, $w, $pte, $str, $stressTest) {
	global $A_LO, $A_HI, $pdo, $maxRetries;
	
	//	Transaction #1: Table 'tasks'
	
	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();

			$res = $pdo->prepare("SELECT id FROM tasks WHERE n=? AND waste=? AND prefix=? AND perm_to_exceed=?");
			$res->execute([$n, $w, $str, $pte]);
				
			if ($row = $res->fetch(PDO::FETCH_NUM)) {
				$id = $row[0];
				$result = "Task id: $id already existed with those properties\n";
			} else {
				$access = mt_rand($A_LO,$A_HI);
				$br = substr("000000000",0,$n);

				$res = $pdo->prepare("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,branch_bin,test) VALUES(?, ?, ?, ?, ?, UNHEX(?), ?)");
				$res->execute([$access, $n, $w, $str, $pte, bPad($br), $stressTest]);

				$result = "Task id: " . $pdo->lastInsertId() . "\n";
			}
			
			$pdo->commit();
			return $result;
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in makeTask() / tasks] ", $e);
		}
	}
}

//	Function to relinquish a task, affecting no other databases.
//	Returns -1 if can't locate task, or client_id of task if successful.
//	If we think we know client_id and/or access already, we make operation provisional on that matching;
//	otherwise arguments supplied should be -1.

function relTask($id, $cid0, $access0) {

	global $A_LO, $A_HI, $pdo, $maxRetries;
	$cid = -1;
	
	//	Transaction #1: 'tasks'
	
	for ($r=1;$r<=$maxRetries;$r++) {
		$cid = -1;
		try {
			$pdo->beginTransaction();

			$res = $pdo->prepare("SELECT id, client_id, access FROM tasks WHERE status='A' AND id=? FOR UPDATE");
			$res->execute([$id]);
			
			if (($row = $res->fetch(PDO::FETCH_NUM))
				&& $id==$row[0]
				&& (($access0 < 0) || $access0==$row[2])
				&& (($cid0 < 0) || $cid0==$row[1])) {
				$cid = intval($row[1]);
				$access2 = mt_rand($A_LO,$A_HI);

				$res2 = $pdo->prepare("UPDATE tasks SET status='U', access=?, client_id=0  WHERE id=?");
				$res2->execute([$access2, $id]);
				
				//	Delete any pending children of the deassigned task
				
				$res3 = $pdo->prepare("DELETE FROM tasks WHERE parent_id=? AND status='P'");
				$res3->execute([$id]);
			} else {
				$cid = -1;
			}
			
			$pdo->commit();
			break;
			
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in relTask() / tasks] ", $e);
		}
	}

return $cid;
}

//	Function to allocate an unallocated task, if there is one.
//
//	Returns:	"Task id: ... " / "Access code:" /"n: ..." / "w: ..." / "str: ... " / "pte: ... " / "pro: ... " / "branchOrder: ..."
//	then all finalised (w,p) pairs
//	or:			"No tasks"
//	or:			"Error ... "

function getTask($cid,$ip,$pi,$version,$teamName,$stressTest) {
	global $pdo, $maxRetries;
	
	//	Transaction #1: Table 'tasks'
	//	Get an unassigned task, gather some data about it, and mark it as assigned to this client
	
	$id = 0;
	$result = "No tasks\n";
	if (CLIENT_CHECKIN) {
		$result = "timeBetweenServerCheckins: ".(CLIENT_CHECKIN)."\n" . $result;
	}
	
	//	Maybe reject a proportion of task requests, if administrator has set ONE_IN_X to a non-zero value
	
	if (ONE_IN_X) {
		$rnum = mt_rand(1,ONE_IN_X);
		if ($rnum == 1) return $result;
	}
	
	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();
			
			$res = $pdo->query("SELECT id,access,n,waste,prefix,perm_to_exceed,prev_perm_ruled_out,HEX(branch_bin),client_id FROM tasks WHERE status='U' AND test='$stressTest' AND branch_bin = (SELECT MIN(branch_bin) FROM tasks WHERE status='U' AND test='$stressTest' FOR UPDATE) FOR UPDATE");
			if ($res && ($row = $res->fetch(PDO::FETCH_NUM))) {
				$id = $row[0];
				$access = $row[1];
				$n = intval($row[2]);
				$w = $row[3];
				$str = $row[4];
				$pte = intval($row[5]);
				$ppro = $row[6];
				$br = substr($row[7],0,strlen($str));
				
				if (intval($row[8])!=0) {
				logError("Task already had client", 
					"Unassigned task $id in getTask() was already assigned to client ".$row['client_id']. ", and is now being assigned to client $cid ");
				}
				
				$res = $pdo->prepare("UPDATE tasks SET status='A', ts_allocated=NOW(), client_id=?, team=? WHERE id=?");
				$res->execute([$cid, $teamName, $id]);
				
			} else {

				//	Verify the absence of unassigned tasks
				
				$noTasks=FALSE;
				$ntasks=-1;
				$res0 = $pdo->query("SELECT COUNT(id) FROM tasks WHERE status='U' AND test='$stressTest'");
				if ($res0 && ($row = $res0->fetch(PDO::FETCH_NUM))) {
					if (is_string($row[0])) {
						$ntasks = intval($row[0]);
						if ($ntasks == 0) {
							$noTasks=TRUE;
						}
					}
				}
				if (!$noTasks) {
					if ($r>5) logError("SELECT MIN failed to find tasks", "SELECT MIN in getTask() failed to find $ntasks tasks");
					else {
						$pdo->rollback();
						continue;
					}
				}
			};
		$pdo->commit();
		break;
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in getTask() / tasks] ", $e);
		}
	}
	
	//	Non-transaction (read only, currency non-critical): many idle clients, i.e. 80%+?
	
	$manyIdle = FALSE;
	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$res = $pdo->query("SELECT current_task!=0, COUNT(id) FROM workers GROUP BY current_task!=0");
			if ($res) {
				$idleBusy=[0,0];
				while ($row = $res->fetch(PDO::FETCH_NUM)) {
					$idleBusy[intval($row[0])]=intval($row[1]);
				}
			$total = $idleBusy[0]+$idleBusy[1];
			if ($total && $idleBusy[0]/$total > 0.8) $manyIdle=TRUE;
			}
		break;
		} catch (Exception $e) {
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in getTask() / idle clients count] ", $e);
		}
	}
	
	//	Transaction #2: Table 'witness_strings'
	//	Ensure that pte that goes to client is at least as high as any perm in witness_strings;
	//	also get any (waste,perm) pairs needed for the client
	
	if ($id > 0) {
		for ($r=1;$r<=$maxRetries;$r++) {
			try {
				$pdo->beginTransaction();
				$res = $pdo->prepare("SELECT perms FROM witness_strings WHERE n=? AND waste=?");
				$res->execute([$n, $w]);

				if ($row = $res->fetch(PDO::FETCH_NUM)) {
					$p0 = intval($row[0]);
				} else {
					$p0 = -1;
				}

				if ($p0 > $pte) {
					$pte = $p0;
				}

				$w0 = 0;
				if ($version >= 8 && $n == 6) {
					$w0 = 115;
				}
				
				$res = $pdo->prepare("SELECT waste, perms FROM witness_strings WHERE n=? AND waste > ? AND final='Y' ORDER BY waste ASC");
				$res->execute([$n, $w0]);

				$result = "Task id: $id\nAccess code: $access\nn: $n\nw: $w\nstr: $str\npte: $pte\npro: $ppro\nbranchOrder: $br\n";
				
				//	If a large fraction of clients are idle, split early and spend less time in trees
				
				if ($manyIdle && (!MAX_TIME_IN_SUBTREE)) {
					$result = $result . "timeBeforeSplit: 300\nmaxTimeInSubtree: 30\n";
				}
				
				if (MAX_TIME_IN_SUBTREE) {
					$result = $result . "maxTimeInSubtree: ".(MAX_TIME_IN_SUBTREE)."\n";
				}
				
				if (CLIENT_CHECKIN) {
					$result = $result . "timeBetweenServerCheckins: ".(CLIENT_CHECKIN)."\n";
				}
				
				while ($row = $res->fetch(PDO::FETCH_NUM)) {
					$result = $result . "(" . $row[0] . "," . $row[1] . ")\n";
				}
				
				$pdo->commit();
				break;
			} catch (Exception $e) {
				$pdo->rollback();
				if ($r==$maxRetries) handlePDOError($e);
				else handlePDOError0("[retry $r of $maxRetries in getTask() / witness_strings] ", $e);
			}
		}
	}
	
	//	Transaction #3: Table 'workers'
	//	Bump the checkin_count for this worker, as proof they're still alive, and link the worker to this task

	$ctsk=0;
	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();
			$res = $pdo->prepare("SELECT current_task FROM workers WHERE id=? AND instance_num=? AND IP=? FOR UPDATE");
			$res->execute([$cid, $pi, $ip]);

			if (!($row = $res->fetch(PDO::FETCH_NUM))) {
				$pdo->commit();
				return "Error: No client found with those details\n";
			} else {
				$ctsk = intval($row[0]);
				if ($ctsk>0) {
				logError("Client already had task", 
					"Client $cid in getTask() was already assigned the task $ctsk, is now being given $id");
				}				
				$res = $pdo->prepare("UPDATE workers SET checkin_count=checkin_count+1, current_task=? WHERE id=?");
				$res->execute([$id,$cid]);
				$pdo->commit();
				break;
			};
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in getTask() / workers] ", $e);
		}
	}
	
	//	Relinquish any orphaned task from this client
	
	if ($ctsk>0) relTask($ctsk, $cid, -1);

return $result;
}

//	Function for a client to abandon a task

function relinquishTask($id, $access, $cid) {

	global $A_LO, $A_HI, $pdo, $maxRetries;
	$result = "Error: Unable to locate task to abandon\n";
	
	//	Transaction #1: 'tasks'
	
	if (relTask($id,$cid,$access) > 0) {
	
	//	Transaction #2: 'workers'

		for ($r=1;$r<=$maxRetries;$r++) {
			try {
				$pdo->beginTransaction();
			
				$res = $pdo->prepare("UPDATE workers SET current_task=0 WHERE id=? AND current_task=?");
				$res->execute([$cid,$id]);
				
				$pdo->commit();
				break;
				
			} catch (Exception $e) {
				$pdo->rollback();
				if ($r==$maxRetries) handlePDOError($e);
				else handlePDOError0("[retry $r of $maxRetries in relinquishTask() / workers] ", $e);
			}
		}
	$result = "Relinquished task\n";
	}
	
return $result;
}

//	Function to cancel stalled tasks

function cancelStalledTasks($maxMin) {

	global $A_LO, $A_HI, $pdo, $maxRetries;
	
	//	Transaction #1: 'tasks'
	//	Any task marked active that has remained inactive for too long is marked unassigned;
	//	we gather up the relevant client IDs to modify the worker table accordingly
	
	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();

			$res = $pdo->prepare("SELECT id, TIMESTAMPDIFF(MINUTE,ts,NOW()), client_id, team FROM tasks WHERE status='A' AND TIMESTAMPDIFF(MINUTE,ts,NOW())>? FOR UPDATE");
			$res->execute([$maxMin]);

			$cancelled = 0;
			$clientsWithStalledTasks = array();
			$teamsWithStalledTasks = array();
			
			while ($row = $res->fetch(PDO::FETCH_NUM)) {

				$stall = intval($row[1]);
				if ($stall > $maxMin) {
					$id = $row[0];
					$cid = $row[2];
					$teamName = $row[3];
					$access = mt_rand($A_LO,$A_HI);

					$res2 = $pdo->prepare("UPDATE tasks SET status='U', access=?, client_id=0 WHERE id=?");
					$res2->execute([$access, $id]);
					
					//	Delete any pending children of the deassigned task
					
					$res3 = $pdo->prepare("DELETE FROM tasks WHERE parent_id=? AND status='P'");
					$res3->execute([$id]);

					$clientsWithStalledTasks[] = [$cid,$id];
					$teamsWithStalledTasks[] = $teamName;
					$cancelled++;
				}
			}
			
			$result = "Cancelled $cancelled tasks\n";			
			$pdo->commit();
			break;
			
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in cancelStalledTasks() / tasks] ", $e);
		}
	}
	
	if ($cancelled > 0) {
	
	//	Transaction #2: 'workers'
	//	Zero the current_task in the workers whose stalled tasks were cancelled
	
		for ($r=1;$r<=$maxRetries;$r++) {
			try {
				$pdo->beginTransaction();

				for ($i=0;$i<$cancelled;$i++) {
					$cid = $clientsWithStalledTasks[$i][0];
					$taskID = $clientsWithStalledTasks[$i][1];
					
					//	Only zero the current_task in the worker record if it matched the deassigned task
					
					$res = $pdo->prepare("UPDATE workers SET current_task=0 WHERE id=? AND current_task=?");
					$res->execute([$cid,$taskID]);
					}
				
				$pdo->commit();
				break;
				
			} catch (Exception $e) {
				$pdo->rollback();
				if ($r==$maxRetries) handlePDOError($e);
				else handlePDOError0("[retry $r of $maxRetries in cancelStalledTasks() / workers] ", $e);
			}
		}
		
	//	Transaction #3: 'teams'
	
		for ($r=1;$r<=$maxRetries;$r++) {
			try {
				$pdo->beginTransaction();

				for ($i=0;$i<count($teamsWithStalledTasks);$i++) {
					$teamName = $teamsWithStalledTasks[$i];

					// Try to increment team task count
					$res = $pdo->prepare("UPDATE teams SET crashouts = crashouts + 1 WHERE team = ?");
					$res->execute([$teamName]);

					// If no rows were affected, we need to add the team to this table
					if ($res->rowCount() == 0) {
						$res = $pdo->prepare("INSERT INTO teams (team, crashouts) values (?, 1)");
						$res->execute([$teamName]);
					}
				}
				
				$pdo->commit();
				break;
				
			} catch (Exception $e) {
				$pdo->rollback();
				if ($r==$maxRetries) handlePDOError($e);
				else handlePDOError0("[retry $r of $maxRetries in cancelStalledTasks() / teams] ", $e);
			}
		}
	}
	
return $result;
}

//	Function to cancel stalled clients

function cancelStalledClients($maxMin)
{
	global $pdo, $maxRetries;
	
	//	Transaction #1: 'workers'
	
	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();
			$res = $pdo->prepare("SELECT id, TIMESTAMPDIFF(MINUTE,ts,NOW()) FROM workers WHERE current_task=0 AND TIMESTAMPDIFF(MINUTE,ts,NOW())>? FOR UPDATE");
			$res->execute([$maxMin]);

			$cancelled = 0;
			
			while ($row = $res->fetch(PDO::FETCH_NUM)) {
				$stall = intval($row[1]);
				
				if ($stall > $maxMin) {
					$id = $row[0];
					
					$res2 = $pdo->prepare("DELETE FROM workers WHERE id=?");
					$res2->execute([$id]);

					$cancelled++;
				}
			}

			$result = "Cancelled $cancelled clients\n";

			$pdo->commit();		
			return $result;
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in cancelStalledClients() / workers] ", $e);
		}
	}
}

//	Function to do further processing if we have finished all tasks for the current (n,w,iter) search
//	This is called by the administrator

function maybeFinishedAllTasks() {
	global $A_LO, $A_HI, $pdo, $maxRetries;
	
	//	Transaction #1: 'tasks'
	//	The 'tasks' table should be completely empty if we have finished all tasks, with all tasks moved
	//	to 'finished_tasks'

	$fin = FALSE;
	$ntasks = 0;

	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->query("SET autocommit=0");
			$pdo->query("LOCK TABLES tasks WRITE");
			$res = $pdo->query("SELECT COUNT(id) FROM tasks");
			
			if ($res && ($row = $res->fetch(PDO::FETCH_NUM))) {
				if (is_string($row[0])) {
					$ntasks = intval($row[0]);
					if ($ntasks == 0) {
						$fin=TRUE;
					}
				}
			}		
			$pdo->query("COMMIT");
			$pdo->query("UNLOCK TABLES");
			break;
			
		} catch (Exception $e) {
			if ($r==$maxRetries) {handlePDOError($e); return;}
			else handlePDOError0("[retry $r of $maxRetries in maybeFinishedAllTasks() / tasks (1)] ", $e);
		}
	}
	
	if (!$fin) return "There are still $ntasks tasks\n";

	//	Transaction #2: 'finished_tasks'

	//	Find the highest value of perm_ruled_out from all tasks for the highest (n,w,iter);
	//	no strings were found with this number of perms or higher, across the whole search.

	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();
			
			$res = $pdo->query("SELECT MAX(n) FROM finished_tasks");
			$row = $res->fetch(PDO::FETCH_NUM);
			if (!$row)
				{
				$pdo->commit();
				return "Error: finished_tasks table is empty\n";
				};
			$n = intval($row[0]);

			$res = $pdo->prepare("SELECT MAX(waste) FROM finished_tasks WHERE n=?");
			$res->execute([$n]);
			$row = $res->fetch(PDO::FETCH_NUM);
			$w = intval($row[0]);

			$res = $pdo->prepare("SELECT MAX(iteration) FROM finished_tasks WHERE n=? AND waste=?");
			$res->execute([$n,$w]);
			$row = $res->fetch(PDO::FETCH_NUM);
			$iter = intval($row[0]);

			$res = $pdo->prepare("SELECT MAX(perm_ruled_out) FROM finished_tasks WHERE n=? AND waste=? AND iteration=? AND status='F'");
			$res->execute([$n, $w, $iter]);
			$row = $res->fetch(PDO::FETCH_NUM);
			$pro = intval($row[0]);		
				
			$pdo->commit();
			break;
	
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) {handlePDOError($e); return;}
			else handlePDOError0("[retry $r of $maxRetries in maybeFinishedAllTasks() / finished_tasks (1)] ", $e);
		}
	}
	
	echo "OK\nFor (n,w,iter)=($n,$w,$iter), MAX(perm_ruled_out)=$pro\n";
	
	//	Transaction #3: 'witness_strings'
	
	//	Was any string found for this search (or maybe for the same (n,w), but by other means)?

	$needTighterBound = TRUE;

	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();
			
			$res = $pdo->prepare("SELECT perms, excl_perms FROM witness_strings WHERE n=? AND waste=? FOR UPDATE");
			$res->execute([$n, $w]);

			if ($row = $res->fetch(PDO::FETCH_NUM)) {
				//	Yes.  Update the excl_perms and final fields.
				
				$p = intval($row[0]);
				$pro0 = intval($row[1]);
				
				if ($pro0 > 0 && $pro0 < $pro) {
					$pro = $pro0;
				}

				if ($pro == $p + 1) {
					$final = 'Y';
					$needTighterBound = FALSE;
					
					//	If we finalised w, this might tighten exclusion on w+1
					 
					$pdo->query("UPDATE witness_strings SET excl_perms=LEAST(excl_perms,$p+$n+1) WHERE n=$n AND waste=".($w+1));
				} else {
					$final = 'N';
					$needTighterBound = TRUE;
				}

				$res = $pdo->prepare("UPDATE witness_strings SET excl_perms=?, final=? WHERE n=? AND waste=?");
				$res->execute([$pro, $final, $n, $w]);
			}

			$pdo->commit();
			break;
	
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) {handlePDOError($e); return;}
			else handlePDOError0("[retry $r of $maxRetries in maybeFinishedAllTasks() / witness_strings] ", $e);
		}
	}

	if (!$needTighterBound) {	
			
		//	Maybe create a new task for a higher w value
		
		$fn = factorial($n);
		if ($p < $fn) {
			$w1 = $w+1;
			$str = substr("123456789",0,$n);
			$br = substr("000000000",0,$n);
			
			//	Step back on increment at high w
			
			$pInc = 2*($n-4);
			if ($n==6 && $w >= 115) $pInc--;
			
			$pte = $p + $pInc;
			if ($pte >= $fn) $pte = $fn-1;
			$pro2 = $p+$n+1;
			$access = mt_rand($A_LO,$A_HI);
			
			//	Transaction #4: 'tasks'
			
			for ($r=1;$r<=$maxRetries;$r++) {
				try {
					$pdo->beginTransaction();
					$res = $pdo->prepare("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,prev_perm_ruled_out,branch_bin) VALUES(?, ?, ?, ?, ?, ?, UNHEX(?))");
					$res->execute([$access, $n, $w1, $str, $pte, $pro2, bPad($br)]);
					$id=$pdo->lastInsertId();
					$pdo->commit();
					return "OK\nTask id: " . $id . " for waste=$w1, perm_to_exceed=$pte, prev_perm_ruled_out=$pro2\n";
				} catch (Exception $e) {
					$pdo->rollback();
					if ($r==$maxRetries) {handlePDOError($e); return;}
					else handlePDOError0("[retry $r of $maxRetries in maybeFinishedAllTasks() / tasks (2)] ", $e);

				}
			}
			
		} else {
			return "OK\nWe reached the superpermutations, no further tasks required!\n";
		}
	} else {
		//	We need to backtrack and search for a lower perm_to_exceed
		
		//	Transaction #5: 'finished_tasks'
		
		for ($r=1;$r<=$maxRetries;$r++) {
			try {
				$pdo->beginTransaction();
				$res = $pdo->prepare("SELECT MIN(perm_to_exceed) FROM finished_tasks WHERE n=? AND waste=? AND iteration=? AND status='F'");
				$res->execute([$n, $w, $iter]);
				$row = $res->fetch(PDO::FETCH_NUM);
				$pte = intval($row[0])-1;
				$pdo->commit();
				break;
			} catch (Exception $e) {
				$pdo->rollback();
				if ($r==$maxRetries) {handlePDOError($e); return;}
				else handlePDOError0("[retry $r of $maxRetries in maybeFinishedAllTasks() / finished_tasks (2)] ", $e);
			}
		}
	
		$str = substr("123456789",0,$n);
		$br = substr("000000000",0,$n);
		$access = mt_rand($A_LO,$A_HI);
		$iter1 = $iter+1;

		//	Transaction #6: 'tasks'
		
		for ($r=1;$r<=$maxRetries;$r++) {
			try {
				$pdo->beginTransaction();
				$res = $pdo->prepare("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,iteration,prev_perm_ruled_out,branch_bin) VALUES(?, ?, ?, ?, ?, ?, ?, UNHEX(?))");
				$res->execute([$access, $n, $w, $str, $pte, $iter1, $pro, bPad($br)]);
				$id=$pdo->lastInsertId();
				$pdo->commit();
				return "OK\nTask id: " . $id . " for waste=$w, perm_to_exceed=$pte, prev_perm_ruled_out=$pro, iteration=$iter1\n";
			} catch (Exception $e) {
				$pdo->rollback();
				if ($r==$maxRetries) {handlePDOError($e); return;}
				else handlePDOError0("[retry $r of $maxRetries in maybeFinishedAllTasks() / tasks (3)] ", $e);
			}
		}
	}
}

//	Function to mark a task as finished
//
//	Returns: "OK ..." or "Error: ... "
	
function finishTask($id, $access, $pro, $str, $teamName, $nodeCount, $stressTest) {
	global $pdo, $maxRetries, $stage;
	
	//	Transaction #1: 'tasks' / 'finished_tasks' / 'num_redundant_tasks'

	$ok=FALSE;
	$cid=0;
	$redun=FALSE;
	$numNew=0;
	
	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$stage=0;
			$pdo->beginTransaction();
			$stage=1;

			$res = $pdo->prepare("SELECT * FROM tasks WHERE id=? AND access=? AND status='A' FOR UPDATE");
			$stage=2;
			$res->execute([$id, $access]);
			$stage=3;

			if ($row = $res->fetch(PDO::FETCH_ASSOC)) {

				//	Check that task is still active
				
				if ($row['status']=='A') {
				
					//	Check that the exclusion string starts with the expected prefix.
					
					$pref = $row['prefix'];
					$pref_len = strlen($pref);
					if (substr($str,0,$pref_len)==$pref) {
						$n_str = $row['n'];
						$n = intval($n_str);
						$w_str = $row['waste'];
						$w = intval($w_str);
						$iter_str = $row['iteration'];
						$iter = intval($iter_str);
						$cid = intval($row['client_id']);
							
						//	Check that the exclusion string is valid
						
						if (analyseString($str,$n) > 0) {
						
							//	Drop prefix	
						
							$str0=substr($str,$pref_len);
							
							//	See if we have found a string that makes other searches redundant
												
							$ppro = intval($row['prev_perm_ruled_out']);

							if ($ppro > 0 && $pro >= $ppro && $pro != factorial($n)+1) {
							
								$res = $pdo->prepare("SELECT * FROM tasks WHERE n=? AND waste=? AND iteration=? AND status='U' FOR UPDATE");
								$stage=4;
								$res->execute([$n, $w, $iter]);
								$stage=5;

								$numNew = 0;

								// Note: you can prepare a statement just once and execute it multiple times!
								
								$res2 = $pdo->prepare("INSERT INTO finished_tasks (original_task_id, access,n,waste,prefix,perm_to_exceed,status,prev_perm_ruled_out,iteration,ts_allocated,ts_finished,excl_witness,checkin_count,perm_ruled_out,client_id,team,redundant,parent_id,parent_pl,test) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NOW(), ?, ?, ?, ?, ?, ?, ?, ?, ?)");
								$stage=6;
								while ($row2 = $res->fetch(PDO::FETCH_ASSOC)) {
									$numNew += 1;
									$pl = intval($row2['parent_pl']);
									$res2->execute([$row2['id'], $row2['access'], $row2['n'], $row2['waste'], substr($row2['prefix'],$pl), $row2['perm_to_exceed'], 'F', $row2['prev_perm_ruled_out'], $row2['iteration'], $row2['ts_allocated'], 'redundant', $row2['checkin_count'], $pro, $row2['client_id'], $teamName,'Y',$row2['parent_id'],$pl,$row2['test']]);
								}
								$redun = TRUE;
								$stage=7;
							}

							$res = $pdo->prepare("INSERT INTO finished_tasks (original_task_id, access,n,waste,prefix,perm_to_exceed,status,prev_perm_ruled_out,iteration,ts_allocated,ts_finished,excl_witness,checkin_count,perm_ruled_out,client_id,team,redundant,parent_id,parent_pl,nodeCount,test) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NOW(), ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
							$pl = intval($row['parent_pl']);
							$stage=8;
							$res->execute([$row['id'], $row['access'], $row['n'], $row['waste'], substr($row['prefix'],$pl), $row['perm_to_exceed'], 'F', $row['prev_perm_ruled_out'], $row['iteration'], $row['ts_allocated'], $str0, $row['checkin_count'], $pro, $row['client_id'], $teamName,$row['redundant'],$row['parent_id'],$pl,$nodeCount,$row['test']]);
							$stage=9;

							$ok=TRUE;

						} else {
							$result = "Error: Invalid string\n";
						}
					} else {
						$result = "Error: String does not start with expected prefix\n";
					}
				} else {
					if ($row['status']=='F') {
						$result = "Cancelled: The task being finalised was already marked finalised, which was unexpected\n";
					} else {
						$result = "Cancelled: The task being finalised was found to have status ".$row['status']. ", which was unexpected\n";
					}
				}
			} else {
				$result = "Cancelled: No match to id=$id, access=$access for the task being finalised. (It may have already been unexpectedly finalised.)\n";
			}
			
			$pdo->commit();
			break;
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in finishTask() / Transaction #1, stage=$stage, task id=$id, client id=$cid] ", $e);
		}
	}
	
	if (!$ok) return $result;
	
	if ($redun) {
	
		//	Transaction #2R: 'tasks'

		for ($r=1;$r<=$maxRetries;$r++) {
			try {
				$pdo->beginTransaction();
				
				//	Delete the redundant tasks, having copied them
				
				$res = $pdo->prepare("DELETE FROM tasks WHERE n=? AND waste=? AND iteration=? AND status='U'");
				$res->execute([$n, $w, $iter]);
				$pdo->commit();
				break;
			} catch (Exception $e) {
				$pdo->rollback();
				if ($r==$maxRetries) handlePDOError($e);
				else handlePDOError0("[retry $r of $maxRetries in finishTask() / tasks DEL-R] ", $e);
			}
		}

		//	Transaction #3R: 'tasks'

		for ($r=1;$r<=$maxRetries;$r++) {
			try {
				$pdo->beginTransaction();
				
				//	Mark assigned tasks as redundant, so splitTask won't split them
				
				$res3 = $pdo->prepare("UPDATE tasks SET redundant='Y' WHERE n=? AND waste=? AND iteration=? AND status='A'");
				$res3->execute([$n, $w, $iter]);
				$pdo->commit();
				break;
			} catch (Exception $e) {
				$pdo->rollback();
				if ($r==$maxRetries) handlePDOError($e);
				else handlePDOError0("[retry $r of $maxRetries in finishTask() / tasks RED] ", $e);
			}
		}
	
		//	Transaction #4R: 'num_redundant_tasks'

		for ($r=1;$r<=$maxRetries;$r++) {
			try {
				$pdo->beginTransaction();
				$update_res = $pdo->prepare("UPDATE num_redundant_tasks SET num_redundant = num_redundant + ?");
				$update_res->execute([$numNew]);
				$pdo->commit();
				break;
			} catch (Exception $e) {
				$pdo->rollback();
				if ($r==$maxRetries) handlePDOError($e);
				else handlePDOError0("[retry $r of $maxRetries in finishTask() / num_redundant_tasks] ", $e);
			}
		}
	}
	
	
	//	Transaction #2: 'tasks'

	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();
			
			//	Delete the finished task, having copied it
			
			$res = $pdo->prepare("DELETE FROM tasks WHERE id=?");
			$res->execute([$id]);
			$pdo->commit();
			break;
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in finishTask() / tasks DEL] ", $e);
		}
	}
	
	//	Transaction #3: 'tasks'

	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();
			
			//	Turn any children split from this task from pending into unassigned
			
			$res = $pdo->prepare("UPDATE tasks SET status = 'U' WHERE parent_id=? AND status='P'");
			$res->execute([$id]);
			$pdo->commit();
			break;
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in finishTask() / tasks PEND] ", $e);
		}
	}
	
	//	Transaction #4: 'num_finished_tasks'

	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();
			$pdo->query("UPDATE num_finished_tasks SET num_finished = num_finished + 1");
			$pdo->commit();
			break;
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in finishTask() / num_finished_tasks] ", $e);
		}
	}
	
	//	Transaction #5: 'total_nodeCount'

	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();
			$pdo->query("UPDATE total_nodeCount SET nodeCount = nodeCount + $nodeCount");
			$pdo->commit();
			break;
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in finishTask() / total_nodeCount] ", $e);
		}
	}
	
	//	Transaction #6: 'teams'

	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();

			// Try to increment team task count
			$res = $pdo->prepare("UPDATE teams SET tasks_completed = tasks_completed + 1, nodeCount = nodeCount + ? WHERE team = ?");
			$res->execute([$nodeCount, $teamName]);

			// If no rows were affected, we need to add the team to this table
			if ($res->rowCount() == 0) {
				$res = $pdo->prepare("INSERT INTO teams (team, tasks_completed, nodeCount) values (?, 1, ?)");
				$res->execute([$teamName, $nodeCount]);
			}
			
			$pdo->commit();
			break;
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in finishTask() / teams] ", $e);
		}
	}
	
	//	Transaction #7: 'workers'

	if ($cid>0) {
		for ($r=1;$r<=$maxRetries;$r++) {
			try {
				$pdo->beginTransaction();
				$res = $pdo->prepare("UPDATE workers SET current_task=0 WHERE id=? AND current_task=?");
				$res->execute([$cid,$id]);
				
				$pdo->commit();
				break;
			} catch (Exception $e) {
				$pdo->rollback();
				if ($r==$maxRetries) handlePDOError($e);
				else handlePDOError0("[retry $r of $maxRetries in finishTask() / workers] ", $e);
			}
		}
	}
	
	if ($n==5 || ($n==6 && $w < 100)) maybeFinishedAllTasks();

	return "OK\n";
}

//	Function to check-in a task
//
//	Returns: "OK" (or "Done" for tasks that have become redundant)
//	or "Error: ... "

function checkIn($id, $access) {
	global $pdo, $maxRetries;
	
	$ok = FALSE;
	$taskDone = FALSE;
	$cid=0;
	
	//	Transaction #1: 'tasks'

	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();

			$res = $pdo->prepare("SELECT * FROM tasks WHERE id=? AND access=? FOR UPDATE");
			$res->execute([$id, $access]);
			
			if ($row = $res->fetch(PDO::FETCH_ASSOC)) {

				//	Check that task is still active
				
				if ($row['status'] == 'A') {
					$cid = intval($row['client_id']);
						
				//	Check that task is not redundant
				
					$taskDone = ($row['redundant']=='Y');
			
					$res = $pdo->prepare("UPDATE tasks SET checkin_count=checkin_count+1 WHERE id=? AND access=?");
					$res->execute([$id, $access]);

					$ok = TRUE;
				} else {
					if ($row['status']=='F') {
						$result = "Cancelled: The task checking in was marked finalised, which was unexpected\n";
					} else {
						$result = "Cancelled: The task checking in was found to have status ".$row['status']. ", which was unexpected\n";
					}
				}
			} else {
				$result = "Cancelled: No match to id=$id, access=$access for the task checking in\n";
			}
				
			$pdo->commit();
			break;

		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in checkIn() / tasks] ", $e);
		}
	}
	
	if (!$ok) return $result;

	//	Transaction #2: 'workers'

	if ($cid>0) {
		for ($r=1;$r<=$maxRetries;$r++) {
			try {

				$pdo->beginTransaction();
				$res = $pdo->prepare("UPDATE workers SET checkin_count=checkin_count+1 WHERE id=?");
				$res->execute([$cid]);
				$pdo->commit();
				break;

			} catch (Exception $e) {
				$pdo->rollback();
				if ($r==$maxRetries) handlePDOError($e);
				else handlePDOError0("[retry $r of $maxRetries in checkIn() / workers] ", $e);
			}
		}
	}
	
	return $taskDone ? "Done\n" : "OK\n";
}

//	Function to create a task split from an existing one, with a specified prefix and branch
//
//	Returns: "OK" (or "Done" for tasks that have become redundant)
//	or "Error: ... "

function splitTask($id, $access, $new_pref, $branchOrder, $stressTest) {
	global $A_LO, $A_HI, $pdo, $maxRetries;
	
	$ok = FALSE;
	$taskDone = FALSE;
	$cid=0;
	
	//	Transaction #1: 'tasks'

	for ($r=1;$r<=$maxRetries;$r++) {
		try {

			$pdo->beginTransaction();

			$res = $pdo->prepare("SELECT * FROM tasks WHERE id=? AND access=? FOR UPDATE");
			$res->execute([$id, $access]);
			
			if ($row = $res->fetch(PDO::FETCH_ASSOC)) {

				//	Check that task is still active
				if ($row['status'] == 'A') {
					$pref = $row['prefix'];
					$pref_len = strlen($pref);
					$n_str = $row['n'];
					$n = intval($n_str);
					$cid = intval($row['client_id']);
						
					//	Check that the new prefix extends the old one

					if (substr($new_pref,0,$pref_len) == $pref) {
						
					//	Check that task is not redundant
				
						if ($row['redundant']!='Y') {

							//	Base the new task on the old one
							//	We do not try to update perms_to_exceed from witness_strings, as getTask() does that.
							
							$new_access = mt_rand($A_LO,$A_HI);
							$fieldList = "";
							$qList = "";
							$valuesList = array();
							$c = 0;
							
							reset($row);
							
							for ($j=0; $j < count($row); $j++) {
								$field = key($row);
								$value = current($row);
								$bb = ($field=='branch_bin');
								
								if ($field=='access') $value = $new_access;
								else if ($field=='prefix') $value = $new_pref; 
								else if ($field=='ts_allocated') $value = 'NOW()';
								else if ($field=='status') $value = 'P';
								else if ($bb) $value = bPad($branchOrder);
								else if ($field=='parent_id') $value = $id;
								else if ($field=='parent_pl') $value = $pref_len;
								
								if ($field != 'id' && $field != 'ts' && $field != 'ts_finished' && $field != 'ts_allocated' && $field != 'checkin_count' && $field != 'client_id') {
									$pre = ($c==0) ? "": ", ";
									$fieldList = $fieldList . $pre . $field;
									$qList = $qList . $pre . ($bb ? 'UNHEX(?)' : '?');
									array_push($valuesList, $value);
									$c++;
								}

								next($row);
							}
							
							$qry = "INSERT INTO tasks (" . $fieldList .") VALUES( " . $qList .")";
							$res = $pdo->prepare($qry);
							$res->execute($valuesList);
						} else {
						//	Splitting task became redundant
						$taskDone = TRUE;
						}
					
						$res = $pdo->prepare("UPDATE tasks SET checkin_count=checkin_count+1 WHERE id=? AND access=?");
						$res->execute([$id, $access]);

						$ok = TRUE;
					} else {
						$result = "Error: Invalid new prefix string $new_pref\n";
					}
				} else {
					if ($row['status']=='F') {
						$result = "Cancelled: The task being split was marked finalised, which was unexpected\n";
					} else {
						$result = "Cancelled: The task being split was found to have status ".$row['status']. ", which was unexpected\n";
					}
				}
			} else {
				$result = "Cancelled: No match to id=$id, access=$access for the task being split\n";
			}
				
			$pdo->commit();
			break;

		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in splitTask() / tasks] ", $e);
		}
	}
	
	if (!$ok) return $result;

	//	Transaction #2: 'workers'

	if ($cid>0) {
		for ($r=1;$r<=$maxRetries;$r++) {
			try {

				$pdo->beginTransaction();
				$res = $pdo->prepare("UPDATE workers SET checkin_count=checkin_count+1 WHERE id=?");
				$res->execute([$cid]);
				$pdo->commit();
				break;

			} catch (Exception $e) {
				$pdo->rollback();
				if ($r==$maxRetries) handlePDOError($e);
				else handlePDOError0("[retry $r of $maxRetries in splitTask() / workers] ", $e);
			}
		}
	}
	
	return $taskDone ? "Done\n" : "OK\n";
}


//	Function to register a worker, using their supplied program instance number and their IP address

function register($pi, $teamName) {
	global $maxClients, $pdo, $maxRetries;

	$ra = $_SERVER['REMOTE_ADDR'];
	if (!is_string($ra)) return "Error: Unable to determine connection's IP address\n";
	
	//	Transaction #1: 'workers'

	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();
			$res = $pdo->query("SELECT COUNT(id) FROM workers");
			
			$ok = TRUE;
			if ($res && ($row = $res->fetch(PDO::FETCH_NUM))) {
				$nw = intval($row[0]);
				if ($nw >= $maxClients) {
					$result = "Thanks for offering to join the project, but unfortunately the server is at capacity right now ($nw out of $maxClients), and cannot accept any more clients. We will continue to increase capacity, so please check back soon!\n";
					$ok = FALSE;
				}
			}
			
			if ($ok) {
				$res = $pdo->prepare("INSERT INTO workers (IP,instance_num,ts_registered,team) VALUES(?, ?, NOW(), ?)");
				$res->execute([$ra, $pi, $teamName]);

				$result = "Registered\nClient id: " . $pdo->lastInsertId() . "\nIP: $ra\nprogramInstance: $pi\nteam name: $teamName\n";
			}
			
			$pdo->commit();
			return $result;

		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in register() / workers] ", $e);
		}
	}
}


//	Function to unregister a worker, using their supplied program instance number, client ID and IP address

function unregister($cid,$ip,$pi) {
	global $A_LO, $A_HI, $pdo, $maxRetries;
		
	//	Transaction #1: 'workers'
	
	$ctsk = 0;

	for ($r=1;$r<=$maxRetries;$r++) {
		try {
			$pdo->beginTransaction();
			$res = $pdo->prepare("SELECT current_task FROM workers WHERE id=? AND instance_num=? AND IP=?");
			$res->execute([$cid, $pi, $ip]);
			if ($row = $res->fetch(PDO::FETCH_NUM)) $ctsk = intval($row[0]);
			
			$res = $pdo->prepare("DELETE FROM workers WHERE id=? AND instance_num=? AND IP=?");
			$res->execute([$cid, $pi, $ip]);

			$result = "OK, client record deleted\n";
		
			$pdo->commit();
			break;
		} catch (Exception $e) {
			$pdo->rollback();
			if ($r==$maxRetries) handlePDOError($e);
			else handlePDOError0("[retry $r of $maxRetries in unregister() / workers] ", $e);
		}
	}
	
	//	Transaction #2: 'tasks'
	//	Unassign any task that was assigned to this client
	
	if ($ctsk>0) {
		if (relTask($ctsk, $cid, -1)==$cid) $result = $result . "Relinquished task $ctsk\n";
	}
	
	return $result;
}

//	Process query string
//	====================

$queryOK = FALSE;
$err = 'Invalid query';
$qs = $_SERVER['QUERY_STRING'];

$pdo = new PDO('mysql:host='.DB_SERVER.';dbname='.DB_DATABASE,DB_USERNAME, DB_PASSWORD);
$pdo->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);

//	Currently not using instanceCount

/*
$didUpdate=FALSE;
$ic=instanceCount(2,2,$didUpdate);
*/
//	We set $ic to zero to say "OK to run, we don't care how many instances of the script are running

$ic = 0;

if (is_string($qs)) {
	parse_str($qs, $q);
	
	//	Validate query string arguments
	
	reset($q);

	$ok = TRUE;
	for ($i=0; $i<count($q); $i++) {
		$k = key($q);
		$v = current($q);
		next($q);
		if ($k != 'action' && $k != 'pwd' && $k != 'team' && $k != 'stressTest' && !checkString($v)) {
			$ok=FALSE;
			break;
		}
		if ($k == 'team' && !checkString2($v)) {
			$ok=FALSE;
			break;
		}
	}
	
	if ($ok) {
		$version = intval($q['version']);
		if ($version < $versionAbsolutelyRequired) {
			$err = "The version of DistributedChaffinMethod you are using has been superseded.\nPlease download version $versionAbsolutelyRequired or later from " . CODE_REPO . "\nThanks for being part of this project!";
		} else {
			if ($version >= 7 && $ic != 0) {
				$queryOK = TRUE;
				echo "Wait\n";
			} else {
				$action = $q['action'];
				
				$stressTest ='N';
				if (strpos($qs,'stressTest')) $stressTest = $q['stressTest'];
				
				$teamName = 'anonymous';
				if (strpos($qs,'team')) $teamName = $q['team'];
			
				if (is_string($action)) {
					if ($action == "hello") {
						$queryOK = TRUE;
						echo "Hello world.\n";
					} else if ($action == "register") {
						if ($version < $versionForNewTasks) {
							$err = "The version of DistributedChaffinMethod you are using has been superseded.\nPlease download version $versionForNewTasks or later from " . CODE_REPO . "\nThanks for being part of this project!";
						} else {
							$pi = $q['programInstance'];
							if (is_string($pi) && is_string($teamName)) {
								$queryOK = TRUE;
								echo register($pi, $teamName);
							}
						}
					} else if ($action == "getTask") {
						$pi = $q['programInstance'];
						$cid = $q['clientID'];
						$ip = $q['IP'];
						if (is_string($pi) && is_string($cid) && is_string($ip) && is_string($teamName)) {
							if ($version < $versionForNewTasks) {
								unregister($cid,$ip,$pi);
								$err = "The version of DistributedChaffinMethod you are using has been superseded.\nPlease download version $versionForNewTasks or later from " . CODE_REPO . "\nThanks for being part of this project!";
							} else {
								$queryOK = TRUE;
								echo getTask($cid,$ip,$pi,$version,$teamName,$stressTest);
							}
						}
					} else if ($action == "unregister") {
						$pi = $q['programInstance'];
						$cid = $q['clientID'];
						$ip = $q['IP'];
						if (is_string($pi) && is_string($cid) && is_string($ip)) {
							$queryOK = TRUE;
							echo unregister($cid,$ip,$pi);
						}
					} else if ($action == "checkIn") {
						$id = $q['id'];
						$access = $q['access'];
						if (is_string($id) && is_string($access)) {
							$queryOK = TRUE;
							echo checkIn($id, $access);
						}
					} else if ($action == "splitTask") {
						$id = $q['id'];
						$access = $q['access'];
						$new_pref = $q['newPrefix'];
						$branchOrder = $q['branchOrder'];
						if (is_string($id) && is_string($access) && is_string($new_pref) && is_string($branchOrder)) {
							$queryOK = TRUE;
							echo splitTask($id, $access, $new_pref, $branchOrder, $stressTest);
						}
					} else if ($action == "cancelStalledTasks") {
						$maxMins_str = $q['maxMins'];
						$maxMins = intval($maxMins_str);
						if (is_string($maxMins_str) && $maxMins>0 && DB_PASSWORD==$q['pwd']) {
							$queryOK = TRUE;
							echo cancelStalledTasks($maxMins);
						}
					} else if ($action == "cancelStalledClients") {
						$maxMins_str = $q['maxMins'];
						$maxMins = intval($maxMins_str);
						if (is_string($maxMins_str) && $maxMins>0 && DB_PASSWORD==$q['pwd']) {
							$queryOK = TRUE;
							echo cancelStalledClients($maxMins);
						}
					} else if ($action == "maybeFinishedAllTasks") {
						if (DB_PASSWORD==$q['pwd']) {
							$queryOK = TRUE;
							echo maybeFinishedAllTasks();
						}
					} else if ($action == "finishTask") {
						$id = $q['id'];
						$access = $q['access'];
						$pro_str = $q['pro'];
						$str = $q['str'];
						$nodeCount = "0";
						if (strpos($qs,'nodeCount')) $nodeCount = $q['nodeCount'];
						if (is_string($id) && is_string($access) && is_string($pro_str) && is_string($str) && is_string($teamName)) {
							$pro = intval($pro_str);
							if ($pro > 0) {
								$queryOK = TRUE;
								echo finishTask($id, $access, $pro, $str, $teamName, $nodeCount,$stressTest);
							}
						}
					} else if ($action == "relinquishTask") {
						$id = $q['id'];
						$access = $q['access'];
						$cid = $q['clientID'];
						if (is_string($id) && is_string($access) && is_string($cid)) {
							$queryOK = TRUE;
							echo relinquishTask($id, $access, $cid);
						}
					} else {
						$n_str = $q['n'];
						$w_str = $q['w'];
						if (is_string($n_str) && is_string($w_str)) {
							$n = intval($n_str);
							$w = intval($w_str);
							if ($n >= $min_n && $n <= $max_n && $w >= 0) {
								$str = $q['str'];
								if (is_string($str)) {
								
									//	"witnessString" action verifies and possibly records a witness to a certain number of distinct permutations being
									//	visited by a string with a certain number of wasted characters.
									//
									//	Query arguments: n, w, str.
									//
									//	Returns:  "Valid string ... " / (n, w, p) for current maximum p, or "Error: ... "
								
									if ($action == "witnessString") {
										$p = analyseString($str,$n);
										$slen = strlen($str);
										$p0 = $slen - $w - $n + 1;
										if ($p == $p0) {
											$queryOK = TRUE;
											echo "Valid string with $p permutations\n";
											$pp = strpos($qs,'pro');
											if ($pp===FALSE) {
												$pro = -1;
											} else {
												$pro_str = $q['pro'];
												if (is_string($pro_str)) {
													$pro = intval($pro_str);
												} else {
													$pro = -1;
												}
											}
											echo maybeUpdateWitnessStrings($n, $w, $p, $str, $pro, $teamName);
										} else if ($p<0) {
											$err = 'Invalid string';
										} else {
											$err = "Unexpected permutation count [$p permutations, expected $p0 for w=$w, length=$slen]";
										}
									}
									
									//	"createTask" action puts a new task into the tasks database.
									//
									//	Query arguments: n, w, str, pte, pwd.
									//
									//	where "pte" is the number of permutations to exceed in any strings the task finds.
									//
									//	Returns: "Task id: ... " or "Error: ... "
								
									else if ($action == "createTask") {
										$p_str = $q['pte'];
										if (is_string($p_str) && DB_PASSWORD==$q['pwd']) {
											$p = intval($p_str);
											if ($p > 0 && analyseString($str,$n) > 0) {
												$queryOK = TRUE;
												echo makeTask($n, $w, $p, $str, $stressTest);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

if (!$queryOK) echo "Error: $err \n";

//	Currently not using instanceCount

//	if ($didUpdate) instanceCount(-2,0);

?>
