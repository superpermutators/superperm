Readme.txt
==========

Author:			Greg Egan
Date:			3 March 2019

Building and testing
--------------------

This file describes the program KernelFinder.c, which searches for non-standard kernels for superpermutations,
as described by Robin Houston here:

	https://groups.google.com/forum/#!msg/superpermutators/VRwU2OIuRhM/fofaUWqrBgAJ

KernelFinder.c is a single, standalone file for a command-line C program, which should compile, link and run in
any command-line environment.

Usage is:

	KernelFinder n [minimumScore [maximumLength]] [palindromic1 | palindromic2]

where:

	n 				specifies the number of digits the kernel permutations use
	
	minimumScore	is the lowest score for a kernel that is included in the output of the search
					[default value is n-2]
					
	maximumLength	is the largest size of kernel to search for
					[default value is 30]
					
NB:  Only kernels with a score that is a multiple of n-2 are included in the output, even if their score exceeds the nominated minimum.

If one of the palindromic options is chosen, the search is restricted to palindromic kernels:

	palindromic1	even number of symbols in the kernel
	palindromic2	odd number of symbols in the kernel
	
A simple test search would be:

	KernelFinder 7 10 18 palindromic1
	
which should create a file named Kernels7_10_18P1.txt, containing two kernels with score 10 and length 18:

666646664466646666
666466646646664666

Files written
-------------

Each search produces a single results file, which is just a list of any kernels found:

	Kernels<n>_<minimumScore>_<maximumLength>[P1|P2].txt

