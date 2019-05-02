<?php
include 'ink2.php';

//	Version of the client ABSOLUTELY required.
//  Note that if this is changed while clients are running tasks,
//	those tasks will be disrupted and will need to be cancelled and reallocated
//	with a call to cancelStalledTasks, and the disrupted clients will need to be unregistered
//	with a call to cancelStalledClients.

$versionAbsolutelyRequired = 6;

//	Version of the client required for new registrations and new tasks to be allocated.
//	If this is changed while clients are running tasks, the task will continue uninterrupted;
//	the client will be unregistered and will exit cleanly the next time it asks for a new task.

$versionForNewTasks = 6;

//	Maximum number of clients to register

$maxClients = 48;

//	Valid range for $n

$min_n = 3;
$max_n = 7;

//	Range for access codes

$A_LO = 100000000;
$A_HI = 999999999;

//	Record number of instances of this script

function instanceCount($inc,$def)
{
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
	for ($i=0; $i<3; $i++)
		{
		if (flock($fp, LOCK_EX))	// acquire an exclusive lock
			{
			$ic = fgets($fp);
			$id=intval($ic);
			$iq = intval($ic)+$inc;
			fseek($fp,0,SEEK_SET);
			fwrite($fp, $iq<10?"0$iq":"$iq");
			fflush($fp);            // flush output before releasing the lock
			flock($fp, LOCK_UN);    // release the lock
			break;
			};
		sleep(1);
		}
	fclose($fp);
	};
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

function maybeUpdateWitnessStrings($n, $w, $p, $str, $pro)
{
if ($pro > 0 && $pro <= $p) return "Error: Trying to set permutations ruled out to $pro while exhibiting a string with $p permutations\n";

global $host, $user_name, $pwd, $dbase;
$mysqli = new mysqli($host, $user_name, $pwd, $dbase);
if ($mysqli->connect_errno)
	{
	return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	}
else
	{
	if (!$mysqli->real_query("LOCK TABLES witness_strings " . ($p>=0 ? "WRITE" : "READ")))
		{
		$mysqli->close();
		return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		};
	$res = $mysqli->query("SELECT perms FROM witness_strings WHERE n=$n AND waste=$w" . ($p>=0 ? " FOR UPDATE" : ""));
	if ($mysqli->errno) $result = "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
	else
		{
		if ($res->num_rows == 0)
			{
			//	No data at all for this (n,w) pair
			
			if ($p >= 0)
				{
				
				//	Try to update the database
				
				if ($pro > 0)
					{
					$final = ($pro == $p+1) ? "Y" : "N";
					if ($mysqli->real_query("INSERT INTO witness_strings (n,waste,perms,str,excl_perms,final) VALUES($n, $w, $p, '$str', $pro, '$final')"))
						$result = "($n, $w, $p)\n";
					else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					}
				else
					{
					if ($mysqli->real_query("INSERT INTO witness_strings (n,waste,perms,str) VALUES($n, $w, $p, '$str')"))
						$result = "($n, $w, $p)\n";
					else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					};
				}
				
				//	If we are just querying, return -1 in lieu of maximum
				
			else $result = "($n, $w, -1)\n";
			}
		else
			{
			//	There is existing data for this (n,w) pair, so check to see if we have a greater permutation count
			
			$res->data_seek(0);
			$row = $res->fetch_array();
			$p0 = intval($row[0]);
			
			if ($p > $p0)
				{
				//	Our new data has a greater permutation count, so update the entry
			
				if ($pro > 0)
					{
					$final = ($pro == $p+1) ? "Y" : "N";
					if ($mysqli->real_query("REPLACE INTO witness_strings (n,waste,perms,str,excl_perms,final) VALUES($n, $w, $p, '$str', $pro, '$final')"))
						$result = "($n, $w, $p)\n";
					else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					}
				else
					{
					if ($mysqli->real_query("REPLACE INTO witness_strings (n,waste,perms,str) VALUES($n, $w, $p, '$str')"))
						$result = "($n, $w, $p)\n";
					else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					};
				}
			else
				{
				//	Our new data does not have a greater permutation count (or might just be a query with $p=-1), so return existing maximum
				
				$result = "($n, $w, $p0)\n";
				};
			};
		};
	
	$mysqli->real_query("UNLOCK TABLES");
	$mysqli->close();
	return $result;
	};
}

