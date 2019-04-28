<!DOCTYPE html>
<html lang="en">
<head>
<title>Chaffin Method Results</title>
<style><!--
table.strings td, table.strings th {padding-left: 10px; padding-right: 10px; border-style: solid; border-color: #aaaaaa; border-width: 1px;}
table.strings td.str, table.strings th.str {text-align: left; word-wrap: break-word; overflow-wrap: break-word; max-width: 500px;}
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
	border-width: 2px; border-bottom-width: 0px;};
--></style>
</head>
<body>
<?php

include 'ink1.php';
 
$min_n = 3;
$max_n = 7;
$noResults = TRUE;

$fieldNamesDisplay0 = array('status','Number of clients'); 
$fieldAlign0 = array('center','right');
$nFields0 = count($fieldNamesDisplay0);
$statusAssoc0 = array("Inactive","Active on a task");

$fieldNames = array('ts','n','waste','perms','str','excl_perms','final');
$fieldNamesDisplay = array('time stamp','n','waste','maximum<br />perms seen','string showing<br />current maximum','minimum<br />perms excluded','finalised?');
$fieldAlign = array('center','right','right','right','str','right','center');
$nFields = count($fieldNames);

$fieldNamesDisplay2 = array('status','Number of tasks');
$fieldAlign2 = array('center','right');
$nFields2 = count($fieldNamesDisplay2);
$statusAssoc2 = array("F"=>"Finished","U"=>"Unassigned","A"=>"Assigned");

$mysqli = new mysqli($host, $user_name, $pwd, $dbase);
if ($mysqli->connect_errno)
	{
	echo "<p>Failed to connect to MySQL: (" . $mysqli->connect_errno . ") " . $mysqli->connect_error . "</p>\n";
	}
else
	{
	echo "<h1>Chaffin Method Results</h1>\n";

	$res = $mysqli->query("SELECT current_task!=0, COUNT(id) FROM workers GROUP BY current_task!=0");
	if ($res->num_rows != 0)
		{
		$noResults = FALSE;
		echo "<table class='strings'><caption>Registered clients</caption>\n";
		echo "<tr>\n";
		for ($i = 0; $i < $nFields0; $i++)
			{
			echo "<th class='". $fieldAlign0[$i] ."'>" . $fieldNamesDisplay0[$i] . "</th>\n";
			};
		echo "</tr>\n";
		for ($row_no = 0; $row_no < $res->num_rows; $row_no++)
			{
			echo "<tr>\n";
			$res->data_seek($row_no);
			$row = $res->fetch_array();
			for ($i = 0; $i < $nFields2; $i++)
				{
				if ($i==0) echo "<td class='". $fieldAlign0[$i] ."'>" . $statusAssoc0[$row[$i]] . "</td>\n";
				else echo "<td class='". $fieldAlign0[$i] ."'>" . $row[$i] . "</td>\n";
				};
			echo "</tr>\n";
			};
		echo "</table>\n";
		};
	$res->close();


	for ($n = $min_n; $n <= $max_n; $n++)
		{
		$res = $mysqli->query("SELECT * FROM witness_strings WHERE n=$n ORDER BY waste ASC");

		if ($res->num_rows != 0)
			{
			$noResults = FALSE;
			echo "<table class='strings'><caption>Results for <i>n</i> = $n</caption>\n";
			echo "<tr>\n";
			for ($i = 0; $i < $nFields; $i++)
				{
				echo "<th class='". $fieldAlign[$i] ."'>" . $fieldNamesDisplay[$i] . "</th>\n";
				};
			echo "</tr>\n";
			for ($row_no = 0; $row_no < $res->num_rows; $row_no++)
				{
				echo "<tr>\n";
				$res->data_seek($row_no);
				$row = $res->fetch_assoc();
				for ($i = 0; $i < $nFields; $i++)
					{
					echo "<td class='". $fieldAlign[$i] ."'>" . $row[$fieldNames[$i]] . "</td>\n";
					};
				echo "</tr>\n";
				};
			echo "</table>\n";
			};
		$res->close();
		
		$res = $mysqli->query("SELECT status, COUNT(id) FROM tasks WHERE n=$n GROUP BY status");
		if ($res->num_rows != 0)
			{
			$noResults = FALSE;
			echo "<table class='strings'><caption>Tasks for <i>n</i> = $n</caption>\n";
			echo "<tr>\n";
			for ($i = 0; $i < $nFields2; $i++)
				{
				echo "<th class='". $fieldAlign2[$i] ."'>" . $fieldNamesDisplay2[$i] . "</th>\n";
				};
			echo "</tr>\n";
			for ($row_no = 0; $row_no < $res->num_rows; $row_no++)
				{
				echo "<tr>\n";
				$res->data_seek($row_no);
				$row = $res->fetch_array();
				for ($i = 0; $i < $nFields2; $i++)
					{
					if ($i==0) echo "<td class='". $fieldAlign2[$i] ."'>" . $statusAssoc2[$row[$i]] . "</td>\n";
					else echo "<td class='". $fieldAlign2[$i] ."'>" . $row[$i] . "</td>\n";
					};
				echo "</tr>\n";
				};
			echo "</table>\n";
			};
		$res->close();
		};
	$mysqli->close();
	
	if ($noResults) echo "<p>The database contains no results.</p>\n";
	};
?>
</body>
</html>
