# depermutate

`depermutate` is a program which reads a list of elements per line from a file
(or STDIN) and attempts to determine the set (S) which contains all unique
elements in the list. It then will print out all the valid permutations of the
set (S) which are present in the list of elements.

Optionally, with the `--numbered` option, it prints the position of the
permutation before the permutation itself, in csv format.

An example of using the `--numbered` option to place each permutation relative
to where it actually appears in the list:

```bash
echo '123121321' | depermutate --numbered | awk -F',' '{printf("%*s%s\n", $1, "", $2);}'
```

## Building

Requires autotools. (autoconf, autolib, automake, autoscan, &c.)

This appliction is set up to be built using autotools. It's a good idea to use
./configure in a build/ subdirectory or outside of the build tree.

You may use autoreconf --install to produce ./configure.

