# desperate
DE-SuperPERmutATE

## Overview

`desperate` reads a list of elements, encoded as a string of elements, and
determines the unique permutations present in the list. The valid set of
elements is given as a character-set on the command-line, similarly to the `tr`
program. `desperate` will print out any valid permutations onto STDOUT.
With the `--numbered` option, it will additionally print the position of the
valid permutation in csv format: "position,permutation".

An example of using the `--numbered` option to place each permutation relative
to where it actually appears in the list:

```bash
echo '123121321' | desperate --numbered '123' | awk -F',' '{printf("%*s%s\n", $1, "", $2);}'
```

## Building

This program is to be built using autotools. You may use autoreconf --install to produce ./configure. It's a good idea to use ./configure in a build/ subdirectory or outside of the build tree.

Feel free to propose changes to the functionality or documentation of this application.
