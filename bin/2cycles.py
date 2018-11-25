#!/usr/bin/python
# -*- encoding: utf8 -*-

import optparse
import sys

SYMBOLS = "123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"

parser = optparse.OptionParser(usage="%prog [options] N")
parser.add_option("-g", "--graph", action="store_true", help="output the results in graphviz format")
parser.add_option("", "--oneline", action="store_true", help="output the results in one-line format")

(options, args) = parser.parse_args()
if options.graph and options.compact:
	parser.error("You can’t specify both --graph and --oneline")
if len(args) != 1: parser.error("Wrong number of arguments")

n = int(args[0])
header = ""
if len(args) == 1:
	s = sys.stdin.read().strip()
elif len(args) == 2:
	filename = args[1]
	header = "\n=>" + filename
	s = open(filename).read().strip()
else:
	parser.error("Wrong number of arguments")

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

def two_cycles_adjacencies(two_cycles):
	max_nb = 0
	n_nbs = {}
	tss = set(two_cycles.iterkeys())
	r = ""
	for (c, p) in sorted(tss):
		nbs = [
			c_ + " " + p_ + ("*" if d_ else "") for (c_, p_, d_) in neighbours(c, p)
			if (c_, p_) in tss
		]
		max_nb = max(max_nb, len(nbs))
		n_nbs[len(nbs)] = n_nbs.get(len(nbs), 0) + 1
		r += c + " " + p + "   " + ", ".join(nbs) + "\n"
	return r

def two_cycles_graphviz(two_cycles):
	ts_list = list(two_cycles.iterkeys())
	tss = set(ts_list)

	index_by_two_cycle = {}
	for (i, ts) in enumerate(ts_list): index_by_two_cycle[ts] = i

	r = "graph {\n"
	for (i, (c, p)) in enumerate(ts_list):
		r += "  \"%s/%s\";\n" % (c, p)
		for (c_, p_, d_) in neighbours(c, p):
			if (c_, p_) not in tss: continue
			if index_by_two_cycle[(c_, p_)] < i: continue
			edge = "\"%s/%s\" -- \"%s/%s\"" % (c, p, c_, p_)
			if d_:
				r += "  { edge[style=bold]; %s; }\n" % (edge,);
			else:
				r += "  %s;\n" % (edge,);

	r += "}\n"
	return r

def two_cycles_oneline(two_cycles):
	return " ".join([
		"%s/%s" % ts
		for ts in sorted(two_cycles.iterkeys())
	])

if options.oneline:
	print two_cycles_oneline(two_cycles)
elif options.graph:
	print two_cycles_graphviz(two_cycles)
else:
	print header
	print two_cycles_adjacencies(two_cycles)
