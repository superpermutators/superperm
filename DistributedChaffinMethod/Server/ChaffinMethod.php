<?php
include '../config.php';

//	Version of the client ABSOLUTELY required.
//  Note that if this is changed while clients are running tasks,
//	those tasks will be disrupted and will need to be cancelled and reallocated
//	with a call to cancelStalledTasks, and the disrupted clients will need to be unregistered
//	with a call to cancelStalledClients.

$versionAbsolutelyRequired = 8;

//	Version of the client required for new registrations and new tasks to be allocated.
//	If this is changed while clients are running tasks, the task will continue uninterrupted;
//	the client will be unregistered and will exit cleanly the next time it asks for a new task.

$versionForNewTasks = 8;

//	Maximum number of clients to register

$maxClients = 100;

//	Valid range for $n

$min_n = 3;
$max_n = 7;

//	Range for access codes

$A_LO = 100000000;
$A_HI = 999999999;

//	Currently not using instanceCount

/*
//	Record number of instances of this script

function instanceCount($inc,$def,&$didUpdate)
{
$ii=0;
$fname = "InstanceCount.txt";
$fp = fopen($fname, "r+");
if ($fp===FALSE)
	{
	$fp = fopen($fname, "w");
	if (!($fp===FALSE))
		{
		fwrite($fp, $def<10?"0$def":"$def");
		fclose($fp);
		};
	}
else	
	{
	$ii=1;
	if (flock($fp, LOCK_EX))
		{
		$ic = fgets($fp);
		$ii = intval($ic);
		$iq = $ii+$inc;
		fseek($fp,0,SEEK_SET);
		fwrite($fp, $iq<10?"0$iq":"$iq");
		fflush($fp);
		flock($fp, LOCK_UN);
		$didUpdate=TRUE;
		}
	else $didUpdate=FALSE;
	fclose($fp);
	};
return $ii; 
}
*/

function handlePDOError($e) {
	print("Error: " . $e->getMessage());
	mail("jay.pantone@gmail.com", "PDO ERROR :(", $e->getMessage());
	mail("jay.pantone@marquette.edu", "PDO ERROR :(", $e->getMessage());
}

function factorial($n)
{
if ($n==1) return 1;
return $n*factorial($n-1);
}


//	Function to check that a string contains only the characters 0-9 and .

function checkString($str)
{
$sp = str_split($str);
$sl = count($sp);
$ok = TRUE;
for ($i=0; $i<$sl; $i++)
	{
	$c = $sp[$i];
	if ($c!='.' && $c!='0')
		{
		$v = intval($c);
		if ($v<1 || $v>9)
			{
			$ok=FALSE;
			break;
			};
		};
	};
return $ok;
}

//	Function to check (what should be) a digit string to see if it is valid, and count the number of distinct permutations it visits.

function analyseString($str, $n)
{
if (is_string($str))
	{
	$slen = strlen($str);
	if ($slen > 0)
		{
		$stri = array_map('intval',str_split($str));
		$strmin = min($stri);
		$strmax = max($stri);
		if ($strmin == 1 && $strmax == $n)
			{
			$perms = array();
			
			//	Loop over all length-n substrings
			
			for ($i=0; $i <= $slen - $n; $i++)
				{
				$s = array_slice($stri,$i,$n);
				$u = array_unique($s);
				if (count($u) == $n)
					{
					$pval = 0;
					$factor = 1;
					for ($j=0; $j<$n; $j++)
						{
						$pval += $factor * $s[$j];
						$factor *= 10;
						};
					if (!in_array($pval, $perms)) $perms[] = $pval;
					};
				};
			return count($perms);
			};
		};
	};
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
	if ($pro > 0 && $pro <= $p) return "Error: Trying to set permutations ruled out to $pro while exhibiting a string with $p permutations\n";

	$isSuper = ($p==factorial($n));

	global $pdo;
	// global $host, $user_name, $pwd, $dbase;
	// $mysqli = new mysqli($host, $user_name, $pwd, $dbase);
	// if ($mysqli->connect_errno)
		// {
		// return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
		// }
	// else
		// {
	try {
		

		// if (!$mysqli->real_query("LOCK TABLES witness_strings " . ($p>=0 ? "WRITE" : "READ") . ($isSuper ? ", superperms WRITE" :"")))
		// 	{
		// 	$mysqli->close();
		// 	return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		// 	}
			
		if ($isSuper) {
			$ip = $_SERVER['REMOTE_ADDR'];

			$res = $pdo->prepare("INSERT INTO superperms (n,waste,perms,str,IP,team) VALUES(?, ?, ?, ?, ?, ?)");
			$res->execute([$n, $w, $p, $str, $ip, $teamName]);

			
			// if (!$mysqli->real_query("INSERT INTO superperms (n,waste,perms,str,IP) VALUES($n, $w, $p, '$str', '$ip')"))
			// 	{
			// 	$mysqli->real_query("UNLOCK TABLES");
			// 	$mysqli->close();
			// 	return "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
			// 	}
		}

		// $res = $mysqli->query("SELECT perms FROM witness_strings WHERE n=$n AND waste=$w" . ($p>=0 ? " FOR UPDATE" : ""));
		$pdo->beginTransaction();
		$res = $pdo->prepare("SELECT perms FROM witness_strings WHERE n=? AND waste=?" . ($p>=0 ? " FOR UPDATE" : ""));
		$res->execute([$n, $w]);

		// if ($mysqli->errno) $result = "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		// else
			// {
		if ($res->rowCount() == 0) {
			//	No data at all for this (n,w) pair
			
			if ($p >= 0) {
				
				//	Try to update the database
				
				if ($pro > 0) {
					$final = ($pro == $p+1) ? "Y" : "N";
					//if ($mysqli->real_query("INSERT INTO witness_strings (n,waste,perms,str,excl_perms,final) VALUES($n, $w, $p, '$str', $pro, '$final')"))
						// $result = "($n, $w, $p)\n";
					// else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					$res = $pdo->prepare("INSERT INTO witness_strings (n,waste,perms,str,excl_perms,final,team) VALUES(?, ?, ?, ?, ?, ?, ?)");
					$res->execute([$n, $w, $p, $str, $pro, $final, $teamName]);
					$result = "($n, $w, $p)\n";
				} else {
					// if ($mysqli->real_query("INSERT INTO witness_strings (n,waste,perms,str) VALUES($n, $w, $p, '$str')"))
						// $result = "($n, $w, $p)\n";
					// else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					$res = $pdo->prepare("INSERT INTO witness_strings (n,waste,perms,str,team) VALUES(?, ?, ?, ?, ?)");
					$res->execute([$n, $w, $p, $str, $teamName]);
					$result = "($n, $w, $p)\n";
				}
			} else {
				//	If we are just querying, return -1 in lieu of maximum
				$result = "($n, $w, -1)\n";
			}
		} else {
			//	There is existing data for this (n,w) pair, so check to see if we have a greater permutation count
			
			$row = $res->fetch(PDO::FETCH_NUM);
			// $res->data_seek(0);
			// $row = $res->fetch_array();
			$p0 = intval($row[0]);
			
			if ($p > $p0) {
				//	Our new data has a greater permutation count, so update the entry
			
				if ($pro > 0) {
					$final = ($pro == $p+1) ? "Y" : "N";
					// if ($mysqli->real_query("REPLACE INTO witness_strings (n,waste,perms,str,excl_perms,final) VALUES($n, $w, $p, '$str', $pro, '$final')"))
						// $result = "($n, $w, $p)\n";
					// else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					$res = $pdo->prepare("REPLACE INTO witness_strings (n,waste,perms,str,excl_perms,final,team) VALUES(?, ?, ?, ?, ?, ?, ?)");
					$res->execute([$n, $w, $p, $str, $pro, $final, $teamName]);
					$result = "($n, $w, $p)\n";
				} else {
					// if ($mysqli->real_query("REPLACE INTO witness_strings (n,waste,perms,str) VALUES($n, $w, $p, '$str')"))
						// $result = "($n, $w, $p)\n";
					// else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					$res = $pdo->prepare("REPLACE INTO witness_strings (n,waste,perms,str,team) VALUES(?, ?, ?, ?, ?)");
					$res->execute([$n, $w, $p, $str, $teamName]);
					$result = "($n, $w, $p)\n";
				}
			} else {
				//	Our new data does not have a greater permutation count (or might just be a query with $p=-1), so return existing maximum
				
				$result = "($n, $w, $p0)\n";
			}
		}
		// }
		
		// $mysqli->real_query("UNLOCK TABLES");
		// $mysqli->close();
		$pdo->commit();
		return $result;
	} catch (Exception $e) {
		$pdo->rollback();
		handlePDOError($e);;
	}
}

