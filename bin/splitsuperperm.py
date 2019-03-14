#!/usr/bin/python
# -*- encoding: utf-8 -*-
from __future__ import division

import sys

SYMBOLS = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

n_str = sys.argv[1]
n = int(n_str)

sorted_perm = list(SYMBOLS[:n])

def split_superperm(superperm):
    counts = {}
    for i in range(n):
        counts[sorted_perm[i]] = 0
    duplicates = 0
    for i in range(n):
        counts[superperm[i]] += 1
        if counts[superperm[i]] > 1:
            duplicates += 1
    if duplicates == 0: print superperm[0:n]
    else: print "..."
    for i in range(1, len(superperm) - n + 1):
        old_char = superperm[i-1]
        p = superperm[i : i + n]
        new_char = p[-1]
        if old_char != new_char:
            counts[old_char] -= 1
            counts[new_char] += 1
            # permutationness may have changed
            if counts[old_char] == 1:
                duplicates -= 1
            if counts[new_char] == 2:
                duplicates += 1
        if duplicates == 0: print p
        else: print "..."

if len(sys.argv) > 2:
    split_superperm(sys.argv[2])
else:
    split_superperm(sys.stdin.read().strip())

