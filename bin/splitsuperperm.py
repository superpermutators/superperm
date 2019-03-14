#!/usr/bin/python
# -*- encoding: utf-8 -*-
from __future__ import division

import argparse
import math
import sys

SYMBOLS = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

def permutations(n, superperm):
    counts = [0 for i in range(n)]
    duplicates = 0
    for i in range(n):
        j = SYMBOLS.index(superperm[i])
        counts[j] += 1
        if counts[j] == 2:
            duplicates += 1
    if duplicates == 0: yield superperm[0:n]
    else: yield "..."
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
        if duplicates == 0: yield superperm[i : i + n]
        else: yield '...'

def split_superperm(superperm, opts):
    if opts.count:
        perms = set(p for p in permutations(opts.n, superperm) if p != '...')
        perm_count = len(perms)
        expected = math.factorial(opts.n)
        print '{}{}'.format(perm_count, '*' if perm_count == expected else '')
    else:
        for p in permutations(opts.n, superperm):
            print p

parser = argparse.ArgumentParser(description='Find permutations in a string')
parser.add_argument('-c', '--count', action='store_true')
parser.add_argument('n', type=int)
opts = parser.parse_args()

if len(sys.argv) > 2:
    split_superperm(sys.argv[2], opts)
else:
    split_superperm(sys.stdin.read().strip(), opts)

