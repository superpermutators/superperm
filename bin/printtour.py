#!/usr/bin/python
# -*- encoding: utf-8 -*-
from __future__ import division

from itertools import permutations
import optparse

parser = optparse.OptionParser(usage="%prog [options] N tour_filename")
parser.add_option("-r", "--rotations", action="store_true",
        help="fill in rotations of visited permutations")
(options, args) = parser.parse_args()

SYMBOLS = "123456789ABCDEFG"

n = int(args[0])
tour_filename = args[1]

perms = list(permutations(range(n)))
n_perms = len(perms)

def read_tour(tour_filename):
    with open(tour_filename, 'r') as f:
        tour_section = False
        for line in f:
            if line.startswith("TOUR_SECTION"):
                tour_section = True
            elif tour_section:
                ix = int(line)
                if ix == -1: break
                yield "".join([ SYMBOLS[i] for i in perms[ix - 1] ])

def rotations(xs):
    for x in xs:
        for i in range(n):
            yield x[i:] + x[:i]

def squash(xs):
    return reduce(lambda x, y: x + y[overlap(x, y):], xs, "")

def overlap(x, y):
    n = min(len(x), len(y))
    for i in range(n, -1, -1):
        if x[len(x)-i:] == y[:i]:
            return i

tour = read_tour(tour_filename)
if options.rotations:
    tour = rotations(tour)
print squash(tour)
