#!/usr/bin/python
# -*- encoding: utf-8 -*-
from __future__ import division

import sys

SYMBOLS = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

n_str = sys.argv[1]
n = int(n_str)

sorted_perm = list(SYMBOLS[:n])

def split_superperm(superperm):
    perms = set()
    counts = [0 for i in range(n)]
    duplicates = 0
    for i in range(n):
        j = SYMBOLS.index(superperm[i])
        counts[j] += 1
        if counts[j] == 2:
            duplicates += 1
    if duplicates == 0: perms.add(superperm[0:n])
    else: print "..."
    for i in range(1, len(superperm) - n + 1):
        old_char = superperm[i-1]
        new_char = superperm[i+n-1]
        if old_char != new_char:
            old = SYMBOLS.index(old_char)
            new = SYMBOLS.index(new_char)
            counts[old] -= 1
            counts[new] += 1
            # number of duplicates may have changed
            if counts[old] == 1:
                duplicates -= 1
            if counts[new] == 2:
                duplicates += 1
        if duplicates == 0: perms.add(superperm[i : i + n])
    print len(perms)

if len(sys.argv) > 2:
    split_superperm(sys.argv[2])
else:
    split_superperm(sys.stdin.read().strip())