//	Function to make a new task record
//
//	Returns "Task id: ... " or "Error: ... "

function makeTask($n, $w, $pte, $str)
{
global $host, $user_name, $pwd, $dbase, $A_LO, $A_HI;

$mysqli = new mysqli($host, $user_name, $pwd, $dbase);
if ($mysqli->connect_errno)
	{
	return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	}
else
	{
	if (!$mysqli->real_query("LOCK TABLES tasks WRITE"))
		{
		$mysqli->close();
		return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		};
		
	$res = $mysqli->query("SELECT id FROM tasks WHERE n=$n AND waste=$w AND prefix='$str' AND perm_to_exceed=$pte");
	if ($mysqli->errno) $result = "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
	else if ($res->num_rows!=0)
		{
		$res->data_seek(0);
		$row = $res->fetch_array();
		$id = $row[0];
		$result = "Task id: $id already existed with those properties\n";
		}
	else
		{
		$access = mt_rand($A_LO,$A_HI);
		$br = substr("000000000",0,$n);
		if ($mysqli->real_query("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,branch_order) VALUES($access, $n, $w, '$str', $pte,'$br')"))
			$result = "Task id: $mysqli->insert_id\n";
		else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		};
	
	$mysqli->real_query("UNLOCK TABLES");
	$mysqli->close();
	return $result;
	};
}

//	Function to allocate an unallocated task, if there is one.
//
//	Returns:	"Task id: ... " / "Access code:" /"n: ..." / "w: ..." / "str: ... " / "pte: ... " / "pro: ... " / "branchOrder: ..."
//	then all finalised (w,p) pairs
//	or:			"No tasks"
//	or:			"Error ... "

function getTask($cid,$ip,$pi)
{
global $host, $user_name, $pwd, $dbase;
$mysqli = new mysqli($host, $user_name, $pwd, $dbase);
if ($mysqli->connect_errno)
	{
	return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	}
else
	{
	if (!$mysqli->real_query("LOCK TABLES tasks WRITE, witness_strings READ, workers WRITE"))
		{
		$mysqli->close();
		return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		};
		
	$wres = $mysqli->query("SELECT * FROM workers WHERE id=$cid AND instance_num=$pi AND IP='$ip' FOR UPDATE");
	if ($mysqli->errno) $result = "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
	else
		{
		if ($wres->num_rows == 0) $result = "Error: No client found with those details\n";
		else
			{
			if (!$mysqli->real_query("UPDATE workers SET checkin_count=checkin_count+1 WHERE id=$cid"))
				 $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
			else
				{
				$res = $mysqli->query("SELECT * FROM tasks WHERE status='U' ORDER BY branch_order LIMIT 1 FOR UPDATE");
				if ($mysqli->errno) $result = "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
				else
					{
					if ($res->num_rows == 0) $result = "No tasks\n";
					else
						{
						$res->data_seek(0);
						$row = $res->fetch_assoc();
						$id = $row['id'];
						$access = $row['access'];
						$n = $row['n'];
						$w = $row['waste'];
	
						$res0 = $mysqli->query("SELECT perms FROM witness_strings WHERE n=$n AND waste=$w");
						if (!$mysqli->errno && $res0->num_rows == 1)
							{
							$res0->data_seek(0);
							$row0 = $res0->fetch_array();
							$p0 = intval($row0[0]);
							}
						else $p0 = -1;
	
						$str = $row['prefix'];
						$pte = intval($row['perm_to_exceed']);
						if ($p0 > $pte) $pte=$p0;
						
						$ppro = $row['prev_perm_ruled_out'];
						$br = $row['branch_order'];
						if (is_string($id) && is_string($access) && is_string($n) && is_string($w) && is_string($str) && is_string($ppro) && is_string($br))
							{
							if ($mysqli->real_query("UPDATE tasks SET status='A', ts_allocated=NOW(), client_id=$cid WHERE id=$id") &&
								$mysqli->real_query("UPDATE workers SET current_task=$id WHERE id=$cid"))
								{
								$result = "Task id: $id\nAccess code: $access\nn: $n\nw: $w\nstr: $str\npte: $pte\npro: $ppro\nbranchOrder: $br\n";
								
								//	Output all finalised (w,p) pairs
								
								$res2 = $mysqli->query("SELECT waste, perms FROM witness_strings WHERE n=$n AND final='Y' ORDER BY waste ASC");
								if ($mysqli->errno) $result = "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
								else
									{
									for ($row_no = 0; $row_no < $res2->num_rows; $row_no++)
										{
										$res2->data_seek($row_no);
										$row = $res2->fetch_array();
										$result = $result . "(" . $row[0] . "," . $row[1] . ")\n";
										};
									};
								}
							else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
							}
						else $result = "Error: Unable to find expected fields in database\n";
						};
					};
				};
			};
		};
	$mysqli->real_query("UNLOCK TABLES");
	$mysqli->close();
	return $result;
	};
}

