# Overview

The aim of this project is to find short superpermutations. Our main technique at the moment is to encode the superpermutation problem as an instance of the Travelling Salesman Problem, and run TSP solvers. There are two main TSP solvers that we use:

* [LKH](http://www.akira.ruc.dk/~keld/research/LKH/) is a fast, randomised, approximate solver.

* [Concorde](http://www.math.uwaterloo.ca/tsp/concorde.html) is a slower exact solver.

# Getting started

The easiest program to start with is LKH, because it's easier to build and faster to run than Concorde.

## Install LKH

Installing LKH on a Unix-like system (e.g. Linux or Mac) can be as simple as running these commands:

```sh
curl -LO http://www.akira.ruc.dk/~keld/research/LKH/LKH-2.0.7.tgz
tar zxf LKH-2.0.7.tgz
cd LKH-2.0.7
make
cp ./LKH /usr/local/bin/
```

## Test it out

A good way to make sure LKH is working as expected is to find a short permutation of 5 symbols, which it can do very fast.

In this directory, run:

```sh
LKH lkh/par/5.par
```

This should create at least one output file in the output directory `lkh/out/`. The filename is tagged with the length of the tour. Adding 5 to this number gives the length of the superpermutation. You will probably have an output file called `lkh/out/5.148.lkh`, which represents a minimal-length superpermutation. You can use the script `printtour.py` to print the superpermutation corresponding to this tour:

```sh
bin/printtour.py 5 lkh/out/5.148.lkh
```

which should print something like

```
123451324153241352413254132451342513452134512341523412534123541231452314253142351423154213542153421543214532143521432514321542312453124351243152431254312
```

## Break new ground

Now you’ve got LKH running, you can start to discover new things! You could look for novel short superpermutations of 6 symbols. It is recommended to use the runner script to avoid some shortcoming in the usage of LKH for this project:

```sh
bin/lkh_runner.py -o lkh/out/
```
You can start multiple instances of LKH, just change the output directory to avoid conflicts:
```sh
bin/lkh_runner.py -o lkh/out/1/
bin/lkh_runner.py -o lkh/out/2/
...
```

Or if you’re feeling ambitious, you can try 7 symbols! The input file for 7 is quite large, so it is not included in the repository and you will have to create it first:

```sh
make atsp/7.atsp
bin/lkh_runner.py -o lkh/out/ -p atsp/7.atsp -n 7
```

LKH runner keeps a maximum of 20 solution files by default.

Share what you find.

You can run the following command to see all options of the runner:
```sh
bin/lkh_runner.py -h
```

## Try Concorde

Concorde is a bit more fiddly to install, but once you have it working it’s simple to run:

```sh
concorde tsp/5.tsp
```

Be warned that it creates a LOT of temporary files in the working directory, so you may want to do something a bit more elaborate and run it from a different directory, e.g.

```sh
SUPERPERM_DIR=$(pwd)
mkdir -p concorde/out
cd /tmp/
concorde -o "$SUPERPERM_DIR"/concorde/out/5.out "$SUPERPERM_DIR"/tsp/5.tsp
cd "$SUPERPERM_DIR"
```

The most obvious low-hanging fruit is to use it to find a provably shortest tour on 6 symbols. This will take at least several days to run.

```sh
SUPERPERM_DIR=$(pwd)
mkdir -p concorde/out
cd /tmp/
concorde -o "$SUPERPERM_DIR"/concorde/out/6.out "$SUPERPERM_DIR"/tsp/6.tsp
```

# Other stuff

* The directory `superpermutations` contains various known superpermutations that are interesting for some reason.

* Want to see what's in the superpermutations? Check out the `demutator` directory. (Be wary of the C code.)
