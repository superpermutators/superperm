#!/usr/bin/python
# -*- encoding: utf-8 -*-
from __future__ import division

from collections import defaultdict
import math
from itertools import permutations
import optparse

parser = optparse.OptionParser(usage="%prog [options] N")
parser.add_option("-b", "--bound", type="int", help="only include edges up to this weight")
parser.add_option("-n", "--no-cyclic", action="store_true", help="no edges between non-adjacent cyclic permutations")
parser.add_option("-s", "--simple", action="store_true", help="only include edges from a.b -> b.a^r")
parser.add_option("--necklace", action="store_true", help="consider permutations equivalent under rotation")
parser.add_option("--bracelet", action="store_true", help="consider permutations equivalent under rotation and reflection")

(options, args) = parser.parse_args()
if len(args) != 1: parser.error("Wrong number of arguments")
N = int(args[0])

if options.bound:
    max_weight = options.bound
else:
    max_weight = N
INF = 99999

SYMBOLS = "123456789ABCDEFG"

perms = list(permutations(range(N)))
n_perms = len(perms)
ordered = tuple(range(N))

def distance(p, q):
    if q == ordered: return 0
    
    weight = INF
    if options.simple:
        for n in range(max_weight+1):
            if p[n:] + tuple(reversed(p[:n])) == q:
                weight = n
                break
    else:
        for n in range(max_weight+1):
            if p[n:] == q[:N-n]:
                weight = n
                break
    
    if weight > 1 and options.no_cyclic:
        sp = "".join([ SYMBOLS[i] for i in p ])
        sq = "".join([ SYMBOLS[i] for i in q ])
        if sp in sq+sq: return INF
    
    return weight

def normalise_necklace(p):
    n = list(p)
    i = n.index(0)
    return n[i:] + n[:i]

def normalise_bracelet(p):
    n = normalise_necklace(p)
    i = n.index(1)
    j = n.index(2)
    if i < j:
        n.reverse()
        return normalise_necklace(n)
    return n

def print_classes(normalise):
    classes = defaultdict(set)
    for (i, p) in enumerate(permutations(range(N))):
        key = "".join(map(str, normalise(p)))
        classes[key].add(i + 1)
    num_classes = len(classes)
    print "GTSP_SETS: %d" % (len(classes),)
    print "GTSP_SET_SECTION"
    for (i, c) in enumerate(classes.values()):
        print "%d %s -1" % (i + 1, " ".join(map(str, c)))
    print "EOF"

if options.necklace:
    print "NAME: supernecklace %d" % (N,)
    print "TYPE: GTSP"
elif options.bracelet:
    print "NAME: superbracelet %d" % (N,)
    print "TYPE: GTSP"
else:
    print "NAME : superperm %d" % (N,)
    print "TYPE : ATSP"
print "DIMENSION : %d" % (n_perms,)
print "EDGE_WEIGHT_TYPE : EXPLICIT"
print "EDGE_WEIGHT_FORMAT : FULL_MATRIX"
print "NODE_COORD_TYPE : NO_COORDS"
print "DISPLAY_DATA_TYPE : NO_DISPLAY"

print "EDGE_WEIGHT_SECTION :"

for p in perms:
    print " ".join([ str(distance(p, q)) for q in perms ])

if options.necklace:
    print_classes(normalise_necklace)
if options.bracelet:
    print_classes(normalise_bracelet)