//	Function to increment the checkin_count of a specified task and return the current maximum permutation for (n,w)

function checkMax($id, $access, $cid, $ip, $pi, $n, $w)
{
global $host, $user_name, $pwd, $dbase;
$mysqli = new mysqli($host, $user_name, $pwd, $dbase);
if ($mysqli->connect_errno)
	{
	return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	}
else
	{
	if (!$mysqli->real_query("LOCK TABLES tasks WRITE, workers WRITE, witness_strings READ"))
		{
		$mysqli->close();
		return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		};
	$res = $mysqli->query("SELECT status FROM tasks WHERE id=$id AND access=$access FOR UPDATE");
	if ($res->num_rows==1)
		{
		$res->data_seek(0);
		$row = $res->fetch_array();
		if ($row[0]=='A')
			{
			if ($mysqli->real_query("UPDATE tasks SET checkin_count=checkin_count+1 WHERE id=$id AND access=$access") &&
				$mysqli->real_query("UPDATE workers SET checkin_count=checkin_count+1 WHERE id=$cid AND instance_num=$pi AND IP='$ip'"))
				{
				$res = $mysqli->query("SELECT perms FROM witness_strings WHERE n=$n AND waste=$w");
				if ($mysqli->errno) $result = "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
				else
					{
					if ($res->num_rows == 0)
						{
						//	No data at all for this (n,w) pair
						
						$result = "($n, $w, -1)\n";
						}
					else
						{
						//	There is existing data for this (n,w) pair, so check to see if we have a greater permutation count
						
						$res->data_seek(0);
						$row = $res->fetch_array();
						$p0 = intval($row[0]);
						
						$result = "($n, $w, $p0)\n";
						};
					};
				}
			else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
			}
		else if ($row[0]=='F') $result = "Error: The task checking in was marked finalised, which was unexpected\n";
		else $result = "Error: The task checking in was found to have status ".$row[0]. ", which was unexpected\n";
		}
	else
		{
		if ($mysqli->errno)	$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		else $result = "Error: No match to id=$id, access=$access for the task checking in\n";
		};
		
	$mysqli->real_query("UNLOCK TABLES");
	$mysqli->close();
	return $result;
	};
}

//	Function to cancel stalled tasks
//
//	Returns some stats about assigned task times since checkin, or "Error: ..."

function cancelStalledTasks($maxMin)
{
global $host, $user_name, $pwd, $dbase, $A_LO, $A_HI;

$mysqli = new mysqli($host, $user_name, $pwd, $dbase);
if ($mysqli->connect_errno)
	{
	return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	}
else
	{
	if (!$mysqli->real_query("LOCK TABLES tasks WRITE, workers WRITE"))
		{
		$mysqli->close();
		return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		};
	$res = $mysqli->query("SELECT id, TIMESTAMPDIFF(MINUTE,ts,NOW()), client_id FROM tasks WHERE status='A' FOR UPDATE");
	if ($mysqli->errno)	$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
	else
		{
		if ($res->num_rows>0)
			{
			$result = "";
			$nass = $res->num_rows;
			$cancelled = 0;
			$maxStall = 0;
			for ($i=0;$i<$nass;$i++)
				{
				$res->data_seek($i);
				$row = $res->fetch_array();
				$stall = intval($row[1]);
				if ($stall > $maxStall) $maxStall = $stall;
				
				if ($stall > $maxMin)
					{
					$id = $row[0];
					$cid = $row[2];
					$access = mt_rand($A_LO,$A_HI);

					if (!($mysqli->real_query("UPDATE tasks SET status='U', access=$access WHERE id=$id") &&
						$mysqli->real_query("UPDATE workers SET current_task=0 WHERE id=$cid")))
						{
						$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
						break;
						};
					
					$cancelled++;
					};
				};
			$result = $result . "$nass assigned tasks, maximum stalled time = $maxStall minutes, cancelled $cancelled tasks\n";
			}
		else $result = "0 assigned tasks\n";
		};
	
	$mysqli->real_query("UNLOCK TABLES");
	$mysqli->close();
	return $result;
	};
}

