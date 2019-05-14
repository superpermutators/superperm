# Readme

Author:			Greg Egan  
  (minor updates by Jay Pantone)  
Date:			14 May 2019

NB:  These notes include some features that are only present from version 10 onwards. Please always install the latest
version of the program.


## Building and testing

**DistributedChaffinMethod.c** is a single, standalone file for a command-line C program. It is intended to compile and run
under MacOS, Linux and some versions of Windows.

In order to run correctly, the program needs:

1. Write access to the current directory.
2. Permission to make outgoing connections to the internet.
3. The presence of the "curl" command line tool, and the ability for the program to run it via the system() call
in the C standard library. These are standard in MacOS/Linux, but for Windows will depend on your precise environment.

If the program compiles correctly, running it with the option "test" will simply test whether or not it can connect to
the server and read back the expected response:

```sh
$ DistributedChaffinMethod test
Random seed is: 1557505343
Fri May 10 07:39:29 2019 Program instance number: 1314126518
Fri May 10 07:39:29 2019 Team name: anonymous
Fri May 10 07:39:29 2019 To server: action=hello
Fri May 10 07:39:29 2019 Server: Hello world.
```

If you wish to commit to running the program to assist in the distributed search, simply run it with no arguments and it will
loop indefinitely, waiting for available tasks to execute.

You can monitor the ongoing results of the search at:

<http://www.supermutations.net/ChaffinMethodResults.php>

To make an effective contribution, the program needs to have (more or less) uninterrupted access to the internet;
if it can't make a connection, it will loop, sleeping for a few minutes then trying again, rather than performing any useful
computations.

## Building under Windows

There are a wide variety of C compilers, IDEs and runtime environments for Windows, and it is impossible for us to test them all in
advance, but we encourage Windows users to first try to compile and run **WindowsBugTest.c**, which can be found
in the Debugging folder of the main repository.

If this program successfully compiles (which might need some switches set to ensure compatibility with modern versions of C)
then running it will attempt to perform a very short computation, without making any contact with the server.  If the output you
see ends like this (apart from the specific dates, of course):

```sh
Thu May  9 13:07:33 2019 Assigned new task (id=0, access=0, n=6, w=105, prefix=1234561234516234512634512364512346512436512463512465312465132465134265134625134652134562135, perm_to_exceed=567, prev_perm_ruled_out=569)
Thu May  9 13:07:33 2019 Finished current search, bestSeenP=490, nodes visited=13910255
```

then the computation was completed successfully, and you can go ahead and build the full program.

However, if the calculation is *not* completed, and **WindowsBugTest** crashes before printing the final line shown above, you will need to
find an alternative set of C tools in order to proceed.


## Shutting down the program

It is best if the program can be left running continuously, but if you do kill the program or shut down the computer it is
running on, then (eventually) the task it was assigned will be reassigned to another instance of the program.

You can tell the program to shut down cleanly, between tasks, by placing a file in its working directory with the name:

STOP_NNNNNNNNNN.txt

where NNNNNNN is the same program instance number as used to distinguish the log files.  Or, if you use the name:

STOP_ALL.txt

then any instance of the program using that directory will treat that as a signal to shut down.

If you are running under MacOS/Linux, you can:
* Type CTRL-C once to tell the program to quit when it has finished with the current task;
* Type CTRL-C between three and six times, to tell the program to *relinquish* the current task (tell the server it has abandoned it) and quit.
* If the program is unable to make contact with the server at all, hitting CTRL-C repeatedly will eventually force it to quit.

Under Windows, CTRL-C will kill the program immediately, so we would prefer that you shut it down by creating a STOP file.
Any text editor can be used to create a file with the required name, and it doesn't matter what text the
file contains. Just remember to delete the STOP file if you want to start running the program again (e.g. after an upgrade).


## Team name

You can specify a team name when launching the program. This team name will show up on the results page next to any 
witness strings or superpermutations found by the client. The results page also keeps a log of how many tasks each
team has completed. To specify a team name, use the "team" argument, followed by an alphanumeric string with at most 32 characters. Spaces are allowed if placed in quotes.

The default team name is "anonymous" if none is specified.

Examples:

```sh
DistributedChaffinMethod team permutators
```

```sh
DistributedChaffinMethod team "spaces work"
```


## Time-limited run

You can tell the program to run for a specified number of minutes, then wait to finish the current task before quitting.

The time limit is specified on the command line:

```sh
DistributedChaffinMethod timeLimit 120
```

would tell the program to run for 120 minutes before quitting (after it has finished its current task).


## Multiple arguments

The "timeLimit" and "team" arguments can be used at the same time, in either order.

Example:

```sh
DistributedChaffinMethod timeLimit 120 team "golden eagles"
```


## Files written

The program writes files:

1. A cumulative log file, "DCMLog_NNNNNNNNNN.txt"
2. A temporary file, "DCMServerResponse_NNNNNNNNNN.txt"

where NNNNNNNNNN is a random integer chosen by each instance of the program.


