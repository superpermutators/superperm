#!/usr/bin/env python

import itertools as it

k = 3

L = [tuple(p) for p in it.permutations(range(k))] # builds each permutation
 # associates each permutation with a single number (its index in L)
idem = {L[i]:i for i in range(len(L))}
cycle = {}

for p in L: # building one-cycles
    if idem[p] not in cycle:
        y = list(p)
        while idem[tuple(y)] not in cycle:
            cycle[idem[tuple(y)]] = idem[p]
            y.append(y.pop(0))

def cycs(L1): # returns one-cycles of each vertex
    return frozenset([cycle[p] for p in L1])


def get(S, E, cutoff):
    """searches exitless paths with at most cutoff one-cycles "skipped"
    (one "skips" a one-cycle by using a 3-edge before fully doing a 2-cycle)"""
    # keeps track of the paths we need to check out, and try to continue
    # further. The key corresponds to the current end of the path, and the
    # value of the key is the set of such paths with that end
    todo = {}
    # keeps track of current least wastage to get to reach states. Its key is
    # also the end of the path, which yeilds another dictonary.
    # The second dictionary is we give a certain wastage, which returns the set
    # of states with that end which are known to have this wastage
    done = {}
    for start, cost in S: # builds todo and done
        if cost > cutoff:
            continue
        # we always pop off our last vertex, that's where we start when we
        # continue our path
        end = start.pop(-1)
        if end not in done:
            done[end] = {i:set() for i in range(cutoff+1)}
            todo[end] = set()
        done[end][cost].add(cycs(start))
        todo[end].add(cycs(start))

    queue = []
    while todo:
        for end in todo:
            queue.append((end, todo[end]))
        todo = {}

        while queue:
            front, options = queue.pop(-1)
            for cost0 in done[front]:
                ops = options&done[front][cost0]
                options -= ops
                # it would be nice to discard the part of ops which has already
                # been checked with the specific front but I haven't gotten
                # around to it.

                for (e, cost1) in E:
                    if cost0+cost1 > cutoff:
                        continue
                    tail = add(e, front)
                    poss = build(ops, cycs(tail[:-1]))

                    if poss:
                        if tail[-1] not in done:
                            done[tail[-1]] = {i:set() for i in range(cutoff+1)}
                        done[tail[-1]][cost0+cost1] |= poss
                        if tail[-1] not in todo:
                            todo[tail[-1]] = set()
                        todo[tail[-1]] |= poss
        print(sum(len(todo[a]) for a in todo))
    findbests(done)

def findbests(done):
    """returns the length of the longest path of each 1-cycle skippage."""
    best = [0]*100
    for end in done:
        for cost in done[end]:
            if done[end][cost]:
                best[cost] = max(best[cost], max(len(p) for p in done[end][cost]))
    for i in range(len(best)):
        if best[i]:
            print(i, best[i])


def build(S, e): # find the paths in S which don't intersect with e
    return {path|e for path in S if not path&e}

def add(e, front):
    """we start our path at front, and then move places according to e"""
    out = [front]
    for step in e:
        out.append(step[out[-1]])
    return out


def phi(m): # represents a move as a tuple of indices
    return tuple(idem[move(p, m)] for p in L)


# m is the indices you move around with your edge since we always complete our
# 1-cycles in exitless paths, the last letter of your permutation will end up
# as the first letter when we're actually doing this move.
def move(p, m):
    x = list(p)
    y = []
    for i in m:
        y.append(x.pop(i))
    return tuple(x+y)

def reduce(seq):
    """converts the rearrangement of genmoves for the move function"""
    out = list(seq)
    for a in range(len(seq)):
        if a < 0:
            continue
        for i in range(a):
            if -1 < seq[i] < seq[a]:
                out[a] -= 1
    return out

def genmoves(n):
    """create all n-edges"""
    return [reduce(a) for a in it.permutations(list(range(n-1))+[-1])]


tau = phi([0, -1]) # 2-edge

 # how we start our path, before our first 3-edge. ([visited vertices], defined
 # wastage)
Ts = [([0], k-2)]
for i in range(k-2):
    Ts.append((Ts[-1][0]+[tau[Ts[-1][0][-1]]], Ts[-1][1]-1))

moves = []

 # how we will continue our path? Consists of a 3-edge, followed by a number of
 # 2-edges
for m in genmoves(3):
    toadd = [((phi(m),), k-2)]  # (tuple of maps, defined wastage)
    # we use the sequence of maps with the "add" function
    for _ in range(k-2):
        toadd.append((toadd[-1][0]+(tau,), toadd[-1][1]-1))
        # checks if we re-entered a 1-cycle
        if len(cycs(add(toadd[-1][0], 0))) < len(toadd[-1][0])+1:
            toadd.pop(-1)
            break
    print(m, phi(m)[0], len(toadd))
    moves += toadd

# by the current definition of wastage I have, I'd say a 4-edge has an extra
# waste of (k-1), as we have double the cost (if we subtract the exprected cost
# of 2 for these being 2+-edges)
'''for m in genmoves(4):
    toadd = [((phi(m), tau), k-3+k-1)]
    for _ in range(k-3):
        toadd.append((toadd[-1][0]+(tau,), toadd[-1][1]-1))
        if len(cycs(add(toadd[-1][0], 0))) < len(toadd[-1][0])+1:
            toadd.pop(-1)
            break
    print(m, phi(m)[0], len(toadd))
    moves += toadd'''

if __name__ == '__main__':
    get(Ts, moves, 13)