//	Function to make a new task record
//
//	Returns "Task id: ... " or "Error: ... "

function makeTask($n, $w, $pte, $str) {
	// global $host, $user_name, $pwd, $dbase, $A_LO, $A_HI;
	global $A_LO, $A_HI, $pdo;

	// $mysqli = new mysqli($host, $user_name, $pwd, $dbase);
	// if ($mysqli->connect_errno)
		// {
		// return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
		// }
	try {
		// if (!$mysqli->real_query("LOCK TABLES tasks WRITE"))
			// {
			// $mysqli->close();
			// return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
			// }

		$pdo->beginTransaction();

		$res = $pdo->prepare("SELECT id FROM tasks WHERE n=? AND waste=? AND prefix=? AND perm_to_exceed=?");
		$res->execute([$n, $w, $str, $pte]);
			
		// $res = $mysqli->query("SELECT id FROM tasks WHERE n=$n AND waste=$w AND prefix='$str' AND perm_to_exceed=$pte");
		// if ($mysqli->errno) $result = "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		// else if ($res->num_rows!=0)
		if ($row = $res->fetch(PDO::FETCH_NUM)) {
			// $res->data_seek(0);
			// $row = $res->fetch_array();
			$id = $row[0];
			$result = "Task id: $id already existed with those properties\n";
		} else {
			$access = mt_rand($A_LO,$A_HI);
			$br = substr("000000000",0,$n);

			$res = $pdo->prepare("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,branch_order) VALUES(?, ?, ?, ?, ?, ?)");
			$res->execute([$access, $n, $w, $str, $pte, $br]);

			// if ($mysqli->real_query("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,branch_order) VALUES($access, $n, $w, '$str', $pte,'$br')"))
			$result = "Task id: " . $pdo->lastInsertId() . "\n";
			// else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		}
		
		// $mysqli->real_query("UNLOCK TABLES");
		// $mysqli->close();
		$pdo->commit();
		return $result;
	} catch (Exception $e) {
		$pdo->rollback();
		handlePDOError($e);;
	}
}

//	Function to allocate an unallocated task, if there is one.
//
//	Returns:	"Task id: ... " / "Access code:" /"n: ..." / "w: ..." / "str: ... " / "pte: ... " / "pro: ... " / "branchOrder: ..."
//	then all finalised (w,p) pairs
//	or:			"No tasks"
//	or:			"Error ... "

function getTask($cid,$ip,$pi,$version,$teamName) {
// global $host, $user_name, $pwd, $dbase;
	global $pdo;
// $mysqli = new mysqli($host, $user_name, $pwd, $dbase);
// if ($mysqli->connect_errno)
	// {
	// return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	// }
	try {
		// if (!$mysqli->real_query("LOCK TABLES tasks WRITE, witness_strings READ, workers WRITE"))
		// 	{
		// 	$mysqli->close();
		// 	return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		// 	};

		$pdo->beginTransaction();

		$res = $pdo->prepare("SELECT * FROM workers WHERE id=? AND instance_num=? AND IP=? FOR UPDATE");
		$res->execute([$cid, $pi, $ip]);

		// $wres = $mysqli->query("SELECT * FROM workers WHERE id=$cid AND instance_num=$pi AND IP='$ip' FOR UPDATE");
		// if ($mysqli->errno) $result = "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		// else
			// {
		if ($res->rowCount() == 0) {
			$result = "Error: No client found with those details\n";
		} else {

			$res = $pdo->prepare("UPDATE workers SET checkin_count=checkin_count+1 WHERE id=?");
			$res->execute([$cid]);

			// if (!$mysqli->real_query("UPDATE workers SET checkin_count=checkin_count+1 WHERE id=$cid"))
				 // $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
			// else
				// {
			$res = $pdo->query("SELECT * FROM tasks WHERE status='U' ORDER BY branch_order LIMIT 1 FOR UPDATE");
			// $res = $mysqli->query("SELECT * FROM tasks WHERE status='U' ORDER BY branch_order LIMIT 1 FOR UPDATE");
			// if ($mysqli->errno) $result = "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
			// else
				// {
			if ($res->rowCount() == 0) {
				$result = "No tasks\n";
			} else {
				// $res->data_seek(0);
				// $row = $res->fetch_assoc();
				$row = $res->fetch();
				$id = $row['id'];
				$access = $row['access'];
				$n = $row['n'];
				$w = $row['waste'];
				$str = $row['prefix'];
				$pte = intval($row['perm_to_exceed']);
				$ppro = $row['prev_perm_ruled_out'];
				$br = $row['branch_order'];


				$res = $pdo->prepare("SELECT perms FROM witness_strings WHERE n=? AND waste=?");
				$res->execute([$n, $w]);

				if ($row = $res->fetch(PDO::FETCH_NUM)) {
					$p0 = intval($row[0]);
				} else {
					$p0 = -1;
				}

				// $res0 = $mysqli->query("SELECT perms FROM witness_strings WHERE n=$n AND waste=$w");
				// if (!$mysqli->errno && $res0->num_rows == 1)
					// {
					// $res0->data_seek(0);
					// $row0 = $res0->fetch_array();
					// }
				// else $p0 = -1;

				if ($p0 > $pte) {
					$pte = $p0;
				}
				
				if (is_integer($id) && is_integer($access) && is_integer($n) && is_integer($w) && is_string($str) && is_integer($ppro) && is_string($br)) {
					$res = $pdo->prepare("UPDATE tasks SET status='A', ts_allocated=NOW(), client_id=?, team=? WHERE id=?");
					$res->execute([$cid, $teamName, $id]);

					$res = $pdo->prepare("UPDATE workers SET current_task=? WHERE id=?");
					$res->execute([$id, $cid]);

					$result = "Task id: $id\nAccess code: $access\nn: $n\nw: $w\nstr: $str\npte: $pte\npro: $ppro\nbranchOrder: $br\n";
					
					//	Output finalised (w,p) pairs
					//	Clients from v8 make use of known values for n=6, w<=115
					
					$w0 = 0;
					if ($version >= 8 && $n == 6) {
						$w0 = 115;
					}
					
					$res = $pdo->prepare("SELECT waste, perms FROM witness_strings WHERE n=? AND waste > ? AND final='Y' ORDER BY waste ASC");
					$res->execute([$n, $w0]);

					// $res2 = $mysqli->query("SELECT waste, perms FROM witness_strings WHERE n=$n AND waste>$w0 AND final='Y' ORDER BY waste ASC");
					// if ($mysqli->errno) $result = "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					// else
						// {
					while ($row = $res->fetch(PDO::FETCH_NUM)) {
						$result = $result . "(" . $row[0] . "," . $row[1] . ")\n";
					}
				}
				else $result = "Error: Unable to find expected fields in database\n";
				// };
			}
			// };
		}
		$pdo->commit();
		// $mysqli->real_query("UNLOCK TABLES");
		// $mysqli->close();
		return $result;
	} catch (Exception $e) {
		$pdo->rollback();
		handlePDOError($e);;
	}
}

//	Function to increment the checkin_count of a specified task and return the current maximum permutation for (n,w)

function checkMax($id, $access, $cid, $ip, $pi, $n, $w) {
// global $host, $user_name, $pwd, $dbase;
// $mysqli = new mysqli($host, $user_name, $pwd, $dbase);
	global $pdo;
// if ($mysqli->connect_errno)
	// {
	// return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	// }
// else
	// {
	try {
		$pdo->beginTransaction();

		// if (!$mysqli->real_query("LOCK TABLES tasks WRITE, workers WRITE, witness_strings READ"))
			// {
			// $mysqli->close();
			// return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
			// };

		$res = $pdo->prepare("SELECT status FROM tasks WHERE id=? AND access=? FOR UPDATE");
		$res->execute([$id, $access]);

		// $res = $mysqli->query("SELECT status FROM tasks WHERE id=$id AND access=$access FOR UPDATE");
		if ($row = $res->fetch(PDO::FETCH_NUM)) {
			// $res->data_seek(0);
			// $row = $res->fetch_array();

			$status = $row[0];

			if ($status == 'A') {
				$res = $pdo->prepare("UPDATE tasks SET checkin_count=checkin_count+1 WHERE id=? AND access=?");
				$res->execute([$id, $access]);

				$res = $pdo->prepare("UPDATE workers SET checkin_count=checkin_count+1 WHERE id=? AND instance_num=? AND IP=?");
				$res->execute([$cid, $pi, $ip]);

				// if ($mysqli->real_query("UPDATE tasks SET checkin_count=checkin_count+1 WHERE id=$id AND access=$access") &&
					// $mysqli->real_query("UPDATE workers SET checkin_count=checkin_count+1 WHERE id=$cid AND instance_num=$pi AND IP='$ip'"))
					// {

				$res = $pdo->prepare("SELECT perms FROM witness_strings WHERE n=? AND waste=?");
				$res->execute([$n, $w]);
				// $res = $mysqli->query("SELECT perms FROM witness_strings WHERE n=$n AND waste=$w");
					// if ($mysqli->errno) $result = "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					// else
					// 	{
				if ($row = $res->fetch(PDO::FETCH_NUM)) {
					$p0 = intval($row[0]);
					$result = "($n, $w, $p0)\n";
				} else {
					$result = "($n, $w, -1)\n";
				}
					// 	if ($res->num_rows == 0)
					// 		{
					// 		//	No data at all for this (n,w) pair
							
					// 		$result = "($n, $w, -1)\n";
					// 		}
					// 	else
					// 		{
					// 		//	There is existing data for this (n,w) pair, so check to see if we have a greater permutation count
							
					// 		$res->data_seek(0);
					// 		$row = $res->fetch_array();
					// 		$p0 = intval($row[0]);
							
					// 		$result = "($n, $w, $p0)\n";
					// 		};
					// 	};
					// }
				// else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
				// }
			} else if ($status == 'F') {
				$result = "Error: The task checking in was marked finalised, which was unexpected\n";
			} else {
				$result = "Error: The task checking in was found to have status " . $status . ", which was unexpected\n";
			}
		} else {
			// if ($mysqli->errno)	$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
			// else 
			$result = "Error: No match to id=$id, access=$access for the task checking in\n";
		}

		$pdo->commit();
		// $mysqli->real_query("UNLOCK TABLES");
		// $mysqli->close();
		return $result;
	} catch (Exception $e) {
		$pdo->rollback();
		handlePDOError($e);;
	}
}

//	Function to cancel stalled tasks
//
//	Returns some stats about assigned task times since checkin, or "Error: ..."

function cancelStalledTasks($maxMin) {
// global $host, $user_name, $pwd, $dbase, $A_LO, $A_HI;
	global $A_LO, $A_HI, $pdo;

// $mysqli = new mysqli($host, $user_name, $pwd, $dbase);
// if ($mysqli->connect_errno)
	// {
	// return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	// }
// else
	// {

	try {
		// if (!$mysqli->real_query("LOCK TABLES tasks WRITE, workers WRITE"))
			// {
			// $mysqli->close();
			// return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
			// };

		$pdo->beginTransaction();

		$res = $pdo->query("SELECT id, TIMESTAMPDIFF(MINUTE,ts,NOW()), client_id FROM tasks WHERE status='A' FOR UPDATE");

		// $res = $mysqli->query("SELECT id, TIMESTAMPDIFF(MINUTE,ts,NOW()), client_id FROM tasks WHERE status='A' FOR UPDATE");
		// if ($mysqli->errno)	$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		// else
			// {
			if ($res->rowCount() > 0) {
				$result = "";
				$nass = $res->rowCount();
				$cancelled = 0;
				$maxStall = 0;
				while($row = $res->fetch(PDO::FETCH_NUM)) {
				// for ($i=0;$i<$nass;$i++)
					// {
					// $res->data_seek($i);
					// $row = $res->fetch_array();
					$stall = intval($row[1]);
					$maxStall = max($stall, $maxStall);
					// if ($stall > $maxStall) {
						// $maxStall = $stall;
					// }
					
					if ($stall > $maxMin) {
						$id = $row[0];
						$cid = $row[2];
						$access = mt_rand($A_LO,$A_HI);

						$res = $pdo->prepare("UPDATE tasks SET status='U', access=? WHERE id=?");
						$res->execute([$access, $id]);

						$res = $pdo->prepare("UPDATE workers SET current_task=0 WHERE id=?");
						$res->execute([$cid]);

						// if (!($mysqli->real_query("UPDATE tasks SET status='U', access=$access WHERE id=$id") &&
						// 	$mysqli->real_query("UPDATE workers SET current_task=0 WHERE id=$cid")))
						// 	{
						// 	$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
						// 	break;
						// 	};
						
						$cancelled++;
					}
				}
				$result = $result . "$nass assigned tasks, maximum stalled time = $maxStall minutes, cancelled $cancelled tasks\n";
			} else {
				$result = "0 assigned tasks\n";
			}
		
		$pdo->commit();
		// $mysqli->real_query("UNLOCK TABLES");
		// $mysqli->close();
		return $result;
		
	} catch (Exception $e) {
		$pdo->rollback();
		handlePDOError($e);;
	}
}

//	Function to cancel stalled clients
//
//	Returns some stats about assigned times since checkin, or "Error: ..."

function cancelStalledClients($maxMin)
{
// global $host, $user_name, $pwd, $dbase;
	global $pdo;

// $mysqli = new mysqli($host, $user_name, $pwd, $dbase);
// if ($mysqli->connect_errno)
	// {
	// return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	// }
// else
	try {
		// if (!$mysqli->real_query("LOCK TABLES workers WRITE"))
			// {
			// $mysqli->close();
			// return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
			// };

		$pdo->beginTransaction();
		$res = $pdo->query("SELECT id, TIMESTAMPDIFF(MINUTE,ts,NOW()) FROM workers FOR UPDATE");
		// $res = $mysqli->query("SELECT id, TIMESTAMPDIFF(MINUTE,ts,NOW()) FROM workers FOR UPDATE");
		// if ($mysqli->errno)	$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		// else
			// {
		if ($res->rowCount() > 0) {

			$result = "";
			$nreg = $res->rowCount();
			$cancelled = 0;
			$maxStall = 0;

			while($row = $res->fetch(PDO::FETCH_NUM)) {
				// $res->data_seek($i);
				// $row = $res->fetch_array();
				$stall = intval($row[1]);
				$maxStall = max($maxStall, $stall);
				// if ($stall > $maxStall) $maxStall = $stall;
				
				if ($stall > $maxMin) {
					$id = $row[0];

					$pdo->prepare("DELETE FROM workers WHERE id=$id");
					$res->execute([$id]);

					// if (!$mysqli->real_query("DELETE FROM workers WHERE id=$id"))
					// 	{
					// 	$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					// 	break;
					// 	};
					$cancelled++;
					// };
				}
			}

			$result = $result . "$nreg registered clients, maximum stalled time = $maxStall minutes, cancelled $cancelled clients\n";

		} else {
			$result = "0 registered clients\n";
		}

		$pdo->commit();		
		// $mysqli->real_query("UNLOCK TABLES");
		// $mysqli->close();
		return $result;
	} catch (Exception $e) {
		$pdo->rollback();
		handlePDOError($e);;
	}
}

//	Function to do further processing if we have finished all tasks for the current (n,w,iter) search

function finishedAllTasks($n, $w, $iter) {
	global $A_LO, $A_HI, $pdo;

	$needTighterBound = TRUE;

	//	Find the highest value of perm_ruled_out from all tasks for this (n,w,iter); no strings were found with this number of perms or
	//	higher, across the whole search.

	// try {
	// Note: we don't beginTransaction or commit because this is called from inside another function who does that on our behalf

	$res = $pdo->prepare("SELECT MAX(perm_ruled_out) FROM finishedtasks WHERE n=? AND waste=? AND iteration=? AND status='F'");
	$res->execute([$n, $w, $iter]);

	// $res = $mysqli->query("SELECT MAX(perm_ruled_out) FROM tasks WHERE n=$n AND waste=$w AND iteration=$iter AND status='F'");
	// if ($mysqli->errno) return "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
	// $res->data_seek(0);
	// $row = $res->fetch_array();

	// What happens if there is no row? Could that happen?
	$row = $res->fetch(PDO::FETCH_NUM);
	$pro = intval($row[0]);
	// $res->close();

	//	Was any string found for this search (or maybe for the same (n,w), but by other means)?
	$res = $pdo->prepare("SELECT perms, excl_perms FROM witness_strings WHERE n=? AND waste=? FOR UPDATE");
	$res->execute([$n, $w]);
	// $res = $mysqli->query("SELECT perms, excl_perms FROM witness_strings WHERE n=$n AND waste=$w FOR UPDATE");
	// if ($mysqli->errno) return "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";

	if ($res->rowCount() > 0) {
		//	Yes.  Update the excl_perms and final fields.
		
		$row = $res->fetch(PDO::FETCH_NUM);
		$p_str = $row[0];
		$p = intval($p_str);
		$pro0_str = $row[1];
		$pro0 = intval($pro0_str);
		
		// if ($pro < $pro0) {
		if ($pro0 > 0 && $pro0 < $pro) {
			$pro = $pro0;
		}

		if ($pro == $p + 1) {
			$final = 'Y';
			$needTighterBound = FALSE;
		} else {
			// $final = ($pro == $p+1) ? "Y" : "N";
			$final = 'N';
			$needTighterBound = TRUE;
		}

		$res = $pdo->prepare("UPDATE witness_strings SET excl_perms=?, final=? WHERE n=? AND waste=?");
		$res->execute([$pro, $final, $n, $w]);
			// if (!$mysqli->real_query("UPDATE witness_strings SET excl_perms=$pro, final='$final' WHERE n=$n AND waste=$w")) {
			// 	return "Error: Unable to update database (" . $mysqli->errno . ") " . $mysqli->error . "\n";
			// }
	}

	if (!$needTighterBound) {	
			
		//	Maybe create a new task for a higher w value
		$fn = factorial($n);
		if ($p < $fn) {
			$w1 = $w+1;
			$str = substr("123456789",0,$n);
			$br = substr("000000000",0,$n);
			$pte = $p + 2*($n-4);
			if ($pte >= $fn) $pte = $fn-1;
			$pro2 = $p+$n+1;
			$access = mt_rand($A_LO,$A_HI);

			$res = $pdo->prepare("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,prev_perm_ruled_out,branch_order) VALUES(?, ?, ?, ?, ?, ?, ?)");
			$res->execute([$access, $n, $w1, $str, $pte, $pro2, $br]);
			return "OK\nTask id: " . $pdo->lastInsertId() . "\n";

			// if ($mysqli->real_query("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,prev_perm_ruled_out,branch_order) VALUES($access, $n, $w1, '$str', $pte, $pro2,'$br')"))
			// 	return "OK\nTask id: $mysqli->insert_id\n";
			// else return "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
			// }
		}
		else {
			return "OK\n";
		}
	} else {
		//	We need to backtrack and search for a lower perm_to_exceed
		
		$res = $pdo->prepare("SELECT MIN(perm_to_exceed) FROM finished_tasks WHERE n=? AND waste=? AND iteration=? AND status='F'");
		$res->execute([$n, $w, $iter]);
		// $res = $mysqli->query("SELECT MIN(perm_to_exceed) FROM tasks WHERE n=$n AND waste=$w AND iteration=$iter AND status='F'");
		// if ($mysqli->errno) return "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		// $res->data_seek(0);
		// $row = $res->fetch_array();

		// What if no row?
		$row = $res->fetch(PDO::FETCH_NUM);
		$pte = intval($row[0])-1;
		// $res->close();
		
		$str = substr("123456789",0,$n);
		$br = substr("000000000",0,$n);
		$access = mt_rand($A_LO,$A_HI);
		$iter1 = $iter+1;

		$res = $pdo->prepare("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,iteration,prev_perm_ruled_out,branch_order) VALUES(?, ?, ?, ?, ?, ?, ?, ?)");
		$res->execute([$access, $n, $w, $str, $pte, $iter1, $pro, $br]);

		// if ($mysqli->real_query("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,iteration,prev_perm_ruled_out,branch_order) VALUES($access, $n, $w, '$str', $pte, $iter1, $pro,'$br')"))
		
		return "OK\nTask id: " . $pdo->lastInsertId() . "\n";
		
		// else return "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
	}
	// } catch (Exception $e) {
		// $pdo->rollback();
		// handlePDOError($e);;
	// }
}

//	Function to mark a task as finished, and if all tasks are finished do some further processing
//
//	Returns: "OK ..." or "Error: ... "
	
function finishTask($id, $access, $pro, $str, $teamName) {
// global $host, $user_name, $pwd, $dbase;
	global $pdo;
// $mysqli = new mysqli($host, $user_name, $pwd, $dbase);
// if ($mysqli->connect_errno)
	// {
	// return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	// }
// else
	try {
		// if (!$mysqli->real_query("LOCK TABLES tasks WRITE, witness_strings WRITE, workers WRITE"))
		// 	{
		// 	$mysqli->close();
		// 	return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		// 	};

		$pdo->beginTransaction();

		// I think we have to lock the whole table to prevent deadlocks where we mark reundant things here
		//   but someone else is trying to get a task in getTask
		// $pdo->exec("LOCK TABLES tasks WRITE, witness_strings WRITE, workers WRITE");		
		// End attempt

		$res = $pdo->prepare("SELECT * FROM tasks WHERE id=? AND access=? AND status='A' FOR UPDATE");
		$res->execute([$id, $access]);

		// $res = $mysqli->query("SELECT * FROM tasks WHERE id=$id AND access=$access FOR UPDATE");
		// if ($res->num_rows==1)
		if ($row = $res->fetch()) {
			// {
			// $res->data_seek(0);
			// $row = $res->fetch_assoc();
			
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
						
					//	Check that the exclusion string is valid
					
					if (analyseString($str,$n) > 0) {
						//	See if we have found a string that makes other searches redundant
											
						$ppro = intval($row['prev_perm_ruled_out']);

						if ($ppro > 0 && $pro >= $ppro && $pro != factorial($n)+1) {
							// $res = $pdo->prepare("UPDATE tasks SET status='F', ts_finished=NOW(), perm_ruled_out=?, excl_witness='Redundant' WHERE n=? AND waste=? AND iteration=? AND status='U'");
							// $res->execute([$pro, $n, $w, $iter]);

							$res = $pdo->prepare("SELECT * FROM tasks WHERE n=? AND waste=? AND iteration=? AND status='U' FOR UPDATE");
							$res->execute([$n, $w, $iter]);

							$update_res = $pdo->prepare("UPDATE num_redundant_tasks SET num_redundant = num_redundant + ?");
							$update_res->execute([$res->rowCount()]);

							while ($row2 = $res->fetch()) {
								$res = $pdo->prepare("INSERT INTO finished_tasks (original_task_id, access,n,waste,prefix,perm_to_exceed,status,branch_order,prev_perm_ruled_out,iteration,ts_allocated,ts_finished,excl_witness,checkin_count,perm_ruled_out,client_id,team) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NOW(), ?, ?, ?, ?, ?)");
								$res->execute([$row2['id'], $row2['access'], $row2['n'], $row2['waste'], $row2['prefix'], $row2['perm_to_exceed'], 'F', $row2['branch_order'], $row2['prev_perm_ruled_out'], $row2['iteration'], $row2['ts_allocated'], 'redundant', $row2['checkin_count'], $pro, $row2['client_id'], $teamName]);
							}

							$res = $pdo->prepare("DELETE FROM tasks WHERE n=? AND waste=? AND iteration=? AND status='U'");
							$res->execute([$n, $w, $iter]);

							// $mysqli->real_query("UPDATE tasks SET status='F', ts_finished=NOW(), perm_ruled_out=$pro, excl_witness='Redundant' WHERE n=$n AND waste=$w AND iteration=$iter AND status='U'");
						}

						$res = $pdo->prepare("INSERT INTO finished_tasks (original_task_id, access,n,waste,prefix,perm_to_exceed,status,branch_order,prev_perm_ruled_out,iteration,ts_allocated,ts_finished,excl_witness,checkin_count,perm_ruled_out,client_id,team) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NOW(), ?, ?, ?, ?, ?)");
						$res->execute([$row['id'], $row['access'], $row['n'], $row['waste'], $row['prefix'], $row['perm_to_exceed'], 'F', $row['branch_order'], $row['prev_perm_ruled_out'], $row['iteration'], $row['ts_allocated'], $str, $row['checkin_count'], $pro, $row['client_id'], $teamName]);
						// echo "INSERT INTO finished_tasks (original_task_id, access,n,waste,prefix,perm_to_exceed,status,branch_order,prev_perm_ruled_out,iteration,ts_allocated,ts_finished,excl_witness,checkin_count,perm_ruled_out,client_id) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, NOW(), ?, ?, ?, ?)";
						// print_r([$row['id'], $row['access'], $row['n'], $row['waste'], $row['prefix'], $row['perm_to_exceed'], $row['status'], $row['branch_order'], $row['prev_perm_ruled_out'], $row['iteration'], $row['ts_allocated'], $row['ts_finished'], $str, $row['checkin_count'], $pro, $row['client_id']]);


						// $res = $pdo->prepare("UPDATE tasks SET status='F', ts_finished=NOW(), perm_ruled_out=?, excl_witness=? WHERE id=? AND access=?");
						// $res->execute([$pro, $str, $id, $access]);

						$res = $pdo->prepare("DELETE FROM tasks WHERE id=? AND access=?");
						$res->execute([$id, $access]);

						$res = $pdo->query("UPDATE num_finished_tasks SET num_finished = num_finished + 1");

						// Try to increment team task count
						$res = $pdo->prepare("UPDATE teams SET tasks_completed = tasks_completed + 1 WHERE team = ?");
						$res->execute([$teamName]);

						// If no rows were affected, we need to add the team to this table
						if ($res->rowCount() == 0) {
							$res = $pdo->prepare("INSERT INTO teams (team, tasks_completed) values (?, 1)");
							$res->execute([$teamName]);
						}

						$res = $pdo->prepare("SELECT id FROM tasks WHERE n=? AND waste=? AND iteration=? AND (status='A' OR status='U') LIMIT 1");
						$res->execute([$n, $w, $iter]);

						if ($res->rowCount() == 0) {
							$result = finishedAllTasks($n, $w, $iter);
						} else {
							$result = "OK\n";
						}

						$cid = $row['client_id'];
						if (intval($cid) > 0) {
							$res = $pdo->prepare("UPDATE workers SET current_task=0 WHERE id=?");
							$res->execute([$cid]);
						}

					
						// if ($mysqli->real_query("UPDATE tasks SET status='F', ts_finished=NOW(), perm_ruled_out=$pro, excl_witness='$str' WHERE id=$id AND access=$access"))
						// 	{
						// 	$res2 = $mysqli->query("SELECT id FROM tasks WHERE n=$n AND waste=$w AND iteration=$iter AND (status='A' OR status='U') LIMIT 1");
						// 	if ($res2->num_rows==0) $result = finishedAllTasks($n, $w, $iter);
						// 	else $result = "OK\n";

						// 	//	Remove this as current task for client
							
						// 	$cid = $row['client_id'];
						// 	if (intval($cid)>0) $mysqli->real_query("UPDATE workers SET current_task=0 WHERE id=$cid");
						// 	}
						// else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					} else {
						$result = "Error: Invalid string\n";
					}
				} else {
					$result = "Error: String does not start with expected prefix\n";
				}
			} else {
				if ($row['status']=='F') {
					$result = "The task being finalised was already marked finalised, which was unexpected\n";
				} else {
					$result = "The task being finalised was found to have status ".$row['status']. ", which was unexpected\n";
				}
			}
		} else {
			$result = "Error: No match to id=$id, access=$access for the task being finalised. (It may have already been unexpectedly finalised.)\n";
		}
		
		// $pdo->exec("UNLOCK TABLES");
		$pdo->commit();
		// $mysqli->real_query("UNLOCK TABLES");
		// $mysqli->close();
		return $result;
	} catch (Exception $e) {
		$pdo->rollback();
		handlePDOError($e);
	}
}

