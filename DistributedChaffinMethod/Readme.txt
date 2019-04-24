Readme.txt
==========

Author:			Greg Egan
Date:			24 April 2019

Building and testing
--------------------

DistributedChaffinMethod.c is a single, standalone file for a command-line C program. It is intended to compile and run
under MacOS, Linux and some versions of Windows, though at the time of writing it has only been tested under MacOS.

In order to run correctly, the program needs:

(1) Write access to the current directory.
(2) Permission to make outgoing connections to the internet.
(3) The presence of the "curl" command line tool, and the ability for the program to run it via the system() call
in the C standard library.

If the program compiles correctly, running it with the option "test" will simply test whether or not it can connect to
the server and read back the expected response:

$ DistributedChaffinMethod test
Wed Apr 24 17:26:16 2019 Server: Hello world.

If you wish to commit to running the program to assist in the distributed search, simply run it with no arguments and it will
loop, waiting for available tasks to execute.

To make an effective contribution, the program needs to have (more or less) uninterrupted access to the internet;
if it can't make a connection, it will loop, sleeping for a few minutes then trying again, rather than performing any useful
computations.

It is also best if the program can be left running continuously, but if you do kill the program or shut down the computer it is
running on, then (eventually) the task it was assigned will be reassigned to another instance of the program.


Files written
-------------

The program writes files:

(1) A cumulative log file, "DCMLog.txt"
(2) A temporary file, "DCMServerResponse.txt"



