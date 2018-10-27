#!/usr/bin/python
# -*- encoding: utf-8 -*-
from __future__ import division

import sys

SYMBOLS = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

n_str = sys.argv[1]
n = int(n_str)

sorted_perm = list(SYMBOLS[:n])

def split_superperm(superperm):
    for i in range(len(superperm) - n + 1):
        p = superperm[i : i + n]
        s = sorted(p)
        if s == sorted_perm: print p
        else: print "..."

if len(sys.argv) > 2:
    split_superperm(sys.argv[2])
else:
    split_superperm(sys.stdin.read().strip())

