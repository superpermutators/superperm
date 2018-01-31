#!/usr/bin/python
# -*- encoding: utf-8 -*-
from __future__ import division

import sys

SYMBOLS = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

def pal(n):
    assert 1 <= n <= len(SYMBOLS)
    s = SYMBOLS[n-1]
    
    if n == 1: return s
    return squash([ x + s + x for x in unsquash(n-1, pal(n-1)) if sorted(x) == list(SYMBOLS[:n-1]) ])

def overlap(x, y):
    n = min(len(x), len(y))
    for i in range(n, -1, -1):
        if x[len(x)-i:] == y[:i]:
            return i

def squash(xs):
    return reduce(lambda x, y: x + y[overlap(x, y):], xs, "")

def unsquash(n, s):
    for i in range(0, len(s)-n+1):
        yield s[i : i+n]

print pal(int(sys.argv[1]))
#import code; code.interact(local=locals())