//	Function to create a task split from an existing one, with a specified prefix and branch
//
//	Returns: "OK"
//	or "Error: ... "

function splitTask($id, $access, $new_pref, $branchOrder) {
// global $host, $user_name, $pwd, $dbase, $A_LO, $A_HI;
	global $A_LO, $A_HI, $pdo;

// $mysqli = new mysqli($host, $user_name, $pwd, $dbase);
// if ($mysqli->connect_errno)
// 	{
// 	return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
// 	}
// else
// 	{

	try {
		// if (!$mysqli->real_query("LOCK TABLES tasks WRITE, workers WRITE, witness_strings READ"))
		// 	{
		// 	$mysqli->close();
		// 	return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		// 	};

		$pdo->beginTransaction();

		$res = $pdo->prepare("SELECT * FROM tasks WHERE id=? AND access=? FOR UPDATE");
		$res->execute([$id, $access]);
		// $res = $mysqli->query("SELECT * FROM tasks WHERE id=$id AND access=$access FOR UPDATE");
		
		// if ($res->num_rows==1)
		if ($row = $res->fetch()) {
			// $res->data_seek(0);
			// $row = $res->fetch_assoc();
			
			//	Check that task is still active
			if ($row['status'] == 'A') {
				$pref = $row['prefix'];
				$pref_len = strlen($pref);
				$n_str = $row['n'];
				$n = intval($n_str);
				$cid = intval($row['client_id']);
					
				//	Check that the new prefix extends the old one

				if (substr($new_pref,0,$pref_len) == $pref) {
					
					//	See if we have a higher permutation in the witness_strings database, to supersede the original task's perm_to_exceed
					
					$w = $row['waste'];
					$pte_str = $row['perm_to_exceed'];
					$pte = intval($pte_str);

					$res = $pdo->prepare("SELECT perms FROM witness_strings WHERE n=? AND waste=?");
					$res->execute([$n, $w]);

					// $resW = $mysqli->query("SELECT perms FROM witness_strings WHERE n=$n AND waste=$w");)
					// if ($resW->num_rows==1)
					if ($rowW = $res->fetch()) {
						// $resW->data_seek(0);
						// $rowW = $resW->fetch_array();
						$pw_str = $rowW[0];
						$pw = intval($pw_str);
						if ($pw > $pte) $pte = $pw;
					}
					
					//	Base the new task on the old one
					
					$new_access = mt_rand($A_LO,$A_HI);
					$fieldList = "";
					// $valuesList = "";
					$valuesList = array();
					$c = 0;
					
					reset($row);
					
					for ($j=0; $j < count($row); $j++) {
						$field = key($row);
						$value = current($row);
						if ($field=='access') $value = $new_access;
						else if ($field=='prefix') $value = $new_pref; 
						else if ($field=='perm_to_exceed') $value = $pte; 
						else if ($field=='ts_allocated') $value = 'NOW()';
						else if ($field=='status') $value = 'U';
						else if ($field=='branch_order') $value = $branchOrder;
						else $value = $value;
						
						if ($field != 'id' && $field != 'ts' && $field != 'ts_finished' && $field != 'ts_allocated' && $field != 'checkin_count' && $field != 'client_id') {
							$pre = ($c==0) ? "": ", ";
							$fieldList = $fieldList . $pre . $field;
							// $valuesList = $valuesList . $pre . $value;
							array_push($valuesList, $value);
							$c++;
						}

						next($row);
					}
						
					$res = $pdo->prepare("INSERT INTO tasks (" . $fieldList .") VALUES( " . implode(",", array_fill(0, count($valuesList), "?")) .")");
					$res->execute($valuesList);

					$res = $pdo->prepare("UPDATE tasks SET checkin_count=checkin_count+1 WHERE id=? AND access=?");
					$res->execute([$id, $access]);

					if ($cid != 0) {
						$res = $pdo->prepare("UPDATE workers SET checkin_count=checkin_count+1 WHERE id=?");
						$res->execute([$cid]);
					}

					$result = "OK\n";
					// if ($mysqli->real_query("INSERT INTO tasks (" . $fieldList .") VALUES( " . $valuesList .")")
					// && $mysqli->real_query("UPDATE tasks SET checkin_count=checkin_count+1 WHERE id=$id AND access=$access")
					// && ($cid==0 || $mysqli->real_query("UPDATE workers SET checkin_count=checkin_count+1 WHERE id=$cid"))) $result = "OK\n";
					// else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					// }
				} else {
					$result = "Error: Invalid new prefix string $new_pref\n";
				}
			} else {
				if ($row['status']=='F') {
					$result = "Error: The task being split was marked finalised, which was unexpected\n";
				} else {
					$result = "Error: The task being split was found to have status ".$row['status']. ", which was unexpected\n";
				}
			}
		} else {
			$result = "Error: No match to id=$id, access=$access for the task being split\n";
		}
			
		$pdo->commit();
		// $mysqli->real_query("UNLOCK TABLES");
		// $mysqli->close();
		return $result;

	} catch (Exception $e) {
		$pdo->rollback();
		handlePDOError($e);;
	}
}


