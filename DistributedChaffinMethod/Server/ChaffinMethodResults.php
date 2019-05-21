<?php
//	Version 13.0
//	Last updated: 21 May 2019
//	Authors: Greg Egan, Jay Pantone

include '../inc/dbinfo.inc';

//	Directory where scripts have permission to write

define('PHP_FILES', 'phpFiles/');
?>
<!DOCTYPE html>
<html lang="en">
<head>
<title>Distributed Chaffin Method Results</title>
<style><!--
table.strings td, table.strings th {padding-left: 10px; padding-right: 10px; border-style: solid; border-color: #aaaaaa; border-width: 1px;}
table.strings td.str, table.strings th.str {text-align: left; word-wrap: break-word; overflow-wrap: break-word; max-width: 500px;}
table.strings td.left, table.strings th.left {text-align: left;}
table.strings td.right, table.strings th.right {text-align: right;}
table.strings td.center, table.strings th.center {text-align: center;}
table.strings {border-style: solid; border-color: black;
	border-width: 2px;
	color: black; background-color: #eeeeee;
	border-collapse: collapse;
	margin-bottom: 20px;
}
table.strings caption {padding: 10px; background-color: #eeeeee;
	border-style: solid; border-color: black;
	border-width: 2px; border-bottom-width: 0px;}
table.strings tr.search {background-color: #ddffdd;}
table.strings tr.prov {background-color: #fbe7b1;}
span.waste {color: #cc3333; font-weight: bold;}
p.fp:first-letter, p.fp::first-letter {font-size: 130%;}
div.status {background-color: #ddffdd; border-style: solid; border-color: black;
	border-width: 2px; padding: 1ex; margin: 1ex; margin-bottom: 20px;}
	
div.leader {height: 400px; overflow-y: scroll; float: right; }
table.leader {background-color: #ffffdd; border-style: solid; border-color: black;
	border-width: 2px; margin: 1ex; border-collapse: collapse;}
table.leader td, table.leader caption {border-style: solid; border-color: black;
	border-width: 2px; background-color: #ffffdd; padding-left: 8px; padding-right: 8px;}
table.leader caption {border-bottom-width: 0px; padding: 5px;}
table.leader td {text-align: right;}
teamName {white-space: nowrap;}
tr.active {background-color: #ddffdd;}
--></style>
</head>
<body>
<h1>Distributed Chaffin Method Results</h1>
<p>The results shown below are from a <b>test server</b> that is trying out new software, which will soon be installed on the main server.
Team Reykjavik <a href="http://ada.mscsnet.mu.edu/ChaffinMethodResults/">recently found</a> an optimal string for <i>w</i>=116, but it would be nice to independently confirm their result
before moving on to the next stage. So if you have some spare CPU cores, join in the search!</p>
<ul>
<li>What is a <a href="https://www.gregegan.net/SCIENCE/Superpermutations/Superpermutations.html">superpermutation</a>?</li>
<li>What is the <a href="https://github.com/superpermutators/superperm/wiki/Chaffin-method">Chaffin method</a>?</li>
<li><a href="https://github.com/nagegerg/superperm/tree/master/DistributedChaffinMethod">GitHub repository</a> with the software you need to join this search. Please read the README!</li>
<li>Sign up to the <a href="https://groups.google.com/forum/#!forum/superpermutators">Superpermutators</a> Google group to get the latest news and discuss the results.</li>
</ul>

<?php
$dtz = new DateTimeZone(date_default_timezone_get());
$dt = new DateTime();
$rtime = $dt->format('Y-m-d H:i:s')." UTC";
$offs = $dtz->getOffset($dt);
if ($offs)
{
if ($offs<0)
	{
	$sign = "-";
	$offs = -$offs;
	}
else $sign = "+";
$mins = floor($offs/60);
$hours = floor($mins/60);
$mins = $mins - 60*$hours;
$rtime = $rtime . $sign . sprintf("%02d:%02d",$hours,$mins);
}

function decorateString($str, $n)
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
			
			$res=substr($str,0,$n-1);
			$prevWaste=0;
			for ($i=0; $i <= $slen - $n; $i++)
				{
				$c = substr($str,$n-1+$i,1);
				$s = array_slice($stri,$i,$n);
				$u = array_unique($s);
				$waste=1;
				if (count($u) == $n)
					{
					$pval = 0;
					$factor = 1;
					for ($j=0; $j<$n; $j++)
						{
						$pval += $factor * $s[$j];
						$factor *= 10;
						};
					if (!in_array($pval, $perms))
						{
						$perms[] = $pval;
						$waste=0;
						};
					};
				if ($waste!=$prevWaste)
					{
					if ($waste) $res = $res . '<span class="waste">';
					else $res = $res . '</span>';
					$prevWaste=$waste;
					};
				$res = $res . $c;
				};
			return $res;
			};
		};
	};
return "";
}

function factorial($n) {
	if ($n==1) return 1;
	return $n*factorial($n-1);
}

function eDisc($n,$w,$pwit,$pte,$ppro,$iter)
{
global $rtime, $noStatusShown;

$noStatusShown=FALSE;

if ($pwit > $pte) $pte=$pwit;

$passNames = array("first","second","third","fourth","fifth");
$rlens=array(0,0,0,9,33,153,872,5906);
$edisc = "<br clear='right' /><div class='status'><h3>Search status as of $rtime</h3>\n";
$edisc = $edisc . "<p class='fp'>We are currently searching for strings consisting of digits between 1 and $n that contain <b>as many permutations as possible</b>, while &#8220;wasting&#8221; ";
$edisc = $edisc . "only <b>$w digits</b>.  By a &#8220;wasted&#8221; digit, we mean one that, when added to the string, did not add a (new) permutation. " ;
$edisc = $edisc . "We don&#x2019;t count the first " .($n-1)." digits as wasted.</p>" ;
$edisc = $edisc . "<p class='fp'>Right now we are <b>on our ". $passNames[$iter]." search for <i>w</i>=$w,</b> looking for strings with <b>more than $pte permutations</b>. " ;
if ($ppro<1000000)
{
$edisc = $edisc . "We have been able to rule out the possibility of any such string having $ppro or more permutations, ";
if ($iter>0) $edisc = $edisc . " thanks to the previous search for <i>w</i>=$w, which aimed for a higher permutation count and found no strings. ";
else $edisc = $edisc . " thanks to the result for <i>w</i>=".($w-1).", which cannot be improved upon by more than $n when a single wasted character is added. ";
};
if ($pwit>0)
{
$edisc = $edisc . "So far, we have found a string with $pwit permutations";
if ($iter==0 && $pwit <= $pte) $edisc = $edisc . ", which was discovered as a by-product of an earlier search.";
else $edisc = $edisc . ".";
}
else
{
$edisc = $edisc . "So far, we have not found any string at all for <i>w</i>=$w.";
};
$edisc = $edisc . "</p><p>[If you&#x2019;re wondering why we have some strings in the table for higher <i>w</i> as well, with some not-all-that-impressive permutation counts, these are ones we found along the way, essentially for free, by making simple extensions to the record-holders. They serve to set a lower bound on what we know is achievable once we reach those higher values of <i>w</i>.]</p>";
$edisc = $edisc . "<p class='fp'>If we are lucky, when we reach a high enough value of <i>w</i> we might find a new superpermutation &#8212; a string that contains ";
$edisc = $edisc . "<b>all</b> ".factorial($n)." permutations of the digits between 1 and $n &#8212; that is shorter than the previous record (which has ";
$edisc = $edisc . "a length of ".$rlens[$n]." and wastes ".($rlens[$n]-factorial($n)-($n-1))." digits).</p>";
$edisc = $edisc . "</div>";
return $edisc;
}

function writeClients($fp)
{
global $pdo, $noClientsShown, $rtime;

$noClientsShown = FALSE;

$res = $pdo->query("SELECT team, FLOOR(nodeCount/1000000000) FROM teams WHERE nodeCount!=0 AND visible='Y' ORDER BY nodeCount DESC");
if ($res && ($rc=$res->rowCount()) != 0)
	{
	$tnc = 0;
	$tres = $pdo->query("SELECT FLOOR(nodeCount/1000000000) FROM total_nodeCount");
	if ($trow = $tres->fetch(PDO::FETCH_NUM)) $tnc=$trow[0];
	
	$tableWidth = 7;
	fwrite($fp,"<div class='leader'><table class='leader'><caption><b>GigaNodes Leader Board</b><br />as of $rtime.<br /><i>Total GigaNodes checked:</i> ".number_format(floatval($tnc),0,".",",")."</caption>");
	$c=0;
	while ($row = $res->fetch(PDO::FETCH_NUM))
		{
		if ($c==0) fwrite($fp,"<tr>");
		fwrite($fp,"<td><span class='team'>".$row[0]."</span><br />".number_format(floatval($row[1]),0,".",",")."</td>");
		$c = ($c+1) % $tableWidth;
		if ($c==0) fwrite($fp,"</tr>");
		};
	if ($c!=0) fwrite($fp,"</tr>");
	fwrite($fp,"</table></div>");
	}
else fwrite($fp,"<!-- NO LEADER -->");

$fieldNamesDisplay0 = array('status','Number of clients'); 
$fieldAlign0 = array('center','right');
$nFields0 = count($fieldNamesDisplay0);
$statusAssoc0 = array("Inactive","Active on a task","<i>Total</i>");

$res = $pdo->query("SELECT current_task!=0, COUNT(id) FROM workers GROUP BY current_task!=0");
if ($res && $res->rowCount() != 0)
	{
	fwrite($fp,"<table class='strings'><caption><b>Registered clients</b> as of<br />$rtime</caption>\n");
	fwrite($fp,"<tr>\n");
	for ($i = 0; $i < $nFields0; $i++)
		{
		fwrite($fp,"<th class='". $fieldAlign0[$i] ."'>" . $fieldNamesDisplay0[$i] . "</th>\n");
		};
	fwrite($fp,"</tr>\n");
	$total=0;
	$rc = $res->rowCount();
	for ($row_no = 0; $row_no < $rc+1; $row_no++)
		{
		if ($row_no == $rc)
			{
			fwrite($fp,"<tr>\n");
			$row=array(2,$total);
			}
		else
			{
			$row = $res->fetch(PDO::FETCH_NUM);
			$total+=intval($row[1]);
			if ($row_no < $rc && $statusAssoc0[$row[0]]=='Active on a task') fwrite($fp,"<tr class='active'>\n");
			else fwrite($fp,"<tr>\n");
			};
		for ($i = 0; $i < $nFields0; $i++)
			{
			if ($i==0) fwrite($fp,"<td class='". $fieldAlign0[$i] ."'>" . $statusAssoc0[$row[$i]] . "</td>\n");
			else fwrite($fp,"<td class='". $fieldAlign0[$i] ."'>" . number_format(floatval($row[$i]),0,".",",") . "</td>\n");
			};
		fwrite($fp,"</tr>\n");
		};
	$res2 = $pdo->query("SELECT COUNT(id) FROM workers GROUP BY IP");
	$distinctIP = $res2->rowCount();
	$maxTPI = 0;
	for ($i=0; $i < $distinctIP; $i++)
		{
		$row2 = $res2->fetch(PDO::FETCH_NUM);
		$tpi = intval($row2[0]);
		if ($tpi > $maxTPI) $maxTPI=$tpi;
		};
	fwrite($fp,"<tr><td class='left' colspan='$nFields0'>$distinctIP distinct IP address".($distinctIP>1?"es":"")."</td></tr>\n");
	fwrite($fp,"<tr><td class='left' colspan='$nFields0'>Maximum # of clients with same IP: $maxTPI</td></tr>\n");
	fwrite($fp,"</table>\n");
	};
}

//	Used if we want to keep server from running too many processes.
//	if (file_get_contents("InstanceCount.txt")!="00") exit("Server is busy.");
 
$min_n = 3;
$max_n = 7;
$noClientsShown = TRUE;
$noStatusShown = TRUE;

$fieldNames = array('ts','n','waste','perms','str','excl_perms','final','team');
$fieldNamesDisplay = array('time stamp','n','waste','maximum<br />perms seen','string showing<br />current maximum','minimum<br />perms excluded','finalised?','team');
$fieldAlign = array('center','right','right','right','str','right','center','center');
$nFields = count($fieldNames);

$fieldNamesDisplay2 = array('status','Number of tasks');
$fieldAlign2 = array('center','right');
$nFields2 = count($fieldNamesDisplay2);
$statusAssoc2 = array("F"=>"Finished","U"=>"Unassigned","A"=>"Assigned","P"=>"Pending");

$fieldNames3 = array('ts','str','team');
$fieldNamesDisplay3 = array('time stamp','superpermutation','team');
$fieldAlign3 = array('center','str','center');
$nFields3 = count($fieldNames3);

try {
$pdo = new PDO('mysql:host='.DB_SERVER.';dbname='.DB_DATABASE,DB_USERNAME, DB_PASSWORD);

$updTS0=time();
$res = $pdo->query("SHOW TABLE STATUS FROM ".DB_DATABASE." LIKE 'superperms'");
if ($res)
	{
	$row = $res->fetch(PDO::FETCH_ASSOC);
	$upd = $row['Update_time'];
	if ($upd) $updTS0 = strtotime($upd);
	};

$updTS=time();
$res = $pdo->query("SHOW TABLE STATUS FROM ".DB_DATABASE." LIKE 'witness_strings'");
if ($res)
	{
	$row = $res->fetch(PDO::FETCH_ASSOC);
	$upd = $row['Update_time'];
	if ($upd) $updTS = strtotime($upd);
	};

for ($n = $max_n; $n >= $min_n; $n--)
	{
	$fname1 = PHP_FILES . "Tasks$n.html";
	
	if ((!file_exists($fname1)) || time() - filemtime($fname1) > 60)
		{
		$fp=fopen($fname1,"w");
	
		$res = $pdo->query("SELECT status, COUNT(id) FROM tasks WHERE n=$n GROUP BY status");
		if ($res->rowCount() != 0)
			{
			if ($noClientsShown) writeClients($fp);
			
			fwrite($fp,"<table class='strings'><caption><b>Tasks for <i>n</i> = $n</b> as of<br />$rtime</caption>\n<tr>\n");
			for ($i = 0; $i < $nFields2; $i++)
				{
				fwrite($fp,"<th class='". $fieldAlign2[$i] ."'>" . $fieldNamesDisplay2[$i] . "</th>\n");
				};
			fwrite($fp,"</tr>\n");
			for ($row_no = 0; $row_no < $res->rowCount(); $row_no++)
				{
				$row = $res->fetch(PDO::FETCH_NUM);
				if ($statusAssoc2[$row[0]]=='Assigned') fwrite($fp,"<tr class='active'>\n");
				else fwrite($fp,"<tr>\n");
				for ($i = 0; $i < $nFields2; $i++)
					{
					if ($i==0) fwrite($fp,"<td class='". $fieldAlign2[$i] ."'>" . $statusAssoc2[$row[$i]] . "</td>\n");
					else fwrite($fp,"<td class='". $fieldAlign2[$i] ."'>" . number_format(floatval($row[$i]),0,".",",") . "</td>\n");
					};
				fwrite($fp,"</tr>\n");
				};
				
			$tfin = 0;
			$tres = $pdo->query("SELECT num_finished FROM num_finished_tasks");
			if ($trow = $tres->fetch(PDO::FETCH_NUM)) $tfin=$trow[0];
			fwrite($fp,"<tr><td class='". $fieldAlign2[0] ."'>Finished</td><td class='". $fieldAlign2[1] ."'>".number_format(floatval($tfin),0,".",",")."</td></tr>\n");

			$tred = 0;
			$tres = $pdo->query("SELECT num_redundant FROM num_redundant_tasks");
			if ($trow = $tres->fetch(PDO::FETCH_NUM)) $tred=$trow[0];
			fwrite($fp,"<tr><td class='". $fieldAlign2[0] ."'>Redundant</td><td class='". $fieldAlign2[1] ."'>".number_format(floatval($tred),0,".",",")."</td></tr>\n");
			fwrite($fp,"</table>\n");
			};
			
		//	Get details of any task as a guide
		
		if ($noStatusShown)
			{
			$res = $pdo->query("SELECT waste, perm_to_exceed, prev_perm_ruled_out, iteration FROM tasks WHERE n=$n AND test='N' LIMIT 1");
			if ($res && $res->rowCount() !=0)
				{
				$row = $res->fetch(PDO::FETCH_NUM);
				$w = intval($row[0]);
				$pte = intval($row[1]);
				$ppro = intval($row[2]);
				$iter = intval($row[3]);
				
					
				//	Get anything relevant from the witness strings
				
				$pwit=-1;
				$res = $pdo->query("SELECT perms FROM witness_strings WHERE n=$n AND waste=$w");
				if ($res && $res->rowCount() !=0)
					{
					$row = $res->fetch(PDO::FETCH_NUM);
					$pwit=intval($row[0]);
					};
				fwrite($fp,eDisc($n,$w,$pwit,$pte,$ppro,$iter));
				}
			}
		fclose($fp);
		};
		
	echo file_get_contents($fname1);

	$fname0 = PHP_FILES . "Super$n.html";
	
	//	Create a fresh HTML file if the database has been updated since the file was modified
	
	if ((!file_exists($fname0)) || $updTS0 > filemtime($fname0))
		{
		$fp=fopen($fname0,"w");
		$res = $pdo->query("SELECT * FROM superperms WHERE n=$n ORDER BY ts DESC");

		if ($res->rowCount() != 0)
			{
			fwrite($fp,"<table class='strings'><caption>Superpermutations for <i>n</i> = $n</caption>\n<tr>\n");
			for ($i = 0; $i < $nFields3; $i++)
				{
				fwrite($fp,"<th class='". $fieldAlign3[$i] ."'>" . $fieldNamesDisplay3[$i] . "</th>\n");
				};
			fwrite($fp,"</tr>\n");
			for ($row_no = 0; $row_no < $res->rowCount(); $row_no++)
				{
				fwrite($fp,"<tr>\n");
				$row = $res->fetch(PDO::FETCH_ASSOC);
				for ($i = 0; $i < $nFields3; $i++)
					{
					$val = $row[$fieldNames3[$i]];
					if ($i==1) $val=decorateString($val, $n);
					fwrite($fp,"<td class='". $fieldAlign3[$i] ."'>" . $val . "</td>\n");
					};
				fwrite($fp,"</tr>\n");
				};
			fwrite($fp,"</table>\n");
			};
		fclose($fp);
		};
		
	echo file_get_contents($fname0);
	
	$fname = PHP_FILES . "Witness$n.html";
	
	//	Create a fresh HTML file if the database has been updated since the file was modified
	
	if ((!file_exists($fname)) || $updTS > filemtime($fname))
		{
		$fp=fopen($fname,"w");
		$res = $pdo->query("SELECT * FROM witness_strings WHERE n=$n ORDER BY waste DESC");

		if ($res->rowCount() != 0)
			{
			//	Identify the lowest non-finalised waste
			
			$res0 = $pdo->query("SELECT MIN(waste) FROM witness_strings WHERE n=$n AND final='N'");
			if ($res0 && ($row0=$res0->fetch(PDO::FETCH_NUM))) $wSearch=$row0[0];
			else $wSearch=-1;
			
			fwrite($fp,"<table class='strings'><caption><b>Results for <i>n</i> = $n</b> as of $rtime</caption><tr>\n");
			for ($i = 0; $i < $nFields; $i++)
				{
				fwrite($fp,"<th class='". $fieldAlign[$i] ."'>" . $fieldNamesDisplay[$i] . "</th>\n");
				};
			fwrite($fp,"</tr>\n");
			for ($row_no = 0; $row_no < $res->rowCount(); $row_no++)
				{
				$row = $res->fetch(PDO::FETCH_ASSOC);
				if ($row['waste']==$wSearch) fwrite($fp,"<tr class='search'>\n");
				else if ($row['waste']>$wSearch) fwrite($fp,"<tr class='prov'>\n");
				else fwrite($fp,"<tr>\n");
				 
				for ($i = 0; $i < $nFields; $i++)
					{
					$val = $row[$fieldNames[$i]];
					if ($i==4) $val=decorateString($val, $n);
					fwrite($fp,"<td class='". $fieldAlign[$i] ."'>" . $val . "</td>\n");
					};
				fwrite($fp,"</tr>\n");
				};
			fwrite($fp,"</table>\n");
			};
		fclose($fp);
		};
		
	echo file_get_contents($fname);
	};
	
$pdo=NULL;

} catch (Exception $e) {
    print "Error opening PDO connection\n";
    die();
}

?>
</body>
</html>
