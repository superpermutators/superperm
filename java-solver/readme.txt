the superperm_sols file I have here is a list of results from my solver, one solution per line.  This can be used with the MakePerms file to spit out actual solutions.  Compared to the solver, this class is a real mess.  If any one wants to clean it up or port it, feel free.

The solver is in 3 files:
	Generator
	Solver
	MakePerms
To change the pattern length (n), edit the strings at the top of Generator and MakePerms.

Each solution lists the edges incoming to the indicated nodes where a non-greedy choice was made.  The node order is as defined in MakePerms.