//	Function to register a worker, using their supplied program instance number and their IP address

function register($pi, $teamName) {
// global $host, $user_name, $pwd, $dbase, $maxClients;
	global $maxClients, $pdo;

	$ra = $_SERVER['REMOTE_ADDR'];
	if (!is_string($ra)) return "Error: Unable to determine connection's IP address\n";

// $mysqli = new mysqli($host, $user_name, $pwd, $dbase);
// if ($mysqli->connect_errno)
// 	{
// 	return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
// 	}
// else
// 	{

	try {
		// if (!$mysqli->real_query("LOCK TABLES workers WRITE"))
		// 	{
		// 	$mysqli->close();
		// 	return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		// 	};

		$pdo->beginTransaction();
		$res = $pdo->query("SELECT COUNT(id) FROM workers");
		// $res = $mysqli->query("SELECT COUNT(id) FROM workers");
		
		$ok = TRUE;
		// if ($res->num_rows!=0)
		if ($row = $res->fetch(PDO::FETCH_NUM)) {
			// $res->data_seek(0);
			// $row = $res->fetch_array();
			$nw = intval($row[0]);
			if ($nw >= $maxClients) {
				$result = "Error: Thanks for offering to join the project, but unfortunately the server is at capacity right now ($nw out of $maxClients), and cannot accept any more clients. We will continue to increase capacity, so please check back soon!\n";
				$ok = FALSE;
			}
		}
		
		if ($ok) {
			$res = $pdo->prepare("INSERT INTO workers (IP,instance_num,ts_registered,team) VALUES(?, ?, NOW(), ?)");
			$res->execute([$ra, $pi, $teamName]);

			// if (!$mysqli->real_query("INSERT INTO workers (IP,instance_num,ts_registered) VALUES('$ra', $pi, NOW())"))
				// $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
			// else
				// {
			$result = "Registered\nClient id: " . $pdo->lastInsertId() . "\nIP: $ra\nprogramInstance: $pi\nteam name: $teamName\n";
				// };
		}
		
		$pdo->commit();
		// $mysqli->real_query("UNLOCK TABLES");
		// $mysqli->close();
		return $result;

	} catch (Exception $e) {
		$pdo->rollback();
		handlePDOError($e);;
	}
}


