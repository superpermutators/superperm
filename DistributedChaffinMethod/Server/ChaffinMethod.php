<?php
include 'ink2.php';

//	Version of the client required

$versionRequired = 3;

//	Valid range for $n

$min_n = 3;
$max_n = 7;

//	Range for access codes

$A_LO = 100000000;
$A_HI = 999999999;

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
	$access = mt_rand($A_LO,$A_HI);
	if ($mysqli->real_query("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed) VALUES($access, $n, $w, '$str', $pte)"))
		$result = "Task id: $mysqli->insert_id\n";
	else $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
	
	$mysqli->real_query("UNLOCK TABLES");
	$mysqli->close();
	return $result;
	};
}

//	Function to allocate an unallocated task, if there is one.
//
//	Returns:	"Task id: ... " / "Access code:" /"n: ..." / "w: ..." / "str: ... " / "pte: ... " / "pro: ... " / all finalised (w,p) pairs
//	or:			"No tasks"
//	or:			"Error ... "

function getTask()
{
global $host, $user_name, $pwd, $dbase;
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
	$res = $mysqli->query("SELECT * FROM tasks WHERE status='U' LIMIT 1 FOR UPDATE");
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
			$str = $row['prefix'];
			$pte = $row['perm_to_exceed'];
			$ppro = $row['prev_perm_ruled_out'];
			if (is_string($id) && is_string($access) && is_string($n) && is_string($w) && is_string($str) && is_string($pte) && is_string($ppro))
				{
				if ($mysqli->real_query("UPDATE tasks SET status='A', ts_allocated=NOW() WHERE id=$id"))
					{
					$result = "Task id: $id\nAccess code: $access\nn: $n\nw: $w\nstr: $str\npte: $pte\npro: $ppro\n";
					
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
		
	$mysqli->real_query("UNLOCK TABLES");
	$mysqli->close();
	return $result;
	};
}

//	Function to increment the checkin_count of a specified task
//
//	Returns: "OK" or "Error: ... "

function checkIn($id, $access)
{
global $host, $user_name, $pwd, $dbase;
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
	$res = $mysqli->query("SELECT status FROM tasks WHERE id=$id AND access=$access FOR UPDATE");
	if ($res->num_rows==1)
		{
		$res->data_seek(0);
		$row = $res->fetch_array();
		if ($row[0]=='A')
			{
			if ($mysqli->real_query("UPDATE tasks SET checkin_count=checkin_count+1 WHERE id=$id AND access=$access"))
				$result = "OK\n";
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

function cancelStalled($maxMin)
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
	$res = $mysqli->query("SELECT id, TIMESTAMPDIFF(MINUTE,ts,NOW()) FROM tasks WHERE status='A' FOR UPDATE");
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
					$access = mt_rand($A_LO,$A_HI);

					if (!$mysqli->real_query("UPDATE tasks SET status='U', access=$access WHERE id=$id"))
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
		$pte = $p + 2*($n-4);
		if ($pte >= $fn) $pte = $fn-1;
		$pro2 = $p+$n+1;
		$access = mt_rand($A_LO,$A_HI);
		if ($mysqli->real_query("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,prev_perm_ruled_out) VALUES($access, $n, $w1, '$str', $pte, $pro2)"))
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
	$access = mt_rand($A_LO,$A_HI);
	$iter1 = $iter+1;
	if ($mysqli->real_query("INSERT INTO tasks (access,n,waste,prefix,perm_to_exceed,iteration,prev_perm_ruled_out) VALUES($access, $n, $w, '$str', $pte, $iter1, $pro)"))
		return "OK\nTask id: $mysqli->insert_id\n";
	else return "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
	};
}

//	Function to mark a task as finished, and if all tasks are finished do some further processing
//
//	Returns: "OK ..." or "Error: ... "

function finishTask($id, $access, $pro, $str, $splitTask)
{
global $host, $user_name, $pwd, $dbase;
$mysqli = new mysqli($host, $user_name, $pwd, $dbase);
if ($mysqli->connect_errno)
	{
	return "Error: Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "\n";
	}
else
	{
	if (!$mysqli->real_query("LOCK TABLES tasks WRITE, witness_strings WRITE"))
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
			//	We drop that condition if we are finalising a split task.
			
			$pref = $row['prefix'];
			$pref_len = strlen($pref);
			if (substr($str,0,$pref_len)==$pref || $splitTask)
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
					if ($mysqli->real_query("UPDATE tasks SET status='F', ts_finished=NOW(), perm_ruled_out=$pro, excl_witness='$str' WHERE id=$id AND access=$access"))
						{
						$res2 = $mysqli->query("SELECT id FROM tasks WHERE n=$n AND waste=$w AND iteration=$iter AND (status='A' OR status='U') LIMIT 1");
						if ($res2->num_rows==0) $result = finishedAllTasks($n, $w, $iter, $mysqli);
						else $result = "OK\n";
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

//	Function to split a task, with the caller retaining control of certain sub-branches as new tasks
//
//	Returns: "Task id: ..." / "Access code: ..." / "str: ... " for all the retained sub-branches,
//	or "Error: ... "

function splitTask($id, $access, $retain)
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
			$prefEnd = intval(substr($pref,$pref_len-1,1));
			$n_str = $row['n'];
			$n = intval($n_str);
				
			//	Check that the retain string is valid: it should be a list of distinct digits between 1 and n, not including
			//	the last digit of the prefix.
			
			$rlen = strlen($retain);
			$rarr = str_split($retain);
			$ok = ($rlen <= $n-1) && count(array_unique($rarr)) == $rlen;
			if ($ok)
				{
				for ($i=0; $i<$rlen; $i++)
					{
					$d = intval($rarr[$i]);
					if ($d < 1 || $d > $n || $d==$prefEnd)
						{
						$ok=FALSE;
						break;
						};
					};
				};
			if ($ok)
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

				//	Clone the original task, appending each retain-string digit to its prefixes, and assigning new access codes.
				
				$result = "";
				
				//	Add all the non-retained digits to make unassigned tasks
				
				for ($d=1; $d<=$n; $d++)
				if ($d!=$prefEnd && (!in_array("$d",$rarr)))
					{
					$rarr[] = "$d";
					};
				
				for ($i=0; $i<$n-1; $i++)
					{
					$is_retained = $i < $rlen;
					$new_access = mt_rand($A_LO,$A_HI);
					$new_pref = $pref . $rarr[$i];
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
						else if ((!$is_retained) && $field=='status') $value = "'U'";
						else $value = "'$value'";
						
						if ($field != 'id' && $field != 'ts' && $field != 'ts_finished' && ($is_retained || $field != 'ts_allocated') && $field != 'checkin_count')
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
					
					if ($is_retained) $result = $result . "Task id: $mysqli->insert_id\nAccess code: $new_access\nstr: $new_pref\n"; 
					};
					
				if ($ok) $ok = $mysqli->real_query("DELETE FROM tasks WHERE id=$id AND access=$access");
					
				if (!$ok) $result = "Error: Unable to update database: (" . $mysqli->errno . ") " . $mysqli->error . "\n";
				}
			else $result = "Error: Invalid 'retain' string $retain\n";
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


//	Process query string
//	====================

//echo "Quit\nError: This server has become overloaded and the project will need to be relaunched at a later date.\nThanks for participating, and apologies for any disappointment.\nPlease stay tuned to the Superpermutators Google Group.  Hopefully we'll be back at some point\n";

$queryOK = FALSE;
$err = 'Invalid query';
$qs = $_SERVER['QUERY_STRING'];

if (is_string($qs))
	{
	parse_str($qs, $queryOutput);
	
	$version = $queryOutput['version'];
	if ((!is_string($version)) || intval($version) < $versionRequired)
		$err = "The version of DiscreteChaffinMethod you are using has been superseded. Please download version $versionRequired or later. Thanks!";
	else
		{
		$action = $queryOutput['action'];
		
		if (is_string($action))
			{
			if ($action == "hello")
				{
				$queryOK = TRUE;
				echo "Hello world.\n";
				}
			else if ($action == "getTask")
				{
				$queryOK = TRUE;
				echo getTask();
				}
			else if ($action == "checkIn")
				{
				$id = $queryOutput['id'];
				$access = $queryOutput['access'];
				if (is_string($id) && is_string($access))
					{
					$queryOK = TRUE;
					echo checkIn($id, $access);
					};
				}
			else if ($action == "splitTask")
				{
				$id = $queryOutput['id'];
				$access = $queryOutput['access'];
				$retain = $queryOutput['retain'];
				if (is_string($id) && is_string($access) && is_string($retain))
					{
					$queryOK = TRUE;
					echo splitTask($id, $access, $retain);
					};
				}
			else if ($action == "cancelStalled")
				{
				$maxMins_str = $queryOutput['maxMins'];
				$maxMins = intval($maxMins_str);
				if (is_string($maxMins_str) && $maxMins>0)
					{
					$queryOK = TRUE;
					echo cancelStalled($maxMins);
					};
				}
			else if ($action == "finishTask")
				{
				$id = $queryOutput['id'];
				$access = $queryOutput['access'];
				$pro_str = $queryOutput['pro'];
				$str = $queryOutput['str'];
				if (is_string($id) && is_string($access) && is_string($pro_str) && is_string($str))
					{
					$pro = intval($pro_str);
					if ($pro > 0)
						{
						$queryOK = TRUE;
						$splitTask = FALSE;
						if (!(strpos($qs,'split')===FALSE))
							{
							$split = $queryOutput['split'];
							$splitTask = is_string($split) && $split == 'Y';
							};
						echo finishTask($id, $access, $pro, $str, $splitTask);
						};
					};
				}
			else
				{
				$n_str = $queryOutput['n'];
				$w_str = $queryOutput['w'];
				if (is_string($n_str) && is_string($w_str))
					{
					$n = intval($n_str);
					$w = intval($w_str);
					if ($n >= $min_n && $n <= $max_n && $w >= 0)
						{
						//	"checkMax" action just queries current maximum permutation count.
						//
						//	Query arguments: n, w.
						//
						//	Returns:  (n, w, p) for current maximum p, or "Error: ... "
						
						if ($action == "checkMax")
							{
							$queryOK = TRUE;
							echo maybeUpdateWitnessStrings($n, $w, -1, "", -1);
							}
						else
							{
							$str = $queryOutput['str'];
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
										$pro_str = $queryOutput['pro'];
										if (is_string($pro_str)) $pro = intval($pro_str);
										else $pro = -1;
										echo maybeUpdateWitnessStrings($n, $w, $p, $str, $pro);
										}
									else if ($p<0) $err = 'Invalid string';
									else $err = "Unexpected permutation count [$p permutations, expected $p0 for w=$w, length=$slen]";
									}
									
								//	"createTask" action puts a new task into the tasks database.
								//
								//	Query arguments: n, w, str, pte.
								//
								//	where "pte" is the number of permutations to exceed in any strings the task finds.
								//
								//	Returns: "Task id: ... " or "Error: ... "
								
								else if ($action == "createTask")
									{
									$p_str = $queryOutput['pte'];
									if (is_string($p_str))
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

if (!$queryOK) echo "Error: $err \n";

?>
