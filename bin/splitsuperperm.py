#!/usr/bin/python
# -*- encoding: utf-8 -*-
from __future__ import division

import argparse
import math
import sys

SYMBOLS = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

def permutations(n, superperm):
    yield superperm[0:n] # The first length-n substring should be a permutation
    counts = [1 for i in range(n)]
    duplicates = 0
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


def infer_n(superperm):
    """ The first n elements should be a permutation, so we can infer n """
    seen = set()
    for i, c in enumerate(superperm):
        if c in seen:
            return i
        else:
            seen.add(c)
    return i


def split_superperm(superperm, opts):
    n = infer_n(superperm)
    if opts.count:
        perms = set(p for p in permutations(n, superperm) if p != '...')
        perm_count = len(perms)
        expected = math.factorial(n)
        print '{}{}'.format(perm_count, '*' if perm_count == expected else '')
    else:
        for p in permutations(n, superperm):
            print p


def split_file(file, opts):
    for line in file:
        # strip comments
        if line.find('#') > -1:
            line = line[:line.find('#')]
        line = line.strip()
        if len(line) == 0:
            continue
        split_superperm(line, opts)

parser = argparse.ArgumentParser(description='Find permutations in a string')
parser.add_argument('-c', '--count', action='store_true')
parser.add_argument('-s', '--string')
parser.add_argument('file', nargs='*')
opts = parser.parse_args()

if opts.string:
    split_superperm(opts.string, opts)
elif len(opts.file) > 0:
    for f in opts.file:
        with open(f) as file:
            split_file(file, opts)
else:
    split_file(sys.stdin, opts)

