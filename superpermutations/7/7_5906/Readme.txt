How these files were created
----------------------------

Using KernelFinder, I generated 1,572,390 palindromic kernels with scores of 10 or more, and lengths up to 60.

	KernelFinder 7 10 60 palindromic1
	KernelFinder 7 10 60 palindromic2

I then used PermutationChains to search for solutions starting from these kernels, with full two-fold symmetry.

	PermutationChains 7 symmPairs fullSymm nsk<kernel>

Of those 1,572,390 kernels, only 7 yielded solutions, with an overall total of 83 solutions.

The fruitful kernels all had scores of 10. They had lengths in Robin Houston's notation ranging from 18 to 26, and they covered between 100 and 140 1-cycles.  They are:

666466646646664666 with 53 solutions
666646664466646666 with 9 solutions
56664666466466646665 with 2 solutions
6664664666446664664666 with 9 solutions
666366466646646664663666 with 1 solution
666646646636636646646666 with 8 solutions
66466466466644666466466466 with 2 solutions
