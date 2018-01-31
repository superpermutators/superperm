#!/usr/bin/python
# -*- encoding: utf-8 -*-
from __future__ import division

import re
import sys

INF = 99999

class ATSP(object):
    def __init__(self, filename):
        with open(filename, 'r') as f:
            edge_weight_section = False
            weights = []
            for line in f:
                mo = re.match(r"^NAME\s*:\s*(\S.*\S)\s*$", line)
                if mo:
                    self.name = mo.group(1)
                    continue
                
                mo = re.match(r"^DIMENSION\s*:\s*(\d+)\s*$", line)
                if mo:
                    self.dimension = int(mo.group(1))
                    continue
                
                if re.match(r"^EDGE_WEIGHT_SECTION", line):
                    edge_weight_section = True
                    continue
                
                if edge_weight_section:
                    if re.match(r"^EOF\s*$", line):
                        break
                    weights += [ int(x) for x in re.split(r"\s+", line) if x ]
        
        self.max_weight = max([ w for w in weights if w < INF ])
        self.edges = []
        for i in range(0, self.dimension * self.dimension, self.dimension):
            self.edges.append(weights[i : i+self.dimension])

filename = sys.argv[1]
atsp = ATSP(filename)

addend = 3 * atsp.max_weight + 1
print >>sys.stderr, "max weight = " + str(atsp.max_weight)

def distance(i, j):
    if i == j: return 0
    else: return atsp.edges[i][j] + addend

print "NAME : %s (symmetrised)" % (atsp.name,)
print "TYPE : TSP"
print "DIMENSION : %d" % (atsp.dimension * 2,)
print "EDGE_WEIGHT_TYPE : EXPLICIT"
print "EDGE_WEIGHT_FORMAT : UPPER_ROW"
print "NODE_COORD_TYPE : NO_COORDS"
print "DISPLAY_DATA_TYPE : NO_DISPLAY"

print "EDGE_WEIGHT_SECTION :"

for i in range(atsp.dimension):
    print " ".join( [ str(INF) ] * (atsp.dimension - i - 1) + [ str(distance(i, j)) for j in range(atsp.dimension) ])

for n in range(atsp.dimension - 1, 0, -1):
    print " ".join([ str(INF) ] * n)
