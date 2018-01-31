#!/usr/bin/python
# -*- encoding: utf-8 -*-
from __future__ import division

import sys

def squash(xs):
    return reduce(lambda x, y: x + y[overlap(x, y):], xs, "")

def overlap(x, y):
    n = min(len(x), len(y))
    for i in range(n, -1, -1):
        if x[len(x)-i:] == y[:i]:
            return i

def do(f):
	print squash([ line.strip() for line in f if not line.startswith(".") ])

if len(sys.argv) > 1:
	(filename,) = sys.argv[1:]
	with open(filename) as f:
		do(f)
else:
	do(sys.stdin)