//	Function to cancel stalled clients
//
//	Returns some stats about assigned times since checkin, or "Error: ..."

function cancelStalledClients($maxMin)
{
global $host, $user_name, $pwd, $dbase;

$mysqli = new mysqli($host, $user_name, $pwd, $dbase);
if ($mysqli->connect_errno)
	{
	return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	}
else
	{
	if (!$mysqli->real_query("LOCK TABLES workers WRITE"))
		{
		$mysqli->close();
		return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		};
	$res = $mysqli->query("SELECT id, TIMESTAMPDIFF(MINUTE,ts,NOW()) FROM workers FOR UPDATE");
	if ($mysqli->errno)	$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
	else
		{
		if ($res->num_rows>0)
			{
			$result = "";
			$nreg = $res->num_rows;
			$cancelled = 0;
			$maxStall = 0;
			for ($i=0;$i<$nreg;$i++)
				{
				$res->data_seek($i);
				$row = $res->fetch_array();
				$stall = intval($row[1]);
				if ($stall > $maxStall) $maxStall = $stall;
				
				if ($stall > $maxMin)
					{
					$id = $row[0];
					if (!$mysqli->real_query("DELETE FROM workers WHERE id=$id"))
						{
						$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
						break;
						};
					$cancelled++;
					};
				};
			$result = $result . "$nreg registered clients, maximum stalled time = $maxStall minutes, cancelled $cancelled clients\n";
			}
		else $result = "0 registered clients\n";
		};
	
	$mysqli->real_query("UNLOCK TABLES");
	$mysqli->close();
	return $result;
	};
}

function factorial($n)
{
if ($n==1) return 1;
return $n*factorial($n-1);
}

//	Function to do further processing if we have finished all tasks for the current (n,w,iter) search