//	Function to unregister a worker, using their supplied program instance number, client ID and IP address

function unregister($cid,$ip,$pi)
{
// global $host, $user_name, $pwd, $dbase;
	global $pdo;

// $mysqli = new mysqli($host, $user_name, $pwd, $dbase);
// if ($mysqli->connect_errno)
	// {
	// return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	// }
// else
	// {
	// if (!$mysqli->real_query("LOCK TABLES workers WRITE"))
		// {
		// $mysqli->close();
		// return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		// };
		
	try {
		$pdo->beginTransaction();
		$res = $pdo->prepare("DELETE FROM workers WHERE id=? AND instance_num=? AND IP=?");
		$res->execute([$cid, $pi, $ip]);

		// if (!$mysqli->real_query("DELETE FROM workers WHERE id=$cid AND instance_num=$pi AND IP='$ip'"))
		// 	$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		// else
		// 	{
		$result = "OK, client record deleted\n";
		// } 
	
		$pdo->commit();
		// $mysqli->real_query("UNLOCK TABLES");
		// $mysqli->close();
		return $result;
	} catch (Exception $e) {
		$pdo->rollback();
		handlePDOError($e);;
	}
}

//	Process query string
//	====================

$queryOK = FALSE;
$err = 'Invalid query';
$qs = $_SERVER['QUERY_STRING'];

