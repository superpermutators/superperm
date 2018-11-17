#!/usr/bin/python
# -*- encoding: utf8 -*-

import sys

SYMBOLS = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
text = ""

n = int(sys.argv[1])
if len(sys.argv) == 2:
	s = sys.stdin.read().strip()
elif len(sys.argv) == 3:
	filename = sys.argv[2]
	text = "\n=>" + filename + "\n"
	s = open(filename).read().strip()
else:
	print >>sys.stderr, "%s: Wrong number of arguments" % (sys.argv[0],)
	sys.exit(1)

sorted_perm = list(SYMBOLS[:n])

def cyclerep(c):
	"""
	The canonical representative of the cyclic equivalence class of c
	"""
	m = min(c)
	i = c.index(m)
	return c[i:] + c[:i]

def two_cycle(p):
	"""
	The 2-cycle determined by the permutation p
	"""
	return (p[-1], cyclerep(p[:-1]))

def three_cycle(p):
	"""
	The 3-cycle determined by the permutation p
	"""
	return (p[-1], p[-2], cyclerep(p[:-2]))

def neighbours(c, p):
	for i in range(len(p)):
		d = p[:i] + p[i+1:]
		for j in range(len(p) - 1):
			yield (p[i], cyclerep(d[:j] + c + d[j:]), i == j)


two_cycles = {}
three_cycles = {}
gap = 1
current_two_cycle = None
for i in range(len(s) - n):
	p = s[i : i+n]
	if sorted(p) == sorted_perm:
		if gap > 0:
			# Entering a different 1-cycle
			ts = two_cycle(p)
			if ts != current_two_cycle:
				# Entering a different 2-cycle
				thrs = three_cycle(p)
				three_cycles.setdefault(thrs, []).append(ts)
			current_two_cycle = ts
			two_cycles.setdefault(ts, []).append(p)
		gap = 0
	else:
		gap += 1

# for (c, p) in sorted(two_cycles.keys()):
# 	print c + " " + p + "  ", ", ".join(two_cycles[(c, p)])

max_nb = 0
n_nbs = {}
tss = set(two_cycles.iterkeys())
for (c, p) in sorted(tss):
	nbs = [
		c_ + " " + p_ + ("*" if d_ else "") for (c_, p_, d_) in neighbours(c, p)
		if (c_, p_) in tss
	]
	max_nb = max(max_nb, len(nbs))
	n_nbs[len(nbs)] = n_nbs.get(len(nbs), 0) + 1
	text += c + " " + p + "   " + ", ".join(nbs) + "\n"

# if max_nb <= 3:
# 	print text

# if n_nbs.get(3, 0) == 0:
# 	print text

# if n_nbs.get(0, 0) == 1: print text

# if n_nbs.get(2, 0) == 0: print text

print text

# print
# thrss = set(three_cycles.iterkeys())
# for (c, d, p) in sorted(thrss):
# 	print c + "/" + d + " " + p
