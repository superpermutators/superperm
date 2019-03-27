Readme.txt
==========

Author:			Greg Egan [based on a previous version by Nathaniel Johnston;
				see http://www.njohnston.ca/2014/08/all-minimal-superpermutations-on-five-symbols-have-been-found/ ]
Date:			27 March 2019

Building and testing
--------------------

This file describes the program ChaffinMethod.c, which uses an algorithm devised by Benjamin Chaffin to compute all strings
(starting with 123...n) that contain the maximum possible number of permutations on n symbols while wasting w
characters, for all values of w from 1 up to the point where all permutations are visited (i.e. these strings become
superpermutations).

ChaffinMethod.c is a single, standalone file for a command-line C program, which should compile, link and run in
any command-line environment.

Usage is:

	ChaffinMethod n

where:

	n must lie between 3 and 7, and specifies the number of symbols being permuted.
	
The n=4 case should complete almost instantly, with output like this (date commands are used before and after to show the elapsed time):

$ date; ChaffinMethod 4; date
Wed 27 Mar 2019 13:39:35 AWST
1 wasted characters: at most 8 permutations, in 12 characters, 1 examples
2 wasted characters: at most 12 permutations, in 17 characters, 1 examples
3 wasted characters: at most 14 permutations, in 20 characters, 2 examples
4 wasted characters: at most 18 permutations, in 25 characters, 1 examples
5 wasted characters: at most 20 permutations, in 28 characters, 7 examples
6 wasted characters: at most 24 permutations, in 33 characters, 1 examples

-----
DONE!
-----
Minimal superpermutations on 4 symbols have 6 wasted characters and a length of 33.

Wed 27 Mar 2019 13:39:35 AWST


The n=5 case should complete in about a minute, with output like this:

$ date; ChaffinMethod 5; date
Wed 27 Mar 2019 13:39:40 AWST
1 wasted characters: at most 10 permutations, in 15 characters, 1 examples
2 wasted characters: at most 15 permutations, in 21 characters, 1 examples
3 wasted characters: at most 20 permutations, in 27 characters, 1 examples
4 wasted characters: at most 23 permutations, in 31 characters, 3 examples
5 wasted characters: at most 28 permutations, in 37 characters, 2 examples
6 wasted characters: at most 33 permutations, in 43 characters, 1 examples
7 wasted characters: at most 36 permutations, in 47 characters, 4 examples
8 wasted characters: at most 41 permutations, in 53 characters, 2 examples
9 wasted characters: at most 46 permutations, in 59 characters, 1 examples
10 wasted characters: at most 49 permutations, in 63 characters, 2 examples
11 wasted characters: at most 53 permutations, in 68 characters, 10 examples
12 wasted characters: at most 58 permutations, in 74 characters, 2 examples
13 wasted characters: at most 62 permutations, in 79 characters, 1 examples
14 wasted characters: at most 66 permutations, in 84 characters, 7 examples
15 wasted characters: at most 70 permutations, in 89 characters, 4 examples
16 wasted characters: at most 74 permutations, in 94 characters, 14 examples
17 wasted characters: at most 79 permutations, in 100 characters, 2 examples
18 wasted characters: at most 83 permutations, in 105 characters, 2 examples
19 wasted characters: at most 87 permutations, in 110 characters, 4 examples
20 wasted characters: at most 92 permutations, in 116 characters, 1 examples
21 wasted characters: at most 96 permutations, in 121 characters, 1 examples
22 wasted characters: at most 99 permutations, in 125 characters, 2 examples
23 wasted characters: at most 103 permutations, in 130 characters, 6 examples
24 wasted characters: at most 107 permutations, in 135 characters, 2 examples
25 wasted characters: at most 111 permutations, in 140 characters, 2 examples
26 wasted characters: at most 114 permutations, in 144 characters, 2 examples
27 wasted characters: at most 116 permutations, in 147 characters, 5 examples
28 wasted characters: at most 118 permutations, in 150 characters, 4 examples
29 wasted characters: at most 120 permutations, in 153 characters, 8 examples

-----
DONE!
-----
Minimal superpermutations on 5 symbols have 29 wasted characters and a length of 153.

Wed 27 Mar 2019 13:40:36 AWST


Files written
-------------

The strings for each value of w, the number of wasted characters, are written to files of the form:

	Chaffin_<n>_W_<w>.txt
