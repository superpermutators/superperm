# Readme

Author:			Greg Egan  
  (minor updates by Jay Pantone)  
Date:			24 May 2019

NB:  These notes include some features that are only present from version 13.1 onwards. Please always install the latest
version of the program.

## Java client

A Java client is available in the repository. This consists of the file `DCM.jar` , and it requires Java release 8, which can be downloaded
from <http://java.com>. 

`DCM.jar` can be run simply by double-clicking it on the desktop for most operating systems, or by typing `java -jar DCM.jar` into a
command line (assuming the jar file has been placed in the current directory).

Once you run the Java client, simply type in your team name and hit the `Register` button. You can quit the program in either of two ways:
`Finish task & quit`, which continues with the current task until it is completed, or `Give up task & quit` which stops work on the task
immediately and tells the server to assign it to someone else, before quitting.

The Java client does not read or write any files at all, though a log of its interactions with the server is available in a scrolling text box at the top
of the application's window.


## Building and testing the C client

**DistributedChaffinMethod.c** is a single, standalone file for a command-line C program. It is intended to compile and run
under MacOS, Linux and some versions of Windows.

In order to run correctly, the program needs:

1. Write access to the current directory.
2. Permission to make outgoing connections to the internet.
3. The presence of the "curl" command line tool, and the ability for the program to run it via the system() call
in the C standard library. These are standard in MacOS/Linux, but for Windows will depend on your precise environment.

Note that the program uses functions in `math.h`, so with some compilers it will require the switch `-lm` to link with the mathematical
functions library.

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
loop indefinitely, waiting for available tasks to execute. Note that:

* To make an effective contribution, the program needs to have (more or less) uninterrupted access to the internet; if it can't make a connection, it will loop, sleeping for a few minutes then trying again, rather than performing any useful
computations.
* If a task completes in less than a certain amount of time, your program might sleep for the remainder of the time, to avoid bombarding the server with too much traffic.
* If you wish to quit the program, there are several choices, discussed in the section **Shutting down the program**. It is much better if you can follow one of these methods, rather than killing the program while it is working on a task.

You can monitor the ongoing results of the search at:

<http://www.supermutations.net/ChaffinMethodResults.php>


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

You can tell the program to shut down cleanly, **after it has finished with the current task**, by placing a file in its working directory with the name:

STOP_NNNNNNNNNN.txt

where NNNNNNN is the same program instance number as used to distinguish the log files.  Or, if you use the name:

STOP_ALL.txt

then *any* instance of the program using that directory will treat that as a signal to shut down. Note that a task might take more
than an hour to complete, so the signal from a STOP file might not come into effect for some time.

You can also tell the program to **give up on its current task**, by placing a file in its working directory with the name:

QUIT_NNNNNNNNNN.txt

or:

QUIT_ALL.txt

When it sees one of these QUIT files, the program will tell the server to reassign its task to another client, and then quit. This should happen
in less than a minute.

If you are running under MacOS/Linux, you can also:
* Type CTRL-C once to tell the program to quit **after it has finished with the current task**.
* Type CTRL-C between three and six times, to tell the program to **give up on the current task and quit**.
* If the program is unable to make contact with the server at all, hitting CTRL-C repeatedly will eventually force it to quit.

Under Windows, CTRL-C will kill the program immediately, so we would prefer that you shut it down by creating a STOP or QUIT file.
Any text editor can be used to create a file with the required name, and it doesn't matter what text the
file contains. Just remember to delete the STOP/QUIT file if you want to start running the program again (e.g. after an upgrade).


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

You can tell the program to run for a specified number of minutes, after which it will **wait to finish the current task** before quitting.

The time limit is specified on the command line:

```sh
DistributedChaffinMethod timeLimit 120
```

would tell the program to run for 120 minutes and then quit, **after it has finished its current task**.

You can also enforce a stricter time limit, using the option `timeLimitHard`.  If you specify:

```sh
DistributedChaffinMethod timeLimitHard 120
```

then the program will run for 120 minutes and quit, within about 5 minutes of that quota, **even if** it is in the middle of a task.

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