//	Currently not using instanceCount

/*
$didUpdate=FALSE;
$ic=instanceCount(2,2,$didUpdate);
*/
//	We set $ic to zero to say "OK to run, we don't care how many instances of the script are running

$ic = 0;

if (is_string($qs))
	{
	parse_str($qs, $q);
	
	//	Validate query string arguments
	
	reset($q);

	$ok = TRUE;
	for ($i=0; $i<count($q); $i++)
		{
		$k = key($q);
		$v = current($q);
		next($q);
		if ($k != 'action' && $k != 'pwd' && $k != 'team' && !checkString($v))
			{
			$ok=FALSE;
			break;
			}
		}
	
	if ($ok)
	{
	$version = intval($q['version']);
	if ($version < $versionAbsolutelyRequired)
		$err = "The version of DistributedChaffinMethod you are using has been superseded.\nPlease download version $versionAbsolutelyRequired or later from https://github.com/superpermutators/superperm/blob/master/DistributedChaffinMethod/DistributedChaffinMethod.c\nThanks for being part of this project!";
	else
		{
		if ($version >= 7 && $ic != 0)
			{
			$queryOK = TRUE;
			echo "Wait\n";
			}
		else
			{
			$action = $q['action'];
			
			if (is_string($action))
				{
				if ($action == "hello")
					{
					$queryOK = TRUE;
					echo "Hello world.\n";
					}
				else if ($action == "register")
					{
					if ($version < $versionForNewTasks)
						$err = "The version of DistributedChaffinMethod you are using has been superseded.\nPlease download version $versionForNewTasks or later from https://github.com/superpermutators/superperm/blob/master/DistributedChaffinMethod/DistributedChaffinMethod.c\nThanks for being part of this project!";
					else
						{
								$pi = $q['programInstance'];
								$teamName = $q['team'];
								if ($teamName == "") $teamName = "anonymous";
								if (is_string($pi) && is_string($teamName))
									{
									$queryOK = TRUE;
									echo register($pi, $teamName);
									}
						}
					}
				else if ($action == "getTask")
					{
					$pi = $q['programInstance'];
					$cid = $q['clientID'];
					$ip = $q['IP'];
					$teamName = $q['team'];
					if ($teamName == "") $teamName = "anonymous";
					if (is_string($pi) && is_string($cid) && is_string($ip) && is_string($teamName))
						{
						if ($version < $versionForNewTasks)
							{
							unregister($cid,$ip,$pi);
							$err = "The version of DistributedChaffinMethod you are using has been superseded.\nPlease download version $versionForNewTasks or later from https://github.com/superpermutators/superperm/blob/master/DistributedChaffinMethod/DistributedChaffinMethod.c\nThanks for being part of this project!";
							}
						else
							{
							$queryOK = TRUE;
							echo getTask($cid,$ip,$pi,$version,$teamName);
							}
						}
					}
				else if ($action == "unregister")
					{
					$pi = $q['programInstance'];
					$cid = $q['clientID'];
					$ip = $q['IP'];
					if (is_string($pi) && is_string($cid) && is_string($ip))
						{
						$queryOK = TRUE;
						echo unregister($cid,$ip,$pi);
						}
					}
				else if ($action == "splitTask")
					{
					$id = $q['id'];
					$access = $q['access'];
					$new_pref = $q['newPrefix'];
					$branchOrder = $q['branchOrder'];
					if (is_string($id) && is_string($access) && is_string($new_pref) && is_string($branchOrder))
						{
						$queryOK = TRUE;
						echo splitTask($id, $access, $new_pref, $branchOrder);
						}
					}
				else if ($action == "cancelStalledTasks")
					{
					$maxMins_str = $q['maxMins'];
					$maxMins = intval($maxMins_str);
					if (is_string($maxMins_str) && $maxMins>0 && $pwd==$q['pwd'])
						{
						$queryOK = TRUE;
						echo cancelStalledTasks($maxMins);
						}
					}
				else if ($action == "cancelStalledClients")
					{
					$maxMins_str = $q['maxMins'];
					$maxMins = intval($maxMins_str);
					if (is_string($maxMins_str) && $maxMins>0 && $pwd==$q['pwd'])
						{
						$queryOK = TRUE;
						echo cancelStalledClients($maxMins);
						}
					}
				else if ($action == "finishTask")
					{
					$id = $q['id'];
					$access = $q['access'];
					$pro_str = $q['pro'];
					$str = $q['str'];
					$teamName = $q['team'];
					if ($teamName == "") $teamName = "anonymous";
					if (is_string($id) && is_string($access) && is_string($pro_str) && is_string($str) && is_string($teamName))
						{
						$pro = intval($pro_str);
						if ($pro > 0)
							{
							$queryOK = TRUE;
							echo finishTask($id, $access, $pro, $str, $teamName);
							}
						}
					}
				else
					{
					$n_str = $q['n'];
					$w_str = $q['w'];
					if (is_string($n_str) && is_string($w_str))
						{
						$n = intval($n_str);
						$w = intval($w_str);
						if ($n >= $min_n && $n <= $max_n && $w >= 0)
							{
							//	"checkMax" action checks in and queries current maximum permutation count.
							//
							//	Returns:  (n, w, p) for current maximum p, or "Error: ... "
							
							if ($action == "checkMax")
								{
								$id = $q['id'];
								$access = $q['access'];
								$cid = $q['clientID'];
								$ip = $q['IP'];
								$pi = $q['programInstance'];
								if (is_string($id) && is_string($access) && is_string($pi) && is_string($cid) && is_string($ip))
									{
									$queryOK = TRUE;
									echo checkMax($id, $access, $cid, $ip, $pi, $n, $w);
									}
								}
							else
								{
								$str = $q['str'];
								if (is_string($str))
									{
									//	"witnessString" action verifies and possibly records a witness to a certain number of distinct permutations being
									//	visited by a string with a certain number of wasted characters.
									//
									//	Query arguments: n, w, str.
									//
									//	Returns:  "Valid string ... " / (n, w, p) for current maximum p, or "Error: ... "
									
									if ($action == "witnessString")
										{
										$p = analyseString($str,$n);
										$slen = strlen($str);
										$p0 = $slen - $w - $n + 1;
										if ($p == $p0)
											{
											$queryOK = TRUE;
											// echo "Valid string $str with $p permutations\n";
											$pp = strpos($qs,'pro');
											if ($pp===FALSE) $pro = -1;
											else
												{
												$pro_str = $q['pro'];
												if (is_string($pro_str)) $pro = intval($pro_str);
												else $pro = -1;
												}
											$teamName = $q['team'];
											if ($teamName == "") $teamName = "anonymous";
											echo maybeUpdateWitnessStrings($n, $w, $p, $str, $pro, $teamName);
											}
										else if ($p<0) $err = 'Invalid string';
										else $err = "Unexpected permutation count [$p permutations, expected $p0 for w=$w, length=$slen]";
										}
										
									//	"createTask" action puts a new task into the tasks database.
									//
									//	Query arguments: n, w, str, pte, pwd.
									//
									//	where "pte" is the number of permutations to exceed in any strings the task finds.
									//
									//	Returns: "Task id: ... " or "Error: ... "
									
									else if ($action == "createTask")
										{
										$p_str = $q['pte'];
										if (is_string($p_str) && $pwd==$q['pwd'])
											{
											$p = intval($p_str);
											if ($p > 0 && analyseString($str,$n) > 0)
												{
												$queryOK = TRUE;
												echo makeTask($n, $w, $p, $str);
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
	}

if (!$queryOK) echo "Error: $err \n";

//	Currently not using instanceCount

//	if ($didUpdate) instanceCount(-2,0);

?>
