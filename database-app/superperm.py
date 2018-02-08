import hashlib
import exceptions
import logging
import math
import re

from google.appengine.ext import ndb

# The standard alphabet of symbols
SYMBOL = "123456789abcdefghijklmnopqrstuvwxyz"

# What could possibly go wrong?
class Exception(exceptions.Exception): pass
class NotMinimal(Exception): message = "Superpermutation is not minimal"
class NotComplete(Exception): message = "Superpermutation is incomplete"
class TooManySymbols(Exception): message = "Too many different symbols (max = %d)" % (len(SYMBOL),)


def _hash(s):
    """Compute the SHA256 hash of the string s.
    """
    h = hashlib.sha256()
    h.update(s)
    return h.hexdigest()

def _elements_are_distinct(iterable, seen=None):
    if seen is None:
        seen = set()
    for element in iterable:
        if element in seen: return False
        seen.add(element)
    return True

def _permutations(s, n):
    for i in xrange(len(s) - n + 1):
        p = s[i : i + n]
        if _elements_are_distinct(p):
            yield p

def _check(s, n):
    elements = set()
    if not _elements_are_distinct(_permutations(s, n), elements):
        raise NotMinimal()
    if len(elements) != math.factorial(n):
        raise NotComplete()

def _overlap(x, y):
    for i in xrange(min(len(x), len(y)), -1, -1):
        if x[-i:] == y[:i]:
            return i
    return 0

def _squash(xs):
    """Eliminate overlaps from a list of strings, resulting in a single string.
    """
    return reduce(lambda x, y: x + y[_overlap(x, y):], xs, "")

def _successor(s, n):
    return _squash([
        p + SYMBOL[n] + p
        for p in _permutations(s, n)
    ]), n+1

def _normalise(s):
    """Translate to the standard alphabet.
    """
    translation = {}
    initial_segment = True
    n = 0
    r = []
    for c in s:
        if c not in translation:
            if not initial_segment:
                raise NotMinimal()
            else:
                translation[c] = SYMBOL[n]
                n += 1
                if n >= len(SYMBOL): raise TooManySymbols()
        else:
            initial_segment = False
        r.append(translation[c])
    return "".join(r), n

def _split(s, n):
    r = [[]]
    o = _overlap(s[1:], s)
    k = 0
    for i in xrange(len(s)):
        if i >= n and s[i] != s[i - n]:
            k += 1
            if k >= n - o:
                r[-1] = r[-1][:o-n+1]
                r.append(list(s[i-n+1 : i]))
        else:
            k = 0
        r[-1].append(s[i])
    
    return [ "".join(a) for a in r ]

def _minimise(s, n):
    parts = _split(s, n)
    minimum = None
    for i in range(len(parts)):
        t = _normalise(_squash(parts[i:] + parts[:i]))[0]
        if minimum is None or t < minimum: minimum = t
        t = _normalise(reversed(t))[0]
        if minimum is None or t < minimum: minimum = t
    return minimum

def _parse(s):
    parts = re.split(r"\s+", s.strip())
    s = _squash(parts)

    logging.info("Squashed: %s", s)
    s, n = _normalise(s)

    logging.info("Normalised: (%s, %d)", s, n)
    _check(s, n)
    logging.info("Checked!")

    s = _minimise(s, n)
    logging.info("Minimised: %s", s)

    return s, n

class StoredSuperpermutation(ndb.Model):
    t_created = ndb.DateTimeProperty(auto_now_add=True)

    n = ndb.IntegerProperty(required=True)
    s = ndb.TextProperty(required=True)


def lookup(raw):
    s, n = _parse(raw)
    threshold_length = sum([ math.factorial(i) for i in xrange(1, n+1) ])

    is_long = (len(s) > threshold_length)

    k = ndb.Key("StoredSuperpermutation", _hash(s))

    is_novel = False
    if not is_long:
        ss = k.get()
        if ss is None:
            is_novel = True
            ss = StoredSuperpermutation(key=k, s=s, n=n)
            ss.put()

    return dict(s=s, n=n, len=len(s), is_long=is_long, threshold_length=threshold_length, is_novel=is_novel)
