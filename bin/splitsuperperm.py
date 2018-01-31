#!/usr/bin/python
# -*- encoding: utf-8 -*-
from __future__ import division

import sys

n_str, superperm = sys.argv[1:]
n = int(n_str)

sorted_perm = map(str, range(1, n+1))
for i in range(len(superperm) - n + 1):
    p = superperm[i : i + n]
    s = sorted(p)
    if s == sorted_perm: print p
    else: print "..."