function finishedAllTasks($n, $w, $iter, $mysqli)
{
global $A_LO, $A_HI;

//	Find the highest value of perm_ruled_out from all tasks for this (n,w,iter); no strings were found with this number of perms or
//	higher, across the whole search.

$res = $mysqli->query("SELECT MAX(perm_ruled_out) FROM tasks WHERE n=$n AND waste=$w AND iteration=$iter AND status='F'");
if ($mysqli->errno) return "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";$res->data_seek(0);

$row = $res->fetch_array();
$pro = intval($row[0]);
$res->close();

//	Was any string found for this search?

$res = $mysqli->query("SELECT perms, excl_perms FROM witness_strings WHERE n=$n AND waste=$w FOR UPDATE");
if ($mysqli->errno) return "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";

if ($res->num_rows > 0)
	{
	//	Yes.  Update the excl_perms and final fields.
	
	$res->data_seek(0);
	$row = $res->fetch_array();
	$p_str = $row[0];
	$p = intval($p_str);
	$pro0_str = $row[1];
	$pro0 = intval($pro0_str);
	
	if ($pro < $pro0)
		{
		$final = ($pro == $p+1) ? "Y" : "N";
		if (!$mysqli->real_query("UPDATE witness_strings SET excl_perms=$pro, final='$final' WHERE n=$n AND waste=$w"))
			return "Error: Unable to update database (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		};
		
	//	Maybe create a new task for a higher w value
	
	$fn = factorial($n);
	if ($p < $fn)
		{
		$w1 = $w+1;
		$str = substr("123456789",0,$n);
		$br = substr("000000000",0,$n);
		$pte = $p + 2*($n-4);
		if ($pte >= $fn) $pte = $fn-1;
		$pro2 = $p+$n+1;
		$access = mt_rand($A_LO,$A_HI);
		if ($mysqli->real_query("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,prev_perm_ruled_out,branch_order) VALUES($access, $n, $w1, '$str', $pte, $pro2,'$br')"))
			return "OK\nTask id: $mysqli->insert_id\n";
		else return "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		}
	else return "OK\n";
	}
else
	{
	//	The search came up with nothing, so we need to backtrack and search for a lower perm_to_exceed
	
	$res = $mysqli->query("SELECT MIN(perm_to_exceed) FROM tasks WHERE n=$n AND waste=$w AND iteration=$iter AND status='F'");
	if ($mysqli->errno) return "Error: Unable to read database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
	$res->data_seek(0);
	$row = $res->fetch_array();
	$pte = intval($row[0])-1;
	$res->close();
	
	$str = substr("123456789",0,$n);
	$br = substr("000000000",0,$n);
	$access = mt_rand($A_LO,$A_HI);
	$iter1 = $iter+1;
	if ($mysqli->real_query("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,iteration,prev_perm_ruled_out,branch_order) VALUES($access, $n, $w, '$str', $pte, $iter1, $pro,'$br')"))
		return "OK\nTask id: $mysqli->insert_id\n";
	else return "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
	};
}

//	Function to mark a task as finished, and if all tasks are finished do some further processing
//
//	Returns: "OK ..." or "Error: ... "

function finishTask($id, $access, $pro, $str)
{
global $host, $user_name, $pwd, $dbase;
$mysqli = new mysqli($host, $user_name, $pwd, $dbase);
if ($mysqli->connect_errno)
	{
	return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	}
else
	{
	if (!$mysqli->real_query("LOCK TABLES tasks WRITE, witness_strings WRITE, workers WRITE"))
		{
		$mysqli->close();
		return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		};
	$res = $mysqli->query("SELECT * FROM tasks WHERE id=$id AND access=$access FOR UPDATE");
	if ($res->num_rows==1)
		{
		$res->data_seek(0);
		$row = $res->fetch_assoc();
		
		//	Check that task is still active
		
		if ($row['status']=='A')
			{
			//	Check that the exclusion string starts with the expected prefix.
			
			$pref = $row['prefix'];
			$pref_len = strlen($pref);
			if (substr($str,0,$pref_len)==$pref)
				{
				$n_str = $row['n'];
				$n = intval($n_str);
				$w_str = $row['waste'];
				$w = intval($w_str);
				$iter_str = $row['iteration'];
				$iter = intval($iter_str);
					
				//	Check that the exclusion string is valid
				
				if (analyseString($str,$n) > 0)
					{
					//	See if we have found a string that makes other searches redundant
										
					$ppro = intval($row['prev_perm_ruled_out']);
					if ($ppro>0 && $pro >= $ppro)
						$qry = "UPDATE tasks SET status='F', ts_finished=NOW(), perm_ruled_out=$pro, excl_witness='Redundant' WHERE n=$n AND waste=$w AND iteration=$iter AND status='U'";
					else $qry = "UPDATE tasks SET status='F', ts_finished=NOW(), perm_ruled_out=$pro, excl_witness='$str' WHERE id=$id AND access=$access";
					
					if ($mysqli->real_query($qry))
						{
						$res2 = $mysqli->query("SELECT id FROM tasks WHERE n=$n AND waste=$w AND iteration=$iter AND (status='A' OR status='U') LIMIT 1");
						if ($res2->num_rows==0) $result = finishedAllTasks($n, $w, $iter, $mysqli);
						else $result = "OK\n";

						//	Remove this as current task for client
						
						$cid = $row['client_id'];
						if (intval($cid)>0) $mysqli->real_query("UPDATE workers SET current_task=0 WHERE id=$cid");
						}
					else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
					}
				else $result = "Error: Invalid string\n";
				}
			else $result = "Error: String does not start with expected prefix\n";
			}
		else if ($row['status']=='F') $result = "The task being finalised was already marked finalised, which was unexpected\n";
		else $result = "The task being finalised was found to have status ".$row['status']. ", which was unexpected\n";
		}
	else
		{
		if ($mysqli->errno)	$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		else $result = "Error: No match to id=$id, access=$access for the task being finalised\n";
		};
		
	$mysqli->real_query("UNLOCK TABLES");
	$mysqli->close();
	return $result;
	};
}

//	Function to create a task split from an existing one, with a specified prefix and branch
//
//	Returns: "OK"
//	or "Error: ... "

function splitTask($id, $access, $new_pref, $branchOrder)
{
global $host, $user_name, $pwd, $dbase, $A_LO, $A_HI;
$mysqli = new mysqli($host, $user_name, $pwd, $dbase);
if ($mysqli->connect_errno)
	{
	return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	}
else
	{
	if (!$mysqli->real_query("LOCK TABLES tasks WRITE, witness_strings READ"))
		{
		$mysqli->close();
		return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		};
	$res = $mysqli->query("SELECT * FROM tasks WHERE id=$id AND access=$access FOR UPDATE");
	if ($res->num_rows==1)
		{
		$res->data_seek(0);
		$row = $res->fetch_assoc();
		
		//	Check that task is still active
		
		if ($row['status']=='A')
			{
			$pref = $row['prefix'];
			$pref_len = strlen($pref);
			$n_str = $row['n'];
			$n = intval($n_str);
				
			//	Check that the new prefix extends the old one

			if (substr($new_pref,0,$pref_len)==$pref)
				{
				//	See if we have a higher permutation in the witness_strings database, to supersede the original task's perm_to_exceed
				
				$w = $row['waste'];
				$pte_str = $row['perm_to_exceed'];
				$pte = intval($pte_str);
				$resW = $mysqli->query("SELECT perms FROM witness_strings WHERE n=$n AND waste=$w");
				if ($resW->num_rows==1)
					{
					$resW->data_seek(0);
					$rowW = $resW->fetch_array();
					$pw_str = $rowW[0];
					$pw = intval($pw_str);
					if ($pw > $pte) $pte = $pw;
					};

				//	Base the new task on the old one
				
				$new_access = mt_rand($A_LO,$A_HI);
				$fieldList = "";
				$valuesList = "";
				$c = 0;
				reset($row);
				for ($j=0; $j < count($row); $j++)
					{
					$field = key($row);
					$value = current($row);
					if ($field=='access') $value = $new_access;
					else if ($field=='prefix') $value = "'$new_pref'"; 
					else if ($field=='perm_to_exceed') $value = $pte; 
					else if ($field=='ts_allocated') $value = 'NOW()';
					else if ($field=='status') $value = "'U'";
					else if ($field=='branch_order') $value = "'$branchOrder'";
					else $value = "'$value'";
					
					if ($field != 'id' && $field != 'ts' && $field != 'ts_finished' && $field != 'ts_allocated' && $field != 'checkin_count' && $field != 'client_id')
						{
						$pre = ($c==0) ? "": ", ";
						$fieldList = $fieldList . $pre . $field;
						$valuesList = $valuesList . $pre . $value;
						$c++;
						};
					next($row);
					};
					
				$ok = $mysqli->real_query("INSERT INTO tasks (" . $fieldList .") VALUES( " . $valuesList .")");
				if (!$ok) break;
				
				$result = "OK\n"; 
				}
			else $result = "Error: Invalid new prefix string $new_pref\n";
			}
		else if ($row['status']=='F') $result = "Error: The task being split was marked finalised, which was unexpected\n";
		else $result = "Error: The task being split was found to have status ".$row['status']. ", which was unexpected\n";
		}
	else
		{
		if ($mysqli->errno)	$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		else $result = "Error: No match to id=$id, access=$access for the task being split\n";
		};
		
	$mysqli->real_query("UNLOCK TABLES");
	$mysqli->close();
	return $result;
	};
} 

//	Function to register a worker, using their supplied program instance number and their IP address

function register($pi)
{
global $host, $user_name, $pwd, $dbase, $maxClients;

$ra = $_SERVER['REMOTE_ADDR'];
if (!is_string($ra)) return "Error: Unable to determine connection's IP address\n";

$mysqli = new mysqli($host, $user_name, $pwd, $dbase);
if ($mysqli->connect_errno)
	{
	return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	}
else
	{
	if (!$mysqli->real_query("LOCK TABLES workers WRITE"))
		{
		$mysqli->close();
		return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		};
	$res = $mysqli->query("SELECT COUNT(id) FROM workers");
	
	$ok = TRUE;
	if ($res->num_rows!=0)
		{
		$res->data_seek(0);
		$row = $res->fetch_array();
		$nw = intval($row[0]);
		if ($nw >= $maxClients)
			{
			$result = "Error: Thanks for offering to join the project, but unfortunately the server is at capacity right now ($nw out of $maxClients), and cannot accept any more clients.\n";
			$ok = FALSE;
			};
		};
	
	if ($ok)
		{
		if (!$mysqli->real_query("INSERT INTO workers (IP,instance_num,ts_registered) VALUES('$ra', $pi, NOW())"))
			$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		else
			{
			$result = "Registered\nClient id: $mysqli->insert_id\nIP: $ra\nprogramInstance: $pi\n";
			};
		};
	
	$mysqli->real_query("UNLOCK TABLES");
	$mysqli->close();
	return $result;
	};
}


//	Function to unregister a worker, using their supplied program instance number, client ID and IP address

function unregister($cid,$ip,$pi)
{
global $host, $user_name, $pwd, $dbase;

$mysqli = new mysqli($host, $user_name, $pwd, $dbase);
if ($mysqli->connect_errno)
	{
	return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	}
else
	{
	if (!$mysqli->real_query("LOCK TABLES workers WRITE"))
		{
		$mysqli->close();
		return "Error: Unable to lock database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
		};
		
	if (!$mysqli->real_query("DELETE FROM workers WHERE id=$cid AND instance_num=$pi AND IP='$ip'"))
		$result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
	else
		{
		$result = "OK, client record deleted\n";
		};
	
	$mysqli->real_query("UNLOCK TABLES");
	$mysqli->close();
	return $result;
	};
}

//	Process query string
//	====================

$queryOK = FALSE;
$err = 'Invalid query';
$qs = $_SERVER['QUERY_STRING'];

instanceCount(1,1);

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
		if ($k!='action' && $k!='pwd' && !checkString($v))
			{
			$ok=FALSE;
			break;
			};
		};
	
	if ($ok)
	{
	$version = intval($q['version']);
	if ($version < $versionAbsolutelyRequired)
		$err = "The version of DistributedChaffinMethod you are using has been superseded.\nPlease download version $versionAbsolutelyRequired or later from https://github.com/superpermutators/superperm/blob/master/DistributedChaffinMethod/DistributedChaffinMethod.c\nThanks for being part of this project!";
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
							if (is_string($pi))
								{
								$queryOK = TRUE;
								echo register($pi);
								};
					};
				}
			else if ($action == "getTask")
				{
				$pi = $q['programInstance'];
				$cid = $q['clientID'];
				$ip = $q['IP'];
				if (is_string($pi) && is_string($cid) && is_string($ip))
					{
					if ($version < $versionForNewTasks)
						{
						unregister($cid,$ip,$pi);
						$err = "The version of DistributedChaffinMethod you are using has been superseded.\nPlease download version $versionForNewTasks or later from https://github.com/superpermutators/superperm/blob/master/DistributedChaffinMethod/DistributedChaffinMethod.c\nThanks for being part of this project!";
						}
					else
						{
						$queryOK = TRUE;
						echo getTask($cid,$ip,$pi);
						};
					};
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
					};
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
					};
				}
			else if ($action == "cancelStalledTasks")
				{
				$maxMins_str = $q['maxMins'];
				$maxMins = intval($maxMins_str);
				if (is_string($maxMins_str) && $maxMins>0 && $pwd==$q['pwd'])
					{
					$queryOK = TRUE;
					echo cancelStalledTasks($maxMins);
					};
				}
			else if ($action == "cancelStalledClients")
				{
				$maxMins_str = $q['maxMins'];
				$maxMins = intval($maxMins_str);
				if (is_string($maxMins_str) && $maxMins>0 && $pwd==$q['pwd'])
					{
					$queryOK = TRUE;
					echo cancelStalledClients($maxMins);
					};
				}
			else if ($action == "finishTask")
				{
				$id = $q['id'];
				$access = $q['access'];
				$pro_str = $q['pro'];
				$str = $q['str'];
				if (is_string($id) && is_string($access) && is_string($pro_str) && is_string($str))
					{
					$pro = intval($pro_str);
					if ($pro > 0)
						{
						$queryOK = TRUE;
						echo finishTask($id, $access, $pro, $str);
						};
					};
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
								};
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
										echo "Valid string $str with $p permutations\n";
										$pp = strpos($qs,'pro');
										if ($pp===FALSE) $pro = -1;
										else
											{
											$pro_str = $q['pro'];
											if (is_string($pro_str)) $pro = intval($pro_str);
											else $pro = -1;
											};
										echo maybeUpdateWitnessStrings($n, $w, $p, $str, $pro);
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
											};
										};
									};
								};
							};
						};
					};
				};
			};
		};
	};
	};

if (!$queryOK) echo "Error: $err \n";

instanceCount(-1,0);

?>
