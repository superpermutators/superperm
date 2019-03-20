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
parser.add_option("-c", "--counts", action="store_true", help="show counts of edges by weight")

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

if options.counts:
    counts = defaultdict(int)
    for q in perms:
        counts[distance(ordered, q)] += 1
    for w, c in counts.iteritems():
        print "{0}: {1} ({2:.3f})".format(w, c, c/math.factorial(N))
    exit(0)

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
