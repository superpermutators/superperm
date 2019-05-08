<?php
	$statusCache = "statusCache.html";
	$cacheTime = 30;

	if (file_exists($statusCache) && (time() - filemtime($statusCache)) < 30) {
		include("statusCache.html");
		exit;
	}

	ob_start();
	ob_implicit_flush(0);
?>

<!DOCTYPE html>
<html lang="en">
<head>
<title>Chaffin Method Results</title>
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
span.waste {color: #cc3333; font-weight: bold;}
--></style>
</head>
<body>

<?php

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

include '../config.php';

//	Used if we want to keep server from running too many processes.
//	if (file_get_contents("InstanceCount.txt")!="00") exit("Server is busy.");
 
$min_n = 3;
$max_n = 7;
$noResults = TRUE;

$fieldNamesDisplay0 = array('status','Number of clients'); 
$fieldAlign0 = array('center','right');
$nFields0 = count($fieldNamesDisplay0);
$statusAssoc0 = array("Inactive","Active on a task");

$fieldNames = array('ts','n','waste','perms','str','excl_perms','final','team');
$fieldNamesDisplay = array('time stamp','n','waste','maximum<br />perms seen','string showing<br />current maximum','minimum<br />perms excluded','finalised?','team');
$fieldAlign = array('center','right','right','right','str','right','center','team');
$nFields = count($fieldNames);

$fieldNamesDisplay2 = array('status','Number of tasks');
$fieldAlign2 = array('center','right');
$nFields2 = count($fieldNamesDisplay2);
$statusAssoc2 = array("F"=>"Finished","U"=>"Unassigned","A"=>"Assigned");

$fieldNames3 = array('ts','str','team');
$fieldNamesDisplay3 = array('time stamp','superpermutation','team');
$fieldAlign3 = array('center','str','center');
$nFields3 = count($fieldNames3);

/*
$mysqli = new mysqli($host, $user_name, $pwd, $dbase);
if ($mysqli->connect_errno)
	{
	echo "<p>Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "</p>\n";
	}
else
	{
*/
echo "<h1>ADA &mdash; Chaffin Method Results</h1>\n";
echo "<div style=\"margin-top:-8px\"><i>The data on this page refreshes at most once per {$cacheTime} seconds.</i></div><br />\n";


// $res = $mysqli->query("SELECT current_task!=0, COUNT(id) FROM workers GROUP BY current_task!=0");
$res = $pdo->query("SELECT current_task!=0, COUNT(id) FROM workers GROUP BY current_task!=0");
if ($res->rowCount() != 0)
	{
	$noResults = FALSE;
	echo "<table class='strings'><caption>Registered clients</caption>\n";
	echo "<tr>\n";
	for ($i = 0; $i < $nFields0; $i++)
		{
		echo "<th class='". $fieldAlign0[$i] ."'>" . $fieldNamesDisplay0[$i] . "</th>\n";
		};
	echo "</tr>\n";
	// for ($row_no = 0; $row_no < $res->num_rows; $row_no++)
	while ($row = $res->fetch())
		{
		echo "<tr>\n";
		// $res->data_seek($row_no);
		// $row = $res->fetch_array();
		$rowNames = ['current_task!=0', 'COUNT(id)'];
		for ($i = 0; $i < $nFields0; $i++)
			{
			if ($i==0) echo "<td class='". $fieldAlign0[$i] ."'>" . $statusAssoc0[$row[$rowNames[$i]]] . "</td>\n";
			else echo "<td class='". $fieldAlign0[$i] ."'>" . $row[$rowNames[$i]] . "</td>\n";
			};
		echo "</tr>\n";
		};

	$res = $pdo->query("SELECT COUNT(id) FROM workers GROUP BY IP");
	$distinctIP = $res->rowCount();

	$maxTPI = 0;
	// for ($i=0; $i < $distinctIP; $i++)
	while ($row = $res->fetch())
		{
		// $res->data_seek($i);
		// $row2 = $res->fetch_array();
		$tpi = intval($row['COUNT(id)']);
		if ($tpi > $maxTPI) $maxTPI=$tpi;
		};
	echo "<tr><td class='left' colspan='$nFields0'>$distinctIP distinct IP address".($distinctIP>1?"es":"")."</td></tr>\n";
	echo "<tr><td class='left' colspan='$nFields0'>Maximum # of clients with same IP: $maxTPI</td></tr>\n";
	echo "</table>\n";
	};
// $res->close();




$res = $pdo->query("SELECT * FROM teams ORDER BY tasks_completed DESC");
if ($res->rowCount() != 0){
	echo "<table class='strings'><caption>Top Teams</caption>\n";
	$allRows = $res->fetchAll();
	echo "<tr>\n";
	foreach($allRows as $ind => $row) {
		echo "<td class='center'>" . $row['team'] . "</th>\n";
	}
	echo "</tr><tr>\n";
	foreach($allRows as $ind => $row) {
		echo "<td class='center'>" . $row['tasks_completed'] . "</th>\n";
	}
	echo "</tr>";
	// for ($row_no = 0; $row_no < $res->num_rows; $row_no++)
	echo "</table>\n";
}









$res = $pdo->query("SHOW TABLE STATUS FROM $db LIKE 'superperms'");
// $res->data_seek(0);
// $row = $res->fetch_assoc();
$row = $res->fetch();
$upd = $row['Update_time'];
$updTS0 = strtotime($upd);

$res = $pdo->query("SHOW TABLE STATUS FROM $db LIKE 'witness_strings'");
// $res->data_seek(0);
// $row = $res->fetch_assoc();
$row = $res->fetch();
$upd = $row['Update_time'];
$updTS = strtotime($upd);

for ($n = $max_n; $n >= $min_n; $n--)
	{
	$res = $pdo->query("SELECT status, COUNT(id) FROM tasks WHERE n=$n GROUP BY status");
	// if ($res->num_rows != 0)
	if ($res->rowCount() != 0)
		{
		$noResults = FALSE;
		echo "<table class='strings'><caption>Tasks for <i>n</i> = $n</caption>\n";
		echo "<tr>\n";
		for ($i = 0; $i < $nFields2; $i++)
			{
			echo "<th class='". $fieldAlign2[$i] ."'>" . $fieldNamesDisplay2[$i] . "</th>\n";
			};
		echo "</tr>\n";
		// for ($row_no = 0; $row_no < $res->num_rows; $row_no++)
		while ($row = $res->fetch())
			{
			echo "<tr>\n";
			// $res->data_seek($row_no);
			// $row = $res->fetch_array();
			$rowNames = ['status', 'COUNT(id)'];
			for ($i = 0; $i < $nFields2; $i++)
				{
				if ($i==0) echo "<td class='". $fieldAlign2[$i] ."'>" . $statusAssoc2[$row[$rowNames[$i]]] . "</td>\n";
				else echo "<td class='". $fieldAlign2[$i] ."'>" . $row[$rowNames[$i]] . "</td>\n";
				};
			echo "</tr>\n";
			};

		$res = $pdo->query("SELECT num_finished from num_finished_tasks");
		if ($row = $res->fetch()){
			echo "<tr>\n";
			echo "<td class='center'>Finished</td>\n";
			echo "<td class='right'>" . $row['num_finished'] . "</td>\n";
			echo "</tr>\n";
		}

		$res = $pdo->query("SELECT num_redundant from num_redundant_tasks");
		if ($row = $res->fetch()){
			echo "<tr>\n";
			echo "<td class='center'>Redundant</td>\n";
			echo "<td class='right'>" . $row['num_redundant'] . "</td>\n";
			echo "</tr>\n";
		}
		echo "</table>\n";
		};
	// $res->close();

	$fname0 = "Super$n.html";
	
	//	Create a fresh HTML file if the database has been updated since the file was modified
	
	if ((!file_exists($fname0)) || $updTS0 > filemtime($fname0))
		{
		$fp=fopen($fname0,"w");
		$res = $pdo->query("SELECT * FROM superperms WHERE n=$n ORDER BY ts DESC");

		if ($res->rowCount() != 0)
			{
			$noResults = FALSE;
			
			fwrite($fp,"<table class='strings'><caption>Superpermutations for <i>n</i> = $n</caption>\n<tr>\n");
			for ($i = 0; $i < $nFields3; $i++)
				{
				fwrite($fp,"<th class='". $fieldAlign3[$i] ."'>" . $fieldNamesDisplay3[$i] . "</th>\n");
				};
			fwrite($fp,"</tr>\n");
			// for ($row_no = 0; $row_no < $res->num_rows; $row_no++)
			while ($row = $res->fetch())
				{
				fwrite($fp,"<tr>\n");
				// $res->data_seek($row_no);
				// $row = $res->fetch_assoc();
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
		// $res->close();
		fclose($fp);
		};
		
	echo file_get_contents($fname0);
	
	$fname = "Witness$n.html";
	
	//	Create a fresh HTML file if the database has been updated since the file was modified
	
	if ((!file_exists($fname)) || $updTS > filemtime($fname))
		{
		$fp=fopen($fname,"w");
		$res = $pdo->query("SELECT * FROM witness_strings WHERE n=$n ORDER BY waste DESC");

		if ($res->rowCount() != 0)
			{
			$noResults = FALSE;
			
			fwrite($fp,"<table class='strings'><caption>Results for <i>n</i> = $n</caption>\n<tr>\n");
			for ($i = 0; $i < $nFields; $i++)
				{
				fwrite($fp,"<th class='". $fieldAlign[$i] ."'>" . $fieldNamesDisplay[$i] . "</th>\n");
				};
			fwrite($fp,"</tr>\n");
			// for ($row_no = 0; $row_no < $res->num_rows; $row_no++)
				// {
			while ($row = $res->fetch()) {
				fwrite($fp,"<tr>\n");
				// $res->data_seek($row_no);
				// $row = $res->fetch_assoc();
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
		// $res->close();
		fclose($fp);
		};
		
	echo file_get_contents($fname);
	
	};
// $mysqli->close();

if ($noResults) echo "<p>The database contains no results.</p>\n";
	// };
?>

<?php echo rand(); ?>
</body>
</html>

<?php
	
	$pageContents = ob_get_contents();

	$fp=fopen($statusCache,"w");
	fwrite($fp,$pageContents);
	fclose($fp);

	include($statusCache);

?>