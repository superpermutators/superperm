The files in this directory are of three kinds:

(1)	ChaffinMethodMaxPerms_<n>.txt

These files give a summary table listing the maximum number of permutations (the second column of the table) that can be visited by any string of digits drawn from 1...n, for various numbers of wasted characters (first column). The number of wasted characters is the length of the string, minus the number of (distinct) permutations visited, minus n-1.

(2)	Chaffin_<n>_W_<w>.txt

These files list all strings starting with 123..n that visit the maximum number of permutations for w wasted characters. In some cases there will be no file of this form, but there will be a file of the next kind, with a single example rather than an exhaustive list.

(2)	Chaffin_<n>_W_<w>_OE.txt

These files contain at least one example of a string starting with 123..n that visits the maximum number of permutations for w wasted characters.
